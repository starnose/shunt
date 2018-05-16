/*
 * serial.c
 *
 *  Created on: 5 Sep 2014
 *      Author: dhicks
 */
#include "shunt.h"
#include "serial.h"

#define SERIAL_BUFFER_SIZE 4096

struct _serialSession
{
    uint8_t         buffer[SERIAL_BUFFER_SIZE];
    uint16_t        in;
    uint16_t        out;
    pthread_mutex_t bufferLock;
    int             fildes;
    char*           devName;
    pthread_t       thread;
};

/*
 * writewrap
 *
 * wrap unixy write calls and take account of errors and retries
 */
ERRORCODE serialWrite (serialSession* session, uint8_t* data, uint16_t length)
{
    int     retCode;
    uint8_t retries;

    retries = 5;

    do
    {
        retCode = write(session->fildes, (void*)data, length);
    } while ( (retCode != length) && ((errno == EAGAIN) || (errno == EINTR)) && (--retries));

    if(retCode != length)
    {
        debug("Write error - %s\n", strerror(errno));
        return ERR_SERIAL_WRITE;
    }

    debug("Wrote buffer - \n");
    hexDebug(data, length);

    return SUCCESS;
}

/*
 * readwrap
 *
 * wrap unixy write calls and take account of errors and retries
 */
static ERRORCODE readwrap (int port, uint8_t* data, uint16_t length, int16_t* readBytes)
{
    int     retCode;
    uint8_t retries;

    retries = 5;

    do
    {
        retCode = read(port, (void*)data, length);
    } while (((errno == EAGAIN) || (errno == EINTR)) && (--retries));

    *readBytes = retCode;

    if(retCode != length)
    {
        if (retCode == 0)
        {
            return ERR_SERIAL_NO_DATA;
        }
        return ERR_SERIAL_READ;
    }

    return SUCCESS;
}


/*
 * openSerial
 *
 * open the serial port and return descriptor
 */
ERRORCODE openSerial(char* devName, int* fildes)
{
    struct termios theTermios;
    ERRORCODE      errorCode;
    int            returnCode;

    returnCode = open(devName, O_RDWR| O_NONBLOCK | O_NDELAY);

    if (returnCode != -1)
    {
        *fildes = returnCode;
        memset(&theTermios, 0, sizeof(struct termios));

        cfmakeraw(&theTermios);
        cfsetspeed(&theTermios, 115200);

        theTermios.c_cflag = CREAD | CLOCAL;     // turn on READ
        theTermios.c_cflag |= CS8;
        theTermios.c_cc[VMIN] = 0;
        theTermios.c_cc[VTIME] = 10;     // 1 sec timeout
        returnCode = ioctl(*fildes, TIOCSETA, &theTermios);

        if (returnCode != -1)
        {
            errorCode = SUCCESS;
        }
        else
        {
            errorCode = ERR_CONFIG_TTY;
            close(*fildes);
        }
    }
    else
    {
        errorCode = ERR_OPEN_TTY;
    }
    return errorCode;
}
/*
 * Different settings
 * Non functional
 */
/*
ERRORCODE openSerial2(char* devName, int* fildes)
{
    struct termios theTermios;
    ERRORCODE      errorCode;
    int            returnCode;

    returnCode = open(devName, O_RDWR| O_NONBLOCK | O_NDELAY);

    if (returnCode != -1)
    {
        *fildes = returnCode;
        memset(&theTermios, 0, sizeof(struct termios));

        tcgetattr(returnCode, &theTermios);
        cfmakeraw(&theTermios);
        cfsetspeed(&theTermios, 115200);

        theTermios.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
        theTermios.c_oflag = 0;
        theTermios.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);

        theTermios.c_cflag &= ~(CSIZE | PARENB);
        theTermios.c_cflag |= CS8;
        theTermios.c_cc[VMIN]  = 1;
        theTermios.c_cc[VTIME] = 0;
        returnCode = tcsetattr(returnCode, TCSAFLUSH, &theTermios);

        if (returnCode != -1)
        {
            errorCode = SUCCESS;
        }
        else
        {
            errorCode = ERR_CONFIG_TTY;
            close(*fildes);
        }
    }
    else
    {
        errorCode = ERR_OPEN_TTY;
    }
    return errorCode;
}
*/

/*
 * serialReadThread
 *
 * Main function for a thread that reads serial
 * data into a buffer
 */
