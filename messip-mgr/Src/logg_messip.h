/**
 * @file logg_messip.h
 *
 * MessIP : Message Passing over TCP/IP
 * Copyright (C) 2001-2007  Olivier Singla
 * http://messip.sourceforge.net/
 *
 **/

#ifndef LOGG_MESSIP_H_
#define LOGG_MESSIP_H_

/*
	MessIP : Message Passing over TCP/IP
    Copyright (C) 2001-2007  Olivier Singla
	http://messip.sourceforge.net/
*/

typedef enum {
    LOG_MESSIP_DEBUG_LEVEL1 = 1000, ///< Not detailled
    LOG_MESSIP_DEBUG_LEVEL2,    	///< More detailled
    LOG_MESSIP_DEBUG_LEVEL3,    	///< Very detailled
    LOG_MESSIP_INFORMATIVE,     	///< Information
    LOG_MESSIP_WARNING,         	///< A warning
    LOG_MESSIP_NON_FATAL_ERROR, 	///< Non fatal error
    LOG_MESSIP_FATAL_ERROR,     	///< Fatal error
    LOG_MESSIP_NOT_YET_DONE,    	///< Feature not yet ported and to be done
} logg_type_t;

int logg( logg_type_t type, char *fmt, ... )
   __attribute__ ( ( format( printf, 2, 3 ) ) );

void messip_logg_off( void );

void messip_logg_on( void );

#endif /*LOGG_MESSIP_H_*/
