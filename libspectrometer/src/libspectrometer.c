#include "libspectrometer.h"
#include "internal.h"

#if defined(_WIN32)
#include <windows.h>

#if !defined (AS_CORE_LIBRARY)
    BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
    {
        switch(fdwReason) {
            case DLL_PROCESS_ATTACH:
                //g_Device = NULL;
                //g_numOfPixelsInFrame = 0;
                break;
            case DLL_PROCESS_DETACH:
                //if (g_Device) {
                //    hid_close(g_Device);
                //    g_Device = NULL;
                //}

                //if (g_savedSerial) {
                //    free(g_savedSerial);
                //    g_savedSerial = NULL;
                //}
                break;
        }
        return TRUE;
    }
#endif  //AS_CORE_LIBRARY
#else
    #include <stdlib.h>
    #include <string.h>


#endif //defined _WIN32



int disconnectDeviceContext(uintptr_t* deviceContextPtr)
{
    DeviceContext_t *deviceContext = NULL;

    if (!deviceContextPtr) {
        return OK;
    }

    deviceContext = (DeviceContext_t*)(*deviceContextPtr);

    hid_close(deviceContext->handle);
    free(deviceContext->serial);

    free(deviceContext);
    *deviceContextPtr = 0;

    return 0;
}


int connectToDeviceBySerial(const char * const serialNumber, uintptr_t* deviceContextPtr)
{
    wchar_t *serialWChar = NULL;
    int cLen = serialNumber? strlen(serialNumber) : 0;

    DeviceContext_t *deviceContext = NULL;

    if (!deviceContextPtr) {
        return NO_DEVICE_CONTEXT_ERROR;
    }

    deviceContext = (DeviceContext_t*)(*deviceContextPtr);
    free(deviceContext);

    deviceContext = malloc(sizeof(DeviceContext_t));
    *deviceContext = NULL_DEVICE_CONTEXT;

    if (cLen) {
        ++cLen;       //for \0
        serialWChar = calloc(sizeof(wchar_t) * cLen, sizeof(wchar_t));
        mbstowcs(serialWChar, serialNumber, cLen);
    }

    if (deviceContext->handle) {
        hid_close(deviceContext->handle);
        deviceContext->handle = NULL;
    }

    deviceContext->handle = hid_open(USBD_VID, USBD_PID, (const wchar_t *)serialWChar);
    if (deviceContext->handle == NULL) {
         free(serialWChar);
         return CONNECT_ERROR_FAILED;
    }

    if (serialWChar) {
        if (serialNumber != deviceContext->serial) {
            free(deviceContext->serial);
            deviceContext->serial = calloc(sizeof(char) * cLen, sizeof(char));
            strcpy(deviceContext->serial, serialNumber);
        }

        free(serialWChar);
    }

    *deviceContextPtr = (uintptr_t)deviceContext;

    return OK;
}

int connectToDeviceByIndex(unsigned int index, uintptr_t* deviceContextPtr)   //0..n-1
{
    int cBytesCount = 0, wcLen = 0;
    int count = 0;
    struct hid_device_info *devices = hid_enumerate(USBD_VID, USBD_PID),
                           *deviceIterator = devices;

    wchar_t *serialWChar = NULL;

    DeviceContext_t *deviceContext = NULL;

    if (!deviceContextPtr) {
        return NO_DEVICE_CONTEXT_ERROR;
    }

    deviceContext = (DeviceContext_t*)(*deviceContextPtr);
    free(deviceContext);
    *deviceContextPtr = 0;

    deviceContext = malloc(sizeof(DeviceContext_t));
    *deviceContext = NULL_DEVICE_CONTEXT;

    while (deviceIterator) {
        ++count;

        if (index == count - 1) {
            serialWChar = deviceIterator->serial_number;
            break;
        } else {
            deviceIterator = deviceIterator->next;
        }
    }

    if (!serialWChar) {
        hid_free_enumeration(devices);
        return CONNECT_ERROR_NOT_FOUND;
    }

    if (deviceContext->handle) {
        hid_close(deviceContext->handle);
        deviceContext->handle = NULL;
    }
    deviceContext->handle = hid_open(USBD_VID, USBD_PID, (const wchar_t *)serialWChar);

    if (deviceContext->handle == NULL) {
        hid_free_enumeration(devices);
        return CONNECT_ERROR_FAILED;
    }

    wcLen = wcslen(serialWChar);
    cBytesCount = wcstombs(NULL, serialWChar, wcLen);

    free(deviceContext->serial);
    deviceContext->serial = calloc(cBytesCount + 1, sizeof(char));
    wcstombs(deviceContext->serial, serialWChar, wcLen);

    hid_free_enumeration(devices);

    *deviceContextPtr = (uintptr_t)deviceContext;

    return OK;
}

