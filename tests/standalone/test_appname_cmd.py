from ragger.backend.interface import BackendInterface

from apps.neo_n3_cmd import Neo_n3_Command

def test_app_name(backend: BackendInterface) -> None:
    client = Neo_n3_Command(backend)
    assert client.get_app_name() == "NEO N3"
