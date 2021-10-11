/*	Copyright (c) 1993, by Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SEMAPHORE_H
#define	_SEMAPHORE_H

#pragma ident	"@(#)semaphore.h	1.11	98/04/02 SMI"

#include <sys/types.h>
#include <sys/fcntl.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct {
	/* this structure must be the same as sema_t in <synch.h> */
	uint32_t	sem_count;	/* semaphore count */
	uint16_t	sem_type;
	uint16_t	sem_magic;
	upad64_t	sem_pad1[3];	/* reserved for a mutex_t */
	upad64_t 	sem_pad2[2];	/* reserved for a cond_t */
}	sem_t;

#define	SEM_FAILED	((sem_t *)(-1))

/*
 * function prototypes
 */
#if	defined(__STDC__)
int	sem_init(sem_t *, int, unsigned int);
int	sem_destroy(sem_t *);
sem_t	*sem_open(const char *, int, ...);
int	sem_close(sem_t *);
int	sem_unlink(const char *);
int	sem_wait(sem_t *);
int	sem_trywait(sem_t *);
int	sem_post(sem_t *);
int	sem_getvalue(sem_t *, int *);
#else
int	sem_init();
int	sem_destroy();
sem_t	*sem_open();
int	sem_close();
int	sem_unlink();
int	sem_wait();
int	sem_trywait();
int	sem_post();
int	sem_getvalue();
#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _SEMAPHORE_H */
