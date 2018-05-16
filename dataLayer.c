/*
 * dataLayer.c
 *
 *  Created on: 21 Aug 2014
 *      Author: dhicks
 */

#include "shunt.h"
#include "serial.h"
#include "utils.h"
#include "dataLayer.h"

#define PACKET_READ_TIMEOUT 2 * RETRANSMISSION_TIMEOUT

#define SYNC_BYTE1 0xBE
#define SYNC_BYTE2 0xEF
#define SYNC_BYTE3 0xED

typedef struct __attribute__((packed)) _dataLayerHeader
{
    uint8_t sync1;
    uint8_t sync2;
    uint8_t sync3;
    uint8_t control;
    uint8_t length1;
    uint8_t length2;
    uint8_t idSeq;
    uint8_t checksum;
} dataLayerHeader;

typedef struct _dataLayerPacket
{
    dataLayerHeader header;
    uint8_t*        data;
    uint16_t        dataLength;
    uint8_t         checksum[4];
} dataLayerPacket;

/*
 * calcChecksum
 *
 * Calculate the header checksum for a datalayer header frame
 *
 * Arguments:
 * header -  7 byte header
 *
 * Returns:
 *  one byte checksum
 */
static uint8_t calcHeaderChecksum (uint8_t* header)
{
    uint8_t output[16];

    generateAesCRC(output, header, 7);

    return output[0];
}

/*
 * calcDataChecksum
 *
 * Calculate the checksum for a datalayer data frame
 *
 * Arguments:
 * data       -  Data to send
 * dataLength - length of the data
 * checksum   - output field, 4 byte checksum
 *
 * Returns: None
 */
static void calcDataChecksum (uint8_t* data, uint16_t dataLength, uint8_t* checksum)
{
    uint8_t output[16];

    generateAesCRC(output, data, dataLength);

    // I DON'T KNOW WHY THIS IS BYTE-REVERSED
    //memcpy(checksum, output, 4);

    checksum[0] = output [3];
    checksum[1] = output [2];
    checksum[2] = output [1];
    checksum[3] = output [0];
}

/*
 * prepareHeader
 *
 * prepare a packet header
 *
 * Arguments:
 * header     - header structure to fill
 * protocol   - protocol byte
 * id         - id nibble
 * seq        - sequence number nibble
 * dataLength - length of the data for this packet
 *
 * Returns: None
 */
static void prepareHeader (dataLayerHeader* header, uint8_t protocol, uint8_t id, uint8_t sequence, uint16_t dataLength)
{
    header->sync1    = SYNC_BYTE1;
    header->sync2    = SYNC_BYTE2;
    header->sync3    = SYNC_BYTE3;
    header->control  = protocol;
    header->length1  = (dataLength >> 8) & 0xFF;
    header->length2  = dataLength & 0xFF;
    header->idSeq    = ((id & 0xF) << 4) + (sequence & 0xF);
    header->checksum = calcHeaderChecksum((uint8_t*)header);
}

/*
 * clearIncoming
 *
 * clear data out of the incoming serial buffer
 * This is called before sending a message, to
 * make sure the buffer is clear
 */
static void clearIncoming(serialSession* session)
{

    uint8_t         clearbuffer;
    int16_t         readBytes = 1;
    time_t          now = time(NULL);
    while((serialRead(session, &clearbuffer, 1, &readBytes) == SUCCESS) && (readBytes == 1) && (time(NULL) < now + 2));
}

/*
 * sendPacket
 *
 * send a data packet over the wire
 *
 * Arguments:
 * serialPort - file descriptor for open serial device
 * packet     - the packet to send
 *
 *
 */
static ERRORCODE sendPacket (serialSession* serialPort, dataLayerPacket* packet)
{
    ERRORCODE retCode;
    uint16_t sendLength;
    uint8_t* sendBuffer;

    sendLength = sizeof(dataLayerHeader);
    if ((packet->data != NULL) &&(packet->dataLength != 0))
    {
        sendLength += packet->dataLength + 4;
    }

    sendBuffer = (uint8_t*)malloc(sendLength);
    if (!sendBuffer)
    {
        return ERR_NO_MEM;
    }

    memcpy(sendBuffer, (uint8_t*)&(packet->header), sizeof(dataLayerHeader));
    if(packet->data != NULL)
    {
        memcpy(sendBuffer + sizeof(dataLayerHeader), packet->data, packet->dataLength);
        memcpy(sendBuffer + sizeof(dataLayerHeader) + packet->dataLength, packet->checksum, 4);
    }

    clearIncoming(serialPort);

    retCode = serialWrite (serialPort, sendBuffer, sendLength);
    free(sendBuffer);

    return retCode;
}


