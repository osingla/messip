/**
 * @file messip.h
 *
 * MessIP : Message Passing over TCP/IP
 * Copyright (C) 2001-2007  Olivier Singla
 * http://messip.sourceforge.net/
 *
 **/

#ifndef MESSIP_H_
#define MESSIP_H_

#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdint.h>
#include <signal.h>
#include <endian.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/select.h>

#define ERRNO errno
#define SOCKET_ERROR -1

#define Sleep(X) sleep(X/1000)

#define closesocket close

#define O_BINARY 0

typedef int SOCKET;

#define PUBLIC

#ifndef __MESSIP__

#define __MESSIP__

#define	MESSIP_DEFAULT_PORT	9200

#define VERSION_MAJOR	0
#define	VERSION_MINOR	9

#define MESSIP_OK	1
#define MESSIP_NOK	0

#define MESSIP_TRUE		-1
#define MESSIP_FALSE	0

#define	MESSIP_MAXLEN_ID 8
#define	MESSIP_CHANNEL_NAME_MAXLEN	47

typedef char messip_id_t[MESSIP_MAXLEN_ID + 1];

/*
 * This is a linked list, as we can have connexion to mutiple servers
 */
typedef struct messip_cnx {
    char path[80];              // "/' if only server
    SOCKET sockfd;
    messip_id_t remote_id;
    struct messip_cnx *prev;
} messip_cnx_t;


typedef struct messip_channel_t {
    char name[MESSIP_CHANNEL_NAME_MAXLEN + 1];
    messip_cnx_t *cnx;
    int32_t f_already_connected;
    messip_id_t remote_id;
    int32_t recv_sockfd_sz;
    SOCKET recv_sockfd[FD_SETSIZE];
    int remote_port;
    in_port_t sin_port;
    in_addr_t sin_addr;
    char sin_addr_str[48];
    SOCKET send_sockfd;
    SOCKET *new_sockfd;
    int32_t new_sockfd_sz;
    int32_t nb_replies_pending; // Nb of receive() without reply()
    int32_t datalen;            // Length of data transmitted
    int32_t datalenr;           // Length of data read
    void **receive_allmsg;      // Dynamic buffer, if receive buffer was to small
    int *receive_allmsg_sz;     // Size allocated for these Dynamic buffer
    int nb_timers;
    int mgr_sockfd;             // Socket in the messip_mgr
} messip_channel_t;

#  define MESSIP_MSG_DISCONNECT		-2
#  define MESSIP_MSG_DISMISSED		-3
#  define MESSIP_MSG_TIMEOUT			-4
#  define MESSIP_MSG_TIMER			-5
#  define MESSIP_MSG_NOREPLY			-6
#  define MESSIP_MSG_DEATH_PROCESS	-7


// -----------------------
// Prototypes of functions
// -----------------------

#  define MESSIP_NOTIMEOUT		-1

#  ifdef __cplusplus
extern "C" {
#  endif

    void messip_init( void );

    messip_cnx_t *messip_connect( char *mgr_ref, messip_id_t const id, int msec_timeout );

    messip_channel_t *messip_channel_create( messip_cnx_t * cnx,
       const char *name, int msec_timeout, int32_t maxnb_msg_buffered );

    int messip_channel_delete( messip_channel_t * ch, int msec_timeout );

    messip_channel_t *messip_channel_connect( messip_cnx_t * cnx, const char *name, int msec_timeout );

    int messip_channel_disconnect( messip_channel_t * ch, int msec_timeout );

    int messip_channel_ping( messip_channel_t * ch, int msec_timeout );

    int messip_receive( messip_channel_t * ch, int32_t *type, void *buffer, int maxlen, int msec_timeout );

    int messip_reply( messip_channel_t * ch, int index, int32_t answer, void *reply_buffer, int reply_len, int msec_timeout );

    int messip_send( messip_channel_t * ch,
       int32_t type,
       void *send_buffer, int send_len, int32_t *answer, void *reply_buffer, int reply_maxlen, int msec_timeout );

    int32_t messip_buffered_send( messip_channel_t * ch, int32_t type, void *send_buffer, int send_len, int msec_timeout );

    timer_t messip_timer_create( messip_channel_t * ch, int32_t type, int msec_1st_shot, int msec_rep_shot, int msec_timeout );

    int messip_timer_delete( messip_channel_t * ch, timer_t timer_id );

    int messip_death_notify( messip_cnx_t * cnx, int msec_timeout, int status );

    void messip_set_log_level( unsigned level );

    unsigned messip_get_log_level( void );

    int messip_log( unsigned level, const char *fmt, ... );

#  define MESSIP_LOG_ERROR          0x01
#  define MESSIP_LOG_WARNING        0x02
#  define MESSIP_LOG_INFO           0x04
#  define MESSIP_LOG_INFO_VERBOSE   0x08
#  define MESSIP_LOG_DEBUG          0x10
    PUBLIC void messip_set_log_level( unsigned level );

    PUBLIC unsigned messip_get_log_level(  );

#  ifdef __cplusplus
}
#  endif
#endif

#endif /*MESSIP_H_*/
