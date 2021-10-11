/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)malloc.c	1.21	99/10/07 SMI"

/*
 * Simplified version of malloc(), calloc() and free(), to be linked with
 * utilities that use [s]brk() and do not define their own version of the
 * routines.
 * The algorithm maps /dev/zero to get extra memory space.
 * Each call to mmap() creates a page. The pages are linked in a list.
 * Each page is divided in blocks. There is at least one block in a page.
 * New memory chunks are allocated on a first-fit basis.
 * Freed blocks are joined in larger blocks. Free pages are unmapped.
 */
#include	"_synonyms.h"

#include	<stdlib.h>
#include	<sys/types.h>
#include	<sys/mman.h>
#include	<memory.h>
#include	<string.h>
#include	"_rtld.h"
#include	"msg.h"
#include	"profile.h"

struct block {
	size_t		size;		/* Space available for user */
	struct page *	page;		/* Backwards reference to page */
	int		status;
	struct block *	next;
	void *		memstart[1];
};

struct page {
	size_t		size;		/* Total page size (incl. header) */
	struct page *	next;
	struct block	block[1];
};

#define	FREE	0
#define	BUSY	1

#define	HDR_BLOCK	(sizeof (struct block) - sizeof (void *))
#define	HDR_PAGE	(sizeof (struct page) - sizeof (void *))
#define	MINSZ		sizeof (void *)

static struct page *	memstart;


/*
 * Defragmentation
 */
static void
defrag(struct page * page)
{
	struct block *	block;

	PRF_MCOUNT(77, defrag);

	for (block = page->block; block; block = block->next) {
		struct block *	block2;

		if (block->status == BUSY)
			continue;
		for (block2 = block->next; block2 && block2->status == FREE;
		    block2 = block2->next) {
			block->next = block2->next;
			block->size += block2->size + HDR_BLOCK;
		}
	}

	/*
	 * Free page
	 */
	if (page->block->size == page->size - HDR_PAGE) {
		if (page == memstart)
			memstart = page->next;
		else {
			struct page * page2;
			for (page2 = memstart; page2->next;
			    page2 = page2->next) {
				if (page2->next == page) {
					page2->next = page->next;
					break;
				}
			}
		}
		(void) munmap((caddr_t)page, (size_t)page->size);
	}
}

static void
split(struct block * block, size_t size)
{
	PRF_MCOUNT(78, split);

	if (block->size > size + sizeof (struct block)) {
		struct block * newblock;
		/* LINTED */
		newblock = (struct block *)
		    ((char *)block + HDR_BLOCK + size);
		newblock->next = block->next;
		block->next = newblock;
		newblock->status = FREE;
		newblock->page = block->page;
		newblock->size = block->size - size - HDR_BLOCK;
		block->size = size;
	}
}


/*
 * Align size on an appropriate boundary
 */
static size_t
align(size_t size, size_t bound)
{
	PRF_MCOUNT(105, align);

	if (size < bound)
		return (bound);
	else
		return (size + bound - 1 - (size + bound - 1) % bound);
}

static void *
_malloc(size_t size)
{
	struct block *	block;
	struct page *	page;

	PRF_MCOUNT(79, _malloc);

	size = align(size, MINSZ);

	/*
	 * Try to locate necessary space
	 */
	for (page = memstart; page; page = page->next) {
		for (block = page->block; block; block = block->next) {
			if ((block->status == FREE) && (block->size >= size))
				goto found;
		}
	}
found:

	/*
	 * Need to allocate a new page
	 */
	if (!page) {
		size_t		totsize = size + HDR_PAGE;
		size_t		totpage = align(totsize, syspagsz);

		/* LINTED */
		if ((page = (struct page *)dz_map(0, totpage,
		    PROT_READ | PROT_WRITE | PROT_EXEC,
		    MAP_PRIVATE)) == (struct page *)-1)
			return (0);

		page->next = memstart;
		memstart = page;
		page->size = totpage;
		block = page->block;
		block->next = 0;
		block->status = FREE;
		block->size = totpage - HDR_PAGE;
		block->page = page;
	}

	split(block, size);

	block->status = BUSY;
	return (&block->memstart);
}

