/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)memlist_new.c	1.4	98/08/12 SMI"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/mutex.h>
#include <sys/param.h>		/* for NULL */
#include <sys/debug.h>
#include <sys/memlist.h>
#include <sys/memlist_impl.h>

static struct memlist *memlist_freelist;
static u_int memlist_freelist_count;
static kmutex_t memlist_freelist_mutex;

/*
 * Caller must test for NULL return.
 */
struct memlist *
memlist_get_one(void)
{
	struct memlist *mlp;

	mutex_enter(&memlist_freelist_mutex);
	mlp = memlist_freelist;
	if (mlp != NULL) {
		memlist_freelist = mlp->next;
		memlist_freelist_count--;
	}
	mutex_exit(&memlist_freelist_mutex);

	return (mlp);
}

void
memlist_free_one(struct memlist *mlp)
{
	ASSERT(mlp != NULL);

	mutex_enter(&memlist_freelist_mutex);
	mlp->next = memlist_freelist;
	memlist_freelist = mlp;
	memlist_freelist_count++;
	mutex_exit(&memlist_freelist_mutex);
}

void
memlist_free_list(struct memlist *mlp)
{
	struct memlist *mlendp;
	u_int count;

	if (mlp == NULL) {
		return;
	}

	count = 1;
	for (mlendp = mlp; mlendp->next != NULL; mlendp = mlendp->next)
		count++;
	mutex_enter(&memlist_freelist_mutex);
	mlendp->next = memlist_freelist;
	memlist_freelist = mlp;
	memlist_freelist_count += count;
	mutex_exit(&memlist_freelist_mutex);
}

void
memlist_free_block(caddr_t base, size_t bytes)
{
	struct memlist *mlp, *mlendp;
	u_int count;

	count = bytes / sizeof (struct memlist);
	if (count == 0)
		return;

	mlp = (struct memlist *)base;
	mlendp = &mlp[count - 1];
	for (; mlp != mlendp; mlp++)
		mlp->next = mlp + 1;
	mlendp->next = NULL;
	mlp = (struct memlist *)base;
	mutex_enter(&memlist_freelist_mutex);
	mlendp->next = memlist_freelist;
	memlist_freelist = mlp;
	memlist_freelist_count += count;
	mutex_exit(&memlist_freelist_mutex);
}

/*
 * Insert into a sorted memory list.
 * new = new element to insert
 * curmemlistp = memory list to which to add segment.
 */
void
memlist_insert(
	struct memlist *new,
	struct memlist **curmemlistp)
{
	struct memlist *cur, *last;
	uint64_t start, end;

	start = new->address;
	end = start + new->size;
	last = NULL;
	for (cur = *curmemlistp; cur; cur = cur->next) {
		last = cur;
		if (cur->address >= end) {
			new->next = cur;
			new->prev = cur->prev;
			cur->prev = new;
			if (cur == *curmemlistp)
				*curmemlistp = new;
			else
				new->prev->next = new;
			return;
		}
		if (cur->address + cur->size > start)
			panic("munged memory list = 0x%x\n", curmemlistp);
	}
	new->next = NULL;
	new->prev = last;
	if (last != NULL)
		last->next = new;
}

void
memlist_del(struct memlist *memlistp,
	struct memlist **curmemlistp)
{
#ifdef DEBUG
	/*
	 * Check that the memlist is on the list.
	 */
	struct memlist *mlp;

	for (mlp = *curmemlistp; mlp != NULL; mlp = mlp->next)
		if (mlp == memlistp)
			break;
	ASSERT(mlp == memlistp);
#endif /* DEBUG */
	if (*curmemlistp == memlistp) {
		ASSERT(memlistp->prev == NULL);
		*curmemlistp = memlistp->next;
	}
	if (memlistp->prev != NULL) {
		ASSERT(memlistp->prev->next == memlistp);
		memlistp->prev->next = memlistp->next;
	}
	if (memlistp->next != NULL) {
		ASSERT(memlistp->next->prev == memlistp);
		memlistp->next->prev = memlistp->prev;
	}
}

struct memlist *
memlist_find(struct memlist *mlp, uint64_t address)
{
	for (; mlp != NULL; mlp = mlp->next)
		if (address >= mlp->address &&
		    address < (mlp->address + mlp->size))
			break;
	return (mlp);
}

/*
 * Add a span to a memlist.
 * Return:
 * MEML_SPANOP_OK if OK.
 * MEML_SPANOP_ESPAN if part or all of span already exists
 * MEML_SPANOP_EALLOC for allocation failure
 */
int
memlist_add_span(
	uint64_t address,
	uint64_t bytes,
	struct memlist **curmemlistp)
{
	struct memlist *dst;
	struct memlist *prev, *next;

	/*
	 * allocate a new struct memlist
	 */

	dst = memlist_get_one();

