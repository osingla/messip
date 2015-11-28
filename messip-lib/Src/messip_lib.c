/**
 * @file messip_lib.c
 *
 * MessIP : Message Passing over TCP/IP
 * Copyright (C) 2001-2007  Olivier Singla
 * http://messip.sourceforge.net/
 *
 * This is the library that encapsulate the communication with the
 * manager (messip_mgr). The library also implements send/receive/reply
 * because the manager is *not* then involved  (unless it's a buffered
 * message).
 **/

/**
 * @mainpage MESSIP: Message-Passing over TCP/IP 
 * 
 * MESSIP implements Message Passing over TCP/IP, using BSD socket interface programming.
 * Three kinds of messages can be sent between Processes or Threads:
 *  - Synchronous Messages (blocking messages), from 0 to 64k in size,
 *  - Asynchronous Messages (non blocking, buffered messages), from 0 to 64k in size,
 *  - Timer messages (one shot and repetitive)
 * 
 * Message Passing consist in the exchange of bytes from one task (Process or Thread) to another (Process or Thread), 
 * whatever if these tasks are on the same node or on another node over a TCP/IP network.
 * 
 * The task sending the message is known as the client and the task receiving the message is known as the server. 
 * In theory, clients and servers do not need a third agent to communicate between themselves, at least for Synchronous Messages 
 * (e.g. messages that do not require to be buffered). In practice, to initiate the communication between clients and servers, 
 * it’s necessary to have a third task, which is called ‘messip-mgr’ (the messip manager). Only one ‘messip_mgr’ is necessary 
 * on a given network. Further versions of messip will enable to have several redundant managers.
 * 
 * A library (shared) provides the messip API functions that enable to talk to either the messip manager or to the other tasks 
 * (either clients or servers).
 * 
 * Each process which wants to use Message Passing must first connect to the messip manager, using the next API function: 
 * messip_connect. If no IP address and no port is specified (preferred method), then a text file /etc/messip is used. 
 * If this file does not exist, then the default IP name ‘localhost’ is used, and the default port 9000 is used.
 * 
 * In order to receive messages, a server must first create a channel (see API function: messip_channel_create). 
 * A channel is identified by a name, which must be unique over the whole network. It’s perfectly fine to create several channels 
 * for a given process, but because you can receive messages on one specific channel and this operation is (usually) blocking 
 * (i.e. you’ll usually wait until you get a message over this channel), in practice a thread will create and manage only 
 * a channel at a time. In the future, it will be possible to receive messages on several channels at the same time. Note that you could poll, 
 * but it’s often not very efficient.
 * 
 * Prior to send any message to a server (whatever it is a Synchronous or an Asynchronous Message), a client must find the channel. 
 * That means that the server must know the name that identifies the channel. In order to be able to further communicate with the server, 
 * that’s the only information that the client must know about the server. The API function to be used is: messip_channel_connect.
 * 
 * Both API functions messip_channel_create and messip_channel_connect involve communication with the messip manager (messip-mgr).
 * 
 * A server typically waits for incoming messages, using the next API function: messip_receive. This is normally a blocking function 
 * (but you can specify a timeout value), that means that you exit from this function on these events:
 *  - a message has been received (either Synchronous or Asynchronous): each Synchronous message has to be replied, unless 
 *    you want to keep the client blocked and waiting for an answer.
 *  - timeout has occurred
 *  - a timer has expired
 *  - a client which was connected has either died or closed the connection to the channel.
 * 
 * A client sends either Synchronous or Asynchronous Messages:
 *  - a Synchronous Message is blocking: that means the client will wait until the message is received by the server and processed. 
 *    The server has to reply to the client in order for the client to be unblocked. Note that you can specify a timeout. 
 *    Synchronous Messaging do not involve the Messip manager (messip_server agent), only the server and the client talk together. 
 *    Sending a Synchronous message is performed with the next API function: messip_send
 *  - an Asynchronous Message is non blocking. That means that a client can’t ignore the state of the server. 
 *    Because of this, there is no reply from the server. Therefore, you can’t assume that the message has been received by the server. 
 *    When the server has created the channel (where he receive messages), he has specified a parameter which is the maximum number 
 *    of messages that can be buffered. Asynchronous Message are slower than Synchronous Message because they are copied twice: 
 *    from the client to the Messip manager, then from the Messip manager to the server.
 * 
 * @see messip_disconnect(), messip_channel_create(), messip_channel_disconnect()
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>

#include "messip.h"
#include "messip_private.h"

#include "messip_utils.h"

#if !defined(TIMER_USE_SIGEV_THREAD) || !defined(TIMER_USE_SIGEV_SIGNAL)
#error Both TIMER_USE_SIGEV_THREAD and TIMER_USE_SIGEV_SIGNAL must be defined !
#endif

#if (TIMER_USE_SIGEV_THREAD!=0) && (TIMER_USE_SIGEV_THREAD!=1)
#error TIMER_USE_SIGEV_THREAD must be either 0 or 1 !
#endif

#if (TIMER_USE_SIGEV_SIGNAL!=0) && (TIMER_USE_SIGEV_SIGNAL!=1)
#error TIMER_USE_SIGEV_SIGNAL must be either 0 or 1 !
#endif

#if (TIMER_USE_SIGEV_THREAD==TIMER_USE_SIGEV_SIGNAL)
#error Either TIMER_USE_SIGEV_THREAD and TIMER_USE_SIGEV_SIGNAL must be set !
#endif

typedef struct list_connect_s {
    char name[MESSIP_CHANNEL_NAME_MAXLEN + 1];	///< TBD
    messip_channel_t *info;						///< TBD
} list_connect_t;
static list_connect_t *list_connect;	///< TBD
static int nb_list_connect;				///< TBD

static unsigned log_level = MESSIP_LOG_ERROR | MESSIP_LOG_WARNING;	///< TBD


/**
 * The messip_init() initialize the library, and must be called prior to any call of other functions 
 * of he library (likely messip_connect).
 * 
 * @note This function must be called only once per process, namely from the main thread.
 * 
 * @see messip_disconnect(), messip_channel_create(), messip_channel_disconnect()
 */
void messip_init( void ) { 
    list_connect = NULL ;
    nb_list_connect = 0;
}                               // messip_init

/**
 * Write data from multiple buffers over a TCP socket 
 * 
 * The main difference with the library function is that this function
 * handles EINTR (signal received while receiving data).
 * 
 * @param sockfd Socket file descriptor
 * @param iov pointer vector points to a struct iovec defining address and bytes 
 * @param iovcnt Number of blocks described into vector  iov
 * @return
 *  - On success, returns the number of bytes written
 *  - On error, -1 is returned, and errno is set appropriately
 */
int messip_writev( SOCKET sockfd, const struct iovec *iov, int iovcnt ) {
    int dcount;
    for ( int cnt = 0; ; ) {
        dcount = writev( sockfd, iov, iovcnt );
        if ( ( dcount == -1 ) && ( errno == EINTR ) )
            continue;
        if ( dcount == -1 ) {
            printf( "%s %d: dcount=%d errno=%d %d\015\012", __FILE__, __LINE__, dcount, errno, cnt++ );
            fflush( stdout );
        }
        if ( errno == EPIPE )
            return dcount;
        assert( dcount != -1 );
        break;
    }                           // for (;;)
    return dcount;
}                               // messip_writev

/**
 *  Read data into multiple buffers from a TCP socket 
 * 
 *  The main difference with the library function is that this function
 * 	handles EINTR (signal received while receiving data).
 * 
 * @param sockfd Socket file descriptor
 * @param iov pointer vector points to a struct iovec defining address and bytes 
 * @param iovcnt Number of blocks described into vector  iov
 * @return
 *  - On success, returns the number of bytes read
 *  - On error, -1 is returned, and errno is set appropriately
 */
int messip_readv( SOCKET sockfd, const struct iovec *iov, int iovcnt ) {
    int dcount;
    for ( int cnt = 0; ; ) {
        dcount = readv( sockfd, iov, iovcnt );
        if ( ( dcount == -1 ) && ( errno == EINTR ) )
            continue;
        if ( ( dcount == -1 ) && ( errno == ECONNRESET ) )
            return -1;
        if ( dcount == -1 ) {
            printf( "%s %d: dcount=%d errno=%d %d\015\012", __FILE__, __LINE__, dcount, errno, cnt++ );
            fflush( stdout );
        }
        assert( dcount != -1 );
        break;
    }                           // for (;;)
    return dcount;
}                               // messip_readv

/**
 *  TBD
 * 
 *  @param fd TBD
 *  @param readfds TBD
 *  @param writefds TBD
 *  @param exceptfds TBD
 *  @param timeout TBD
 *  @return TBD
 */
int messip_select( int fd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout ) {
    int status;
    do {
        fd_set ready;
        status = select( fd, NULL, &ready, NULL, NULL );
        if ( ( status < 0 ) && ( status != EINTR ) )
            return status;
    } while ( status < 0 );
    return status;
}                               // messip_select

/**
 * Connection to the messip manager (messip_mgr) 
 * 
 * @param mgr_ref mgr_ref can be
 * - NULL: in this case:
 *     - Either the file /etc/messip exists an contains the IP address or host;
 *     - Or specify a host or IP address to designate the node where the messip manager is running. 
 * - Or if the file /etc/messip does not exist, “localhost” is then used.
 * @param id task identifier, which is used only for information. This is a string of up to 8 characters.
 * @param msec_timeout  if not 0, is a timeout (expressed in milliseconds) where the function exits if connection 
 *		with the messip manager fails.
 * @return  A pointer to a messip_cnx_t structure (that will be used next on a channel creation or connection),
 *	or -1 if an error occurred (errno is set).
 * - ETIMEDOUT Occurs if the messip manager did not answer the connection request within the provided time.
 * 
 * @note From the same thread, only one connection to the messip manager will be allowed.
 * 
 * @see messip_disconnect(), messip_channel_create(), messip_channel_disconnect()
 */
