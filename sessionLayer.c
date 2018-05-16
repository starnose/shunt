/*
 * sessionLayer.c
 *
 *  Created on: 27 Aug 2014
 *      Author: dhicks
 */

#include "shunt.h"
#include "cmac.h"
#include "utils.h"
#include "transportLayer.h"
#include "sessionLayer.h"

#define PROTECTION_CLEAR_UNSIGNED 0x00
#define PROTECTION_CLEAR_RMAC     0x01
#define PROTECTION_AES_RMAC       0x02
#define PROTECTION_CLEAR_CMAC     0x05
#define PROTECTION_AES_CMAC       0x06

#define COMMAND_HELLO     0x01 // Hello-Request
#define COMMAND_HELLO_REP 0x02 // Hello-Reply
#define COMMAND_CHALLENGE 0x07 // Challenge-Request
#define COMMAND_SUCCESS   0x03 // Operation successful
#define COMMAND_FAILURE   0x04 // Operation failed
#define COMMAND_DATA      0x05 // Data Transfer

struct _sessionDetails
{
    transportConnection connection;
    uint8_t             protection;
    uint8_t             transID;
    uint8_t             key[16];
};

/*
 * sendHello
 *
 * Perform the hello exchange
 * non-static function as get USN will call directly
 */
ERRORCODE initSession(serialSession* serialPort, uint8_t** respData, uint16_t* respLen, uint8_t* key, sessionDetails** retDetails)
{
    ERRORCODE         errorCode;
    uint8_t           command[12];
    sessionDetails*   details;
    struct helloResp* rsp;

    details = (struct _sessionDetails*)malloc(sizeof(struct _sessionDetails));
    if(!details)
    {
        return ERR_NO_MEM;
    }

    memcpy(details->key, key, 16);

    details->protection = PROTECTION_CLEAR_UNSIGNED;
    details->transID = 0;

    errorCode = connectTransportLayer(serialPort, &(details->connection));
    if(errorCode != SUCCESS)
    {
        free(details);
        return errorCode;
    }

    command[0] = (COMMAND_HELLO << 4);
    command[1] = 0;
    command[2] = 0;
    command[3] = 8;
    memcpy(command + 4, "HI-USIP", 8);

    errorCode = sendTransportData(&(details->connection), command, 12);
    if (errorCode == SUCCESS)
    {
        errorCode = receiveTransportData(&(details->connection), respData, respLen);
        if (errorCode == SUCCESS)
        {
            if (respData && (*respLen > 0))
            {
                debug("Received Hello Response - \n");
                hexDebug(*respData, *respLen);
                *retDetails = details;

                rsp = (struct helloResp*)(*respData);
                debug("----------------------------------------------------\n");
                debug("Unit Data:\n");
                debug("Lifecycle stage - %d\n", rsp->lifeCycle);
                debug("USIP Version    - %d.%d\n", rsp->usipMajorVersion, rsp->sblMajorVersion);
                debug("SBL Version     - %d.%d\n", rsp->sblMajorVersion, rsp->sblMinorVersion);
                debug("HAL Version     - %d.%d\n", rsp->halMajorVersion, rsp->halMinorVersion);
                debug("USN             - %2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x\n",
                        rsp->usn[0],rsp->usn[1],rsp->usn[2],rsp->usn[3],rsp->usn[4],rsp->usn[5],rsp->usn[6],rsp->usn[7],
                        rsp->usn[8],rsp->usn[9],rsp->usn[10],rsp->usn[11],rsp->usn[12],rsp->usn[13],rsp->usn[14],rsp->usn[15]);
                debug("Random data     - %2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x\n",
                        rsp->random[0],rsp->random[1],rsp->random[2],rsp->random[3],rsp->random[4],rsp->random[5],rsp->random[6],rsp->random[7],
                        rsp->random[8],rsp->random[9],rsp->random[10],rsp->random[11],rsp->random[12],rsp->random[13],rsp->random[14],rsp->random[15]);
                debug("----------------------------------------------------\n");
            }
            else
            {
                debug("No Hello Response Data received!\n");
                free(details);
            }
        }
        else
        {
            debug("No Hello Response received! %d\n", errorCode);
            free(details);
        }
    }
    else
    {
        debug("Failed to send transport Hello %d\n", errorCode);
        free(details);
    }
    return errorCode;
}

/*
 * challengeSequence
 *
 * Send a challenge request and process the response
 */
