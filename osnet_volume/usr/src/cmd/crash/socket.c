/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		Copyright (C) 1991  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)socket.c	1.10	99/03/21 SMI"

/*
 * This file contains code for the crash functions:  socket.
 */

#include <stdio.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include "crash.h"

static void prsocket(void *addr, int phys);
static void kmsocket(void *kaddr, void *buf);
static char *pr_state(ulong_t state, ulong_t mode);
static char *pr_addr(int family, struct sockaddr *addr, int addrlen);
static char *pr_flags(ulong_t flags);
static char *pr_options(ulong_t options);
static void prsoconfig(int full, int lock);

static	int phys = 0;
static	int full = 0;
static	int lock = 0;

static	char *heading =
"ADDR FAM/TYPE/PROTO MAJ/MIN ACCESSVP ERR STATE\n";

/* get arguments for socket function */
int
getsocket()
{
	long addr = -1;
	int c;

	phys = full = lock = 0;

	optind = 1;
	while ((c = getopt(argcnt, args, "flpw:")) != EOF) {
		switch (c) {
			case 'f' :	full = 1;
					break;
			case 'l' :	lock = 1;
					break;
			case 'p' :	phys = 1;
					break;
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}

	if (!full && !lock)
		fprintf(fp, "%s", heading);
	if (args[optind]) {
		do {
			if ((addr = strcon(args[optind++], 'h')) == -1)
				error("\n");
			prsocket((void *)addr, phys);
		} while (args[optind]);
	} else {
		kmem_cache_apply(kmem_cache_find("sock_cache"),
			kmsocket);
	}
	return (0);
}


/*ARGSUSED1*/
static void
kmsocket(void *kaddr, void *buf)
{
	prsocket(kaddr, 0);
}

static void
prsocket(void *addr, int phys)
{
	struct sonode sos, *so;
	int addrbuf[1024];
	struct sockaddr *sa = (struct sockaddr *)addrbuf;

	readbuf(addr, 0, phys, &sos, sizeof (sos), "socket table slot");
	so = &sos;
	if (full)
		fprintf(fp, "%s", heading);
	/* addr family/type/protocol, maj,min, accessvp, state */
	fprintf(fp, "%8p %d/%d/%d     %4u,%-3u %8p %d  %s\n", addr,
		so->so_family, so->so_type, so->so_protocol,
		getemajor(so->so_dev), geteminor(so->so_dev),
		so->so_accessvp, so->so_error,
		pr_state(so->so_state, so->so_mode));


	if (so->so_laddr_sa != NULL && so->so_laddr_len != 0) {
		readmem(so->so_laddr_sa, 1, &addrbuf,
			so->so_laddr_len, "socket laddr");
		fprintf(fp, "\tLocal addr: %s\n",
			pr_addr(sa->sa_family,
				(struct sockaddr *)addrbuf,
				so->so_laddr_len));
	}
	if (so->so_faddr_sa != NULL && so->so_faddr_len != 0) {
		readmem(so->so_faddr_sa, 1, &addrbuf,
			so->so_faddr_len, "socket faddr");
		fprintf(fp, "\tRemote addr: %s\n",
			pr_addr(sa->sa_family,
				(struct sockaddr *)addrbuf,
				so->so_faddr_len));
	}
	if (so->so_delayed_error != 0 && so->so_eaddr_mp != NULL) {
		fprintf(fp, "\tDelayed error %d for mblk: 0x%p\n",
			so->so_delayed_error, so->so_eaddr_mp);
	}

	if (full) {
		fprintf(fp, "\tflags %s vers %d, pushcnt %d\n",
			pr_flags(so->so_flag), so->so_version,
			so->so_pushcnt);
		fprintf(fp, "\toobmsg %8p, cnt %d/%d, pgrp %d\n",
			so->so_oobmsg, so->so_oobsigcnt, so->so_oobcnt,
			so->so_pgrp);

		fprintf(fp, "\toptions %s ling %d/%d sb %d rb %d\n",
			pr_options(so->so_options),
			so->so_linger.l_onoff, so->so_linger.l_linger,
			so->so_sndbuf, so->so_rcvbuf);

		fprintf(fp, "\ttsdu %d, etsdu %d, addr %d, opt %d, "
			"tidu %d, type %d\n",
			so->so_tsdu_size, so->so_etsdu_size, so->so_addr_size,
			so->so_opt_size, so->so_tidu_size, so->so_serv_type);

		if (so->so_family == AF_UNIX) {
			fprintf(fp, "\tux_vp: %8p, "
				"ux_laddr %p:%x, ux_faddr %p:%x\n",
				so->so_ux_bound_vp,
				so->so_ux_laddr.sou_vp,
				so->so_ux_laddr.sou_magic,
				so->so_ux_faddr.sou_vp,
				so->so_ux_faddr.sou_magic);
		}
		/* print vnode info */
		fprintf(fp, "\nVNODE :\n");
		fprintf(fp, "VCNT VFSMNTED   VFSP     STREAMP     VTYPE  ");
		fprintf(fp, "RDEV   VDATA    VFILOCKS VFLAG \n");
		prvnode(&so->so_vnode, lock);
		fprintf(fp, "\n");
	}
}

static char *
pr_state(ulong_t state, ulong_t mode)
{
	static char buf[2048];

	buf[0] = 0;
	if (state & SS_ISCONNECTED)
		strcat(buf, "ISCONNECTED ");
	if (state & SS_ISCONNECTING)
		strcat(buf, "ISCONNECTING ");
	if (state & SS_ISDISCONNECTING)
		strcat(buf, "ISDISCONNECTING ");
	if (state & SS_CANTSENDMORE)
		strcat(buf, "CANTSENDMORE ");

	if (state & SS_CANTRCVMORE)
		strcat(buf, "CANTRCVMORE ");
	if (state & SS_ISBOUND)
		strcat(buf, "ISBOUND ");
	if (state & SS_NDELAY)
		strcat(buf, "NDELAY ");
	if (state & SS_NONBLOCK)
		strcat(buf, "NONBLOCK ");

	if (state & SS_ASYNC)
		strcat(buf, "ASYNC ");
	if (state & SS_ACCEPTCONN)
		strcat(buf, "ACCEPTCONN ");
	if (state & SS_HASCONNIND)
		strcat(buf, "HASCONNIND ");
	if (state & SS_SAVEDEOR)
		strcat(buf, "SAVEDEOR ");

	if (state & SS_RCVATMARK)
		strcat(buf, "RCVATMARK ");
	if (state & SS_OOBPEND)
		strcat(buf, "OOBPEND ");
	if (state & SS_HAVEOOBDATA)
		strcat(buf, "HAVEOOBDATA ");
	if (state & SS_HADOOBDATA)
		strcat(buf, "HADOOBDATA ");

	if (state & SS_FADDR_NOXLATE)
		strcat(buf, "FADDR_NOXLATE ");
	if (state & SS_WUNBIND)
		strcat(buf, "WUNBIND ");

	if (mode & SM_PRIV)
		strcat(buf, "PRIV ");
	if (mode & SM_ATOMIC)
		strcat(buf, "ATOMIC ");
	if (mode & SM_ADDR)
		strcat(buf, "ADDR ");
	if (mode & SM_CONNREQUIRED)
		strcat(buf, "CONNREQUIRED ");

	if (mode & SM_FDPASSING)
		strcat(buf, "FDPASSING ");
	if (mode & SM_EXDATA)
		strcat(buf, "EXDATA ");
	if (mode & SM_OPTDATA)
		strcat(buf, "OPTDATA ");
	if (mode & SM_BYTESTREAM)
		strcat(buf, "BYTESTREAM ");
	return (buf);
}

static char *
pr_addr(int family, struct sockaddr *addr, int addrlen)
{
	static char buf[8192];
	char abuf[INET6_ADDRSTRLEN];

	if (addr == NULL || addrlen == 0) {
		sprintf(buf, "(len %d) 0x%p", addrlen, addr);
		return (buf);
	}
	switch (family) {
	case AF_INET: {
		struct sockaddr_in *sin = (struct sockaddr_in *)addr;

		sprintf(buf, "(len %d) %s/%d",
		    addrlen,
		    inet_ntop(AF_INET, (void *)&sin->sin_addr,
		    abuf, sizeof (abuf)),
		    ntohs(sin->sin_port));
		break;
	}
	case AF_INET6: {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;

		sprintf(buf, "(len %d) %s/%d",
		    addrlen,
		    inet_ntop(AF_INET6, (void *)&sin6->sin6_addr,
		    abuf, sizeof (abuf)),
		    ntohs(sin6->sin6_port));
		break;
	}

	case AF_UNIX: {
		struct sockaddr_un *soun = (struct sockaddr_un *)addr;

		sprintf(buf, "(len %d) %s",
			addrlen,
			(soun == NULL) ? "(none)" : soun->sun_path);
		break;
	}
	default:
		sprintf(buf, "(unknown af %d)", family);
		break;
	}
	return (buf);
}

static char *
pr_flags(ulong_t flags)
{
	static char buf[1024];

	buf[0] = 0;
	if (flags & SOMOD)
		strcat(buf, "MOD ");
	if (flags & SOACC)
		strcat(buf, "ACC ");
	if (flags & SOLOCKED)
		strcat(buf, "LOCKED ");
	if (flags & SOREADLOCKED)
		strcat(buf, "READLOCKED ");
	if (flags & SOWANT)
		strcat(buf, "WANT ");
	if (flags & SOCLONE)
		strcat(buf, "CLONE ");
	return (buf);
}

static char *
pr_options(ulong_t options)
{
	static char buf[1024];

	buf[0] = 0;
	if (options & SO_DEBUG)
		strcat(buf, "DEBUG ");
	if (options & SO_ACCEPTCONN)
		strcat(buf, "ACCEPTCONN ");
	if (options & SO_REUSEADDR)
		strcat(buf, "REUSEADDR ");
	if (options & SO_KEEPALIVE)
		strcat(buf, "KEEPALIVE ");
	if (options & SO_DONTROUTE)
		strcat(buf, "DONTROUTE ");
	if (options & SO_BROADCAST)
		strcat(buf, "BROADCAST ");
	if (options & SO_USELOOPBACK)
		strcat(buf, "USELOOPBACK ");
	if (options & SO_LINGER)
		strcat(buf, "LINGER ");
	if (options & SO_OOBINLINE)
		strcat(buf, "OOBINLINE ");
	if (options & SO_DGRAM_ERRIND)
		strcat(buf, "DGRAM_ERRIND ");
	return (buf);
}

static Sym *Sphead;

/* get arguments for soconfig function */
int
getsoconfig()
{
	int c;

	full = lock = 0;

	if (!Sphead)
		if ((Sphead = symsrch("sphead")) == NULL)
			error("sphead not found in symbol table\n");

	optind = 1;
	while ((c = getopt(argcnt, args, "flw:")) != EOF) {
		switch (c) {
			case 'f' :	full = 1;
					break;
			case 'l' :	lock = 1;
					break;
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}

	fprintf(fp, "%8s %6s %6s %-10.10s   %s\n",
		"FAMILY", "TYPE", "PROTO", "VNODE", "DEVPATH");
	prsoconfig(full, lock);
	return (0);
}

/* print vfs switch table */
static void
prsoconfig(int full, int lock)
{
	struct sockparams sps;
	char devpath[MAXPATHLEN + 1];
	void *addr;

	readmem((void *)Sphead->st_value, 1, &addr,
	    sizeof (addr), "sphead");

	while (addr != NULL) {
		readmem(addr, 1, &sps, sizeof (sps), "sockparams");

		if (sps.sp_devpath != NULL) {
			/* NULL terminate */
			devpath[sizeof (devpath) - 1] = NULL;
			readmem(sps.sp_devpath, 1, devpath,
				sizeof (devpath) - 1, "sp_devpath");
		} else {
			devpath[0] = NULL;
		}
		fprintf(fp, "%6d %6d %6d   0x%p   %s\n",
			sps.sp_domain, sps.sp_type, sps.sp_protocol,
			sps.sp_vnode, devpath);
		if (full) {
			/* print vnode info */
			fprintf(fp, "VNODE :\n");
			fprintf(fp, "VCNT VFSMNTED   VFSP     STREAMP     "
				"VTYPE  ");
			fprintf(fp, "RDEV   VDATA    VFILOCKS VFLAG \n");
			prvnode(sps.sp_vnode, lock);
			fprintf(fp, "\n");
		}
		addr = sps.sp_next;
	}
}
