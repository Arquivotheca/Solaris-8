/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)shmsys.c	1.13	97/04/06 SMI"

/*LINTLIBRARY*/

#pragma weak shmat = _shmat
#pragma weak shmctl = _shmctl
#pragma weak shmdt = _shmdt
#pragma weak shmget = _shmget

#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/ipc.h>
#include	<sys/shm.h>
#include	<sys/syscall.h>


#define	SHMAT	0
#define	SHMCTL	1
#define	SHMDT	2
#define	SHMGET	3

#ifdef _LP64
#error	"converted to explicit trap version"
#endif

void *
shmat(int shmid, const void *shmaddr, int shmflg)
{
	return ((void *)syscall(SYS_shmsys, SHMAT, shmid, shmaddr, shmflg));
}

int
shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
	return (syscall(SYS_shmsys, SHMCTL, shmid, cmd, buf));
}

int
shmdt(char *shmaddr)
{
	return (syscall(SYS_shmsys, SHMDT, shmaddr));
}

int
shmget(key_t key, size_t size, int shmflg)
{
	return (syscall(SYS_shmsys, SHMGET, key, size, shmflg));
}
