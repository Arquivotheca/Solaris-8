/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pr_lseek.c	1.2	99/03/23 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "libproc.h"

typedef union {
	offset_t	full;		/* full 64 bit offset value */
	uint32_t	half[2];	/* two 32-bit halves */
} offsets_t;

/*
 * lseek() system call -- executed by subject process.
 */
off_t
pr_lseek(struct ps_prochandle *Pr, int filedes, off_t offset, int whence)
{
	int syscall;		/* SYS_lseek or SYS_llseek */
	int nargs;		/* 3 or 4, depending on syscall */
	offsets_t off;
	sysret_t rval;		/* return value from lseek() */
	argdes_t argd[4];	/* arg descriptors for lseek() */
	argdes_t *adp;

	if (Pr == NULL)
		return (lseek(filedes, offset, whence));

	adp = &argd[0];		/* filedes argument */
	adp->arg_value = filedes;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* offset argument */
	if (Pstatus(Pr)->pr_dmodel == PR_MODEL_NATIVE) {
		syscall = SYS_lseek;
		nargs = 3;
		adp->arg_value = offset;
		adp->arg_object = NULL;
		adp->arg_type = AT_BYVAL;
		adp->arg_inout = AI_INPUT;
		adp->arg_size = 0;
	} else {
		syscall = SYS_llseek;
		nargs = 4;
		off.full = offset;
		adp->arg_value = off.half[0];	/* first 32 bits */
		adp->arg_object = NULL;
		adp->arg_type = AT_BYVAL;
		adp->arg_inout = AI_INPUT;
		adp->arg_size = 0;
		adp++;
		adp->arg_value = off.half[1];	/* second 32 bits */
		adp->arg_object = NULL;
		adp->arg_type = AT_BYVAL;
		adp->arg_inout = AI_INPUT;
		adp->arg_size = 0;
	}

	adp++;			/* whence argument */
	adp->arg_value = whence;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	rval = Psyscall(Pr, syscall, nargs, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0) ? rval.sys_errno : ENOSYS;
		return (-1);
	}

	if (Pstatus(Pr)->pr_dmodel == PR_MODEL_NATIVE)
		offset = rval.sys_rval1;
	else {
		off.half[0] = (uint32_t)rval.sys_rval1;
		off.half[1] = (uint32_t)rval.sys_rval2;
		offset = (off_t)off.full;
	}

	return (offset);
}

/*
 * llseek() system call -- executed by subject process.
 */
offset_t
pr_llseek(struct ps_prochandle *Pr, int filedes, offset_t offset, int whence)
{
	int syscall;		/* SYS_lseek or SYS_llseek */
	int nargs;		/* 3 or 4, depending on syscall */
	offsets_t off;
	sysret_t rval;		/* return value from llseek() */
	argdes_t argd[4];	/* arg descriptors for llseek() */
	argdes_t *adp;

	if (Pr == NULL)
		return (llseek(filedes, offset, whence));

	adp = &argd[0];		/* filedes argument */
	adp->arg_value = filedes;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* offset argument */
	if (Pstatus(Pr)->pr_dmodel == PR_MODEL_LP64) {
		syscall = SYS_lseek;
		nargs = 3;
		adp->arg_value = offset;
		adp->arg_object = NULL;
		adp->arg_type = AT_BYVAL;
		adp->arg_inout = AI_INPUT;
		adp->arg_size = 0;
	} else {
		syscall = SYS_llseek;
		nargs = 4;
		off.full = offset;
		adp->arg_value = off.half[0];	/* first 32 bits */
		adp->arg_object = NULL;
		adp->arg_type = AT_BYVAL;
		adp->arg_inout = AI_INPUT;
		adp->arg_size = 0;
		adp++;
		adp->arg_value = off.half[1];	/* second 32 bits */
		adp->arg_object = NULL;
		adp->arg_type = AT_BYVAL;
		adp->arg_inout = AI_INPUT;
		adp->arg_size = 0;
	}

	adp++;			/* whence argument */
	adp->arg_value = whence;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	rval = Psyscall(Pr, syscall, nargs, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0) ? rval.sys_errno : ENOSYS;
		return (-1);
	}

	if (Pstatus(Pr)->pr_dmodel == PR_MODEL_LP64)
		offset = rval.sys_rval1;
	else {
		off.half[0] = (uint32_t)rval.sys_rval1;
		off.half[1] = (uint32_t)rval.sys_rval2;
		offset = off.full;
	}

	return (offset);
}
