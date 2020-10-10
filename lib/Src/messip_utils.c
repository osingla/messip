/**
 * @file messip_utils.c
 *
 *	MessIP : Message Passing over TCP/IP
 *  Copyright (C) 2001-2007  Olivier Singla
 *	http://messip.sourceforge.net/
 *
 *	TBD
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "messip.h"

int read_etc_messip( char *hostname, int *port_used, int *port_http_used ) {
    char line[512], host[80], path[80];
    size_t len;
    int port, port_http;

    strcpy( hostname, "localhost" );
    FILE *fp = fopen( "/usr/etc/messip", "r" );
    if ( fp == NULL ) {
        fprintf( stderr, "Unable to open /usr/etc/messip - errno=%d\n", errno );
        return -1;
    }

    *port_used = -1;
    if ( port_http_used != NULL )
        *port_http_used = -1;
    int first = 1;
    while ( fgets( line, sizeof( line ) - 1, fp ) ) {

        /*--- Skip line beginning with '#' ---*/
        if ( ( ( len = strlen( line ) ) >= 1 ) && ( line[len - 1] == '\n' ) )
            line[len - 1] = 0;
        if ( !strcmp( line, "" ) || ( line[0] == '#' ) )
            continue;

        sscanf( line, "%s %d %d %s", host, &port, &port_http, path );
//      if ( !strcmp( hostname, host ) )
        {
            strcpy( hostname, host );
            *port_used = port;
            if ( port_http_used != NULL )
                *port_http_used = port_http;
        }
        first = 0;

    }                           // while

    if ( first ) {
        fprintf( stderr, "No hostname in /usr/etc/messip\n" );
        return -1;
    }

    fclose( fp );
    return 0;

}                               // read_etc_messip


/*
	get_taskname
		Get the Process Name, from the PID
		
	This is a Linux only version
*/

/* Returns the clock speed of the system's CPU in MHz, as reported by
  /proc/cpuinfo. On a multiprocessor machine, returns the speed of
  the first CPU. On error returns zero. */

static float cpu_clock_speed( void ) {
    FILE *fp;
    char buffer[4095 + 1];
    size_t bytes_read;
    char *match;
    float clock_speed;

    /*
     * Read the entire contents of /proc/cpuinfo into the buffer. 
     */
    fp = fopen( "/proc/cpuinfo", "r" );
    if ( fp == NULL )
        return 0;
    bytes_read = fread( buffer, 1, sizeof( buffer ), fp );
    fclose( fp );

    /*
     * Bail if read failed or if buffer isn't big enough. 
     */
    if ( bytes_read == 0 || bytes_read == sizeof( buffer ) )
        return 0;

    /*
     * NUL-terminate the text. 
     */
    buffer[bytes_read] = '\0';

    /*
     * Locate the line that starts with "cpu MHz". 
     */
    match = strstr( buffer, "cpu MHz" );
    if ( match == NULL )
        return 0;

    /*
     * Parse the line to extract the clock speed. 
     */
    sscanf( match, "cpu MHz : %f", &clock_speed );
    return clock_speed;
}                               // cpu_clock_speed


float get_cpu_clock_speed( void ) {
    static float speed = 0;
    if ( speed == 0 )
        speed = cpu_clock_speed(  ) * 1000;
    return speed;
}                               // get_cpu_clock_speed
