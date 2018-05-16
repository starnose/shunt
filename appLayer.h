/*
 * appLayer.h
 *
 *  Created on: 21 Aug 2014
 *      Author: dhicks
 */

#ifndef APPLAYER_H_
#define APPLAYER_H_

#define CHUNK_SIZE 512

ERRORCODE eraseSectors     (serialSession* serialPort, uint8_t* key, uint8_t startSect,  uint8_t endSect,   bool override);
ERRORCODE flashProgram     (serialSession* serialPort, uint8_t* key, uint8_t offsetSect, char*   imageFile, bool override, bool dryrun);
ERRORCODE getUSN           (serialSession* serialPort, uint8_t* key);
ERRORCODE pingUSIP         (serialSession* serialPort);
ERRORCODE testSessionLayer (serialSession* serialPort, uint8_t* key);
ERRORCODE echoRCS          (serialSession* serialPort, uint8_t* key);

#endif /* APPLAYER_H_ */
