/*
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)mallint.h	1.4	98/07/21 SMI"	/* SVr4.0 1.2	*/

#ifndef	_REENTRANT
#define	_REENTRANT
#endif

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <memory.h>
#include <thread.h>
#include <synch.h>
#include <malloc.h>
#include <procfs.h>
#include <limits.h>

/* debugging macros */
#ifdef	DEBUG
#define	ASSERT(p)	((void) ((p) || (abort(), 0)))
#define	COUNT(n)	((void) n++)
static int		nmalloc, nrealloc, nfree;
#else
#define	ASSERT(p)	((void)0)
#define	COUNT(n)	((void)0)
#endif /* DEBUG */

/* for conveniences */
#ifndef NULL
#define	NULL		(0)
#endif

#define	WORDSIZE	(sizeof (WORD))
#define	MINSIZE		(sizeof (TREE) - sizeof (WORD))
#define	ROUND(s)	if ((s)%WORDSIZE) (s) += (WORDSIZE - ((s)%WORDSIZE))

/*
 * All of our allocations will be aligned on the least multiple of 4,
 * at least, so the two low order bits are guaranteed to be available.
 */
#ifdef _LP64
#define	ALIGN		16
#else
#define	ALIGN		8
#endif

/* the proto-word; size must be ALIGN bytes */
typedef union _w_ {
	size_t		w_i;		/* an unsigned int */
	struct _t_	*w_p[2];	/* two pointers */
} WORD;

/* structure of a node in the free tree */
typedef struct _t_ {
	WORD	t_s;	/* size of this element */
	WORD	t_p;	/* parent node */
	WORD	t_l;	/* left child */
	WORD	t_r;	/* right child */
	WORD	t_n;	/* next in link list */
	WORD	t_d;	/* dummy to reserve space for self-pointer */
} TREE;

/* usable # of bytes in the block */
#define	SIZE(b)		(((b)->t_s).w_i)
#define	RSIZE(b)	(((b)->t_s).w_i & ~BITS01)

/* free tree pointers */
#define	PARENT(b)	(((b)->t_p).w_p[0])
#define	LEFT(b)		(((b)->t_l).w_p[0])
#define	RIGHT(b)	(((b)->t_r).w_p[0])

/* forward link in lists of small blocks */
#define	AFTER(b)	(((b)->t_p).w_p[0])

/* forward and backward links for lists in the tree */
#define	LINKFOR(b)	(((b)->t_n).w_p[0])
#define	LINKBAK(b)	(((b)->t_p).w_p[0])

/* set/test indicator if a block is in the tree or in a list */
#define	SETNOTREE(b)	(LEFT(b) = (TREE *)(-1))
#define	ISNOTREE(b)	(LEFT(b) == (TREE *)(-1))

/* functions to get information on a block */
#define	DATA(b)		(((char *)(b)) + WORDSIZE)
#define	BLOCK(d)	((TREE *)(((char *)(d)) - WORDSIZE))
#define	SELFP(b)	(&(NEXT(b)->t_s.w_p[1]))
#define	LAST(b)		((b)->t_s.w_p[1])
#define	NEXT(b)		((TREE *)(((char *)(b)) + RSIZE(b) + WORDSIZE))
#define	BOTTOM(b)	((DATA(b) + RSIZE(b) + WORDSIZE) == Baddr)

/* functions to set and test the lowest two bits of a word */
#define	BIT0		(01)		/* ...001 */
#define	BIT1		(02)		/* ...010 */
#define	BITS01		(03)		/* ...011 */
#define	ISBIT0(w)	((w) & BIT0)	/* Is busy? */
#define	ISBIT1(w)	((w) & BIT1)	/* Is the preceding free? */
#define	SETBIT0(w)	((w) |= BIT0)	/* Block is busy */
#define	SETBIT1(w)	((w) |= BIT1)	/* The preceding is free */
#define	CLRBIT0(w)	((w) &= ~BIT0)	/* Clean bit0 */
#define	CLRBIT1(w)	((w) &= ~BIT1)	/* Clean bit1 */
#define	SETBITS01(w)	((w) |= BITS01)	/* Set bits 0 & 1 */
#define	CLRBITS01(w)	((w) &= ~BITS01) /* Clean bits 0 & 1 */
#define	SETOLD01(n, o)	((n) |= (BITS01 & (o)))

/* system call to get more memory */
#define	GETCORE		sbrk
#define	ERRCORE		((char *)(-1))
#define	CORESIZE	(1024*ALIGN)

/* where are these *really* declared? */
extern	int	_mutex_lock(mutex_t *mp);
extern	int	_mutex_unlock(mutex_t *mp);
