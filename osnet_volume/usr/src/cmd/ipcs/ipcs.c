/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ipcs.c	1.27	99/10/07 SMI"

/*
 * ipcs - IPC status
 *
 * Examine and print certain things about
 * message queues, semaphores and shared memory.
 *
 * As of SVR4, IPC information is obtained via msgctl, semctl and shmctl
 * to the extent possible.  /dev/kmem is used only to obtain configuration
 * information and to determine the IPC identifiers present in the system.
 * This change ensures that the information in each msgid_ds, semid_ds or
 * shmid_ds data structure that we obtain is complete and consistent.
 * For example, the shm_nattch field of a shmid_ds data structure is
 * only guaranteed to be meaningful when obtained via shmctl; when read
 * directly from /dev/kmem, it may contain garbage.
 * If the user supplies an alternate corefile (using -C), no attempt is
 * made to obtain information using msgctl/semctl/shmctl.
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/vnode.h>
#include <sys/param.h>
#include <sys/var.h>
#include <kvm.h>
#include <nlist.h>
#include <sys/elf.h>
#include <fcntl.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <langinfo.h>
#include <string.h>

#define		SYS5	0
#define		SYS3	1

#define	TIME	0
#define	MSG	1
#define	SEM	2
#define	SHM	3
#define	MSGINFO	4
#define	SEMINFO	5
#define	SHMINFO	6

#define	USAGE "usage: ipcs [-Aabcimopqst] [-C corefile] [-N namelist]\n"

/*
 * Given an index into an IPC table (message table, shared memory
 * table) determine the corresponding IPC
 * identifier (msgid, shmid).  This requires knowledge of
 * the table size, the corresponding ipc_perm structure (for the
 * sequence number contained therein) and the undocumented method
 * by which the kernel assigns new identifiers.
 */
#define	IPC_ID(tblsize, index, permp)	((index) + (tblsize) * (permp)->seq)

/*
 * Semaphore id's are generated as 2 16 bit values, the index and sequence
 * number.
 */
#define	SEMA_SEQ_SHIFT	16
#define	SEM_ID(index, permp)	(((permp)->seq << SEMA_SEQ_SHIFT) + index)

static struct nlist nl[] = {	/* name list entries for IPC facilities */
	{"time"},
	{"msgque"},
	{"sema"},
	{"shmem"},
	{"msginfo"},
	{"seminfo"},
	{"shminfo"},
	{NULL}
};

static char chdr[] = "T         ID      KEY        MODE        OWNER    GROUP",
				/* common header format */
	chdr2[] = "  CREATOR   CGROUP",
				/* c option header format */
	*name = NULL,		/* name list file */
	*mem = NULL,		/* memory file */
	opts[] = "AabcimopqstC:N:"; /* allowable options for getopt */

static int	bflg,		/* biggest size: */
				/*	segsz on m; qbytes on q; nsems on s */
		cflg,		/* creator's login and group names */
		iflg,		/* ISM attaches */
		mflg,		/* shared memory status */
		oflg,		/* outstanding data: */
				/*	nattch on m; cbytes, qnum on q */
		pflg,		/* process id's: lrpid, lspid on q; */
				/*	cpid, lpid on m */
		qflg,		/* message queue status */
		sflg,		/* semaphore status */
		tflg,		/* times: atime, ctime, dtime on m;	*/
				/*	ctime, rtime, stime on q;	*/
				/*	ctime, otime on s */
		Cflg,		/* user supplied corefile */
		Nflg,		/* user supplied namelist */

		err;		/* option error count */

static kvm_t *kd;

static int reade(kvm_t *, uintptr_t, void *, size_t);
static void hp(char, char *, struct ipc_perm *, int, int, int);
static void tp(time_t);