void *
malloc(size_t size)
{
	void *	rc;
	int	bind = 0;

	PRF_MCOUNT(100, malloc);

	if ((ti_version > 0) &&
	    (((bind = bind_guard(THR_FLG_MALLOC)) == 1)))
		(void) rw_wrlock(&malloclock);
	rc = _malloc(size);
	if ((ti_version > 0) && bind) {
		(void) rw_unlock(&malloclock);
		(void) bind_clear(THR_FLG_MALLOC);
	}
	return (rc);
}

static void *
_calloc(size_t num, size_t size)
{
	void *	mp;

	PRF_MCOUNT(80, _calloc);

	num *= size;
	if ((mp = _malloc(num)) == NULL)
		return (NULL);
	(void) memset(mp, 0, num);
	return (mp);
}

void *
calloc(size_t num, size_t size)
{
	void *	rc;
	int	bind = 0;

	PRF_MCOUNT(101, calloc);

	if ((ti_version > 0) &&
	    (((bind = bind_guard(THR_FLG_MALLOC)) == 1)))
		(void) rw_wrlock(&malloclock);

	rc = _calloc(num, size);
	if ((ti_version > 0) && bind) {
		(void) rw_unlock(&malloclock);
		(void) bind_clear(THR_FLG_MALLOC);
	}
	return (rc);
}

static void *
_realloc(void * ptr, size_t size)
{
	struct block *	block;
	size_t		osize;
	void *		newptr;

	PRF_MCOUNT(81, _realloc);

	/* LINTED */
	block = (struct block *)((char *)ptr - HDR_BLOCK);
	size = align(size, MINSZ);
	osize = block->size;

	/*
	 * Join block with next one if it is free
	 */
	if (block->next && block->next->status == FREE) {
		block->size += block->next->size + HDR_BLOCK;
		block->next = block->next->next;
	}

	if (size <= block->size) {
		split(block, size);
		return (ptr);
	}

	if ((newptr = _malloc(size)) == NULL)
		return (NULL);
	(void) memcpy(newptr, ptr, osize);
	block->status = FREE;
	defrag(block->page);
	return (newptr);
}

void *
realloc(void * ptr, size_t size)
{
	void *	rc;
	int	bind = 0;

	PRF_MCOUNT(102, realloc);

	if (ptr == NULL)
		return (malloc(size));

	if ((ti_version > 0) &&
	    (((bind = bind_guard(THR_FLG_MALLOC)) == 1)))
		(void) rw_wrlock(&malloclock);

	rc = _realloc(ptr, size);

	if ((ti_version > 0) && bind) {
		(void) rw_unlock(&malloclock);
		(void) bind_clear(THR_FLG_MALLOC);
	}
	return (rc);
}

static void
_free(void * ptr)
{
	struct block *	block;

	PRF_MCOUNT(82, _free);

	if (ptr == NULL)
		return;

	/* LINTED */
	block = (struct block *)((char *)ptr - HDR_BLOCK);
	block->status = FREE;

	defrag(block->page);
}

void
free(void * ptr)
{
	int	bind = 0;

	if ((ti_version > 0) &&
	    (((bind = bind_guard(THR_FLG_MALLOC)) == 1)))
		(void) rw_wrlock(&malloclock);

	_free(ptr);

	if ((ti_version > 0) && bind) {
		(void) rw_unlock(&malloclock);
		(void) bind_clear(THR_FLG_MALLOC);
	}
}

/*
 * We can use any memory after ld.so.1's .bss up until the next page boundary
 * as allocatable memory.
 */
void
addfree(void * ptr, size_t bytes)
{
	struct block *	block;
	struct page *	page;

	if (bytes <= sizeof (struct page))
		return;
	page = ptr;
	page->next = memstart;
	memstart = page;
	page->size = bytes;
	block = page->block;
	block->next = 0;
	block->status = FREE;
	block->size = bytes - HDR_PAGE;
	block->page = page;
}
