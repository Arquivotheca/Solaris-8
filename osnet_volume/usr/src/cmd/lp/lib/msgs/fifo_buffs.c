/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)fifo_buffs.c	1.3	97/05/14 SMI"	/* SVr4.0 1.3	*/
/* LINTLIBRARY */


#include	<errno.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	"lp.h"
#include	"msgs.h"

static	fifobuffer_t	**FifoBufferTable	= NULL;
static	int		FifoBufferTableSize	= 0;

/*
**	Local functions
*/
static	int		InitFifoBufferTable (void);
static	int		GrowFifoBufferTable (int);
static	fifobuffer_t	*NewFifoBuffer (int);


int
ResetFifoBuffer(int fd)
{
	if ((!FifoBufferTableSize) && (InitFifoBufferTable () < 0))
		return	-1;

	if (fd >= FifoBufferTableSize)
		return	0;

	if (FifoBufferTable [fd]) {
		FifoBufferTable [fd]->full = 0;
		FifoBufferTable [fd]->psave =
		FifoBufferTable [fd]->psave_end = 
			FifoBufferTable [fd]->save;
	}
	return	0;
}


fifobuffer_t *
GetFifoBuffer(int fd)
{
	if (fd < 0) {
		errno = EINVAL;
		return	NULL;
	}
	if ((fd >= FifoBufferTableSize) && (GrowFifoBufferTable (fd) < 0))
		return	NULL;

	if (!FifoBufferTable [fd]) {
		if (!NewFifoBuffer (fd))
			return	NULL;
		
		FifoBufferTable [fd]->full = 0;
		FifoBufferTable [fd]->psave =
		FifoBufferTable [fd]->psave_end = 
			FifoBufferTable [fd]->save;
	}
	
	return	FifoBufferTable [fd];
}


static	int
InitFifoBufferTable()
{
	if (FifoBufferTableSize)
		return	0;

	FifoBufferTable = (fifobuffer_t **)
		Calloc (100, sizeof (fifobuffer_t *));
	if (!FifoBufferTable)
		return	-1;	/* ENOMEM is already set. */

	FifoBufferTableSize = 100;

	return	0;
}


static int
GrowFifoBufferTable (int fd)
{
	fifobuffer_t	**newpp;

	newpp = (fifobuffer_t **)
		Realloc ((void*)FifoBufferTable,
		(fd+10)*sizeof (fifobuffer_t *));
	if (!newpp)
		return	-1;	/* ENOMEM is already set. */

	FifoBufferTableSize = fd+10;

	return	0;
}


static fifobuffer_t *
NewFifoBuffer(int fd)
{
	int	i;

	for (i=0; i < FifoBufferTableSize; i++)
	{
		if (FifoBufferTable [i] &&
		    Fcntl (i, F_GETFL) < 0 &&
                    errno == EBADF)
		{
			FifoBufferTable [fd] = FifoBufferTable [i];
			FifoBufferTable [i] = NULL;
			return	FifoBufferTable [fd];
		}
	}
	FifoBufferTable [fd] = (fifobuffer_t *)
		Calloc (1, sizeof (fifobuffer_t));

	return	FifoBufferTable [fd];
}
