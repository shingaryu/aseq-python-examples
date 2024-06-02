from ctypes import *
from platform import system
from typing import Any

c_uintptr = c_size_t  # Works for all intents and purposes

class DeviceContext(Structure):
    class _HidDevice(Structure):
        pass

    _fields_ = [("handle", POINTER(_HidDevice)),
                ("numOfPixelsInFrame", c_uint16),
                ("serial", c_char_p)]

class DeviceInfo(Structure):
    pass
DeviceInfo._fields_ = [("serialNumber", c_char_p),
                       ("next", POINTER(DeviceInfo))]

class DeviceInfoIterator:
    def __init__(self, head: POINTER(DeviceInfo)):
        self.curr = head

    def __iter__(self):
        return self

    def __next__(self) -> str:
        if not self.curr:
            raise StopIteration

        serial = self.curr.contents.serialNumber.decode()
        self.curr = self.curr.contents.next
        return serial

if system() == "Windows":
    from os import add_dll_directory, getcwd
    add_dll_directory(getcwd())
    libspectr = CDLL("libspectrometer.dll")
else:
    libspectr = CDLL("libspectrometer.so")

# Return types
libspectr.getDevicesCount.restype = c_uint32
libspectr.getDevicesInfo.restype = POINTER(DeviceInfo)
libspectr.clearDevicesInfo.restype = None

# Argument types
libspectr.disconnectDeviceContext.argtypes = [POINTER(c_uintptr)]
libspectr.connectToDeviceBySerial.argtypes = [c_char_p, POINTER(c_uintptr)]
libspectr.connectToDeviceByIndex.argtypes = [c_uint, POINTER(c_uintptr)]
libspectr.clearDevicesInfo.argtypes = [POINTER(DeviceInfo)]
libspectr.setFrameFormat.argtypes = [c_uint16, c_uint16, c_uint8, POINTER(c_uint16), POINTER(c_uintptr)]
libspectr.setExposure.argtypes = [c_uint32, c_uint8, POINTER(c_uintptr)]
libspectr.setAcquisitionParameters.argtypes = [c_uint16, c_uint16, c_uint8, c_uint32, POINTER(c_uintptr)]
libspectr.setMultipleParameters.argtypes = [c_uint16, c_uint16, c_uint8, c_uint32, c_uint8, c_uint8, POINTER(c_uintptr)]
libspectr.setExternalTrigger.argtypes = [c_uint8, c_uint8, POINTER(c_uintptr)]
libspectr.setOpticalTrigger.argtypes = [c_uint8, c_uint16, c_uint16, POINTER(c_uintptr)]
libspectr.triggerAcquisition.argtypes = [POINTER(c_uintptr)]
libspectr.getStatus.argtypes = [POINTER(c_uint8), POINTER(c_uint16), POINTER(c_uintptr)]
libspectr.getAcquisitionParameters.argtypes = [POINTER(c_uint16), POINTER(c_uint16), POINTER(c_uint8), POINTER(c_uint32), POINTER(c_uintptr)]
libspectr.getFrameFormat.argtypes = [POINTER(c_uint16), POINTER(c_uint16), POINTER(c_uint8), POINTER(c_uint16), POINTER(c_uintptr)]
libspectr.getFrame.argtypes = [POINTER(c_uint16), c_uint16, POINTER(c_uintptr)]
libspectr.clearMemory.argtypes = [POINTER(c_uintptr)]
libspectr.eraseFlash.argtypes = [POINTER(c_uintptr)]
libspectr.readFlash.argtypes = [POINTER(c_uint8), c_uint32, c_uint32, POINTER(c_uintptr)]
libspectr.writeFlash.argtypes = [POINTER(c_uint8), c_uint32, c_uint32, POINTER(c_uintptr)]
libspectr.resetDevice.argtypes = [POINTER(c_uintptr)]
libspectr.detachDevice.argtypes = [POINTER(c_uintptr)]

class SpectrometerError(Exception):
    pass

class SpectrometerConnectionError(SpectrometerError, ConnectionError):
    pass

# TODO Proper type hints for this function
def _errcheck(result, func, arguments: tuple) -> Any:
    if result == 0:
        return

    # TODO Write better error messages using more specific classes
    # TODO Find a use for the other parameters of this function
    if result == 500: raise SpectrometerConnectionError("wrong identifier")
    if result == 501: raise SpectrometerConnectionError("not found")
    if result == 502: raise SpectrometerConnectionError("failed")
    if result == 503: raise SpectrometerError("device not initialized")
    if result == 504: raise SpectrometerError("writing process failed")
    if result == 505: raise SpectrometerError("reading process failed")
    if result == 506: raise SpectrometerError("wrong answer")
    if result == 507: raise SpectrometerError("remaining packets in frame mismatch")
    if result == 508: raise SpectrometerError("wrong number of packets in frame")
    if result == 509: raise SpectrometerError("input parameter not initialized")
    if result == 510: raise SpectrometerError("remaining packets in flash mismatch")
    if result == 516: raise SpectrometerConnectionError("wrong serial number")
    if result == 585: raise SpectrometerError("no device context")

    raise SpectrometerError(f"unexpected spectrometer error code: '{result}'")

# Error checking
libspectr.disconnectDeviceContext.errcheck = _errcheck
libspectr.connectToDeviceBySerial.errcheck = _errcheck
libspectr.connectToDeviceByIndex.errcheck = _errcheck
libspectr.setFrameFormat.errcheck = _errcheck
libspectr.setExposure.errcheck = _errcheck
libspectr.setAcquisitionParameters.errcheck = _errcheck
libspectr.setMultipleParameters.errcheck = _errcheck
libspectr.setExternalTrigger.errcheck = _errcheck
libspectr.setOpticalTrigger.errcheck = _errcheck
libspectr.triggerAcquisition.errcheck = _errcheck
libspectr.getStatus.errcheck = _errcheck
libspectr.getAcquisitionParameters.errcheck = _errcheck
libspectr.getFrameFormat.errcheck = _errcheck
libspectr.getFrame.errcheck = _errcheck
libspectr.clearMemory.errcheck = _errcheck
libspectr.eraseFlash.errcheck = _errcheck
libspectr.readFlash.errcheck = _errcheck
libspectr.writeFlash.errcheck = _errcheck
libspectr.resetDevice.errcheck = _errcheck
libspectr.detachDevice.errcheck = _errcheck
