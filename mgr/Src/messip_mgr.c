/**
 * @file messip_mgr.c
 *
 * MessIP : Message Passing over TCP/IP
 * Copyright (C) 2001-2007  Olivier Singla
 * http://messip.sourceforge.net/
 *
 * This is the server that manages the names, and the buffered messages 
 * (non blocking fixed messages).
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <getopt.h>
#include <fcntl.h>
#include <assert.h>
#include <endian.h>

#include "messip.h"
#include "messip_private.h"

#include "messip_utils.h"

#include "logg_messip.h"


static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define	LOCK \
	pthread_mutex_lock( &mutex )
#define	UNLOCK \
	pthread_mutex_unlock( &mutex )


/*
	Buffered message
*/
typedef struct buffered_msg_t {
    messip_id_t id_from;
    messip_id_t id_to;
    int32_t type;
    void *data;                 // Can be NULL
    int32_t datalen;
} buffered_msg_t;

/*
	List of active connexions (nodes)
*/
typedef struct {
    time_t when;
    messip_id_t id;
    char process_name[MESSIP_CHANNEL_NAME_MAXLEN + 1];
    struct sockaddr_in xclient_addr;
    int sockfd;
    int nb_cnx_channels;
    int *sockfd_cnx_channels;
#ifdef DEBUG
    int state;
    messip_id_t id_blocked_on;
    time_t when_blocked_on;
#endif
} connexion_t;
static int nb_connexions;
static connexion_t **connexions;

/*--- List of active channels ---*/
typedef struct channel_t {
    messip_id_t id;
    connexion_t *cnx;
    char channel_name[MESSIP_CHANNEL_NAME_MAXLEN + 1];
    time_t when;                // When this channel has been created
    int sockfd;
    in_port_t sin_port;
    in_addr_t sin_addr;
    char sin_addr_str[48];
    int f_notify_deaths;        // Send a Msg on the death of each process

    // Buffered Messages
    pthread_t tid_client_send_buffered_msg;
    int bufferedsend_sockfd;
    int32_t maxnb_msg_buffered;
    int32_t nb_msg_buffered;
    int reply_on_release_sockfd;
    buffered_msg_t **buffered_msg;  // Dynamic Array

    int nb_clients;
    int *cnx_clients;           // Dynamic Array

} channel_t;
static int nb_channels;
static channel_t **channels;    // This is an array

static int f_bye;               // Set to 1 when SIGINT has been applied

static int messip_port;         ///< TBD
static int messip_port_http;    ///< TBD
static char messip_hostname[64];    ///< TBD

char *logg_dir;                 // Specified by --l or set to NULL


enum {
    KEY_CONNEXION_INDEX = 123,
    KEY_CONNEXION_ID,
    KEY_CONNEXION_ADDRESS,
    KEY_CONNEXION_PORT,
    KEY_CONNEXION_SOCKET,
    KEY_CONNEXION_SINCE
};

/**
 * TBD 
 * 
 * @param c1 TBD
 * @param c2 TBD
 * @return TBD
 */
static int qsort_channels( const void *c1, const void *c2 ) {
    const channel_t **ch1 = ( const channel_t ** ) c1;
    const channel_t **ch2 = ( const channel_t ** ) c2;
    return strcmp( ( *ch1 )->channel_name, ( *ch2 )->channel_name );
}                               // qsort_channels

/**
 * TBD 
 * 
 * @param c1 TBD
 * @param c2 TBD
 * @return TBD
 */
static int bsearch_channels( const void *c1, const void *c2 ) {
    const channel_t **ch2 = ( const channel_t ** ) c2;
    return strcmp( c1, ( *ch2 )->channel_name );
}                               // bsearch_channels

/**
 * TBD 
 * 
 * @param sockfd TBD  
 * @return TBD
 */
static channel_t *search_ch_by_sockfd( int sockfd ) {
    for ( int index = 0; index < nb_channels; index++ ) {
    	channel_t *ch = channels[index];
        if ( ch->sockfd == sockfd )
            return ch;
    }
    return NULL;
}                               // search_ch_by_sockfd

#if 0

/*
	pid means the PID of the server
*/
static inline channel_t *
search_ch_by_pid( messip_id_t id ) {
    for ( int index = 0; index < nb_channels; index++ ) {
        channel_t *ch = channels[index];
        if ( !strcmp( ch->id == id ) )
            return ch;
    }
    return NULL;
}                               // search_ch_by_pid
#endif

/**
 * TBD 
 * 
 * @param sockfd TBD
 * @return TBD
 */
inline static connexion_t *search_cnx_by_sockfd( int sockfd ) {
    for ( int index = 0; index < nb_connexions; index++ ) {
        connexion_t *cnx = connexions[index];
        if ( cnx->sockfd == sockfd )
            return cnx;
    }
    return NULL;
}                               // search_cnx_by_sockfd

/**
 * TBD 
 * 
 * @param sockfd TBD
 * @param iov TBD
 * @param iovcnt TBD
 * @return TBD
 */
static int do_writev( int sockfd, const struct iovec *iov, int iovcnt ) {
    int dcount;

    int sz = 0;
    int n;
    for ( n = 0; n < iovcnt; n++ )
        sz += iov[n].iov_len;

    int cnt = -1;
    errno = 0;
    for ( ;; ) {
        cnt++;
        dcount = writev( sockfd, iov, iovcnt );
        if ( ( dcount == -1 ) && ( errno == ECONNRESET ) )
            return dcount;
        if ( ( dcount == -1 ) && ( errno == EINTR ) )
            continue;
        break;
    }                           // for (;;)
    if ( cnt )
        logg( LOG_MESSIP_NON_FATAL_ERROR, "%s %d: %d\n", __FILE__, __LINE__, cnt );

    return dcount;
}                               // do_writev

/**
 * TBD 
 * 
 * @param sockfd TBD
 * @param iov TBD
 * @param iovcnt TBD
 * @return TBD
 */
static int do_readv( int sockfd, const struct iovec *iov, int iovcnt ) {
    int dcount;
    int cnt = -1;

    for ( ;; ) {
        cnt++;
        dcount = readv( sockfd, iov, iovcnt );
        if ( ( dcount == -1 ) && ( errno == ECONNRESET ) )
            return dcount;
        if ( ( dcount == -1 ) && ( errno == EINTR ) )
            continue;
        break;
    }                           // for (;;)
    if ( cnt )
        logg( LOG_MESSIP_NON_FATAL_ERROR, "%s %d: %d\n", __FILE__, __LINE__, cnt );

    return dcount;
}                               // do_readv

/**
 * TBD 
 */
static void debug_show( void ) {
    connexion_t *cnx;
    channel_t *ch;
    int index;
    char when[33];

    /*
     * Print out the active connexions
     */

    printf( "\n------------\n" );
    printf( "%d active connexion%s\n", nb_connexions, ( nb_connexions > 1 ) ? "s" : "" );
    if ( nb_connexions > 0 ) {
        printf( "         Pid Process             Tid Address       Port Socket Since              " );
        printf( "\n" );
        for ( index = 0; index < nb_connexions; index++ ) {
            cnx = connexions[index];
            strftime( when, 32, "%02d-%b-%y %02H:%02M:%02S", localtime( &cnx->when ) );
            printf( "%3d:%-8s %-12s %6d %d %-18s",
               index, cnx->id, inet_ntoa( cnx->xclient_addr.sin_addr ), cnx->xclient_addr.sin_port, cnx->sockfd, when );
            printf( " %d:", cnx->nb_cnx_channels );
            int k;
            for ( k = 0; k < cnx->nb_cnx_channels; k++ )
                printf( "%s%d", ( k ) ? "-" : "", cnx->sockfd_cnx_channels[k] );
            printf( "\n" );
        }                       // for
    }                           // if

    /*
     * Print out the active channels
     */

    printf( "\n%d active channels\n", nb_channels );
    if ( nb_channels > 0 ) {
        printf
           ( "          Pid Process             Tid Address       Port Name            Socket Created            Buffered Clients\n" );
        for ( index = 0; index < nb_channels; index++ ) {
            int k;
            char tmp[100];

            ch = channels[index];
            strftime( when, 32, "%02d-%b-%y %02H:%02M:%02S", localtime( &ch->when ) );
            strcpy( tmp, "" );
            for ( k = 0; k < ch->nb_clients; k++ )
                sprintf( &tmp[strlen( tmp )], "-%d", ch->cnx_clients[k] );
            printf( "%c%3d:%-8s %-12s %5d %-16s %5d %-18s %5d/%-5d %d%s\n",
               ( ch->f_notify_deaths ) ? 'D' : ' ',
               index,
               ch->id,
               ch->sin_addr_str,
               ch->sin_port,
               ch->channel_name, ch->sockfd, when, ch->nb_msg_buffered, ch->maxnb_msg_buffered, ch->nb_clients, tmp );
        }                       // for
    }                           // if

}                               // debug_show

