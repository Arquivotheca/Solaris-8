/*
 *  Copyright (c) 1995, by Sun Microsystems, Inc.
 *  All rights reserved.
 *
 *  malloc.c -- dynamic memory allocation for DOS (with debugging!)
 *
 *  I've had a lot of bad experiences looking for memory leaks under DOS
 *  (wild pointers don't just SEGV - they tend to force a reboot, which
 *  destroys the debugging context).  I looked in vain for a freeware version
 *  of a Purify(TM)-like tool for use under DOS, but was finally forced
 *  to write my own.  This is it.
 *
 *  Basically, this is a pretty straight-forward implementation of a buddy-
 *  system memory allocator.  The nice thing about the buddy system (aside
 *  from it's blinding speed) is that the natural pairing of memory blocks
 *  makes it easy to check the disposition of all dynamically allocated mem-
 *  ory.  The downside is that it's not the most memory efficient allocation
 *  strategy (there tends to be a large amount of internal fragmentation).
 *  As the database gets larger, this routine becomes unusable.  But recom-
 *  piling without -DDEBUG will revert to the standard DOS malloc routine.
 */

#ident	"<@(#)malloc.c	1.11	95/09/15 SMI>"
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "err.h"

#ifdef DEBUG	/* Debugging version of malloc/free!			    */

struct hdr {
	/*
	 *  Storage header:
	 *
	 *  All dynamically allocated blocks have a header with the following
	 *  format.  The "next" and "prev" fields only appear in available
	 *  blocks, however.
	 */

	unsigned short len;	/* Length of block			    */
	unsigned short tag;	/* Allocated/free tag (& magic number)	    */

	struct hdr *next;	/* Ptr to next available block of this size */
	struct hdr *prev;	/* Ptr to previous available block	    */
};

/* Fixed overhead in each memory block ..				    */
#define	overhead ((int)&((struct hdr *)0)->next)

static char *who = 0;		/* Current entry point name		    */
static int recur = -1;		/* Recursive malloc/free - see below	    */
static char *libname = "libc";	/* Library name				    */
static char *frename = "free";	/* free routine's name			    */

#define	avail_head(n) ((struct hdr *)((char *)&avail[n] - overhead))
#define	avail_init(n) { avail_head(n), avail_head(n) }
#define	MAX_AVAIL 12

static struct { struct hdr *next, *prev; } avail[MAX_AVAIL] = {
	/*
	 *  The free list headers:
	 *
	 *  We only maintain 12 of them here.  The 13th is the DOS free
	 *  segment list.
	 */

	avail_init(0), avail_init(1), avail_init(2),  avail_init(3),
	avail_init(4), avail_init(5), avail_init(6),  avail_init(7),
	avail_init(8), avail_init(9), avail_init(10), avail_init(11)
};

#define	TAIL  0x9669
#define	MAGIC 0x3A5C

#define	allocated(p) ((p)->tag == MAGIC)
#define	swap(x, y) { struct hdr *t = x; x = y; y = t; }
#define	buddy(p, m) ((struct hdr *)((unsigned long)(p) ^ (m)))

/*
 *  Fancy debugging stuff can be disabled by simply compling without the
 *  -DDEBUG switch!
 */

#define	assert(c, x) if ((recur <= 0) && !(x)) {			      \
	/*								      \
	 *  Internal "assert" macro:					      \
	 *								      \
	 *  We check the internal state of our data structures by "assert"ing \
	 *  conditions that we know should hold.  If said conditions are not  \
	 *  true, we issue the following error message giving the caller's    \
	 *  file and line number.					      \
	 *								      \
	 *  NOTE: We have a slight problem in that error recovery routines    \
	 *	  may to use malloc/free as much as anyone else.  When this   \
	 *	  happens, the "recur" flag will be positive and we don't     \
	 *	  bother re-issuing the message.			      \
	 */								      \
									      \
	fatal("memory corrupted (code %d) at entry to %s - \"%s\":%d\n", c,   \
							    who, file, line); \
}