messip_cnx_t *messip_connect( char *mgr_ref, messip_id_t id, int msec_timeout ) {

    /*--- NULL and /etc/messip does not exist ? ---*/
    int port = MESSIP_DEFAULT_PORT;
    char hostname[64];
    if ( !mgr_ref && ( access( "/etc/messip", F_OK ) == -1 ) ) {
        strcpy( hostname, "localhost" );
    }
    else if ( !mgr_ref ) {
        read_etc_messip( hostname, &port, NULL );
    }
    else {
        strcpy( hostname, mgr_ref );
    }

    /*--- Connect socket using name specified ---*/
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    struct hostent *hp = gethostbyname( hostname );
    if ( hp == NULL ) {
        printf( "*** %s : unknown host!***\015\012", hostname );
        fflush( stdout );
        return NULL;
    }

    /*--- Allocate a connexion structure ---*/
    messip_cnx_t *cnx = ( messip_cnx_t * ) malloc( sizeof( messip_cnx_t ) );
    if ( cnx == NULL )
        return NULL;
    memset( cnx, 0, sizeof( messip_cnx_t ) );

    /*--- Create socket ---*/
    cnx->sockfd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( cnx->sockfd < 0 ) {
        printf( "*** Unable to open a socket! ***\015\012" );
        fflush( stdout );
        free( cnx );
        return NULL;
    }
    fcntl( cnx->sockfd, F_SETFL, FD_CLOEXEC );
//  logg( NULL, "%s: sockfd=%d\n", __FUNCTION__, cnx->sockfd );

    /*
     * Change to non-blocking mode to enable timeout with connect() 
     */
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        if ( fcntl( cnx->sockfd, F_SETFL, fcntl( cnx->sockfd, F_GETFL, 0 ) | O_NONBLOCK ) == -1 ) {
            printf( "Error: fcntl() !\015\012" );
            fflush( stdout );
            closesocket( cnx->sockfd );
            free( cnx );
            return NULL;
        }
    }                           // if

    /*--- Connect to the port ---*/
    memcpy( &server.sin_addr, hp->h_addr, hp->h_length );
    server.sin_port = htons( port );
    int status = connect( cnx->sockfd, ( const struct sockaddr * ) &server, sizeof( server ) );
    if ( ( msec_timeout != MESSIP_NOTIMEOUT ) && ( status ) ) {
        if ( errno == EINPROGRESS ) {
            fd_set ready;
            FD_ZERO( &ready );
            FD_SET( cnx->sockfd, &ready );
            struct timeval tv;
            tv.tv_sec = msec_timeout / 1000;
            tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
            status = select( ( int ) cnx->sockfd + 1, NULL, &ready, NULL, &tv );    // <&>
            assert( status != -1 );
            if ( !FD_ISSET( cnx->sockfd, &ready ) ) {
                closesocket( cnx->sockfd );
                free( cnx );
                errno = ETIMEDOUT;
                return NULL;
            }
        }
        else
        {
            printf( "%s %d:\015\012\tUnable to connect to host %s, port %d\015\012", __FILE__, __LINE__, hostname, port );
            fflush( stdout );
            closesocket( cnx->sockfd );
            free( cnx );
            return NULL;
        }
    }
    else if ( status ) {        // if
        printf( "%s %d:\015\012\tUnable to connect to host %s, port %d\015\012", __FILE__, __LINE__, hostname, port );
        fflush( stdout );
        closesocket( cnx->sockfd );
        free( cnx );
        return NULL;
    }

    /*--- Ready to write ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        fd_set ready;
        FD_ZERO( &ready );
        FD_SET( cnx->sockfd, &ready );
        struct timeval tv;
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) cnx->sockfd + 1, NULL, &ready, NULL, &tv );    // <&>
        assert( status != -1 );
        if ( !FD_ISSET( cnx->sockfd, &ready ) ) {
            closesocket( cnx->sockfd );
            free( cnx );
            errno = ETIMEDOUT;
            return NULL;
        }
    }                           // if

    /*--- Send a message to the server ---*/
    messip_send_connect_t msgsend;
    int32_t op;
    struct iovec iovec[2];
    iovec[0].iov_base = &op;
    iovec[0].iov_len = sizeof( int32_t );
    op = MESSIP_OP_CONNECT;
    IDCPY( msgsend.id, id );
    iovec[1].iov_base = &msgsend;
    iovec[1].iov_len = sizeof( msgsend );
    status = messip_writev( cnx->sockfd, iovec, 2 );
    assert( ( status == sizeof( int32_t ) + sizeof( msgsend ) ) );

    /*--- Ready to read ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        fd_set ready;
        FD_ZERO( &ready );
        FD_SET( cnx->sockfd, &ready );
        struct timeval tv;
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) cnx->sockfd, &ready, NULL, NULL, &tv );    // <&>
        assert( status != -1 );
        if ( !FD_ISSET( cnx->sockfd, &ready ) ) {
            closesocket( cnx->sockfd );
            free( cnx );
            errno = ETIMEDOUT;
            return NULL;
        }
    }                           // if

    /*--- Now wait for an answer from the server ---*/
    messip_reply_connect_t reply;
    iovec[0].iov_base = &reply;
    iovec[0].iov_len = sizeof( reply );
    status = messip_readv( cnx->sockfd, iovec, 1 );
    assert( status == sizeof( messip_reply_connect_t ) );
    assert( reply.ok == MESSIP_OK );
    IDCPY( cnx->remote_id, id );

    // Ok
    return cnx;
}                               // messip_connect

/**
 * Delete the connection with the messip manager (messip_mgr) 
 * 
 * @param cnx is the connection (to the messip manager) structure which was returned by messip_connect() 
 * @param msec_timeout  if not 0, is a timeout (expressed in milliseconds) where the function exits if connection 
 *		with the messip manager fails.
 * @return  0 ir no error, or -1 if an error occurred (errno is set).
 * - ETIMEDOUT Occurs if the messip manager did not answer the connection request within the provided time.
 * 
 * @todo Function not yet implemented
 */
int messip_disconnect( messip_cnx_t *cnx, int msec_timeout ) { 

	// TODO
	
    // Ok
    return 0;
}                               // messip_connect

/**
 * Enables a server to create a channel. In order for a server to receive messages (see messip_receive), 
 * a server must first create a channel. A client who wants to send a message to this server will first 
 * connect to this channel (using messip_channel_connect), then will send the message on it (using messip_send). 
 * A channel is identified by a name, which must be unique.
 * Note that messip_channel_create must be called only by a server (receiving messages), 
 * but not by a client (sending messages).
 * 
 * @param cnx is the connection (to the messip manager) structure which was returned by messip_connect() 
 * @param name is a name that identify the channel. This name must be unique.
 * @param msec_timeout if not 0, is a timeout (expressed in milliseconds)  where the function exits if 
 * 		connection with the messip manager fails.
 * @param maxnb_msg_buffered specify the maximum number of Asynchronous messages that can be buffered 
 * 		by the messip manager. It’s valid to specify a value of 0: in this case, sending an Asynchronous message 
 * 		on this channel will not be allowed, and therefore will fail.
 * @return A pointer to a messip_channel_t structure (that will be used next when waiting messages from this channel), 
 * 	or -1 if an error occurred (errno is then set):
 *    - ETIMEDOUT : Occurs if the messip manager did not answered the connection request within the expressed time.
 * 
 * @note Names of the channel must be unique, across the whole network.
 * 
 * @see messip_channel_delete(), messip_channel_connect(), messip_receive()
 */
