/*
 * cmac.h
 *
 *  Created on: 14 Mar 2014
 *      Author: David H
 */

#ifndef CMAC_H_
#define CMAC_H_

#include <stdint.h>

#define CMAC_LENGTH 16

void generateCMac(const uint8_t* data, uint32_t dataLength, const uint8_t* key, uint8_t* mac);

#endif /* CMAC_H_ */
