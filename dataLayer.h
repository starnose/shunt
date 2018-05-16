/*
 * dataLayer.h
 *
 *  Created on: 21 Aug 2014
 *      Author: dhicks
 */

#ifndef DATALAYER_H_
#define DATALAYER_H_

ERRORCODE sendDataLayerPacket    (serialSession* serialPort, uint8_t  protocol, uint8_t  id, uint8_t  sequence, uint8_t*  data, uint16_t  dataLength);
ERRORCODE receiveDataLayerPacket (serialSession* serialPort, uint8_t* protocol, uint8_t* id, uint8_t* sequence, uint8_t** data, uint16_t* dataLength, uint8_t timeout);

#endif /* DATALAYER_H_ */
