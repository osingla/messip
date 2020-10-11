/**
 * @file messip++_example_1.cpp
 * 
 **/

/**
 * @mainpage messip - Examples programs - No. 1
 * 
 * MessIP : Message Passing over TCP/IP \n
 * Copyright (C) 2001-2007  Olivier Singla \n
 * http://messip.sourceforge.net/ \n\n
 * 
 * Server:
 * - connect to the messip manager
 * - create a channel ('one') to receive messages
 * - wait for a message (no timeout) ["Hello"]
 * - when received the message, reply back with another message ["Bonjour"]
 * 
 * Client:
 * - connect to the messip manager
 * - locate the channel ('one') to send messages
 * - send a message (no timeout) ["Hello"]
 * - and print the message replied back ["Bonjour"]
 * 
 **/

#include <unistd.h>

#include <string>

#include <messip++.h>

Messip messip;

/**
 *  Main function
 * 
 *  @param argc Number of arguments
 *  @param argv List of the arguments (array)
 *  @return 0 if no error
 */
int main( int argc, char *argv[] ) {
    std::string cnxname = "hello";
    messip.connect(cnxname);
    sleep(9999);
    return 0;
}                               // main
