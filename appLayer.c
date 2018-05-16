/*
 * appLayer.c
 *
 *  Created on: 21 Aug 2014
 *      Author: dhicks
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "shunt.h"
#include "utils.h"
#include "serial.h"
#include "transportLayer.h"
#include "sessionLayer.h"
#include "commandLayer.h"
#include "appLayer.h"

/*
 * rawEraseSectors
 *
 * erase a set of sectors using an already open session
 */
static ERRORCODE rawEraseSectors(sessionDetails* details, uint8_t startSect, uint8_t endSect, bool override)
{
    ERRORCODE retCode;

    printf("Erasing sector - ");
    while (startSect <= endSect)
    {
        printf(" %d", startSect);
        fflush(stdout);
        retCode = eraseFlash(details, startSect, override);
        if (retCode != SUCCESS)
        {
            printf("\nFAILED erasing sector %d\n", startSect);
        }
        startSect++;
    }
    printf("\nComplete\n");

    return retCode;
}

/*
 * eraseSectors
 *
 * start a session and erase sectors
 */
ERRORCODE eraseSectors (serialSession* serialPort, uint8_t* key, uint8_t startSect, uint8_t endSect, bool override)
{
    ERRORCODE       retCode;
    sessionDetails* details;

    if ((startSect > 35) || (endSect > 35) || (startSect > endSect))
    {
        printf("Bad sector encountered - %d %d\n", startSect, endSect);
        return ERR_BAD_SECTOR;
    }

    retCode = startSessionLayer(serialPort, key, &details);
    if (retCode == SUCCESS)
    {

        retCode = rawEraseSectors(details, startSect, endSect, override);
        endSession(details);
    }
    else
    {
        printf("Session start fail\n");
    }

    return retCode;
}

/*
 * Map of sector number to size in K
 */
uint8_t sectorMap[] = {
                          32, 32, 32, 32,  4,  4,  4,  4,
                           4,  4,  4,  4,  4,  4,  4,  4,
                           4,  4,  4,  4,  4,  4,  4,  4,
                           4,  4,  4,  4,  4,  4,  4,  4,
                           4,  4,  4,  4
                      };

static uint32_t calcOffsetAddress(uint8_t startSect)
{
    uint32_t offset = 0;

    while (startSect)
    {
        offset += sectorMap[startSect] * 1024;
        startSect--;
    }
    return offset;
}

static uint8_t findSectorForAddr(uint32_t addr)
{
    uint32_t offset = 0;
    uint8_t  sectorNum = 0;

    while ((offset < addr) && (sectorNum < 36))
    {
        offset += sectorMap[sectorNum] * 1024;
        if (addr < offset)
        {
            break;
        }
        sectorNum++;
    }
    return sectorNum;
}

/*
 * flashProgram
 *
 * flash a program from a raw (bin) file
 *
 * This function is WAY too long
 */