uint32_t getDevicesCount()
{
    int count = 0;
    struct hid_device_info *devices = hid_enumerate(USBD_VID, USBD_PID),
                           *device = devices;

    while (device != NULL) {
        ++count;
        device = device->next;
    }

    hid_free_enumeration(devices);
    return count;
}

DeviceInfo_t *getDevicesInfo()
{
    DeviceInfo_t *resultList = NULL,
                        *current = NULL,
                        *parent = NULL;

    int cBytesCount = 0, wcLen = 0;

    struct hid_device_info *devices = hid_enumerate(USBD_VID, USBD_PID),
                           *device = devices;

    wchar_t *serialWChar = NULL;

    while (device) {
        current = malloc(sizeof(DeviceInfo_t));

        serialWChar = device->serial_number;
        wcLen = wcslen(serialWChar);
        cBytesCount = wcstombs(NULL, serialWChar, wcLen);

        current->serialNumber = calloc(cBytesCount + 1, sizeof(char));
        wcstombs(current->serialNumber, serialWChar, wcLen);
        current->next = NULL;

        if (resultList == NULL) {
            resultList = current;
        }

        if (parent != NULL) {
            parent->next = current;
        }

        parent = current;
        device = device->next;
    }

    hid_free_enumeration(devices);
    return resultList;
}

void clearDevicesInfo(struct DeviceInfo_t *devices)
{
    _recursiveClearing(devices);    
    free(devices);
}

/**
\details {
    sends:
    outReport[1]=4;
    outReport[2]=LO(startElement);
    outReport[3]=HI(startElement);
    outReport[4]=LO(endElement);
    outReport[5]=HI(endElement);
    outReport[6]=reduce;

    gets:
    inReport[0]=0x84;
    inReport[1]=errorCode;
    inReport[2]=LO(frameElements);
    inReport[3]=HI(frameElements);
}
*/
int setFrameFormat(uint16_t numOfStartElement, uint16_t numOfEndElement, uint8_t reductionMode, uint16_t *numOfPixelsInFrame, uintptr_t* deviceContextPtr)
{
    unsigned char report[EXTENDED_PACKET_SIZE];
    int result = -1;
    int errorCode = -1;
    DeviceContext_t *deviceContext = NULL;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    deviceContext = (DeviceContext_t*)(*deviceContextPtr);

    report[0] = ZERO_REPORT_ID;
    report[1] = SET_FRAME_FORMAT_REQUEST;
    report[2] = LOW_BYTE(numOfStartElement);
    report[3] = HIGH_BYTE(numOfStartElement);
    report[4] = LOW_BYTE(numOfEndElement);
    report[5] = HIGH_BYTE(numOfEndElement);
    report[6] = reductionMode;

    result = _writeReadFunction(report, CORRECT_SET_FRAME_FORMAT_REPLY, STANDARD_TIMEOUT_MILLISECONDS, deviceContextPtr);
    if (result != OK) {
        return result;
    }

    errorCode = report[1];
    if (!errorCode) {
        deviceContext->numOfPixelsInFrame = (report[3] << 8) | report[2];

        if (numOfPixelsInFrame) {
            *numOfPixelsInFrame = deviceContext->numOfPixelsInFrame;
        }
    }

    return errorCode;
}