/**
 * TBD 
 * 
 * @param arg TBD
 * @return TBD
 */
static void *debug_thread( void *arg ) {
    int sig;
    sigset_t set;

    sigemptyset( &set );
    sigaddset( &set, SIGUSR2 );
    pthread_sigmask( SIG_BLOCK, &set, NULL );

    /*--- Wait until SIGUSR1 signal ---*/
    fprintf( stdout, "For debugging: kill -s SIGUSR1 %d\n", getpid(  ) );
    for ( ;; ) {
        sigemptyset( &set );
        sigaddset( &set, SIGUSR1 );
        sigwait( &set, &sig );
        LOCK;
        debug_show(  );
        UNLOCK;
    }

    /*--- Never exit anyway ---*/
    return NULL;

}                               // debug_thread

/**
 * TBD 
 * 
 * @param msg TBD  
 * @param title TBD
 * @param tag TBD
 */
static void http_table_column_title( char *msg, char *title, char *tag ) {
    strcat( msg, "      <td valign=\"top\" align=\"center\" bgcolor=\"#c0c0c0\">" );
    strcat( msg, "<a href=\"" );
    strcat( msg, tag );
    strcat( msg, "\"></big><b>" );
    strcat( msg, "</big><b>" );
    strcat( msg, title );
    strcat( msg, "      </b></big></td>\n" );
}                               // http_table_column_title

/**
 * TBD 
 * 
 * @param buff TBD
 * @param val TBD
 */
static void http_table_column_add_int( char **buff, int val ) {
    char tmp[100];
    sprintf( tmp, "%d", val );
    *buff = strdup( tmp );
}                               // http_table_column_add_int

/**
 * TBD 
 * 
 * @param buff TBD
 * @param val TBD
 */
static void http_table_column_add_long( char **buff, long val ) {
    char tmp[100];
    sprintf( tmp, "%ld", val );
    *buff = strdup( tmp );
}                               // http_table_column_add_long

/**
 * TBD 
 * 
 * @param buff TBD
 * @param val TBD
 */
static void http_table_column_add_string( char **buff, char *val ) {
    char tmp[100];
    sprintf( tmp, "%s", val );
    *buff = strdup( tmp );
}                               // http_table_column_add_string

/**
 * TBD 
 * 
 * @param msg TBD  
 * @param val TBD
 */
static void http_table_column_add( char *msg, char *val ) {
    sprintf( &msg[strlen( msg )], "      <td valign=\"top\" align=\"right\" >%s<br></td>\n", val );
    free( val );
}                               // http_table_column_add

/**
 * TBD 
 * 
 * @param msg TBD  
 * @param key TBD
 */
static void http_build_table_connexions( char *msg, int key ) {
    connexion_t *cnx;
    int index;
    char when[33];
    typedef struct {
        char *index;
        char *id;
        char *address;
        char *port;
        char *socket;
        char *since;
    } column_t;
    column_t *tab, *p;

    /*--- Headers of the table ---*/
    strcat( msg, "<table cellpadding=\"2\" cellspacing=\"2\" border=\"1\" width=\"100%\">\n" "  <tbody>\n" "    <tr>\n" );
    http_table_column_title( msg, "Index", "/connexion/index" );
    http_table_column_title( msg, "Id", "/connexion/id" );
    http_table_column_title( msg, "Address", "/connexion/address" );
    http_table_column_title( msg, "Port", "/connexion/port" );
    http_table_column_title( msg, "Socket", "/connexion/socket" );
    http_table_column_title( msg, "Since", "/connexion/since" );
    strcat( msg, "    </tr>\n" );

    tab = malloc( sizeof( column_t ) * nb_connexions );
    for ( p = tab, index = 0; index < nb_connexions; index++, p++ ) {
        cnx = connexions[index];
        http_table_column_add_int( &p->index, index );
        http_table_column_add_string( &p->id, cnx->id );
        http_table_column_add_string( &p->address, inet_ntoa( cnx->xclient_addr.sin_addr ) );
        http_table_column_add_int( &p->port, cnx->xclient_addr.sin_port );
        http_table_column_add_int( &p->socket, cnx->sockfd );
        strftime( when, 32, "%02d-%b-%y %02H:%02M:%02S", localtime( &cnx->when ) );
        http_table_column_add_string( &p->since, when );
    }                           // http_build_table_connexions
    for ( p = tab, index = 0; index < nb_connexions; index++, p++ ) {
        strcat( msg, "    <tr>\n" );
        http_table_column_add( msg, p->index );
        http_table_column_add( msg, p->id );
        http_table_column_add( msg, p->address );
        http_table_column_add( msg, p->port );
        http_table_column_add( msg, p->socket );
        http_table_column_add( msg, p->since );
        strcat( msg, "    </tr>\n" );
    }
    free( tab );

    /*--- End of table ---*/
    strcat( msg, "  </tbody>\n" "</table>\n" );

}                               // http_build_table_connexions

/**
 * TBD 
 * 
 * @param msg TBD
 */
static void http_build_table_channels( char *msg ) {
    channel_t *ch;
    int index;
    char when[33];

    /*--- Headers of the table ---*/
    strcat( msg, "<table cellpadding=\"2\" cellspacing=\"2\" border=\"1\" width=\"100%\">\n" "  <tbody>\n" "    <tr>\n" );
    http_table_column_title( msg, "Index", "/channel/index" );
    http_table_column_title( msg, "Id", "/channel/id" );
    http_table_column_title( msg, "Address", "/channel/address" );
    http_table_column_title( msg, "Port", "/channel/port" );
    http_table_column_title( msg, "Name", "/channel/name" );
    http_table_column_title( msg, "Socket", "/channel/socket" );
    http_table_column_title( msg, "Created", "/channel/created" );
    http_table_column_title( msg, "Buffered", "/channel/buffered" );
    http_table_column_title( msg, "Clients", "/channel/clients" );
    strcat( msg, "    </tr>\n" );

    for ( index = 0; index < nb_channels; index++ ) {
        int k;
        char tmp[100];

        ch = channels[index];
        strftime( when, 32, "%02d-%b-%y %02H:%02M:%02S", localtime( &ch->when ) );
        strcpy( tmp, "" );
        for ( k = 0; k < ch->nb_clients; k++ )
            sprintf( &tmp[strlen( tmp )], "-%d", ch->cnx_clients[k] );
        sprintf( &msg[strlen( msg )], "    <tr>\n" "      <td valign=\"top\" align=\"right\" >%d<br>\n" // index
           "      </td>\n" "      <td valign=\"top\" align=\"right\" >%s<br>\n" // id
           "      </td>\n" "      <td valign=\"top\" align=\"right\" >%s<br>\n" // sin_addr_str
           "      </td>\n" "      <td valign=\"top\" align=\"right\" >%d<br>\n" // sin_port
           "      </td>\n" "      <td valign=\"top\" align=\"right\" >%s<br>\n" // name
           "      </td>\n" "      <td valign=\"top\" align=\"right\" >%d<br>\n" // sockfd
           "      </td>\n" "      <td valign=\"top\" align=\"right\" >%s<br>\n" // when
           "      </td>\n" "      <td valign=\"top\" align=\"right\" >%d/%d<br>\n"  // nb_msg_buffered, maxnb_msg_buffered
           "      </td>\n" "      <td valign=\"top\" align=\"right\" >%d%s<br>\n"   // nb_clients, tmp
           "      </td>\n" "    </tr>\n",
//      printf( "%c%3d:%8d %-12s %4ld %-12s %10ld %-16s %5d %-18s %5d/%-5d %d%s\n",
//         (ch->f_notify_deaths) ? 'D' : ' ',
           index,
           ch->id,
           ch->sin_addr_str,
           ch->sin_port,
           ch->channel_name, ch->sockfd, when, ch->nb_msg_buffered, ch->maxnb_msg_buffered, ch->nb_clients, tmp );
    }                           // for

    strcat( msg, "  </tbody>\n" "</table>\n" );

}                               // http_build_table_channels

/**
 * TBD 
 * 
 * @param sockfd TBD
 * @param version TBD
 * @param subversion TBD
 * @param key TBD
 * @return TBD
 */