ERRORCODE flashProgram (serialSession* serialPort, uint8_t* key, uint8_t offsetSect, char* imageFile, bool override, bool dryrun)
{
    struct stat     imageInfo;
    int             sysRet;
    int             imageStream;
    off_t           imageSize;
    uint64_t        imageCompare;
    uint32_t        percent;
    uint32_t        lastpercent;
    uint32_t        offsetAddr;
    uint32_t        endAddr;
    uint32_t        totalFlashSize;
    uint32_t        availableSize;
    ssize_t         readLength;
    uint8_t         buffer[CHUNK_SIZE];
    uint16_t        length;
    uint8_t         endSector;
    ERRORCODE       retCode;
    sessionDetails* details;

    sysRet = stat(imageFile, &imageInfo);

    if (sysRet != 0)
    {
        printf("Error getting image file details %d\n", errno);
        return ERR_STAT;
    }

    imageSize = imageInfo.st_size;
    offsetAddr = calcOffsetAddress(offsetSect);
    totalFlashSize = 0x40000;
    if (override == false)
    {
        totalFlashSize -= 4 * 1024;
    }
    availableSize = totalFlashSize - calcOffsetAddress(offsetSect);
    endAddr = offsetAddr + imageSize - 1;
    endSector = findSectorForAddr(endAddr);
    imageCompare = imageSize;

    debug("imageSize - %lld, offsetSect %u, offsetAddr %u, availableSize %u, endAddress %u, endSector %u\n", imageSize, offsetSect, offsetAddr, availableSize, endAddr, endSector);

    if (endSector > 35)
    {
        printf("Image goes beyond sector 35! End sector - %u\n", endSector);
        return ERR_TOO_BIG;
    }

    if ((endSector > 34) && (override == false))
    {
        printf("Refusing to set sector 35\n");
        return ERR_TOO_BIG;
    }

    if (imageSize > availableSize)
    {
        printf("Image too big for available space. Image size - %lld, available - %u\n", imageSize, availableSize);
        return ERR_TOO_BIG;
    }

    if (dryrun == true)
    {
        printf("DryRun - Would erase and flash sectors %u to %u\n", offsetSect, endSector);
        return SUCCESS;
    }

    imageStream = open(imageFile, O_RDONLY);

    if (imageStream == -1)
    {
        printf("Unable to open file %s, %d\n", imageFile, errno);
        return ERR_FILE_OPEN;
    }

    retCode = startSessionLayer(serialPort, key, &details);
    if (retCode == SUCCESS)
    {
        retCode = rawEraseSectors(details, offsetSect, endSector, override);
        if (retCode == SUCCESS)
        {
            lastpercent = 0;
            printf("Flashing image -\n");
            printf("0%%.....................50%%.....................100%%\n");
            while(imageSize)
            {
                percent = ((imageCompare - imageSize)*100)/imageCompare;
                if(imageSize > CHUNK_SIZE)
                {
                    length = CHUNK_SIZE;
                }
                else
                {
                    length = imageSize;
                }
                imageSize -= length;

                readLength = read(imageStream, buffer, length);
                if (readLength != length)
                {
                    printf("Unable to read %u bytes from file! Only got %ld\n", length, readLength);
                    retCode = ERR_FILE_READ;
                    break;
                }

                debug("Sending writeflash command with address 0x%x, length %u\n", offsetAddr + 0xa1000000, length);
                hexDebug(buffer, length);

                retCode = writeFlash(details, offsetAddr + 0xa1000000, buffer, length);
                if (retCode != SUCCESS)
                {
                    printf("!\n**Failed to write section 0x%x**\n", offsetAddr);
                    fflush(stdin);
                    break;
                }
                if(percent > lastpercent + 1)
                {
                    lastpercent = percent;
                    printf("#");
                    fflush(stdout);
                }
                offsetAddr += length;
            }
        }
        if (retCode == SUCCESS) printf("#\n");
        endSession(details);
    }
    else
    {
        printf("Session start failure\n");
    }

    close(imageStream);

    return retCode;
}

ERRORCODE testSessionLayer(serialSession* serialPort, uint8_t* key)
{
    sessionDetails* details;
    ERRORCODE       retCode;

    retCode = startSessionLayer(serialPort, key, &details);
    if (retCode == SUCCESS)
    {
        endSession(details);
    }

    return retCode;
}

ERRORCODE getUSN (serialSession* serialPort, uint8_t* key)
{
    uint8_t*          respData = NULL;
    uint16_t          respLen;
    ERRORCODE         errorCode;
    struct helloResp* rsp;
    sessionDetails*   details;

    errorCode = initSession(serialPort, &respData, &respLen, key, &details);

    if ((respData) && (respLen > 0) && (errorCode == SUCCESS))
    {
        rsp = (struct helloResp*) respData;
        printf("----------------------------------------------------\n");
        printf("USIP Unit Data:\n");
        printf("Lifecycle stage - %d\n", rsp->lifeCycle);
        printf("USIP Version    - %d.%d\n", rsp->usipMajorVersion, rsp->sblMajorVersion);
        printf("SBL Version     - %d.%d\n", rsp->sblMajorVersion, rsp->sblMinorVersion);
        printf("HAL Version     - %d.%d\n", rsp->halMajorVersion, rsp->halMinorVersion);
        printf("USN             - %2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x\n",
                rsp->usn[0],rsp->usn[1],rsp->usn[2],rsp->usn[3],rsp->usn[4],rsp->usn[5],rsp->usn[6],rsp->usn[7],
                rsp->usn[8],rsp->usn[9],rsp->usn[10],rsp->usn[11],rsp->usn[12],rsp->usn[13],rsp->usn[14],rsp->usn[15]);
        printf("Random data     - %2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x\n",
                rsp->random[0],rsp->random[1],rsp->random[2],rsp->random[3],rsp->random[4],rsp->random[5],rsp->random[6],rsp->random[7],
                rsp->random[8],rsp->random[9],rsp->random[10],rsp->random[11],rsp->random[12],rsp->random[13],rsp->random[14],rsp->random[15]);
        printf("----------------------------------------------------\n");
        free(details);
        free(respData);
    }

    return errorCode;
}

