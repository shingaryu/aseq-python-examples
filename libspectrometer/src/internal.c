#include <stdlib.h>
#include "internal.h"

//hid_device*  g_Device = NULL;
//uint16_t g_numOfPixelsInFrame = 0;
//char* g_savedSerial = NULL;

const DeviceContext_t NULL_DEVICE_CONTEXT = { // or maybe FOO_DEFAULT or something
    NULL, 0, NULL
};

#define OK 0
#define CONNECT_ERROR_WRONG_ID 500
#define CONNECT_ERROR_NOT_FOUND 501
#define CONNECT_ERROR_FAILED 502
#define DEVICE_NOT_INITIALIZED 503
#define WRITING_PROCESS_FAILED 504
#define READING_PROCESS_FAILED 505
#define WRONG_ANSWER 506
#define GET_FRAME_REMAINING_PACKETS_ERROR 507
#define NUM_OF_PACKETS_IN_FRAME_ERROR 508
#define INPUT_PARAMETER_NOT_INITIALIZED 509
#define READ_FLASH_REMAINING_PACKETS_ERROR 510

#define CONNECT_ERROR_WRONG_SERIAL_NUMBER 516
#define NO_DEVICE_CONTEXT_ERROR 585

int _verifyDeviceContextByPtr(const uintptr_t* const deviceContextPtr)
{
    if (!deviceContextPtr) {
        return NO_DEVICE_CONTEXT_ERROR;
    }

    if (*deviceContextPtr == 0) {
        return DEVICE_NOT_INITIALIZED;
    }

    return OK;
}

int _reconnect(uintptr_t *deviceContextPtr)
{
    int result = 0;
    DeviceContext_t* deviceContext = NULL;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    deviceContext = (DeviceContext_t*)(*deviceContextPtr);

    result = connectToDeviceBySerial(deviceContext->serial, deviceContextPtr);
    return result;
}

void _recursiveClearing(DeviceInfo_t * const devices)
{
    if (devices) {
        _recursiveClearing(devices->next);

        free(devices->serialNumber);
        devices->serialNumber = NULL;

        //NOTE: check if this step is neccessary
        if (devices->next) {
            free(devices->next);
            devices->next = NULL;
        }
    }
}

int _tryWrite(unsigned char* const report, uintptr_t *deviceContextPtr)
{
    int result = -1;
    int reconnectResult = -1;
    bool reconnectAttempted = false;

    DeviceContext_t *deviceContext = NULL;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    deviceContext = (DeviceContext_t*)(*deviceContextPtr);

    do {
        result = hid_write(deviceContext->handle, (const unsigned char*)report, EXTENDED_PACKET_SIZE);
        if (result != HID_OPERATION_WRITE_SUCCESS) {
            if (reconnectAttempted) {
                return WRITING_PROCESS_FAILED;
            }

            reconnectResult = _reconnect(deviceContextPtr);
            if (reconnectResult != OK) {
                return reconnectResult;
            }
            reconnectAttempted = true;
        }
    } while (result != HID_OPERATION_WRITE_SUCCESS);

    return OK;
}

int _tryRead(unsigned char * const report, unsigned char correctAnswer, uint16_t timeout, uintptr_t *deviceContextPtr)
{
    int result = -1;

    DeviceContext_t *deviceContext = NULL;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    deviceContext = (DeviceContext_t*)(*deviceContextPtr);

    result = hid_read_timeout(deviceContext->handle, report, EXTENDED_PACKET_SIZE, timeout);

    if (result != HID_OPERATION_READ_SUCCESS){
        return READING_PROCESS_FAILED;
    }

    if (report[0] != correctAnswer) {
        return WRONG_ANSWER;
    }

    return OK;
}

int _writeOnlyFunction(unsigned char* const report, uintptr_t *deviceContextPtr)
{
    int result = -1;
    DeviceContext_t *deviceContext = NULL;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    deviceContext = (DeviceContext_t*)(*deviceContextPtr);

    if (!deviceContext->handle) {
        result = _reconnect(deviceContextPtr);
        if (result != OK) {
            return result;
        }
    }

    result = _tryWrite(report, deviceContextPtr);
    return result;
}

int _writeReadFunction(unsigned char* const report, uint8_t correctReply, uint16_t timeout, uintptr_t *deviceContextPtr)
{
    int result = -1;

    DeviceContext_t *deviceContext = NULL;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    deviceContext = (DeviceContext_t*)(*deviceContextPtr);

    if (!deviceContext->handle) {
        result = _reconnect(deviceContextPtr);
        if (result != OK) {
            return result;
        }
    }

    result = _tryWrite(report, deviceContextPtr);
    if (result != OK) {
        return result;
    }

    result = _tryRead(report, correctReply, timeout, deviceContextPtr);
    return result;
}

