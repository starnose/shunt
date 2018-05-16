/*
 * transportLayer.h
 *
 *  Created on: 26 Aug 2014
 *      Author: dhicks
 */

#ifndef TRANSPORTLAYER_H_
#define TRANSPORTLAYER_H_

#include "serial.h"

typedef struct _transportConnection
{
    uint8_t        lastSeq;
    uint8_t        chanID;
    serialSession* serialPort;
} transportConnection;

ERRORCODE connectTransportLayer    (serialSession*       serialPort, transportConnection* con);
ERRORCODE transportLayerPing       (transportConnection* con);
ERRORCODE disconnectTransportLayer (transportConnection* con);

ERRORCODE sendTransportData        (transportConnection* con, uint8_t* data, uint16_t dataLength);
ERRORCODE receiveTransportData     (transportConnection* con, uint8_t** data, uint16_t* length);

#endif /* TRANSPORTLAYER_H_ */