messip_channel_t *messip_channel_create( messip_cnx_t *cnx, const char *name, int msec_timeout, int32_t maxnb_msg_buffered ) {
    int status;
    ssize_t dcount;
    struct sockaddr_in server_addr;
    struct sockaddr_in sock_name;
    socklen_t sock_namelen;
    messip_channel_t *ch;
    fd_set ready;
    struct timeval tv;
    int k;
    int32_t op;
    messip_send_channel_create_t msgsend;
    messip_reply_channel_create_t reply;
    struct iovec iovec[3];

    /*--- Create socket ---*/
    SOCKET sockfd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( sockfd < 0 ) {
        printf( "Unable to open a socket!\015\012" );
        fflush( stdout );
        return NULL;
    }
    fcntl( sockfd, F_SETFL, FD_CLOEXEC );
//  logg( NULL, "%s:, sockfd=%d\n", __FUNCTION__, sockfd );

    /*--- Bind the socket ---*/
    memset( &server_addr, 0, sizeof( server_addr ) );
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl( INADDR_ANY );
    server_addr.sin_port = htons( 0 );
    status = bind( sockfd, ( struct sockaddr * ) &server_addr, sizeof( struct sockaddr_in ) );
    if ( status < 0 ) {
        printf( "Unable to bind - status=%d\015\012", status );
        fflush( stdout );
        closesocket( sockfd );
        return NULL;
    }

    sock_namelen = sizeof( struct sockaddr_in );
    getsockname( sockfd, ( struct sockaddr * ) &sock_name, &sock_namelen );
    server_addr.sin_port = ntohs( sock_name.sin_port );

    listen( sockfd, 8 );

    /*--- Ready to write ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( cnx->sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) cnx->sockfd + 1, NULL, &ready, NULL, &tv );    // <&>
        assert( status != -1 );
        if ( !FD_ISSET( cnx->sockfd, &ready ) ) {
            closesocket( sockfd );
            errno = ETIMEDOUT;
            return NULL;
        }
    }                           // if

    /*--- Send a message to the server ---*/
    op = MESSIP_OP_CHANNEL_CREATE;
    iovec[0].iov_base = &op;
    iovec[0].iov_len = sizeof( int32_t );
    IDCPY( msgsend.id, cnx->remote_id );
    msgsend.maxnb_msg_buffered = maxnb_msg_buffered;
    strncpy( msgsend.channel_name, name, MESSIP_CHANNEL_NAME_MAXLEN );
    msgsend.channel_name[MESSIP_CHANNEL_NAME_MAXLEN] = 0;
    msgsend.sin_port = server_addr.sin_port;
    strcpy( msgsend.sin_addr_str, inet_ntoa( server_addr.sin_addr ) );
    iovec[1].iov_base = &msgsend;
    iovec[1].iov_len = sizeof( msgsend );
    status = messip_writev( cnx->sockfd, iovec, 2 );
    messip_log( MESSIP_LOG_INFO, "channel_create: send status= %d remote_socket=%d  port=%d\n",
       status, sockfd, server_addr.sin_port );
    assert( ( status == sizeof( int32_t ) + sizeof( messip_send_channel_create_t ) ) );

    /*--- Ready to read ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( cnx->sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) cnx->sockfd + 1, &ready, NULL, NULL, &tv );    // <&>
        assert( status != -1 );
        if ( !FD_ISSET( cnx->sockfd, &ready ) ) {
            closesocket( sockfd );
            errno = ETIMEDOUT;
            return NULL;
        }
    }                           // if

    /*--- Now wait for an answer from the server ---*/
    iovec[0].iov_base = &reply;
    iovec[0].iov_len = sizeof( reply );
    dcount = messip_readv( cnx->sockfd, iovec, 1 );
    messip_log( MESSIP_LOG_INFO, "channel_create: reply status= %d \n", status );
    assert( dcount == sizeof( messip_reply_channel_create_t ) );

    /*--- Channel creation failed ? ---*/
    if ( reply.ok == MESSIP_NOK ) {
        closesocket( sockfd );
        return NULL;
    }

    /*--- Ok ---*/
    ch = ( messip_channel_t * ) malloc( sizeof( messip_channel_t ) );
    strcpy( ch->name, name );
    ch->cnx = cnx;
    IDCPY( ch->remote_id, cnx->remote_id );
    ch->sin_port = reply.sin_port;
    ch->sin_addr = reply.sin_addr;
    strcpy( ch->sin_addr_str, reply.sin_addr_str );
    ch->recv_sockfd_sz = 0;
    ch->recv_sockfd[ch->recv_sockfd_sz++] = sockfd;
    ch->send_sockfd = -1;
    ch->nb_replies_pending = 0;
    ch->new_sockfd_sz = 1;
    ch->new_sockfd = ( SOCKET * ) malloc( sizeof( int ) * ch->new_sockfd_sz );
    ch->receive_allmsg = ( void ** ) malloc( sizeof( void ** ) * ch->new_sockfd_sz );
    ch->receive_allmsg_sz = ( int * ) malloc( sizeof( int * ) * ch->new_sockfd_sz );
    for ( k = 0; k < ch->new_sockfd_sz; k++ ) {
        ch->new_sockfd[k] = -1;
        ch->receive_allmsg[k] = NULL;
        ch->receive_allmsg_sz[k] = 0;
    }

    return ch;
}                               // messip_channel_create

/**
 * Enables a server to delete a channel that was previously created by messip_channel_create()
 * 
 * @param ch Channel connection (to the server) structure which was returned by messip_channel_connect() 
 * @param msec_timeout if not 0, is a timeout (expressed in milliseconds) where the function exits if connection 
 *     with the messip manager fails.
 * 
 * @return 
 * 	- If the operation succeed, the value 0 is returned.
 *	- If the operation timeout, the value –1 is returned and then errno is set:
 *      - ETIMEDOUT Occurs if the messip manager did not answered the channel deletion request within the expressed time
 *	- If the deletion can’t occur because of an active connection, the number of clients currently 
 *    connected to this channel is then return.
 *
 * @note Only channels with no active connection on it (see messip_channel_connect) are allowed to be deleted.
 * 
 * @see messip_channel_create(), messip_channel_connect(),messip_receive()
 */
int messip_channel_delete( messip_channel_t *ch, int msec_timeout ) {
    int status;
    ssize_t dcount;
    fd_set ready;
    struct timeval tv;
    int32_t op;
    messip_send_channel_delete_t msgsend;
    messip_reply_channel_delete_t reply;
    struct iovec iovec[2];

    /*--- Ready to write ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( ch->cnx->sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) ch->cnx->sockfd + 1, NULL, &ready, NULL, &tv );    // <&>
        assert( status != -1 );
        if ( !FD_ISSET( ch->cnx->sockfd, &ready ) ) {
            errno = ETIMEDOUT;
            return -1;
        }
    }                           // if

    /*--- Send a message to the server ---*/
    op = MESSIP_OP_CHANNEL_DELETE;
    iovec[0].iov_base = &op;
    iovec[0].iov_len = sizeof( int32_t );
    IDCPY( msgsend.id, ch->remote_id );
    strcpy( msgsend.name, ch->name );
    iovec[1].iov_base = &msgsend;
    iovec[1].iov_len = sizeof( msgsend );
    status = messip_writev( ch->cnx->sockfd, iovec, 2 );
    messip_log( MESSIP_LOG_INFO, "channel_delete: send status= %d\n", status );
    assert( ( status == sizeof( int32_t ) + sizeof( messip_send_channel_delete_t ) ) );

    /*--- Ready to read ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( ch->cnx->sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) ch->cnx->sockfd + 1, &ready, NULL, NULL, &tv );    // <&>
        assert( status != -1 );
        if ( !FD_ISSET( ch->cnx->sockfd, &ready ) ) {
            errno = ETIMEDOUT;
            return -1;
        }
    }                           // if

    /*--- Now wait for an answer from the server ---*/
    iovec[0].iov_base = &reply;
    iovec[0].iov_len = sizeof( reply );
    dcount = messip_readv( ch->cnx->sockfd, iovec, 1 );
    messip_log( MESSIP_LOG_INFO, "channel_delete: reply status= %d \n", dcount );
    assert( dcount == sizeof( messip_reply_channel_delete_t ) );

    /*--- Channel creation failed ? ---*/
    return reply.nb_clients;
}                               // messip_channel_delete

/**
 * Enables a client to connect to a channel owned by a server. Prior to send any message to a server,
 * this operation must be performed by a client.  
 * 
 * messip_channel_connect must be called only by a client (sending messages), 
 * but not by a server (receiving messages). It’s perfectly valid to have the same process 
 * acting both as server and client: in this case, there will be a thread for the server, 
 * and another thread for the client.
 * 
 * @param cnx connection (to the messip manager) structure which was returned by messip_connect() 
 * @param name name that identify the channel you want to connect to. This name is unique.
 * @param msec_timeout if not 0, is a timeout (expressed in milliseconds) where the function exits 
 * 		if connection with the messip manager fails.
 *  
 * @return A pointer to a messip_channel_t structure (that will be used next when sending messages on this channel), 
 *   or -1 if an error occurred (errno is then set):
 *      - ETIMEDOUT occurs if either the messip manage and the server owning the channel 
 *        did not answered the connection request within the expressed time.
 * 
 * @note
 *   Only one active connection to the same channel can be performed by a given thread.
 * 
 * @see messip_channel_create(), messip_channel_disconnect(), messip_receive(), messip_send()
 */
