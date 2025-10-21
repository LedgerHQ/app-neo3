from typing import Union, cast, List, Tuple
import re
import enum
import struct
from pathlib import Path

from ragger.backend.interface import BackendInterface, RAPDU
from ragger.backend import RaisePolicy
from ragger.bip import pack_derivation_path

from neo3.network.payloads.transaction import Transaction, HighPriorityAttribute, OracleResponse
from neo3.network.payloads.verification import WitnessScope, Signer
from neo3.core import types, serialization

from apps.neo_n3_cmd_builder import InsType, Neo_n3_CommandBuilder
from apps.exception import errors, DeviceException


CLA = Neo_n3_CommandBuilder.CLA
bip44_path: str = "m/44'/888'/0'/0/0"
network_magic = 123  # actual value doesn't matter


class ParserStatus(enum.IntEnum):
    PARSING_OK = 1
    INVALID_LENGTH_ERROR = -1
    VERSION_PARSING_ERROR = -2
    VERSION_VALUE_ERROR = -3
    NONCE_PARSING_ERROR = -4
    SYSTEM_FEE_PARSING_ERROR = -5
    SYSTEM_FEE_VALUE_ERROR = -6
    NETWORK_FEE_PARSING_ERROR = -7
    NETWORK_FEE_VALUE_ERROR = -8
    VALID_UNTIL_BLOCK_PARSING_ERROR = -9
    SIGNER_LENGTH_PARSING_ERROR = -10
    SIGNER_LENGTH_VALUE_ERROR = -11
    SIGNER_ACCOUNT_PARSING_ERROR = -12
    SIGNER_ACCOUNT_DUPLICATE_ERROR = -13
    SIGNER_SCOPE_PARSING_ERROR = -14
    SIGNER_SCOPE_VALUE_ERROR_GLOBAL_FLAG = -15
    SIGNER_ALLOWED_CONTRACTS_LENGTH_PARSING_ERROR = -16
    SIGNER_ALLOWED_CONTRACTS_LENGTH_VALUE_ERROR = -17
    SIGNER_ALLOWED_CONTRACT_PARSING_ERROR = -18
    SIGNER_ALLOWED_GROUPS_LENGTH_PARSING_ERROR = -19
    SIGNER_ALLOWED_GROUPS_LENGTH_VALUE_ERROR = -20
    SIGNER_ALLOWED_GROUPS_PARSING_ERROR = -21
    ATTRIBUTES_LENGTH_PARSING_ERROR = -22
    ATTRIBUTES_LENGTH_VALUE_ERROR = -23
    ATTRIBUTES_UNSUPPORTED_TYPE = -24
    ATTRIBUTES_DUPLICATE_TYPE = -25
    SCRIPT_LENGTH_PARSING_ERROR = -26
    SCRIPT_LENGTH_VALUE_ERROR = -27
    SIGNER_SCOPE_GROUPS_NOT_ALLOWED_ERROR = -28
    SIGNER_SCOPE_CONTRACTS_NOT_ALLOWED_ERROR = -29


PARSER_RE = re.compile(r"\s+(?P<name>.*) = (?P<value>-?\d{1,2})")


def parse_parser_codes(path: Path) -> List[Tuple[str, int]]:
    if not path.is_file():
        raise FileNotFoundError(f"Can't find file: '{path}'")

    with open(str(path.absolute()), encoding="utf-8") as f:
        lines = f.readlines()

    include = False

    results = []
    for line in lines:
        if "PARSING_OK" in line:
            include = True
        if "parser_status_e" in line:
            include = False

        if include:
            m = PARSER_RE.match(line)
            if m:
                results.append((m.group(1), int(m.group(2))))

    return results