/*
 * traverseSyncBytes
 *
 * Find a header off the serial line
 * and read the sync bytes
 *
 * Arguments:
 * serialPort - file descriptor for open serial port
 * header     - header to read into
 * deaLine    - timeout deadLine for packet read
 *
 * Returns:
 *      error code, SUCCESS on success
 */
static ERRORCODE traverseSyncBytes (serialSession* serialPort, dataLayerHeader* header, time_t deadLine)
{
    uint8_t  retCode = ERR_SERIAL_TIMEOUT;
    uint8_t  syncbytes[] = {SYNC_BYTE1, SYNC_BYTE2, SYNC_BYTE3};
    uint8_t* syncptr;
    uint8_t* headerptr;
    int16_t  readBytes;

    syncptr   = syncbytes;
    headerptr = (uint8_t*)header;

    while ((syncptr < syncbytes + 3) && (time(NULL) <= deadLine))
    {
        retCode = serialRead(serialPort, headerptr, 1, &readBytes);
        if (retCode != SUCCESS)
        {
            continue;
        }

        if (*headerptr == *syncptr)
        {
            syncptr++;
            headerptr++;
        }
        else if (*headerptr == syncbytes[0])
        {
            syncptr = syncbytes + 1;
            headerptr = (uint8_t*)header;
            *headerptr = syncbytes[0];
            headerptr++;
        }
        else
        {
            debug("Throw away - %2.2x - %c\n", *headerptr, isprint(*headerptr)?*headerptr:'.');
            syncptr = syncbytes;
            headerptr = (uint8_t*)header;
        }
    }

    if (syncptr == syncbytes + 3)
    {
        retCode = SUCCESS;
    }
    else
    {
        debug("READ HEADER failed with error - %d\n", retCode);
    }

    return retCode;
}

/*
 * readUntil
 *
 * Read a number of bytes until timeout
 *
 * Arguments:
 * serialPort     - file descriptor for open serial port
 * buffer         - buffer to read into
 * remainingBytes - bytes to read
 * deadLine       - timeout deadLine for packet read
 *
 * Returns:
 *      error code, SUCCESS on success
 */
static ERRORCODE readUntil (serialSession* serialPort, uint8_t* buffer,  uint16_t remainingBytes, time_t deadLine)
{
    uint8_t  retCode = ERR_SERIAL_TIMEOUT;
    uint8_t* bufferptr;
    int16_t  readBytes;
    struct   timespec tinySleep;

    bufferptr = buffer;

    while ((time(NULL) <= deadLine))
    {
        tinySleep.tv_nsec = 10000;
        tinySleep.tv_sec  = 0;

        retCode = serialRead(serialPort, bufferptr, remainingBytes, &readBytes);

        if (retCode == ERR_SERIAL_NO_DATA)
        {
            nanosleep(&tinySleep, NULL);
            continue;
        }

        if (retCode == SUCCESS)
        {
            if (readBytes != remainingBytes)
            {
                remainingBytes -= readBytes;
                bufferptr += readBytes;
                nanosleep(&tinySleep, NULL);
                continue;
            }
            else
            {
                break;
            }
        }

        break;
    }

    return retCode;
}

/*
 * validateHeader
 *
 * validate a header checksum
 *
 * Arguments:
 * header - the header to validate
 *
 * Returns:
 *      error code, SUCCESS on success
 */
static ERRORCODE validateHeader (dataLayerHeader* header)
{
    uint8_t   checksum;
    ERRORCODE retCode;

    checksum = calcHeaderChecksum((uint8_t*)header);

    if (checksum != header->checksum)
    {
        debug("Header not validated\n");
        retCode = ERR_VALIDATION;
    }
    else
    {
        debug("Header validated\n");
        retCode = SUCCESS;
    }
    return retCode;
}


