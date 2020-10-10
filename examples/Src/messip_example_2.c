/**
 * @file p.c
 * 
 **/

/**
 * @mainpage messip - Examples programs - No. 2
 * 
 * MessIP : Message Passing over TCP/IP \n
 * Copyright (C) 2001-2007  Olivier Singla \n
 * http://messip.sourceforge.net/ \n\n
 * 
 * Server (p server):
 * - connect to the messip manager
 * - create a channel ('one') to receive messages
 * - wait for a message (no timeout) from {p2},
 *   do not reply immediatelly
 * - wait for a message (no timeout) from {p3},
 *   do not reply immediatelly
 * - simulate some processing (sleep 5)
 * - then reply back with another message to {p2}
 * - then reply back with another message to {p3}
 * 
 * Clients (p client1 and p client2):
 * - connect to the messip manager
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
int server( int argc, char *argv[] ) {
    /*
     * Connect to one messip server
     */
    display( "Server", "start process\n" );
    messip_init(  );
    messip_cnx_t *cnx = messip_connect( NULL, "ex2/p1", MESSIP_NOTIMEOUT );
    if ( !cnx )
        cancel( "Unable to find messip server\n" );

    /*
     * Create channel 'one'
     */
    messip_channel_t *ch = messip_channel_create( cnx, "one", MESSIP_NOTIMEOUT, 0 );
    if ( !ch )
        cancel( "Unable to create channel '%s'\n", "one" );

    /*
     * Ask to be notified of the death of processes
     */
    messip_death_notify( cnx, MESSIP_TRUE, MESSIP_NOTIMEOUT );

    /*
     * Now wait for the 1st message on this channel
     */
    char rec_buff1[80];
    memset( rec_buff1, 0, sizeof( rec_buff1 ) );
    int32_t type;
    int index1 = messip_receive( ch, &type, rec_buff1, sizeof( rec_buff1 ), MESSIP_NOTIMEOUT );
    if ( index1 == -1 ) {
        fprintf( stderr, "Error on receive message 1 on channel '%s'\n", "one" );
        return -1;
    }
    display( "Server", "received (1) %d:'%s' type=%d from id %s index=%d\n", ch->datalen, rec_buff1, type, ch->remote_id, index1 );
    if ( index1 == MESSIP_MSG_DISMISSED ) {
        fprintf( stderr, "Process dismissed!\n" );
        return -1;
    }
    assert( !strcmp( rec_buff1, "Hello1" ) );

    /*
     * Then wait for the 2nd message on this channel
     */
    char rec_buff2[80];
    memset( rec_buff2, 0, sizeof( rec_buff2 ) );
    int index2 = messip_receive( ch, &type, rec_buff2, sizeof( rec_buff2 ), MESSIP_NOTIMEOUT );
    if ( index2 == -1 ) {
        fprintf( stderr, "Error on receive message 2 on channel '%s'\n", "one" );
        return -1;
    }
    display( "Server", "received (2) %d:'%s' type=%d from id %s index=%d\n", ch->datalen, rec_buff2, type, ch->remote_id, index2 );
    if ( index2 == MESSIP_MSG_DISMISSED ) {
        fprintf( stderr, "Process dismissed!\n" );
        return -1;
    }
    assert( !strcmp( rec_buff2, "Hello2" ) );

    sleep( 5 );
    display( "Server", "reply to 1st receive\n" );
    messip_reply( ch, index1, 1234, "Bonjour1", 9, MESSIP_NOTIMEOUT );
    display( "Server", "reply to 2nd receive\n" );
    messip_reply( ch, index2, 2345, "Bonjour2", 9, MESSIP_NOTIMEOUT );

    /*
     * Now wait for the 3rd message on this channel
     */
    char rec_buff3[80];
    memset( rec_buff3, 0, sizeof( rec_buff3 ) );
    int index3 = messip_receive( ch, &type, rec_buff3, sizeof( rec_buff3 ), MESSIP_NOTIMEOUT );
    if ( index3 == -1 ) {
        fprintf( stderr, "Error on receive message 3 on channel '%s'\n", "one" );
        return -1;
    }
    display( "Server", "received (3) %d:'%s' type=%d from id %s index=%d\n", ch->datalen, rec_buff3, type, ch->remote_id, index3 );
    if ( index3 == MESSIP_MSG_DISMISSED ) {
        fprintf( stderr, "Process dismissed!\n" );
        return -1;
    }
    assert( !strcmp( rec_buff3, "Hello3" ) || !strcmp( rec_buff3, "Hello4" ) );

    /*
     * Then wait for the 4th message on this channel
     */
    char rec_buff4[80];
    memset( rec_buff4, 0, sizeof( rec_buff4 ) );
    int index4 = messip_receive( ch, &type, rec_buff4, sizeof( rec_buff4 ), MESSIP_NOTIMEOUT );
    if ( index4 == -1 ) {
        fprintf( stderr, "Error on receive message 4 on channel '%s'\n", "one" );
        return -1;
    }
    display( "Server", "received (4) %d:'%s' type=%d from id %s index=%d\n", ch->datalen, rec_buff4, type, ch->remote_id, index4 );
    if ( index4 == MESSIP_MSG_DISMISSED ) {
        fprintf( stderr, "Process dismissed!\n" );
        return -1;
    }
    assert( !strcmp( rec_buff3, "Hello3" ) || !strcmp( rec_buff3, "Hello4" ) );

    sleep( 5 );
    if ( !strcmp( rec_buff3, "Hello3" ) ) {
        messip_reply( ch, index4, 2345, "Bonjour4", 9, MESSIP_NOTIMEOUT );
        messip_reply( ch, index3, 1234, "Bonjour3", 9, MESSIP_NOTIMEOUT );
    }
    else {
        messip_reply( ch, index3, 2345, "Bonjour4", 9, MESSIP_NOTIMEOUT );
        messip_reply( ch, index4, 1234, "Bonjour3", 9, MESSIP_NOTIMEOUT );
    }

    return 0;

}                               // server

