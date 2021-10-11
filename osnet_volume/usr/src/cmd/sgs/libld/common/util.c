/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)util.c	1.36	99/05/27 SMI"

/*
 * Utility functions
 */
#include	<unistd.h>
#include	<stdio.h>
#include	<string.h>
#include	<fcntl.h>
#include	<sys/types.h>
#include	<sys/mman.h>
#include	<errno.h>
#include	"msg.h"
#include	"_libld.h"


/*
 * We interpose on malloc, calloc, & free to use libld's faster
 * memory allocation routines.
 */
#pragma weak	malloc = libld_malloc

void
/* ARGSUSED 0 */
free(void * ptr)
{
}

void *
calloc(size_t nelem, size_t elsize)
{
	return (libld_calloc(nelem, elsize));
}

void *
realloc(void *ptr, size_t size)
{
	size_t	prev_size;
	void *	vptr;

	if (ptr == NULL)
		return (libld_malloc(size));

	/*
	 * size of the allocated blocks is stored *just* before
	 * the blocks address.
	 */
	prev_size = *((size_t *)ptr - 1);
	/*
	 * If the block actually fits then just return.
	 */
	if (size <= prev_size)
		return (ptr);
	vptr = libld_malloc(size);
	(void) memcpy(vptr, ptr, prev_size);
	return (vptr);
}

/*
 * libld_malloc() and zero_map() are used for both performance
 * and for ease of programining:
 *
 * Performance:
 *	The link-edit is a short lived process which doesn't really
 *	free much of the dynamic memory that it requests.  Because
 *	of this it is much more important that we optimize for the
 *	quick memory allocations then the re-usability of the
 *	memory.
 *
 *	By also mmaping blocks of pages in from /dev/zero we don't
 *	need to waste the overhead of zeroing out these pages
 *	for calloc() requests.
 *
 * Memory Management:
 *	By doing all libld memory management through the ld_malloc
 *	routine it's much easier to free up all memory at the end
 *	by simply unmaping all off the blocks that were mapped in
 *	through zero_map.  This is much simpler then trying to
 *	track all of the libld structures and which were dynamically
 *	allocate and which are actually pointers into the ELF files.
 *
 *	It's important that we can free up all of our dynamic memory
 *	because libld is used by ld.so.1 when it performs dlopen()'s
 *	of relocatable objects.
 *
 * Format:
 *	The memory blocks for each allocation store the size of
 *	the allocation in the first 4 bytes of the block.  The
 *	pointer that is returned by *alloc() is actually the
 *	address of (block + 4):
 *
 *		(addr - 4)	block_size
 *		(addr)		<allocated block>
 *
 *	This is done because in order to implement the realloc()
 *	routine you must know the size of the old block in order
 *	to perform the memcpy().
 */

void *
zero_map(size_t size)
{
	int	fd = 0;
	void *	addr;
	int	err;

	if ((fd = open(MSG_ORIG(MSG_PTH_DEVZERO), O_RDWR)) == -1) {
		err = errno;
		(void) eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN),
		    MSG_ORIG(MSG_PTH_DEVZERO), strerror(err));
		return (0);
	}
	if ((addr = mmap(0, size, PROT_READ | PROT_WRITE | PROT_EXEC,
	    MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		err = errno;
		(void) eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MMAP),
		    MSG_ORIG(MSG_PTH_DEVZERO), strerror(err));
		return (0);
	}
	(void) close(fd);
	return (addr);
}

void *
libld_malloc(size_t size)
{
	Ld_heap *	ldhp = ld_heap;
	void *		vptr;
	size_t		asize = size + sizeof (size_t);

	/*
	 * If this is the first allocation, or the allocation request is greater
	 * than the current free space available, allocate a new heap.
	 */
	if ((ldhp == 0) ||
	    (((unsigned long)ldhp->lh_end -
	    (unsigned long)ldhp->lh_free) <= asize)) {
		Ld_heap *	new_ldhp;
		size_t	ldhpsz = (size_t)S_ROUND(sizeof (Ld_heap), HEAPALIGN);
		size_t	alloc_size = (size_t)S_ROUND((asize + ldhpsz),
		    HEAPALIGN);

		/*
		 * Allocate a block that is at minimum 'HEAPBLOCK' size
		 */
		if (alloc_size < HEAPBLOCK)
			alloc_size = HEAPBLOCK;

		if ((new_ldhp = (Ld_heap *)zero_map(alloc_size)) == 0)
			return (0);

		new_ldhp->lh_next = ldhp;
		new_ldhp->lh_free = vptr = (void *)((unsigned long)new_ldhp +
			ldhpsz);
		new_ldhp->lh_end = (void *)((size_t)new_ldhp + alloc_size);
		ldhp = ld_heap = new_ldhp;
	}
	vptr = ldhp->lh_free;
	/*
	 * Assign size to head of allocated block (used by realloc)
	 */
	*((size_t *)vptr) = size;
	vptr = (void *)((unsigned long)vptr + sizeof (size_t));

	/*
	 * Increment free to point to next available block
	 */
	ldhp->lh_free = (void *)S_ROUND((unsigned long)ldhp->lh_free +
		asize, HEAPALIGN);
	return (vptr);
}

