/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)stralloc.c	1.13	97/11/22 SMI"	/* from SVr4.0 1.3.2.1 */

/*
 * Copyright (c) 1993-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * mailx -- a modified version of a University of California at Berkeley
 *	mail program
 *
 * Memory allocation routines.
 * Memory handed out here are reclaimed at the top of the command
 * loop each time, so they need not be freed.
 */

#include "rcv.h"
#include <locale.h>

static void		*lastptr;	/* addr of last buffer allocated */
static struct strings	*lastsp;	/* last string space allocated from */

/*
 * Allocate size more bytes of space and return the address of the
 * first byte to the caller.  An even number of bytes are always
 * allocated so that the space will always be on a word boundary.
 * The string spaces are of exponentially increasing size, to satisfy
 * the occasional user with enormous string size requests.
 */

void *
salloc(unsigned size)
{
	register char *t;
	register unsigned s;
	register struct strings *sp;
	int index;

	s = size;
#if defined(u3b) || defined(sparc)
	s += 3;		/* needs alignment on quad boundary */
	s &= ~03;
#elif defined(i386)
	s++;
	s &= ~01;
#else
#error Unknown architecture!
#endif
	index = 0;
	for (sp = &stringdope[0]; sp < &stringdope[NSPACE]; sp++) {
		if (sp->s_topFree == NOSTR && (STRINGSIZE << index) >= s)
			break;
		if (sp->s_nleft >= s)
			break;
		index++;
	}
	if (sp >= &stringdope[NSPACE])
		panic("String too large");
	if (sp->s_topFree == NOSTR) {
		index = sp - &stringdope[0];
		sp->s_topFree = (char *) calloc(STRINGSIZE << index,
		    (unsigned) 1);
		if (sp->s_topFree == NOSTR) {
			fprintf(stderr, gettext("No room for space %d\n"),
			    index);
			panic("Internal error");
		}
		sp->s_nextFree = sp->s_topFree;
		sp->s_nleft = STRINGSIZE << index;
	}
	sp->s_nleft -= s;
	t = sp->s_nextFree;
	sp->s_nextFree += s;
	lastptr = t;
	lastsp = sp;
	return(t);
}

/*
 * Reallocate size bytes of space and return the address of the
 * first byte to the caller.  The old data is copied into the new area.
 */

void *
srealloc(void *optr, unsigned size)
{
	void *nptr;

	/* if we just want to expand the last allocation, that's easy */
	if (optr == lastptr) {
		register unsigned s, delta;
		register struct strings *sp = lastsp;

		s = size;
#if defined(u3b) || defined(sparc)
		s += 3;		/* needs alignment on quad boundary */
		s &= ~03;
#elif defined(i386)
		s++;
		s &= ~01;
#else
#error Unknown architecture!
#endif defined(u3b) || defined(sparc)
		delta = s - (sp->s_nextFree - (char *)optr);
		if (delta <= sp->s_nleft) {
			sp->s_nextFree += delta;
			sp->s_nleft -= delta;
			return (optr);
		}
	}
	nptr = salloc(size);
	if (nptr)
		memcpy(nptr, optr, size);	/* XXX - copying too much */
	return nptr;
}

/*
 * Reset the string area to be empty.
 * Called to free all strings allocated
 * since last reset.
 */
void 
sreset(void)
{
	register struct strings *sp;
	register int index;

	if (noreset)
		return;
	minit();
	index = 0;
	for (sp = &stringdope[0]; sp < &stringdope[NSPACE]; sp++) {
		if (sp->s_topFree == NOSTR)
			continue;
		sp->s_nextFree = sp->s_topFree;
		sp->s_nleft = STRINGSIZE << index;
		index++;
	}
	lastptr = NULL;
	lastsp = NULL;
}