messip_channel_t *messip_channel_connect( messip_cnx_t *cnx, const char *name, int msec_timeout ) {
    int status;
    messip_channel_t *info;
    fd_set ready;
    struct timeval tv;
    messip_datasend_t datasend;
    ssize_t dcount;
    int32_t op;
    messip_send_channel_connect_t msgsend;
    messip_reply_channel_connect_t msgreply;
    struct iovec iovec[2];

    /*--- Ready to write ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( cnx->sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) FD_SETSIZE, NULL, &ready, NULL, &tv );
        assert( status != -1 );
        if ( !FD_ISSET( cnx->sockfd, &ready ) ) {
            errno = ETIMEDOUT;
            return NULL;
        }
    }                           // if

    /*--- Send a message to the messip manager ---*/
    op = MESSIP_OP_CHANNEL_CONNECT;
    iovec[0].iov_base = &op;
    iovec[0].iov_len = sizeof( int32_t );
    IDCPY( msgsend.id, cnx->remote_id );
    strncpy( msgsend.name, name, MESSIP_CHANNEL_NAME_MAXLEN );
    iovec[1].iov_base = &msgsend;
    iovec[1].iov_len = sizeof( msgsend );
    status = messip_writev( cnx->sockfd, iovec, 2 );
    messip_log( MESSIP_LOG_INFO, "channel_connect: send status= %d  sockfd=%d\n", status, cnx->sockfd );
    if ( ( status != sizeof( int32_t ) + sizeof( messip_send_channel_connect_t ) ) ) {
        printf( ">> %s %5d: status=%d \n", __FILE__, __LINE__, status );
        fflush( stdout );
    }
    assert( ( status == sizeof( int32_t ) + sizeof( messip_send_channel_connect_t ) ) );

    /*--- Ready to read ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( cnx->sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) FD_SETSIZE, &ready, NULL, NULL, &tv );
        assert( status != -1 );
        if ( !FD_ISSET( cnx->sockfd, &ready ) ) {
            errno = ETIMEDOUT;
            return NULL;
        }
    }                           // if

    /*--- Now wait for an answer from the messip manager ---*/
    for ( ;; ) {
        iovec[0].iov_base = &msgreply;
        iovec[0].iov_len = sizeof( msgreply );
        dcount = messip_readv( cnx->sockfd, iovec, 1 );
        if ( ( dcount == -1 ) && ( errno == EINTR ) )
            continue;
        break;
    }                           // for (;;)
    messip_log( MESSIP_LOG_INFO, "channel_connect: reply dcount=%d already_connected=%d\n", dcount,
       msgreply.f_already_connected );
    if ( dcount != sizeof( messip_reply_channel_connect_t ) ) {
        printf( "dcount=%d errno=%d\n", dcount, errno );
        fflush( stdout );
    }
    assert( dcount == sizeof( messip_reply_channel_connect_t ) );

    /*--- Locate channel has failed ? ---*/
    if ( msgreply.ok == MESSIP_NOK ) {
        return NULL;
    }

    /*--- Use an existant connection or create a new one ---*/
    if ( msgreply.f_already_connected ) {
        int n, found;

        for ( found = 0, n = 0; n < nb_list_connect; n++ ) {
            if ( !strcmp( list_connect[n].name, name ) ) {
                found = 1;
                break;
            }
        }
        assert( found == 1 );
        info = list_connect[n].info;
        info->f_already_connected = 1;

    }
    else {

        /*--- Ok ---*/
        info = ( messip_channel_t * ) malloc( sizeof( messip_channel_t ) );
        info->f_already_connected = 0;
        info->cnx = cnx;
        IDCPY( info->remote_id, msgreply.id );
        info->sin_port = msgreply.sin_port;
        info->sin_addr = msgreply.sin_addr;
        strcpy( info->sin_addr_str, msgreply.sin_addr_str );
        strcpy( info->name, name );
        info->mgr_sockfd = msgreply.mgr_sockfd;

        /*--- Create socket ---*/
        info->send_sockfd = socket( AF_INET, SOCK_STREAM, 0 );
//      logg( NULL, "%s: send_sockfd = %d \n", __FUNCTION__, info->send_sockfd );
        if ( info->send_sockfd < 0 ) {
            printf( "Unable to open a socket!\015\012" );
            fflush( stdout );
            return NULL;
        }
        fcntl( info->send_sockfd, F_SETFL, FD_CLOEXEC );

        /*--- Connect socket using name specified ---*/
        struct sockaddr_in sockaddr;
        memset( &sockaddr, 0, sizeof( sockaddr ) );
        sockaddr.sin_family = AF_INET;
        sockaddr.sin_port = htons( info->sin_port );
        sockaddr.sin_addr.s_addr = info->sin_addr;
        if ( connect( info->send_sockfd, ( const struct sockaddr * ) &sockaddr, sizeof( sockaddr ) ) < 0 ) {
            printf( "%s %d:\015\012\tUnable to connect to host %s, port %d (name=%s)\015\012",
               __FILE__, __LINE__, inet_ntoa( sockaddr.sin_addr ), info->sin_port, name );
            fflush( stdout );
            closesocket( info->send_sockfd );
            return NULL;
        }

        /*--- Update list of connections to channels ---*/
        if ( nb_list_connect == 0 ) {
            list_connect = ( list_connect_t * ) malloc( sizeof( list_connect_t ) );
            nb_list_connect = 1;
        }
        else {
            list_connect = ( list_connect_t * ) realloc( list_connect, sizeof( list_connect_t ) * ( nb_list_connect + 1 ) );
            nb_list_connect++;
        }
        strcpy( list_connect[nb_list_connect - 1].name, name );
        list_connect[nb_list_connect - 1].info = info;

    }                           // else

    /*--- Send a fake message ---*/
    datasend.flag = MESSIP_FLAG_CONNECTING;
    iovec[0].iov_base = &datasend;
    iovec[0].iov_len = sizeof( datasend );
    dcount = messip_writev( info->send_sockfd, iovec, 1 );
    assert( dcount == sizeof( messip_datasend_t ) );

    return info;
}                               // messip_channel_connect

/**
 * Enables a client to disconnect from a channel owned by a server.
 * 
 * @note
 *  - No exchange is performed with the server (the owner of the channel), but only with the messip manager.
 *  - messip_channel_disconnect must be called only by a client (sending messages), 
 *    but not by a server (receiving messages
 * 
 * @param ch channel connection (to the server) structure which was returned by messip_channel_connect() 
 * @param msec_timeout if not 0, is a timeout (expressed in milliseconds) where the function exits if 
 * 		the connection with the messip manager fails.
 *  
 * @return A pointer to a messip_channel_t structure (that will be used next when sending messages on this channel), 
 * 		or -1 if an error occurred (errno is then set):
 *  - ETIMEDOUT occurs if either the messip manage and the server owning the channel did not answered 
 *    the connection request within the expressed time.
 * 
 * @see messip_channel_create(), messip_channel_disconnect(), messip_receive(), messip_send()
 */
int messip_channel_disconnect( messip_channel_t *ch, int msec_timeout ) {
    int status;
    fd_set ready;
    struct timeval tv;
    messip_datasend_t datasend;
    ssize_t dcount;
    struct iovec iovec[2];
    messip_send_channel_disconnect_t msgsend;
    messip_reply_channel_disconnect_t reply;
    int32_t op;

    /*--- Timeout to write ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( ch->send_sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) FD_SETSIZE, NULL, &ready, NULL, &tv );
        assert( status != -1 );
        if ( !FD_ISSET( ch->send_sockfd, &ready ) )
            return MESSIP_MSG_TIMEOUT;
    }

    /*--- Message to send ---*/
    datasend.flag = MESSIP_FLAG_DISCONNECTING;
    IDCPY( datasend.id, ch->cnx->remote_id );
    datasend.type = -1;
    datasend.datalen = 0;

    /*--- Send a message to the 'server' ---*/
    iovec[0].iov_base = &datasend;
    iovec[0].iov_len = sizeof( datasend );
    dcount = messip_writev( ch->send_sockfd, iovec, 1 );
    messip_log( MESSIP_LOG_INFO, "messip_channel_disconnect: sendmsg dcount=%d local_fd=%d [errno=%d] \n",
       dcount, ch->send_sockfd, errno );
    assert( dcount == sizeof( messip_datasend_t ) );

    /*
     * Now notify also messip_mgr
     */

    /*--- Ready to write ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( ch->cnx->sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) FD_SETSIZE, NULL, &ready, NULL, &tv );
        assert( status != -1 );
        if ( !FD_ISSET( ch->cnx->sockfd, &ready ) ) {
            errno = ETIMEDOUT;
            return -1;
        }
    }                           // if

    /*--- Send a message to the server ---*/
    op = MESSIP_OP_CHANNEL_DISCONNECT;
    iovec[0].iov_base = &op;
    iovec[0].iov_len = sizeof( int32_t );
    IDCPY( msgsend.id, ch->cnx->remote_id );
    strcpy( msgsend.name, ch->name );
    iovec[1].iov_base = &msgsend;
    iovec[1].iov_len = sizeof( msgsend );
    status = messip_writev( ch->cnx->sockfd, iovec, 2 );
    messip_log( MESSIP_LOG_INFO, "channel_disconnect: send status= %d\n", status );
    assert( ( status == sizeof( int32_t ) + sizeof( messip_send_channel_disconnect_t ) ) );

    /*--- Ready to read ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( ch->cnx->sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) FD_SETSIZE, &ready, NULL, NULL, &tv );
        assert( status != -1 );
        if ( !FD_ISSET( ch->cnx->sockfd, &ready ) ) {
            errno = ETIMEDOUT;
            return -1;
        }
    }                           // if

    /*--- Now wait for an answer from the server ---*/
    iovec[0].iov_base = &reply;
    iovec[0].iov_len = sizeof( reply );
    dcount = messip_readv( ch->cnx->sockfd, iovec, 1 );
    messip_log( MESSIP_LOG_INFO, "channel_disconnect: reply status= %d \n", dcount );
    assert( dcount == sizeof( reply ) );

    /*--- Channel deletion failed ? ---*/
    return reply.ok;
}                               // messip_channel_disconnect

/**
 *  TBD
 * 
 *  @param ch TBD
 *  @param msec_timeout TBD
 *  @return TBD
 */
int messip_channel_ping( messip_channel_t *ch, int msec_timeout ) {
    ssize_t dcount;
    messip_datasend_t datasend;
    messip_datareply_t datareply;
    struct iovec iovec[1];
    fd_set ready;
    struct timeval tv;
    int status;

    /*--- Timeout to write ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( ch->send_sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) FD_SETSIZE, NULL, &ready, NULL, &tv );
        assert( status != -1 );
        if ( !FD_ISSET( ch->send_sockfd, &ready ) )
            return MESSIP_MSG_TIMEOUT;
    }

    /*--- Message to send ---*/
    datasend.flag = MESSIP_FLAG_PING;
    IDCPY( datasend.id, ch->cnx->remote_id );
    datasend.type = -1;
    datasend.datalen = 0;

    /*--- Send a message to the 'server' ---*/
    iovec[0].iov_base = &datasend;
    iovec[0].iov_len = sizeof( datasend );
    dcount = messip_writev( ch->send_sockfd, iovec, 1 );
    messip_log( MESSIP_LOG_INFO, "messip_channel_ping: sendmsg dcount=%d local_fd=%d [errno=%d] \n",
       dcount, ch->send_sockfd, errno );
    assert( dcount == sizeof( messip_datasend_t ) );

    /*--- Timeout to read ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( ch->send_sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) FD_SETSIZE, &ready, NULL, NULL, &tv );
        assert( status != -1 );
        if ( !FD_ISSET( ch->send_sockfd, &ready ) )
            return MESSIP_MSG_TIMEOUT;
    }
    else {
        FD_ZERO( &ready );
        FD_SET( ch->send_sockfd, &ready );
        status = select( ( int ) ch->send_sockfd + 1, &ready, NULL, NULL, NULL );
    }

    /*--- Read reply from 'server' ---*/
    iovec[0].iov_base = &datareply;
    iovec[0].iov_len = sizeof( datareply );
    dcount = messip_readv( ch->send_sockfd, iovec, 1 );
    if ( dcount <= 0 ) {
        messip_log( MESSIP_LOG_ERROR, "%s %d\n\t dcount=%d  errno=%d\n", __FILE__, __LINE__, dcount, errno );
        return -1;
    }

    /*--- Ok ---*/
    IDCPY( ch->remote_id, datareply.id );
    return 0;
}                               // messip_channel_ping