def test_parser_codes():
    # path with tests
    conftest_folder_path: Path = Path(__file__).parent
    # types.h should be in src/types.h
    types_h_path = conftest_folder_path.parent.parent / "src" / "transaction" / "transaction_types.h"

    expected_parser_codes: List[Tuple[str, int]] = parse_parser_codes(types_h_path)
    for name, value in expected_parser_codes:
        assert name in ParserStatus.__members__
        assert value == ParserStatus.__members__[name].value, f"value mismatch for {name}"


def serialize(cla: int, ins: Union[int, enum.IntEnum], p1: int = 0, p2: int = 0, cdata: bytes = b"") -> bytes:
    ins = cast(int, ins.value) if isinstance(ins, enum.IntEnum) else cast(int, ins)
    header: bytes = struct.pack("BBBBB", cla, ins, p1, p2, len(cdata))
    return header + cdata


def send_bip44_and_magic(backend: BackendInterface) -> None:
    backend.exchange_raw(serialize(cla=CLA,
                                   ins=InsType.INS_SIGN_TX,
                                   p1=0x00,
                                   p2=0x80,
                                   cdata=pack_derivation_path(bip44_path)[1:])) # No length prefix

    magic = struct.pack("I", network_magic)
    backend.exchange_raw(serialize(cla=CLA,
                                   ins=InsType.INS_SIGN_TX,
                                   p1=0x01,
                                   p2=0x80,
                                   cdata=magic))


def send_raw_tx_data(backend: BackendInterface, data: bytes) -> RAPDU:
    return backend.exchange_raw(serialize(cla=CLA,
                                          ins=InsType.INS_SIGN_TX,
                                          p1=0x02,  # seq number
                                          p2=0x00,  # last apdu
                                          cdata=data))


def test_invalid_version_value(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    version = struct.pack("B", 1)  # version should be 0
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, version)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.VERSION_VALUE_ERROR


def test_invalid_nonce_parsing(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00'  # a valid nonce would be 4 bytes
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, version + nonce)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.NONCE_PARSING_ERROR


def test_system_fee_parsing(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00' * 4
    system_fee = b'\x00'  # a valid system_fee would be 4 bytes
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, version + nonce + system_fee)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.SYSTEM_FEE_PARSING_ERROR


def test_system_fee_value(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00' * 4
    system_fee = struct.pack(">q", -1)  # negative value not allowed. Yes NEO uses int64 not uint64 *shrug*
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, version + nonce + system_fee)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.SYSTEM_FEE_VALUE_ERROR


def test_network_fee_parsing(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00' * 4
    system_fee = struct.pack(">q", 0)
    network_fee = b'\x00'  # a valid system_fee would be 4 bytes
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, version + nonce + system_fee + network_fee)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.NETWORK_FEE_PARSING_ERROR


def test_network_fee_value(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00' * 4
    system_fee = struct.pack(">q", 0)
    network_fee = struct.pack(">q", -1)  # negative value not allowed. Yes NEO uses int64 not uint64 *shrug*
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, version + nonce + system_fee + network_fee)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.NETWORK_FEE_VALUE_ERROR


def test_valid_until_block(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00' * 4
    system_fee = struct.pack(">q", 0)
    network_fee = struct.pack(">q", 0)
    valid_until_block = b'\x00'  # a valid 'valid_until_block' would be 4 bytes
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, version + nonce + system_fee + network_fee + valid_until_block)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.VALID_UNTIL_BLOCK_PARSING_ERROR


def test_signers_length(backend: BackendInterface) -> None:
    # by not adding a 'varint' to the data to indicate the signers length, we should fail to parse
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00' * 4
    system_fee = struct.pack(">q", 0)
    network_fee = struct.pack(">q", 0)
    valid_until_block = b'\x00' * 4
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, version + nonce + system_fee + network_fee + valid_until_block)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.SIGNER_LENGTH_PARSING_ERROR


def test_signers_length2(backend: BackendInterface) -> None:
    # test signer length too large (3 vs max 2 allowed)
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00' * 4
    system_fee = struct.pack(">q", 0)
    network_fee = struct.pack(">q", 0)
    valid_until_block = b'\x00' * 4
    signer_length = b'\x03'  # max allowed is 2
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, version + nonce + system_fee + network_fee + valid_until_block + signer_length)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.SIGNER_LENGTH_VALUE_ERROR


