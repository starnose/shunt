/*
 * shunt.h
 *
 *  Created on: 21 Aug 2014
 *      Author: dhicks
 */

#ifndef SHUNT_H_
#define SHUNT_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define MAX_CANDIDATES         9
#define RETRANSMISSION_TIMEOUT 10

typedef uint16_t ERRORCODE;

#define SUCCESS             0
#define ERR_DIRECTORY       1
#define ERR_NO_DEVICE       2
#define ERR_TOO_MANY        3
#define ERR_OPEN_TTY        4
#define ERR_CONFIG_TTY      5
#define ERR_SERIAL_WRITE    6
#define ERR_SERIAL_READ     7
#define ERR_SERIAL_TIMEOUT  8
#define ERR_VALIDATION      9
#define ERR_TRANSPORT_SEND  10
#define ERR_CONREP          11
#define ERR_SERIAL_NO_DATA  12
#define ERR_NO_MEM          13
#define ERR_CREATE_MUTEX    14
#define ERR_CREATE_THREAD   15
#define ERR_LOCK_FAIL       16
#define ERR_OPENSSL_KEY     17
#define ERR_CHALLENGE_FAIL  18
#define ERR_COMMAND_INVAL   19
#define ERR_COMMAND_ALREADY 20
#define ERR_COMMAND_UNK     21
#define ERR_COMMAND_LENGTH  22
#define ERR_BAD_SECTOR      23
#define ERR_SIG_LENGTH      24
#define ERR_BAD_OPCODE      25
#define ERR_STAT            26
#define ERR_TOO_BIG         27
#define ERR_FILE_OPEN       28
#define ERR_FILE_READ       29
#define ERR_AGAIN           30

#define MODE_FLASH    0
#define MODE_ERASE    1
#define MODE_USN      2
#define MODE_PING     3
#define MODE_TEST     4
#define MODE_RCS_TEST 5

#define MOD_ADD(x,y,mod)        (x)+=(y); (x) = (x) % (mod)
#define MOD_INCREMENT(x,mod)    MOD_ADD(x,1,mod)
#define MOD_SUBTRACT(x,y,mod)   ((x)>=(y))?((x)-(y)):((mod)-((y)-(x)))

#define MOD_MEMCPY(destination, buffer, size, offset, mod) if ((offset) + (size) > mod) { \
                                                               memcpy(destination, (buffer) + (offset), mod - (offset)); \
                                                               memcpy(destination, buffer, (size) - (mod - (offset))); \
                                                           } \
                                                           else memcpy(destination, (buffer) + (offset), size);

struct __attribute__((packed)) helloResp
{
    uint8_t command;
    uint8_t firstZero;
    uint8_t secondZero;
    uint8_t separator;
    uint8_t hiResp[7];
    uint8_t lifeCycle;
    uint8_t usipMajorVersion;
    uint8_t usipMinorVersion;
    uint8_t sblMajorVersion;
    uint8_t sblMinorVersion;
    uint8_t halMajorVersion;
    uint8_t halMinorVersion;
    uint8_t usn[16];
    uint8_t random[16];
};

typedef void* (*pthreadFunc)(void*);

#define debug(string, ...) (debugFunc)("%s:(%d):%s:"string,__FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define debug_raw(string)  debugFunc(string)
#define hexDebug(x,y)      hexDebugFunc(x,y)

int  (*debugFunc)(const char* fmt, ...);
void (*hexDebugFunc)(uint8_t* buf, uint32_t length);

#endif /* SHUNT_H_ */
