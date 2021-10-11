/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)atexit.c	1.14	99/10/07 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <mtlib.h>
#include <stdlib.h>
#include <thread.h>
#include <synch.h>
#include <sys/types.h>
#include <string.h>
#include <link.h>
#include "libc_int.h"
#include "atexit.h"
#include <errno.h>

int _preexec_exit_handlers(Lc_addr_range_t range[], unsigned int count);

#pragma init(_atexit_init)
#pragma weak _ld_libc

static void
_atexit_init(void)
{
	Lc_interface reginfo[3];

	reginfo[0].ci_tag = CI_VERSION;
	reginfo[0].ci_un.ci_val = CI_V_CURRENT;
	reginfo[1].ci_tag = CI_ATEXIT;
	reginfo[1].ci_un.ci_func = _preexec_exit_handlers;
	reginfo[2].ci_tag = CI_NULL;
	reginfo[2].ci_un.ci_val = 0;

	if (_ld_libc != 0) {
		_ld_libc(&reginfo);
	}
}

/*
 * Note that the local memory management hack contained herein -
 * occasioned by the insistence of our brothers sh(1) and csh(1) that
 * they can do malloc, etc., better than libc can - *IS NECESSARY* and
 * should not be removed.  Those programs define their own malloc and
 * realloc routines, and initialize the underlying mechanism in main().
 * This means that calls to malloc occuring before main will crash.  The
 * loader calls atexit(3C) before calling main, so we'd better not need
 * memory when it does.
 */

/* Local type declarations */

typedef void (*_exithdlr_func_t) (void);
typedef int memloc_t;

typedef struct _exthdlr {
	memloc_t		next;	/* next in handler list */
	_exithdlr_func_t	hdlr;	/* handler itself */
} _exthdlr_t;

typedef struct {
	_exthdlr_t d;		/* entry */
	int free;		/* available for use */
} mem_t;

typedef struct {
	mem_t *  ptr;		/* array of entries */
	memloc_t size;		/* current size of this array */
	memloc_t free_count;	/* how many are availible */
} mem_hdr_t;


/* General forward function declarations */

static int in_range(_exithdlr_func_t, Lc_addr_range_t[], unsigned int count);


/* Memory-related forward function declarations */

/* high-level entry points - used by consumers of this memory model */
static memloc_t		get_mem(void);
static void		free_mem(memloc_t);

/* low-level entry points - only used by the implementation */
#define			m(a) (&cur_hdr->ptr[a].d)
static void		mem_init(mem_t *, memloc_t);


/* Static data definitions */

/* Memory-related */
#define	STATIC_COUNT	32
#define	UNDEF		-1
/* Note: UNDEF is the position-independent equivalent of NULL */

static mem_t		static_mem [STATIC_COUNT];
static mem_t *		dyn_mem = NULL;

static mem_hdr_t	static_hdr;
static mem_hdr_t	dyn_hdr;

static mem_hdr_t *	cur_hdr = NULL;

static mutex_t		mem_lock = DEFAULTMUTEX;

/* General */
static memloc_t		head = UNDEF;
static int		exithandler_once;
static mutex_t		exitfns_lock = DEFAULTMUTEX;
static mutex_t		exithandler_once_lock = DEFAULTMUTEX;


/* Global data definitions */

void *			__exit_frame_monitor = NULL;


/* Global function definitions */

int
atexit(void (*func)(void))
{
	memloc_t p = get_mem();

	if (p == UNDEF) {
		return (-1);
	}

	m(p)->hdlr = func;
	(void) _mutex_lock(&exitfns_lock);
	m(p)->next = head;
	head = p;
	(void) _mutex_unlock(&exitfns_lock);
	return (0);
}


void
_exithandle(void)
{
	memloc_t p;

	(void) _mutex_lock(&exithandler_once_lock);

	if (!exithandler_once) {
		exithandler_once = 1;
	} else {
		(void) _mutex_unlock(&exithandler_once_lock);
		return;
	}

	(void) _mutex_unlock(&exithandler_once_lock);
	(void) _mutex_lock(&exitfns_lock);
	p = head;

	while (p != UNDEF) {
		head = m(p)->next;
		(void) _mutex_unlock(&exitfns_lock);
		m(p)->hdlr();
		free_mem(p);
		(void) _mutex_lock(&exitfns_lock);
		p = head;
	}

	head = UNDEF;
	(void) _mutex_unlock(&exitfns_lock);
}

