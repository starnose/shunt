/*
 * serial.h
 *
 *  Created on: 5 Sep 2014
 *      Author: dhicks
 */

#ifndef SERIAL_H_
#define SERIAL_H_

typedef struct _serialSession serialSession;

ERRORCODE serialInit(char* devName, serialSession** session);

ERRORCODE serialRead(serialSession* session, uint8_t* data, uint16_t length, int16_t* readBytes);
ERRORCODE serialWrite(serialSession* session, uint8_t* data, uint16_t length);

void      destroySession(serialSession* session);

#endif /* SERIAL_H_ */