static unsigned
roundup(unsigned n, unsigned c, char *file, int line) {
	/*
	 *  Round length up to nearest paragraph:
	 *
	 *  The minimum amount of storage we can allocate is 16 bytes; the max
	 *  is 64K less our fixed overhead (8 bytes).  This routine rounds the
	 *  request value "n" appropriately and checks to make sure it doesn't
	 *  exceed the maximum.  If it looks good we return the rounded value;
	 *  if not we blow up!
	 */

	unsigned long N;

	N = (((unsigned long)n * (unsigned long)c) + overhead + sizeof (short));
	N = ((N + 15) & ~0xF);

	if ((recur <= 0) && (N > 0xFFFFUL)) {
		/*
		 *  Oops!  This failure is most likely to occur from "calloc"
		 *  when a bogus multiplier is used.  If we're called from
		 *  "free" or the C library, the problem isn't a bad length
		 *  argument (e.g, "free" doesn't have a length argument),
		 *  but corrupted data structures.
		 */

		assert(0, (file != libname) && (file != frename));
		fatal("bad %s size - \"%s\":%d\n", who, file, line);
	}
	return ((unsigned)N);
}

static void
scan_lists(char *file, int line)
{
	/*
	 *  Validate free lists:
	 *
	 *  This routine scans the available lists, checking to see that
	 *  each entry contained therein remains uncorrupted.
	 */

	int j;
	struct hdr *p;

	for (j = 0; (recur <= 0) && (j < MAX_AVAIL); j++) {
		/*
		 *  Check all 12 available lists, but only do so once per
		 *  invocation.  The global "recur" flag will be positive
		 *  if we get re-called attempting to "printf" an error
		 *  message.
		 */

		for (p = avail[j].next; p != avail_head(j); p = p->next) {
			/*
			 *  For available blocks we check that:
			 *
			 *    1.  The "tag" field claims the block is free
			 *    2.  Free list forward & back ptrs are valid
			 *    3.  The block length matches the free list index
			 */

			assert(1, p->tag == ~MAGIC);
			assert(2, p->next->prev == p);
			assert(3, p->prev->next == p);
			assert(4, p->len == (1 << (j+4)));
		}
	}
}

static struct hdr *
get_hdr(void *v, char *file, int line)
{
	/*
	 *  Find block header:
	 *
	 *  Users of "realloc" and "free" must pass in the address of a
	 *  buffer obtained by one of the "*alloc" variants.  This routine
	 *  locates the block header for that buffer ("v") and checks to
	 *  make sure it hasn't been corrupted.
	 */

	unsigned long m;
	struct hdr *p = (struct hdr *)((char *)v - overhead);

	if (recur <= 0) {
		/*
		 *  The block header should be properly aligned, it should
		 *  have a valid tag, and there should be a TAIL marker
		 *  at the end of the buffer (as defined by the "len" field).
		 */

		/* Check for NULL pointers first ...			    */
		assert(5, !((unsigned long)p & 0xF));

		/* Check for tag and tail ...				    */
		assert(6, allocated(p));
		assert(7, *(short *)((char *)v + p->len) == TAIL);

		/* Check for proper buffer alignment ...		    */
		for (m = 16; m < p->len; m <<= 1);
		assert(8, !((unsigned long)p & ((m-1) & 0xFFFF)));
	}

	return (p);
}

