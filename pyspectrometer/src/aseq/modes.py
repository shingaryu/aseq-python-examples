from enum import IntEnum

class ScanMode(IntEnum):
    CONTINUOUS = 0
    FIRST_FRAME_IDLE = 1
    EVERY_FRAME_IDLE = 2
    FRAME_AVERAGING = 3

class ReductionMode(IntEnum):
    NO_AVERAGE = 0
    AVERAGE_OF_2 = 1
    AVERAGE_OF_4 = 2
    AVERAGE_OF_8 = 3

class ExternalTriggerMode(IntEnum):
    DISABLED = 0
    ENABLED = 1
    ONE_TIME = 2

class OpticalTriggerMode(IntEnum):
    DISABLED = 0
    FALLING_EDGE = 1
    ON_THRESHOLD = 2
    ONE_TIME_RISING_EDGE = 0x81
    ONE_TIME_FALLING_EDGE = 0x82
