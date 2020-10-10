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

static void display( char const *mark, char *fmt, ... ) {
    if ( now0 == 0 )
        now0 = time( NULL );
    va_list ap;
    char msg[500];
    va_start( ap, fmt );
    vsprintf( msg, fmt, ap );
    va_end( ap );

    printf( "%3ld: %s: %s", time( NULL ) - now0, mark, msg );
    fflush( stdout );
}                               // display

int exec_server_client( int argc, char *argv[], 
	int (*pf_server)(int argc, char *argv[]), int (*pf_client)(int argc, char *argv[]) ) {
    pid_t pid_client = fork();
    if (pid_client == 0) {
		return pf_client( argc, argv );
    }
    else {
		int s = pf_server( argc, argv );
        int wstatus;
        waitpid(pid_client, &wstatus, 0);
        return s;
    }
}								// exec_server_client

int exec_server_client2( int argc, char *argv[], 
	int (*pf_server)(int argc, char *argv[]), 
	int (*pf_client1)(int argc, char *argv[]),
	int (*pf_client2)(int argc, char *argv[]) ) {
    pid_t pid_client1 = fork();
    if (pid_client1 == 0) {
		int s = pf_server( argc, argv );
        int wstatus;
        waitpid(pid_client1, &wstatus, 0);
        return s;
    }
    else {
        pid_t pid_client2 = fork();
        if (pid_client2 == 0) {
    		int s = pf_client1( argc, argv );
            return s;
        }
        else {
    		int s = pf_client2( argc, argv );
            int wstatus;
            waitpid(pid_client2, &wstatus, 0);
            return s;
        }
    }
}								// exec_server_client2

#endif                         /*EXAMPLE_UTILS_H_ */
