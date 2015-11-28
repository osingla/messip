/**
 * @file p.c
 * 
 **/

/**
 * @mainpage messip - Examples programs - No. 6
 * 
 * MessIP : Message Passing over TCP/IP \n
 * Copyright (C) 2001-2007  Olivier Singla \n
 * http://messip.sourceforge.net/ \n\n
 * 
 * This example shows how to use a timer:
 * canned messages sent automatically uppon timer expiration. 
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
    messip_cnx_t *cnx = messip_connect( NULL, "ex6/p1", MESSIP_NOTIMEOUT );
    if ( !cnx ) 
        cancel( "Unable to find messip server\n" );

	/*
		Create channel 'one'
	*/
	messip_channel_t *ch = messip_channel_create( cnx, "one", MESSIP_NOTIMEOUT, 0 );
	if ( !ch ) 
		cancel( "Unable to create channel '%s'\n", "one" );

	/*
		1st timer
	*/
	timer_t timer1 = messip_timer_create( ch, 1961, 5000, 30000, MESSIP_NOTIMEOUT );
	display( "1st timer id=%d: type=%d - 1st shot in 5 sec, each 30 sec after that\n",
		timer1, 1961 );
	delay( 10000 );
	timer_t timer2 = messip_timer_create( ch, 1789, 20000, 10000, MESSIP_NOTIMEOUT );
	display( "2nd timer id=%d: type=%d - 1st shot in 20 sec, each 10 sec after that\n",
		timer2, 1789 );

	/*
		Now receive messages
	*/
	int cnt = 0;
	for (;;) {
	    char rec_buff[100];
		cnt++;
        int32_t type;
		int index = messip_receive( ch, 
			&type, 
			rec_buff, sizeof(rec_buff), 
			MESSIP_NOTIMEOUT );
		assert(index != -1);
		if (index == MESSIP_MSG_TIMER) {
			display( "timer %4d type=%d from %s\n", 
				cnt, type, ch->remote_id  );
		}
		else if ( (index == MESSIP_MSG_DISCONNECT) || (index == MESSIP_MSG_DISMISSED) ) {
			display( "index=%d type=%d from id=%s\n", 
				index, type, ch->remote_id );
		}
		else {
			display( "index=%d received '%s' type=%d from id=%s\n", 
				index, rec_buff, type, ch->remote_id );
			messip_reply( ch, index, 0, "ABCDEFGHI", 10, MESSIP_NOTIMEOUT );
		}
	}							// for (;;)

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
    messip_cnx_t *cnx = messip_connect( NULL, "ex1/p2", MESSIP_NOTIMEOUT );
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
		Send a 1st message on channel 'one'
	*/
	char rec_buff[ 100 ];
	memset( rec_buff, 0, sizeof(rec_buff) );
    int32_t answer;
	int status = messip_send( ch, 
		1, "Hello1", 7, 
		&answer, rec_buff, sizeof(rec_buff), 
		MESSIP_NOTIMEOUT );
	display( "send status=%d received back='%s'  remote_id=%s\n", 
		status, rec_buff, 
		ch->remote_id );	

	/*
		Send a 2nd message on channel 'one'
	*/
	memset( rec_buff, 0, sizeof(rec_buff) );
	status = messip_send( ch, 
		3, "Hello2", 7, 
		&answer, rec_buff, sizeof(rec_buff), 
		MESSIP_NOTIMEOUT );
	display( "send status=%d received back='%s'  remote_id=%s\n", 
		status, rec_buff, 
		ch->remote_id );	

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
