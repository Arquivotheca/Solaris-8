/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)semsys.c	1.18	97/09/09 SMI"

/*LINTLIBRARY*/
#pragma weak semctl = _semctl
#pragma weak semget = _semget
#pragma weak semop = _semop

#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/ipc.h>
#include	<sys/sem.h>
#include	"libc.h"

#define	SEMCTL	0
#define	SEMGET	1
#define	SEMOP	2

union semun {
	int val;
	struct semid_ds *buf;
	ushort *array;
};


#include <stdarg.h>

/*
 * XXX The kernel implementation of semsys expects a struct containing
 * XXX the "value" of the semun argument, but the compiler passes a
 * XXX pointer to it, since it is a union.  So, we convert here and pass
 * XXX the value, but to keep the naive user from being penalized
 * XXX for the counterintuitive behaviour of the compiler, we ignore
 * XXX the union if it will not be used by the system call (to
 * XXX protect the caller from SIGSEGVs.
 * XXX  e.g. semctl(semid, semnum, cmd, NULL);  which
 * XXX would otherwise always result in a segmentation violation)
 * XXX We do this partly for consistency, since the ICL port did it
 */

int
semctl(int semid, int semnum, int cmd, ...)
{
	uintptr_t arg;
	va_list ap;

	switch (cmd) {
	case SETVAL:
		va_start(ap, cmd);
		arg = (uintptr_t)va_arg(ap, union semun).val;
		va_end(ap);
		break;
	case GETALL:
	case SETALL:
		va_start(ap, cmd);
		arg = (uintptr_t)va_arg(ap, union semun).array;
		va_end(ap);
		break;
	case IPC_STAT:
	case IPC_SET:
		va_start(ap, cmd);
		arg = (uintptr_t)va_arg(ap, union semun).buf;
		va_end(ap);
		break;
	default:
		arg = 0;
		break;
	}

	return (_semsys(SEMCTL, semid, semnum, cmd, arg));
}


int
semget(key_t key, int nsems, int semflg)
{
	return (_semsys(SEMGET, key, nsems, semflg));
}

int
semop(int semid, struct sembuf *sops, size_t nsops)
{
	return (_semsys(SEMOP, semid, sops, nsops));
}
