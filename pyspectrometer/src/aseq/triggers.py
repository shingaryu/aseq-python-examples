from ctypes import POINTER
from enum import IntFlag

from .lib import c_uintptr, libspectr
from .modes import ExternalTriggerMode

class Trigger:
    def __call__(self):
        raise NotImplementedError

class SoftwareTrigger(Trigger):
    def __init__(self, ctx: POINTER(c_uintptr)):
        self._ctx = ctx

    def __call__(self):
        libspectr.triggerAcquisition(self._ctx)

class ExternalTrigger(Trigger):
    class SignalEdge(IntFlag):
       RISING = 1
       FALLING = 2

    def __init__(self, ctx: POINTER(c_uintptr),
                 mode: ExternalTriggerMode = ExternalTriggerMode.DISABLED,
                 edge: SignalEdge = SignalEdge(0)):
        self._ctx = ctx
        # TODO Find sensible defaults
        self._mode, self._edge = mode, edge

        libspectr.setExternalTrigger(self._mode, self._edge, self._ctx)

    @property
    def mode(self):
        return self._mode

    @mode.setter
    def mode(self, value: ExternalTriggerMode):
        if self._mode != value:
            libspectr.setExternalTrigger(value, self._edge, self._ctx)
            self._mode = value

    @property
    def edge(self):
        return self._edge

    @edge.setter
    def edge(self, value: SignalEdge):
        if self._edge != value:
            libspectr.setExternalTrigger(self._mode, value, self._ctx)
            self._edge = value

# TODO Add proper support
class OpticalTrigger(Trigger):
    pass
