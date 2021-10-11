/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pr_getsockname.c	1.2	98/12/10 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include "libproc.h"

static int
get_sock_peer_name(struct ps_prochandle *Pr,
	int syscall, int sock, struct sockaddr *name, socklen_t *namelen)
{
	sysret_t rval;		/* return value from get{sock|peer}name() */
	argdes_t argd[4];	/* arg descriptors for get{sock|peer}name() */
	argdes_t *adp;

	adp = &argd[0];		/* sock argument */
	adp->arg_value = sock;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* name argument */
	adp->arg_value = 0;
	adp->arg_object = name;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_OUTPUT;
	adp->arg_size = *namelen;

	adp++;			/* namelen argument */
	adp->arg_value = 0;
	adp->arg_object = namelen;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_INOUT;
	adp->arg_size = sizeof (*namelen);

	adp++;			/* version argument */
	adp->arg_value = SOV_DEFAULT;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	rval = Psyscall(Pr, syscall, 4, &argd[0]);

	if (rval.sys_errno < 0)
		rval.sys_errno = ENOSYS;

	if (rval.sys_errno == 0)
		return (0);
	errno = rval.sys_errno;
	return (-1);
}

/* libc system call interface */
extern int _so_getsockname(int, struct sockaddr *, socklen_t *, int);
extern int _so_getpeername(int, struct sockaddr *, socklen_t *, int);

/*
 * getsockname() system call -- executed by subject process
 */
int
pr_getsockname(struct ps_prochandle *Pr,
	int sock, struct sockaddr *name, socklen_t *namelen)
{
	if (Pr == NULL)		/* no subject process */
		return (_so_getsockname(sock, name, namelen, SOV_DEFAULT));

	return (get_sock_peer_name(Pr, SYS_getsockname, sock, name, namelen));
}

/*
 * getpeername() system call -- executed by subject process
 */
int
pr_getpeername(struct ps_prochandle *Pr,
	int sock, struct sockaddr *name, socklen_t *namelen)
{
	if (Pr == NULL)		/* no subject process */
		return (_so_getpeername(sock, name, namelen, SOV_DEFAULT));

	return (get_sock_peer_name(Pr, SYS_getpeername, sock, name, namelen));
}
