from ctypes import POINTER, byref, cast, c_uint16
from enum import IntEnum
from typing import Union

from numpy import empty, ndarray

from .lib import DeviceContext, c_uintptr, libspectr

def get_frame_size(ctx: POINTER(c_uintptr)) -> int:
    ctx = cast(ctx, POINTER(POINTER(DeviceContext)))
    try:
        return ctx.contents.contents.numOfPixelsInFrame
    except ValueError:
        return 3694  # Frames contain 32 starting, 1 user, 14 final elements

class Memory:
    def __init__(self, ctx: POINTER(c_uintptr)):
        self._ctx = ctx

    def __len__(self):
        frames_in_memory = c_uint16()
        libspectr.getStatus(None, byref(frames_in_memory), self._ctx)
        return frames_in_memory.value

    def __getitem__(self, key: Union[int, slice]) -> ndarray:
        if isinstance(key, int):
            size = len(self)
            if key < 0: key += size
            if key < 0 or key >= size:
                raise IndexError("index out of range")

            buffer = empty(get_frame_size(self._ctx), dtype=c_uint16)
            libspectr.getFrame(buffer.ctypes.data_as(POINTER(c_uint16)), key, self._ctx)
            return buffer[32:-14][::-1]

        if isinstance(key, slice):
            indices = range(*key.indices(len(self)))

            buffer = empty((len(indices), get_frame_size(self._ctx)), dtype=c_uint16)
            for idx, buf in zip(indices, buffer):
                libspectr.getFrame(buf.ctypes.data_as(POINTER(c_uint16)), idx, self._ctx)
            return buffer[:, 32:-14][:, ::-1]

        raise TypeError(f"indices must be integers or slices, not {type(key).__name__}")

    def clear(self):
        libspectr.clearMemory(self._ctx)

class FakeMemory:
    class Status(IntEnum):
        NOT_READY = 0
        READY = 1
        READY_WITH_LOST = 2

    def __init__(self, ctx: POINTER(c_uintptr)):
        self._ctx = ctx

    def __len__(self):
        return 1

    def __getitem__(self, key) -> ndarray:
        buffer = empty(get_frame_size(self._ctx), dtype=c_uint16)
        libspectr.getFrame(buffer.ctypes.data_as(POINTER(c_uint16)), 0xFFFF, self._ctx)
        return buffer[32:-14][::-1]

    def clear(self):
        pass

    def status(self):
        frame_status = c_uint16()
        libspectr.getStatus(None, byref(frame_status), self._ctx)
        return self.Status(frame_status.value)
