/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)mdestroy.c	1.7	96/11/26 SMI"	/* SVr4.0 1.5	*/
# include	<string.h>
# include	<stropts.h>
# include	<errno.h>
# include	<stdlib.h>
# include	<unistd.h>

# include	"lp.h"
# include	"msgs.h"

int mdestroy(MESG *md)
{
	struct pollfd pfd;
	struct strrecvfd    recbuf;

	if (!md || md->type != MD_MASTER || md->file == NULL) {
		errno = EINVAL;
		return(-1);
	}

	if (fdetach(md->file) != 0)
		return(-1);

	pfd.fd = md->readfd;
	pfd.events = POLLIN;
	while (poll(&pfd, 1, 500) > 0) {
		if (ioctl(md->readfd, I_RECVFD, &recbuf) == 0)
			close(recbuf.fd);
	}

	/*
	 * Pop connld module
	 */
	if (ioctl(md->writefd, I_POP, 0) != 0)
		return(-1);

	Free(md->file);
	md->file = NULL;

	(void) mdisconnect(md);

	return(0);
}
