/**
 * @file p.c
 * 
 **/

/**
 * @mainpage messip - Examples programs - No. 8
 * 
 * MessIP : Message Passing over TCP/IP \n
 * Copyright (C) 2001-2007  Olivier Singla \n
 * http://messip.sourceforge.net/ \n\n
 * 
 * Messages sent/received using several channels.
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
#include <getopt.h>
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
static messip_channel_t *create_channel( messip_cnx_t *cnx, char *name )
{
	messip_channel_t *ch = messip_channel_create( cnx, name, MESSIP_NOTIMEOUT, 0 );
	if ( !ch ) 
		cancel( "Unable to create channel '%s'\n", "one" );
	return ch;
}								// create_channel

static int server( int argc, char *argv[] ) {
	/*--- Connect to one messip server ---*/
	display( "Server", "Start process\n" );
	messip_init( );
    messip_cnx_t *cnx = messip_connect( NULL, "ex8/p1", MESSIP_NOTIMEOUT );
    if ( !cnx ) {
        cancel( "Unable to find messip server\n" );
    }

	/*--- Create 5 channels ---*/
	messip_channel_t *ch1 = create_channel( cnx, "one" );
	messip_channel_t *ch2 = create_channel( cnx, "two" );
	messip_channel_t *ch3 = create_channel( cnx, "three" );
	messip_channel_t *ch4 = create_channel( cnx, "four" );
	messip_channel_t *ch5 = create_channel( cnx, "five" );

	/*--- Now delete the 3rd channel ---*/
	display( "Server", "Channels have been created. Waiting 15 seconds...\n" );
	delay( 15000 );
	int status = messip_channel_delete( ch3, MESSIP_NOTIMEOUT );
	display( "Server", "messip_channel_delete #3: status=%d\n", status );
	assert(status == 0);
	delay( 15000 );

	/*--- Attach to channel four ---*/
	messip_channel_t *xch4 = messip_channel_connect( cnx, "four", MESSIP_NOTIMEOUT );
	assert(xch4 != NULL);
	status = messip_channel_delete( ch4, MESSIP_NOTIMEOUT );
	display( "Server", "messip_channel_delete #4: status=%d\n", status );
	assert(status == 1);

	/*--- Now delete the 4th channel ---*/
	status = messip_channel_disconnect( xch4, MESSIP_NOTIMEOUT );
	display( "Server", "messip_channel_disconnect #4: status=%d\n", status );
	status = messip_channel_delete( ch4, MESSIP_NOTIMEOUT );
	display( "Server", "messip_channel_delete #4: status=%d\n", status );
	assert(status == 0);

	/*--- Now wait for one message on this on channel ---*/
	display( "Server", "Wait now 10 seconds...\n" );
	delay( 10000 );
	char rec_buff[ 80 ];
	memset( rec_buff, 0, sizeof(rec_buff) );
    int32_t type;
	int index = messip_receive( ch1, &type, rec_buff, sizeof(rec_buff), MESSIP_NOTIMEOUT );
	if ( index == -1 )
	{
		fprintf( stderr, "Error on receive message on channel '%s'\n", "one" );
		return -1;
	}
	display( "Server", "received %d-%d:'%s' type=%d from id=%s index=%d\n", 
		ch1->datalen, ch1->datalenr, rec_buff, type, ch1->remote_id, index );
	assert( !strcmp( rec_buff, "Hello" ) );
	assert( type == 1961 );
	display( "Server", "...waiting 10 seconds before replying to the client...\n" );
	delay( 10000 );
	messip_reply( ch1, index, 3005, "Bonjour", 8, MESSIP_NOTIMEOUT );

	/*--- Wait now channel-deconnexion message ---*/
	memset( rec_buff, 0, sizeof(rec_buff) );
	index = messip_receive( ch1, &type, rec_buff, sizeof(rec_buff), MESSIP_NOTIMEOUT );
	if ( index == -1 )
	{
		fprintf( stderr, "Error on receive message on channel '%s'\n", "one" );
		return -1;
	}
	display( "Server", "received %d-%d:'%s' type=%d from %s index=%d\n", 
		ch1->datalen, ch1->datalenr, rec_buff, type, ch1->remote_id, index );
	assert(index==MESSIP_MSG_DISCONNECT || index==MESSIP_MSG_DISMISSED);

	display( "Server", "End process\n" );
	return 0;
}                               // server

/**
 *  Client-side function
 * 
 *  @param argc Number of arguments
 *  @param argv List of the arguments (array)
 *  @return 0 if no error
 */
static int client( int argc, char *argv[] ) {
	/*--- Connect to one messip server ---*/
	messip_init( );
	display( "Client", "start process\n" );
    messip_cnx_t *cnx = messip_connect( NULL, "ex8/p2", MESSIP_NOTIMEOUT );
    if ( !cnx ) {
        cancel( "Unable to find messip server\n" );
    }

	/*--- Localize channel 'one' ---*/
	messip_channel_t *ch = messip_channel_connect( cnx, "one", MESSIP_NOTIMEOUT );
	if ( !ch ) {
		cancel( "Unable to localize channel '%s'\n", "one" );
    }
	display( "Client", "Channel located - remote_id=%s\n", 
		ch->remote_id );

	/*--- Send a message on channel 'one' ---*/
	display( "Client", "sending now a message to the server...\n" );
	char rec_buff[ 80 ];
	memset( rec_buff, 0, sizeof( rec_buff ) );
    int32_t answer;
	int status = messip_send( ch, 
		1961, "Hello", 6, 
		&answer, rec_buff, sizeof( rec_buff ), 
		MESSIP_NOTIMEOUT );
	display( "Client", "send status=%d received back=%d-%d:'%s' answer=%d  id=%s\n", 
		status, ch->datalen, ch->datalenr, rec_buff, answer, ch->remote_id );	
	assert( answer == 3005 );

	/*--- Then 2 ping messages ---*/
	delay( 3000 );
	display( "Client", "Waiting for ping...\n" );
	messip_channel_ping( ch, 5000 );
	delay( 3000 );
	messip_channel_ping( ch, 5000 );
	display( "Client", "Waiting for ping...\n" );

	/*--- Disconnect from this channel ---*/
	messip_channel_disconnect( ch, MESSIP_NOTIMEOUT );

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
