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

from ragger.navigator import NavInsID

ROOT_SCREENSHOT_PATH = Path(__file__).parent.resolve()

def test_sign_tx(backend, scenario_navigator):
    client = Neo_n3_Command(backend)

    bip44_path: str = "m/44'/888'/0'/0/0"

    pub_key = client.get_public_key(bip44_path=bip44_path)

    pk: VerifyingKey = VerifyingKey.from_string(
        pub_key,
        curve=NIST256p,
        hashfunc=sha256
    )

    signer = Signer(account=types.UInt160.from_string("d7678dd97c000be3f33e9362e673101bac4ca654"),
                    scope=WitnessScope.CUSTOM_CONTRACTS)

    signer2 = Signer(account=types.UInt160.from_string("d7678dd97c000be3f33e9362e673101bac412345"),
                     scope=WitnessScope.CUSTOM_CONTRACTS | WitnessScope.CUSTOM_GROUPS)
    for i in range(1, 17):
        signer.allowed_contracts.append(types.UInt160(20 * i.to_bytes(1, 'little')))

    signer2.allowed_contracts.append(types.UInt160.from_string("5b7074e873973a6ed3708862f219a6fbf4d1c411"))
    signer2.allowed_contracts.append(types.UInt160.from_string("862f219a6fbf4d1c4115b7074e873973a6ed3708"))

    witness = Witness(invocation_script=b'', verification_script=b'\x55')
    magic = 860833102

    # build a NEO transfer script
    from_account = address_to_script_hash("NSiVJYZej4XsxG5CUpdwn7VRQk8iiiDMPM").to_array()
    to_account = address_to_script_hash("NU5unwNcWLqPM21cNCRP1LPuhxsTpYvNTf").to_array()
    amount = 11 * 1
    data = None
    sb = vm.ScriptBuilder()
    sb.emit_contract_call_with_args(NeoToken().hash, "transfer", [from_account, to_account, amount, data])
    tx = Transaction(version=0,
                     nonce=123,
                     system_fee=456,
                     network_fee=789,
                     valid_until_block=1,
                     attributes=[],
                     signers=[signer, signer2],
                     script=sb.to_array(),
                     witnesses=[witness])

    with client.sign_tx(bip44_path=bip44_path,
                        transaction=tx,
                        network_magic=magic):
        scenario_navigator.review_approve()        

    der_sig = backend.last_async_response.data

    with serialization.BinaryWriter() as writer:
        tx.serialize_unsigned(writer)
        tx_data: bytes = writer.to_array()

    assert pk.verify(signature=der_sig,
                     data=struct.pack("I", magic) + sha256(tx_data).digest(),
                     hashfunc=sha256,
                     sigdecode=sigdecode_der) is True


def test_sign_vote_script_tx(backend, firmware, navigator, test_name):
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

    # build a NEO vote script
    from_account = address_to_script_hash("NSiVJYZej4XsxG5CUpdwn7VRQk8iiiDMPM").to_array()
    vote_to = bytes.fromhex("03b209fd4f53a7170ea4444e0cb0a6bb6a53c2bd016926989cf85f9b0fba17a70c")

    sb = vm.ScriptBuilder()
    sb.emit_contract_call_with_args(NeoToken().hash, "vote", [from_account, vote_to])

    tx = Transaction(version=0,
                     nonce=123,
                     system_fee=456,
                     network_fee=789,
                     valid_until_block=1,
                     attributes=[],
                     signers=[signer],
                     script=sb.to_array(),
                     witnesses=[witness])

    with client.sign_vote_tx(bip44_path=bip44_path,
                             transaction=tx,
                             network_magic=magic):

        if backend.firmware.device.startswith("nano"):
            navigator.navigate_until_text_and_compare(navigate_instruction=NavInsID.RIGHT_CLICK,
                                                      validation_instructions=[NavInsID.BOTH_CLICK],
                                                      text="Approve",
                                                      path=ROOT_SCREENSHOT_PATH,
                                                      test_case_name=test_name)

        elif backend.firmware.device == "stax":
            # Navigate a bit through rejection screens before confirming
            nav_ins = []
            nav_ins.append(NavInsID.USE_CASE_REVIEW_REJECT)   # screen reject?
            nav_ins.append(NavInsID.USE_CASE_CHOICE_REJECT)   # screen 0
            nav_ins.append(NavInsID.USE_CASE_REVIEW_TAP)      # screen 1
            nav_ins.append(NavInsID.USE_CASE_REVIEW_TAP)      # screen 2
            nav_ins.append(NavInsID.USE_CASE_REVIEW_TAP)      # screen 3
            nav_ins.append(NavInsID.USE_CASE_REVIEW_TAP)      # screen approve?
            nav_ins.append(NavInsID.USE_CASE_REVIEW_PREVIOUS) # screen 3
            nav_ins.append(NavInsID.USE_CASE_REVIEW_TAP)      # screen approve?
            nav_ins.append(NavInsID.USE_CASE_REVIEW_REJECT)   # screen reject?
            nav_ins.append(NavInsID.USE_CASE_CHOICE_REJECT)   # screen approve?
            nav_ins.append(NavInsID.USE_CASE_REVIEW_CONFIRM)

            navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, nav_ins)
        elif backend.firmware.device == "flex":
            # Navigate a bit through rejection screens before confirming
            nav_ins = []
            nav_ins.append(NavInsID.USE_CASE_REVIEW_REJECT)   # screen reject?
            nav_ins.append(NavInsID.USE_CASE_CHOICE_REJECT)   # screen 0
            nav_ins.append(NavInsID.SWIPE_CENTER_TO_LEFT)      # screen 1
            nav_ins.append(NavInsID.SWIPE_CENTER_TO_LEFT)      # screen 2
            nav_ins.append(NavInsID.SWIPE_CENTER_TO_LEFT)      # screen 3
            nav_ins.append(NavInsID.SWIPE_CENTER_TO_LEFT)      # screen approve?
            nav_ins.append(NavInsID.USE_CASE_REVIEW_PREVIOUS) # screen 3
            nav_ins.append(NavInsID.SWIPE_CENTER_TO_LEFT)      # screen approve?
            nav_ins.append(NavInsID.USE_CASE_REVIEW_REJECT)   # screen reject?
            nav_ins.append(NavInsID.USE_CASE_CHOICE_REJECT)   # screen approve?
            nav_ins.append(NavInsID.USE_CASE_REVIEW_CONFIRM)

            navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, nav_ins)

    der_sig = backend.last_async_response.data

    with serialization.BinaryWriter() as writer:
        tx.serialize_unsigned(writer)
        tx_data: bytes = writer.to_array()

    assert pk.verify(signature=der_sig,
                     data=struct.pack("I", magic) + sha256(tx_data).digest(),
                     hashfunc=sha256,
                     sigdecode=sigdecode_der) is True
