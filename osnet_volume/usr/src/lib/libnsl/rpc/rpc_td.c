
/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc.
 */

#pragma ident	"@(#)rpc_td.c	1.10	97/04/29 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)rpc_td.c 1.32 89/03/16 Copyr 1988 Sun Micro";
#endif

#include "rpc_mt.h"
#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/trace.h>
#include <errno.h>
#include <tiuser.h>
#include <string.h>
#include <stropts.h>
#include <netinet/tcp.h>
#include <stdlib.h>

#define	MAXOPTSIZE 64

__td_setnodelay(fd)
	int fd;
{
	int rval = 0;
	static mutex_t td_opt_lock = DEFAULTMUTEX;
	static struct t_optmgmt t_optreq, t_optret;
	int state;


	/* VARIABLES PROTECTED BY td_opt_lock: t_optreq, t_optret */

	trace2(TR__td_setnodelay, 0, fd);

	if ((state = t_getstate(fd)) == -1)
		return (-1);

	mutex_lock(&td_opt_lock);
	if ((state == T_IDLE) && (t_optreq.flags != T_NEGOTIATE)) {
		int i = 1;
		struct opthdr *opt;

		t_optreq.flags = T_NEGOTIATE;
		t_optreq.opt.maxlen = MAXOPTSIZE;
		t_optreq.opt.buf = (char *) malloc(MAXOPTSIZE);
		opt = (struct opthdr *)(t_optreq.opt.buf);
		opt->name = TCP_NODELAY;
		opt->len = 4;
		opt->level = IPPROTO_TCP;
		(void) memcpy((caddr_t) (t_optreq.opt.buf +
				sizeof (struct opthdr)), &i, sizeof (int));
		t_optreq.opt.len = (int)(sizeof (struct opthdr) +
						sizeof (int));

		t_optret.opt.maxlen = MAXOPTSIZE;
		t_optret.opt.len = 0;
		t_optret.opt.buf = (char *) malloc(MAXOPTSIZE);
	}

	if (state == T_IDLE)
		rval = t_optmgmt(fd, &t_optreq, &t_optret);

	mutex_unlock(&td_opt_lock);
	trace2(TR__td_setnodelay, 1, fd);
	return (rval);
}