/*
 * Append an item to the specified list, and return a pointer to the list
 * node created.
 */
Listnode *
list_appendc(List * lst, const void * item)
{
	Listnode *	_lnp;

	if ((_lnp = libld_malloc(sizeof (Listnode))) == (Listnode *)0)
		return (0);

	_lnp->data = (void *)item;
	_lnp->next = NULL;

	if (lst->head == NULL)
		lst->tail = lst->head = _lnp;
	else {
		lst->tail->next = _lnp;
		lst->tail = lst->tail->next;
	}
	return (_lnp);
}

/*
 * Add an item after the specified listnode, and return a pointer to the list
 * node created.
 */
Listnode *
list_insertc(List * lst, const void * item, Listnode * lnp)
{
	Listnode *	_lnp;

	if ((_lnp = libld_malloc(sizeof (Listnode))) == (Listnode *)0)
		return (0);

	_lnp->data = (void *)item;
	_lnp->next = lnp->next;
	if (_lnp->next == NULL)
		lst->tail = _lnp;
	lnp->next = _lnp;
	return (_lnp);
}

/*
 * Prepend an item to the specified list, and return a pointer to the
 * list node created.
 */
Listnode *
list_prependc(List * lst, const void * item)
{
	Listnode *	_lnp;

	if ((_lnp = libld_malloc(sizeof (Listnode))) == (Listnode *)0)
		return (0);

	_lnp->data = (void *)item;

	if (lst->head == NULL) {
		_lnp->next = NULL;
		lst->tail = lst->head = _lnp;
	} else {
		_lnp->next = lst->head;
		lst->head = _lnp;
	}
	return (_lnp);
}

/*
 * Find out where to insert the node for reordering.  List of insect structures
 * is traversed and the is_txtndx field of the insect structure is examined
 * and that determines where the new input section should be inserted.
 * All input sections which have a non zero is_txtndx value will be placed
 * in ascending order before sections with zero is_txtndx value.  This
 * implies that any section that does not appear in the map file will be
 * placed at the end of this list as it will have a is_txtndx value of 0.
 * Returns:  NULL if the input section should be inserted at beginning
 * of list else A pointer to the entry AFTER which this new section should
 * be inserted.
 */
Listnode *
list_where(List * lst, Word num)
{
	Listnode *	ln, * pln;	/* Temp list node ptr */
	Is_desc	*	isp;		/* Temp Insect structure */
	Word		n;

	/*
	 * No input sections exist, so add at beginning of list
	 */
	if (lst->head == NULL)
		return (NULL);

	for (ln = lst->head, pln = ln; ln != NULL; pln = ln, ln = ln->next) {
		isp = (Is_desc *)ln->data;
		/*
		 *  This should never happen, but if it should we
		 *  try to do the right thing.  Insert at the
		 *  beginning of list if no other items exist, else
		 *  end of already existing list, prior to this null
		 *  item.
		 */
		if (isp == NULL) {
			if (ln == pln) {
				return (NULL);
			} else {
				return (pln);
			}
		}
		/*
		 *  We have reached end of reorderable items.  All
		 *  following items have is_txtndx values of zero
		 *  So insert at end of reorderable items.
		 */
		if ((n = isp->is_txtndx) > num || n == 0) {
			if (ln == pln) {
				return (NULL);
			} else {
				return (pln);
			}
		}
		/*
		 *  We have reached end of list, so insert
		 *  at the end of this list.
		 */
		if ((n != 0) && (ln->next == NULL))
			return (ln);
	}
	return (NULL);
}

/*
 * Determine if a shared object definition structure already exists and if
 * not create one.  These definitions provide for recording information
 * regarding shared objects that are still to be processed.  Once processed
 * shared objects are maintained on the ofl_sos list.  The information
 * recorded in this structure includes:
 *
 *  o	DT_USED requirements.  In these cases definitions are added during
 *	mapfile processing of `-' entries (see map_dash()).
 *
 *  o	implicit NEEDED entries.  As shared objects are processed from the
 *	command line so any of their dependencies are recorded in these
 *	structures for later processing (see process_dynamic()).
 *
 *  o	version requirements.  Any explicit shared objects that have version
 *	dependencies on other objects have their version requirements recorded.
 *	In these cases definitions are added during mapfile processing of `-'
 *	entries (see map_dash()).  Also, shared objects may have versioning
 *	requirements on their NEEDED entries.  These cases are added during
 *	their version processing (see vers_need_process()).
 *
 *	Note: Both process_dynamic() and vers_need_process() may generate the
 *	initial version definition structure because you can't rely on what
 *	section (.dynamic or .SUNW_version) may be processed first from	any
 *	input file.
 */
