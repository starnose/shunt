/*
 * shunt
 *
 * shunt is a simple app for pushing data to a USIP
 * chip's internal flash as fast and simply as possible
 */

#include "shunt.h"
#include "utils.h"
#include "serial.h"
#include "appLayer.h"

/*
 * presentChoices
 *
 * present a list of up to ten items to display
 * and ask the user to choose between them
 */
ERRORCODE presentChoices (char* prompt, char** strArray, uint8_t numChoices, uint8_t* choice)
{
    uint8_t   input;
    uint8_t   keyPress;
    uint8_t   enumerator;
    ERRORCODE errorCode;
    
    if (numChoices > MAX_CANDIDATES)
    {
        printf("TOO MANY DEVICES!!\n");
        return ERR_TOO_MANY;
    }
    
    input = numChoices + 1;

    while (input > numChoices)
    {
        printf("%s\n", prompt);
        enumerator = 0;
        while(enumerator < numChoices)
        {
            printf("%d) %s\n", enumerator, strArray[enumerator]);
            enumerator++;
        }
        /*
         * for some reason we need to clear a new line at this point
         */
        //keyPress = getchar();
        keyPress = getchar();
        if (keyPress >= '0')
        {
            input = keyPress - '0';
        }
        else
        {
            input = numChoices + 1;
        }
        if (input > numChoices)
        {
            printf("Invalid choice (%d), please choose from 0 to %d\n", keyPress, numChoices);
        }
        else
        {
            *choice = input;
            errorCode = SUCCESS;
        }
    }
    return errorCode;
}

/*
 * findDevice
 *
 * Find likely looking tty's, prepare a candidate list of usb
 * serial devices
 */
ERRORCODE findDevice(char* deviceName)
{
    DIR*           dp;
    struct dirent* ep;
    char           candidates[MAX_CANDIDATES][100];
    char*          candidateList[MAX_CANDIDATES];
    uint8_t        counter;
    uint8_t        choice;
    ERRORCODE      errorCode;


    dp = opendir ("/dev");

    if (dp == NULL)
    {
        debug("Couldn't open the directory - permissions issue?\n");
        errorCode = ERR_DIRECTORY;
    }
    else
    {
        counter = 0;
    
        while ((ep = readdir (dp)) && counter <= MAX_CANDIDATES)
        {
            if ( strstr(ep->d_name, "tty") && strstr(ep->d_name, "usbserial"))
            {
                memcpy(candidates[counter], "/dev/", 5);
                memcpy(candidates[counter] + 5, ep->d_name, strlen(ep->d_name) + 1);
                candidateList[counter] = candidates[counter];
                counter ++;
            }
        }
        closedir (dp);
    
        switch(counter)
        {
        case 0:
            errorCode = ERR_NO_DEVICE;
            break;
        case 1:
            memcpy(deviceName, candidates[0], strlen(candidates[0]) + 1);
            errorCode = SUCCESS;
            break;
        default:
            errorCode = presentChoices("Please choose a device:", candidateList, counter, &choice);
            if (errorCode == SUCCESS)
            {
                memcpy(deviceName, candidates[choice], strlen(candidates[choice]) + 1);
            }
            break;
        }
    }
    return errorCode;
}

void printHelp(char* name)
{
    printf("\n");
    printf("%s [-l <tty device>] [-f <image file> [-o <offset>] [-D] | -d [-s <sector>] [-e <sector>] | -u | -p | -t | -r] [-O] [-k key] [-v]\n", name);
    printf("%s -h|-?\n\n", name);
    printf("\t-l <tty>    Specify the tty device to use (default - autodetect)\n");
    printf("Flash Mode:\n");
    printf("\t-f <image>  Specify the image to flash\n");
    printf("\t-o <offset> offset sector for flashing (default 0)\n");
    printf("\t-D          dry-run (do not actually flash)\n");
    printf("Erase Mode:\n");
    printf("\t-d          Erase flash (default - erase sectors 0-34)\n");
    printf("\t-s <sector> Start sector for erase (default 0)\n");
    printf("\t-e <sector> End sector for erase (default 34)\n");
    printf("Other Modes:\n");
    printf("\t-u          Just retrieve USN and other unit details\n");
    printf("\t-p          Ping the USIP bootloader (at transport layer)\n");
    printf("\t-t          Test the session layer connection only\n");
    printf("\t-r          Test the RCS installation and echo\n");
    printf("Other Options:\n");
    printf("\t-O          Override sector 35 protection\n");
    printf("\t-k <key>    Communication key for use with USIP bootloader, 16 bytes (default 0x61...)\n");
    printf("\t-v          Verbose (debug) output\n");
    printf("\t-h          Print this help and exit\n\n");
}

