/**
 * @file messip_private.h
 *
 * MessIP : Message Passing over TCP/IP
 * Copyright (C) 2001-2007  Olivier Singla
 * http://messip.sourceforge.net/
 *
 **/

#ifndef MESSIP_PRIVATE_H_
#define MESSIP_PRIVATE_H_

/*
*/

#define SIGVAL_PTR sival_ptr

//
// /etc/messip
// 

typedef struct messip_mgr {
    char host[80];
    int port;
    char path[80];
    struct messip_mgr *next;
} messip_mgr_t;


// op : int32_t
enum {
    MESSIP_OP_CONNECT = 0x01010101,
    MESSIP_OP_CHANNEL_CREATE = 0x02020202,
    MESSIP_OP_CHANNEL_DELETE = 0x03030303,
    MESSIP_OP_CHANNEL_CONNECT = 0x04040404,
    MESSIP_OP_CHANNEL_DISCONNECT = 0x05050505,
    MESSIP_OP_CHANNEL_PING = 0x06060606,
    MESSIP_OP_BUFFERED_SEND = 0x07070707,
    MESSIP_OP_DEATH_NOTIFY = 0x08080808,
    MESSIP_OP_SIN = 0x09090909
};


// -------------------------
// MESSIP_OP_CONNECT message
// -------------------------

typedef struct {
    messip_id_t id;
} messip_send_connect_t;

typedef struct {
    int32_t ok;
} messip_reply_connect_t;


// ---------------------------------------
// MESSIP_OP_CHANNEL_CREATE channel_create
// ---------------------------------------

typedef struct {
    messip_id_t id;
    int32_t maxnb_msg_buffered;
    char channel_name[MESSIP_CHANNEL_NAME_MAXLEN + 1];
    uint16_t sin_port;
    char sin_addr_str[48];
} messip_send_channel_create_t;

typedef struct {
    int32_t ok;
    in_port_t sin_port;
    in_addr_t sin_addr;
    char sin_addr_str[48];
} messip_reply_channel_create_t;


// ----------------------------------------
// MESSIP_OP_CHANNEL_DELETE channel_delete
// ----------------------------------------

typedef struct {
    messip_id_t id;
    char name[MESSIP_CHANNEL_NAME_MAXLEN + 1];
} messip_send_channel_delete_t;

typedef struct {
    int32_t nb_clients;
} messip_reply_channel_delete_t;


// ----------------------------------------
// MESSIP_OP_CHANNEL_CONNECT channel_connect
// ----------------------------------------

typedef struct {
    messip_id_t id;
    char name[MESSIP_CHANNEL_NAME_MAXLEN + 1];
} messip_send_channel_connect_t;

typedef struct {
    int32_t ok;
    int32_t f_already_connected;
    messip_id_t id;
    in_port_t sin_port;         // 2 bytes
    in_addr_t sin_addr;         // 4 bytes
    char sin_addr_str[48];
    int mgr_sockfd;             // Socket in the messip_mgr
} messip_reply_channel_connect_t;


// -----------------------------------------------
// MESSIP_OP_CHANNEL_DISCONNECT channel_disconnect
// -----------------------------------------------

typedef struct {
    messip_id_t id;
    char name[MESSIP_CHANNEL_NAME_MAXLEN + 1];
} messip_send_channel_disconnect_t;

typedef struct {
    int32_t ok;
} messip_reply_channel_disconnect_t;


// -----------------------------------
// MESSIP_OP_CHANNEL_PING channel_ping
// -----------------------------------

typedef struct {
    messip_id_t id;
} messip_send_channel_ping_t;

typedef struct {
    int32_t ok;                 // MESSIP_OK or MESSIP_NOK
} messip_reply_channel_ping_t;


// -------------------------------------
// MESSIP_OP_BUFFERED_SEND buffered_send
// -------------------------------------

typedef struct {
    messip_id_t id_from;
    int32_t type;
    int32_t datalen;
    int mgr_sockfd;             // Socket in the messip_mgr
} messip_send_buffered_send_t;

typedef struct {
    int32_t ok;                 // MESSIP_OK or MESSIP_NOK
    int32_t nb_msg_buffered;
} messip_reply_buffered_send_t;


// ----------------------
// MESSIP_OP_DEATH_NOTIFY 
// ----------------------

typedef struct {
    messip_id_t id_from;
    int32_t status;
} messip_send_death_notify_t;

typedef struct {
    int32_t ok;                 // MESSIP_OK or MESSIP_NOK
} messip_reply_death_notify_t;


// ----------------------------------------------
// Additional information sent on a messip_send()
// ----------------------------------------------

#define MESSIP_FLAG_CONNECTING		1
#define MESSIP_FLAG_DISCONNECTING	2
#define MESSIP_FLAG_DISMISSED		3
#define MESSIP_FLAG_TIMER			5
#define MESSIP_FLAG_BUFFERED		6
#define MESSIP_FLAG_PING			7
#define MESSIP_FLAG_DEATH_PROCESS	8

typedef struct {
    int32_t flag;
    messip_id_t id;
    int32_t type;
    int32_t datalen;
} messip_datasend_t;

typedef struct {
    messip_id_t id;
    int32_t answer;
    int32_t datalen;
} messip_datareply_t;


// --------------------------
// 
// --------------------------

int messip_writev( SOCKET sockfd, const struct iovec *iov, int iovcnt );
int messip_readv( SOCKET sockfd, const struct iovec *iov, int iovcnt );


#define IDCPY( DST, SRC ) \
    strncpy( DST, SRC, MESSIP_MAXLEN_ID ); \
    DST[MESSIP_MAXLEN_ID] = 0;

#endif /*MESSIP_PRIVATE_H_*/