static int http_send_status( int sockfd, int version, int subversion, int key ) {
    struct iovec iovec[1];
    char *msg1, *msg2;
    int dcount;

    /*--- Page header ---*/
    msg2 = malloc( 32768 );
    sprintf( msg2,
       "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
       "<html>\n"
       "<head>\n"
       "  <title>MessIP - Information</title>\n"
       "  <meta http-equiv=\"content-type\"\n" " content=\"text/html; charset=ISO-8859-1\">\n" "</head>\n" );
    strcat( msg2,
       "<b>MessIP</b> : Message Passing over TCP/IP<br>"
       "<a href=\"http://messip.sourceforge.net/\">http://messip.sourceforge.net/</a><br>" );
    sprintf( &msg2[strlen( msg2 )],
       "Version <b>%d.%dc</b> compiled on <b>%s %s</b><br>",
       VERSION_MAJOR, VERSION_MINOR, __DATE__, __TIME__ );
    strcat( msg2, "<hr width=\"100%\" size=\"2\"><br>" );

    /*--- Active connections ---*/
    sprintf( &msg2[strlen( msg2 )],
       "<body text=\"#000000\" bgcolor=\"#dddddd\" link=\"#000099\" vlink=\"#990099\" alink=\"#000099\">\n"
       "%d active connection%s:<br>\n", nb_connexions, ( nb_connexions > 1 ) ? "s" : "" );
    if ( nb_connexions > 0 )
        http_build_table_connexions( msg2, key );

    /*--- Active channels ---*/
    sprintf( &msg2[strlen( msg2 )], "<p>%d active channel%s:<br>\n", nb_channels, ( nb_channels > 1 ) ? "s" : "" );
    if ( nb_channels > 0 )
        http_build_table_channels( msg2 );

    /*--- End of page ---*/
    strcat( msg2, "<br>\n" "<br>\n" "</body>\n" "</html>\n" "\n" );

    /*--- Header to send ---*/
    msg1 = malloc( 1024 );
    sprintf( msg1,
       "HTTP/%d.%d 200 OK\r\n"
       "Content-type: text/html\r\n" "Content-Length: %d\r\n" "\r\n", version, subversion, strlen( msg2 ) );
    iovec[0].iov_base = msg1;
    iovec[0].iov_len = strlen( msg1 );
    dcount = do_writev( sockfd, iovec, 1 );
    free( msg1 );

    /*--- Send now the page ---*/
    iovec[0].iov_base = msg2;
    iovec[0].iov_len = strlen( msg2 );
    dcount = do_writev( sockfd, iovec, 1 );
    free( msg2 );

    /*--- Ok ---*/
    return 0;

}                               // http_send_status

typedef struct {
    int sockfd_accept;
    struct sockaddr_in client_addr;
    unsigned client_addr_len;
} clientdescr_t;

/**
 * TBD 
 * 
 * @param arg TBD
 * @return TBD
 */
static void *thread_http_thread( void *arg ) {
    clientdescr_t *descr = ( clientdescr_t * ) arg;
    ssize_t dcount;
    char request[500];
    struct iovec iovec[1];
    int status, version, subversion;
    char tag[200];
    int key;

    logg( LOG_MESSIP_NON_FATAL_ERROR, "thread_http_thread: pid=%d tid=%10ld\n", getpid(  ), pthread_self(  ) );

    for ( ;; ) {

        iovec[0].iov_base = request;
        iovec[0].iov_len = sizeof( request ) - 1;
        dcount = do_readv( descr->sockfd_accept, iovec, 1 );
        if ( dcount <= 0 )
            break;

        if ( dcount >= 15 ) {
            status = sscanf( request, "GET %s HTTP/%d.%d", tag, &version, &subversion );
            key = -1;
            if ( status == 3 ) {
                if ( !strcmp( tag, "/connexion/index" ) )
                    key = KEY_CONNEXION_INDEX;
                else if ( !strcmp( tag, "/connexion/id" ) )
                    key = KEY_CONNEXION_ID;
                else if ( !strcmp( tag, "/connexion/address" ) )
                    key = KEY_CONNEXION_ADDRESS;
                else if ( !strcmp( tag, "/connexion/port" ) )
                    key = KEY_CONNEXION_PORT;
                else if ( !strcmp( tag, "/connexion/socket" ) )
                    key = KEY_CONNEXION_SOCKET;
                else if ( !strcmp( tag, "/connexion/since" ) )
                    key = KEY_CONNEXION_SINCE;
                http_send_status( descr->sockfd_accept, version, subversion, key );
                shutdown( descr->sockfd_accept, SHUT_RDWR );    // Close the connection
                break;
            }
        }

        request[dcount] = 0;
        printf( "dcount=%d [%s]\n", dcount, request );

    }                           // for (;;)

    /*--- Done ---*/
    free( descr );
    pthread_exit( NULL );
    return NULL;
}                               // thread_http_thread

/**
 * TBD 
 * 
 * @param arg TBD  
 * @return TBD
 */
static void *http_thread( void *arg ) {
    sigset_t set;
    int sockfd;
    int status;
    struct sockaddr_in server_addr;

    sigemptyset( &set );
    sigaddset( &set, SIGUSR2 );
    pthread_sigmask( SIG_BLOCK, &set, NULL );

    /*--- Create socket ---*/
    sockfd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( sockfd < 0 ) {
        printf( "%s %d\n\tUnable to open a socket!\n", __FILE__, __LINE__ );
        fflush( stdout );
        exit( -1 );
    }

    /*--- Bind the socket ---*/
    memset( &server_addr, 0, sizeof( server_addr ) );
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl( INADDR_ANY );
    server_addr.sin_port = htons( messip_port_http );
    status = bind( sockfd, ( struct sockaddr * ) &server_addr, sizeof( struct sockaddr_in ) );
    if ( status == EADDRINUSE ) {
        fprintf( stderr, "%s %d\n\tUnable to bind, port %d is already in use\n", __FILE__, __LINE__, messip_port_http );
        if ( closesocket( sockfd ) == -1 )
            fprintf( stderr, "Error %d while closing socket %d\n", errno, sockfd );
        exit( -1 );
    }
    if ( status < 0 ) {
        fprintf( stderr, "%s %d\n\tUnable to bind - port %d - status=%d errno=%s\n",
           __FILE__, __LINE__, messip_port_http, status, strerror( errno ) );
        if ( closesocket( sockfd ) == -1 )
            fprintf( stderr, "Error %d while closing socket %d\n", errno, sockfd );
        exit( -1 );
    }

    listen( sockfd, 8 );

    for ( ;; ) {
        clientdescr_t *descr;
        pthread_t tid;
        pthread_attr_t attr;

        descr = malloc( sizeof( clientdescr_t ) );
        descr->client_addr_len = sizeof( struct sockaddr_in );
        descr->sockfd_accept = accept( sockfd, ( struct sockaddr * ) &descr->client_addr, &descr->client_addr_len );
        if ( descr->sockfd_accept == -1 ) {
            if ( errno == EINTR )   // A signal has been applied
                continue;
            fprintf( stderr, "Socket non accepted - errno=%d\n", errno );
            if ( closesocket( sockfd ) == -1 )
                fprintf( stderr, "Error %d while closing socket %d\n", errno, sockfd );
            exit( -1 );
        }
#if 1
        logg( LOG_MESSIP_DEBUG_LEVEL1, "http: accepted a msg from %s, port=%d, socket=%d\n",
           inet_ntoa( descr->client_addr.sin_addr ), descr->client_addr.sin_port, descr->sockfd_accept );
#endif
        pthread_attr_init( &attr );
        pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );
        pthread_create( &tid, &attr, &thread_http_thread, descr );
    }                           // for (;;)

    /*--- Never exit anyway ---*/
    if ( closesocket( sockfd ) == -1 )
        fprintf( stderr, "Error %d while closing socket %d\n", errno, sockfd );
    return NULL;

}                               // http_thread

/**
 * TBD 
 * 
 * @param sockfd TBD  
 * @param client_addr TBD
 * @param new_cnx TBD
 * @return TBD
 */
