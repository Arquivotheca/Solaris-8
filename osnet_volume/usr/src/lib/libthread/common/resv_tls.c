/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)resv_tls.c 1.2	99/06/09 SMI"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "libthread.h"

/*
 * All interfaces defined in this file are contract-private
 */

/*
 * Information common to all threads' reserved TLS.
 */
struct resv_tls_common _resv_tls_common = {	0,
						{
							(unsigned char)0,
							(unsigned char)0,
							(unsigned char)0,
							(unsigned char)0,
							(unsigned char)0,
							(unsigned char)0,
							(unsigned char)0,
							(unsigned char)0
						},
						{0, 0, 0, 0, 0, 0, 0, 0},
						{0, 0, 0, 0, 0, 0, 0, 0},
						DEFAULTMUTEX
					 };

/*
 * Synchronous thread slot allocate routine
 */

int
thr_slot_sync_allocate(thr_slot_handle_t *handle, PFrV destructor, void	*param)
{
	int 			slot_index, i;

	_lmutex_lock(&_resv_tls_common.lock);
	/* Check if there are any slots available */
	if (_resv_tls_common.nslots == MAX_RESV_TLS) {
		_lmutex_unlock(&_resv_tls_common.lock);
		return (ENOMEM);
	}
	if (_resv_tls_common.nslots == 0) {
		/*
		 * Reserve the first slot, setting the corresponding
		 * element in  _resv_tls_common.slot_map
		 */
		_resv_tls_common.slot_map[0] = (unsigned char)1;
		slot_index = 0;
	} else {
	/*
	 * find a free slot.
	 * First look into the nslots index to see if that is free.
	 * If not, scan through the slot map to find an
	 * unallocated slot.
	 */
		if (_resv_tls_common.slot_map[_resv_tls_common.nslots]) {
			/*
			 * There are only nslots that are allocated,
			 * slot[nslots] is already allocated, so there
			 * must be a slot[i] where i<nslots that is free.
			 */
			for (i = 0; _resv_tls_common.slot_map[i]; i++);
			ASSERT(i < MAX_RESV_TLS);
			slot_index = i;
		} else {
			slot_index = _resv_tls_common.nslots;
		}
		_resv_tls_common.slot_map[slot_index] = (unsigned char)1;
	}

	/*
	 * Increment the number of allocated slots,
	 * Set the destructor and pass-through parameter
	 * for the newly allocated slot
	 */

	_resv_tls_common.nslots++;
	_resv_tls_common.destructors[slot_index] = destructor;
	_resv_tls_common.pass_through_param[slot_index] = param;
	_lmutex_unlock(&_resv_tls_common.lock);

	/*
	 * Return the slot handle to the caller
	 */
	*handle = (thr_slot_handle_t)
			((uintptr_t)&(curthread->t_resvtls[slot_index])
			- (uintptr_t)curthread);

	return (0);
}

int
thr_slot_sync_deallocate(thr_slot_handle_t handle)
{
	PFrV			p;
	void			*v;
	thr_slot_handle_t	offset;

	/*
	 * Translate the handle to a slot offset within t_resvtls
	 */
	offset = (handle - ((uintptr_t)(curthread->t_resvtls) -
			(uintptr_t)curthread))/sizeof (uintptr_t);

	if (offset >= MAX_RESV_TLS || offset < 0) {
		return (EINVAL);
	}

	/*
	 * Decrement the active slot count, set the destructor to NULL,
	 * Update slot_map to mark the slot as free.
	 */
	_lmutex_lock(&_resv_tls_common.lock);
	/* Has the slot already been deallocated? */
	if (!(_resv_tls_common.slot_map[offset])) {
		_lmutex_unlock(&_resv_tls_common.lock);
		return (EINVAL);
	}
	_resv_tls_common.nslots--;
	_resv_tls_common.destructors[offset] = NULL;
	_resv_tls_common.pass_through_param[offset] = NULL;
	_resv_tls_common.slot_map[offset] = (unsigned char)0;
	_lmutex_unlock(&_resv_tls_common.lock);

	/*
	 * Set the slot value for the calling thread to NULL
	 * This is necessary as the allocation of another slot with
	 * non-NULL destructor could take place after the deallocation
	 * while the current thread is still alive.
	 * Now if the current thread dies, the following may happen.
	 * Slot gets reused: the handle of the newly allocated slot
	 * is the same as the passed-in argument, handle for this
	 * routine.
	 * In this case, the thread-exit routine will find a non-NULL
	 * slot value for the current thread which is associated with
	 * a non-NULL destructor and hence attempt to call the destructor
	 * wrongly.
	 * This can be prevented by setting the slot-value associated
	 * with the current thread to NULL.
	 */
	curthread->t_resvtls[offset] = 0;

	return (0);
}

/*
 * This function is used to free the thread specific storage on a
 * per-thread basis. Is called at thread-exit time within the context
 * of the exiting thread itself.
 */

void
_destroy_resv_tls(void)
{
	PFrV	func;
	void	*pass_through;
	int	i, new_resvtls;
	void	*resvtls_value;

/*
 * All we do here is call the thread-specific destructors
 * (if non-NULL) for all the allocated slots.
 * It is to be noted that since NULL is reserved for uninitialized
 * slot state representation, the destructor will not be called
 * for a slot with a NULL value.
 * We need to iterate through all the slots at least once owing
 * to the possibility of thr_slot_set()s within a destructor.
 * The interface specification  doesn't encourage slot_sets() within
 * a destructor. However, it may be difficult for some applications
 * to follow that restriction. Hence iteration is provided as a safety
 * measure to ensure proper cleanup.
 * For more details on the need to iterate, please refer the
 * implementation notes and comments for the _destroy_tsd() function
 * in common/tsd.c
 */
	do {
		new_resvtls = 0;
		for (i = 0; i < MAX_RESV_TLS; i++) {
			resvtls_value = curthread->t_resvtls[i];
			if (resvtls_value != NULL) {
				_lmutex_lock(&_resv_tls_common.lock);
				func = _resv_tls_common.destructors[i];
				pass_through =
					_resv_tls_common.pass_through_param[i];
				_lmutex_unlock(&_resv_tls_common.lock);
				if (func) {
					(*func)(pass_through);
					curthread->t_resvtls[i] = NULL;
					new_resvtls = 1;
				}
			}
		}
	} while (new_resvtls);
}

/*
 * Slot access routine
 */
void *
thr_slot_get(thr_slot_handle_t handle)
{
	return (*(void **)((caddr_t)curthread + handle));
}

/*
 * Slot set routine
 */
void
thr_slot_set(thr_slot_handle_t handle, void *value)
{
	*(void **)((caddr_t)curthread + handle) = value;
}