/**
 *  TBD
 * 
 *  @param ch TBD
 *  @param index TBD
 *  @param msec_timeout TBD
 *  @return TBD
 */
static int ping_reply( messip_channel_t *ch, int index, int msec_timeout ) {
    ssize_t dcount;
    struct iovec iovec[1];
    messip_datareply_t datareply;
    fd_set ready;
    struct timeval tv;
    int status;

    /*--- Message to reply back ---*/
    IDCPY( datareply.id, ch->remote_id );
    datareply.datalen = 0;
    datareply.answer = -1;

    /*--- Timeout to write ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( ch->new_sockfd[index], &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) FD_SETSIZE, NULL, &ready, NULL, &tv );
        assert( status != -1 );
        if ( !FD_ISSET( ch->new_sockfd[index], &ready ) )
            return MESSIP_MSG_TIMEOUT;
    }

    /*--- Now wait for an answer from the server ---*/
    iovec[0].iov_base = &datareply;
    iovec[0].iov_len = sizeof( datareply );
    dcount = messip_writev( ch->new_sockfd[index], iovec, 1 );
    messip_log( MESSIP_LOG_INFO_VERBOSE, "ping_reply: sendmsg: dcount=%d  index=%d new_sockfd=%d errno=%d\n",
       dcount, index, ch->new_sockfd[index], errno );
    assert( dcount == sizeof( messip_datareply_t ) );

    /*--- Ok ---*/
    return 0;
}                               // ping_reply

/**
 *  TBD
 * 
 *  @param ch TBD
 *  @param sockfd TBD
 *  @param msec_timeout TBD
 *  @return TBD
 */
static int reply_to_thread_client_send_buffered_msg( messip_channel_t *ch, SOCKET sockfd, int msec_timeout ) {
    ssize_t dcount;
    struct iovec iovec[1];
    messip_datareply_t datareply;
    fd_set ready;
    struct timeval tv;
    int status;

    /*--- Message to reply back ---*/
    IDCPY( datareply.id, ch->remote_id );
    datareply.datalen = -1;
    datareply.answer = -1;

    /*--- Timeout to write ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) sockfd + 1, NULL, &ready, NULL, &tv ); // <&>
        assert( status != -1 );
        if ( !FD_ISSET( sockfd, &ready ) )
            return MESSIP_MSG_TIMEOUT;
    }

    /*--- Now wait for an answer from the server ---*/
    iovec[0].iov_base = &datareply;
    iovec[0].iov_len = sizeof( datareply );
    dcount = messip_writev( sockfd, iovec, 1 );
//  logg( NULL, "@reply_to_thread_client_send_buffered_msg to sockfd=%d: sendmsg: dcount=%d  errno=%d\n",
//        sockfd, dcount, errno );
    assert( dcount == sizeof( messip_datareply_t ) );

    /*--- Ok ---*/
    return 0;
}                               // reply_to_thread_client_send_buffered_msg

/**
 * Enables a server to receive messages sent on a channel owned by this server. 
 * Note that this is a blocking function, i.e. the client is blocked until not only the server has received 
 * the message, but also until the server has replied to the client (see messip_reply).
 * 
 * If the message requires a reply, the value returned by messip_receive should then be used as parameter 
 * when using messip_reply.
 * 
 * @param ch channel connection (to the server) structure which was returned by messip_channel_connect() 
 * @param type 32-bits numbers that can be used optionally to identify the kind of message sent to the serve.
 * @param rec_buffer pointer to the buffer where the message sent will be stored. 
 *    If set to an address of a pointer and if max_len is set to 0, then the buffer is dynamically allocated, 
 *    it then will have to be free-ed later.
 * @param maxlen maximum length of the message that can be stored in the receiving buffer. If the server has sent more bytes,
 *     the whole call is failing.
 * @param msec_timeout if not 0, is a timeout (expressed in milliseconds) where the function exits if connection 
 *     with the messip manager fails.
 * @return A status of the operation:
  *   - -1 if an error occurred (errno is then set). 
 *   - MESSIP_MSG_DISCONNECT: the client has called messip_channel_disconnect
 *   - MESSIP_MSG_DISMISSED: the connection between the client and the server has been broken. The main reason is that the client died
 *   - MESSIP_MSG_TIMEOUT: the operation timed out.
 *   - MESSIP_MSG_TIMER: a timer has been triggered on this channel.
 *   - MESSIP_MSG_NOREPLY: the message received was not an Asynchronous message, 
 *     and therefore does not require a reply.
 *   - Any other value (i.e. >= 0) is the parameter to use with messip_reply or messip_receive_more:
 *      - ETIMEDOUT  occurs if the client owning the channel did not received the message replied back 
 *                   within the expressed time.
 *      - ENOMEM     out of memory.
 *      - EFAULT     buffer points outside your accessible address space.
 *      - ECONNRESET Message has been received, but reply fails because the initial sender lost the connection 
 *                   with the server.
 * 
 * @see messip_channel_create(), messip_reply(), messip_receive_more(), messip_send()
 */
