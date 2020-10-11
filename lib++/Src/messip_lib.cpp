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

#include <string>

#include "../../lib/Src/messip.h"

#include "messip++.h"

Messip::Messip() {

}

Messip::~Messip() {

}
