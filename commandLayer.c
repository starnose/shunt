/*
 * commandLayer.c
 *
 *  Created on: 8 Sep 2014
 *      Author: dhicks
 *
 * Code and send individual commands to the USIP
 */

#include "shunt.h"
#include "sessionLayer.h"
#include "commandLayer.h"

#define COMMAND_ERR_NO             0x00000000
#define COMMAND_ERR_INVAL          0xEAFFFFFF
#define COMMAND_ERR_ALREADY        0XC9FEFFFF

#define MAX_COMMAND_LENGTH         (0xFFFF - 4)

/*
 * sendCommandAndReceiveResponse
 *
 * Attempt to send a command and receive a reply
 * 'response' is a 4-byte return-code buffer.
 * This is not useful for SIGN_CHECK_FLASH but should work for everything else
 */
static ERRORCODE sendCommandAndReceiveResponse(sessionDetails* session, uint8_t* cmd, uint16_t cmdLength, uint8_t** respData, uint16_t* respLength)
{
    ERRORCODE retCode;
    uint8_t*  responseMessage;
    uint16_t  responseLength;
    uint16_t  embeddedLength;
    uint32_t  errCode;
    uint8_t   retries;

    retCode = ERR_AGAIN;
    retries = 0;

    while ((retCode == ERR_AGAIN) && (retries++ < 100))
    {
        retCode = sendSessionData(session, cmd, cmdLength);
        if (retCode == SUCCESS)
        {
            debug("Command send success\n");
            retCode = receiveSessionData(session, &responseMessage, &responseLength);
            if (retCode == SUCCESS)
            {
                debug("Got command response\n");
                hexDebug(responseMessage, responseLength);
                embeddedLength = responseMessage[1] + (responseMessage[0] << 8);
                if (embeddedLength != responseLength - 2)
                {
                   debug("Response lengths don't match - %u %u\n", responseLength, embeddedLength);
                   /*
                    * Command seems to have failed, retry
                    */
                   retCode = ERR_AGAIN;
                   continue;
                }
                if(responseLength >= 6)
                {
                    errCode = (((((responseMessage[responseLength - 4] << 8) + responseMessage[responseLength - 3]) << 8)
                                       + responseMessage[responseLength - 2]) << 8) + responseMessage[responseLength - 1];
                    switch (errCode)
                    {
                    case COMMAND_ERR_NO:
                        debug("ERR_NO\n");
                        retCode = SUCCESS;
                        if(responseLength > 6 && respData != NULL)
                        {
                            *respData = (uint8_t*) malloc(responseLength - 6);
                            if (!respData)
                            {
                                debug("Could not allocate response buffer! %d\n", errno);
                                retCode = ERR_NO_MEM;
                            }
                            else
                            {
                                memcpy(*respData, responseMessage + 2, responseLength - 4);
                                *respLength = responseLength - 6;
                            }
                        }
                        break;

                    case COMMAND_ERR_INVAL:
                        debug("ERR_INVAL\n");
                        retCode = ERR_COMMAND_INVAL;
                        break;

                    case COMMAND_ERR_ALREADY:
                        debug("ERR_ALREADY\n");
                        retCode = ERR_COMMAND_ALREADY;
                        break;

                    default:
                        debug("UNKNOWN response code! 0x%x\n", errCode);
                        retCode = ERR_COMMAND_UNK;
                        break;
                    }
                }
                free(responseMessage);
            }
            else
            {
                debug("No command response received\n");
                retCode = ERR_AGAIN;
                continue;
            }
        }
    }
    return retCode;
}

/*
 * writeKey
 *
 * Write a key to OTP
 * Probably best not to use this...
 */
ERRORCODE writeKey (sessionDetails* session, uint8_t scope, uint8_t usage, uint8_t* key)
{
    uint8_t   command[19];
    ERRORCODE retCode;

    command[0] = COMMAND_WRITE_KEY;
    command[1] = scope;
    command[2] = usage;
    memcpy(command + 3, key, 16);

    retCode = sendCommandAndReceiveResponse(session, command, 19, NULL, NULL);

    return retCode;
}

/*
 * writeTimeout
 *
 * Set the ROM Bootloader timeout for phase 6.
 * Probably best not to use this...
 */
ERRORCODE writeTimeout (sessionDetails* session, uint16_t timeout)
{
    uint8_t   command[3];
    ERRORCODE retCode;

    command[0] = COMMAND_WRITE_TIMEOUT;
    command[1] = timeout >> 8;
    command[2] = timeout & 0xff;

    retCode = sendCommandAndReceiveResponse(session, command, 3, NULL, NULL);

    return retCode;
}

/*
 * updateLifeCycle
 *
 * Set the USIP to the next lifecycle
 * Probably best not to use this...
 *
 * It's unclear if setting byte 2 to 0 is the same as levaing the field
 * empty. It may be better to send a one-byte command
 */
