/*
 * Copyright (c) 1992-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)xstat.c	1.16	97/12/23 SMI"	/* SVr4.0 1.3	*/

#define	_SYSCALL32

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <libproc.h>
#include "ramdata.h"
#include "proto.h"

static	void	show_stat32(struct ps_prochandle *, long);
#ifdef _LP64
static	void	show_stat64(struct ps_prochandle *, long);
#endif

#if defined(i386) && defined(_STAT_VER)

/*
 * Old SVR3 stat structure.
 */
struct	o_stat {
	o_dev_t	st_dev;
	o_ino_t	st_ino;
	o_mode_t st_mode;
	o_nlink_t st_nlink;
	o_uid_t st_uid;
	o_gid_t st_gid;
	o_dev_t	st_rdev;
	off32_t	st_size;
	time_t st_atim;
	time_t st_mtim;
	time_t st_ctim;
};

static void
show_o_stat(struct ps_prochandle *Pr, long offset)
{
	struct o_stat statb;
	timestruc_t ts;

	if (offset != NULL &&
	    Pread(Pr, &statb, sizeof (statb), offset) == sizeof (statb)) {
		(void) printf(
		    "%s    d=0x%.8X i=%-5u m=0%.6o l=%-2u u=%-5u g=%-5u",
		    pname,
		    statb.st_dev & 0xffff,
		    statb.st_ino,
		    statb.st_mode,
		    statb.st_nlink % 0xffff,
		    statb.st_uid,
		    statb.st_gid);

		switch (statb.st_mode & S_IFMT) {
		case S_IFCHR:
		case S_IFBLK:
			(void) printf(" rdev=0x%.4X\n", statb.st_rdev & 0xffff);
			break;
		default:
			(void) printf(" sz=%u\n", (uint32_t)statb.st_size);
			break;
		}

		ts.tv_nsec = 0;
		ts.tv_sec = statb.st_atim;
		prtimestruc("at = ", &ts);
		ts.tv_sec = statb.st_atim;
		prtimestruc("mt = ", &ts);
		ts.tv_sec = statb.st_atim;
		prtimestruc("ct = ", &ts);
	}
}

void
show_stat(struct ps_prochandle *Pr, long offset)
{
	show_o_stat(Pr, offset);
}

void
show_xstat(struct ps_prochandle *Pr, int version, long offset)
{
	switch (version) {
	case _R3_STAT_VER:
		show_o_stat(Pr, offset);
		break;
	case _STAT_VER:
		show_stat32(Pr, offset);
		break;
	}
}

#else

void
show_stat(struct ps_prochandle *Pr, long offset)
{
#ifdef _LP64
	if (data_model == PR_MODEL_LP64)
		show_stat64(Pr, offset);
	else
		show_stat32(Pr, offset);
#else
	show_stat32(Pr, offset);
#endif
}

/* ARGSUSED */
void
show_xstat(struct ps_prochandle *Pr, int version, long offset)
{
	show_stat(Pr, offset);
}

#endif

static void
show_stat32(struct ps_prochandle *Pr, long offset)
{
	struct stat32 statb;
	timestruc_t ts;

	if (offset != NULL &&
	    Pread(Pr, &statb, sizeof (statb), offset) == sizeof (statb)) {
		(void) printf(
		    "%s    d=0x%.8X i=%-5u m=0%.6o l=%-2u u=%-5u g=%-5u",
		    pname,
		    statb.st_dev,
		    statb.st_ino,
		    statb.st_mode,
		    statb.st_nlink,
		    statb.st_uid,
		    statb.st_gid);

		switch (statb.st_mode & S_IFMT) {
		case S_IFCHR:
		case S_IFBLK:
			(void) printf(" rdev=0x%.8X\n", statb.st_rdev);
			break;
		default:
			(void) printf(" sz=%u\n", statb.st_size);
			break;
		}

		TIMESPEC32_TO_TIMESPEC(&ts, &statb.st_atim);
		prtimestruc("at = ", &ts);
		TIMESPEC32_TO_TIMESPEC(&ts, &statb.st_mtim);
		prtimestruc("mt = ", &ts);
		TIMESPEC32_TO_TIMESPEC(&ts, &statb.st_ctim);
		prtimestruc("ct = ", &ts);

		(void) printf(
		    "%s    bsz=%-5d blks=%-5d fs=%.*s\n",
		    pname,
		    statb.st_blksize,
		    statb.st_blocks,
		    _ST_FSTYPSZ,
		    statb.st_fstype);
	}
}

void
show_stat64_32(struct ps_prochandle *Pr, long offset)
{
	struct stat64_32 statb;
	timestruc_t ts;

	if (offset != NULL &&
	    Pread(Pr, &statb, sizeof (statb), offset) == sizeof (statb)) {
		(void) printf(
		    "%s    d=0x%.8X i=%-5llu m=0%.6o l=%-2u u=%-5u g=%-5u",
		    pname,
		    statb.st_dev,
		    (u_longlong_t)statb.st_ino,
		    statb.st_mode,
		    statb.st_nlink,
		    statb.st_uid,
		    statb.st_gid);

		switch (statb.st_mode & S_IFMT) {
		case S_IFCHR:
		case S_IFBLK:
			(void) printf(" rdev=0x%.8X\n", statb.st_rdev);
			break;
		default:
			(void) printf(" sz=%llu\n", (long long)statb.st_size);
			break;
		}

		TIMESPEC32_TO_TIMESPEC(&ts, &statb.st_atim);
		prtimestruc("at = ", &ts);
		TIMESPEC32_TO_TIMESPEC(&ts, &statb.st_mtim);
		prtimestruc("mt = ", &ts);
		TIMESPEC32_TO_TIMESPEC(&ts, &statb.st_ctim);
		prtimestruc("ct = ", &ts);

		(void) printf("%s    bsz=%-5d blks=%-5lld fs=%.*s\n",
		    pname,
		    statb.st_blksize,
		    (longlong_t)statb.st_blocks,
		    _ST_FSTYPSZ,
		    statb.st_fstype);
	}
}

#ifdef _LP64
static void
show_stat64(struct ps_prochandle *Pr, long offset)
{
	struct stat64 statb;

	if (offset != NULL &&
	    Pread(Pr, &statb, sizeof (statb), offset) == sizeof (statb)) {
		(void) printf(
		    "%s    d=0x%.16lX i=%-5lu m=0%.6o l=%-2u u=%-5u g=%-5u",
		    pname,
		    statb.st_dev,
		    statb.st_ino,
		    statb.st_mode,
		    statb.st_nlink,
		    statb.st_uid,
		    statb.st_gid);

		switch (statb.st_mode & S_IFMT) {
		case S_IFCHR:
		case S_IFBLK:
			(void) printf(" rdev=0x%.16lX\n", statb.st_rdev);
			break;
		default:
			(void) printf(" sz=%lu\n", statb.st_size);
			break;
		}

		prtimestruc("at = ", (timestruc_t *)&statb.st_atim);
		prtimestruc("mt = ", (timestruc_t *)&statb.st_mtim);
		prtimestruc("ct = ", (timestruc_t *)&statb.st_ctim);

		(void) printf(
		    "%s    bsz=%-5d blks=%-5ld fs=%.*s\n",
		    pname,
		    statb.st_blksize,
		    statb.st_blocks,
		    _ST_FSTYPSZ,
		    statb.st_fstype);
	}
}
#endif	/* _LP64 */