/**
\details {

    sends:
    outReport[1] = 2;
    outReport[2] = exposure[0];
    outReport[3] = exposure[1];
    outReport[4] = exposure[2];
    outReport[5] = exposure[3];
    outReport[6] = force;

    gets:
    inReport[0] = 0x82;
    inReport[1] = errorCode;
}
*/
int setExposure(uint32_t timeOfExposure, uint8_t force, uintptr_t* deviceContextPtr)
{
    unsigned char report[EXTENDED_PACKET_SIZE];
    int result = -1;
    int errorCode = -1;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    report[0] = ZERO_REPORT_ID;
    report[1] = SET_EXPOSURE_REQUEST;
    report[2] = LOW_BYTE(LOW_WORD(timeOfExposure));           //(exposure >> 24) & 0xFF;
    report[3] = HIGH_BYTE(LOW_WORD(timeOfExposure));          //(exposure >> 16) & 0xFF;
    report[4] = LOW_BYTE(HIGH_WORD(timeOfExposure));          //(exposure >> 8)  & 0xFF;
    report[5] = HIGH_BYTE(HIGH_WORD(timeOfExposure));         //exposure & 0xFF;
    report[6] = force;

    result = _writeReadFunction(report, CORRECT_SET_EXPOSURE_REPLY, STANDARD_TIMEOUT_MILLISECONDS, deviceContextPtr);
    if (result != OK) {
        return result;
    }

    errorCode = report[1];
    return errorCode;
}

/**
    \details
    sends:
    outReport[1] = 3;
    outReport[2] = LO(scans);
    outReport[3] = HI(scans);
    outReport[4] = LO(blankScans);
    outReport[5] = HI(blankScans);
    outReport[6] = scanMode;

    gets:
    inReport[0]=0x83;
    inReport[1]=errorCode;

*/
int setAcquisitionParameters(uint16_t numOfScans, uint16_t numOfBlankScans, uint8_t/*ScanMode_t*/ scanMode, uint32_t timeOfExposure, uintptr_t* deviceContextPtr)
{
    uint8_t report[EXTENDED_PACKET_SIZE];
    int result = -1;
    int errorCode = -1;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    report[0] = ZERO_REPORT_ID;
    report[1] = SET_ACQUISITION_PARAMETERS_REQUEST;
    report[2] = LOW_BYTE(numOfScans);
    report[3] = HIGH_BYTE(numOfScans);
    report[4] = LOW_BYTE(numOfBlankScans);
    report[5] = HIGH_BYTE(numOfBlankScans);
    report[6] = scanMode;
    report[7] = LOW_BYTE(LOW_WORD(timeOfExposure));
    report[8] = HIGH_BYTE(LOW_WORD(timeOfExposure));
    report[9] = LOW_BYTE(HIGH_WORD(timeOfExposure));
    report[10] = HIGH_BYTE(HIGH_WORD(timeOfExposure));

    result = _writeReadFunction(report, CORRECT_SET_ACQUISITION_PARAMETERS_REPLY, STANDARD_TIMEOUT_MILLISECONDS, deviceContextPtr);
    if (result != OK) {
        return result;
    }

    errorCode = report[1];
    return errorCode;
}

int setMultipleParameters(uint16_t numOfScans, uint16_t numOfBlankScans, uint8_t /*ScanMode_t*/ scanMode, uint32_t timeOfExposure, uint8_t/*EnableMode_t*/ enableMode, uint8_t/*TriggerFront_t*/ signalFrontMode, uintptr_t* deviceContextPtr)
{
    unsigned char report[EXTENDED_PACKET_SIZE];
    int result = -1;
    int errorCode = -1;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    report[0] = ZERO_REPORT_ID;
    report[1] = SET_ALL_PARAMETERS_REQUEST;
    report[2] = LOW_BYTE(numOfScans);
    report[3] = HIGH_BYTE(numOfScans);
    report[4] = LOW_BYTE(numOfBlankScans);
    report[5] = HIGH_BYTE(numOfBlankScans);
    report[6] = scanMode;
    report[7] = LOW_BYTE(LOW_WORD(timeOfExposure));
    report[8] = HIGH_BYTE(LOW_WORD(timeOfExposure));
    report[9] = LOW_BYTE(HIGH_WORD(timeOfExposure));
    report[10] = HIGH_BYTE(HIGH_WORD(timeOfExposure));
    report[11] = enableMode;
    report[12] = signalFrontMode;

    result = _writeReadFunction(report, CORRECT_GET_ACQUISITION_PARAMETERS_REPLY, STANDARD_TIMEOUT_MILLISECONDS, deviceContextPtr);
    if (result != OK) {
        return result;
    }

    errorCode = report[1];
    return errorCode;
}