ERRORCODE challengeSequence(sessionDetails* session, uint8_t* challengeData)
{
    uint8_t   command[20];
    uint8_t   cache[16];
    ERRORCODE retCode;
    uint8_t*  respData;
    uint16_t  respLength;

    command[0] = (COMMAND_CHALLENGE << 4) | session->protection;
    command[1] = 0x00;
    command[2] = 0x00;
    command[3] = 0x10;

    memcpy(cache, challengeData, 16);
    hexDebug(challengeData, 16);
    cache[0] ^= session->protection;

    retCode = aesEncrypt(command + 4, cache, session->key);
    if (retCode != SUCCESS)
    {
        return retCode;
    }

    retCode = sendTransportData(&(session->connection), command, 20);
    if (retCode == SUCCESS)
    {
        retCode = receiveTransportData(&(session->connection), &respData, &respLength);
        if (retCode == SUCCESS)
        {
            debug("Received message -\n");
            hexDebug(respData, respLength);
            if (respLength == 4)
            {
                if (((respData[0]) >> 4) == COMMAND_SUCCESS)
                {
                    debug("Challenge Success!\n");
                    retCode = SUCCESS;
                }
                else
                {
                    debug("Challenge fail\n");
                    retCode = ERR_CHALLENGE_FAIL;
                }
            }
            else
            {
                debug("Bad length - %u\n", respLength);
            }
            if(respData) free(respData);
        }
        else
        {
            debug("Failed to receive challenge response %d\n", retCode);
        }
    }
    else
    {
        debug("Failed to send transport Challenge %d\n", retCode);
    }
    return retCode;
}

/*
 * startSessionlayer
 *
 * Connect a new session
 */
ERRORCODE startSessionLayer(serialSession* serialPort, uint8_t* key, sessionDetails** retDetails)
{
    ERRORCODE retCode;
    uint8_t*  respData;
    uint16_t  respLength;

    retCode = initSession(serialPort, &respData, &respLength, key, retDetails);
    if (retCode != SUCCESS)
    {
        debug("Hello process failed\n");
        return retCode;
    }

    retCode = challengeSequence(*retDetails, respData + 34);
    free(respData);

    if (retCode != SUCCESS)
    {
        debug("Challenge process failed\n");
        free(*retDetails);
    }
    else
    {
        debug("startSessionLayer Success!\n");
    }
    return retCode;
}

/*
 * sendSessionData
 *
 * format, encrypt and send a session-layer data packet
 */
ERRORCODE sendSessionData (sessionDetails* session, uint8_t* data, uint16_t length)
{
    ERRORCODE retCode;
    uint8_t*  commandBody;
    uint16_t  commandLength;
    uint16_t  dataLength;


    debug("Data length %d\n", length);

    if (session->protection == PROTECTION_CLEAR_UNSIGNED)
    {
        commandLength = length + 4;
        dataLength = length;
    }
    else
    {
        commandLength = length + (16 - (length % 16)) + 4 + 16;
        dataLength = commandLength - 4;
    }

    commandBody = (uint8_t*)malloc(commandLength);

    debug("Command length %d\n", commandLength);

    commandBody[0] = (COMMAND_DATA << 4) | session->protection;
    commandBody[1] = (session->transID)++;
    commandBody[2] = dataLength >> 8;
    commandBody[3] = dataLength &0xFF;

    if (session->protection == PROTECTION_CLEAR_UNSIGNED)
    {
        memcpy(commandBody + 4, data, length);
    }
    else
    {
        retCode = aesPadAndEncryptEcb(commandBody + 4, data, length, session->key);
        if (retCode != SUCCESS)
        {
            debug("Failed to encrypt command %d", retCode);
            free(commandBody);
            return retCode;
        }

        generateCMac(data, length, session->key, commandBody + commandLength - 16);
    }

    retCode = sendTransportData(&(session->connection), commandBody, commandLength);
    free(commandBody);

    return retCode;
}

/*
 * receiveSessionData
 *
 * get a session level DATA packet
 * Only supports zero encryption for now
 */
ERRORCODE receiveSessionData (sessionDetails* session, uint8_t** data, uint16_t* length)
{
    ERRORCODE retCode;
    uint8_t*  respBody;
    uint16_t  respLength;

    retCode = receiveTransportData(&(session->connection), &respBody, &respLength);

    if (retCode == SUCCESS)
    {
        debug("Received DATA packet\n");
        /* Do some validation here? */

        if (respLength > 2)
        {
            *length = respLength - 2;
            *data = (uint8_t*)malloc(*length);
            memcpy(*data, respBody + 2, *length);
            free(respBody);
        }
    }
    else
    {
        debug("Failed to receive DATA packet\n");
    }
    return retCode;
}

/*
 * endSession
 *
 * end a session and clean up
 */
void endSession (sessionDetails* session)
{
    disconnectTransportLayer(&(session->connection));
    free(session);
}
