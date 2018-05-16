/*
 * sessionLayer.h
 *
 *  Created on: 27 Aug 2014
 *      Author: dhicks
 */

#ifndef SESSIONLAYER_H_
#define SESSIONLAYER_H_

#include "serial.h"

typedef struct _sessionDetails sessionDetails;

/*
 * Exposed as a way to do only the 'HI-USIP' sequence
 */
ERRORCODE initSession(serialSession* serialPort, uint8_t** respData, uint16_t* respLen, uint8_t* key, sessionDetails** retDetails);

/*
 * real way to start a full session
 */
ERRORCODE startSessionLayer(serialSession* serialPort, uint8_t* key, sessionDetails** retDetails);

/*
 * send commands from application layer
 */
ERRORCODE sendSessionData(sessionDetails* session, uint8_t* data, uint16_t length);

/*
 * receive a command at the session layer
 */
ERRORCODE receiveSessionData (sessionDetails* session, uint8_t** data, uint16_t* length);

/*
 * Send a disconnection and free the session
 */
void endSession (sessionDetails* session);

#endif /* SESSIONLAYER_H_ */
