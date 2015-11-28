/**
 * @file example_utils.h
 *
 * MessIP : Message Passing over TCP/IP
 * Copyright (C) 2001-2007  Olivier Singla
 * http://messip.sourceforge.net/
 *
 **/

#ifndef EXAMPLE_UTILS_H_

#define EXAMPLE_UTILS_H_

#include <stdarg.h>
#include <time.h>

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

static inline void delay( int msec ) {
    usleep( msec * 1000 );
}

static void cancel( char *fmt, ... ) {
    va_list ap;
    va_start( ap, fmt );
    vprintf( fmt, ap );
    fflush( stdout );
    va_end( ap );
    exit( -1 );
}                               // cancel

static void display( char *fmt, ... ) {
    if ( now0 == 0 )
        now0 = time( NULL );
    va_list ap;
    char msg[500];
    va_start( ap, fmt );
    vsprintf( msg, fmt, ap );
    va_end( ap );

    printf( "%3ld: %s", time( NULL ) - now0, msg );
    fflush( stdout );
}                               // display

int exec_server_client( int argc, char *argv[], 
	int (*pf_server)(int argc, char *argv[]), int (*pf_client)(int argc, char *argv[]) ) {
	if ( (argc > 1) && !strcmp(argv[1], "server") )
		return pf_server( argc, argv );
	else if ( (argc > 1) && !strcmp(argv[1], "client") )
		return pf_client( argc, argv );
	else {
		printf( "%s server|client\n", argv[0] );
		fflush( stdout );
	}
	return -1;
}								// exec_server_client

int exec_server_client2( int argc, char *argv[], 
	int (*pf_server)(int argc, char *argv[]), 
	int (*pf_client1)(int argc, char *argv[]),
	int (*pf_client2)(int argc, char *argv[]) ) {
	if ( (argc > 1) && !strcmp(argv[1], "server") )
		return pf_server( argc, argv );
	else if ( (argc > 1) && !strcmp(argv[1], "client1") )
		return pf_client1( argc, argv );
	else if ( (argc > 1) && !strcmp(argv[1], "client2") )
		return pf_client2( argc, argv );
	else {
		printf( "%s server|client1|client2\n", argv[0] );
		fflush( stdout );
	}
	return -1;
}								// exec_server_client2

#endif                         /*EXAMPLE_UTILS_H_ */
