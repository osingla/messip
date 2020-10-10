/**
 * @file p.c
 * 
 **/

/**
 * @mainpage messip - Examples programs - No. 3
 * 
 * MessIP : Message Passing over TCP/IP \n
 * Copyright (C) 2001-2007  Olivier Singla \n
 * http://messip.sourceforge.net/ \n\n
 *
 * process p1 (p server):
 * - connect to the messip server
 * - create a channel ('one') to receive messages
 * - wait for a message (no timeout) from {p2},
 *   do not reply immediatelly
 * - wait for a message (no timeout) from {p3},
 *   do not reply immediatelly
 * - simulate some processing (sleep 5)
 * - then reply back with another message to {p2}
 * - then reply back with another message to {p3}
 * 
 * processes p2 and p3
 * - connect to the messip server
 * - locate the channel ('one') to send messages
 * - send a message (no timeout)
 * - and print the message replied back
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

#include "messip.h"

static time_t now0 = 0;
#include "example_utils.h"

/**
 *  Server-side function
 * 
 *  @param argc Number of arguments
 *  @param argv List of the arguments (array)
 *  @return 0 if no error
 */
static int server( int argc, char *argv[] ) {
	char				rec_buff[ 80 ];
	int32_t				type;
	int					index;

	display( "Server", "Start process\n" );
	messip_init( );
    messip_cnx_t *cnx = messip_connect( NULL, "ex3/p1", MESSIP_NOTIMEOUT );
    if ( !cnx ) 
        cancel( "Unable to find messip server\n" );

	/*
		Create channel 'one'
	*/
	messip_channel_t *ch = messip_channel_create( cnx, "one", 5000, 0 );	// 5 seconds timeout
	if ( !ch ) 
		cancel( "Unable to create channel '%s'\n", "one" );

	/*
		Now wait for some messages this on channel
	*/
	for (;;) {
		memset( rec_buff, 0, sizeof(rec_buff) );
		index = messip_receive( ch, 
			&type, rec_buff, sizeof(rec_buff), 
			10000 );			// 10 seconds timeout
		assert(index != -1);
		display( "Server", "received %d%s '%s' type=%d index=%d\n", 
			index, 
			(index == MESSIP_MSG_TIMEOUT) ? " (timeout)" : 
				((index == MESSIP_MSG_DISMISSED) ? " (dismissed)" : ""),
			rec_buff, type, index );
		if (index == MESSIP_MSG_DISMISSED) {
			display( "Server", "id %s dismissed\n", 
				ch->remote_id );
			static int cpt = 0;
			if ( ++cpt % 5 == 0 ) {
				fprintf( stdout, "Sleep for 15 seconds...\n" );
				sleep( 15 );
			}
			continue;
		}
		if (index == MESSIP_MSG_TIMEOUT)
			continue;
		if ( !strcmp( rec_buff, "Hello" ) )
			messip_reply( ch, index, 987, "Bonjour", 8, 5000 );
		else if ( !strcmp( rec_buff, "Bye" ) )
			messip_reply( ch, index, 765,"Ciao", 5, 5000 );
	}

	return 0;
}								// server

/**
 *  Client-side function
 * 
 *  @param argc Number of arguments
 *  @param argv List of the arguments (array)
 *  @return 0 if no error
 */
static int client( int argc, char *argv[] ) {
	int					status;
	char				rec_buff[80];
	int32_t				answer;
	int					index;

	display( "Client", "Start process\n" );
	messip_init( );
    messip_cnx_t *cnx = messip_connect( NULL, "ex3/p2", MESSIP_NOTIMEOUT );
    if ( !cnx ) 
        cancel( "Unable to find messip server\n" );

	/*
		Localize channel 'one'
	*/
    messip_channel_t *ch = NULL;
    for (time_t t0 = time(NULL); time(NULL) - t0 < 10; ) {
        ch = messip_channel_connect( cnx, "one", 5000 );
        if (ch)
            break;
        sleep(1);
    }
    if ( !ch )
        cancel( "Unable to localize channel '%s'\n", "one" );
	display( "Client", "Channel located - remote_id=%s\n", 
		ch->remote_id );

	for (;;) {
		memset( rec_buff, 0, sizeof( rec_buff ) );
		status = messip_send( ch, 
			123, "Hello", 6, 
			&answer, rec_buff, sizeof(rec_buff), 5000 );
		display( "Client", "status=%d answer=%d '%s'\n", 
			status, answer, rec_buff );	
		if (index == MESSIP_MSG_TIMEOUT)
			continue;
		break;
	}

	return 0;
}								// client

/**
 *  Main function
 * 
 *  @param argc Number of arguments
 *  @param argv List of the arguments (array)
 *  @return 0 if no error
 */
int main( int argc, char *argv[] ) {
    return exec_server_client( argc, argv, server, client );
}                               // main
