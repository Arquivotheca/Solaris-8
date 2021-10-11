/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)tcflush.c	1.8	96/10/15 SMI"	/* SVr4.0 1.1 */

/*LINTLIBRARY*/

#pragma weak tcflush = _tcflush

#include "synonyms.h"
#include <sys/termios.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * flush read, write or both sides
 */

/*
 * TCIFLUSH  (0) -> flush data received but not read
 * TCOFLUSH  (1) -> flush data written but not transmitted
 * TCIOFLUSH (2) -> flush both
 */

int
tcflush(int fildes, int queue_selector)
{
	return (ioctl(fildes, TCFLSH, queue_selector));
}
