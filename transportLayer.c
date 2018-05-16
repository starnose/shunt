/*
 * transportLayer.c
 *
 *  Created on: 26 Aug 2014
 *      Author: dhicks
 */

#include "shunt.h"
#include "serial.h"
#include "dataLayer.h"
#include "transportLayer.h"

// Protocol -
#define CON_REQ       0x01 // Connection request
#define CON_REP       0x02 // Connection reply
#define DISC_REQ      0x03 // Disconnection request
#define DISC_REP      0x04 // Disconnection reply
// Data Transfer -
#define DATA_TRANSFER 0x05 // Data Exchange
#define ACK           0x06 // Acknowledge
// Maintenance -
#define ECHO_REQ      0x0B // Echo request
#define ECHO_REP      0x0C // Echo reply
// Configuration -
#define CHG_SP_0      0x07 // Change speed to 57600 b/s
#define CHG_SP_1      0x17 // Change speed to 115200 b/s
#define CHG_SP_2      0x27 // Change speed to 230400 b/s
#define CHG_SP_3      0x37 // Change speed to 345600 b/s
#define CHG_SP_4      0x47 // Change speed to 460800 b/s
#define CHG_SP_5      0x57 // Change speed to 576000 b/s
#define CHG_SP_6      0x67 // Change speed to 691200 b/s
#define CHG_SP_7      0x77 // Change speed to 806400 b/s
#define CHG_SP_8      0x87 // Change speed to 921600 b/s
#define CHG_SP_REP    0x08 // Change speed reply

#define CHAN_ID       0x09 // Channel ID used by Maxim flashloader

#define RETRANSMISSION_ATTEMPTS 5

ERRORCODE connectTransportLayer (serialSession* serialPort, transportConnection* con)
{
    uint8_t   protocol;
    uint8_t   id;
    uint8_t   seq;
    uint8_t*  data;
    uint16_t  dataLength;
    uint8_t   retries;

    ERRORCODE errorCode;

    retries = RETRANSMISSION_ATTEMPTS;

    con->chanID = CHAN_ID;
    con->lastSeq = 0;
    con->serialPort = serialPort;

    while(retries)
    {
        //CON_REQ
        errorCode = sendDataLayerPacket(con->serialPort, CON_REQ, con->chanID, con->lastSeq, NULL, 0);
        if (errorCode != SUCCESS)
        {
            return errorCode;
        }

        //CON_REP
        errorCode = receiveDataLayerPacket(con->serialPort, &protocol, &id, &seq, &data, &dataLength, 3);
        if (errorCode != SUCCESS)
        {
            debug("Receive failed - %d\n", errorCode);
            // TODO - if TIMEOUT then retry
            if (errorCode == ERR_SERIAL_TIMEOUT)
            {
                retries--;
                continue;
            }
            else
            {
                return errorCode;
            }
        }

        if ((protocol != CON_REP) || (dataLength != 0) || (id != con->chanID) || (seq != con->lastSeq) || (data != NULL))
        {
            if (data != NULL)
            {
                free(data);
            }
            // something isn't right
            return ERR_CONREP;
        }

        //ACK
        errorCode = sendDataLayerPacket(con->serialPort, ACK, con->chanID, con->lastSeq, NULL, 0);
        if (errorCode != SUCCESS)
        {
            return errorCode;
        }
        break;
    }

    debug("Connected to USIP!\n");

    return SUCCESS;
}

ERRORCODE disconnectTransportLayer (transportConnection* con)
{
    ERRORCODE errorCode;

    uint8_t   protocol;
    uint8_t   id;
    uint8_t   seq;
    uint8_t*  data;
    uint16_t  dataLength;

    MOD_INCREMENT(con->lastSeq, 16);

    errorCode = sendDataLayerPacket(con->serialPort, DISC_REQ, con->chanID, con->lastSeq, NULL, 0);
    if (errorCode != SUCCESS)
    {
        return errorCode;
    }

    receiveDataLayerPacket(con->serialPort, &protocol, &id, &seq, &data, &dataLength, RETRANSMISSION_TIMEOUT);

    return SUCCESS;
}

ERRORCODE sendTransportData (transportConnection* con, uint8_t* data, uint16_t dataLength)
{
    ERRORCODE errorCode;
    uint8_t*  respData;
    uint16_t  respLength;
    uint8_t   protocol;
    uint8_t   id;
    uint8_t   seq;
    uint8_t   retries;

    retries = RETRANSMISSION_ATTEMPTS;

    while(retries)
    {
        errorCode = sendDataLayerPacket(con->serialPort, DATA_TRANSFER, con->chanID, con->lastSeq, data, dataLength);
        if (errorCode != SUCCESS)
        {
            return errorCode;
        }

        // hopefully receive an ACK
        respData = NULL;
        errorCode = receiveDataLayerPacket(con->serialPort, &protocol, &id, &seq, &respData, &respLength, RETRANSMISSION_TIMEOUT);
        if (respData)
        {
            free(respData);
        }

        if (errorCode == SUCCESS || errorCode == ERR_VALIDATION)
        {
            if (protocol == ACK)
            {
                debug("Received ACK\n");
            }
            else
            {
                debug("Received garbled or broken message, protocol %u\n", protocol);
                errorCode = SUCCESS;
            }
            break;
        }

        retries--;
    }

    MOD_INCREMENT(con->lastSeq, 16);

    return errorCode;
}

ERRORCODE receiveTransportData (transportConnection* con, uint8_t** data, uint16_t* length)
{
    ERRORCODE errorCode;
    uint8_t   protocol;
    uint8_t   seq;
    uint8_t   id;

 //   while(1)
   // {
        //Receive a packet
        errorCode = receiveDataLayerPacket(con->serialPort, &protocol, &id, &seq, data, length, RETRANSMISSION_TIMEOUT);
        if (errorCode != SUCCESS)
        {
            // ACK the damn thing anyway
            sendDataLayerPacket(con->serialPort, ACK, con->chanID, con->lastSeq, NULL, 0);
        }
        else
        {
            errorCode = sendDataLayerPacket(con->serialPort, ACK, con->chanID, con->lastSeq, NULL, 0);
        }
 //   if (protocol != DATA_TRANSFER)
 //       {
 //           continue;
 //       }
 //       break;
 //   }
    MOD_INCREMENT(con->lastSeq, 16);

    return errorCode;
}

ERRORCODE transportLayerPing (transportConnection* con)
{
    ERRORCODE errorCode;
    uint8_t*  respData;
    uint16_t  dataLength;
    uint8_t   protocol;
    uint8_t   id;
    uint8_t   seq;

    MOD_INCREMENT(con->lastSeq, 16);

    respData = NULL;
    dataLength = 0;

    errorCode = sendDataLayerPacket(con->serialPort, ECHO_REQ, con->chanID, con->lastSeq, (uint8_t*)"Banana!", 8);
    if (errorCode != SUCCESS)
    {
        return errorCode;
    }

    // should receive ECHO_RESP
    errorCode = receiveDataLayerPacket(con->serialPort, &protocol, &id, &seq, &respData, &dataLength, 1);
    if (respData)
    {
        debug("Ping response - %s\n", respData);
        free(respData);
    }

    if (errorCode != SUCCESS)
    {
        debug("Ping fail!\n");
        return errorCode;
    }

    debug("Ping successful!\n");
    return SUCCESS;
}