int setExternalTrigger(uint8_t /*EnableMode_t*/ enableMode, uint8_t /*TriggerFront_t*/ signalFrontMode, uintptr_t* deviceContextPtr)
{
    unsigned char report[EXTENDED_PACKET_SIZE];
    int result = -1;
    int errorCode = -1;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    report[0] = ZERO_REPORT_ID;
    report[1] = SET_EXTERNAL_TRIGGER_REQUEST;
    report[2] = enableMode;
    report[3] = signalFrontMode;

    result = _writeReadFunction(report, CORRECT_SET_EXTERNAL_TRIGGER_REPLY, STANDARD_TIMEOUT_MILLISECONDS, deviceContextPtr);
    if (result != OK) {
        return result;
    }

    errorCode = report[1];
    return errorCode;
}

/**
    \details
    outReport[1] = 0x0B;
    outReport[2] = triggerMode;
    outReport[3] = LO(pixel);
    outReport[4] = HI(pixel);
    outReport[5] = LO(threshold);
    outReport[6] = HI(threshold);

    inReport[0] = 0x8B;
    inReport[1] = errorCode;
*/
int setOpticalTrigger(uint8_t /*OpticalTriggerMode_t*/ enableMode, uint16_t pixel, uint16_t threshold, uintptr_t* deviceContextPtr)
{    
    uint8_t report[EXTENDED_PACKET_SIZE];
    int result = -1;
    int errorCode = -1;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    report[0] = ZERO_REPORT_ID;
    report[1] = SET_OPTICAl_TRIGGER_REQUEST;
    report[2] = enableMode;
    report[3] = LOW_BYTE(pixel);
    report[4] = HIGH_BYTE(pixel);
    report[5] = LOW_BYTE(threshold);
    report[6] = HIGH_BYTE(threshold);

    result = _writeReadFunction(report, CORRECT_SET_OPTICAL_TRIGGER_REPLY, STANDARD_TIMEOUT_MILLISECONDS, deviceContextPtr);
    if (result != OK) {
        return result;
    }

    errorCode = report[1];
    return errorCode;
}

int triggerAcquisition(uintptr_t* deviceContextPtr)
{
    unsigned char report[EXTENDED_PACKET_SIZE];
    int result = -1;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    report[0] = ZERO_REPORT_ID;
    report[1] = SET_SOFTWARE_TRIGGER_REQUEST;

    result = _writeOnlyFunction(report, deviceContextPtr);
    return result;
}


/**
    \details
    sends:
    outReport[0] = 0
    outReport[1] = 1;
    outReport[2] = 0;

    gets:
    inReport[0] = 0x81;
    inReport[1] = flags;
    inReport[2] = LO(framesInMemory);
    inReport[3] = HI(framesInMemory);
*/
int getStatus(uint8_t *statusFlags, uint16_t *framesInMemory, uintptr_t* deviceContextPtr)
{
    unsigned char report[EXTENDED_PACKET_SIZE];
    int result = -1;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    report[0] = ZERO_REPORT_ID;
    report[1] = STATUS_REQUEST;

    result = _writeReadFunction(report, CORRECT_STATUS_REPLY, STANDARD_TIMEOUT_MILLISECONDS, deviceContextPtr);
    if (result != OK) {
        return result;
    }

    if (statusFlags) {
        *statusFlags = report[1];
    }

    if (framesInMemory) {
        *framesInMemory = (report[3] << 8) | (report[2]);
    }

    return OK;
}

