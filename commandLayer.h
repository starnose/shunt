/*
 * commandLayer.h
 *
 *  Created on: 8 Sep 2014
 *      Author: dhicks
 */

#ifndef COMMANDLAYER_H_
#define COMMANDLAYER_H_

#define KEY_SCOPE_TESTING               0x03
#define KEY_SCOPE_PRE_PERSONALISATION   0x04
#define KEY_SCOPE_PERSONALISTAION       0x05
#define KEY_SCOPE_FINAL_PERSONALISATION 0x06

#define KEY_USAGE_AUTHENTICATION        0x01
#define KEY_USAGE_ENCRYPTION            0x02
#define KEY_USAGE_SIGNATURE             0x03

#define COMMAND_WRITE_KEY               0xB2
#define COMMAND_WRITE_TIMEOUT           0x3A
#define COMMAND_UPDATE_LIFE_CYCLE       0xDA
#define COMMAND_WRITE_FLASH             0xC3
#define COMMAND_ERASE_FLASH             0xD4
#define COMMAND_VERIFY_FLASH            0x08
#define COMMAND_SIGN_CHECK_FLASH        0xF7
#define COMMAND_BLANK_CHECK_FLASH       0xE5
#define COMMAND_LOCK_FLASH              0xD6
#define COMMAND_WRITE_PROCEDURE         0x73
#define COMMAND_REGISTER_PROCEDURE      0x75
#define COMMAND_RCS_ONE                 0x01

ERRORCODE updateLifeCycle     (sessionDetails* session);
ERRORCODE writeTimeout        (sessionDetails* session,  uint16_t  timeout);
ERRORCODE writeKey            (sessionDetails* session,  uint8_t   scope,     uint8_t  usage,   uint8_t* key);
ERRORCODE writeFlash          (sessionDetails* session,  uint32_t  address,   uint8_t* data,    uint16_t dataLength);
ERRORCODE verifyFlash         (sessionDetails* session,  uint32_t  address,   uint8_t* data,    uint16_t dataLength);
ERRORCODE signCheckFlash      (sessionDetails* session,  uint32_t  address,   uint32_t length,  bool     otp,        uint8_t** signature);
ERRORCODE eraseFlash          (sessionDetails* session,  uint8_t   sector,    bool     override);
ERRORCODE lockFlash           (sessionDetails* session,  uint8_t   sector,    bool     override);
ERRORCODE blankCheckFlash     (sessionDetails* session);
ERRORCODE writeProcedure      (sessionDetails* session,  uint32_t  address,   uint8_t* data,    uint16_t dataLength);
ERRORCODE registerProcedure   (sessionDetails* session,  uint8_t   opCode,    uint32_t address);
ERRORCODE callCustomProcedure (sessionDetails* session,  uint8_t   commandID, uint8_t* data, uint16_t dataLength,
                               uint8_t**       respData, uint16_t* respLength);

#endif /* COMMANDLAYER_H_ */
