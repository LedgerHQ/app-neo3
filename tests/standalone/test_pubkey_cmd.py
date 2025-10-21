from pathlib import Path

from apps.neo_n3_cmd import Neo_n3_Command

from ragger.navigator import NavInsID, NavIns
from ragger.bip import calculate_public_key_and_chaincode, CurveChoice
from ragger.backend import RaisePolicy

ROOT_SCREENSHOT_PATH = Path(__file__).parent.resolve()

def test_get_public_key_no_confirm(backend, firmware):
    client = Neo_n3_Command(backend)
    for path in ["m/44'/888'/0'/0/0", "m/44'/888'/1'/0/0", "m/44'/888'/10'/1/23"]:
        pub_key = client.get_public_key(bip44_path=path)
        ref_public_key, _ = calculate_public_key_and_chaincode(curve=CurveChoice.Nist256p1, path=path)
        assert pub_key.hex() == ref_public_key
        print(pub_key.hex())

def test_get_public_key_confirm_ok(backend, scenario_navigator):
    client = Neo_n3_Command(backend)
    path = "m/44'/888'/0'/0/0"
    with client.get_public_key_async(bip44_path=path):
        scenario_navigator.address_review_approve()

    pub_key = backend.last_async_response.data

    ref_public_key, _ = calculate_public_key_and_chaincode(curve=CurveChoice.Nist256p1, path=path)
    assert pub_key.hex() == ref_public_key

def test_get_public_key_confirm_refused(backend, scenario_navigator):
    client = Neo_n3_Command(backend)
    path = "m/44'/888'/0'/0/0"
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    with client.get_public_key_async(bip44_path=path):
        scenario_navigator.address_review_reject()
        

    rapdu = backend.last_async_response
    assert rapdu.status == 0x6985 # Deny error