int messip_receive( messip_channel_t *ch, int32_t *type, void *rec_buffer, int maxlen, int msec_timeout ) {
    ssize_t dcount;
    struct iovec iovec[3];
    messip_datasend_t datasend;
    SOCKET new_sockfd = -1;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    fd_set ready;
    struct timeval tv;
    int status;
    int32_t len, len_to_read;
    int n, k, nothing, index;
    void *rbuff = NULL;

    if ( ch->nb_replies_pending == ch->new_sockfd_sz ) {
        ch->new_sockfd = ( SOCKET * ) realloc( ch->new_sockfd, sizeof( SOCKET * ) * ( ch->new_sockfd_sz + 1 ) );
        ch->new_sockfd[ch->new_sockfd_sz] = -1;
        ch->receive_allmsg = ( void ** ) realloc( ch->receive_allmsg, sizeof( void * ) * ( ch->new_sockfd_sz + 1 ) );
        ch->receive_allmsg_sz = ( int * ) realloc( ch->receive_allmsg_sz, sizeof( int * ) * ( ch->new_sockfd_sz + 1 ) );
        ch->receive_allmsg[ch->new_sockfd_sz] = NULL;
        index = ch->new_sockfd_sz++;
    }
    else {
        for ( index = 0; index < ch->new_sockfd_sz; index++ )
            if ( ch->new_sockfd[index] == -1 )
                break;
        assert( index < ch->new_sockfd_sz );
    }

  restart:

    /*--- Timeout ? ---*/
    do {
        SOCKET maxfd = 0;
        FD_ZERO( &ready );
        for ( n = 0; n < ch->recv_sockfd_sz; n++ ) {
            FD_SET( ch->recv_sockfd[n], &ready );
            if ( ch->recv_sockfd[n] > maxfd )
                maxfd = ch->recv_sockfd[n];
        }
        if ( msec_timeout != MESSIP_NOTIMEOUT ) {
            if ( msec_timeout == 1 ) {
                tv.tv_sec = 0;
                tv.tv_usec = 1;
            }
            else {
                tv.tv_sec = msec_timeout / 1000;
                tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
            }
            status = select( ( int ) maxfd + 1, &ready, NULL, NULL, &tv );
        }
        else {
            status = select( ( int ) maxfd + 1, &ready, NULL, NULL, NULL );
        }
    } while ( ( status == -1 ) && ( errno == EINTR ) );
    if ( status == -1 )
        return -1;
    if ( ( msec_timeout != MESSIP_NOTIMEOUT ) && ( status == 0 ) ) {
        ch->new_sockfd[index] = -1;
        return MESSIP_MSG_TIMEOUT;
    }

    for ( nothing = 1, n = 0; n < ch->recv_sockfd_sz; n++ ) {
        if ( FD_ISSET( ch->recv_sockfd[n], &ready ) ) {
            nothing = 0;
            break;
        }
    }
    if ( nothing ) {
        *type = -1;
        ch->new_sockfd[index] = -1;
        return MESSIP_MSG_TIMEOUT;
    }

    /*--- Accept a new connection ---*/
    if ( !n ) {
        client_addr_len = sizeof( struct sockaddr_in );
        errno = 9999;
        new_sockfd = accept( ch->recv_sockfd[0], ( struct sockaddr * ) &client_addr, &client_addr_len );
        if ( new_sockfd == -1 ) {
            printf( "messip_receive) %s %d\015\012\terrno=%d\015\012", __FILE__, __LINE__, errno );
            fflush( stdout );
            ch->new_sockfd[index] = new_sockfd;
            return -1;
        }
//      logg( NULL,
//         "%s: accepted a msg from %s, port=%d  - old socket=%d new=%d\n",
//            __FUNCTION__,
//            inet_ntoa( client_addr.sin_addr ), client_addr.sin_port, ch->recv_sockfd[0],
//            new_sockfd );
        ch->recv_sockfd[ch->recv_sockfd_sz++] = new_sockfd;
    }
    else {
        new_sockfd = ch->recv_sockfd[n];
    }

    /*--- Create a new channel info ---*/
    ch->new_sockfd[index] = new_sockfd;
//  logg( NULL, "@messip_receive: pending=%d sz=%d new_sockfd=%d index=%d\n",
//      ch->nb_replies_pending, ch->new_sockfd_sz, new_sockfd, index );

    /*--- (R1) First read the fist part of the message ---*/
    iovec[0].iov_base = &datasend;
    iovec[0].iov_len = sizeof( datasend );
    dcount = messip_readv( new_sockfd, iovec, 1 );
    if ( ( dcount == 0 ) || ( ( dcount == -1 ) && ( errno == ECONNRESET ) ) ) {
        shutdown( ch->recv_sockfd[n], SHUT_RDWR );
        closesocket( ch->recv_sockfd[n] );
        for ( k = n + 1; k < ch->recv_sockfd_sz; k++ )
            ch->recv_sockfd[k - 1] = ch->recv_sockfd[k];
        ch->recv_sockfd_sz--;
        goto restart;
    }
    if ( dcount == -1 ) {
        printf( "messip_receive) %s %d\015\012\tdcount=%d  errno=%d\015\012", __FILE__, __LINE__, dcount, errno );
        fflush( stdout );
        ch->new_sockfd[index] = new_sockfd;
        return -1;
    }
    if ( datasend.flag == MESSIP_FLAG_CONNECTING )
        goto restart;
//  logg( NULL, "@messip_receive part1: dcount=%d state=%d datalen=%d flags=%d\n",
//        dcount, datasend.state, datasend.datalen, datasend.flag );

    /*--- If message was a channel-disconnect, nothing additional to read ---*/
    IDCPY( ch->remote_id, datasend.id );
    if ( datasend.flag == MESSIP_FLAG_DISCONNECTING ) {
        *type = ( int32_t ) new_sockfd;
        ch->new_sockfd[index] = -1;
        return MESSIP_MSG_DISCONNECT;
    }                           // if
    if ( datasend.flag == MESSIP_FLAG_DISMISSED ) {
        *type = ( int32_t ) new_sockfd;
        *type = ( int32_t ) ch->recv_sockfd[0];
        ch->new_sockfd[index] = -1;
        return MESSIP_MSG_DISMISSED;
    }                           // if
    if ( datasend.flag == MESSIP_FLAG_DEATH_PROCESS ) {
        assert( 0 );
//      *type    = datasend.pid;
//      *subtype = (int)datasend.tid;
//      ch->new_sockfd[index] = -1;
//      return MESSIP_MSG_DEATH_PROCESS;
    }                           // if

    /*--- If message was a timer, nothing additional to read ---*/
    *type = datasend.type;
    if ( datasend.flag == MESSIP_FLAG_TIMER ) {
        ch->datalen = -1;
        ch->datalenr = ( int32_t ) new_sockfd;
        ch->new_sockfd[index] = -1;
        return MESSIP_MSG_TIMER;
    }

    /*--- If message is a ping, reply to the sender ---*/
    if ( datasend.flag == MESSIP_FLAG_PING ) {
        ping_reply( ch, index, msec_timeout );
        goto restart;
    }

    /*--- Dynamic allocation asked ? ---*/
    ch->datalen = datasend.datalen;
    ch->datalenr = 0;
    iovec[0].iov_base = &len;
    iovec[0].iov_len = sizeof( uint32_t );
    if ( ( rec_buffer != NULL ) && ( maxlen == 0 ) ) {
        rbuff = ( void * ) malloc( datasend.datalen );
        if ( rbuff == NULL ) {
            ch->new_sockfd[index] = -1;
            errno = ENOMEM;
            return MESSIP_NOK;
        }
        len_to_read = datasend.datalen;
        iovec[1].iov_base = rbuff;
        iovec[1].iov_len = len_to_read;
    }
    else {
        len_to_read = ( maxlen < datasend.datalen ) ? maxlen : datasend.datalen;
        iovec[1].iov_base = rec_buffer;
        iovec[1].iov_len = len_to_read;
    }

    /*--- (R2) Now read the message, unless if it's a timer ---*/
    dcount = messip_readv( new_sockfd, iovec, 2 );
//  logg( NULL, "@messip_receive part2: dcount=%d len_to_read=%d\n",
//        dcount, len_to_read );
    if ( ( dcount == 0 ) || ( ( dcount == -1 ) && ( errno == ECONNRESET ) ) ) {
        shutdown( ch->recv_sockfd[n], SHUT_RDWR );
        closesocket( ch->recv_sockfd[n] );
        for ( k = n + 1; k < ch->recv_sockfd_sz; k++ )
            ch->recv_sockfd[k - 1] = ch->recv_sockfd[k];
        ch->recv_sockfd_sz--;
        goto restart;
    }
    if ( dcount == -1 ) {
        messip_log( MESSIP_LOG_INFO, "messip_receive) %s %d\n\tdcount=%d  errno=%s\n",
           __FILE__, __LINE__, dcount, strerror( errno ) );
        ch->new_sockfd[index] = new_sockfd;
        return -1;
    }
    assert( dcount == sizeof( uint32_t ) + len_to_read );
    ch->datalenr = len_to_read;

    /*
     * Allocate a temp buffer to hold the whole message - used by Msgread()
     * Will be free-ed by Reply()
     */
    if ( datasend.flag != MESSIP_FLAG_BUFFERED ) {
        ch->receive_allmsg[index] = malloc( datasend.datalen );
        ch->receive_allmsg_sz[index] = datasend.datalen;
        assert( len_to_read <= datasend.datalen );
        memmove( ch->receive_allmsg[index], rec_buffer, len_to_read );
    }
    else {
        ch->receive_allmsg[index] = NULL;
        ch->receive_allmsg_sz[index] = 0;
    }

    /*
     * Read more data ? (provided buffer was too small)
     */
//  logg( NULL, "+++ datalen=%d maxlen=%d\n", datasend.datalen, maxlen );
    if ( ( rec_buffer != NULL ) && ( maxlen != 0 ) && ( maxlen < datasend.datalen ) ) {

        /*--- Now read the message, unless if it's a timer ---*/
        char *t = ( char * ) ch->receive_allmsg[index];
        iovec[0].iov_base = &t[len_to_read];
        len_to_read = ch->datalen - maxlen;
        iovec[0].iov_len = len_to_read;
        dcount = messip_readv( new_sockfd, iovec, 1 );
        if ( dcount == -1 ) {
            messip_log( MESSIP_LOG_INFO, "messip_receive_more) %s %d\n\tdcount=%d  errno=%d\n",
               __FILE__, __LINE__, dcount, errno );
            return -1;
        }
//      fprintf( stdout, "@messip_receive part3: dcount=%d len_to_read=%d\n",
//            dcount, len_to_read );
        assert( dcount == len_to_read );
        ch->datalenr += len_to_read;

    }

    /*--- Dynamic allocation ? ---*/
    if ( ( rec_buffer != NULL ) && ( maxlen == 0 ) )
        *( void ** ) rec_buffer = rbuff;

    /*--- Ok ---*/
    if ( datasend.flag == MESSIP_FLAG_BUFFERED ) {
        reply_to_thread_client_send_buffered_msg( ch, new_sockfd, msec_timeout );
//      reply_to_thread_client_send_buffered_msg( ch->cnx->sockfd, msec_timeout );
        ch->new_sockfd[index] = -1;
        return MESSIP_MSG_NOREPLY;
    }
    else {
        ch->nb_replies_pending++;
        return index;
    }
}                               // messip_receive

/**
 * Enables a client to send a synchronous message to a channel owned by a server. 
 * Note that this is a blocking function, i.e. the client is blocked until the server not only 
 * until the server has received the message, but also until the server has replied to the client.
 * 
 * @note 
 *   - No exchange at all is performed with the messip manager, but only with the server 
 *     (i.e.the process owning the channel).
 *   - messip_channel_send must be called only by a client (sending messages), 
 *     but not by a server (receiving messages).
 * 
 * @param ch channel connection (to the server) structure which was returned by messip_channel_connect()
 * @param type 32-bits number that can be used optionally to identify the kind of message sent to the server
 * @param send_buffer pointer to the message to send. Note it’s valid to specify NULL and  
 *    send_len set to 0, in this case no message is sent (only the two 32-bits type codes).
 * @param send_len length of the message to be sent (can be 0).
 * @param answer 32-bit number that can be used optionally to identify the status or type of answer. 
 *    Note that this pointer can be NULL.
 * @param reply_buffer pointer to a buffer where to store the answer sent back from the server. 
 *    As an option, you can specify the address to a pointer and specify 0 for reply_maxlen, then the buffer 
 *    will be dynamically allocated (you will have later to free it).
 * @param reply_maxlen Maximum length for the reply.
 * @param msec_timeout msec_timeout, if not 0, is a timeout (expressed in milliseconds)  where the function exits 
 *    if connection with the messip manager fails.
 * 
 * @return A pointer to a messip_channel_t structure (that will be used next when sending messages on this channel),
 *    or -1 if an error occurred (errno is then set):
 *      - ETIMEDOUT occurs if either the server owning the channel did not received and replied to the 
 *        message within the expressed time.
 *
 * @see messip_channel_create(), messip_channel_disconnect(), messip_receive(), messip_send()
 */