int
main(int argc, char *argv[])
{
	int	o,	/* option flag */
		id;	/* IPC identifier */
	long	i, n;	/* table size */
	time_t	time;	/* date in memory file */
	char	tbuf[BUFSIZ];
	char	*dfmt;  /* date format pointer */

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	(void) memset(tbuf, 0, sizeof (tbuf));
	dfmt = nl_langinfo(_DATE_FMT);

	/* Go through the options and set flags. */
	while ((o = getopt(argc, argv, opts)) != EOF)
		switch (o) {
		case 'A':
			bflg = cflg = iflg = oflg = pflg = tflg = 1;
			break;
		case 'a':
			bflg = cflg = oflg = pflg = tflg = 1;
			break;
		case 'b':
			bflg = 1;
			break;
		case 'c':
			cflg = 1;
			break;
		case 'C':
			mem = optarg;
			Cflg = 1;
			break;
		case 'i':
			iflg = 1;
			break;
		case 'm':
			mflg = 1;
			break;
		case 'N':
			name = optarg;
			Nflg = 1;
			break;
		case 'o':
			oflg = 1;
			break;
		case 'p':
			pflg = 1;
			break;
		case 'q':
			qflg = 1;
			break;
		case 's':
			sflg = 1;
			break;
		case 't':
			tflg = 1;
			break;
		case '?':
			err++;
			break;
		}
	if (err || (optind < argc)) {
		(void) fprintf(stderr, gettext(USAGE));
		exit(1);
	}

	/*
	 * If the user supplied either the corefile or namelist then
	 * reset the uid/gid to the user invoking this command.
	 */
	if (Cflg || Nflg) {
		(void) setuid(getuid());
		(void) setgid(getgid());
	}

	if ((mflg + qflg + sflg) == 0)
		mflg = qflg = sflg = 1;

	/* Check out namelist and memory files. */
	if ((kd = kvm_open(name, mem, NULL, O_RDONLY, argv[0])) == NULL)
		exit(1);

	if (kvm_nlist(kd, nl) == -1) {
		perror(gettext("ipcs: can't read namelist"));
		exit(1);
	}

	if (nl[TIME].n_value == 0) {
		perror(gettext("ipcs: no namelist"));
		exit(1);
	}
	(void) reade(kd, nl[TIME].n_value, &time, sizeof (time));
	(void) cftime(tbuf, dfmt, &time);
	(void) printf(gettext("IPC status from %s as of %s\n"),
	    mem ? mem : "<running system>", tbuf);

	/*
	 * Print Message Queue status report.
	 */
	if (qflg) {
		struct msqid_ds	qds;	/* message queue data */
		struct msginfo msginfo;	/* message queue information */
		struct msqid_ds	*pmsgque;
		uintptr_t addr;

		if (nl[MSG].n_value && nl[MSGINFO].n_value &&
		    reade(kd, nl[MSGINFO].n_value, &msginfo,
		    sizeof (msginfo)) &&
		    reade(kd, nl[MSG].n_value, &pmsgque, sizeof (pmsgque)) &&
		    pmsgque != 0 && (n = msginfo.msgmni) != 0) {

			(void) printf("%s%s%s%s%s%s\nMessage Queues:\n", chdr,
			    cflg ? chdr2 : "",
			    oflg ? " CBYTES  QNUM" : "",
			    bflg ? " QBYTES" : "",
			    pflg ? " LSPID LRPID" : "",
			    tflg ?
			    "   STIME    RTIME    CTIME " : "");
		} else {
#ifdef XPG4
			(void) printf(gettext("Message Queue"
						" facility not in system.\n"));
#else
			(void) printf(gettext("Message Queue"
						" facility inactive.\n"));
#endif
			qflg = 0;
			n = 0;
		}

		for (i = 0, addr = (uintptr_t)pmsgque; i < n;
		    i++, addr += sizeof (qds)) {

			(void) reade(kd, addr, &qds, sizeof (qds));
			if ((qds.msg_perm.mode & IPC_ALLOC) == 0)
				continue;

			id = IPC_ID(n, i, &qds.msg_perm);
			if (!Cflg && msgctl(id, IPC_STAT, &qds) < 0)
				continue;

			hp('q', "SRrw-rw-rw-", &qds.msg_perm, id, SYS5, SYS5);
			if (oflg)
				(void) printf(" %6lu %5lu",
				    qds.msg_cbytes, qds.msg_qnum);
			if (bflg)
				(void) printf(" %6lu", qds.msg_qbytes);
			if (pflg)
				(void) printf(" %5d %5d",
				    qds.msg_lspid, qds.msg_lrpid);
			if (tflg) {
				tp(qds.msg_stime);
				tp(qds.msg_rtime);
				tp(qds.msg_ctime);
			}
			(void) printf("\n");
		}
	}

	/*
	 * Print Shared Memory status report.
	 */
	if (mflg) {
		struct shmid_ds	mds;	/* shared memory data */
		struct shminfo shminfo;	/* shared memory information */
		struct shmid_ds *pshm;
		uintptr_t addr;

		if (nl[SHM].n_value && nl[SHMINFO].n_value &&
		    reade(kd, nl[SHMINFO].n_value, &shminfo,
		    sizeof (shminfo)) &&
		    reade(kd, nl[SHM].n_value, &pshm, sizeof (pshm)) &&
		    pshm != 0 && (n = shminfo.shmmni) != 0) {

			if (!qflg || oflg || bflg || pflg || tflg || iflg)
				(void) printf("%s%s%s%s%s%s%s\n", chdr,
				    cflg ? chdr2 : "",
				    oflg ? " NATTCH" : "",
				    bflg ? "      SEGSZ" : "",
				    pflg ? "  CPID  LPID" : "",
				    tflg ? "   ATIME    DTIME    CTIME " : "",
				    iflg ? " ISMATTCH" : "");
			(void) printf(gettext("Shared Memory:\n"));
		} else {
#ifdef XPG4
			(void) printf(gettext("Shared Memory"
						" facility not in system.\n"));
#else
			(void) printf(gettext("Shared Memory"
						" facility inactive.\n"));
#endif
			mflg = 0;
			n = 0;
		}

		for (i = 0, addr = (uintptr_t)pshm;
		    i < n; i++, addr += sizeof (mds)) {

			(void) reade(kd, addr, &mds, sizeof (mds));
			if ((mds.shm_perm.mode & IPC_ALLOC) == 0)
				continue;
			id = IPC_ID(n, i, &mds.shm_perm);
			if (!Cflg && shmctl(id, IPC_STAT, &mds) < 0)
				continue;
			hp('m', "DCrw-rw-rw-", &mds.shm_perm, id, SYS5, SYS5);
			if (oflg)
				(void) printf(" %6lu", mds.shm_nattch);
			if (bflg)
				(void) printf(" %10lu", mds.shm_segsz);
			if (pflg)
				(void) printf(" %5d %5d",
				    mds.shm_cpid, mds.shm_lpid);
			if (tflg) {
				tp(mds.shm_atime);
				tp(mds.shm_dtime);
				tp(mds.shm_ctime);
			}
			if (iflg)
				(void) printf(" %8lu", mds.shm_cnattch);
			(void) printf("\n");
		}
	}

	/*
	 * Print Semaphore facility status.
	 */
	if (sflg) {
		struct semid_ds	sds;	/* semaphore data structure */
		struct seminfo seminfo;	/* semaphore information */
		struct semid_ds *psem;
		uintptr_t addr;
		union semun {
			int val;
			struct semid_ds *buf;
			ushort *array;
		} semarg;

		if (nl[SEM].n_value && nl[SEMINFO].n_value &&
		    reade(kd, nl[SEMINFO].n_value, &seminfo,
		    sizeof (seminfo)) &&
		    reade(kd, nl[SEM].n_value, &psem, sizeof (psem)) &&
		    psem != 0 && (n = seminfo.semmni) != 0) {

			if (bflg || tflg || (!qflg && !mflg))
				(void) printf("%s%s%s%s\n", chdr,
				    cflg ? chdr2 : "",
#ifdef XPG4
				    (bflg || tflg) ? " NSEMS" : "",
#else
				    bflg ? " NSEMS" : "",
#endif
				    tflg ? "   OTIME    CTIME " : "");
			(void) printf(gettext("Semaphores:\n"));
		} else {
#ifdef XPG4
			(void) printf(gettext("Semaphore facility"
							" not in system.\n"));
#else
			(void) printf(gettext("Semaphore facility"
							" inactive.\n"));
#endif
			n = 0;
		}

		for (i = 0, addr = (uintptr_t)psem; i < n;
		    i++, addr += sizeof (sds)) {

			(void) reade(kd, addr, &sds, sizeof (sds));
			if ((sds.sem_perm.mode & IPC_ALLOC) == 0)
				continue;
			id = SEM_ID(i, &sds.sem_perm);
			semarg.buf = &sds;
			if (Cflg ||
			    semctl(id, 0, IPC_STAT, semarg) == 0) {
				hp('s', "--ra-ra-ra-",
				    &sds.sem_perm, id, SYS5, SYS5);
#ifdef XPG4
				if (bflg || tflg)
#else
				if (bflg)
#endif
					(void) printf(" %5u", sds.sem_nsems);
				if (tflg) {
					tp(sds.sem_otime);
					tp(sds.sem_ctime);
				}
				(void) printf("\n");
			}
		}
	}

	return (0);
}

