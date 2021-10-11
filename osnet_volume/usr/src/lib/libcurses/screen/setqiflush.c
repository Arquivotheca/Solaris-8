/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)setqiflush.c	1.10	97/08/20 SMI" /* SVr4.0 1.3 */

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	"curses_inc.h"

/*
 *	Set/unset flushing the output queue on interrupts or quits.
 */

void
_setqiflush(int yes)
{
#ifdef SYSV
	if (yes)
		cur_term->Nttybs.c_lflag &= ~NOFLSH;
	else
		cur_term->Nttybs.c_lflag |= NOFLSH;
	(void) reset_prog_mode();
#endif /* SYSV */
}