static ERRORCODE getBody (serialSession* serialPort, uint8_t* dataBody, uint16_t expLength, time_t deadLine)
{
    uint8_t retCode;
    uint8_t dataChecksum[4];
    uint8_t calcChecksum[4];

    retCode = readUntil(serialPort, dataBody, expLength, deadLine);
    if (retCode != SUCCESS)
    {
        debug("Failed to read body\n");
        return retCode;
    }

    retCode = readUntil(serialPort, dataChecksum, 4, deadLine);
    if (retCode != SUCCESS)
    {
        debug("Failed to read tail\n");
        return retCode;
    }

    calcDataChecksum(dataBody, expLength, calcChecksum);
    if (memcmp(calcChecksum, dataChecksum, 4) != 0)
    {
        debug("Failed checksum\n");
     //   retCode = ERR_VALIDATION;
    }

    return retCode;
}

/*
 * sendDataLayerPacket
 *
 * construct and send a data layer packet
 *
 * Arguments:
 * serialPort - file descriptor for open serial device
 * protocol   - protocol byte
 * id         - id nibble
 * seq        - sequence number nibble
 * data       - the data to send
 * dataLength - length of the data for this packet
 *
 * Returns:
 *      error code, SUCCESS on success;
 */
ERRORCODE sendDataLayerPacket (serialSession* serialPort, uint8_t protocol, uint8_t id, uint8_t sequence, uint8_t* data, uint16_t dataLength)
{
    dataLayerPacket packet;
    ERRORCODE       retCode;


    prepareHeader(&(packet.header), protocol, id, sequence, dataLength);

    packet.data       = data;
    packet.dataLength = dataLength;
    calcDataChecksum(packet.data, packet.dataLength, packet.checksum);

    retCode = sendPacket(serialPort, &packet);

    return retCode;
}

/*
 * sendDataLayerPacket
 *
 * construct and send a data layer packet
 *
 * Arguments:
 * serialPort - file descriptor for open serial device
 * protocol   - protocol byte
 * id         - id nibble
 * seq        - sequence number nibble
 * data       - the data to send
 * dataLength - length of the data for this packet
 *
 * Returns:
 *      error code, SUCCESS on success;
 */
ERRORCODE receiveDataLayerPacket (serialSession* serialPort, uint8_t* protocol, uint8_t* id, uint8_t* sequence, uint8_t** data, uint16_t* dataLength, uint8_t timeout)
{
    int             retCode;
    dataLayerHeader header;
    time_t          deadLine;
    uint16_t        expLength;
    uint8_t*        dataBody;

    deadLine = time(NULL) + timeout;

    retCode = traverseSyncBytes(serialPort, &header, deadLine);
    if (retCode != SUCCESS)
    {
        return retCode;
    }

    retCode = readUntil(serialPort, ((uint8_t*)&header) + 3, 5, deadLine);
    if (retCode != SUCCESS)
    {
        return retCode;
    }

    debug("Got header - \n");
    hexDebug((uint8_t*)&header, sizeof(header));

    retCode = validateHeader(&header);
    if (retCode != SUCCESS)
    {
        /* THIS IS A HACK for broken ACK packets */
        if (header.control == 0x06)
        {
            header.length1 = 0;
            header.length2 = 0;
            retCode = validateHeader(&header);
            if (retCode != SUCCESS)
            {
                debug("Could not validate header even after retry\n");
                return retCode;
            }
        }
        else
        {
            return retCode;
        }
    }

    expLength = (((uint16_t)(header.length1)) << 8) + header.length2;
    if (expLength > 0)
    {
        deadLine = time(NULL) + timeout;
        dataBody = (uint8_t*) malloc(expLength);

        retCode = getBody(serialPort, dataBody, expLength, deadLine);
        if (retCode != SUCCESS)
        {
            debug("Failed to get body\n");
            free(dataBody);
            return retCode;
        }
        debug("Got Data - \n");
        hexDebug(dataBody, expLength);
    }
    else
    {
        dataBody = NULL;
    }

    *protocol   = header.control;
    *id         = (header.idSeq >> 4) & 0xF;
    *sequence   = header.idSeq & 0x0F;
    *data       = dataBody;
    *dataLength = expLength;

    return SUCCESS;
}