ERRORCODE updateLifeCycle (sessionDetails* session)
{
    uint8_t   command[2];
    ERRORCODE retCode;

    command[0] = COMMAND_UPDATE_LIFE_CYCLE;
    command[1] = 0;

    retCode = sendCommandAndReceiveResponse(session, command, 2, NULL, NULL);

    return retCode;
}

/*
 * writeFlash
 *
 * Write some data into flash
 *
 * address is expected to be in kseg1 VIRTUAL memory !!
 */
ERRORCODE writeFlash (sessionDetails* session, uint32_t address, uint8_t* data, uint16_t dataLength)
{
    uint8_t*  command;
    ERRORCODE retCode;
    uint32_t  fullLength = dataLength + 7;

    if (fullLength > MAX_COMMAND_LENGTH )
    {
        return ERR_COMMAND_LENGTH;
    }

    command = (uint8_t*)malloc(fullLength);
    if (!command)
    {
        return ERR_NO_MEM;
    }

    command[0] = COMMAND_WRITE_FLASH;
    command[1] = (address >> 24) & 0xFF;
    command[2] = (address >> 16) & 0xFF;
    command[3] = (address >> 8) & 0xFF;
    command[4] = address & 0xFF;
    command[5] = (dataLength >> 8) & 0xFF;
    command[6] = dataLength & 0xFF;
    memcpy(command + 7, data, dataLength);

    debug("Sending command of length %lu\n", fullLength);
    retCode = sendCommandAndReceiveResponse(session, command, fullLength, NULL, NULL);

    free(command);

    return retCode;
}

/*
 * eraseFlash
 *
 * erase a given sector. Will refuse to erase sector 35 unless override is set to true
 */
ERRORCODE eraseFlash (sessionDetails* session, uint8_t sector, bool override)
{
    ERRORCODE       retCode;
    uint8_t         command[2];

    if (sector > 35)
    {
        debug("Bad Sector\n");
        return ERR_BAD_SECTOR;
    }

    if ((sector == 35) && (override != true))
    {
        debug("Refusing to erase sector 35\n");
        return ERR_BAD_SECTOR;
    }

    command[0] =  COMMAND_ERASE_FLASH;
    command[1] =  sector;

    retCode = sendCommandAndReceiveResponse(session, command, 2, NULL, NULL);

    return retCode;
}

/*
 * blankCheckFlash
 *
 * Set the USIP to the next lifecycle
 * Probably best not to use this...
 *
 * It's unclear if setting byte 2 to 0 is the same as levaing the field
 * empty. It may be better to send a one-byte command
 */
ERRORCODE blankCheckFlash (sessionDetails* session)
{
    uint8_t   command;
    ERRORCODE retCode;

    command = COMMAND_BLANK_CHECK_FLASH;

    retCode = sendCommandAndReceiveResponse(session, &command, 1, NULL, NULL);

    return retCode;
}

/*
 * eraseFlash
 *
 * erase a given sector. Will refuse to erase sector 35 unless override is set to true
 */
ERRORCODE lockFlash (sessionDetails* session, uint8_t sector, bool override)
{
    ERRORCODE       retCode;
    uint8_t         command[2];

    if (sector > 35)
    {
        debug("Bad Sector\n");
        return ERR_BAD_SECTOR;
    }

    if ((sector == 35) && (override != true))
    {
        debug("Refusing to lock sector 35\n");
        return ERR_BAD_SECTOR;
    }

    command[0] =  COMMAND_LOCK_FLASH;
    command[1] =  sector;

    retCode = sendCommandAndReceiveResponse(session, command, 2, NULL, NULL);

    return retCode;
}

/*
 * verifyFlash
 *
 * Check the data in flash matches the given data
 *
 * address is expected to be in kseg1 VIRTUAL memory !!
 */
ERRORCODE verifyFlash (sessionDetails* session, uint32_t address, uint8_t* data, uint16_t dataLength)
{
    uint8_t*  command;
    ERRORCODE retCode;
    uint32_t  fullLength = dataLength + 7;

    if (fullLength > MAX_COMMAND_LENGTH )
    {
        return ERR_COMMAND_LENGTH;
    }

    command = (uint8_t*)malloc(fullLength);
    if (!command)
    {
        return ERR_NO_MEM;
    }

    command[0] = COMMAND_VERIFY_FLASH;
    command[1] = (address >> 24) & 0xFF;
    command[2] = (address >> 16) & 0xFF;
    command[3] = (address >> 8) & 0xFF;
    command[4] = address & 0xFF;
    command[5] = (dataLength >> 8) & 0xFF;
    command[6] = dataLength & 0xFF;
    memcpy(command + 7, data, dataLength);

    retCode = sendCommandAndReceiveResponse(session, command, fullLength, NULL, NULL);
    free(command);
    return retCode;
}

/*
 * signCheckFlash
 *
 * Generate a signature on the flash
 *
 * The signature is calculated using the 16-byte signature
 * algorithm described in Maxim Document - DS10H29. This is then
 * encrypted using a key generated from TKs (I'm not sure if DKUs is good enough)
 * xor'd with the unit's USN
 *
 * Signature is a 16 byte output buffer
 */