def test_signers_account(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00' * 4
    system_fee = struct.pack(">q", 0)
    network_fee = struct.pack(">q", 0)
    valid_until_block = b'\x00' * 4
    signer_length = b'\x01'
    # by providing no data for account it should fail to parse
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, version + nonce + system_fee + network_fee + valid_until_block + signer_length)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.SIGNER_ACCOUNT_PARSING_ERROR


def test_signers_scope(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00' * 4
    system_fee = struct.pack(">q", 0)
    network_fee = struct.pack(">q", 0)
    valid_until_block = b'\x00' * 4
    signer_length = b'\x01'
    account = b'\x00' * 20  # UInt160
    # by providing no scope data it should fail to parse
    data = version + nonce + system_fee + network_fee + valid_until_block + signer_length + account
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, data)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.SIGNER_SCOPE_PARSING_ERROR


def test_signers_scope_global(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00' * 4
    system_fee = struct.pack(">q", 0)
    network_fee = struct.pack(">q", 0)
    valid_until_block = b'\x00' * 4
    signer_length = b'\x01'
    account = b'\x00' * 20  # UInt160
    # a global scope is not allowed to be combined with other scopes
    scope = WitnessScope.GLOBAL | WitnessScope.CALLED_BY_ENTRY
    scope = scope.to_bytes(1, 'little')

    data = version + nonce + system_fee + network_fee + valid_until_block + signer_length + account + scope
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, data)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.SIGNER_SCOPE_VALUE_ERROR_GLOBAL_FLAG


def test_signers_scope_contracts(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00' * 4
    system_fee = struct.pack(">q", 0)
    network_fee = struct.pack(">q", 0)
    valid_until_block = b'\x00' * 4
    signer_length = b'\x01'
    account = b'\x00' * 20  # UInt160

    scope = WitnessScope.CUSTOM_CONTRACTS
    scope = scope.to_bytes(1, 'little')

    contracts_count = b'\x11'

    data = version + nonce + system_fee + network_fee + valid_until_block + signer_length + account + scope + contracts_count
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, data)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.SIGNER_ALLOWED_CONTRACTS_LENGTH_VALUE_ERROR


def test_signers_scope_contracts_no_data(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00' * 4
    system_fee = struct.pack(">q", 0)
    network_fee = struct.pack(">q", 0)
    valid_until_block = b'\x00' * 4
    signer_length = b'\x01'
    account = b'\x00' * 20  # UInt160

    scope = WitnessScope.CUSTOM_CONTRACTS
    scope = scope.to_bytes(1, 'little')

    contracts_count = b'\x01'
    # by not providing any actual contract data we should fail
    data = version + nonce + system_fee + network_fee + valid_until_block + signer_length + account + scope + contracts_count
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, data)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.SIGNER_ALLOWED_CONTRACT_PARSING_ERROR


def test_signers_scope_groups(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00' * 4
    system_fee = struct.pack(">q", 0)
    network_fee = struct.pack(">q", 0)
    valid_until_block = b'\x00' * 4
    signer_length = b'\x01'
    account = b'\x00' * 20  # UInt160

    scope = WitnessScope.CUSTOM_GROUPS
    scope = scope.to_bytes(1, 'little')
    groups_count = b'\x03'

    data = version + nonce + system_fee + network_fee + valid_until_block + signer_length + account + scope + groups_count
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, data)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.SIGNER_ALLOWED_GROUPS_LENGTH_VALUE_ERROR