ERRORCODE pingUSIP (serialSession* serialPort)
{
    ERRORCODE errorCode;
    int pings;

    transportConnection con;
    pings = 5;

    do
    {
        debug("Attempt connection...\n");
        errorCode = connectTransportLayer(serialPort, &con);
        debug("Return %d\n", errorCode);
    } while ((errorCode == ERR_SERIAL_TIMEOUT) || (errorCode == ERR_SERIAL_NO_DATA));

    if (errorCode != SUCCESS)
    {
        return errorCode;
    }

    debug("Connection success!\n");

    while (pings)
    {
        debug("Ping...\n");
        errorCode = transportLayerPing(&con);
        if (errorCode != SUCCESS)
        {
            debug("Error.... timeout on response\n");
        }
        else
        {
            debug("Ping response!\n");
        }
        sleep(1);
        pings--;
    }

    return disconnectTransportLayer(&con);
}

ERRORCODE echoRCS(serialSession* serialPort, uint8_t* key)
{
    ERRORCODE         retCode;
    off_t             rcsSize;
    int               sysRet;
    struct stat       rcsInfo;
    uint16_t          respLen;
    uint32_t          rcsBase = 0xa0008000;
    sessionDetails*   details;
    uint8_t*          rcsData;
    int               rcsStream;
    int               readLength;
    char*             rcsFile = "./RCS/RCS_release.bin";
    uint8_t*          resp;
    uint8_t*          message = (uint8_t*)"Banana";

    sysRet = stat(rcsFile, &rcsInfo);

    if (sysRet != 0)
    {
        printf("Error getting rcs file details %d\n", errno);
        return ERR_STAT;
    }

    rcsSize = rcsInfo.st_size;

    retCode = startSessionLayer(serialPort, key, &details);

    if(retCode != SUCCESS)
    {
        debug("Failed to start session\n");
        return retCode;
    }

    rcsStream = open(rcsFile, O_RDONLY);

    rcsData = malloc(rcsSize);

    readLength = read(rcsStream, rcsData, rcsSize);
    if (readLength != rcsSize)
    {
        printf("Unable to read %lld bytes from RCS! Only got %d\n", rcsSize, readLength);
        free(rcsData);
        return ERR_FILE_READ;
    }

    retCode = writeProcedure (details, rcsBase, rcsData, rcsSize);
    free(rcsData);
    if (retCode == SUCCESS)
    {
        debug("RCS written successfully\n");
        retCode = registerProcedure (details, COMMAND_RCS_ONE, rcsBase);
        if (retCode == SUCCESS)
        {
            debug("RCS registered successfully\n");
            retCode = callCustomProcedure(details, COMMAND_RCS_ONE, message, 7, &resp, &respLen);
            if (retCode == SUCCESS)
            {
                printf("RCS execution successful, request - %s, response - \n", message);
                hexDump(resp, respLen);
                free(resp);
            }
            else
            {
                printf("RCS execution failed - code %d\n", retCode);
            }
        }
        else
        {
            debug("RCS registration failed\n");
        }
    }
    else
    {
        debug("RCS write failed\n");
    }

    endSession(details);

    return retCode;
}
