from apps.neo_n3_cmd import Neo_n3_Command
from apps.get_version import get_version_from_makefile

LEDGER_MAJOR_VERSION, LEDGER_MINOR_VERSION, LEDGER_PATCH_VERSION = get_version_from_makefile()

def test_get_app_and_version(backend, backend_name, firmware):
    client = Neo_n3_Command(backend)

    app_name, version_str = client.get_app_and_version()
    assert app_name == "NEO N3"
    version = version_str.split('.')
    assert version[0] == str(LEDGER_MAJOR_VERSION)
    assert version[1] == str(LEDGER_MINOR_VERSION)
    assert version[2] == str(LEDGER_PATCH_VERSION)