static int handle_client_connect( int sockfd, struct sockaddr_in *client_addr, connexion_t ** new_cnx ) {
    struct iovec iovec[1];
    messip_send_connect_t msg;
    messip_reply_connect_t reply;
    connexion_t *cnx;
    ssize_t dcount;

    /*--- Read additional data specific to this message ---*/
    iovec[0].iov_base = &msg;
    iovec[0].iov_len = sizeof( msg );
    dcount = do_readv( sockfd, iovec, 1 );
    if ( dcount == -1 ) {
        fprintf( stderr, "%s %d: errno=%d\n", __FILE__, __LINE__, errno );
        return -1;
    }
    if ( dcount != sizeof( msg ) ) {
        fprintf( stderr, "%s %d\n\tread %d of %d - errno=%d\n", __FILE__, __LINE__, dcount, sizeof( msg ), errno );
        return -1;
    }

    /*--- Allocate a new connexion ---*/
    cnx = malloc( sizeof( connexion_t ) );
    time( &cnx->when );
    IDCPY( cnx->id, msg.id );
    memmove( &cnx->xclient_addr, client_addr, sizeof( struct sockaddr_in ) );
    cnx->nb_cnx_channels = 0;
    cnx->sockfd_cnx_channels = NULL;
    cnx->sockfd = sockfd;
    LOCK;
    if ( connexions == NULL )
        connexions = malloc( sizeof( connexion_t * ) );
    else
        connexions = realloc( connexions, ( nb_connexions + 1 ) * sizeof( connexion_t * ) );
    connexions[nb_connexions++] = cnx;
    UNLOCK;
    *new_cnx = cnx;

#if 0
    logg( LOG_MESSIP_DEBUG_LEVEL1, "handle_msg_connect: pid=%d[%X] tid=%ld ip=%s port=%d\n",
       cnx->pid, cnx->pid, cnx->tid, inet_ntoa( client_addr->sin_addr ), client_addr->sin_port );
#endif

    /*--- Reply to the client ---*/
    reply.ok = MESSIP_OK;
    iovec[0].iov_base = &reply;
    iovec[0].iov_len = sizeof( reply );
    dcount = do_writev( sockfd, iovec, 1 );
    assert( dcount == sizeof( reply ) );

    return 0;

}                               // handle_client_connect

/**
 * TBD 
 * 
 * @param sockfd TBD  
 * @param client_addr TBD
 * @param cnx TBD
 * @return TBD
 */
static int client_channel_create( int sockfd, struct sockaddr_in *client_addr, connexion_t ** cnx ) {
    channel_t **pch, *ch;
    struct iovec iovec[1];
    messip_send_channel_create_t msg;
    messip_reply_channel_create_t reply;
    ssize_t dcount;

    /*--- Read additional data specific to this message ---*/
    iovec[0].iov_base = &msg;
    iovec[0].iov_len = sizeof( msg );
    dcount = do_readv( sockfd, iovec, 1 );
    if ( dcount == -1 ) {
        fprintf( stderr, "%s %d: errno=%d\n", __FILE__, __LINE__, errno );
        return -1;
    }
    if ( dcount != sizeof( msg ) ) {
        fprintf( stderr, "%s %d: read %d of %d - errno=%d\n", __FILE__, __LINE__, dcount, sizeof( msg ), errno );
        return -1;
    }

    logg( LOG_MESSIP_INFORMATIVE, "channel_create: id=%s ip=%s port=%d name=%s\n",
       msg.id, inet_ntoa( client_addr->sin_addr ), client_addr->sin_port, msg.channel_name );

    /*--- Is there any channel with this name ? ---*/
    LOCK;
    pch = bsearch( msg.channel_name, channels, nb_channels, sizeof( channel_t * ), bsearch_channels );
    if ( pch != NULL ) {

        UNLOCK;
        reply.ok = MESSIP_NOK;

    }                           // if
    else {

        /*--- Allocate a new channel ---*/
        if ( channels == NULL )
            channels = malloc( sizeof( channel_t * ) );
        else
            channels = realloc( channels, ( nb_channels + 1 ) * sizeof( channel_t * ) );
        ch = malloc( sizeof( channel_t ) );
        channels[nb_channels++] = ch;

        /*--- Create a new channel ---*/
        ch->cnx = *cnx;
        ch->when = time( NULL );
        IDCPY( ch->id, msg.id );
        ch->maxnb_msg_buffered = msg.maxnb_msg_buffered;
        ch->sockfd = sockfd;
        ch->tid_client_send_buffered_msg = 0;
        ch->bufferedsend_sockfd = 0;
        ch->nb_msg_buffered = 0;
        ch->buffered_msg = NULL;
        ch->nb_clients = 0;
        ch->cnx_clients = NULL;
        ch->f_notify_deaths = MESSIP_FALSE; // Send a Msg on the death of each process
        strncpy( ch->channel_name, msg.channel_name, MESSIP_CHANNEL_NAME_MAXLEN );
        ch->channel_name[MESSIP_CHANNEL_NAME_MAXLEN] = 0;
        ch->sin_port = msg.sin_port;
        ch->sin_addr = client_addr->sin_addr.s_addr;
        strcpy( ch->sin_addr_str, inet_ntoa( client_addr->sin_addr ) );

        /*--- Keep the channels sorted ---*/
        qsort( channels, nb_channels, sizeof( channel_t * ), qsort_channels );
        reply.ok = MESSIP_OK;
        reply.sin_port = ch->sin_port;
        reply.sin_addr = ch->sin_addr;
        strcpy( reply.sin_addr_str, ch->sin_addr_str );

        UNLOCK;
    }                           // else

    /*--- Reply to the client ---*/
    iovec[0].iov_base = &reply;
    iovec[0].iov_len = sizeof( reply );
    dcount = do_writev( sockfd, iovec, 1 );
    assert( dcount == sizeof( reply ) );

    return 0;

}                               // client_channel_create

/**
 * TBD 
 * 
 * @param ch TBD
 * @param  index TBD
 */
static void destroy_channel( channel_t * ch, int index ) {
    int k;

#if 0
    logg( LOG_MESSIP_NON_FATAL_ERROR, "Destroy channel %d [%s] index=%d\n", index, ch->name, index );
#endif

    if ( ch->tid_client_send_buffered_msg ) {
        pthread_cancel( ch->tid_client_send_buffered_msg );
        if ( closesocket( ch->bufferedsend_sockfd ) == -1 )
            fprintf( stderr, "Error %d while closing socket %d\n", errno, ch->bufferedsend_sockfd );
    }                           // if

    if ( ch->nb_msg_buffered > 0 ) {
        for ( k = 0; k < ch->nb_msg_buffered; k++ )
            free( ch->buffered_msg[k] );
        free( ch->buffered_msg );
    }                           // if

    if ( index == -1 ) {
        for ( k = 0; k < nb_channels; k++ ) {
            if ( channels[k] == ch ) {
                index = k;
                break;
            }
        }
        assert( index != -1 );
    }                           // if

    for ( k = index + 1; k < nb_channels; k++ )
        channels[k - 1] = channels[k];

    nb_channels--;

}                               // destroy_channel

/**
 * TBD 
 * 
 * @param sockfd TBD  
 * @param client_addr TBD
 * @return TBD
 */
static int client_channel_delete( int sockfd, struct sockaddr_in *client_addr ) {
    channel_t **pch, *ch;
    struct iovec iovec[1];
    messip_send_channel_delete_t msg;
    messip_reply_channel_delete_t reply;
    ssize_t dcount;

    /*--- Read additional data specific to this message ---*/
    iovec[0].iov_base = &msg;
    iovec[0].iov_len = sizeof( msg );
    dcount = do_readv( sockfd, iovec, 1 );
    if ( dcount == -1 ) {
        fprintf( stderr, "%s %d: errno=%d\n", __FILE__, __LINE__, errno );
        return -1;
    }
    if ( dcount != sizeof( messip_send_channel_delete_t ) ) {
        fprintf( stderr, "%s %d: read %d of %d - errno=%d\n",
           __FILE__, __LINE__, dcount, sizeof( messip_send_channel_delete_t ), errno );
        return -1;
    }

#if 0
    logg( LOG_MESSIP_NON_FATAL_ERROR, "channel_delete: pid=%d tid=%ld name=%s\n", msg.pid, msg.tid, msg.name );
#endif

    /*--- Search this channel name ---*/
    LOCK;
    pch = bsearch( msg.name, channels, nb_channels, sizeof( channel_t * ), bsearch_channels );
    ch = ( pch ) ? *pch : NULL;
    UNLOCK;

    /*--- Reply to the client ---*/
    if ( ch == NULL ) {
        reply.nb_clients = -1;
    }
    else {
        LOCK;
        if ( strcmp( ch->id, msg.id ) ) {
            reply.nb_clients = -1;
        }                       // if
        else {
            if ( ch->nb_clients > 0 ) {
                reply.nb_clients = ch->nb_clients;
            }
            else {
                destroy_channel( ch, -1 );
                reply.nb_clients = 0;
            }
        }                       // else
        UNLOCK;
    }                           // else
    iovec[0].iov_base = &reply;
    iovec[0].iov_len = sizeof( reply );
    dcount = do_writev( sockfd, iovec, 1 );
    assert( dcount == sizeof( reply ) );

    return MESSIP_OK;

}                               // client_channel_delete

