from ragger.backend import RaisePolicy

from apps.exception import errors, DeviceException


def test_bad_cla(backend, firmware):
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = backend.exchange(cla=0xa0,  # 0xa0 instead of 0xe0
                             ins=0x03,
                             p1=0x00,
                             p2=0x00,
                             data=b"")
    assert DeviceException.exc[rapdu.status] == errors.ClaNotSupportedError


def test_bad_ins(backend, firmware):
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = backend.exchange(cla=0x80,
                             ins=0xff,  # bad INS
                             p1=0x00,
                             p2=0x00,
                             data=b"")

    assert DeviceException.exc[rapdu.status] == errors.InsNotSupportedError


def test_wrong_p1p2(backend, firmware):
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    rapdu = backend.exchange(cla=0x80,
                             ins=0x04, # fix to InsType.public key
                             p1=0x01,  # 0x01 instead of 0x00
                             p2=0x00,
                             data=b"")

    assert DeviceException.exc[rapdu.status] == errors.WrongP1P2Error


def test_wrong_data_length(backend, firmware):
    backend.raise_policy = RaisePolicy.RAISE_NOTHING
    # APDUs must be at least 5 bytes: CLA, INS, P1, P2, Lc.
    rapdu = backend.exchange_raw(b"8000")

    assert DeviceException.exc[rapdu.status] == errors.WrongDataLengthError