/*
 * reade - read kernel memory with error exit
 */
int
reade(kvm_t *kd, uintptr_t addr, void *b, size_t s)
{
	if (kvm_kread(kd, addr, b, s) != s) {
		perror(gettext("ipcs: read error"));
		exit(1);
	}
	return (1);
}

/*
 * hp - common header print
 */
/*ARGSUSED4*/
void
hp(
	char		type,	/* facility type */
	char		*modesp, /* ptr to mode replacement characters */
	struct ipc_perm	*permp,	/* ptr to permission structure */
	int		slot,	/* facility slot number */
	int		slots,	/* # of facility slots */
	int		sys3)	/* system 5 vs. system 3 */
{
	int		i;	/* loop control */
	struct group	*g;	/* ptr to group group entry */
	struct passwd	*u;	/* ptr to user passwd entry */
	char		keyfield[13];

	if (sys3)
		(void) printf("%c%s%s", type, "    x	  ", "xenix    ");
	else {
		(void) sprintf(keyfield, "  %#x", permp->key);
		(void) printf("%c %10d %-13s", type, slot, keyfield);
	}

	for (i = 02000; i; modesp++, i >>= 1)
		(void) printf("%c", ((int)permp->mode & i) ? *modesp : '-');
	if ((u = getpwuid(permp->uid)) == NULL)
		(void) printf("%9d", permp->uid);
	else
		(void) printf("%9.8s", u->pw_name);
	if ((g = getgrgid(permp->gid)) == NULL)
		(void) printf("%9d", permp->gid);
	else
		(void) printf("%9.8s", g->gr_name);

	if (cflg) {
		if ((u = getpwuid(permp->cuid)) == NULL)
			(void) printf("%9d", permp->cuid);
		else
			(void) printf("%9.8s", u->pw_name);
		if ((g = getgrgid(permp->cgid)) == NULL)
			(void) printf("%9d", permp->cgid);
		else
			(void) printf("%9.8s", g->gr_name);
	}
}

/*
 * tp - time entry printer
 */
void
tp(time_t time)
{
	struct tm *t;	/* ptr to converted time */

	if (time) {
		t = localtime(&time);
		(void) printf(" %2d:%2.2d:%2.2d",
		    t->tm_hour, t->tm_min, t->tm_sec);
	} else
		(void) printf(gettext(" no-entry"));
}