/**
 * TBD 
 * 
 * @param sockfd TBD
 * @param client_addr TBD
 * @return TBD
 */
static int client_channel_connect( int sockfd, struct sockaddr_in *client_addr ) {
    channel_t **pch, *ch;
    struct iovec iovec[1];
    messip_send_channel_connect_t msg;
    messip_reply_channel_connect_t reply;
    ssize_t dcount;
    int k;

    /*--- Read additional data specific to this message ---*/
    iovec[0].iov_base = &msg;
    iovec[0].iov_len = sizeof( msg );
    dcount = do_readv( sockfd, iovec, 1 );
    if ( dcount == -1 ) {
        fprintf( stderr, "%s %d: errno=%d\n", __FILE__, __LINE__, errno );
        return -1;
    }
    if ( dcount != sizeof( messip_send_channel_connect_t ) ) {
        fprintf( stderr, "%s %d: read %d of %d - errno=%d\n",
           __FILE__, __LINE__, dcount, sizeof( messip_send_channel_connect_t ), errno );
        return -1;
    }

#if 0
    logg( LOG_MESSIP_NON_FATAL_ERROR, "channel_connect: pid=%d tid=%ld name=%s\n", msg.pid, msg.tid, msg.name );
#endif

    /*--- Search this channel name ---*/
    LOCK;
    pch = bsearch( msg.name, channels, nb_channels, sizeof( channel_t * ), bsearch_channels );
    ch = ( pch ) ? *pch : NULL;
    UNLOCK;

    /*--- Reply to the client ---*/
    if ( ch == NULL ) {
        reply.ok = MESSIP_NOK;
    }
    else {

        /*--- Is this client already connected ? ---*/
        for ( reply.f_already_connected = 0, k = 0; k < ch->nb_clients; k++ ) {
            if ( ch->cnx_clients[k] == sockfd ) {
                reply.f_already_connected = 1;
                break;
            }
        }

        reply.ok = MESSIP_OK;
        IDCPY( reply.id, ch->id );
        reply.sin_port = ch->sin_port;
        reply.sin_addr = ch->sin_addr;
        memmove( reply.sin_addr_str, ch->sin_addr_str, sizeof( reply.sin_addr_str ) );
        reply.mgr_sockfd = ch->sockfd;
    }
    iovec[0].iov_base = &reply;
    iovec[0].iov_len = sizeof( reply );
    dcount = do_writev( sockfd, iovec, 1 );
    assert( dcount == sizeof( reply ) );

    /*--- Maintain a list of clients connected to this connection ---*/
    if ( ch ) {
        if ( !reply.f_already_connected ) {
            LOCK;

            if ( ch->nb_clients == 0 )
                ch->cnx_clients = malloc( sizeof( int ) );
            else
                ch->cnx_clients = realloc( ch->cnx_clients, ( ch->nb_clients + 1 ) * sizeof( int ) );
            ch->cnx_clients[ch->nb_clients++] = sockfd;

            connexion_t *cnx = search_cnx_by_sockfd( sockfd );
            if ( cnx->nb_cnx_channels == 0 )
                cnx->sockfd_cnx_channels = malloc( sizeof( int ) );
            else
                cnx->sockfd_cnx_channels = realloc( cnx->sockfd_cnx_channels, ( cnx->nb_cnx_channels + 1 ) * sizeof( int ) );
            cnx->sockfd_cnx_channels[cnx->nb_cnx_channels++] = ch->sockfd;

            UNLOCK;

        }                       // if
    }                           // if

    return MESSIP_OK;

}                               // client_channel_connect

/**
 * TBD 
 * 
 * @param sockfd TBD
 * @param client_addr TBD
 * @return TBD
 */
static int client_channel_disconnect( int sockfd, struct sockaddr_in *client_addr ) {
    channel_t **pch, *ch;
    struct iovec iovec[1];
    messip_send_channel_disconnect_t msg;
    messip_reply_channel_disconnect_t reply;
    ssize_t dcount;
    int index, k;

    /*--- Read additional data specific to this message ---*/
    iovec[0].iov_base = &msg;
    iovec[0].iov_len = sizeof( msg );
    dcount = do_readv( sockfd, iovec, 1 );
    if ( dcount == -1 ) {
        fprintf( stderr, "%s %d: errno=%d\n", __FILE__, __LINE__, errno );
        return -1;
    }
    if ( dcount != sizeof( messip_send_channel_disconnect_t ) ) {
        fprintf( stderr, "%s %d: read %d of %d - errno=%d\n",
           __FILE__, __LINE__, dcount, sizeof( messip_send_channel_disconnect_t ), errno );
        return -1;
    }

#if 0
    logg( LOG_MESSIP_NON_FATAL_ERROR, "channel_disconnect: pid=%d tid=%ld name=%s  sockfd=%d\n",
       msg.pid, msg.tid, msg.name, sockfd );
#endif

    /*--- Search this channel name ---*/
    LOCK;
    pch = bsearch( msg.name, channels, nb_channels, sizeof( channel_t * ), bsearch_channels );
    ch = ( pch ) ? *pch : NULL;
    UNLOCK;

    /*--- Reply to the client ---*/
    if ( ch == NULL ) {
        reply.ok = MESSIP_NOK;
    }
    else {
        LOCK;
        for ( index = 0; index < nb_channels; index++ ) {
            int nb, new_nb;
            channel_t *channel;

            channel = channels[index];
            for ( nb = 0, k = 0; k < channel->nb_clients; k++ )
                if ( channel->cnx_clients[k] == sockfd )
                    nb++;
            new_nb = channel->nb_clients - nb;
            if ( new_nb == 0 ) {
                channel->nb_clients = 0;
                free( channel->cnx_clients );
                channel->cnx_clients = NULL;
            }
            else {
                int w, *clients;
                clients = malloc( sizeof( int ) * new_nb );
                for ( w = 0, k = 0; k < channel->nb_clients; k++ ) {
                    if ( channel->cnx_clients[k] != sockfd )
                        clients[w++] = channel->cnx_clients[k];
                }               // for (k)
                assert( w == new_nb );
                channel->nb_clients = new_nb;
                free( channel->cnx_clients );
                channel->cnx_clients = clients;
            }
        }                       // for (ch)
        reply.ok = MESSIP_OK;
        UNLOCK;
    }                           // else
    iovec[0].iov_base = &reply;
    iovec[0].iov_len = sizeof( reply );
    dcount = do_writev( sockfd, iovec, 1 );
    assert( dcount == sizeof( reply ) );

    return MESSIP_OK;

}                               // client_channel_disconnect

/**
 * TBD 
 * 
 * @param arg TBD
 * @return TBD
 */
