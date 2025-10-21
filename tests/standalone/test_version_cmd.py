from apps.neo_n3_cmd import Neo_n3_Command
from apps.get_version import get_version_from_makefile

LEDGER_MAJOR_VERSION, LEDGER_MINOR_VERSION, LEDGER_PATCH_VERSION = get_version_from_makefile()

def test_version(backend, firmware):
    client = Neo_n3_Command(backend)
    assert client.get_version() == (LEDGER_MAJOR_VERSION, LEDGER_MINOR_VERSION, LEDGER_PATCH_VERSION)