void *
Xmalloc(unsigned n, char *file, int line)
{
	/*
	 *  Buddy system allocator:
	 *
	 *    We use 13 free lists, one for each power of 2 between 16 and
	 *    64k.  We use the DOS's internal free segment list for the 64k
	 *    list; all others are defined above ("avail[]").  See Knuth,
	 *    Vol 1, for a full analysis of the algorhitm.
	 *
	 *    No one calls Xmalloc directly, they call "malloc" which is either
	 *    a macro that adds the "file" and "line" arguments, or is the
	 *    cover routine at the end of this file.
	 */

	int j, k = 0;
	unsigned long m;
	unsigned w, len;
	struct hdr *p, *q = 0;
	if (w = !(++recur || who)) who = "malloc";

	scan_lists(file, line);
	len = roundup(n, 1, file, line);
	for (m = 16; m < len; m <<= 1) k++;
	for (j = k; (j < MAX_AVAIL) && (avail[j].next == avail_head(j)); j++);

	if (!recur && (j < MAX_AVAIL)) {
		/*
		 *  If there's a free block big enough to statisfy the caller's
		 *  request, remove it from the free list (header address goes
		 *  to "q" register).
		 */

		q = avail[j].next;
		q->next->prev = q->prev;
		q->prev->next = q->next;

	} else {
		/*
		 *  .. Otherwise, ask DOS if we can have another full segment.
		 *  We also use this code to allocate memory when the free
		 *  lists appear to be corrupted ("recur" flag is non-zero).
		 *  If this happens, we bump the "recur" flag again to make
		 *  the error recovery condition permanent.
		 */

		unsigned pars = (recur ? ((n+16) >> 4) : 0x1000);
		if (recur) recur += 2;

		_asm {
			/*
			 *  Issue the DOS "allocate segment" command.  The
			 *  "pars" register gives the size of the segement
			 *  in paragraphs.  This is usually a full 64KB
			 *  request, but may be less if we're in an error
			 *  situation.
			 *
			 *  Segment portion of "q" register is updated if
			 *  the DOS call works.
			 */

			push  bx
			mov   bx, pars
			mov   ax, 4800h
			int   21h
			jc    skp
			mov   word ptr [q+2], ax
		skp:	pop   bx
		}
	}

	if (q && !recur) {
		/*
		 *  We've got a block, but it may be bigger than we need.
		 *  If it is, stick the unused memory on the right side
		 *  back on the free list.
		 */

		for (m = (unsigned)(1 << (j+3)); j-- > k; m >>= 1) {
			/*
			 *  Keep decrementing the "j" register (log[blocksize])
			 *  until it matches the "k" register (log[request-
			 *  size]).  Each iteration peels an "m"-byte block
			 *  off the back end of the block at "q" and puts in
			 *  in the avail list.
			 */

			p = buddy(q, m);
			p->tag = ~MAGIC;
			p->len = m;

			avail[j].prev->next = p;
			p->prev = avail[j].prev;
			p->next = avail_head(j);
			avail[j].prev = p;
		}

		q->len = n;
		q->tag = MAGIC;
		q = (struct hdr *)((char *)q + overhead);
		*(short *)((char *)q + n) = TAIL;
	}

	recur--;
	if (w) who = 0;
	return ((void *)q);
}

void
Xfree(void *v, char *file, int line)
{
	/*
	 *  Buddy system free routine:
	 *
	 *    See Knuth for details, but don't do anythying if the recur flag
	 *    is non-zero upon entry.  This means we've detected corruption of
	 *    the free lists.  Trying to add something to them at this point
	 *    could well lead to disaster!
	 */

	int w;
	struct hdr *p;

	if (w = !(++recur || who)) who = frename;
	scan_lists(file, line);

	if (!recur && (p = get_hdr(v, file, line))) {
		/*
		 *  The "p" register now points to the header for the block
		 *  the caller's trying to free.  We want to put this struct
		 *  on one of the free lists, but which one?  The proper list
		 *  index is given by "log2(blocksize)" which we compute into
		 *  the "j" register.
		 */

		int j = 0;
		unsigned long m;

		p->len = roundup(p->len, 1, file, line);
		for (m = 16; m < p->len; m <<= 1) j++;
		p->len = m;

		while (j < MAX_AVAIL) {
			/*
			 *  Now check to see if the current block's buddy
			 *  is free.  If so, remove it from its free list
			 *  and double the target block's size.
			 */

			struct hdr *q = buddy(p, p->len);

			if (allocated(q) || (p->len != q->len)) {
				/*
				 *  All or part of the buddy block is still
				 *  in use.  Stick the newly free'd block
				 *  back on the "j"th avail list.
				 */

				avail[j].prev->next = p;
				p->prev = avail[j].prev;
				p->next = avail_head(j);
				avail[j].prev = p;
				p->tag = ~MAGIC;
				goto fin;
			}

			q->next->prev = q->prev;
			q->prev->next = q->next;
			if (q < p) swap(p, q);
			p->len <<= 1;
			q->tag = 0;
			j++;
		}

		_asm {
			/*
			 *  We've combined all buddies into a 64k free seg-
			 *  ment.  Return this to DOS.
			 */

			push  es
			mov   es, word ptr [p+2]
			mov   ax, 4900h
			int   21h
			jc    skp
			neg   j
		skp:    pop   es
		}

		assert(9, j < 0);
	}

fin:	if (w) who = 0;
	recur -= 1;
}