int messip_send( messip_channel_t *ch, int32_t type, 
	void *send_buffer, int send_len, int32_t *answer, 
	void *reply_buffer, int reply_maxlen, int msec_timeout ) {
    ssize_t dcount;
    messip_datasend_t datasend;
    messip_datareply_t datareply;
    struct iovec iovec[3];
    fd_set ready;
    struct timeval tv;
    int status;
    int32_t len, len_to_read;
    void *rbuff = NULL;

    /*--- Timeout to write ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( ch->send_sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) ch->send_sockfd + 1, NULL, &ready, NULL, &tv );
        assert( status != -1 );
        if ( !FD_ISSET( ch->send_sockfd, &ready ) )
            return MESSIP_MSG_TIMEOUT;
    }

    /*--- Message to send ---*/
    datasend.flag = 0;
    IDCPY( datasend.id, ch->cnx->remote_id );
    datasend.type = type;
    datasend.datalen = send_len;

    /*--- (S1) Send a message to the 'server' ---*/
    iovec[0].iov_base = &datasend;
    iovec[0].iov_len = sizeof( datasend );
    len = reply_maxlen;
    iovec[1].iov_base = &len;
    iovec[1].iov_len = sizeof( uint32_t );
    iovec[2].iov_base = send_buffer;
    iovec[2].iov_len = send_len;
    dcount = messip_writev( ch->send_sockfd, iovec, 3 );
//  logg( NULL, "{messip_send/3} sendmsg send_len=%d dcount=%d local_fd=%d [errno=%d] \n",
//        send_len, dcount, ch->send_sockfd, errno );
    if ( dcount == -1 ) {
        printf( "%s %d:\015\012\terrno=%m\015\012", __FILE__, __LINE__ );
        fflush( stdout );
        return -1;
    }
    assert( dcount == ( sizeof( messip_datasend_t ) + sizeof( uint32_t ) + send_len ) );

    /*--- Timeout to read ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( ch->send_sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) ch->send_sockfd + 1, &ready, NULL, NULL, &tv );
        assert( status != -1 );
        if ( !FD_ISSET( ch->send_sockfd, &ready ) )
            return MESSIP_MSG_TIMEOUT;
    }

    /*--- (S2) Read reply from 'server' ---*/
    iovec[0].iov_base = &datareply;
    iovec[0].iov_len = sizeof( datareply );
    dcount = messip_readv( ch->send_sockfd, iovec, 1 );
    if ( dcount == 0 ) {
//      fprintf( stderr, "%s %d:\015\012\terrno=%d\015\012", __FILE__, __LINE__, errno );
        errno = ECONNRESET;
        return -1;
    }
    if ( dcount == -1 ) {
        if ( errno != ECONNRESET ) {
            printf( "%s %d:\015\012\terrno=%d\015\012", __FILE__, __LINE__, errno );
            fflush( stdout );
        }
        return -1;
    }
    *answer = datareply.answer;

    /*--- (S3) Read now the reply, if there is one ---*/
//  logg( NULL, "--- datalen=%d maxlen=%d\n", datareply.datalen, reply_maxlen );
    if ( ( reply_buffer != NULL ) && ( reply_maxlen == 0 ) && ( datareply.datalen > 0 ) ) {
        len_to_read = datareply.datalen;
        rbuff = malloc( datareply.datalen );
        iovec[0].iov_base = rbuff;
        iovec[0].iov_len = len_to_read;
    }
    else {
        len_to_read = ( datareply.datalen < reply_maxlen ) ? datareply.datalen : reply_maxlen;
        iovec[0].iov_base = reply_buffer;
        iovec[0].iov_len = len_to_read;
    }
    if ( len_to_read > 0 ) {
        dcount = messip_readv( ch->send_sockfd, iovec, 1 );
        messip_log( MESSIP_LOG_INFO_VERBOSE, "messip_send: recvmsg dcount=%d local_fd=%d [errno=%d]\n",
           dcount, ch->send_sockfd, errno );
        if ( dcount == 0 ) {
            printf( "%s %d:\015\012\terrno=%d\015\012", __FILE__, __LINE__, errno );
            fflush( stdout );
            errno = ECONNRESET;
            return -1;
        }
        if ( dcount == -1 ) {
            printf( "%s %d:\015\012\terrno=%d\015\012", __FILE__, __LINE__, errno );
            fflush( stdout );
            return -1;
        }
    }                           // if

    if ( len_to_read < datareply.datalen ) {
//      logg( NULL, "ZUT!! %d %d\n", len_to_read, datareply.datalen );
        int rem = datareply.datalen - len_to_read;
        char *temp = ( char * ) malloc( rem );
        iovec[0].iov_base = temp;
        iovec[0].iov_len = rem;
        dcount = messip_readv( ch->send_sockfd, iovec, 1 );
//      logg( NULL, "@messip_send: rem=%d dcount=%d local_fd=%d [errno=%d]\n",
//            rem, dcount, ch->send_sockfd, errno );
        free( temp );
    }

    /*--- Dynamic allocation ? ---*/
    if ( ( reply_buffer != NULL ) && ( reply_maxlen == 0 ) )
        *( void ** ) reply_buffer = rbuff;

    /*--- Ok ---*/
    ch->datalen = datareply.datalen;
    ch->datalenr = len_to_read;
    IDCPY( ch->remote_id, datareply.id );
    return 0;
}                               // messip_send

/**
 * Enable a client to send an Asynchronous Message to a server.
 * The server will buffer these messages, until the maximum number of buffered messages is reached. 
 * There is no reply from the server, therefore the client does not wait, unless the maximum number 
 * of buffered messages is  reached. 
 * 
 * @param ch channel connection (to the server) structure which was returned by messip_channel_connect() 
 * @param type 32-bits number that can be used optionally to identify the kind of message sent to the server
 * @param send_buffer pointer to the message to send. Note it’s valid to specify NULL and  send_len set to 0, 
 * 	in this case no message is sent (only the two 32-bits codes).
 * @param send_len length of the message to be sent (can be 0).
 * @param msec_timeout if not 0, is a timeout (expressed in milliseconds)
 *  where the function exits if connection with the messip manager fails.
 * 
 * @return pointer to a messip_channel_t structure (that will be used next when sending messages on this channel), 
 *  or -1 if an error occurred (errno is then set).
 * 
 * @see messip_channel_create(), messip_channel_disconnect(), messip_receive(), messip_send()
 */
int32_t messip_buffered_send( messip_channel_t *ch, int32_t type, void *send_buffer, int send_len, int msec_timeout ) {
    ssize_t dcount;
    fd_set ready;
    struct timeval tv;
    int status;
    int32_t op;
    messip_send_buffered_send_t msgsend;
    messip_reply_buffered_send_t msgreply;
    struct iovec iovec[3];

    /*--- Timeout to write ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( ch->cnx->sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) FD_SETSIZE, NULL, &ready, NULL, &tv );
        assert( status != -1 );
        if ( !FD_ISSET( ch->cnx->sockfd, &ready ) )
            return MESSIP_MSG_TIMEOUT;
    }

    /*--- Send a service message to the server + the private message ---*/
    op = MESSIP_OP_BUFFERED_SEND;
    iovec[0].iov_base = &op;
    iovec[0].iov_len = sizeof( int32_t );
    IDCPY( msgsend.id_from, ch->remote_id );
    msgsend.type = type;
    msgsend.datalen = send_len;
    msgsend.mgr_sockfd = ch->mgr_sockfd;    // Socket in the messip_mgr

    iovec[1].iov_base = &msgsend;
    iovec[1].iov_len = sizeof( msgsend );
    iovec[2].iov_base = send_buffer;
    iovec[2].iov_len = send_len;
    dcount = messip_writev( ch->cnx->sockfd, iovec, 3 );
    messip_log( MESSIP_LOG_INFO_VERBOSE, "messip_buffered_send: send status= %d  sockfd=%d\n", dcount, ch->cnx->sockfd );
    assert( ( dcount == sizeof( int32_t ) + sizeof( msgsend ) + send_len ) );

    /*--- Ready to read ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( ch->cnx->sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) FD_SETSIZE, &ready, NULL, NULL, &tv );
        assert( status != -1 );
        if ( !FD_ISSET( ch->cnx->sockfd, &ready ) ) {
            errno = ETIMEDOUT;
            return -1;
        }
    }                           // if

    /*--- Now wait for an answer from the server ---*/
    iovec[0].iov_base = &msgreply;
    iovec[0].iov_len = sizeof( msgreply );
    dcount = messip_readv( ch->cnx->sockfd, iovec, 1 );
//  printf( "@messip_buffered_send: reply dcount= %d,%d \n", 
//      dcount, sizeof( msgreply ) );
    assert( dcount == sizeof( msgreply ) );

    return msgreply.nb_msg_buffered;
}                               // messip_buffered_send

/**
 * Enables a server to reply to a client that has sent a messages to a channel owned by this server
 * The client was blocked on the messip_send() function, and the messip_reply() function will unblock the client.
 * 
 * @param ch channel connection (to the server) structure which was returned by messip_channel_connect() 
 * @param index value previously returned by the messip_receive()
 * @param answer 32-bits number that can be used optionally to identify the kind of message sent back 
 * 		to the client.
 * @param reply_buffer pointer to the buffer where the message to be sent is stored. A NULL value is valid 
 * 		(as long as reply_len is set to 0).
 * @param reply_len length of the message to be sent. Note that 0 is a valid value 
 * 		(in this case reply_buffer should be set to NULL).
 * @param msec_timeout if not 0, is a timeout (expressed in milliseconds) where the function exits 
 * 		if connection with the client has failed.
 * 
 * @return A status of the operation:
 *	- MESSIP_NOK if an error occurred (errno is then set).
 *	- MESSIP_MSG_DISCONNECT: the client has called messip_channel_disconnect
 * 	- MESSIP_MSG_DISMISSED: the connection between the client and the server has been broken.
 * 	  The principal reason is that the client died.
 *	- MESSIP_MSG_TIMEOUT:
 *	- MESSIP_OK if the reply function has successed.
 * 
 * @see messip_channel_create(), messip_reply(), messip_receive_more(), messip_send()
 */
int messip_reply( messip_channel_t *ch, int index, int32_t answer, void *reply_buffer, int reply_len, int msec_timeout ) {
    ssize_t dcount;
    struct iovec iovec[2];
    messip_datareply_t datareply;
    fd_set ready;
    struct timeval tv;
    int status, sz;

    if ( ( index < 0 ) || ( index > ch->nb_replies_pending ) )
        return -1;

    /*--- Message to reply back ---*/
//  logg( NULL, "messip_reply:  pthread_self=%d\n", pthread_self() );
    IDCPY( datareply.id, ch->cnx->remote_id );
    datareply.datalen = reply_len;
    datareply.answer = answer;

    /*--- Timeout to write ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( ch->new_sockfd[index], &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) FD_SETSIZE, NULL, &ready, NULL, &tv );
        assert( status != -1 );
        if ( !FD_ISSET( ch->new_sockfd[index], &ready ) )
            return MESSIP_MSG_TIMEOUT;
    }

    /*--- Now wait for an answer from the server ---*/
    sz = 0;
    iovec[sz].iov_base = &datareply;
    iovec[sz++].iov_len = sizeof( messip_datareply_t );
    if ( reply_len > 0 ) {
        iovec[sz].iov_base = reply_buffer;
        iovec[sz++].iov_len = reply_len;
    }
    dcount = messip_writev( ch->new_sockfd[index], iovec, sz );
    messip_log( MESSIP_LOG_INFO_VERBOSE, "messip_reply: sendmsg: dcount=%d  index=%d new_sockfd=%d errno=%d\n",
       dcount, index, ch->new_sockfd[index], errno );
    assert( dcount == ( sizeof( messip_datareply_t ) + reply_len ) );

    --ch->nb_replies_pending;
    ch->new_sockfd[index] = -1;

    free( ch->receive_allmsg[index] );
    ch->receive_allmsg[index] = NULL;
    ch->receive_allmsg_sz[index] = 0;

    /*--- Ok ---*/
    return 0;
}                               // messip_reply