static void *thread_client_send_buffered_msg( void *arg ) {
    channel_t *ch = ( channel_t * ) arg;
    messip_datasend_t datasend;
    messip_datareply_t datareply;
    struct iovec iovec[3];
    buffered_msg_t *bmsg;
    int status;
    ssize_t dcount;
    uint32_t len;
    fd_set ready;
    int sockfd;
    int nb, k;
    int do_reply;
    int sig;
    sigset_t set;

    logg( LOG_MESSIP_NON_FATAL_ERROR, "thread_client_send_buffered_msg: pid=%d tid=%ld\n", getpid(  ), pthread_self(  ) );
    for ( ;; ) {

        /*--- Wait until SIGUSR2 is applied to the thread ---*/
        sigemptyset( &set );
        sigaddset( &set, SIGUSR2 );
        sigwait( &set, &sig );

        LOCK;
        nb = ch->nb_msg_buffered;
        sockfd = ch->bufferedsend_sockfd;
        UNLOCK;

        while ( nb > 0 ) {

            /*--- Wait until ready to write ---*/
            FD_ZERO( &ready );
            FD_SET( sockfd, &ready );
            status = select( FD_SETSIZE, NULL, &ready, NULL, NULL );

            LOCK;
            if ( ch->nb_msg_buffered == 0 ) {
                UNLOCK;
                break;
            }
            bmsg = ch->buffered_msg[0];

            /*--- Message to send ---*/
            datasend.flag = MESSIP_FLAG_BUFFERED;
            IDCPY( datasend.id, bmsg->id_from );
            datasend.type = bmsg->type;
            datasend.datalen = bmsg->datalen;
            UNLOCK;

            /*--- Send a message to the 'server' ---*/
            iovec[0].iov_base = &datasend;
            iovec[0].iov_len = sizeof( datasend );
            iovec[1].iov_base = &len;
            iovec[1].iov_len = sizeof( int32_t );
            iovec[2].iov_base = bmsg->data;
            iovec[2].iov_len = bmsg->datalen;
            dcount = do_writev( sockfd, iovec, 3 );
            if ( dcount != sizeof( datasend ) + bmsg->datalen + sizeof( int32_t ) ) {
                printf( "dcount=%d expected=%d datalen=%d\n",
                   dcount, sizeof( datasend ) + bmsg->datalen + sizeof( int32_t ), bmsg->datalen );
            }
            assert( dcount == sizeof( datasend ) + bmsg->datalen + sizeof( int32_t ) );

            /*--- Now wait for an answer from the server ---*/
            errno = -1;
            iovec[0].iov_base = &datareply;
            iovec[0].iov_len = sizeof( datareply );
            dcount = do_readv( sockfd, iovec, 1 );

            /*--- Clean-up this message ---*/
            LOCK;
            if ( bmsg->data )
                free( bmsg->data );
            free( bmsg );
            if ( --ch->nb_msg_buffered == 0 ) {
                free( ch->buffered_msg );
                ch->buffered_msg = NULL;
            }
            else {
                for ( k = 1; k <= ch->nb_msg_buffered; k++ )
                    ch->buffered_msg[k - 1] = ch->buffered_msg[k];
                ch->buffered_msg = realloc( ch->buffered_msg, sizeof( buffered_msg_t * ) * ch->nb_msg_buffered );
            }

            nb = ch->nb_msg_buffered;
            do_reply = ( nb + 1 == ch->maxnb_msg_buffered );
            UNLOCK;

            if ( ( dcount <= 0 ) && ( errno == ECONNRESET ) )
                continue;
            assert( dcount == sizeof( messip_datareply_t ) );

            /*--- Reply to the server, which was blocked because too many buffered messages ---*/
            if ( do_reply ) {
                messip_reply_buffered_send_t msgreply;

                msgreply.ok = MESSIP_OK;
                msgreply.nb_msg_buffered = nb;
                iovec[0].iov_base = &msgreply;
                iovec[0].iov_len = sizeof( msgreply );
                dcount = do_writev( ch->reply_on_release_sockfd, iovec, 1 );
                assert( dcount == sizeof( msgreply ) );
            }                   // if

        }                       // while (nb > 0)

    }                           // for (;;)

    /*--- Done ---*/
    pthread_exit( NULL );
    return NULL;

}                               // thread_client_send_buffered_msg

/**
 * TBD 
 * 
 * @param sockfd TBD
 * @param client_addr TBD
 * @return TBD
 */
static int client_death_notify( int sockfd, struct sockaddr_in *client_addr ) {
    ssize_t dcount;
    channel_t *ch;
    struct iovec iovec[1];
    messip_send_death_notify_t msgsend;
    messip_reply_death_notify_t msgreply;
    connexion_t *cnx;

    /*--- Read additional data specific to this message ---*/
    iovec[0].iov_base = &msgsend;
    iovec[0].iov_len = sizeof( msgsend );
    dcount = do_readv( sockfd, iovec, 1 );
    if ( dcount == -1 ) {
        fprintf( stderr, "%s %d: errno=%d\n", __FILE__, __LINE__, errno );
        return -1;
    }
    if ( dcount != sizeof( msgsend ) ) {
        fprintf( stderr, "%s %d: read %d of %d - errno=%d\n", __FILE__, __LINE__, dcount, sizeof( msgsend ), errno );
        return -1;
    }

#if 0
    logg( LOG_MESSIP_NON_FATAL_ERROR, "client_death_notify: pid=%d tid=%ld status=%d\n",
       msgsend.pid_from, msgsend.tid_from, msgsend.status );
#endif

    /*--- Update internal data ---*/
    LOCK;
    ch = search_ch_by_sockfd( sockfd );
    assert( ch != NULL );
    ch->f_notify_deaths = msgsend.status;
    cnx = ch->cnx;
    UNLOCK;

    /*--- Reply to the client ---*/
    msgreply.ok = MESSIP_OK;
    iovec[0].iov_base = &msgreply;
    iovec[0].iov_len = sizeof( msgreply );
    dcount = do_writev( sockfd, iovec, 1 );
    assert( dcount == sizeof( msgreply ) );

    return 0;

}                               // client_death_notify

/**
 * TBD 
 * 
 * @param sockfd TBD  
 * @param client_addr TBD
 * @return TBD
 */
static int client_buffered_send( int sockfd, struct sockaddr_in *client_addr ) {
    ssize_t dcount;
    channel_t *ch;
    buffered_msg_t *bmsg;
    connexion_t *cnx;
    pthread_attr_t attr;
    void *data;
    struct iovec iovec[1];
    messip_send_buffered_send_t msg;
    messip_reply_buffered_send_t msgreply;
    int nb;
    int do_reply;
    struct sockaddr_in sockaddr;

    /*--- Read additional data specific to this message ---*/
    iovec[0].iov_base = &msg;
    iovec[0].iov_len = sizeof( msg );
    dcount = do_readv( sockfd, iovec, 1 );
    if ( dcount == -1 ) {
        fprintf( stderr, "%s %d: errno=%d\n", __FILE__, __LINE__, errno );
        return -1;
    }
    if ( dcount != sizeof( msg ) ) {
        fprintf( stderr, "%s %d: read %d of %d - errno=%d\n", __FILE__, __LINE__, dcount, sizeof( msg ), errno );
        return -1;
    }

    /*--- Read the private message ---*/
    if ( msg.datalen == 0 ) {
        data = NULL;
    }
    else {
        data = malloc( msg.datalen );
        dcount = read( sockfd, data, msg.datalen );
        if ( dcount != msg.datalen ) {
            fprintf( stderr, "Should have read %d bytes - only %d have been read\n", msg.datalen, dcount );
            return -1;
        }
    }

#if 0
    logg( "client_buffered_send: pid=%d tid=%d type=%d %d [%s]\n", msg.pid_from, msg.tid_from, msg.type, msg.datalen, data );
#endif

    /*--- Create a thread to manage buffered messages ? ---*/
    LOCK;
    ch = search_ch_by_sockfd( msg.mgr_sockfd );
    if ( ch == NULL ) {
        UNLOCK;
        fprintf( stderr, "%s: socket %d not found\n", __FUNCTION__, msg.mgr_sockfd );
        return -1;
    }
    cnx = ch->cnx;

    /*--- Create socket then connection ---*/
    if ( ch->bufferedsend_sockfd == 0 ) {
        messip_datasend_t datasend;
        struct iovec iovec[1];
        ssize_t dcount;

        ch->bufferedsend_sockfd = socket( AF_INET, SOCK_STREAM, 0 );
        if ( ch->bufferedsend_sockfd < 0 ) {
            UNLOCK;
            fprintf( stderr, "%s %d\n\tUnable to open a socket!\n", __FILE__, __LINE__ );
            return -1;
        }

        /*--- Connect socket using name specified ---*/
        memset( &sockaddr, 0, sizeof( sockaddr ) );
        sockaddr.sin_family = AF_INET;
        sockaddr.sin_port = htons( ch->sin_port );
        sockaddr.sin_addr.s_addr = ch->sin_addr;
        if ( connect( ch->bufferedsend_sockfd, ( const struct sockaddr * ) &sockaddr, sizeof( sockaddr ) ) < 0 ) {
            UNLOCK;
            fprintf( stderr, "%s %d\n\tUnable to connect to host %s, port %d - errno=%d\n",
               __FILE__, __LINE__, inet_ntoa( sockaddr.sin_addr ), sockaddr.sin_port, errno );
            if ( closesocket( ch->bufferedsend_sockfd ) == -1 )
                fprintf( stderr, "Error %d while closing socket %d\n", errno, ch->bufferedsend_sockfd );
            return -1;
        }

        /*--- Send a fake message ---*/
        datasend.flag = MESSIP_FLAG_CONNECTING;
        iovec[0].iov_base = &datasend;
        iovec[0].iov_len = sizeof( datasend );
        dcount = do_writev( ch->bufferedsend_sockfd, iovec, 1 );
        assert( dcount == sizeof( datasend ) );

    }                           // if

    /*--- Update internal queue, managed by the thread client_send_buffered_msg ---*/
    bmsg = malloc( sizeof( buffered_msg_t ) );
    bmsg->type = msg.type;
    IDCPY( bmsg->id_from, msg.id_from );
    IDCPY( bmsg->id_to, cnx->id );
    bmsg->datalen = msg.datalen;
    bmsg->data = data;
    if ( ( nb = ch->nb_msg_buffered ) == 0 )
        ch->buffered_msg = malloc( sizeof( buffered_msg_t * ) );
    else
        ch->buffered_msg = realloc( ch->buffered_msg, sizeof( buffered_msg_t * ) * ( ch->nb_msg_buffered + 1 ) );
    ch->buffered_msg[ch->nb_msg_buffered++] = bmsg;
    do_reply = ( ch->nb_msg_buffered < ch->maxnb_msg_buffered );
    UNLOCK;

    if ( ch->tid_client_send_buffered_msg == 0 ) {
        pthread_attr_init( &attr );
        pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );
        pthread_create( &ch->tid_client_send_buffered_msg, &attr, thread_client_send_buffered_msg, ch );
    }                           // if

    /*--- Signal the buffered msg send thread that there is something ---*/
    pthread_kill( ch->tid_client_send_buffered_msg, SIGUSR2 );

    /*--- Reply to the client ---*/
    if ( do_reply ) {
        msgreply.ok = MESSIP_OK;
        msgreply.nb_msg_buffered = nb;
        iovec[0].iov_base = &msgreply;
        iovec[0].iov_len = sizeof( msgreply );
        dcount = do_writev( sockfd, iovec, 1 );
        assert( dcount == sizeof( msgreply ) );
    }                           // if
    else {
        ch->reply_on_release_sockfd = sockfd;
    }

    return 0;

}                               // client_buffered_send

