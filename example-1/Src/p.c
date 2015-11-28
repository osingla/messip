/**
 * @file p.c
 * 
 **/

/**
 * @mainpage messip - Examples programs - No. 1
 * 
 * MessIP : Message Passing over TCP/IP \n
 * Copyright (C) 2001-2007  Olivier Singla \n
 * http://messip.sourceforge.net/ \n\n
 * 
 * Server (p server):
 * - connect to the messip manager
 * - create a channel ('one') to receive messages
 * - wait for a message (no timeout) ["Hello"]
 * - when received the message, reply back with another message ["Bonjour"]
 * 
 * Client p2 (p client):
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
     * Connect to the messip server
     */
    display( "Start process\n" );
    messip_set_log_level( MESSIP_LOG_ERROR | MESSIP_LOG_WARNING
       | MESSIP_LOG_INFO /* | MESSIP_LOG_INFO_VERBOSE | MESSIP_LOG_DEBUG */  );
    messip_init(  );
    messip_cnx_t *cnx = messip_connect( NULL, "ex1/p1", MESSIP_NOTIMEOUT );
    if ( !cnx )
        cancel( "Unable to find messip manager\n" );

    /*
     * Create channel 'one'
     * We'll receive messages on this channel
     */
    messip_channel_t *ch = messip_channel_create( cnx, "one", MESSIP_NOTIMEOUT, 0 );
    if ( !ch )
        cancel( "Unable to create channel '%s'\n", "one" );

    /*
     * Now wait for one message on this on channel
     */
    display( "Server waiting 10 seconds - Sender (the client) should stay blocked...\n" );
    delay( 10000 );
    int32_t type;
    char rec_buff[80];
    int index = messip_receive( ch,
       &type,                   // this value will be given by the client
       rec_buff, sizeof( rec_buff ),    // Receive buffer - Max. size specified
       MESSIP_NOTIMEOUT );      // No timeout if communication would hang
    if ( index == -1 ) {
        fprintf( stderr, "%s %d:\n\tError on receive message #1 on channel '%s', errno=%d\n",
           __FILE__, __LINE__, "one", errno );
        return -1;
    }
    display( "received %d-%d:'%s' type=%d from id=%s index=%d\n",
       ch->datalen, ch->datalenr, rec_buff, type, ch->remote_id, index );
    assert( index >= 0 );
    assert( !strcmp( rec_buff, "Hello" ) );
    assert( type == 1961 );
    display( "...waiting 10 seconds before replying to the client...\n" );
    delay( 10000 );
    messip_reply( ch, index, 3005, "Bonjour", 8, MESSIP_NOTIMEOUT );

    /*
     * Wait another message, this time with dynamic allocation
     */
    char *rbuff;
    index = messip_receive( ch, &type,  // this value will be given by the client
       &rbuff, 0,               // Receive buffer - Dynamic allocation
       MESSIP_NOTIMEOUT );
    if ( index == -1 ) {
        fprintf( stderr, "%s %d:\n\tError on receive message #2 on channel '%s', errno=%d\n",
           __FILE__, __LINE__, "one", errno );
        return -1;
    }
    display( "received %d-%d:'%s' type=%d from id=%s index=%d\n",
       ch->datalen, ch->datalenr, rbuff, type, ch->remote_id, index );
    assert( index >= 0 );
    assert( type == 123 );
    messip_reply( ch, index, 256, "Linux", 6, MESSIP_NOTIMEOUT );
    free( rbuff );

    /*
     * Wait now channel-deconnexion message (sent by the client program)
     */
    memset( rec_buff, 0, sizeof( rec_buff ) );
    index = messip_receive( ch, &type, rec_buff, sizeof( rec_buff ), MESSIP_NOTIMEOUT );
    if ( index == -1 ) {
        fprintf( stderr, "%s %d:\n\tError on receive message #2 on channel '%s', errno=%d\n",
           __FILE__, __LINE__, "one", errno );
        return -1;
    }
    display( "received %d-%d:'%s' type=%d from id=%s index=%d\n",
       ch->datalen, ch->datalenr, rec_buff, type, ch->remote_id, index );
    assert( index == MESSIP_MSG_DISCONNECT || index == MESSIP_MSG_DISMISSED );

    /*
     * We just exit as
     * the MessIP manager will take care of releasing resources (channel)
     */
    display( "End process\n" );
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
    messip_set_log_level( MESSIP_LOG_ERROR | MESSIP_LOG_WARNING
       | MESSIP_LOG_INFO /* | MESSIP_LOG_INFO_VERBOSE | MESSIP_LOG_DEBUG */  );
    messip_init(  );
    display( "start process\n" );
    messip_cnx_t *cnx = messip_connect( NULL, "ex1/p2", MESSIP_NOTIMEOUT );
    if ( !cnx )
        cancel( "Unable to find messip manager\n" );

    /*--- Localize channel 'one' ---*/
    messip_channel_t *ch = messip_channel_connect( cnx, "one", MESSIP_NOTIMEOUT );
    if ( !ch )
        cancel( "Unable to localize channel '%s'\n", "one" );
    display( "Channel located - remote_id=%s\n", ch->remote_id );

    /*--- Send a 1st message on channel 'one' ---*/
    display( "sending now a 1st message to the server...\n" );
    int32_t answer;
    char rec_buff[80];
    int status = messip_send( ch,
       1961, "Hello", 6,
       &answer, rec_buff, sizeof( rec_buff ),
       MESSIP_NOTIMEOUT );
    if ( status == -1 ) {
        fprintf( stderr, "%s %d:\n\tError on send message #1 on channel '%s', errno=%d\n", __FILE__, __LINE__, "one", errno );
        return -1;
    }
    display( "send status=%d received back=%d-%d:'%s' answer=%d  id=%s\n",
       status, ch->datalen, ch->datalenr, rec_buff, answer, ch->remote_id );
    assert( answer == 3005 );

    /*--- Send a 2nd message on channel 'one' ---*/
    display( "sending now a 2nd message to the server...\n" );
    char *rbuff;
    status = messip_send( ch, 123, "Unix", 5, &answer, &rbuff, 0, MESSIP_NOTIMEOUT );
    display( "send status=%d received back=%d-%d:'%s' answer=%d  id=%s\n",
       status, ch->datalen, ch->datalenr, rbuff, answer, ch->remote_id );
    if ( status == -1 ) {
        fprintf( stderr, "%s %d:\n\tError on send message #2 on channel '%s', errno=%d\n", __FILE__, __LINE__, "one", errno );
        return -1;
    }
    assert( answer == 256 );
    free( rbuff );

    /*--- Then 2 ping messages ---*/
    delay( 3000 );
    display( "Waiting for ping...\n" );
    status = messip_channel_ping( ch, 5000 );
    display( "Status ping = %d\n", status );
    delay( 3000 );
    display( "Waiting for ping...\n" );
    status = messip_channel_ping( ch, 5000 );
    display( "Status ping = %d\n", status );

    /*--- Disconnect from this channel ---*/
    status = messip_channel_disconnect( ch, MESSIP_NOTIMEOUT );
    display( "Status disconnect channel = %d\n", status );

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