/**
 *  TBD
 * 
 *  @param ch TBD
 *  @param type TBD
 *  @param msec_timeout TBD
 *  @return TBD
 */
static int messip_timer_send( messip_channel_t *ch, int32_t type, int msec_timeout ) {
    ssize_t dcount;
    messip_datasend_t datasend;
    struct iovec iovec[1];
    fd_set ready;
    struct timeval tv;
    int status;

    /*--- Timeout to write ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( ch->send_sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        status = select( ( int ) FD_SETSIZE, NULL, &ready, NULL, &tv );
        assert( status != -1 );
        if ( !FD_ISSET( ch->send_sockfd, &ready ) )
            return MESSIP_MSG_TIMEOUT;
    }

    /*--- Message to send ---*/
    datasend.flag = MESSIP_FLAG_TIMER;
    IDCPY( datasend.id, ch->remote_id );
    datasend.type = type;
    datasend.datalen = 0;

    /*--- Send a message to the 'server' ---*/
    iovec[0].iov_base = &datasend;
    iovec[0].iov_len = sizeof( datasend );
    dcount = messip_writev( ch->send_sockfd, iovec, 1 );
    messip_log( MESSIP_LOG_INFO_VERBOSE, "messip_timer_send: sendmsg dcount=%d type=%d local_fd=%d\n",
       dcount, ch->send_sockfd, type );
    assert( dcount == sizeof( messip_datasend_t ) );

    /*--- Ok ---*/
    return 0;
}                               // messip_timer_send

typedef struct messip_timer_s {
    timer_t timer_id;
    messip_channel_t *ch;
    int32_t user_type;
} messip_timer_t;

#  if TIMER_USE_SIGEV_THREAD==1

/**
 *  TBD
 * 
 *  @param sigval TBD
 */
static void timer_thread_handler( sigval_t sigval ) {
    messip_timer_t *timer_info = ( messip_timer_t * ) sigval.SIGVAL_PTR;
    messip_channel_t *ch = ( messip_channel_t * ) timer_info->ch;
    int status;

    if ( ch->send_sockfd == -1 ) {
        if ( ( ch = messip_channel_connect( ch->cnx, ch->name, MESSIP_NOTIMEOUT ) ) == NULL )
            assert( 0 );
        timer_info->ch = ch;
    }                           // if
    status = messip_timer_send( ch, timer_info->user_type, MESSIP_NOTIMEOUT );
}                               // timer_thread_handler

#  elif TIMER_USE_SIGEV_SIGNAL==1

/**
 *  TBD
 * 
 *  @param signo TBD
 *  @param info TBD
 *  @param context TBD
 */
static void sig_action( int signo, siginfo_t *info, void *context ) {
    messip_timer_t *timer_info = ( messip_timer_t * ) info->si_value.SIGVAL_PTR;
    messip_channel_t *ch = ( messip_channel_t * ) timer_info->ch;
    int status;

    if ( ch->send_sockfd == -1 ) {
        if ( ( ch = messip_channel_connect( ch->cnx, ch->name, MESSIP_NOTIMEOUT ) ) == NULL )
            assert( 0 );
        timer_info->ch = ch;
    }                           // if
    status = messip_timer_send( ch, timer_info->user_type, MESSIP_NOTIMEOUT );
}                               // sig_action

/**
 *  TBD
 * 
 *  @param ch TBD
 *  @param type TBD
 *  @param msec_1st_shot TBD
 *  @param msec_rep_shot TBD
 *  @param msec_timeout TBD
 *  @return TBD
 */
timer_t messip_timer_create( messip_channel_t *ch, int32_t type, int msec_1st_shot, int msec_rep_shot, int msec_timeout ) {
    struct sigevent event;
    struct itimerspec itime;
#    if defined(TIMER_USE_SIGEV_SIGNAL)
    struct sigaction sig_act;
#    endif

    messip_timer_t *timer_info = ( messip_timer_t * ) malloc( sizeof( messip_timer_t ) );
    timer_info->ch = ch;
    timer_info->user_type = type;

    memset( &event, 0, sizeof( struct sigevent ) );
#    if TIMER_USE_SIGEV_THREAD==1
    event.sigev_notify = SIGEV_THREAD;
    event.sigev_notify_function = &timer_thread_handler;
#    elif TIMER_USE_SIGEV_SIGNAL==1
    event.sigev_notify = SIGEV_SIGNAL;
    event.sigev_signo = SIGRTMIN;
#    endif
    event.sigev_value.SIGVAL_PTR = timer_info;
    timer_create( CLOCK_REALTIME, &event, &timer_info->timer_id );

#    if defined(TIMER_USE_SIGEV_SIGNAL)
    sigemptyset( &sig_act.sa_mask );    /* block current signal */
    sigaddset( &sig_act.sa_mask, SIGUSR1 ); /* and also these one: */
    sigaddset( &sig_act.sa_mask, SIGUSR2 );
    sigaddset( &sig_act.sa_mask, SIGALRM );
    sigaddset( &sig_act.sa_mask, SIGPIPE );
    sigaddset( &sig_act.sa_mask, SIGHUP );
    sigaddset( &sig_act.sa_mask, SIGCHLD );
    sigaddset( &sig_act.sa_mask, SIGRTMIN + 1 );
    sig_act.sa_flags = SA_SIGINFO;
    sig_act.sa_sigaction = sig_action;
    sigaction( SIGRTMIN,        /* Set action for SIGRTMIN      */
       &sig_act,                /* Action to take on signal     */
       0 );                     /* Don't care about old actions */
#    endif

    itime.it_value.tv_sec = msec_1st_shot / 1000;
    itime.it_value.tv_nsec = ( msec_1st_shot % 1000 ) * 1000000;
    itime.it_interval.tv_sec = msec_rep_shot / 1000;
    itime.it_interval.tv_nsec = ( msec_rep_shot % 1000 ) * 1000000;
    timer_settime( timer_info->timer_id, 0, &itime, NULL );

    return timer_info->timer_id;
}                               // messip_timer_create

#endif

/**
 *  TBD
 * 
 *  @param cnx TBD
 *  @param status TBD
 *  @param msec_timeout TBD
 *  @return TBD
 */
int32_t messip_death_notify( messip_cnx_t *cnx, int status, int msec_timeout ) {
    ssize_t dcount;
    fd_set ready;
    struct timeval tv;
    int ok;
    int32_t op;
    messip_send_death_notify_t msgsend;
    messip_reply_death_notify_t msgreply;
    struct iovec iovec[2];

    /*--- Timeout to write ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( cnx->sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        ok = select( ( int ) FD_SETSIZE, NULL, &ready, NULL, &tv );
        assert( ok != -1 );
        if ( !FD_ISSET( cnx->sockfd, &ready ) )
            return MESSIP_MSG_TIMEOUT;
    }

    /*--- Send a service message to the server + the private message ---*/
    op = MESSIP_OP_DEATH_NOTIFY;
    iovec[0].iov_base = &op;
    iovec[0].iov_len = sizeof( int32_t );
    IDCPY( msgsend.id_from, cnx->remote_id );
    msgsend.status = status;
    iovec[1].iov_base = &msgsend;
    iovec[1].iov_len = sizeof( msgsend );
    dcount = messip_writev( cnx->sockfd, iovec, 2 );
    messip_log( MESSIP_LOG_INFO, "messip_death_notify: send status= %d  sockfd=%d\n", dcount, cnx->sockfd );
    assert( ( dcount == sizeof( int32_t ) + sizeof( msgsend ) ) );

    /*--- Ready to read ? ---*/
    if ( msec_timeout != MESSIP_NOTIMEOUT ) {
        FD_ZERO( &ready );
        FD_SET( cnx->sockfd, &ready );
        tv.tv_sec = msec_timeout / 1000;
        tv.tv_usec = ( msec_timeout % 1000 ) * 1000;
        ok = select( ( int ) FD_SETSIZE, &ready, NULL, NULL, &tv );
        assert( ok != -1 );
        if ( !FD_ISSET( cnx->sockfd, &ready ) ) {
            errno = ETIMEDOUT;
            return -1;
        }
    }                           // if

    /*--- Now wait for an answer from the server ---*/
    iovec[0].iov_base = &msgreply;
    iovec[0].iov_len = sizeof( msgreply );
    dcount = messip_readv( cnx->sockfd, iovec, 1 );
    messip_log( MESSIP_LOG_INFO, "messip_death_notify: reply dcount= %d \n", dcount );
    assert( dcount == sizeof( msgreply ) );

    return msgreply.ok;
}                               // messip_death_notify

/**
 *  TBD
 * 
 *  @param level TBD
 */
void messip_set_log_level( unsigned level ) {
    log_level = level;
}                               // messip_set_log_level

/**
 *  TBD
 * 
 *  @return TBD
 */
unsigned messip_get_log_level( void ) {
    return log_level;
}                               // messip_get_log_level

/**
 *  TBD
 * 
 *  @param level TBD
 *  @param fmt TBD
 *  @param ... TBD
 *  @return TBD
 */
int messip_log( unsigned level, const char *fmt, ... ) {
    if ( !( level & log_level ) )
        return 0;
    va_list ap;
    va_start( ap, fmt );
    int len = vprintf( fmt, ap );
    va_end( ap );
    fflush( stdout );
     return len;
}                               // messip_log
