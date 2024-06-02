from ctypes import POINTER, c_uint8
from warnings import warn

from .lib import c_uintptr, libspectr

class Flash:
    def __init__(self, ctx: POINTER(c_uintptr)):
        self._ctx = ctx

    def read(self, length: int, offset: int = 0) -> bytes:
        if length < 0 or offset < 0:
            raise ValueError("length and offset must be positive")
        if offset > 0x1FFFF:
            raise ValueError("maximum offset exceeded")

        # XXX Raise an exception instead?
        if offset + length > 0x20000:
            length = 0x20000 - offset

        buffer = (c_uint8 * length)()
        libspectr.readFlash(buffer, offset, length, self._ctx)
        return bytes(buffer)

    def write(self, buffer: bytes, offset: int = 0):
        if offset < 0:
            raise ValueError("offset must be positive")
        if offset > 0x1FFFF:
            raise ValueError("maximum offset exceeded")

        length = len(buffer)
        # XXX Raise an exception instead?
        if offset + length > 0x20000:
            length = 0x20000 - offset
        if any(byte != 0xFF for byte in self.read(length, offset)):
            warn("only empty memory locations can be written to")

        buffer = (c_uint8 * length).from_buffer_copy(buffer)
        libspectr.writeFlash(buffer, offset, length, self._ctx)

    def erase(self):
        libspectr.eraseFlash(self._ctx)