def test_signers_scope_groups_no_data(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00' * 4
    system_fee = struct.pack(">q", 0)
    network_fee = struct.pack(">q", 0)
    valid_until_block = b'\x00' * 4
    signer_length = b'\x01'
    account = b'\x00' * 20  # UInt160

    scope = WitnessScope.CUSTOM_GROUPS
    scope = scope.to_bytes(1, 'little')
    groups_count = b'\x01'

    data = version + nonce + system_fee + network_fee + valid_until_block + signer_length + account + scope + groups_count
    # by not providing any actual group data we should fail
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, data)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.SIGNER_ALLOWED_CONTRACT_PARSING_ERROR


def test_attributes(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    version = b'\x00'
    nonce = b'\x00' * 4
    system_fee = struct.pack(">q", 0)
    network_fee = struct.pack(">q", 0)
    valid_until_block = b'\x00' * 4
    signer_length = b'\x01'
    account = b'\x00' * 20  # UInt160
    scope = WitnessScope.CALLED_BY_ENTRY
    scope = scope.to_bytes(1, 'little')

    # by not providing any attributes data it will fail the read a varint
    data = version + nonce + system_fee + network_fee + valid_until_block + signer_length + account + scope
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, data)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.ATTRIBUTES_LENGTH_PARSING_ERROR


def test_attributes_value(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    signer = Signer(account=types.UInt160.from_string("d7678dd97c000be3f33e9362e673101bac4ca654"),
                    scope=WitnessScope.CALLED_BY_ENTRY)
    # exceed max attributes count (2)
    attributes = [HighPriorityAttribute(), HighPriorityAttribute(), HighPriorityAttribute()]
    tx = Transaction(version=0, nonce=0, system_fee=0, network_fee=0, valid_until_block=1, signers=[signer],
                     attributes=attributes)

    with serialization.BinaryWriter() as br:
        tx.serialize_unsigned(br)
        data = br.to_array()

    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, data)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.ATTRIBUTES_LENGTH_VALUE_ERROR


def test_attributes_unsupported(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    signer = Signer(account=types.UInt160.from_string("d7678dd97c000be3f33e9362e673101bac4ca654"),
                    scope=WitnessScope.CALLED_BY_ENTRY)
    # unsupported attribute
    attributes = [OracleResponse._serializable_init()]
    tx = Transaction(version=0, nonce=0, system_fee=0, network_fee=0, valid_until_block=1, signers=[signer],
                     attributes=attributes)

    with serialization.BinaryWriter() as br:
        tx.serialize_unsigned(br)
        data = br.to_array()

    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, data)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.ATTRIBUTES_UNSUPPORTED_TYPE


def test_attributes_duplicates(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    signer = Signer(account=types.UInt160.from_string("d7678dd97c000be3f33e9362e673101bac4ca654"),
                    scope=WitnessScope.CALLED_BY_ENTRY)
    # duplicate attribute
    attributes = [HighPriorityAttribute(), HighPriorityAttribute()]
    tx = Transaction(version=0, nonce=0, system_fee=0, network_fee=0, valid_until_block=1, signers=[signer],
                     attributes=attributes)

    with serialization.BinaryWriter() as br:
        tx.serialize_unsigned(br)
        data = br.to_array()

    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, data)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.ATTRIBUTES_DUPLICATE_TYPE


def test_script(backend: BackendInterface) -> None:
    send_bip44_and_magic(backend)
    signer = Signer(account=types.UInt160.from_string("d7678dd97c000be3f33e9362e673101bac4ca654"),
                    scope=WitnessScope.CALLED_BY_ENTRY)
    # no script is BAD
    tx = Transaction(version=0, nonce=0, system_fee=0, network_fee=0, valid_until_block=1, signers=[signer])

    with serialization.BinaryWriter() as br:
        tx.serialize_unsigned(br)
        data = br.to_array()

    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = send_raw_tx_data(backend, data)
    assert DeviceException.exc[rapdu.status] == errors.TxParsingFailError
    assert int.from_bytes(rapdu.data, 'little', signed=True) == ParserStatus.SCRIPT_LENGTH_VALUE_ERROR