int getAcquisitionParameters(uint16_t* numOfScans, uint16_t* numOfBlankScans, uint8_t *scanMode, uint32_t* timeOfExposure, uintptr_t* deviceContextPtr)
{
    uint8_t report[EXTENDED_PACKET_SIZE];
    int result = -1;    

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    report[0] = ZERO_REPORT_ID;
    report[1] = GET_ACQUISITION_PARAMETERS_REQUEST;

    result = _writeReadFunction(report, CORRECT_GET_ACQUISITION_PARAMETERS_REPLY, STANDARD_TIMEOUT_MILLISECONDS, deviceContextPtr);
    if (result != OK) {
        return result;
    }

    if (numOfScans) {
        *numOfScans = (report[2] << 8) | report[1];
    }

    if (numOfBlankScans) {
        *numOfBlankScans = (report[4] << 8) | report[3];
    }

    if (scanMode) {
        *scanMode = report[5];
    }

    if (timeOfExposure) {
        *timeOfExposure = (report[9] << 24) | (report[8] << 16) | (report[7] << 8) | report[6];
    }

    return OK;
}


/**
\details
outReport[1] = 8;

inReport[0] = 0x88;
inReport[1] = LO(startElement);
inReport[2] = HI(startElement);
inReport[3] = LO(endElement);
inReport[4] = HI(endElement);
inReport[5] = redux;
inReport[6] = LO(numOfFrameElements);
inReport[7] = HI(numOfFrameElements);
*/
int getFrameFormat(uint16_t *numOfStartElement, uint16_t *numOfEndElement, uint8_t *reductionMode, uint16_t *numOfPixelsInFrame, uintptr_t* deviceContextPtr)
{
    unsigned char report[EXTENDED_PACKET_SIZE];
    int result = -1;
    DeviceContext_t *deviceContext = NULL;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    deviceContext = (DeviceContext_t*)(*deviceContextPtr);

    report[0] = ZERO_REPORT_ID;
    report[1] = GET_FRAME_FORMAT_REQUEST;

    result = _writeReadFunction(report, CORRECT_GET_FRAME_FORMAT_REPLY, STANDARD_TIMEOUT_MILLISECONDS, deviceContextPtr);
    if (result != OK) {
        return result;
    }

    if (numOfStartElement) {
        *numOfStartElement = (report[2] << 8) | report[1];
    }

    if (numOfEndElement) {
        *numOfEndElement = (report[4] << 8) | report[3];
    }

    if (reductionMode) {
        *reductionMode = report[5];
    }

    deviceContext->numOfPixelsInFrame = (report[7] << 8) | report[6];

    if (numOfPixelsInFrame) {
        *numOfPixelsInFrame = deviceContext->numOfPixelsInFrame;
    }

    return OK;
}

