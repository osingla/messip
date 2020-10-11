/**
 * @file messip_lib.cpp
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

#include <syslog.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <string>

#include "../../lib/Src/log.h"
#include "../../lib/Src/messip.h"

#include "messip++.h"

class MessipCnx {

public:

    MessipCnx();
    ~MessipCnx();
    
private:

    friend class Messip;
    messip_cnx_t *cnx;

};

MessipCnx::MessipCnx() {

    DEBUG( "Constructor" );

    cnx = NULL;
}

MessipCnx::~MessipCnx() {

}

Messip::Messip() {

    DEBUG( "Constructor" );

    messip_cnx = new MessipCnx();
    
}

Messip::~Messip() {

    delete messip_cnx;

}

int Messip::connect(std::string msg_ref, std::string id, int msec_timeout) {

    return 0;
}

int Messip::connect(std::string id, int msec_timeout) {

    DEBUG("");
    messip_cnx->cnx = messip_connect(NULL, id.c_str(), msec_timeout);
    DEBUG("%p", messip_cnx->cnx);

    return 0;
}


