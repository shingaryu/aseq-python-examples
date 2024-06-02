#ifndef SPECTRLIB_INTERNAL_H
#define SPECTRLIB_INTERNAL_H

#include <wchar.h>
#include <stdbool.h>
#include "hidapi.h"

#include <stdint.h>

#define USBD_VID                     0xE220
#define USBD_PID                     0x0100

#define LOW_BYTE(x)     ((unsigned char)((x)&0xFF))
#define HIGH_BYTE(x)    ((unsigned char)(((x)>>8)&0xFF))
#define LOW_WORD(x)     ((unsigned short)((x)&0xFFFF))
#define HIGH_WORD(x)    ((unsigned short)(((x)>>16)&0xFFFF))

#define HIDAPI_OPERATION_ERROR -1
#define HID_OPERATION_READ_SUCCESS PACKET_SIZE
#define HID_OPERATION_WRITE_SUCCESS PACKET_SIZE + 1
#define STANDARD_TIMEOUT_MILLISECONDS 100
#define ERASE_FLASH_TIMEOUT_MILLISECONDS 5000

#define PACKET_SIZE 64
#define EXTENDED_PACKET_SIZE 1 + PACKET_SIZE //bytes
#define MAX_PACKETS_IN_FRAME 124
#define REMAINING_PACKETS_ERROR 250
#define NUM_OF_PIXELS_IN_PACKET 30
#define MAX_READ_FLASH_PACKETS 100
#define MAX_FLASH_WRITE_PAYLOAD 58

#define ZERO_REPORT_ID 0

#define STATUS_REQUEST 1
#define SET_EXPOSURE_REQUEST 2
#define SET_ACQUISITION_PARAMETERS_REQUEST 3
#define SET_FRAME_FORMAT_REQUEST 4
#define SET_EXTERNAL_TRIGGER_REQUEST 5
#define SET_SOFTWARE_TRIGGER_REQUEST 6
#define CLEAR_MEMORY_REQUEST 7
#define GET_FRAME_FORMAT_REQUEST 8
#define GET_ACQUISITION_PARAMETERS_REQUEST 9
#define SET_ALL_PARAMETERS_REQUEST 0x0C

/*...*/
#define GET_FRAME_REQUEST 0x0A
#define SET_OPTICAl_TRIGGER_REQUEST 0x0B

#define READ_FLASH_REQUEST 0x1A
#define WRITE_FLASH_REQUEST 0x1B
#define ERASE_FLASH_REQUEST 0x1C

#define RESET_REQUEST 0xF1
#define DETACH_REQUEST 0xF2

#define CORRECT_STATUS_REPLY 0x81
#define CORRECT_SET_EXPOSURE_REPLY 0x82
#define CORRECT_SET_ACQUISITION_PARAMETERS_REPLY 0x83
#define CORRECT_SET_FRAME_FORMAT_REPLY 0x84
#define CORRECT_SET_EXTERNAL_TRIGGER_REPLY  0x85
#define CORRECT_SET_SOFTWARE_TRIGGER_REPLY  0x86
#define CORRECT_CLEAR_MEMORY_REPLY 0x87
#define CORRECT_GET_FRAME_FORMAT_REPLY 0x88
#define CORRECT_GET_ACQUISITION_PARAMETERS_REPLY 0x89
#define CORRECT_SET_ALL_PARAMETERS_REPLY 0x8C
/*...*/
#define CORRECT_GET_FRAME_REPLY 0x8A
#define CORRECT_SET_OPTICAL_TRIGGER_REPLY 0x8B
#define CORRECT_READ_FLASH_REPLY 0x9A
#define CORRECT_WRITE_FLASH_REPLY 0x9B
#define CORRECT_ERASE_FLASH_REPLY 0x9C

typedef enum ScanMode_t {CONTINUOUS_MODE, FIRST_FRAME_IDLE_MODE, EVERY_FRAME_IDLE_MODE, FRAME_AVERAGING_MODE} ScanMode_t;
typedef enum ReductionMode_t {NO_AVERAGE, AVERAGE_OF_2, AVERAGE_OF_4, AVERAGE_OF_8} ReductionMode_t;
typedef enum EnableMode_t {EXTERNAL_TRIGGER_DISABLED, TRIGGER_ENABLED, ONE_TIME_TRIGGER} EnableMode_t;
typedef enum TriggerFront_t {FRONT_DISABLED, FRONT_RISING, FRONT_FALLING, BOTH_RISING_AND_FALLING} TriggerFront_t;
typedef enum OpticalTriggerMode_t {OPTICAL_TRIGGER_DISABLED, TRIGGER_FOR_FALLING_EDGE, TRIGGER_ON_THRESHOLD, ONE_TIME_TRIGGER_FOR_RISING_EDGE = 0x81, ONE_TIME_TRIGGER_FOR_FALLING_EDGE = 0x82} OpticalTriggerMode_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

typedef struct DeviceContext_t {
    hid_device*  handle;
    uint16_t numOfPixelsInFrame;
    char* serial;
} DeviceContext_t;

#ifndef DEVICE_INFO
#define DEVICE_INFO
typedef struct DeviceInfo_t{
      char* serialNumber;
      struct DeviceInfo_t *next;
} DeviceInfo_t;
#endif

extern const DeviceContext_t NULL_DEVICE_CONTEXT;

int connectToDeviceBySerial(const char * const serialNumber,  uintptr_t* deviceContextPtr);

int _verifyDeviceContextByPtr(const uintptr_t* const deviceContextPtr);

int _reconnect(uintptr_t* deviceContextPtr);
void _recursiveClearing(DeviceInfo_t * const devices);
int _tryWrite(unsigned char* const report, uintptr_t* deviceContextPtr);
int _tryRead(unsigned char * const report, unsigned char correctAnswer, uint16_t timeout, uintptr_t* deviceContextPtr);
int _writeOnlyFunction(unsigned char * const report, uintptr_t* deviceContextPtr);
int _writeReadFunction(unsigned char* const report, uint8_t correctReply, uint16_t timeout, uintptr_t* deviceContextPtr);

#endif