/**
\details
{
outReport[1]=0x0A;
outReport[2]=LO(offset);
outReport[3]=HI(offset);
outReport[4]=LO(frameNum);
outReport[5]=HI(frameNum);
outReport[6]=packets;

inReport[0]=0x8A;
inReport[1]=LO(offset);
inReport[2]=HI(offset);
inReport[3]=packetsRemaining_or_IsError;
inReport[4]=LO(frame[offset]);
inReport[5]=HI(frame[offset]);
inReport[6]=LO(frame[offset+1]);
inReport[7]=HI(frame[offset+1]);
...
inReport[62]=LO(frame[offset+29]);
inReport[63]=HI(frame[offset+29]);
}
*/
int getFrame(uint16_t *framePixelsBuffer, uint16_t numOfFrame, uintptr_t* deviceContextPtr)
{    
    uint8_t report[EXTENDED_PACKET_SIZE];
    int result = -1;

    /* Total frame request parameters: */
    uint16_t pixelOffset = 0;
    uint8_t numOfPacketsToGet = 0, numOfPacketsLeft = 0, numOfPacketsReceived = 0;

    bool continueGetInReport = true;
    uint16_t totalNumOfReceivedPixels = 0;

    uint8_t indexOfPixelInPacket = 0;
    int indexInPacket = 0;

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

    if (!framePixelsBuffer) {
        return INPUT_PARAMETER_NOT_INITIALIZED;
    }

    if (!deviceContext->numOfPixelsInFrame) {
        result = getFrameFormat(NULL, NULL, NULL, NULL, deviceContextPtr);
        if (result != OK)
            return result;
    }

    numOfPacketsToGet = (deviceContext->numOfPixelsInFrame) / NUM_OF_PIXELS_IN_PACKET;
    numOfPacketsToGet += (deviceContext->numOfPixelsInFrame % NUM_OF_PIXELS_IN_PACKET)? 1 : 0;

    if (numOfPacketsToGet > MAX_PACKETS_IN_FRAME) {
        return NUM_OF_PACKETS_IN_FRAME_ERROR;
    }

    report[0] = ZERO_REPORT_ID;
    report[1] = GET_FRAME_REQUEST;
    report[2] = LOW_BYTE(pixelOffset);
    report[3] = HIGH_BYTE(pixelOffset);
    report[4] = LOW_BYTE(numOfFrame);
    report[5] = HIGH_BYTE(numOfFrame);
    report[6] = numOfPacketsToGet;

    result = _tryWrite(report, deviceContextPtr);
    if (result != OK) {
        return result;
    }
    continueGetInReport = true;
    while (continueGetInReport) {
        result = hid_read_timeout(deviceContext->handle, report, EXTENDED_PACKET_SIZE, STANDARD_TIMEOUT_MILLISECONDS);
        if (result != HID_OPERATION_READ_SUCCESS){
            return READING_PROCESS_FAILED;
        }

        if (report[0] != CORRECT_GET_FRAME_REPLY) {
            return WRONG_ANSWER;
        }

        ++numOfPacketsReceived;

        numOfPacketsLeft = report[3];
        if (numOfPacketsLeft >= REMAINING_PACKETS_ERROR ||
            (numOfPacketsLeft != numOfPacketsToGet - numOfPacketsReceived)) {
            return GET_FRAME_REMAINING_PACKETS_ERROR;
        }

        if (numOfPacketsLeft != numOfPacketsToGet - numOfPacketsReceived) {
            return GET_FRAME_REMAINING_PACKETS_ERROR;
        }

        continueGetInReport = (numOfPacketsLeft > 0)? true : false;

        pixelOffset = (report[2] << 8) | report[1];

        indexInPacket = 4;
        indexOfPixelInPacket = 0;

        while ((totalNumOfReceivedPixels < deviceContext->numOfPixelsInFrame) && (indexOfPixelInPacket < NUM_OF_PIXELS_IN_PACKET)) {
            uint16_t pixel = (report[indexInPacket + 1] << 8) | report[indexInPacket];
            framePixelsBuffer[pixelOffset + indexOfPixelInPacket] = pixel;

            indexInPacket += 2;
            ++indexOfPixelInPacket;
            ++totalNumOfReceivedPixels;
        }
    }

    return OK;
}

/**
    \details
    outReport[0]=7;

    inReport[0]=0x87;
    inReport[1]=errorCode;

*/
int clearMemory(uintptr_t* deviceContextPtr)
{
    unsigned char report[EXTENDED_PACKET_SIZE];
    int result = -1;
    int errorCode = -1;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    report[0] = ZERO_REPORT_ID;
    report[1] = CLEAR_MEMORY_REQUEST;

    result = _writeReadFunction(report, CORRECT_CLEAR_MEMORY_REPLY, STANDARD_TIMEOUT_MILLISECONDS, deviceContextPtr);
    if (result != OK) {
        return result;
    }

    errorCode = report[1];
    return errorCode;
}

/**
\details
outReport[0] = 0x1C;

inReport[0] = 0x9C;
inReport[1] = errorCode;
*/
int eraseFlash(uintptr_t* deviceContextPtr)
{
    int result = -1;
    int errorCode = -1;
    uint8_t report[EXTENDED_PACKET_SIZE];

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    report[0] = ZERO_REPORT_ID;
    report[1] = ERASE_FLASH_REQUEST;

    result = _writeReadFunction(report, CORRECT_ERASE_FLASH_REPLY, ERASE_FLASH_TIMEOUT_MILLISECONDS, deviceContextPtr);
    if (result != OK) {
        return result;
    }

    errorCode = report[1];
    return errorCode;
}