ERRORCODE serialReadThread(serialSession* session)
{
    uint8_t   incoming;
    ERRORCODE retCode;
    int       sysRet;
    int16_t   actual;

    struct   timespec tinySleep;

    while(1)
    {
        retCode = readwrap(session->fildes, &incoming, 1, &actual);

        if(retCode == ERR_SERIAL_NO_DATA)
        {
            tinySleep.tv_nsec = 10000;
            tinySleep.tv_sec  = 0;
            nanosleep(&tinySleep, NULL);
            continue;
        }

        if ((retCode != SUCCESS) || (actual != 1))
        {
            debug("Read failed, thread exit %d\n", retCode);
            break;
        }


        sysRet = pthread_mutex_lock(&(session->bufferLock));
        if (sysRet != 0)
        {
            debug("Failed to lock buffer %d\n", errno);
            break;
        }

        //debug("Thread got char - 0x%x\n", incoming);

        session->buffer[session->in] = incoming;
        MOD_INCREMENT(session->in, SERIAL_BUFFER_SIZE);

        if (session->in == session->out)
        {
            debug("Buffer overflow!\n");
            MOD_INCREMENT(session->out, SERIAL_BUFFER_SIZE);
        }

        sysRet = pthread_mutex_unlock(&(session->bufferLock));
        if (sysRet != 0)
        {
            debug("Failed to unlock buffer %d\n", errno);
            break;
        }
    }
    return SUCCESS;
}

/*
 * serialRead
 *
 * read data out of the buffer/cache
 */
ERRORCODE serialRead(serialSession* session, uint8_t* data, uint16_t length, int16_t* readBytes)
{
    int       sysRet;
    uint16_t  availableBytes;
    ERRORCODE retCode;

    sysRet = pthread_mutex_lock(&(session->bufferLock));
    if (sysRet != 0)
    {
        debug("Failed to lock buffer %d\n", errno);
        return ERR_LOCK_FAIL;
    }

    availableBytes = MOD_SUBTRACT(session->in, session->out, SERIAL_BUFFER_SIZE);
    if(availableBytes != 0)
    {
        *readBytes = availableBytes>length?length:availableBytes;
        MOD_MEMCPY(data, session->buffer, *readBytes, session->out, SERIAL_BUFFER_SIZE);
        MOD_ADD(session->out, *readBytes, SERIAL_BUFFER_SIZE);
        retCode = SUCCESS;
    }
    else
    {
        retCode = ERR_SERIAL_NO_DATA;
    }

    sysRet = pthread_mutex_unlock(&(session->bufferLock));
    if (sysRet != 0)
    {
        debug("Failed to unlock buffer %d\n", errno);
        return ERR_LOCK_FAIL;
    }

    return retCode;
}

/*
 * createSession
 *
 * Allocate and initialise a session structure
 */
ERRORCODE createSession(char* devName, serialSession** session)
{
    int sysRet;

    *session = (serialSession*)malloc(sizeof(struct _serialSession));
    if (!(*session))
    {
        debug("Failed to allocate session %d\n", errno);
        return ERR_NO_MEM;
    }

    (*session)->in = 0;
    (*session)->out = 0;
    (*session)->fildes = 0;
    (*session)->thread = 0;

    (*session)->devName = (char*)calloc(sizeof(char), strlen(devName) + 1);
    if (!(*session))
    {
        debug("Failed to allocate device Buffer %d\n", errno);
        free(*session);
        return ERR_NO_MEM;
    }
    strcpy((*session)->devName, devName);

    sysRet = pthread_mutex_init(&((*session)->bufferLock), NULL);
    if (sysRet != 0)
    {
        debug("Failed to create mutex, %d\n", errno);
        free((*session)->devName);
        free(*session);
        return ERR_CREATE_MUTEX;
    }

    return SUCCESS;
}

/*
 * destroySession
 *
 * destroy a serial session
 */
void destroySession(serialSession* session)
{
    /*
     * should this kill the thread?
     */
    pthread_cancel(session->thread);
    pthread_mutex_destroy(&(session->bufferLock));
    if (session->thread) pthread_cancel(session->thread);
    if (session->fildes) close(session->fildes);
    if (session->devName) free(session->devName);
    if (session) free(session);
}

/*
 * serialInit
 *
 * Open the serial port, start the reader thread, exit
 */
ERRORCODE serialInit(char* devName, serialSession** session)
{
    ERRORCODE retCode;
    int       sysRet;

    retCode = createSession(devName, session);
    if (retCode != SUCCESS)
    {
        return retCode;
    }

    retCode = openSerial((*session)->devName, &((*session)->fildes));
    if (retCode != SUCCESS)
    {
        debug("Failed to open serial port\n");
        destroySession(*session);
        return retCode;
    }

    sysRet = pthread_create(&((*session)->thread), NULL, (pthreadFunc)serialReadThread, *session);
    if (sysRet != 0)
    {
        debug("Failed to start thread %d\n", errno);
        destroySession(*session);
        return ERR_CREATE_THREAD;
    }

    return SUCCESS;
}