void *
_get_exit_frame_monitor(void)
{
	return (&__exit_frame_monitor);
}

/*
 * The following is a routine which the loader (ld.so.1) calls when it
 * processes dlclose calls on objects with atexit registrations.  It
 * executes the exit handlers that fall within the union of the ranges
 * specified by the elements of the array range in the REVERSE ORDER of
 * their registration.  Do not change this characteristic; it is REQUIRED
 * BEHAVIOR.
 */

int
_preexec_exit_handlers(Lc_addr_range_t range[], unsigned int count)
{
	memloc_t o;		/* previous node */
	memloc_t p;		/* this node */

	o = UNDEF;
	(void) _mutex_lock(&exitfns_lock);
	p = head;

	while (p != UNDEF) {
		if (in_range(m(p)->hdlr, range, count)) {
			/* We need to execute this one */

			if (o != UNDEF) {
				m(o)->next = m(p)->next;
			} else {
				head = m(p)->next;
			}

			(void) _mutex_unlock(&exitfns_lock);
			m(p)->hdlr();
			free_mem(p);
			(void) _mutex_lock(&exitfns_lock);
			p = head;
			o = UNDEF;
			continue;
		}

		o = p;
		p = m(p)->next;
	}

	(void) _mutex_unlock(&exitfns_lock);
	return (0);
}

/* Static function definitions */

static int
in_range(_exithdlr_func_t addr, Lc_addr_range_t ranges[], unsigned int count)
{
	unsigned int idx;

	for (idx = 0; idx < count; idx++) {
		if ((void *) addr >= ranges[idx].lb &&
		    (void *) addr <= ranges[idx].ub) {
			return (1);
		}
	}

	return (0);
}

static memloc_t
get_mem(void)
{
	memloc_t res;

	(void) _mutex_lock(&mem_lock);

	if (cur_hdr == NULL) {
		/* memory uninitialized - start with the static array */

		static_hdr.ptr = &static_mem[0];
		static_hdr.size = STATIC_COUNT;
		static_hdr.free_count = STATIC_COUNT;
		mem_init(&static_mem[0], STATIC_COUNT);
		cur_hdr = &static_hdr;
	} else if (cur_hdr->free_count == 0) {
		/* out of free slots - must expand */

		if (cur_hdr == &static_hdr) {
			/* first expansion - switch to dynamic array */

			if ((dyn_mem =
			    (mem_t *) malloc(2 * sizeof (static_mem))) ==
			    NULL) {
				(void) _mutex_unlock(&mem_lock);
				errno = ENOMEM;
				return (UNDEF);
			}

			(void) memcpy(dyn_mem, static_mem, sizeof (static_mem));
			mem_init(dyn_mem + STATIC_COUNT, STATIC_COUNT);

			dyn_hdr.ptr = dyn_mem;
			dyn_hdr.size = 2 * STATIC_COUNT;
			dyn_hdr.free_count = STATIC_COUNT;
			cur_hdr = &dyn_hdr;
		} else {
			/* subsequent expansion - use realloc */

			if ((dyn_mem =
			    realloc(dyn_mem, 2 * dyn_hdr.size *
			    sizeof (mem_t))) == NULL) {
				(void) _mutex_unlock(&mem_lock);
				errno = ENOMEM;
				return (UNDEF);
			}

			dyn_hdr.ptr = dyn_mem;
			mem_init(dyn_mem + dyn_hdr.size, dyn_hdr.size);
			dyn_hdr.free_count = dyn_hdr.size;
			dyn_hdr.size += dyn_hdr.size;
		}
	}

	/* give back the next free slot */

	for (res = 0; res < cur_hdr->size; res++) {
		if (cur_hdr->ptr[res].free) {
			cur_hdr->ptr[res].free = 0;
			cur_hdr->free_count--;
			break;
		}
	}

	(void) _mutex_unlock(&mem_lock);
	return (res);
}

static void
free_mem(memloc_t mem)
{
	mem_t *p;
	(void) _mutex_lock(&mem_lock);
	p = &cur_hdr->ptr[mem];
	p->free = 1;
	p->d.next = UNDEF;
	p->d.hdlr = NULL;
	cur_hdr->free_count++;
	(void) _mutex_unlock(&mem_lock);
}

static void
mem_init(mem_t * p, memloc_t count)
{
	while (--count >= 0) {
		p[count].d.hdlr = NULL;
		p[count].d.next = UNDEF;
		p[count].free = 1;
	}
}
