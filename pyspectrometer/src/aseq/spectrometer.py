from ctypes import POINTER, byref, c_uint8, c_uint16, c_uint32, cast, pointer
from enum import IntFlag
from types import TracebackType
from typing import Optional, Tuple, Type

from .flash import Flash
from .lib import DeviceContext, DeviceInfoIterator, SpectrometerError, c_uintptr, libspectr
from .memory import FakeMemory, Memory
from .modes import ReductionMode, ScanMode
from .triggers import SoftwareTrigger

class Spectrometer:
    class Status(IntFlag):
        IN_PROGRESS = 1
        MEMORY_FULL = 2

    def __init__(self, serial: Optional[str] = None):
        self.serial = serial
        self.ctx = pointer(c_uintptr())

        self.flash = Flash(self.ctx)
        self.memory = Memory(self.ctx)
        self.trigger = SoftwareTrigger(self.ctx)

        # Acquisition parameters
        self._num_of_scans = None
        self._num_of_blank_scans = None
        self._scan_mode = None
        self._exposure_time = None

        # Frame parameters
        self._element_range = None, None
        self._reduction_mode = None
        self._frame_size = None

    def __str__(self):
        return f"Spectrometer [{self.serial or 'ASQ_SPC???????'}]: {'' if self.ctx.contents else 'dis'}connected"
    
    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, exc_type: Optional[Type[BaseException]],
                 exc_val: Optional[BaseException],
                 exc_tb: Optional[TracebackType]) -> bool:
        # TODO Handling of exceptions inside the context manager
        self.disconnect()
        return False

    @property
    def num_of_scans(self):
        if self._num_of_scans is not None:
            return self._num_of_scans.value

    @num_of_scans.setter
    def num_of_scans(self, value: int):
        if self._num_of_scans is None:
            raise SpectrometerError("device not initialized")

        if self._num_of_scans.value == value:
            return

        libspectr.setAcquisitionParameters(
            value,
            self._num_of_blank_scans,
            self._scan_mode,
            self._exposure_time,
            self.ctx)

        self._num_of_scans.value = value

    @property
    def num_of_blank_scans(self):
        if self._num_of_blank_scans is not None:
            return self._num_of_blank_scans.value

    @num_of_blank_scans.setter
    def num_of_blank_scans(self, value: int):
        if self._num_of_blank_scans is None:
            raise SpectrometerError("device not initialized")

        if self._num_of_blank_scans.value == value:
            return

        libspectr.setAcquisitionParameters(
            self._num_of_scans,
            value,
            self._scan_mode,
            self._exposure_time,
            self.ctx)

        self._num_of_blank_scans.value = value

    @property
    def scan_mode(self):
        if self._scan_mode is not None:
            return ScanMode(self._scan_mode.value)

    @scan_mode.setter
    def scan_mode(self, value: int):
        if self._scan_mode is None:
            raise SpectrometerError("device not initialized")

        if self._scan_mode.value == ScanMode(value):
            return

        libspectr.setAcquisitionParameters(
            self._num_of_scans,
            self._num_of_blank_scans if value != ScanMode.FRAME_AVERAGING else 0,
            value,
            self._exposure_time,
            self.ctx)

        self._scan_mode.value = value
        if value == ScanMode.FRAME_AVERAGING:
            self._num_of_blank_scans.value = 0

        self._set_memory()

    @property
    def exposure_time(self):
        if self._exposure_time is not None:
            return self._exposure_time.value * 10

    @exposure_time.setter
    def exposure_time(self, value: int):
        if self._exposure_time is None:
            raise SpectrometerError("device not initialized")

        value = round(value / 10)  # Rounding to the nearest multiple of 10 μs
        if self._exposure_time.value == value:
            return

        libspectr.setAcquisitionParameters(
            self._num_of_scans,
            self._num_of_blank_scans,
            self._scan_mode,
            value,
            self.ctx)

        self._exposure_time.value = value

    @property
    def element_range(self):
        if None not in self._element_range:
            return self._element_range[0].value, self._element_range[1].value
        return None, None

    @element_range.setter
    def element_range(self, value: Tuple[int, int]):
        if None in self._element_range:
            raise SpectrometerError("device not initialized")

        if self._element_range[0].value == value[0] and \
           self._element_range[1].value == value[1]:
            return

        libspectr.setFrameFormat(
            value[0],
            value[1],
            self._reduction_mode,
            byref(self._frame_size),
            self.ctx)

        self._element_range[0].value, self._element_range[1].value = value

    @property
    def reduction_mode(self):
        if self._reduction_mode is not None:
            return ReductionMode(self._reduction_mode.value)

    @reduction_mode.setter
    def reduction_mode(self, value: int):
        if self._reduction_mode is None:
            raise SpectrometerError("device not initialized")

        if self._reduction_mode.value == ReductionMode(value):
            return

        libspectr.setFrameFormat(
            self._element_range[0],
            self._element_range[1],
            value,
            byref(self._frame_size),
            self.ctx)

        self._reduction_mode.value = value

    @property
    def frame_size(self):
        if self._frame_size is not None:
            return self._frame_size.value

    def connect(self):
        libspectr.connectToDeviceBySerial(self.serial.encode() if self.serial else None, self.ctx)

        # Acquisition parameters
        self._num_of_scans = c_uint16()
        self._num_of_blank_scans = c_uint16()
        self._scan_mode = c_uint8()
        self._exposure_time = c_uint32()

        libspectr.getAcquisitionParameters(
            byref(self._num_of_scans),
            byref(self._num_of_blank_scans),
            byref(self._scan_mode),
            byref(self._exposure_time),
            self.ctx)

        self._set_memory()

        # Frame parameters
        self._element_range = c_uint16(), c_uint16()
        self._reduction_mode = c_uint8()
        self._frame_size = c_uint16()

        libspectr.getFrameFormat(
            byref(self._element_range[0]),
            byref(self._element_range[1]),
            byref(self._reduction_mode),
            byref(self._frame_size),
            self.ctx)

    def reconnect(self):
        self.connect()

    def disconnect(self):
        libspectr.disconnectDeviceContext(self.ctx)

        # Acquisition parameters
        self._num_of_scans = None
        self._num_of_blank_scans = None
        self._scan_mode = None
        self._exposure_time = None

        self._set_memory()

        # Frame parameters
        self._element_range = None, None
        self._reduction_mode = None
        self._frame_size = None

    def reset(self):
        libspectr.resetDevice(self.ctx)

        # Acquisition parameters
        self._num_of_scans.value = 1
        self._num_of_blank_scans.value = 0
        self._scan_mode.value = ScanMode.CONTINUOUS
        self._exposure_time.value = 10

        self._set_memory()

        # Frame parameters
        self._element_range[0].value, self._element_range[1].value = 0, 3647
        self._reduction_mode.value = ReductionMode.NO_AVERAGE
        self._frame_size.value = 3694

        ctx = cast(self.ctx, POINTER(POINTER(DeviceContext)))
        ctx.contents.contents.numOfPixelsInFrame = 3694

    def status(self):
        status_flags = c_uint8()
        libspectr.getStatus(byref(status_flags), None, self.ctx)
        return self.Status(status_flags.value)

    def _set_memory(self):
        if self._scan_mode is None or self._scan_mode.value != ScanMode.FRAME_AVERAGING:
            if isinstance(self.memory, FakeMemory):
                self.memory = Memory(self.ctx)
        else:
            if isinstance(self.memory, Memory):
                self.memory = FakeMemory(self.ctx)

    @staticmethod
    def device_count() -> int:
        return libspectr.getDevicesCount()

    @staticmethod
    def device_list():
        info = libspectr.getDevicesInfo()
        serials = list(DeviceInfoIterator(info))
        libspectr.clearDevicesInfo(info)
        return serials
