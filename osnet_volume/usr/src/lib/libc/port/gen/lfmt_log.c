/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 */
#pragma	ident	"@(#)lfmt_log.c	1.9	97/08/12 SMI"

/*LINTLIBRARY*/

/* lfmt_log() - log info */
#include "synonyms.h"
#include <mtlib.h>
#include <pfmt.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/types32.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <fcntl.h>
#include <errno.h>
#include <synch.h>
#include <thread.h>
#include "pfmt_data.h"
#include <time.h>
#include <stropts.h>
#include <unistd.h>
#include <strings.h>
#include <sys/uio.h>

#define	MAXMSG	1024
#define	LOGNAME		"/dev/conslog"
#define	LOG_CONSOLE	"/dev/console"

__lfmt_log(const char *text, const char *sev, va_list args, long flag, int ret)
{
	static int fd = -1;
	struct strbuf dat;
	int msg_offset;
	long len;
	char msgbuf[MAXMSG];
	int err;
	int fdd;

	len = ret + sizeof (long) + 3;

	if (len > sizeof (msgbuf)) {
		errno = ERANGE;
		return (-2);
	}

	*(long *)msgbuf = flag;
	msg_offset = (int)sizeof (long);

	(void) rw_rdlock(&_rw_pfmt_label);
	if (*__pfmt_label)
		msg_offset += sprintf(msgbuf + msg_offset, __pfmt_label);
	(void) rw_unlock(&_rw_pfmt_label);

	if (sev)
		msg_offset += sprintf(msgbuf + msg_offset, sev, flag & 0xff);

	msg_offset += 1 + vsprintf(msgbuf + msg_offset, text, args);
	msgbuf[msg_offset++] = '\0';

	if (fd == -1 &&
		((fd = open(LOGNAME, O_WRONLY)) == -1 ||
				fcntl(fd, F_SETFD, 1) == -1))
		return (-2);

	dat.maxlen = MAXMSG;
	dat.len = (int)msg_offset;
	dat.buf = msgbuf;

	if (putmsg(fd, 0, &dat, 0) == -1) {
		(void) close(fd);
		return (-2);
	}

	/*
	 *  Display it to a console
	 */
	if ((flag & MM_CONSOLE) != 0) {
		char *p;
		time_t t;
		char buf[128];
		err = errno;
		fdd = open(LOG_CONSOLE, O_WRONLY);
		if (fdd != -1) {
			/*
			 * Use C locale for time stamp.
			 */
			(void) time(&t);
			(void) sprintf(buf, ctime(&t));
			p = (char *)strrchr(buf, '\n');
			if (p != NULL)
				*p = ':';
			(void) write(fdd, buf, strlen(buf));
			(void) write(fdd, msgbuf+sizeof (long),
				msg_offset-sizeof (long));
			(void) write(fdd, "\n", 1);
		} else
			return (-2);
		(void) close(fdd);
		errno = err;
	}
	return (ret);
}
