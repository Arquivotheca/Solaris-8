/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)disp5.c	1.27	96/04/10 SMI"	/* SVr4.0 1.13	*/

#include "dispatch.h"
#include <syslog.h>

extern int		Net_fd;

extern MESG *		Net_md;

/**
 ** s_child_done()
 **/

void
s_child_done(char *m, MESG *md)
{
	long			key;
	short			slot;
	short			status;
	short			err;


	getmessage (m, S_CHILD_DONE, &key, &slot, &status, &err);
	syslog(LOG_DEBUG, "s_child_done(%d, %d, %d, %d)", key, slot, status,
	       err);

	if ((0 <= slot) && (slot < ET_Size) && (Exec_Table[slot].key == key) &&
	    (Exec_Table[slot].md == md)) {
		/*
		 * Remove the message descriptor from the listen
		 * table, then forget about it; we don't want to
		 * accidently match this exec-slot to a future,
		 * unrelated child.
		 */
		DROP_MD (Exec_Table[slot].md);
		Exec_Table[slot].md = 0;

		Exec_Table[slot].pid = -99;
		Exec_Table[slot].status = status;
		Exec_Table[slot].errno = err;
		DoneChildren++;

	}

	return;
}
