/**
 * @file p.c
 * 
 **/

/**
 * @mainpage messip - Examples programs - No. 5
 * 
 * MessIP : Message Passing over TCP/IP \n
 * Copyright (C) 2001-2007  Olivier Singla \n
 * http://messip.sourceforge.net/ \n\n
 *
 * Performance test program: the client sends messages to the server for 10 seconds
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
    messip_cnx_t *cnx = messip_connect( NULL, "ex5/p1", MESSIP_NOTIMEOUT );
    if ( !cnx ) 
        cancel( "Unable to find messip server\n" );

	/*--- Create channel 'one' ---*/
	messip_channel_t *ch = messip_channel_create( cnx, "one", MESSIP_NOTIMEOUT, 0 );
	if ( !ch ) 
		cancel( "Unable to create channel '%s'\n", "one" );

	/*--- Now wait for the 1st message on this on channel ---*/
	char *rec_buff = malloc( 100 );
	memset( rec_buff, 0, 100 );
    int32_t type;
	int index = messip_receive( ch, &type, rec_buff, 100, MESSIP_NOTIMEOUT );
	if ( index == -1 )
		cancel( "Error on receive message on channel '%s'\n", "one" );
	display( "received '%s' from '%s' type=%X  index=%d\n", 
		rec_buff, ch->remote_id, 
		type, index );
	assert ( !strcmp( rec_buff, "Hello1" ) );
	messip_reply( ch, index, 0x201, "Bonjour1", 9, MESSIP_NOTIMEOUT );
	int send_sz  = type & 0xFFFF;
	int reply_sz = type >> 16;

	/*--- Now wait for the 2nd message on this on channel ---*/
	memset( rec_buff, 0, 100 );
	index = messip_receive( ch, &type, rec_buff, 100, MESSIP_NOTIMEOUT );
	if ( index == -1 )
	    cancel( "Error on receive message on channel '%s'\n", "one" );
	display( "received '%s' from '%s' index=%d\n", 
		rec_buff, ch->remote_id, index );
	assert ( !strcmp( rec_buff, "Hello2" ) );
	messip_reply( ch, index, 0x302, "Bonjour2", 9, MESSIP_NOTIMEOUT );
	free( rec_buff );
	rec_buff = (send_sz) ? malloc( send_sz ) : NULL;
	char *reply_buff = (reply_sz) ? malloc( reply_sz ) : NULL;
	for ( int k = 0; k < reply_sz; k++ )
		reply_buff[k] = 97 + k%26;

	/*--- Now receive messages ---*/
	int cnt = 0;
	for (;;) {
		index = messip_receive( ch, &type, rec_buff, send_sz, MESSIP_NOTIMEOUT );
		assert(index != -1);
		if ( type == -1 )
			break;
        if ( type != cnt++ )
            cancel( "OOPS! type=%X cnt=%X\n", type, cnt-1 );
//		display( "%d: received '%s' from '%s' index=%d\n", 
//			cnt, rec_buff, ch->remote_id, index );
		if (index != MESSIP_MSG_NOREPLY)
			messip_reply( ch, index, cnt, reply_buff, reply_sz, MESSIP_NOTIMEOUT );
	}							// for (;;)
	display( "Received %ld msg\n", cnt );

	return 0;
}                               // server

static int is_blocking=1;
static int is_non_blocking=0;
static int send_sz=100;
static int reply_sz=20;
static int duration=10;

/**
 *  Client-side function
 * 
 *  @param argc Number of arguments
 *  @param argv List of the arguments (array)
 *  @return 0 if no error
 */
static int client( int argc, char *argv[] ) {
	int32_t	answer;
	int status;
	int k;
	int option_index,
	  c;
	static struct option long_options[] = {
		{ "blocking", 1, NULL, 'b' },
		{ "noblocking", 1, NULL, 'n' },
		{ "duration", 1, NULL, 'd' },
		{ "send", 1, NULL, 's' },
		{ "reply", 1, NULL, 'r' },
		{ NULL, 0, NULL, 0 }
	};

	/*--- Any parameter ? ---*/
	for (;;)
	{
		c = getopt_long( argc, argv, "bnd:s:r:", long_options, &option_index );
		if ( c == -1 )
			break;
//		printf( "c=%d option_index=%d arg=[%s]\n", c, option_index, optarg );
		switch ( c )
		{
			case 'b' :
				is_blocking = 1;
				is_non_blocking = 0;
				break;
			case 'n' :
				is_blocking = 0;
				is_non_blocking = 1;
				break;
			case 'd' :
				duration = atoi( optarg );
				break;
			case 's' :
				send_sz = atoi( optarg );
				break;
			case 'r' :
				reply_sz = atoi( optarg );
				break;
		}						// switch
	}							// for (;;)
	fprintf( stdout, "Test %s messages for %d seconds, sending %d bytes, replying %d bytes\n",
		(is_blocking) ? "blocking" : "non-blocking",
		duration,
		send_sz, reply_sz );

	/*--- Connect to one messip server ---*/
	messip_init( );
	display( "start process\n" );
    messip_cnx_t *cnx = messip_connect( NULL, "ex5/p2", MESSIP_NOTIMEOUT );
    if ( !cnx ) 
        cancel( "Unable to find messip server\n" );

	/*--- Localize channel 'one' ---*/
	messip_channel_t *ch = messip_channel_connect( cnx, "one", MESSIP_NOTIMEOUT );
	if ( !ch ) 
		cancel( "Unable to localize channel '%s'\n", "one" );
	display( "Channel located - remote_id=%s\n", 
		ch->remote_id );

	/*--- Send a 1st message on channel 'one' ---*/
	char *reply_buff = malloc( 100 );
	memset( reply_buff, 0, 100 );
	status = messip_send( ch, 
		send_sz + (reply_sz << 16), "Hello1", 7, 
		&answer, reply_buff, 100, 
		MESSIP_NOTIMEOUT );
	display( "send status=%d received back='%s'  remote_id=%s\n", 
		status, reply_buff, ch->remote_id );	

	/*--- Send a 2nd message on channel 'one' ---*/
	memset( reply_buff, 0, 100 );
	status = messip_send( ch, 
		0, "Hello2", 7, 
		&answer, reply_buff, 100, 
		MESSIP_NOTIMEOUT );
	display( "send status=%d received back='%s'  remote_id=%s\n", 
		status, reply_buff, ch->remote_id );	

	/*--- Send blocking messages for 10 seconds ---*/
	char *send_buff = (send_sz) ? malloc( send_sz ) : NULL;
	for ( k = 0; k < send_sz; k++ )
		send_buff[k] = 65 + k%26;
	free( reply_buff );
	reply_buff = (reply_sz) ? malloc( reply_sz ) : NULL;
	uint32_t cnt = 0;
    time_t t0 = time( NULL );
	if ( is_blocking ) {
		while ( time( NULL ) - t0 < duration ) {
			int status = messip_send( ch, 
				cnt++, send_buff, send_sz, 
				&answer, reply_buff, reply_sz, 
				MESSIP_NOTIMEOUT );
			assert(status >= 0);
		}						// while
	}							// if
	else {
		while ( time( NULL ) - t0 < duration ) {
			int status = messip_buffered_send( ch, 
				cnt++, send_buff, send_sz, 
				MESSIP_NOTIMEOUT );
			assert( status != -1 );
		}                       // while
	}							// else

	display( "sent %d msg, %d msg/sec\n", 
        cnt, cnt/duration );
	status = messip_send( ch, 
		-1, NULL, 0, 
		&answer, NULL, 0, 
		MESSIP_NOTIMEOUT );

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
