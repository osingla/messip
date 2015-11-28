/**
 * @file p.c
 * 
 **/

/**
 * @mainpage messip - Examples programs - No. 9
 * 
 * MessIP : Message Passing over TCP/IP \n
 * Copyright (C) 2001-2007  Olivier Singla \n
 * http://messip.sourceforge.net/ \n\n
 * 
 * Shows what do to when the message sent by the client exceeds the size of the receiving buffer on the server side.
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
	/*
		Connect to one messip server
	*/
	display( "Start process\n" );
	messip_init( );
    messip_cnx_t *cnx = messip_connect( NULL, "ex9/p1", MESSIP_NOTIMEOUT );
    if ( !cnx ) 
        cancel( "Unable to find messip server\n" );

	/*
		Create channel 'one'
	*/
	messip_channel_t *ch = messip_channel_create( cnx, "one", MESSIP_NOTIMEOUT, 0 );
	if ( !ch ) 
		cancel( "Unable to create channel '%s'\n", "one" );

	/*
		Now wait for one message on this on channel
	*/
	char rec_buff[80];
	memset( rec_buff, 0, sizeof(rec_buff) );
	int32_t type;
	int index = messip_receive( ch, 
		&type, rec_buff, sizeof(rec_buff)-1, 
		MESSIP_NOTIMEOUT );
	if ( index == -1 )
		cancel( "Error on receive message on channel '%s'\n", "one" );
	display( "received %d-%d:'%s' type=%d from %s index=%d\n", 
		ch->datalen, ch->datalenr, rec_buff, type, ch->remote_id, index );
	display( "received more %d-%d:'%s' index=%d\n", 
		ch->datalen, ch->datalenr, ch->receive_allmsg[index], index );
	assert( type == 1 );
	display( "...waiting 10 seconds before replying to the client...\n" );
	delay( 10000 );
	messip_reply( ch, index, 3005, "Bonjour", 8, MESSIP_NOTIMEOUT );

	/*
		Now wait for another message on this on channel
	*/
	memset( rec_buff, 0, sizeof(rec_buff) );
	index = messip_receive( ch, 
		&type, rec_buff, sizeof(rec_buff)-1, 
		MESSIP_NOTIMEOUT );
	if ( index == -1 )
		cancel( "Error on receive message on channel '%s'\n", "one" );
	display( "received %d-%d:'%s' type=%d from %s index=%d\n", 
		ch->datalen, ch->datalenr, rec_buff, type, ch->remote_id, index );
	assert( !strcmp( rec_buff, "Et voila!\n" ) );
	assert( type == 1961 );
	messip_reply( ch, index, 2003, "ok!", 4, MESSIP_NOTIMEOUT );

	/*
		Wait now channel-deconnexion message
	*/
	memset( rec_buff, 0, sizeof(rec_buff) );
	index = messip_receive( ch, &type, rec_buff, sizeof(rec_buff), MESSIP_NOTIMEOUT );
	if ( index == -1 )
		cancel( "Error on receive message on channel '%s'\n", "one" );
	display( "received %d-%d:'%s' type=%d from %s index=%d\n", 
		ch->datalen, ch->datalenr, rec_buff, type, ch->remote_id, index );
	assert(index==MESSIP_MSG_DISCONNECT || index==MESSIP_MSG_DISMISSED);

	display( "End process\n" );
	return 0;
}                               // server

static char msg1[] =
"ONE REASON THAT this type of versatility is not possible today is that handheld\n"
"gadgets are typically built around highly optimized specialty chips that do one\n"
"thing really well. These chips are fast and relatively cheap, but their circuits\n"
"are literally written in stone--or at least in silicon. A multipurpose gadget\n"
"would have to have many specialized chips--a costly and clumsy solution\n";

static char msg2[] =
"Et voila!\n";

/**
 *  Client-side function
 * 
 *  @param argc Number of arguments
 *  @param argv List of the arguments (array)
 *  @return 0 if no error
 */
static int client( int argc, char *argv[] ) {
	/*
		Connect to one messip server
	*/
	messip_init( );
	display( "start process\n" );
    messip_cnx_t *cnx = messip_connect( NULL, "ex9/p2", MESSIP_NOTIMEOUT );
    if ( !cnx ) 
        cancel( "Unable to find messip server\n" );

	/*
		Localize channel 'one'
	*/
	messip_channel_t *ch = messip_channel_connect( cnx, "one", MESSIP_NOTIMEOUT );
	if ( !ch ) 
		cancel( "Unable to localize channel '%s'\n", "one" );
	display( "Channel located - remote_id=%s\n", 
		ch->remote_id );

	/*
		Send a message on channel 'one'
	*/
	display( "sending now a message (len=%d)to the server...\n", sizeof(msg1) );
	char rec_buff[ 80 ];
	memset( rec_buff, 0, sizeof( rec_buff ) );
    int32_t answer;
	int status = messip_send( ch, 
		1, msg1, sizeof(msg1), 
		&answer, rec_buff, sizeof( rec_buff ), 
		MESSIP_NOTIMEOUT );
	display( "send status=%d received back=%d-%d:'%s' answer=%d  id=%s\n", 
		status, ch->datalen, ch->datalenr, rec_buff, answer, ch->remote_id );	
	assert(answer == 3005);
	display( "Now waiting 20 seconds...\n" );
	delay( 20000 );

	/*
	*/
	status = messip_send( ch, 
		1961, msg2, sizeof(msg2), 
		&answer, rec_buff, sizeof( rec_buff ), 
		MESSIP_NOTIMEOUT );
	display( "send status=%d received back=%d-%d:'%s' answer=%d  id=%s\n", 
		status, ch->datalen, ch->datalenr, rec_buff, answer, ch->remote_id );	
	assert(answer == 2003);
	

	/*
		Disconnect from this channel
	*/
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
