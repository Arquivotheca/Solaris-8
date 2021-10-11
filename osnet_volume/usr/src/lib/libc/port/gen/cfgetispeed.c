/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright(c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cfgetispeed.c	1.10	96/10/15 SMI"	/* SVr4.0 1.1	*/

/* LINTLIBRARY */

#pragma weak cfgetispeed = _cfgetispeed
#include "synonyms.h"
#include <sys/types.h>
#include <sys/termios.h>

/*
 * returns input baud rate stored in c_cflag pointed by termios_p
 */

speed_t
cfgetispeed(const struct termios *termios_p)
{
	return (termios_p->c_cflag & CIBAUDEXT ?
	    ((termios_p->c_cflag & CIBAUD) >> 16) + (CIBAUD >> 16) + 1 :
	    (termios_p->c_cflag & CIBAUD) >> 16);
}
