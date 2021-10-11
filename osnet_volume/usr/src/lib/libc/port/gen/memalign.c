/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)memalign.c	1.11	96/12/02 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#pragma weak memalign = _memalign

#include "synonyms.h"
#include <mtlib.h>
#include "mallint.h"
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>

#define	_misaligned(p)		((unsigned)(p) & 3)
		/* 4-byte "word" alignment is considered ok in LP64 */
#define	_nextblk(p, size)	((TREE *) ((char *) (p) + (size)))

/*
 * memalign(align, nbytes)
 *
 * Description:
 *	Returns a block of specified size on a specified alignment boundary.
 *
 * Algorithm:
 *	Malloc enough to ensure that a block can be aligned correctly.
 *	Find the alignment point and return the fragments
 *	before and after the block.
 *
 * Errors:
 *	Returns NULL and sets errno as follows:
 *	[EINVAL]
 *		if nbytes = 0,
 *		or if alignment is misaligned,
 *		or if the heap has been detectably corrupted.
 *	[ENOMEM]
 *		if the requested memory could not be allocated.
 */

void *
memalign(size_t align, size_t nbytes)
{
	size_t	 reqsize;	/* Num of bytes to get from malloc() */
	TREE	*p;		/* Ptr returned from malloc() */
	TREE	*blk;		/* For addressing fragment blocks */
	size_t	blksize;	/* Current (shrinking) block size */
	TREE	*alignedp;	/* Ptr to properly aligned boundary */
	TREE	*aligned_blk;	/* The block to be returned */
	size_t	frag_size;	/* size of fragments fore and aft */
	size_t	 x;

	/*
	 * check for valid size and alignment parameters
	 */
	if (nbytes == 0 || _misaligned(align) || align == 0) {
		errno = EINVAL;
		return (NULL);
	}

	/*
	 * Malloc enough memory to guarantee that the result can be
	 * aligned correctly. The worst case is when malloc returns
	 * a block so close to the next alignment boundary that a
	 * fragment of minimum size cannot be created.  In order to
	 * make sure we can handle this, we need to force the
	 * alignment to be at least as large as the minimum frag size
	 * (MINSIZE + WORDSIZE).
	 */
	ROUND(nbytes);
	if (nbytes < MINSIZE)
		nbytes = MINSIZE;
	ROUND(align);
	while (align < MINSIZE + WORDSIZE)
		align <<= 1;
	reqsize = nbytes + align + MINSIZE + WORDSIZE;
	p = (TREE *) malloc(reqsize);
	if (p == (TREE *) NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	(void) _mutex_lock(&__malloc_lock);

	/*
	 * get size of the entire block (overhead and all)
	 */
	blk = BLOCK(p);			/* back up to get length word */
	blksize = SIZE(blk);
	CLRBITS01(blksize);

	/*
	 * locate the proper alignment boundary within the block.
	 */
	x = (size_t) p;
	if (x % align != 0)
		x += align - (x % align);
	alignedp = (TREE *)x;
	aligned_blk = BLOCK(alignedp);

	/*
	 * Check out the space to the left of the alignment
	 * boundary, and split off a fragment if necessary.
	 */
	frag_size = (size_t)aligned_blk - (size_t)blk;
	if (frag_size != 0) {
		/*
		 * Create a fragment to the left of the aligned block.
		 */
		if (frag_size < MINSIZE + WORDSIZE) {
			/*
			 * Not enough space. So make the split
			 * at the other end of the alignment unit.
			 * We know this yields enough space, because
			 * we forced align >= MINSIZE + WORDSIZE above.
			 */
			frag_size += align;
			aligned_blk = _nextblk(aligned_blk, align);
		}
		blksize -= frag_size;
		SIZE(aligned_blk) = blksize | BIT0;
		frag_size -= WORDSIZE;
		SIZE(blk) = frag_size | BIT0 | ISBIT1(SIZE(blk));
		_free_unlocked(DATA(blk));
	}

	/*
	 * Is there a (sufficiently large) fragment to the
	 * right of the aligned block?
	 */
	frag_size = blksize - nbytes;
	if (frag_size >= MINSIZE + WORDSIZE) {
		/*
		 * split and free a fragment on the right
		 */
		blksize = SIZE(aligned_blk);
		SIZE(aligned_blk) = nbytes;
		blk = NEXT(aligned_blk);
		SETOLD01(SIZE(aligned_blk), blksize);
		frag_size -= WORDSIZE;
		SIZE(blk) = frag_size | BIT0;
		_free_unlocked(DATA(blk));
	}
	(void) _mutex_unlock(&__malloc_lock);
	return (DATA(aligned_blk));
}
