/**
 * @file messip++.h
 *
 * MessIP : Message Passing over TCP/IP
 * Copyright (C) 2001-2007  Olivier Singla
 * http://messip.sourceforge.net/
 *
 **/

#ifndef MESSIPPP_H_
#define MESSIPPP_H_

class MessipCnx;

class Messip {

public:

    Messip();
    ~Messip();
    
    int connect(std::string msg_ref, std::string id, int msec_timeout=-1);
    int connect(std::string id, int msec_timeout=-1);

/*
    messip_channel_t *messip_channel_create( messip_cnx_t * cnx,
       const char *name, int msec_timeout, int32_t maxnb_msg_buffered );
*/

private:

    friend class MessipCnx;
    class MessipCnx *messip_cnx;

};

#endif /*MESSIPPP_H_*/
