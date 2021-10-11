/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * spmalloc.c -- routines for "high memory" allocation
 */

#ident	"@(#)spmalloc.c	1.4	99/07/14 SMI"

/*
 * Some versions of boot.bin leave some or all of the first 64K of
 * protected mode memory available for use by bootconf.  The amount
 * of memory can be determined by doing an INT 21/4A with ES = FFFF.
 * The return value represents the number of paragraphs in the block
 * which would start at FFFF, so we can use one paragraph less than
 * reported, starting at FFFF:0010.  On a boot.bin that does not
 * provide this feature the attempt to resize the (non-existent)
 * block at FFFF will simply fail.
 *
 * We can use this memory for any purpose where no-one uses address
 * calculations that assume the memory is below 1 Meg.  That rules
 * out usage with BIOS since we cannot be sure what different BIOS
 * implementations will do.
 *
 * Each block of free or allocated memory is preceded by a spcl_hdr
 * that defines its size.  Free blocks are chained together into a
 * free list in ascending address order.  Since all blocks are in
 * the same segment, the chain contains only the offset.  Block size
 * indicators show the amount of memory NOT including the header itself.
 *
 * The routines fall back to regular malloc/free to keep things
 * simple for callers.  Callers just need to be sure that the usage
 * of the requested memory is appropriate.
 */

#include "debug.h"
#include <dos.h>
#include <string.h>

extern void fatal(const char *, ...);
extern void free(void *);
extern void *malloc(unsigned int);
extern void * memset(void *, int, unsigned int);
extern void *realloc(void *, unsigned int);

typedef struct spcl_hdr {
	unsigned short size;
	unsigned short next;
} spcl_hdr;

union spcl_addr {
	char *p;
	struct spcl_hdr *h;
	unsigned short s[2];
};

unsigned short spcl_free_list;

#define	SPCL_SEG	0xFFFF
#define	SPCL_OFFSTART	0x10
#define	SPCL_MINBLK	(sizeof (long) + sizeof (spcl_hdr))
#define	SPCL_INUSE	0xF00F

#define	SPCL_ADD(p, n)	((spcl_hdr *)(((char *)(p)) + (n)))
#define	SPCL_MEM2HDR(p)	(SPCL_ADD(p, 0 - sizeof (spcl_hdr)))
#define	SPCL_HDR2MEM(p)	(SPCL_ADD(p, sizeof (spcl_hdr)))


/* Set up special memory free list */
void
spcl_init(void)
{
	unsigned short spcl_size = 0;
	union spcl_addr u;

#ifdef __lint
	spcl_size = 0x1000;
#else
	_asm {
		push	bx
		push	es
		mov	bx, 0ffffh
		mov	es, bx
		mov	ah, 4ah
		int	21h
		jc	no_spcl
		mov	spcl_size, bx
	no_spcl:
		pop	es
		pop	bx
	}
#endif

	if (spcl_size != 0) {
		u.s[0] = SPCL_OFFSTART;
		u.s[1] = SPCL_SEG;
		u.h->size = ((spcl_size - 1) << 4) - sizeof (spcl_hdr);
		u.h->next = 0;
		spcl_free_list = SPCL_OFFSTART;
	}
}

/* If free block is immediately followed by another, coalesce them */
static void
try_coalesce(spcl_hdr *p)
{
	union spcl_addr u;
	spcl_hdr *next;

	u.h = p;
	if (p->next && p->next == u.s[0] + sizeof (spcl_hdr) + p->size) {
		u.s[0] = p->next;
		u.s[1] = SPCL_SEG;
		next = u.h;
		p->next += sizeof (spcl_hdr) + next->size;
		p->size += sizeof (spcl_hdr) + next->size;
	}
}