/**
 * TBD 
 * 
 * @param sockfd TBD
 * @param client_addr TBD
 * @param op TBD
 * @param new_cnx TBD
 * @return TBD
 */
static int handle_client_msg( int sockfd, struct sockaddr_in *client_addr, int32_t op, connexion_t ** new_cnx ) {

    switch ( op ) {

        case MESSIP_OP_CONNECT:
            handle_client_connect( sockfd, client_addr, new_cnx );
            return 1;

        case MESSIP_OP_CHANNEL_CREATE:
            client_channel_create( sockfd, client_addr, new_cnx );
            return 1;

        case MESSIP_OP_CHANNEL_DELETE:
            client_channel_delete( sockfd, client_addr );
            return 1;

        case MESSIP_OP_CHANNEL_CONNECT:
            client_channel_connect( sockfd, client_addr );
            return 1;

        case MESSIP_OP_CHANNEL_DISCONNECT:
            client_channel_disconnect( sockfd, client_addr );
            return 1;

        case MESSIP_OP_BUFFERED_SEND:
            client_buffered_send( sockfd, client_addr );
            return 1;

        case MESSIP_OP_DEATH_NOTIFY:
            client_death_notify( sockfd, client_addr );
            return 1;

        case MESSIP_OP_SIN:
            LOCK;
            debug_show(  );
            UNLOCK;
            return 0;

        default:
            fprintf( stderr, "%s %d:\n\tUnknown code op %d - 0x%08X\n", __FILE__, __LINE__, op, op );
            break;

    }                           // switch (op)

    return 0;
}                               // handle_client_msg

/**
 * TBD 
 * 
 * @param ch TBD
 * @param id TBD
 * @param code TBD
 * @return TBD
 */
static int notify_server_death_client( channel_t * ch, messip_id_t id, int code ) {
    messip_datasend_t datasend;
    int sockfd;
    struct iovec iovec[1];
    int status;
    ssize_t dcount;
    fd_set ready;
    struct sockaddr_in sockaddr;

    sockfd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( sockfd < 0 ) {
        fprintf( stderr, "%s %d\n\tUnable to open a socket!\n", __FILE__, __LINE__ );
        return -1;
    }

    /*--- Connect socket using name specified ---*/
    memset( &sockaddr, 0, sizeof( sockaddr ) );
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons( ch->sin_port );
    sockaddr.sin_addr.s_addr = ch->sin_addr;
    if ( connect( sockfd, ( const struct sockaddr * ) &sockaddr, sizeof( sockaddr ) ) < 0 ) {
        if ( errno != ECONNREFUSED )
            fprintf( stderr, "%s %d\n\tUnable to connect to host %s, port %d - errno=%d\n",
               __FILE__, __LINE__, inet_ntoa( sockaddr.sin_addr ), sockaddr.sin_port, errno );
        if ( closesocket( sockfd ) == -1 )
            fprintf( stderr, "Error %d while closing socket %d\n", errno, sockfd );
        return -1;
    }

    /*--- Wait until enable to write ---*/
    FD_ZERO( &ready );
    FD_SET( sockfd, &ready );
    status = select( FD_SETSIZE, NULL, &ready, NULL, NULL );

    /*--- Send a fake message ---*/
    memset( &datasend, 0, sizeof( messip_datasend_t ) );
    datasend.flag = MESSIP_FLAG_CONNECTING;
    iovec[0].iov_base = &datasend;
    iovec[0].iov_len = sizeof( datasend );
    dcount = do_writev( sockfd, iovec, 1 );
    assert( dcount == sizeof( datasend ) );

    /*--- Message to send ---*/
    datasend.flag = code;
    IDCPY( datasend.id, id );
    datasend.type = -1;
    datasend.datalen = 0;

    /*--- Send a message to the 'server' ---*/
    iovec[0].iov_base = &datasend;
    iovec[0].iov_len = sizeof( datasend );
    dcount = do_writev( sockfd, iovec, 1 );
//  assert( dcount == sizeof( datasend ) );

    /*--- ok ---*/
    if ( closesocket( sockfd ) == -1 )
        fprintf( stderr, "Error %d while closing socket %d\n", errno, sockfd );
    return 0;

}                               // notify_server_death_client

/**
 * TBD 
 * 
 * @param arg TBD
 * @return TBD
 */
static void *thread_client_thread( void *arg ) {
    clientdescr_t *descr = ( clientdescr_t * ) arg;
    ssize_t dcount;
    int32_t op;
    int index, found;
    connexion_t *new_cnx, **cnx, *connexion;
    channel_t **ch, *channel;
    int k;
    struct iovec iovec[1];
    messip_id_t id;
    int search_socket = 0;

#if 0
    logg( LOG_MESSIP_NON_FATAL_ERROR, "thread_client_thread: pid=%d tid=%ld\n", getpid(  ), pthread_self(  ) );
#endif

    for ( new_cnx = NULL;; ) {

        iovec[0].iov_base = &op;
        iovec[0].iov_len = sizeof( int32_t );
        dcount = do_readv( descr->sockfd_accept, iovec, 1 );
        if ( dcount <= 0 )
            break;

        if ( dcount != sizeof( int32_t ) ) {
            fprintf( stderr, "%s %d:\n\tread %d byte[%08X], should have read %d bytes\n",
               __FILE__, __LINE__, dcount, op, sizeof( int32_t ) );
            break;
        }

        search_socket = handle_client_msg( descr->sockfd_accept, &descr->client_addr, op, &new_cnx );

    }                           // for (;;)

    /*--- Close the connection ---*/
    shutdown( descr->sockfd_accept, SHUT_RDWR );

    if ( !search_socket ) {
        if ( close( descr->sockfd_accept ) == -1 )
            fprintf( stderr, "Error %d while closing socket %d\n", errno, descr->sockfd_accept );
        free( descr );
        pthread_exit( NULL );
        return NULL;
    }

    /*--- Destroy this connection ---*/
    LOCK;
    for ( cnx = connexions, found = 0, index = 0; index < nb_connexions; index++, cnx++ ) {
        if ( ( *cnx )->sockfd == descr->sockfd_accept ) {
            found = 1;
            break;
        }
    }
    if ( !found ) {
        fprintf( stderr, "%s %d:\n\tfound should be true\n", __FILE__, __LINE__ );
        UNLOCK;
        if ( close( descr->sockfd_accept ) == -1 )
            fprintf( stderr, "Error %d while closing socket %d\n", errno, descr->sockfd_accept );
        free( descr );
        pthread_exit( NULL );
        return NULL;
    }
    connexion = connexions[index];

    logg( LOG_MESSIP_INFORMATIVE, "Destroy connexion #%d sockfd=%-3d id=%s [%s]\n",
       index, connexion->sockfd, connexion->id, connexion->process_name );
    if ( closesocket( connexion->sockfd ) == -1 )
        fprintf( stderr, "Unable to close socket %d: errno=%d\n", connexion->sockfd, errno );
    IDCPY( id, connexion->id );
    for ( k = index + 1; k < nb_connexions; k++ )
        connexions[k - 1] = connexions[k];
    if ( connexion->nb_cnx_channels )
        free( connexion->sockfd_cnx_channels );
    free( connexion );
    if ( --nb_connexions == 0 )
        connexions = NULL;

    /*--- Notify all owners of connected channels that this client dismissed ---*/
    for ( index = 0; index < nb_channels; index++ ) {
        int nb, new_nb;

        channel = channels[index];
        for ( nb = 0, k = 0; k < channel->nb_clients; k++ ) {
            if ( channel->cnx_clients[k] == descr->sockfd_accept ) {
                notify_server_death_client( channel, id, MESSIP_FLAG_DISMISSED );
                nb++;
            }                   // if
        }                       // for (k)
        new_nb = channel->nb_clients - nb;
        if ( new_nb == 0 ) {
            channel->nb_clients = 0;
            free( channel->cnx_clients );
            channel->cnx_clients = NULL;
        }
        else {
            int w, *clients;
            clients = malloc( sizeof( int ) * new_nb );
            for ( w = 0, k = 0; k < channel->nb_clients; k++ ) {
                if ( channel->cnx_clients[k] != descr->sockfd_accept )
                    clients[w++] = channel->cnx_clients[k];
            }                   // for (k)
            assert( w == new_nb );
            channel->nb_clients = new_nb;
            free( channel->cnx_clients );
            channel->cnx_clients = clients;
        }
    }                           // for (index)

    /*--- Notify other processes (optional) that this process is now dead---*/
    for ( ch = channels, index = 0; index < nb_channels; index++, ch++ ) {
        if ( ( *ch )->cnx == connexion )
            continue;

        channel = channels[index];
        if ( !channel->f_notify_deaths )
            continue;

        notify_server_death_client( channel, id, MESSIP_FLAG_DEATH_PROCESS );

    }                           // for (index)

    /*--- Destroy all channels related to this connection, if any ---*/
    for ( ch = channels, index = 0; index < nb_channels; index++, ch++ ) {
        if ( ( *ch )->cnx == connexion ) {
            channel = channels[index];
            destroy_channel( channel, index );
            index--;
        }                       // if
    }                           // for (ch)
    UNLOCK;

    /*--- Done ---*/
    free( descr );
    pthread_exit( NULL );
    return NULL;
}                               // thread_client_thread