/**
\details
outReport[1] = 0x1A;
outReport[2] = absoluteOffset[0];     //low byte
outReport[3] = absoluteOffset[1];     //little endian!
outReport[4] = absoluteOffset[2];
outReport[5] = absoluteOffset[3];     //high byte
outReport[6] = packets;

inReport[0] = 0x9A;
inReport[1] = LO(localOffset);
inReport[2] = HI(localOffset);
inReport[3] = packetsRemaining_or_IsError;
inReport[4] = flash[absoluteOffset + localOffset];
inReport[5] = flash[absoluteOffset + localOffset + 1];
..
inReport[63] = flash[absoluteOffset + localOffset + 59];
*/
int readFlash(uint8_t *buffer, uint32_t absoluteOffset, uint32_t bytesToRead, uintptr_t* deviceContextPtr)
{
    int result = -1;
    uint8_t report[EXTENDED_PACKET_SIZE];    

    uint32_t numOfPacketsToGet = 0;
    uint8_t numOfPacketsToGetCurrent = 0, numOfPacketsReceivedCurrent = 0, numOfPacketsLeftCurrent = 0;

    bool continueGetInReport = true;
    uint16_t localOffset = 0;
    uint32_t totalNumOfReceivedBytes = 0;

    uint8_t indexOfByteInPacket;
    int indexInPacket;

    uint32_t offsetIncrement = 0;
    uint8_t payloadSize = PACKET_SIZE - 4;

    DeviceContext_t *deviceContext = NULL;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    if (!buffer) {
        return INPUT_PARAMETER_NOT_INITIALIZED;
    }

    deviceContext = (DeviceContext_t*)(*deviceContextPtr);

    if (!deviceContext->handle) {
        result = _reconnect(deviceContextPtr);
        if (result != OK) {
            return result;
        }
    }

    numOfPacketsToGet = bytesToRead / payloadSize;
    numOfPacketsToGet += (bytesToRead % payloadSize)? 1 : 0;

    while(numOfPacketsToGet) {
        numOfPacketsToGetCurrent = (numOfPacketsToGet > MAX_READ_FLASH_PACKETS)? MAX_READ_FLASH_PACKETS : numOfPacketsToGet;

        report[0] = ZERO_REPORT_ID;
        report[1] = READ_FLASH_REQUEST;
        report[2] = LOW_BYTE(LOW_WORD(absoluteOffset + offsetIncrement));
        report[3] = HIGH_BYTE(LOW_WORD(absoluteOffset + offsetIncrement));
        report[4] = LOW_BYTE(HIGH_WORD(absoluteOffset + offsetIncrement));
        report[5] = HIGH_BYTE(HIGH_WORD(absoluteOffset + offsetIncrement));
        report[6] = numOfPacketsToGetCurrent;

        result = hid_write(deviceContext->handle, (const unsigned char*)report, EXTENDED_PACKET_SIZE);
        if (result != HID_OPERATION_WRITE_SUCCESS) {
            return WRITING_PROCESS_FAILED;
        }

        numOfPacketsReceivedCurrent = 0;
        continueGetInReport = true;
        while (continueGetInReport) {
            result = hid_read_timeout(deviceContext->handle, report, EXTENDED_PACKET_SIZE, STANDARD_TIMEOUT_MILLISECONDS);
            if (result != HID_OPERATION_READ_SUCCESS){
                return READING_PROCESS_FAILED;
            }

            ++numOfPacketsReceivedCurrent;

            if (report[0] != CORRECT_READ_FLASH_REPLY) {
                return WRONG_ANSWER;
            }

            numOfPacketsLeftCurrent = report[3];

            if (numOfPacketsLeftCurrent >= REMAINING_PACKETS_ERROR || (numOfPacketsLeftCurrent != numOfPacketsToGetCurrent - numOfPacketsReceivedCurrent)) {
                return READ_FLASH_REMAINING_PACKETS_ERROR;
            }
            continueGetInReport = (numOfPacketsLeftCurrent > 0)? true : false;

            localOffset = (report[2] << 8) | report[1];

            indexInPacket = 4;
            indexOfByteInPacket = 0;

            while ((totalNumOfReceivedBytes < bytesToRead) && (indexOfByteInPacket < payloadSize)) {
                buffer[offsetIncrement + localOffset + indexOfByteInPacket++] = report[indexInPacket++];
                ++totalNumOfReceivedBytes;
            }
        }

        if (numOfPacketsToGet > MAX_READ_FLASH_PACKETS) {
            numOfPacketsToGet -= MAX_READ_FLASH_PACKETS;
            offsetIncrement += MAX_READ_FLASH_PACKETS * payloadSize;
        } else {
            numOfPacketsToGet = 0;
        }
    }

    return OK;
}

