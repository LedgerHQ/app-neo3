import struct
from hashlib import sha256
from pathlib import Path

from apps.neo_n3_cmd import Neo_n3_Command

from ecdsa.curves import NIST256p
from ecdsa.keys import VerifyingKey
from ecdsa.util import sigdecode_der

from neo3.network.payloads.transaction import Transaction, HighPriorityAttribute, OracleResponse
from neo3.network.payloads.verification import Witness, WitnessScope, Signer
from neo3.core import types, serialization
from neo3 import contracts, vm
from neo3.wallet.utils import address_to_script_hash
from neo3.api.wrappers import NeoToken

from ragger.navigator import NavInsID, NavIns
from ragger.backend import RaisePolicy

ROOT_SCREENSHOT_PATH = Path(__file__).parent.resolve()


def test_arbitrary_scripts_allowed(backend, firmware, navigator, test_name):
    client = Neo_n3_Command(backend)

    bip44_path: str = "m/44'/888'/0'/0/0"

    pub_key = client.get_public_key(bip44_path=bip44_path)
    pk: VerifyingKey = VerifyingKey.from_string(
        pub_key,
        curve=NIST256p,
        hashfunc=sha256
    )

    signer = Signer(account=types.UInt160.from_string("d7678dd97c000be3f33e9362e673101bac4ca654"),
                    scope=WitnessScope.CALLED_BY_ENTRY)
    witness = Witness(invocation_script=b'', verification_script=b'\x55')
    magic = 860833102

    sb = vm.ScriptBuilder()
    sb.emit_contract_call_with_args(NeoToken().hash, "arbitrary", [])

    tx = Transaction(version=0,
                     nonce=123,
                     system_fee=456,
                     network_fee=789,
                     valid_until_block=1,
                     attributes=[],
                     signers=[signer],
                     script=sb.to_array(),
                     witnesses=[witness])

    # Change setting
    if backend.firmware.device.startswith("nano"):
        navigator.navigate_until_text_and_compare(navigate_instruction=NavInsID.RIGHT_CLICK,
                                                  validation_instructions=[NavInsID.BOTH_CLICK, NavInsID.BOTH_CLICK],
                                                  text="Setting",
                                                  path=ROOT_SCREENSHOT_PATH,
                                                  test_case_name=test_name + "_0",
                                                  screen_change_before_first_instruction=False)
    elif backend.firmware.device == "stax" or backend.firmware.device == "flex":
        nav_ins = [NavInsID.USE_CASE_HOME_SETTINGS,

                   NavIns(NavInsID.TOUCH, (350,115)),
                   NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT]
        navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name + "_0", nav_ins, screen_change_before_first_instruction=False)

    with client.sign_vote_tx(bip44_path=bip44_path,
                             transaction=tx,
                             network_magic=magic):

        if backend.firmware.device.startswith("nano"):
            navigator.navigate_until_text_and_compare(navigate_instruction=NavInsID.RIGHT_CLICK,
                                                      validation_instructions=[NavInsID.BOTH_CLICK],
                                                      text="Approve",
                                                      path=ROOT_SCREENSHOT_PATH,
                                                      test_case_name=test_name + "_1")
        elif backend.firmware.device == "flex" or backend.firmware.device == "stax":
            navigator.navigate_until_text_and_compare(NavInsID.SWIPE_CENTER_TO_LEFT,
                                                      [NavInsID.USE_CASE_REVIEW_CONFIRM, NavInsID.USE_CASE_STATUS_DISMISS],
                                                      "Hold to sign",
                                                      ROOT_SCREENSHOT_PATH,
                                                      test_name + "_1")

    der_sig = backend.last_async_response.data

    with serialization.BinaryWriter() as writer:
        tx.serialize_unsigned(writer)
        tx_data: bytes = writer.to_array()

    assert pk.verify(signature=der_sig,
                     data=struct.pack("I", magic) + sha256(tx_data).digest(),
                     hashfunc=sha256,
                     sigdecode=sigdecode_der) is True


def test_arbitrary_scripts_refused(backend, firmware, navigator, test_name):
    client = Neo_n3_Command(backend)

    bip44_path: str = "m/44'/888'/0'/0/0"

    signer = Signer(account=types.UInt160.from_string("d7678dd97c000be3f33e9362e673101bac4ca654"),
                    scope=WitnessScope.CALLED_BY_ENTRY)
    witness = Witness(invocation_script=b'', verification_script=b'\x55')
    magic = 860833102

    sb = vm.ScriptBuilder()
    sb.emit_contract_call_with_args(NeoToken().hash, "arbitrary", [])

    tx = Transaction(version=0,
                     nonce=123,
                     system_fee=456,
                     network_fee=789,
                     valid_until_block=1,
                     attributes=[],
                     signers=[signer],
                     script=sb.to_array(),
                     witnesses=[witness])

    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    with client.sign_vote_tx(bip44_path=bip44_path,
                             transaction=tx,
                             network_magic=magic):

        if backend.firmware.device.startswith("nano"):
            navigator.navigate_until_text_and_compare(navigate_instruction=NavInsID.RIGHT_CLICK,
                                                      validation_instructions=[NavInsID.BOTH_CLICK],
                                                      text="Understood, abort..",
                                                      path=ROOT_SCREENSHOT_PATH,
                                                      test_case_name=test_name)
        elif backend.firmware.device == "stax" or backend.firmware.device == "flex":
            nav_ins = [NavInsID.USE_CASE_CHOICE_CONFIRM,
                       NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT]
            navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name + "_0", nav_ins)

    assert backend.last_async_response.status == 0x6985 # Deny error
    assert backend.last_async_response.data == b""

    if backend.firmware.device == "stax" or backend.firmware.device == "flex":
        with client.sign_vote_tx(bip44_path=bip44_path,
                                 transaction=tx,
                                 network_magic=magic):
            nav_ins = [NavInsID.USE_CASE_CHOICE_REJECT]
            navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name + "_1", nav_ins)

            assert backend.last_async_response.status == 0x6985 # Deny error
            assert backend.last_async_response.data == b""