ERRORCODE signCheckFlash (sessionDetails* session, uint32_t address, uint32_t length, bool otp, uint8_t** signature)
{
    uint8_t   command[9];
    ERRORCODE retCode;
    uint16_t  sigLength;

    command[0] = COMMAND_SIGN_CHECK_FLASH;
    if (otp == true)
    {
        command[1] = 0;
        command[2] = 0;
        command[3] = 0;
        command[4] = 0;
    }
    else
    {
        command[1] = (address >> 24) & 0xFF;
        command[2] = (address >> 16) & 0xFF;
        command[3] = (address >> 8) & 0xFF;
        command[4] = address & 0xFF;
    }

    command[5] = (length >> 24) & 0xFF;
    command[6] = (length >> 16) & 0xFF;
    command[7] = (length >> 8) & 0xFF;
    command[8] = length & 0xFF;

    retCode = sendCommandAndReceiveResponse(session, command, 9, signature, &sigLength);

    if (retCode == SUCCESS)
    {
        if (sigLength != 32)
        {
            debug("Bad signature length encountered - %u\n", sigLength);
            if (*signature)
            {
                free(*signature);
                *signature = NULL;
            }
            retCode = ERR_SIG_LENGTH;
        }
        else
        {
            debug("got SIGN value - \n");
            hexDebug(*signature, 16);
            debug("got SSIGN value - \n");
            hexDebug((*signature) + 16, 16);
        }
    }
    return retCode;
}

/*
 * writeProcedure
 *
 * Write some data into RAM for a RCS procedure thing
 *
 * address is expected to be in kseg0 or kseg1 virtual memory space
 */
ERRORCODE writeProcedure (sessionDetails* session, uint32_t address, uint8_t* data, uint16_t dataLength)
{
    uint8_t*  command;
    ERRORCODE retCode;
    uint32_t  fullLength = dataLength + 7;

    if (fullLength > MAX_COMMAND_LENGTH )
    {
        return ERR_COMMAND_LENGTH;
    }

    command = (uint8_t*)malloc(fullLength);
    if (!command)
    {
        return ERR_NO_MEM;
    }

    command[0] = COMMAND_WRITE_PROCEDURE;
    command[1] = (address >> 24) & 0xFF;
    command[2] = (address >> 16) & 0xFF;
    command[3] = (address >> 8) & 0xFF;
    command[4] = address & 0xFF;
    command[5] = (dataLength >> 8) & 0xFF;
    command[6] = dataLength & 0xFF;
    memcpy(command + 7, data, dataLength);

    retCode = sendCommandAndReceiveResponse(session, command, fullLength, NULL, NULL);
    free(command);
    return retCode;
}

/*
 * registerProcedure
 *
 * register a procedure
 * address is expected to be in kseg0 or kseg1
 * opcode must not be one of the ones above
 */
ERRORCODE registerProcedure (sessionDetails* session, uint8_t opCode, uint32_t address)
{
    uint8_t   command[6];
    ERRORCODE retCode;

    switch (opCode)
    {
    case COMMAND_WRITE_KEY:
    case COMMAND_WRITE_TIMEOUT:
    case COMMAND_UPDATE_LIFE_CYCLE:
    case COMMAND_WRITE_FLASH:
    case COMMAND_ERASE_FLASH:
    case COMMAND_VERIFY_FLASH:
    case COMMAND_SIGN_CHECK_FLASH:
    case COMMAND_BLANK_CHECK_FLASH:
    case COMMAND_LOCK_FLASH:
    case COMMAND_WRITE_PROCEDURE:
    case COMMAND_REGISTER_PROCEDURE:
        debug("Bad Opcode!\n");
        return ERR_BAD_OPCODE;

    default:
        break;
    }

    command[0] = COMMAND_REGISTER_PROCEDURE;
    command[1] = opCode;
    command[2] = (address >> 24) & 0xFF;
    command[3] = (address >> 16) & 0xFF;
    command[4] = (address >> 8) & 0xFF;
    command[5] = address & 0xFF;

    retCode = sendCommandAndReceiveResponse(session, command, 6, NULL, NULL);

    return retCode;
}

/*
 * callCustomProcedure
 *
 * Call a previously registered RCS
 */
ERRORCODE callCustomProcedure (sessionDetails* session,  uint8_t   commandID, uint8_t* data, uint16_t dataLength,
                               uint8_t**       respData, uint16_t* respLength)
{
    uint8_t*  command;
    ERRORCODE retCode;
    uint32_t  fullLength = dataLength + 1;

    if (fullLength > MAX_COMMAND_LENGTH )
    {
        return ERR_COMMAND_LENGTH;
    }

    command = (uint8_t*)malloc(fullLength);
    if (!command)
    {
        return ERR_NO_MEM;
    }

    command[0] = commandID;
    memcpy(command + 1, data, dataLength);

    debug("*respData 0x%x\n", *respData);
    retCode = sendCommandAndReceiveResponse(session, command, fullLength, respData, respLength);
    free(command);
    debug("*respData 0x%x\n", *respData);
    return retCode;
}
