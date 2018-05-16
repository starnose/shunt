/*
 * utils.c
 *
 *  Created on: 21 Aug 2014
 *      Author: dhicks
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include <openssl/aes.h>

#include "utils.h"

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"



/*
 * xorBuffer
 *
 * xor two buffers into a third
 *
 * Arguments:
 * dest   - destination buffer
 * src1   - source buffer 1
 * src2   - source buffer 2
 * length - number of bytes to xor
 *
 * Returns: none
 */
void xorBuffer (uint8_t *dest, const uint8_t *src1, const uint8_t *src2, uint16_t length)
{
    uint16_t count = 0;

    while (count < length)
    {
        dest[count] = src1[count] ^ src2[count];
        count++;
    }
}

/*
 * aesEncrypt
 *
 * encrypt one block (16 bytes) with aes
 *
 * Arguments:
 * dest - the destination buffer
 * src  - the source buffer
 * key  - the key to use (presumed 16 bytes)
 */
ERRORCODE aesEncrypt (uint8_t* dest, const uint8_t* src, const uint8_t* key)
{
    AES_KEY osslkey;
    int     retCode;

    retCode = AES_set_encrypt_key(key, 128, &osslkey);
    if (retCode == 0)
    {
        AES_encrypt(src, dest, &osslkey);
        return SUCCESS;
    }
    else
    {
        return ERR_OPENSSL_KEY;
    }
}

/*
 * aesPadAndEncryptEcb
 *
 * encrypt with aes in ECB mode, right padded with 0x8000....
 *
 * Arguments:
 * dest - the destination buffer, at least src + (16 - (length %16)) bytes
 * src  - the source buffer
 * key  - the key to use (presumed 16 bytes)
 */
ERRORCODE aesPadAndEncryptEcb (uint8_t* dest, const uint8_t* src, const uint16_t length, const uint8_t* key)
{
    uint8_t   block[16];
    uint16_t  ctr;
    ERRORCODE retCode;

    for (ctr = 0; ctr < length; ctr += 16)
    {
        if (length - ctr < 16)
        {
            memset(block, 0, sizeof(block));
            memcpy(block, src + ctr, length - ctr);
            block[length - ctr] = 0x80;
            retCode = aesEncrypt(dest + ctr, block, key);
        }
        else
        {
            retCode = aesEncrypt(dest + ctr, src + ctr, key);
        }
        if (retCode != SUCCESS)
        {
            break;
        }
    }
    return retCode;
}

/*
 * generateAesCRC
 *
 * Calculate an AES-CRC, which is a CBC-MAC with null keys and IV
 *
 * Arguments:
 * dest   - the destination buffer, 16 bytes
 * src    - the source buffer
 * srcLen - the length of the src buffer, should be a multiple of 16
 */
void generateAesCRC (uint8_t* dest, const uint8_t* src, uint32_t srcLen)
{
    uint8_t buffer[16];
    uint8_t key[16];

    memset(dest, 0, 16);
    memset(key, 0, 16);

    while (srcLen)
    {
        memset(buffer, 0, 16);
        if (srcLen < 16)
        {
            memcpy(buffer, src, srcLen);
            srcLen = 0;
        }
        else
        {
            memcpy(buffer, src, 16);
            srcLen -= 16;
            src += 16;
        }

        xorBuffer(buffer, buffer, dest, 16);
        aesEncrypt(dest, buffer, key);
    }
}

/*
 * hexDump
 *
 * Dump out hex in a friendly format
 */
void hexDump(uint8_t* buf, uint32_t length)
{
    uint32_t ctr = 0;
    uint32_t lctr = 0;

    unsigned char hexbuf[67];
    memset(hexbuf,' ',66);
    hexbuf[66]=0;

    while(length)
    {
        hexbuf[lctr++] =(((*buf)>>4)>9)?(((*buf)>>4)-10 + 'A'):((*buf)>>4) + '0';
        hexbuf[lctr++] =(((*buf)&0xF)>9)?(((*buf)&0xF)-10 + 'A'):((*buf)&0xF) + '0';
        lctr++;
        hexbuf[50+(ctr%16)] = isprint(*buf)?*buf:'.';
        ctr++;
        if (!(ctr%8)) lctr++;
        if (!(ctr%16))
        {
            printf("%s\n",hexbuf);
            lctr=0;
            memset(hexbuf,' ',66);
        }
        buf++;
        length--;
    }
    if (ctr%16){printf("%s\n",hexbuf); lctr=0;}
}

/*
 * hexFake
 *
 * don't dump out anything!
 */
void hexFake(__attribute__((unused)) uint8_t* buf, __attribute__((unused)) uint32_t length)
{

}

/*
 * debugFake
 *
 * a fake debug routine that does nothing
 */
int debugFake(__attribute__((unused)) const char* fmt, ...)
{
    return 0;
}
