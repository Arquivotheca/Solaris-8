/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pr_stat.c	1.2	99/03/23 SMI"

#include <sys/isa_defs.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include "libproc.h"

#ifdef _LP64
static void
stat_32_to_n(struct stat32 *src, struct stat *dest)
{
	(void) memset(dest, 0, sizeof (*dest));
	dest->st_dev = DEVEXPL(src->st_dev);
	dest->st_ino = (ino_t)src->st_ino;
	dest->st_mode = (mode_t)src->st_mode;
	dest->st_nlink = (nlink_t)src->st_nlink;
	dest->st_uid = (uid_t)src->st_uid;
	dest->st_gid = (gid_t)src->st_gid;
	dest->st_rdev = DEVEXPL(src->st_rdev);
	dest->st_size = (off_t)(uint32_t)src->st_size;
	TIMESPEC32_TO_TIMESPEC(&dest->st_atim, &src->st_atim);
	TIMESPEC32_TO_TIMESPEC(&dest->st_mtim, &src->st_mtim);
	TIMESPEC32_TO_TIMESPEC(&dest->st_ctim, &src->st_ctim);
	dest->st_blksize = (int)src->st_blksize;
	dest->st_blocks = (blkcnt_t)(uint32_t)src->st_blocks;
	(void) memcpy(dest->st_fstype, src->st_fstype,
	    sizeof (dest->st_fstype));
}
#endif	/* _LP64 */

/*
 * stat() system call -- executed by subject process
 */
int
pr_stat(struct ps_prochandle *Pr, const char *path, struct stat *buf)
{
	sysret_t rval;			/* return value from stat() */
	argdes_t argd[3];		/* arg descriptors for stat() */
	argdes_t *adp = &argd[0];	/* first argument */
	int syscall;			/* which syscall, stat or xstat */
	int nargs;			/* number of actual arguments */
#ifdef _LP64
	struct stat32 statb32;
#endif	/* _LP64 */

	if (Pr == NULL)		/* no subject process */
		return (stat(path, buf));

	/*
	 * This is filthy, but /proc reveals everything about the
	 * system call interfaces, despite what the architects of the
	 * header files may desire.  We have to know here whether we
	 * are calling stat() or xstat() in the subject.
	 */
#if defined(_STAT_VER)
	syscall = SYS_xstat;
	nargs = 3;
	adp->arg_value = _STAT_VER;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;
	adp++;			/* move to pathname argument */
#else			/* newest version of stat(2) */
	syscall = SYS_stat;
	nargs = 2;
#endif

	adp->arg_value = 0;
	adp->arg_object = (void *)path;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = strlen(path)+1;
	adp++;			/* move to buffer argument */

	adp->arg_value = 0;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_OUTPUT;
#ifdef _LP64
	if (Pstatus(Pr)->pr_dmodel == PR_MODEL_ILP32) {
		adp->arg_object = &statb32;
		adp->arg_size = sizeof (statb32);
	} else {
		adp->arg_object = buf;
		adp->arg_size = sizeof (*buf);
	}
#else	/* _LP64 */
	adp->arg_object = buf;
	adp->arg_size = sizeof (*buf);
#endif	/* _LP64 */

	rval = Psyscall(Pr, syscall, nargs, &argd[0]);

	if (rval.sys_errno < 0)
		rval.sys_errno = ENOSYS;

	if (rval.sys_errno) {
		errno = rval.sys_errno;
		return (-1);
	}

#ifdef _LP64
	if (Pstatus(Pr)->pr_dmodel == PR_MODEL_ILP32)
		stat_32_to_n(&statb32, buf);
#endif	/* _LP64 */
	return (0);
}

/*
 * lstat() system call -- executed by subject process
 */
