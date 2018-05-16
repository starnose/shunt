/*
 * helloworld.c
 *
 *  Created on: 16 Sep 2014
 *      Author: dhicks
 */

#include <infra/error.h>

int rcs( int *argc, char *argv ) __attribute__((section (".text.start")));
int rcs( int *argc, char *argv ) {
    int n;
    int i;
    int result;
    /* the argument count should contain the one-byte opcode
     * and the optional command arguments */
    /* size of the command arguments */
    n = (*argc) - 1;
    if(n < 1) {
        /* no arguments after the opcode */
        /* no answer */
        *argc = 0;
        /* return an error */
        result = -ERR_INVAL;
    }
    else
    {
        /* build the echo: shift the arguments to the beginning of the buffer
         * (to remove the opcode) */
        for(i=0; i<n; i++) {
            argv[i] = argv[i+1];
        }
        /* length of the answer */
        *argc = n;
        /* no error */
        result = ERR_NO;
    }
    return result;
}
