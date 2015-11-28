/**
 * @file p.c
 * 
 **/

/**
 * @mainpage messip - Examples programs - No. 7
 * 
 * MessIP : Message Passing over TCP/IP \n
 * Copyright (C) 2001-2007  Olivier Singla \n
 * http://messip.sourceforge.net/ \n\n
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
    messip_cnx_t *cnx = messip_connect( NULL, "ex7/p1", MESSIP_NOTIMEOUT );
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
	delay( 20000 );
	char rec_buff[80];
	memset( rec_buff, 0, sizeof( rec_buff ) );
    int32_t type;
	int index = messip_receive( ch, &type, rec_buff, sizeof( rec_buff ), MESSIP_NOTIMEOUT );
	if (index == -1)
		cancel( "Error on receive message on channel '%s'\n", "one" );
	display( "index=%d received '%s' type=%d  from id=%s\n", 
		index, rec_buff, type, ch->remote_id );
	assert( !strcmp( rec_buff, "Hello" ) );
	assert( type == 0x1961 );
	messip_reply( ch, index, 0x3005, "Bonjour", 8, MESSIP_NOTIMEOUT );

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
	/*
		Connect to one messip server
	*/
	messip_init( );
	display( "start process\n" );
    messip_cnx_t *cnx = messip_connect( NULL, "ex7/p2", MESSIP_NOTIMEOUT );
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
	char rec_buff[ 80 ];
	memset( rec_buff, 0, sizeof( rec_buff ) );
    int32_t answer;
	int status = messip_send( ch, 
		0x1961, "Hello", 6, 
		&answer, rec_buff, sizeof( rec_buff ), 
		MESSIP_NOTIMEOUT );
	display( "send status=%d received back='%s' answer=%d  remote id=%s\n", 
		status, rec_buff, answer,
		ch->remote_id );	
	assert( answer == 0x3005 );

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
