/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)trace.c	1.10	98/06/23 SMI"	/* SVr4.0 1.1	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989,1991,1992  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 *
 */


/*
 * Routing Table Management Daemon
 */
#define	RIPCMDS
#include "defs.h"
#include <sys/stat.h>

char *inet_ntoa();

#define	NRECORDS	50		/* size of circular trace buffer */
#ifdef DEBUG
FILE	*ftrace = stdout;
int	tracing = 1;
#endif

static int	iftraceinit();

void
traceinit(ifp)
	register struct interface *ifp;
{

	if (iftraceinit(ifp, &ifp->int_input) &&
	    iftraceinit(ifp, &ifp->int_output))
		return;
	tracing = 0;
	(void) fprintf(stderr, "traceinit: can't init %s\n",
	    (ifp->int_name != NULL) ? ifp->int_name : "(noname)");
}

static int
iftraceinit(ifp, ifd)
	struct interface *ifp;
	register struct ifdebug *ifd;
{
	register struct iftrace *t;

	ifd->ifd_records =
	    (struct iftrace *)malloc((unsigned)NRECORDS *
	    sizeof (struct iftrace));
	if (ifd->ifd_records == 0)
		return (0);
	ifd->ifd_front = ifd->ifd_records;
	ifd->ifd_count = 0;
	for (t = ifd->ifd_records; t < ifd->ifd_records + NRECORDS; t++) {
		t->ift_size = 0;
		t->ift_packet = 0;
	}
	ifd->ifd_if = ifp;
	return (1);
}

void
traceon(file)
	char *file;
{
	struct stat stbuf;

	if (ftrace != NULL)
		return;
	if (stat(file, &stbuf) >= 0 && (stbuf.st_mode & S_IFMT) != S_IFREG)
		return;
	ftrace = fopen(file, "a");
	if (ftrace == NULL)
		return;
	(void) dup2(fileno(ftrace), 1);
	(void) dup2(fileno(ftrace), 2);
}

void
traceonfp(fp)
	FILE *fp;
{
	if (ftrace != NULL)
		return;
	ftrace = fp;
	if (ftrace == NULL)
		return;
	(void) dup2(fileno(ftrace), 1);
	(void) dup2(fileno(ftrace), 2);
}

void
traceoff()
{
	if (!tracing)
		return;
	if (ftrace != NULL)
		(void) fclose(ftrace);
	ftrace = NULL;
	tracing = 0;
}

void
trace(ifd, who, p, len, m)
	register struct ifdebug *ifd;
	struct sockaddr *who;
	char *p;
	int len, m;
{
	register struct iftrace *t;

	if (ifd->ifd_records == 0)
		return;
	t = ifd->ifd_front++;
	if (ifd->ifd_front >= ifd->ifd_records + NRECORDS)
		ifd->ifd_front = ifd->ifd_records;
	if (ifd->ifd_count < NRECORDS)
		ifd->ifd_count++;
	if (t->ift_size > 0 && t->ift_size < len && t->ift_packet) {
		free(t->ift_packet);
		t->ift_packet = 0;
	}
	t->ift_stamp = (int)time((time_t *)0);
	t->ift_who = *who;
	if (len > 0 && t->ift_packet == 0) {
		t->ift_packet = malloc((unsigned)len);
		if (t->ift_packet == 0)
			len = 0;
	}
	if (len > 0)
		bcopy(p, t->ift_packet, len);
	t->ift_size = len;
	t->ift_metric = m;
}

void
traceaction(fd, action, rt)
	FILE *fd;
	char *action;
	struct rt_entry *rt;
{
	struct sockaddr_in *dst, *gate;
	static struct bits {
		ulong_t	t_bits;
		char	*t_name;
	} flagbits[] = {
		{ RTF_UP,		"UP" },
		{ RTF_GATEWAY,		"GATEWAY" },
		{ RTF_HOST,		"HOST" },
		{ 0 }
	}, statebits[] = {
		{ RTS_PASSIVE,		"PASSIVE" },
		{ RTS_REMOTE,		"REMOTE" },
		{ RTS_INTERFACE,	"INTERFACE" },
		{ RTS_CHANGED,		"CHANGED" },
		{ RTS_INTERNAL,		"INTERNAL" },
		{ RTS_EXTERNAL,		"EXTERNAL" },
		{ RTS_SUBNET,		"SUBNET" },
		{ RTS_DEFAULT,		"DEFAULT" },
		{ RTS_POINTOPOINT,	 "POINTOPOINT" },
		{ RTS_PRIVATE,		"PRIVATE" },
		{ 0 }
	};
	register struct bits *p;
	register int first;
	char *cp;
	time_t now = time((time_t *)0);

