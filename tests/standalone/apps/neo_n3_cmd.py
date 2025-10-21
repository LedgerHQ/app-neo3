import struct
from typing import Tuple, Generator
from contextlib import contextmanager

from ragger.backend.interface import BackendInterface, RAPDU

from .neo_n3_cmd_builder import Neo_n3_CommandBuilder, InsType

from .transaction import Transaction
from neo3.network import payloads


class Neo_n3_Command:
    def __init__(self,
                 backend: BackendInterface,
                 debug: bool = False) -> None:
        self.backend = backend
        self.builder = Neo_n3_CommandBuilder(debug=debug)
        self.debug = debug

    def get_app_and_version(self) -> Tuple[str, str]:
        response = self.backend.exchange_raw(
            self.builder.get_app_and_version()
        ).data

        # response = format_id (1) ||
        #            app_name_len (1) ||
        #            app_name (var) ||
        #            version_len (1) ||
        #            version (var) ||
        offset: int = 0

        format_id: int = response[offset]
        offset += 1
        app_name_len: int = response[offset]
        offset += 1
        app_name: str = response[offset:offset + app_name_len].decode("ascii")
        offset += app_name_len
        version_len: int = response[offset]
        offset += 1
        version: str = response[offset:offset + version_len].decode("ascii")
        offset += version_len

        return app_name, version

    def get_version(self) -> Tuple[int, int, int]:
        response = self.backend.exchange_raw(
            self.builder.get_version()
        ).data

        # response = MAJOR (1) || MINOR (1) || PATCH (1)
        assert len(response) == 3

        major, minor, patch = struct.unpack(
            "BBB",
            response
        )  # type: int, int, int

        return major, minor, patch

    def get_app_name(self) -> str:
        cmd = self.builder.get_app_name()
        rapdu = self.backend.exchange_raw(cmd)
        return rapdu.data.decode("ascii")

    def get_public_key(self, bip44_path: str) -> bytes:
        response = self.backend.exchange_raw(
            self.builder.get_public_key(bip44_path=bip44_path, display=False)
        ).data

        assert len(response) == 65 # 04 + 64 bytes of uncompressed key

        return response

    @contextmanager
    def get_public_key_async(self, bip44_path: str) -> Generator[RAPDU, None, None]:
        payload = self.builder.get_public_key(bip44_path=bip44_path, display=True)
        with self.backend.exchange_async_raw(payload) as response:
            yield response

    @contextmanager
    def sign_tx(self, bip44_path: str, transaction: payloads.transaction.Transaction, network_magic: int) -> Generator[RAPDU, None, None]:
        for is_last, chunk in self.builder.sign_tx(bip44_path=bip44_path,
                                                   transaction=transaction,
                                                   network_magic=network_magic):
            if not is_last:
                self.backend.exchange_raw(chunk)
            else:
                with self.backend.exchange_async_raw(chunk) as response:
                    yield response

    @contextmanager
    def sign_vote_tx(self, bip44_path: str, transaction: Transaction, network_magic: int) -> Generator[RAPDU, None, None]:
        for is_last, chunk in self.builder.sign_tx(bip44_path=bip44_path,
                                                   transaction=transaction,
                                                   network_magic=network_magic):
            if not is_last:
                self.backend.exchange_raw(chunk)
            else:
                with self.backend.exchange_async_raw(chunk) as response:
                    yield response
