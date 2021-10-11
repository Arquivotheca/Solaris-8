/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_umem.c	1.1	99/08/11 SMI"

/*
 * These routines simply provide wrappers around malloc(3C) and free(3C)
 * for now.  In the future we hope to provide a userland equivalent to
 * the kmem allocator, including cache allocators.
 */

#include <strings.h>
#include <stdlib.h>

#include <mdb/mdb_debug.h>
#include <mdb/mdb_stdlib.h>
#include <mdb/mdb_frame.h>
#include <mdb/mdb_umem.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb.h>

#ifdef DEBUG
int umem_flags = UMF_DEBUG;
#else
int umem_flags = 0;
#endif

/*ARGSUSED*/
static void *
umem_handler(size_t nbytes, uint_t flags)
{
	ulong_t kb = nbytes / 1024;

	if (errno == EAGAIN) {
		void *ptr = NULL;
		char buf[64];

		(void) mdb_iob_snprintf(buf, sizeof (buf),
		    "[ sleeping for %luK of free memory ... ]", kb ? kb : 1);

		(void) mdb_iob_puts(mdb.m_err, buf);
		(void) mdb_iob_flush(mdb.m_err);

		do {
			delay(1000);
			ptr = malloc(nbytes);
		} while (ptr == NULL && errno == EAGAIN);

		if (ptr != NULL)
			return (ptr);

		(void) memset(buf, '\b', strlen(buf));
		(void) mdb_iob_puts(mdb.m_err, buf);
		(void) mdb_iob_flush(mdb.m_err);

		(void) memset(buf, ' ', strlen(buf));
		(void) mdb_iob_puts(mdb.m_err, buf);
		(void) mdb_iob_flush(mdb.m_err);

		(void) memset(buf, '\b', strlen(buf));
		(void) mdb_iob_puts(mdb.m_err, buf);
		(void) mdb_iob_flush(mdb.m_err);
	}

	die("failed to allocate %luK -- terminating\n", kb ? kb : 1);
	/*NOTREACHED*/
}

static void
umem_gc_enter(void *ptr, size_t nbytes)
{
	mdb_mblk_t *blkp = mdb_alloc(sizeof (mdb_mblk_t), UM_SLEEP);

	blkp->blk_addr = ptr;
	blkp->blk_size = nbytes;
	blkp->blk_next = mdb.m_frame->f_mblks;

	mdb.m_frame->f_mblks = blkp;
}

/*
 * If we're compiled in debug mode, we use this function (gratuitously
 * stolen from kmem.c) to set uninitialized and freed regions to
 * special bit patterns.
 */
static void
umem_copy_pattern(uint32_t pattern, void *buf_arg, size_t size)
{
	/* LINTED - alignment of bufend */
	uint32_t *bufend = (uint32_t *)((char *)buf_arg + size);
	uint32_t *buf = buf_arg;

	while (buf < bufend - 3) {
		buf[3] = buf[2] = buf[1] = buf[0] = pattern;
		buf += 4;
	}

	while (buf < bufend)
		*buf++ = pattern;
}

void *
mdb_alloc(size_t nbytes, uint_t flags)
{
	void *ptr;

	nbytes = (nbytes + sizeof (uint32_t) - 1) & ~(sizeof (uint32_t) - 1);
	ptr = nbytes ? malloc(nbytes) : NULL;

	if ((flags & UM_SLEEP) && nbytes != 0) {
		while (ptr == NULL)
			ptr = umem_handler(nbytes, flags);
	}

	if (ptr != NULL && (umem_flags & UMF_DEBUG) != 0)
		umem_copy_pattern(UMEM_UNINITIALIZED_PATTERN, ptr, nbytes);

	if (flags & UM_GC)
		umem_gc_enter(ptr, nbytes);

	return (ptr);
}

void *
mdb_zalloc(size_t nbytes, uint_t flags)
{
	void *ptr = mdb_alloc(nbytes, flags);

	if (ptr != NULL)
		bzero(ptr, nbytes);

	return (ptr);
}

void
mdb_free(void *ptr, size_t nbytes)
{
	ASSERT(ptr != NULL || nbytes == 0);

	if (ptr != NULL) {
		if (umem_flags & UMF_DEBUG)
			umem_copy_pattern(UMEM_FREE_PATTERN, ptr, nbytes);
		free(ptr);
	}
}

void
mdb_recycle(mdb_mblk_t **blkpp)
{
	mdb_mblk_t *blkp, *nblkp;

	for (blkp = *blkpp; blkp != NULL; blkp = nblkp) {
		mdb_dprintf(MDB_DBG_UMEM, "garbage collect %p size %lu bytes\n",
		    blkp->blk_addr, (ulong_t)blkp->blk_size);

		nblkp = blkp->blk_next;
		mdb_free(blkp->blk_addr, blkp->blk_size);
		mdb_free(blkp, sizeof (mdb_mblk_t));
	}

	*blkpp = NULL;
}