/**
    \details
    outReport[1] = 0x1B;
    outReport[2] = offset[0];     //low byte
    outReport[3] = offset[1];     //little endian!
    outReport[4] = offset[2];
    outReport[5] = offset[3];     //high byte
    outReport[6] = numberOfbytes;     // <= 58 (max payload length)

    outReport[7] = bytesToWrite[0]; //payload begin
    outReport[8] = bytesToWrite[1];
    ...
    outReport[n] = bytesToWrite[m]; //payload end
    //n = numberOfbytes + 6, m = numberOfbytes - 1;

    inReport[0] = 0x9B;
    inReport[1] = errorCode;

*/
int writeFlash(uint8_t *buffer, uint32_t absoluteOffset, uint32_t bytesToWrite, uintptr_t* deviceContextPtr)
{
    int result = -1;
    uint8_t report[EXTENDED_PACKET_SIZE];
    uint8_t errorCode = -1;

    uint32_t index, byteIndex = 0;
    uint32_t bytesLeftToWrite = bytesToWrite;

    DeviceContext_t *deviceContext = NULL;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    if (!buffer) {
        return INPUT_PARAMETER_NOT_INITIALIZED;
    }

    deviceContext = (DeviceContext_t*)(*deviceContextPtr);

    if (deviceContext->handle == NULL) {
        result = _reconnect(deviceContextPtr);
        if (result != OK) {
            return result;
        }
    }    

    while (bytesLeftToWrite) {
        report[0] = ZERO_REPORT_ID;
        report[1] = WRITE_FLASH_REQUEST;
        report[2] = LOW_BYTE(LOW_WORD(absoluteOffset));
        report[3] = HIGH_BYTE(LOW_WORD(absoluteOffset));
        report[4] = LOW_BYTE(HIGH_WORD(absoluteOffset));
        report[5] = HIGH_BYTE(HIGH_WORD(absoluteOffset));
        report[6] = (bytesLeftToWrite > MAX_FLASH_WRITE_PAYLOAD) ? MAX_FLASH_WRITE_PAYLOAD : bytesLeftToWrite;

        index = 7;
        while ((byteIndex < bytesToWrite) && (index < EXTENDED_PACKET_SIZE)) {
            report[index++] = buffer[byteIndex++];
        }

        result = hid_write(deviceContext->handle, (const unsigned char*)report, EXTENDED_PACKET_SIZE);
        if (result != HID_OPERATION_WRITE_SUCCESS) {
            return WRITING_PROCESS_FAILED;
        }

        result = hid_read_timeout(deviceContext->handle, report, EXTENDED_PACKET_SIZE, STANDARD_TIMEOUT_MILLISECONDS);
        if (result != HID_OPERATION_READ_SUCCESS){
            return READING_PROCESS_FAILED;
        }

        if (report[0] != CORRECT_WRITE_FLASH_REPLY) {
            return WRONG_ANSWER;
        }

        errorCode = report[1];
        if (errorCode != OK) {
            return errorCode;
        }

        if (bytesLeftToWrite > MAX_FLASH_WRITE_PAYLOAD) {
            bytesLeftToWrite -= MAX_FLASH_WRITE_PAYLOAD;
            absoluteOffset += MAX_FLASH_WRITE_PAYLOAD;
        } else
            bytesLeftToWrite = 0;
    }

    return errorCode;
}

int resetDevice(uintptr_t* deviceContextPtr)
{
    unsigned char report[EXTENDED_PACKET_SIZE];
    int result = -1;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    report[0] = ZERO_REPORT_ID;
    report[1] = RESET_REQUEST;

    result = _writeOnlyFunction(report, deviceContextPtr);
    return result;
}

int detachDevice(uintptr_t* deviceContextPtr)
{
    unsigned char report[EXTENDED_PACKET_SIZE];
    int result;

    result = _verifyDeviceContextByPtr(deviceContextPtr);
    if (result != OK)
        return result;

    report[0] = ZERO_REPORT_ID;
    report[1] = DETACH_REQUEST;

    result = _writeOnlyFunction(report, deviceContextPtr);
    return result;
}