	if (dst == NULL) {
		return (MEML_SPANOP_EALLOC);
	}

	dst->address = address;
	dst->size = bytes;

	/*
	 * First insert.
	 */
	if (*curmemlistp == NULL) {
		dst->prev = NULL;
		dst->next = NULL;
		*curmemlistp = dst;
		return (MEML_SPANOP_OK);
	}

	/*
	 * Insert into sorted list.
	 */
	for (prev = NULL, next = *curmemlistp; next != NULL;
	    prev = next, next = next->next) {
		if (address > (next->address + next->size))
			continue;

		/*
		 * Else insert here.
		 */

		/*
		 * Prepend to next.
		 */
		if ((address + bytes) == next->address) {
			memlist_free_one(dst);

			next->address = address;
			next->size += bytes;

			return (MEML_SPANOP_OK);
		}

		/*
		 * Append to next.
		 */
		if (address == (next->address + next->size)) {
			memlist_free_one(dst);

			if (next->next) {
				/*
				 * don't overlap with next->next
				 */
				if ((address + bytes) > next->next->address) {
					return (MEML_SPANOP_ESPAN);
				}
				/*
				 * Concatenate next and next->next
				 */
				if ((address + bytes) == next->next->address) {
					struct memlist *mlp = next->next;

					if (next == *curmemlistp)
						*curmemlistp = next->next;

					mlp->address = next->address;
					mlp->size += next->size;
					mlp->size += bytes;

					if (next->prev)
						next->prev->next = mlp;
					mlp->prev = next->prev;

					memlist_free_one(next);
					return (MEML_SPANOP_OK);
				}
			}

			next->size += bytes;

			return (MEML_SPANOP_OK);
		}

		/* don't overlap with next */
		if ((address + bytes) > next->address) {
			memlist_free_one(dst);
			return (MEML_SPANOP_ESPAN);
		}

		/*
		 * Insert before next.
		 */
		dst->prev = prev;
		dst->next = next;
		next->prev = dst;
		if (prev == NULL) {
			*curmemlistp = dst;
		} else {
			prev->next = dst;
		}
		return (MEML_SPANOP_OK);
	}

	/*
	 * End of list, prev is valid and next is NULL.
	 */
	prev->next = dst;
	dst->prev = prev;
	dst->next = NULL;

	return (MEML_SPANOP_OK);
}

/*
 * Delete a span from a memlist.
 * Return:
 * MEML_SPANOP_OK if OK.
 * MEML_SPANOP_ESPAN if part or all of span does not exist
 * MEML_SPANOP_EALLOC for allocation failure
 */
int
memlist_delete_span(
	uint64_t address,
	uint64_t bytes,
	struct memlist **curmemlistp)
{
	struct memlist *dst, *next;

	/*
	 * Find element containing address.
	 */
	for (next = *curmemlistp; next != NULL; next = next->next) {
		if ((address >= next->address) &&
		    (address < next->address + next->size))
			break;
	}

	/*
	 * If start address not in list.
	 */
	if (next == NULL) {
		return (MEML_SPANOP_ESPAN);
	}

	/*
	 * Error if size goes off end of this struct memlist.
	 */
	if (address + bytes > next->address + next->size) {
		return (MEML_SPANOP_ESPAN);
	}

	/*
	 * Span at beginning of struct memlist.
	 */
	if (address == next->address) {
		/*
		 * If start & size match, delete from list.
		 */
		if (bytes == next->size) {
			if (next == *curmemlistp)
				*curmemlistp = next->next;
			if (next->prev != NULL)
				next->prev->next = next->next;
			if (next->next != NULL)
				next->next->prev = next->prev;

			memlist_free_one(next);
		} else {
			/*
			 * Increment start address by bytes.
			 */
			next->address += bytes;
			next->size -= bytes;
		}
		return (MEML_SPANOP_OK);
	}

	/*
	 * Span at end of struct memlist.
	 */
	if (address + bytes == next->address + next->size) {
		/*
		 * decrement size by bytes
		 */
		next->size -= bytes;
		return (MEML_SPANOP_OK);
	}

	/*
	 * Delete a span in the middle of the struct memlist.
	 */
	{
		/*
		 * create a new struct memlist
		 */
		dst = memlist_get_one();

		if (dst == NULL) {
			return (MEML_SPANOP_EALLOC);
		}

		/*
		 * Existing struct memlist gets address
		 * and size up to start of span.
		 */
		dst->address = address + bytes;
		dst->size = (next->address + next->size) - dst->address;
		next->size = address - next->address;

		/*
		 * New struct memlist gets address starting
		 * after span, until end.
		 */

		/*
		 * link in new memlist after old
		 */
		dst->next = next->next;
		dst->prev = next;

		if (next->next != NULL)
			next->next->prev = dst;
		next->next = dst;
	}
	return (MEML_SPANOP_OK);
}