void *
Xrealloc(void *v, unsigned n, char *file, int line)
{
	/*
	 *  Buddy system realloc:
	 *
	 *    Unfortunately, the buddy system does not lend itself to simple
	 *    and efficient implementations of "realloc".  We don't bother
	 *    trying to do anything smart -- we're assuming that realloc
	 *    isn't used often enough to really matter!
	 */

	void *u;
	unsigned r;

	who = "realloc";
	r = roundup(n, 1, file, line);
	scan_lists(file, line); /* Validate free lists */

	if ((u = Xmalloc(n, file, line)) && (v != 0)) {
		/*
		 *  There's real data that needs to be copied into the re-
		 *  allocated buffer.  Figure out the proper byte count and
		 *  do it!
		 */

		struct hdr *p = get_hdr(v, file, line);
		if (n > p->len) n = p->len;
		memcpy(u, v, n);

		Xfree(v, file, line);
	}

	who = 0;
	return (u);
}

void *
Xcalloc(unsigned c, unsigned n, char *file, int line)
{
	/*
	 *  Buddy system calloc:
	 *
	 *  Really just "malloc" followed by a bzero.
	 */

	void *p;
	who = "calloc";
	roundup(n, c, file, line);

	if (p = Xmalloc(n *= c, file, line)) memset(p, 0, n);
	who = (char *)0;
	return (p);
}

#endif

/*
 *  User code has been using the malloc/free macros defined in "debug.h" to
 *  call the above routines, but pre-compiled routines in the C library
 *  can't do this.  Nor can we let them use the libc version of malloc since
 *  it would get in our way.  Instead, we provide cover routines that call
 *  their "X*alloc" equivalents with dummy "file" and "line" arguments.
 *
 *  NOTE:  One problem with this scheme is that, if an application module
 *	   forgets to include "debug.h", any malloc errors it causes will
 *	   be attributed to the library.
 */

#undef free
#undef malloc
#undef calloc
#undef realloc

#ifdef DEBUG
void(free)(void *p) { Xfree(p, libname, 0); }
void *(malloc)(unsigned n) { return (Xmalloc(n, libname, 0)); }
void *(realloc)(void *p, unsigned n) { return (Xrealloc(p, n, libname, 0)); }
void *(calloc)(unsigned c, unsigned n) { return (Xcalloc(c, n, libname, 0)); }

#else	/* Non debugging version:					    */
	/*								    */
	/*    These routines will only be called from routines that were    */
	/*    compiled with '-DDEBUG'.  This allows individual files to	    */
	/*    to be debugged without recompiling everything (but you don't  */
	/*    get any malloc debugging info if you do it that way).	    */

void
Xfree(void *p, char *file, int line)
{
	free(p);
}

void *
Xmalloc(unsigned n, char *file, int line)
{
	return (malloc(n));
}

void *
Xrealloc(void *p, unsigned n, char *file, int line)
{
	return (realloc(p, n));
}

void *
Xcalloc(unsigned c, unsigned n, char *file, int line)
{
	return (calloc(c, n));
}

#endif