Sdf_desc *
sdf_find(const char * name, List * lst)
{
	Listnode *	lnp;
	Sdf_desc *	sdf;

	for (LIST_TRAVERSE(lst, lnp, sdf))
		if (strcmp(name, sdf->sdf_name) == 0)
			return (sdf);

	return (0);
}

Sdf_desc *
sdf_add(const char * name, List * lst)
{
	Sdf_desc *	sdf;

	if (!(sdf = libld_calloc(sizeof (Sdf_desc), 1)))
		return ((Sdf_desc *)S_ERROR);

	sdf->sdf_name = name;

	if (list_appendc(lst, sdf) == 0)
		return ((Sdf_desc *)S_ERROR);
	else
		return (sdf);
}

/*
 * Determine a least common multiplier.  Input sections contain an alignment
 * requirement, which elf_update() uses to insure that the section is aligned
 * correctly off of the base of the elf image.  We must also insure that the
 * sections mapping is congruent with this alignment requirement.  For each
 * input section associated with a loadable segment determine whether the
 * segments alignment must be adjusted to compensate for a sections alignment
 * requirements.
 */
Xword
lcm(Xword a, Xword b)
{
	Xword	_r, _a, _b;

	if ((_a = a) == 0)
		return (b);
	if ((_b = b) == 0)
		return (a);

	if (_a > _b)
		_a = b, _b = a;
	while ((_r = _b % _a) != 0)
		_b = _a, _a = _r;
	return ((a / _a) * b);
}



/*
 * Cleanup an Ifl_desc
 */
void
ifl_list_cleanup(List * ifl_list)
{
	Listnode *	lnp;
	Ifl_desc *	ifl;

	for (LIST_TRAVERSE(ifl_list, lnp, ifl))
		if (ifl->ifl_elf)
			(void) elf_end(ifl->ifl_elf);
	ifl_list->head = 0;
	ifl_list->tail = 0;
}

/*
 * Cleanup all memory that has been dynamically allocated
 * durring libld processing and elf_end() all Elf descriptors that
 * are still open.
 */
void
ofl_cleanup(Ofl_desc * ofl)
{
	Ld_heap *	cur, * prev;
	Ar_desc *	adp;
	Listnode *	lnp;

	ifl_list_cleanup(&ofl->ofl_objs);
	ifl_list_cleanup(&ofl->ofl_sos);

	for (LIST_TRAVERSE(&ofl->ofl_ars, lnp, adp)) {
		Ar_aux *	aup;
		Elf_Arsym *	arsym;
		for (arsym = adp->ad_start, aup = adp->ad_aux;
		    arsym->as_name; ++arsym, ++aup) {
			if ((aup->au_mem) && (aup->au_mem != AREXTRACTED)) {
				(void) elf_end(aup->au_mem->am_elf);
				/*
				 * null out all entries to this member so
				 * that we don't attempt to elf_end()
				 * it again.
				 */
				ar_member(adp, arsym, aup, 0);
			}
		}
		(void) elf_end(adp->ad_elf);
	}

	(void) elf_end(ofl->ofl_elf);
	(void) elf_end(ofl->ofl_welf);

	for (cur = ld_heap, prev = 0; cur; cur = cur->lh_next) {
		if (prev)
			(void) munmap((caddr_t)prev, (size_t)prev->lh_end -
				(size_t)prev);
		prev = cur;
	}
	if (prev)
		(void) munmap((caddr_t)prev, (size_t)prev->lh_end -
			(size_t)prev);
	ld_heap = 0;
}

/*
 * Add a string, separated by a colon, to an existing string.  Typically used
 * to maintain filter, rpath and audit names, of which there is normally only
 * one string supplied anyway.
 */
char *
add_string(char * old, char * str)
{
	char *	new;

	if (old) {
		char *	_str;

		/*
		 * If an original string exists, make sure this new string
		 * doesn't get duplicated.
		 */
		if ((_str = strstr(old, str)) != NULL) {
			if (((_str == old) ||
			    (*(_str - 1) == *(MSG_ORIG(MSG_STR_COLON)))) &&
			    (_str += strlen(str)) &&
			    ((*_str == '\0') ||
			    (*_str == *(MSG_ORIG(MSG_STR_COLON)))))
				return (old);
		}

		if ((new = libld_calloc(1, strlen(old) + strlen(str) + 2)) == 0)
			return ((char *)S_ERROR);
		(void) sprintf(new, MSG_ORIG(MSG_FMT_COLPATH), old, str);
	} else {
		if ((new = libld_malloc(strlen(str) + 1)) == 0)
			return ((char *)S_ERROR);
		(void) strcpy(new, str);
	}

	return (new);
}

/*
 * Messaging support - funnel everything through _dgettext() as this provides
 * a stub binding to libc, or a real binding to libintl.
 */
extern char *	_dgettext(const char *, const char *);

const char *
_libld_msg(Msg mid)
{
	return (_dgettext(MSG_ORIG(MSG_SUNW_OST_SGS), MSG_ORIG(mid)));
}