void *
spcl_malloc(unsigned short size)
{
	union spcl_addr u;
	unsigned short off;
	spcl_hdr *p;
	spcl_hdr *prev;
	spcl_hdr *next;
	unsigned short adj_size;
	static int first_time = 1;

	if (first_time) {
		first_time = 0;
		spcl_init();
	}

	adj_size = ((size + sizeof (long) - 1) & ~(sizeof (long) - 1));

	/* Simple first-fit algorithm */
	for (off = spcl_free_list, prev = 0; off; off = p->next) {
		u.s[0] = off;
		u.s[1] = SPCL_SEG;
		p = u.h;
		if (p->size >= adj_size) {
			/*
			 * Found a block that is big enough.  Split it if
			 * there is enough space left over for a minimal
			 * block.  Remove the allocated block from the
			 * chain and return a pointer to its data.
			 */
			if (p->size >= adj_size + SPCL_MINBLK) {
				next = SPCL_ADD(p,
				    sizeof (spcl_hdr) + adj_size);
				next->next = p->next;
				next->size = p->size - sizeof (spcl_hdr) -
				    adj_size;
				p->next = _FP_OFF(next);
				p->size = adj_size;
			}
			if (prev) {
				prev->next = p->next;
			} else {
				spcl_free_list = p->next;
			}
			p->next = SPCL_INUSE;
			return (SPCL_HDR2MEM(p));
		}
		prev = p;
	}

	return (malloc(size));
}

void
spcl_free(void *s)
{
	union spcl_addr u;
	spcl_hdr *b;
	spcl_hdr *p;
	unsigned short off;
	unsigned short b_off;

	if (_FP_SEG(s) != SPCL_SEG) {
		free(s);
		return;
	}
	b = SPCL_MEM2HDR(s);
	b_off = _FP_OFF(b);

	if (b->next != SPCL_INUSE) {
		fatal("spcl_free: bad free or arena corrupted\n");
		return;
	}

	for (off = spcl_free_list, p = 0; off && off < b_off; off = p->next) {
		u.s[0] = off;
		u.s[1] = SPCL_SEG;
		p = u.h;
	}

	if (p) {
		b->next = p->next;
		p->next = b_off;
	} else {
		b->next = spcl_free_list;
		spcl_free_list = b_off;
	}

	/*
	 * Try to coalesce with a following block, then into a
	 * previous block, if any.
	 */
	try_coalesce(b);
	if (p)
		try_coalesce(p);
}

/*
 * This implementation of spcl_realloc does not handle the special cases
 * of s == 0 (malloc equivalent) and newsize == 0 (free equivalent).
 */
void *
spcl_realloc(void *s, unsigned short newsize)
{
	spcl_hdr *b;
	spcl_hdr *n;
	void *ans;
	unsigned short adj_size;

	if (_FP_SEG(s) != SPCL_SEG) {
		return (realloc(s, newsize));
	}
	b = SPCL_MEM2HDR(s);

	if (b->next != SPCL_INUSE) {
		fatal("spcl_realloc: bad free or arena corrupted\n");
		return (0);
	}

	adj_size = ((newsize + sizeof (long) - 1) & ~(sizeof (long) - 1));
	if (b->size < adj_size) {
		/*
		 * For now we just take the simple approach of allocating
		 * the desired size and copying.  If we get really tight
		 * on space we might need to add a test for whether there
		 * is enough adjacent memory to expand the block.  Copy
		 * the lesser of b->size and newsize because b->size could
		 * be greater than newsize due to block rounding.
		 */
		if ((ans = spcl_malloc(newsize)) != 0) {
			memcpy(ans, s, b->size < newsize ? b->size : newsize);
			spcl_free(s);
		}
		return (ans);
	}
	if (b->size >= adj_size + SPCL_MINBLK) {
		/*
		 * Reducing size by enough to make a free block.
		 * Make it look like an allocated block then free it.
		 */
		n = SPCL_ADD(b, sizeof (spcl_hdr) + adj_size);
		n->next = SPCL_INUSE;
		n->size = b->size - adj_size - sizeof (spcl_hdr);
		b->size = adj_size;
		spcl_free(SPCL_HDR2MEM(n));
	}
	return (s);
}

void *
spcl_calloc(unsigned short n, unsigned short size)
{
	unsigned short sum;
	void *answer;

	sum = n * size;
	answer = spcl_malloc(sum);
	if (answer) {
		memset(answer, 0, sum);
	}
	return (answer);
}
