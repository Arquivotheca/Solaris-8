/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *	Copyright (c) 1991, 1996 by Sun Microsystems, Inc
 *	All rights reserved.
 *
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

#pragma ident	"@(#)lck.c	1.14	98/07/17 SMI"

/*
 * This file contains code for the crash function lck.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/var.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/flock.h>
#include <sys/elf.h>
#include <sys/fs/ufs_inode.h>
#include <sys/flock_impl.h>
#include "crash.h"

static void prilcks();
static void prlcks(int, void *);

struct procid {				/* effective ids */
	pid_t epid;
	int valid;
};

static struct procid *procptr;		/* pointer to procid structure */

/* get effective and sys ids into table */
static void
getprocid()
{
	struct proc *prp, prbuf;
	struct pid pid;
	static int lckinit = 0;
	int i;

	if (lckinit == 0) {
		procptr = (struct procid *)malloc((sizeof (struct procid) *
		    vbuf.v_proc));
		lckinit = 1;
	}

	for (i = 0; i < vbuf.v_proc; i++) {
		prp = slot_to_proc(i);
		if (prp == NULL)
			procptr[i].valid = 0;
		else {
			readmem(prp, 1, &prbuf, sizeof (proc_t),
			    "proc table");
			readmem(prbuf.p_pidp, 1, &pid, sizeof (struct pid),
			    "pid table");
			procptr[i].epid = pid.pid_id;
			procptr[i].valid = 1;
		}
	}
}

/* find process with same id and sys id */
static int
findproc(pid)
pid_t pid;
{
	int slot;

	for (slot = 0; slot < vbuf.v_proc; slot++)
		if ((procptr[slot].valid) &&
		    (procptr[slot].epid == pid))
			return (slot);
	return (-1);
}

/* get arguments for lck function */
int
getlcks()
{
	int phys = 0;
	long addr = -1;
	int c;

	optind = 1;
	while ((c = getopt(argcnt, args, "epw:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			case 'p' :	phys = 1;
					break;
			case 'e' :
					break;
			default  :	longjmp(syn, 0);
		}
	}
	getprocid();

	if (args[optind]) {
		do {
			if ((addr = strcon(args[optind], 'h')) == -1)
				error("\n");
			fprintf(fp,
"TYP WHENCE  START    LEN    PID   FLAGS STATE PREV     NEXT\n");
			prlcks(phys, (void *)addr);
		} while (args[++optind]);
	} else
		prilcks();
	return (0);
}

static int lck_active;
static int lck_sleep;

static void
kmprilcks(void *kaddr, void *buf)
{
	struct inode	*ip = buf;
	lock_descriptor_t *head, *lptr;
	lock_descriptor_t fibuf;
	char hexbuf[20], *str;
	char *vnodeptr;

	if (ip->i_mode == 0)
		return;
	if (ip->i_vnode.v_filocks == NULL)
		return;

	head = (lock_descriptor_t *)ip->i_vnode.v_filocks;

	lptr = head;
	readmem(lptr, 1, &fibuf, sizeof (fibuf), "filock info");
	vnodeptr = (char *)fibuf.l_vnode;

	do {

		fprintf(fp, "%8lx", (long)kaddr);

		if (fibuf.l_status == FLK_ACTIVE_STATE)
			lck_active++;
		else
			lck_sleep++;

		if (fibuf.l_type == F_RDLCK)
			str = "r";
		else if (fibuf.l_type == F_WRLCK)
			str = "w";
		else
			str = "?";

		if ((fibuf.l_end == MAXEND) || (fibuf.l_end == MAX_U_OFF_T))
			strcpy(hexbuf, "MAXEND");
		else
			sprintf(hexbuf, "%llx", fibuf.l_end);
		fprintf(fp, "  %s   %-8llx %-8s %-4d   %-5d %04x %-5d",
			str,
			fibuf.l_start,
			hexbuf,
			findproc(fibuf.l_flock.l_pid),
			fibuf.l_flock.l_pid,
			fibuf.l_state,
			fibuf.l_status);
		fprintf(fp, "   %-8x %-8x",
				fibuf.l_prev,
				fibuf.l_next);
		fprintf(fp, " %-8x\n", lptr);
		lptr = fibuf.l_next;
		readmem(lptr, 1, &fibuf, sizeof (fibuf), "filock info");
	} while (((char *)fibuf.l_vnode == vnodeptr));
}

/* print lock information relative to ufs_inodes (default) */
static void
prilcks()
{
	lck_active = 0;
	lck_sleep = 0;

	fprintf(fp, "\nActive and Sleep Locks:\n");
	fprintf(fp, "INO      TYP  START    END      PROC   PID  FLAGS "
	    "STATE   PREV     NEXT     LOCK\n");

	kmem_cache_apply(kmem_cache_find("ufs_inode_cache"), kmprilcks);

	fprintf(fp, "\nSummary From List:\n");
	fprintf(fp, " TOTAL    ACTIVE  SLEEP\n");
	fprintf(fp, " %4d    %4d    %4d\n",
		lck_active+lck_sleep,
		lck_active,
		lck_sleep);
}

/* print linked list of locks */
static void
prlcks(int phys, void *addr)
{
	struct lock_descriptor fibuf;

	readbuf(addr, 0, phys, (char *)&fibuf, sizeof (fibuf), "frlock");
	if (fibuf.l_flock.l_type == F_RDLCK)
		fprintf(fp, " r ");
	else if (fibuf.l_flock.l_type == F_WRLCK)
		fprintf(fp, " w ");
	else if (fibuf.l_flock.l_type == F_UNLCK)
		fprintf(fp, " u ");
	else
		fprintf(fp, " - ");
	fprintf(fp, " %1d       %-8llx %-6d %-6d %04x %-5d %-8x %-8x\n",
		fibuf.l_flock.l_whence,
		fibuf.l_flock.l_start,
		fibuf.l_flock.l_len,
		fibuf.l_flock.l_pid,
		fibuf.l_state,
		fibuf.l_status,
		fibuf.l_prev,
		fibuf.l_next);
}
