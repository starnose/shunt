/*
 * utils.h
 *
 *  Created on: 21 Aug 2014
 *      Author: dhicks
 */

#ifndef UTILS_H_
#define UTILS_H_

#include "shunt.h"

void      xorBuffer           (uint8_t *dest, const uint8_t *src1, const uint8_t* src2, uint16_t length);
ERRORCODE aesEncrypt          (uint8_t* dest, const uint8_t* src,  const uint8_t* key);
void      generateAesCRC      (uint8_t* dest, const uint8_t* src,        uint32_t srcLen);
ERRORCODE aesPadAndEncryptEcb (uint8_t* dest, const uint8_t* src, const uint16_t length, const uint8_t* key);
void      hexDump             (uint8_t* buf, uint32_t length);
int       debugFake           (const char* fmt, ...);
void      hexFake             (uint8_t* buf, uint32_t length);

#endif /* UTILS_H_ */
