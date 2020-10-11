/**
 * @file p.c
 * 
 **/

/**
 * @mainpage messip - Examples programs - No. 4
 * 
 * MessIP : Message Passing over TCP/IP \n
 * Copyright (C) 2001-2007  Olivier Singla \n
 * http://messip.sourceforge.net/ \n\n
 * 
 * Test of asynchronous messages (non blocking buffered messages)
 * 
 * process p1 (p server):
 * - connect to the messip server
 * - create a channel ('one') to receive messages
 * - wait for messages
 * - when received a synchronous message, reply back with another message ["Bonjour"]
 * 
 * process p2 (p client):
 * - connect to the messip server
 * - locate the channel ('one') to send messages
 * - send several asynchronous messages
 * - send 2 synchronous messages
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

	/*--- Connect to one messip server ---*/
	messip_init( );
    messip_cnx_t *cnx = messip_connect( NULL, "ex4/p1", MESSIP_NOTIMEOUT );
    if ( !cnx ) {
        cancel( "Unable to find messip server\n" );
    }

	/*--- Create channel 'one' ---*/
	messip_channel_t *ch = messip_channel_create( cnx, "one", MESSIP_NOTIMEOUT, 3 );    // 3 msg only can be bufferred
	if ( !ch ) {
		cancel( "Unable to create channel '%s'\n", "one" );
    }

	/*--- Now wait for the 1st message on this channel ---*/
	display( "Server", "Sleep for 10 seconds...\n" );
	delay( 10000 );
	for (;;) {
		memset( rec_buff, 0, sizeof(rec_buff) );
		index = messip_receive( ch, 
			&type, 
			rec_buff, sizeof( rec_buff ), 
			MESSIP_NOTIMEOUT );
		assert( index != -1 );
		display( "Server", "index=%-3d from '%s' type=%4d %d:%d-'%s'\n", 
			index, ch->remote_id, type, ch->datalen, ch->datalenr, rec_buff );
		if (index == MESSIP_MSG_NOREPLY)
			continue;
		if (index == MESSIP_MSG_DISMISSED)
			break;
		assert(index >= 0);
		messip_reply( ch, index, 3, "Bonjour", 8, MESSIP_NOTIMEOUT );
	    delay( 10000 );
	}

	return 0;
}                               // server

static void send_buffered( messip_channel_t *ch, int32_t type, char *msg, long timeout ) {
	int status = messip_buffered_send( ch, type, msg, strlen(msg)+1, timeout );
	display( "Client", "send to %s: type=%d [%s] status=%d\n", 
		ch->remote_id, type, msg, status );
	assert (status >= 0);
}								// send_buffered

/**
 *  Client-side function
 * 
 *  @param argc Number of arguments
 *  @param argv List of the arguments (array)
 *  @return 0 if no error
 */
static int client( int argc, char *argv[] ) {

	int					status;
	int32_t				answer;
	char				rec_buff[200];

	/*--- Connect to one messip server ---*/
	messip_init( );
	display( "Client", "start process\n" );
    messip_cnx_t *cnx = messip_connect( NULL, "ex4/p2", MESSIP_NOTIMEOUT );
    if ( !cnx ) {
        cancel( "Unable to find messip server\n" );
    }

	/*--- Localize channel 'one' ---*/
	messip_channel_t *ch = messip_channel_connect( cnx, "one", MESSIP_NOTIMEOUT );
	if ( !ch ) {
		cancel( "Unable to localize channel '%s'\n", "one" );
    }

	send_buffered( ch, 8001, "Un",     MESSIP_NOTIMEOUT );
	send_buffered( ch, 7002, "Deux",   MESSIP_NOTIMEOUT );
	send_buffered( ch,    1, "Trois",  MESSIP_NOTIMEOUT );
	send_buffered( ch,    3, "Quatre", MESSIP_NOTIMEOUT );
	send_buffered( ch,    5, "Cinq",   MESSIP_NOTIMEOUT );

	/*--- Send now a blocking message ---*/
	status = messip_send( ch, 
		0x1961, "Hello2", 7, 
		&answer, rec_buff, sizeof(rec_buff), 
		MESSIP_NOTIMEOUT );
	display( "Client", "send2 status=%d remote_id='%s' answer=%d reply=[%s]\n", 
		status, 
		ch->remote_id,
		answer, rec_buff );

	sleep( 10 );

	return 0;
}                               // client

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
