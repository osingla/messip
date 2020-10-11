/**
 * @file messip_example_1.c
 * 
 **/

/**
 * @mainpage messip - Examples programs - No. 1
 * 
 * MessIP : Message Passing over TCP/IP \n
 * Copyright (C) 2001-2007  Olivier Singla \n
 * http://messip.sourceforge.net/ \n\n
 * 
 * Server:
 * - connect to the messip manager
 * - create a channel ('one') to receive messages
 * - wait for a message (no timeout) ["Hello"]
 * - when received the message, reply back with another message ["Bonjour"]
 * 
 * Client:
 * - connect to the messip manager
 * - locate the channel ('one') to send messages
 * - send a message (no timeout) ["Hello"]
 * - and print the message replied back ["Bonjour"]
 * 
 **/

#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <sys/wait.h>

#include "messip.h"

static time_t now0 = 0;
#include "example_utils.h"

/**
 *  Main function
 * 
 *  @param argc Number of arguments
 *  @param argv List of the arguments (array)
 *  @return 0 if no error
 */
int main( int argc, char *argv[] ) {

    /*
     * Connect to the messip server
     */
    display( "Server", "Start process\n" );
    messip_set_log_level( MESSIP_LOG_ERROR | MESSIP_LOG_WARNING
       | MESSIP_LOG_INFO /* | MESSIP_LOG_INFO_VERBOSE | MESSIP_LOG_DEBUG */  );
    messip_init(  );
    messip_cnx_t *cnx = messip_connect( NULL, "ex1/p1", MESSIP_NOTIMEOUT );
    if ( !cnx ) {
        cancel( "Unable to find messip manager\n" );
    }

    /*
     * Create channel 'one'
     * We'll receive messages on this channel
     */
    messip_channel_t *ch = messip_channel_create( cnx, "one", MESSIP_NOTIMEOUT, 0 );
    if ( !ch ) {
        cancel( "Unable to create channel '%s'\n", "one" );
    }

    int *p = NULL;
    *p = 123;

    return 0;
}                               // main