int
pr_lstat(struct ps_prochandle *Pr, const char *path, struct stat *buf)
{
	sysret_t rval;			/* return value from lstat() */
	argdes_t argd[3];		/* arg descriptors for lstat() */
	argdes_t *adp = &argd[0];	/* first argument */
	int syscall;			/* which syscall, lstat or lxstat */
	int nargs;			/* number of actual arguments */
#ifdef _LP64
	struct stat32 statb32;
#endif	/* _LP64 */

	if (Pr == NULL)		/* no subject process */
		return (lstat(path, buf));

	/*
	 * This is filthy, but /proc reveals everything about the
	 * system call interfaces, despite what the architects of the
	 * header files may desire.  We have to know here whether we
	 * are calling lstat() or lxstat() in the subject.
	 */
#if defined(_STAT_VER)
	syscall = SYS_lxstat;
	nargs = 3;
	adp->arg_value = _STAT_VER;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;
	adp++;			/* move to pathname argument */
#else			/* newest version of lstat(2) */
	syscall = SYS_lstat;
	nargs = 2;
#endif

	adp->arg_value = 0;
	adp->arg_object = (void *)path;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = strlen(path)+1;
	adp++;			/* move to buffer argument */

	adp->arg_value = 0;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_OUTPUT;
#ifdef _LP64
	if (Pstatus(Pr)->pr_dmodel == PR_MODEL_ILP32) {
		adp->arg_object = &statb32;
		adp->arg_size = sizeof (statb32);
	} else {
		adp->arg_object = buf;
		adp->arg_size = sizeof (*buf);
	}
#else	/* _LP64 */
	adp->arg_object = buf;
	adp->arg_size = sizeof (*buf);
#endif	/* _LP64 */

	rval = Psyscall(Pr, syscall, nargs, &argd[0]);

	if (rval.sys_errno < 0)
		rval.sys_errno = ENOSYS;

	if (rval.sys_errno) {
		errno = rval.sys_errno;
		return (-1);
	}

#ifdef _LP64
	if (Pstatus(Pr)->pr_dmodel == PR_MODEL_ILP32)
		stat_32_to_n(&statb32, buf);
#endif	/* _LP64 */
	return (0);
}

/*
 * fstat() system call -- executed by subject process
 */
int
pr_fstat(struct ps_prochandle *Pr, int fd, struct stat *buf)
{
	sysret_t rval;			/* return value from fstat() */
	argdes_t argd[3];		/* arg descriptors for fstat() */
	argdes_t *adp = &argd[0];	/* first argument */
	int syscall;			/* which syscall, fstat or fxstat */
	int nargs;			/* number of actual arguments */
#ifdef _LP64
	struct stat32 statb32;
#endif	/* _LP64 */

	if (Pr == NULL)		/* no subject process */
		return (fstat(fd, buf));

	/*
	 * This is filthy, but /proc reveals everything about the
	 * system call interfaces, despite what the architects of the
	 * header files may desire.  We have to know here whether we
	 * are calling fstat() or fxstat() in the subject.
	 */
#if defined(_STAT_VER)
	syscall = SYS_fxstat;
	nargs = 3;
	adp->arg_value = _STAT_VER;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;
	adp++;			/* move to fd argument */
#else			/* newest version of fstat(2) */
	syscall = SYS_fstat;
	nargs = 2;
#endif

	adp->arg_value = fd;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;
	adp++;			/* move to buffer argument */

	adp->arg_value = 0;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_OUTPUT;
#ifdef _LP64
	if (Pstatus(Pr)->pr_dmodel == PR_MODEL_ILP32) {
		adp->arg_object = &statb32;
		adp->arg_size = sizeof (statb32);
	} else {
		adp->arg_object = buf;
		adp->arg_size = sizeof (*buf);
	}
#else	/* _LP64 */
	adp->arg_object = buf;
	adp->arg_size = sizeof (*buf);
#endif	/* _LP64 */

	rval = Psyscall(Pr, syscall, nargs, &argd[0]);

	if (rval.sys_errno < 0)
		rval.sys_errno = ENOSYS;

	if (rval.sys_errno) {
		errno = rval.sys_errno;
		return (-1);
	}

#ifdef _LP64
	if (Pstatus(Pr)->pr_dmodel == PR_MODEL_ILP32)
		stat_32_to_n(&statb32, buf);
#endif	/* _LP64 */
	return (0);
}
