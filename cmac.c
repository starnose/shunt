/*
 * cmac.c
 *
 *  Created on: 14 Mar 2014
 *      Author: David H
 */

#include "cmac.h"
#include "utils.h"
#include <string.h>

#define AES_BLOCKSIZE  16
#define AES_RVAL       0x87

#define TDEA_BLOCKSIZE 8
#define TDEA_RVAL      0x1B

#define CMAC_BLOCKSIZE AES_BLOCKSIZE
#define CMAC_RVAL      AES_RVAL
#define CMAC_ALGORITHM aesEncrypt


/*
 * deriveSubkeys
 *
 * Derive the two subkeys used for CMAC, K1 and K2
 *
 * Arguments -
 * key  - the main CMAC key
 * key1 - output buffer for K1
 * key2 - output buffer for K2
 */
static void deriveSubkeys(const uint8_t *key, uint8_t *key1, uint8_t* key2)
{
    uint8_t i;

    memset(key1, 0, CMAC_BLOCKSIZE);

    CMAC_ALGORITHM(key2, key1, key);

    for(i = 0; i < 8; i++)
    {
        key1[i] = (key2[i] << 1) | ((i<7)?(key2[i+1]>>7):0);
    }

    if (key2[0] & 0x80)
    {
        key1[CMAC_BLOCKSIZE] ^= CMAC_RVAL;
    }

    for(i = 0; i < 8; i++)
    {
        key2[i] = (key1[i] << 1) | ((i<7)?(key1[i+1]>>7):0);
    }

    if (key1[0] & 0x80)
    {
        key2[CMAC_BLOCKSIZE] ^= CMAC_RVAL;
    }
}

/*
 * generateCMac
 *
 * generate a CMAC according to ANSI X9 TR-31 2010
 *
 * Arguments -
 * data         - buffer to MAC
 * dataLength   - length of buffer
 * key          - 16 byte key
 * mac          - 8 byte mac output field
 */
void generateCMac(const uint8_t* data, uint32_t dataLength, const uint8_t* key, uint8_t* mac)
{
    uint8_t  key1[CMAC_BLOCKSIZE];
    uint8_t  key2[CMAC_BLOCKSIZE];

    uint8_t  lastBlock[CMAC_BLOCKSIZE];

    uint8_t  outPut[CMAC_BLOCKSIZE];

    uint8_t  remainder = dataLength % CMAC_BLOCKSIZE;
    uint32_t mainBufferLength = dataLength - remainder;
    uint32_t bufferCounter = 0;

    // set up subkeys
    deriveSubkeys(key, key1, key2);

    // set up Mn:
    if(remainder)
    {
        memset(lastBlock, 0x00, sizeof(lastBlock));
        memcpy(lastBlock, data + dataLength - remainder, remainder);
        lastBlock[remainder] = 0x80;
        xorBuffer(lastBlock,
                  lastBlock,
                  key2,
                  CMAC_BLOCKSIZE);
    }
    else
    {
        xorBuffer(lastBlock,
                  data + dataLength - CMAC_BLOCKSIZE,
                  key1,
                  CMAC_BLOCKSIZE);
        mainBufferLength -= CMAC_BLOCKSIZE;
    }

    // do the CMAC....
    memset(key1, 0, sizeof(key1));
    memset(key2, 0, sizeof(key2));

    while(bufferCounter < mainBufferLength)
    {
        xorBuffer(key1, key2, data + bufferCounter, CMAC_BLOCKSIZE);
        CMAC_ALGORITHM(key2, key1 , key);
        bufferCounter += 8;
    }

    xorBuffer(key1, key2, lastBlock, CMAC_BLOCKSIZE);

    CMAC_ALGORITHM(outPut, key1 , key);

    memcpy(mac, outPut, CMAC_LENGTH);
}