/**
 * TBD 
 * 
 * @param signum TBD  
 * @return TBD
 */
static void sigint_sighandler( int signum ) {
    connexion_t *cnx;
    channel_t *ch;
    int index;

    LOCK;

    for ( index = 0; index < nb_channels; index++ ) {
        ch = channels[index];
        destroy_channel( ch, index );
    }                           // for

    for ( index = 0; index < nb_connexions; index++ ) {
        cnx = connexions[index];
        if ( closesocket( cnx->sockfd ) == -1 )
            fprintf( stderr, "Error %d while closing socket %d\n", errno, cnx->sockfd );
    }                           // for

    UNLOCK;
    f_bye = 1;

}                               // sigint_sighandler

/**
 * TBD 
 */
static void help( void ) {
    printf( "messip-mgr [-p] [-l]\n" );
    printf( "-p port : TCP port used between the library and the manager\n" );
    printf( "-l n    : logging value\n" );
    exit( -1 );
}                               // help

/**
 * TBD 
 * 
 * @param argc TBD  
 * @param argv TBD
 * @return TBD
 */
static void get_options( int argc, char *argv[] ) {
    int option_index, c;
    static struct option long_options[] = {
        {"port", 1, NULL, 'p'},
        {"log", 1, NULL, 'l'},
        {NULL, 0, NULL, 0}
    };

    /*--- Read /erx/messip if exist ---*/
    messip_port = MESSIP_DEFAULT_PORT;
    messip_port_http = messip_port + 1;
    if ( access( "/etc/messip", F_OK ) == 0 )
        read_etc_messip( messip_hostname, &messip_port, &messip_port_http );

    /*--- Any parameter ? ---*/
    logg_dir = NULL;
    for ( ;; ) {
        c = getopt_long( argc, argv, "p:l:", long_options, &option_index );
        if ( c == -1 )
            break;
//      printf( "c=%d option_index=%d arg=[%s]\n", c, option_index, optarg );
        switch ( c ) {
            case 'p':
                messip_port = atoi( optarg );
                break;
            case 'l':
                logg_dir = optarg;
                break;
            case 'h':
                messip_port_http = atoi( optarg );
                break;
            default:
                help(  );
                break;
        }                       // switch
    }                           // for (;;)

    printf( "Using %s:%d for Messaging\n", messip_hostname, messip_port );
    printf( "Using %s:%d for http\n", messip_hostname, messip_port_http );
    fflush( stdout );

}                               // get_options

/**
 *  Main function
 * 
 *  @param argc Number of arguments
 *  @param argv List of the arguments (array)
 *  @return 0 if no error
 */
int main( int argc, char *argv[] ) {
    int sockfd;
    struct sockaddr_in server_addr;
    int status;
    pthread_t tid;
    pthread_attr_t attr;
    struct sigaction sa;
    sigset_t set;

    fprintf( stdout, "To stop It:    kill -s SIGINT  %d\n", getpid(  ) );
    f_bye = 0;                  // Set to 1 when SIGINT has been applied
    get_options( argc, argv );

    // Initialize active connexions and channels
    nb_connexions = 0;
    connexions = NULL;
    nb_channels = 0;
    channels = NULL;

    // Register a function that clean-up opened sockets when exiting
    sa.sa_handler = sigint_sighandler;
    sa.sa_flags = 0;
    sigemptyset( &sa.sa_mask );
    sigaction( SIGINT, &sa, NULL );

    sigemptyset( &set );
    sigaddset( &set, SIGUSR2 );
    pthread_sigmask( SIG_BLOCK, &set, NULL );

    // Create a mutex, in order to protect shared table of data
    if ( pthread_mutex_init( &mutex, NULL ) == -1 ) {
        fprintf( stderr, "%s %d\n\tUnable to initialize mutex - errno=%d\n", __FILE__, __LINE__, errno );
        return -1;
    }

    // Create socket
    sockfd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( sockfd < 0 ) {
        fprintf( stderr, "%s %d\n\tUnable to open a socket!\n", __FILE__, __LINE__ );
        return -1;
    }

    // Bind the socket
    memset( &server_addr, 0, sizeof( server_addr ) );
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl( INADDR_ANY );
    server_addr.sin_port = htons( messip_port );
    status = bind( sockfd, ( struct sockaddr * ) &server_addr, sizeof( struct sockaddr_in ) );
    if ( status == EADDRINUSE ) {
        fprintf( stderr, "%s %d\n\tUnable to bind, port %d is already in use\n", __FILE__, __LINE__, messip_port );
        if ( closesocket( sockfd ) == -1 )
            fprintf( stderr, "Error %d while closing socket %d\n", errno, sockfd );
        return -1;
    }
    if ( status < 0 ) {
        fprintf( stderr, "%s %d\nUnable to bind - port %d - status=%d errno=%s\n",
           __FILE__, __LINE__, messip_port, status, strerror( errno ) );
        if ( closesocket( sockfd ) == -1 )
            fprintf( stderr, "Error %d while closing socket %d\n", errno, sockfd );
        return -1;
    }

    listen( sockfd, 8 );

    // Create a specific thread to debug information (apply SIGUSR1)
    pthread_attr_init( &attr );
    pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );
    pthread_create( &tid, &attr, &debug_thread, NULL );

    // Create a specific thread to debug information (HTTP server)
    pthread_attr_init( &attr );
    pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );
    pthread_create( &tid, &attr, &http_thread, NULL );

    for ( ; !f_bye; ) {
        clientdescr_t *descr;
        pthread_t tid;
        pthread_attr_t attr;

        descr = malloc( sizeof( clientdescr_t ) );
        descr->client_addr_len = sizeof( struct sockaddr_in );
        descr->sockfd_accept = accept( sockfd, ( struct sockaddr * ) &descr->client_addr, &descr->client_addr_len );
        if ( descr->sockfd_accept == -1 ) {
            if ( errno == EINTR )   // A signal has been applied
                continue;
            fprintf( stderr, "Socket non accepted - errno=%d\n", errno );
            if ( closesocket( sockfd ) == -1 )
                fprintf( stderr, "Error %d while closing socket %d\n", errno, sockfd );
            return -1;
        }
#if 0
        logg( LOG_MESSIP_DEBUG_LEVEL1, "accepted a msg from %s, port=%d, socket=%d\n",
           inet_ntoa( descr->client_addr.sin_addr ), descr->client_addr.sin_port, descr->sockfd_accept );
#endif
        pthread_attr_init( &attr );
        pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );
        pthread_create( &tid, &attr, &thread_client_thread, descr );
    }                           // for (;;)

    if ( closesocket( sockfd ) == -1 )
        fprintf( stderr, "Error %d while closing socket %d\n", errno, sockfd );
    return 0;
}                               // main