/**
 *  Client-side function
 * 
 *  @param argc Number of arguments
 *  @param argv List of the arguments (array)
 *  @return 0 if no error
 */
int client( char const *mark, int send_int, char *send_str1, char *send_str2, char *rec_str1, char *rec_str2 ) {

    /*--- Connect to one messip server ---*/
    display( mark, "start process\n" );
    messip_init(  );
    messip_cnx_t *cnx = messip_connect( NULL, "ex2/p2", MESSIP_NOTIMEOUT );
    if ( !cnx )
        cancel( "Unable to find messip server\n" );

    /*--- Localize channel 'one' ---*/
    messip_channel_t *ch = NULL;
    for (time_t t0 = time(NULL); time(NULL) - t0 < 10; ) {
        ch = messip_channel_connect( cnx, "one", MESSIP_NOTIMEOUT );
        if (ch)
            break;
        sleep(1);
    }
    if ( !ch )
        cancel( "Unable to localize channel '%s'\n", "one" );

    /*--- Send a 1st message on channel 'one' ---*/
    char rec_buff[80];
    memset( rec_buff, 0, sizeof( rec_buff ) );
    int32_t answer;
    assert( strlen( send_str1 ) + 1 == 7 );
    int status = messip_send( ch,
       send_int, send_str1, 7,
       &answer, rec_buff, sizeof( rec_buff ),
       MESSIP_NOTIMEOUT );
    display( mark, "send status=%d received back=%d:'%s' answer=%d\n", status, ch->datalen, rec_buff, answer );
    assert( !strcmp( rec_buff, rec_str1 ) );

    /*--- Send a 2nd message on channel 'one' ---*/
    memset( rec_buff, 0, sizeof( rec_buff ) );
    assert( strlen( send_str2 ) + 1 == 7 );
    status = messip_send( ch, send_int, send_str2, 7, &answer, rec_buff, sizeof( rec_buff ), MESSIP_NOTIMEOUT );
    display( mark, "send status=%d received back=%d:'%s' answer=%d\n", status, ch->datalen, rec_buff, answer );
    assert( !strcmp( rec_buff, rec_str2 ) );

    return 0;
}                               // client

/**
 *  Client-side function
 * 
 *  @param argc Number of arguments
 *  @param argv List of the arguments (array)
 *  @return 0 if no error
 */
int client1( int argc, char *argv[] ) {
    return client( "Client1", 1961, "Hello1", "Hello3", "Bonjour1", "Bonjour3" );
}                               // client1

/**
 *  Client-side function
 * 
 *  @param argc Number of arguments
 *  @param argv List of the arguments (array)
 *  @return 0 if no error
 */
int client2( int argc, char *argv[] ) {
    return client( "Client2", 2002, "Hello2", "Hello4", "Bonjour2", "Bonjour4" );
}                               // client2

/**
 *  Main function
 * 
 *  @param argc Number of arguments
 *  @param argv List of the arguments (array)
 *  @return 0 if no error
 */
int main( int argc, char *argv[] ) {
    return exec_server_client2( argc, argv, server, client1, client2 );
}                               // main
