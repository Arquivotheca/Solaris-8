/*
 * Copyright (c) 1992-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)sbrk.c		1.6	97/12/22 SMI"

#pragma weak sbrk = _sbrk
#pragma weak brk = _brk

#include "synonyms.h"
#include <synch.h>
#include <errno.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <inttypes.h>
#include <unistd.h>
#include "mtlib.h"
#include "libc.h"

extern mutex_t __sbrk_lock;
extern void *_nd;
extern int _brk_unlocked(void *);
void *_sbrk_unlocked(intptr_t);

/*
 * The break must always be at least 8-byte aligned
 */
#if (_MAX_ALIGNMENT < 8)
#define	ALIGNSZ		8
#else
#define	ALIGNSZ		_MAX_ALIGNMENT
#endif

#define	BRKALIGN(x)	\
	(char *)(((uintptr_t)(x) + ALIGNSZ - 1) & ~(ALIGNSZ - 1))

void *
sbrk(intptr_t addend)
{
	void *result;

	_mutex_lock(&__sbrk_lock);
	result = _sbrk_unlocked(addend);
	_mutex_unlock(&__sbrk_lock);

	return (result);
}

/*
 * _sbrk_unlocked() aligns the old break, adds the addend, aligns
 * the new break, and calls _brk_unlocked() to set the new break.
 * We must align the old break because _nd may begin life misaligned.
 * The addend can be either positive or negative, so there are two
 * overflow/underflow edge conditions to reject:
 *
 *   - the addend is negative and brk + addend < 0.
 *   - the addend is positive and brk + addend > ULONG_MAX
 */
void *
_sbrk_unlocked(intptr_t addend)
{
	char *old_brk = BRKALIGN(_nd);
	char *new_brk = BRKALIGN(old_brk + addend);

	if ((addend > 0 && new_brk < old_brk) ||
	    (addend < 0 && new_brk > old_brk)) {
		errno = ENOMEM;
		return ((void *)-1);
	}

	return (_brk_unlocked(new_brk) == 0 ? old_brk : (void *)-1);
}

int
brk(void *new_brk)
{
	int result;

	_mutex_lock(&__sbrk_lock);

	/*
	 * Need to align this here;  _brk_unlocked won't do it for us.
	 */
	result = _brk_unlocked(BRKALIGN(new_brk));
	_mutex_unlock(&__sbrk_lock);

	return (result);
}