int main (int argc, char** argv)
{
    int            opt;
    char*          device;
    char*          defaultImageFile = "usip.complete.bin";
    uint8_t        key[16];
    char           keyInt[3];
    char*          imageFile;
    char           deviceBuffer[100];
    serialSession* serialPort;
    ERRORCODE      errorCode;
    uint8_t        mode;
    uint8_t        startSect;
    uint8_t        endSect;
    uint8_t        offsetSect;
    struct stat    statStruct;
    uint8_t        keyCounter;
    bool           override;
    bool           dryrun;

    mode = MODE_FLASH;
    device = NULL;
    startSect = 0;
    endSect = 34;
    offsetSect = 0;
    memset(deviceBuffer, 0, sizeof(deviceBuffer));
    memset(key, 0x61, sizeof(key));
    override = false;
    dryrun   = false;
    
    debugFunc = debugFake;
    hexDebugFunc = hexFake;

    imageFile = defaultImageFile;

    while ((opt = getopt(argc, argv, ":l:f:o:Dds:e:utOpvrh?")) != -1)
    {
        switch(opt)
        {
        case 'l':
            device = optarg;
            break;
        case 'f':
            imageFile = optarg;
            break;
        case 'o':
            offsetSect = atoi(optarg);
            if (offsetSect > 35)
            {
                debug("offset sector value too large - %d (max 35)\n", offsetSect);
                printHelp(argv[0]);
                exit(1);
            }
            break;
        case 'D':
            dryrun = true;
            break;
        case 'd':
            mode = MODE_ERASE;
            break;
        case 's':
            startSect = atoi(optarg);
            if (startSect >= 35)
            {
                debug("start sector value too large - %d (max 35)\n", startSect);
                printHelp(argv[0]);
                exit(1);
            }
            break;
        case 'e':
            endSect = atoi(optarg);
            if (startSect >= 35)
            {
                debug("end sector value too large - %d (max 35)\n", endSect);
                printHelp(argv[0]);
                exit(1);
            }
            break;
        case 'k':
            if (strlen(optarg) != 32)
            {
                debug("Key too length wrong - %lu, expected 32\n", strlen(optarg));
                printHelp(argv[0]);
                exit(1);
            }
            keyCounter = 0;
            while(keyCounter < 16)
            {
                keyInt[0] = optarg[2*keyCounter];
                keyInt[1] = optarg[(2*keyCounter) + 1];
                keyInt[2] = 0;
                key[keyCounter] = atoi(keyInt);
                keyCounter++;
            }
            break;
        case 'u':
            mode = MODE_USN;
            break;
        case 'p':
            mode = MODE_PING;
            break;
        case 't':
            mode = MODE_TEST;
            break;
        case 'r':
            mode = MODE_RCS_TEST;
            break;
        case 'O':
            override = true;
            break;
        case 'v':
            debugFunc = printf;
            hexDebugFunc = hexDump;
            break;
        case 'h':
        case '?':
            printHelp(argv[0]);
            exit(0);
        case ':':
            printf("Option %c requires an argument\n", optopt);
            printHelp(argv[0]);
            exit(1);
        default:
            printf("Unknown option - %c\n", opt);
            printHelp(argv[0]);
            exit(1);
        }
    }
    
    if ((mode == MODE_FLASH) && (stat(imageFile, &statStruct) == -1))
    {
        printf("Image file %s can't be accessed - %s\n", imageFile, strerror(errno));
        printHelp(argv[0]);
        exit(1);
    }

    if (startSect > endSect)
    {
        printf("Start sector (%d) must be less than end sector (%d)!\n",startSect, endSect);
        printHelp(argv[0]);
        exit(1);
    }

    if (device == NULL)
    {
        errorCode = findDevice(deviceBuffer);
        if (errorCode != SUCCESS)
        {
            printf("Failed to find a device\n");
            exit(1);
        }
        device = deviceBuffer;
        
    }

    printf("%s running in mode - ", argv[0]);

    switch (mode)
    {
    case MODE_ERASE:
        printf("ERASE\n");
        printf("Erasing sectors %d to %d\n", startSect, endSect);
        break;
    case MODE_FLASH:
        printf("FLASH\n");
        printf("Flashing image %s at offset %d\n", imageFile, offsetSect);
        break;
    case MODE_USN:
        printf("USN\n");
        break;
    case MODE_PING:
        printf("PING\n");
        break;
    case MODE_TEST:
        printf("TEST\n");
        break;
    case MODE_RCS_TEST:
        printf("RCS TEST\n");
        break;
    }

    debug("Comms key - \n");
    hexDebug(key, 16);

    debug("Opening %s ... \n", device);

    errorCode = serialInit(deviceBuffer, &serialPort);
    if (errorCode != SUCCESS)
    {
        printf("Failed to open serial port - %s\n", strerror(errno));
        exit(1);
    }
    debug("Port open\n");

    switch (mode)
    {
    case MODE_ERASE:
        errorCode = eraseSectors(serialPort, key, startSect, endSect, override);
        break;
    case MODE_FLASH:
        errorCode = flashProgram(serialPort, key, offsetSect, imageFile, override, dryrun);
        break;
    case MODE_USN:
        errorCode = getUSN(serialPort, key);
        break;
    case MODE_PING:
        errorCode = pingUSIP(serialPort);
        break;
    case MODE_TEST:
        errorCode = testSessionLayer(serialPort, key);
        break;
    case MODE_RCS_TEST:
        errorCode = echoRCS(serialPort, key);
        break;
    }

    debug("Completed with code - %u\n", errorCode);

    if (errorCode == SUCCESS)
    {
        printf("Operation completed successfully\n");
    }
    else
    {
        printf("Operation FAILED, code %d\n", errorCode);
    }

    destroySession(serialPort);
    return 0;
}