	if (fd == NULL)
		return;
	(void) fprintf(fd, "%.15s %s ", ctime(&now)+4, action);
	if (rt) {
		dst = (struct sockaddr_in *)&rt->rt_dst;
		gate = (struct sockaddr_in *)&rt->rt_router;
		(void) fprintf(fd, "dst %s ", inet_ntoa(dst->sin_addr));
		(void) fprintf(fd, "via %s metric %d",
		    inet_ntoa(gate->sin_addr), rt->rt_metric);
		if (rt->rt_ifp) {
			(void) fprintf(fd, " if %s",
			    (rt->rt_ifp->int_name != NULL) ?
				rt->rt_ifp->int_name : "(noname)");
		}
		(void) fprintf(fd, " state");
		cp = " %s";
		for (first = 1, p = statebits; p->t_bits > 0; p++) {
			if ((rt->rt_state & p->t_bits) == 0)
				continue;
			(void) fprintf(fd, cp, p->t_name);
			if (first) {
				cp = "|%s";
				first = 0;
			}
		}
		if (first)
			(void) fprintf(fd, " 0");
		if (rt->rt_flags != (RTF_UP | RTF_GATEWAY)) {
			cp = " %s";
			for (first = 1, p = flagbits; p->t_bits > 0; p++) {
				if ((rt->rt_flags & p->t_bits) == 0)
					continue;
				(void) fprintf(fd, cp, p->t_name);
				if (first) {
					cp = "|%s";
					first = 0;
				}
			}
		}
	}
	(void) putc('\n', fd);
	if (!tracepackets && rt && (rt->rt_state & RTS_PASSIVE) == 0 &&
	    rt->rt_ifp)
		dumpif(fd, rt->rt_ifp);
	(void) fflush(fd);
}

void
dumpif(fd, ifp)
	FILE *fd;
	register struct interface *ifp;
{
	if (ifp->int_input.ifd_count || ifp->int_output.ifd_count) {
		(void) fprintf(fd, "*** Packet history for interface %s ***\n",
		    (ifp->int_name != NULL) ? ifp->int_name : "(noname)");
		dumptrace(fd, "to", &ifp->int_output);
		dumptrace(fd, "from", &ifp->int_input);
		(void) fprintf(fd, "*** end packet history ***\n");
	}
	(void) fflush(fd);
}

void
dumptrace(fd, dir, ifd)
	FILE *fd;
	char *dir;
	register struct ifdebug *ifd;
{
	register struct iftrace *t;
	char *cp;

	if (strcmp(dir, "to") == 0)
		cp = "Output";
	else
		cp = "Input";
	if (ifd->ifd_front == ifd->ifd_records &&
	    ifd->ifd_front->ift_size == 0) {
		(void) fprintf(fd, "%s: no packets.\n", cp);
		(void) fflush(fd);
		return;
	}
	(void) fprintf(fd, "%s trace:\n", cp);
	t = ifd->ifd_front - ifd->ifd_count;
	if (t < ifd->ifd_records)
		t += NRECORDS;
	for (; ifd->ifd_count; ifd->ifd_count--, t++) {
		if (t >= ifd->ifd_records + NRECORDS)
			t = ifd->ifd_records;
		if (t->ift_size == 0)
			continue;
		(void) fprintf(fd, "%.24s: metric=%d\n", ctime(&t->ift_stamp),
			t->ift_metric);
		dumppacket(fd, dir, (struct sockaddr_in *)&t->ift_who,
		    t->ift_packet, t->ift_size);
	}
}

void
dumppacket(fd, dir, who, cp, size)
	FILE *fd;
	struct sockaddr_in *who;		/* should be sockaddr */
	char *dir, *cp;
	register int size;
{
	register struct rip *msg = (struct rip *)cp;
	register struct netinfo *n;

	if (msg->rip_cmd && msg->rip_cmd < RIPCMD_MAX) {
		(void) fprintf(fd, "%s %s %s.%d", ripcmds[msg->rip_cmd],
		    dir, inet_ntoa(who->sin_addr), ntohs(who->sin_port));
	} else {
		(void) fprintf(fd, "Bad cmd 0x%x %s %x.%d\n", msg->rip_cmd,
		    dir, inet_ntoa(who->sin_addr), ntohs(who->sin_port));
		(void) fprintf(fd, "size=%d cp=%x packet=%x\n",
		    size, cp, packet);
		(void) fflush(fd);
		return;
	}
	switch (msg->rip_cmd) {

	case RIPCMD_REQUEST:
	case RIPCMD_RESPONSE:
		(void) fprintf(fd, ":\n");
		size -= 4 * sizeof (char);
		n = msg->rip_nets;
		for (; size > 0; n++, size -= sizeof (struct netinfo)) {
			if (size < sizeof (struct netinfo))
				break;
			(void) fprintf(fd, "\tdst %s metric %d\n",
#define	satosin(sa)	((struct sockaddr_in *)&sa)
			    inet_ntoa(satosin(n->rip_dst)->sin_addr),
			    ntohl((ulong_t)n->rip_metric));
		}
		break;

	case RIPCMD_TRACEON:
		(void) fprintf(fd, ", file=%*s\n", size, msg->rip_tracefile);
		break;

	case RIPCMD_TRACEOFF:
		(void) fprintf(fd, "\n");
		break;
	}
	(void) fflush(fd);
}
