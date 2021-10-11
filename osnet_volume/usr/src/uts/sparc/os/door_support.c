/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)door_support.c	1.14	98/07/29 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/door.h>
#include <sys/door_data.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <sys/stack.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/debug.h>

/*
 * The offsets of these structure members are known in libc
 */
struct door_results {
	void		*cookie;
	char		*data_ptr;
	size_t		data_size;
	door_desc_t	*desc_ptr;
	size_t		desc_num;
	void		(*pc)();
	int		nservers;
	door_info_t	*door_info;
};

/*
 * ... and so we need a 32-bit equivalent in the 64-bit kernel.
 */
struct door_results32 {
	caddr32_t	cookie;
	caddr32_t	data_ptr;
	size32_t	data_size;
	caddr32_t	desc_ptr;
	size32_t	desc_num;
	caddr32_t	pc;
	int		nservers;
	caddr32_t	door_info;
};

/*
 * All door server threads are dispatched here.
 * 	They copy out the arguments passed by the caller and return
 *	to user land to execute the object invocation
 */
int
door_server_dispatch(door_data_t *caller_t, door_node_t *dp)
{
	struct		door_results	dr;
	caddr_t		data_ptr;
	caddr_t		did_ptr;
	int		ndid;
	int		door_size;
	void		lwp_setsp();
	void		lwp_clear_uwin();
	caddr_t		newsp;
	int		nonnative;

	nonnative = lwp_getdatamodel(ttolwp(curthread)) != DATAMODEL_NATIVE;

	if (caller_t == NULL) {		/* no caller, so no data */
		dr.data_size = 0;
		dr.desc_num = 0;
		dr.data_ptr = NULL;
		dr.desc_ptr = NULL;
		data_ptr = curthread->t_door->d_sp;
	} else {
		ASSERT(caller_t->d_flag & DOOR_HOLD);

		dr.data_ptr = caller_t->d_args.data_ptr;
		dr.data_size = caller_t->d_args.data_size;
		dr.desc_num = caller_t->d_args.desc_num;
		if ((ndid = dr.desc_num) == 0)
			door_size = 0;	/* Avoid a multiplication if 0 */
		else
			door_size = dr.desc_num * sizeof (door_desc_t);
		/*
		 * Place the arguments on the stack and point to them.
		 */
		if (nonnative)
			did_ptr = curthread->t_door->d_sp - SA32(door_size);
		else
			did_ptr = curthread->t_door->d_sp - SA(door_size);
		if (dr.data_ptr == DOOR_UNREF_DATA) {
			/* Unref upcall */
			data_ptr = did_ptr;
			dr.data_size = 0;
		} else if (dr.data_size == 0) {
			/* No data */
			data_ptr = did_ptr;
			dr.data_ptr = 0;
		} else {
			if (nonnative)
				data_ptr = did_ptr - SA32(dr.data_size);
			else
				data_ptr = did_ptr - SA(dr.data_size);
			dr.data_ptr = data_ptr;
			if (dr.data_size <= door_max_arg ||
			    caller_t->d_upcall) {
				if (copyout(caller_t->d_buf, data_ptr,
				    dr.data_size) != 0) {
					door_fp_close(caller_t->d_fpp,
					    dr.desc_num);
					return (E2BIG);
				}
			}
		}

		/*
		 * stuff the passed doors into our proc, copyout the dids
		 */
		if (ndid > 0) {
			door_desc_t *start;
			door_desc_t *didpp;
			struct file **fpp;

			start = didpp = (door_desc_t *)kmem_alloc(door_size,
			    KM_SLEEP);
			fpp = caller_t->d_fpp;

			while (ndid--) {
				if (door_insert(*fpp, didpp) == -1) {
					/*
					 * Cleanup up newly created fd's
					 * and close any remaining fps.
					 */
					door_fd_close(start, didpp - start);
					door_fp_close(fpp, ndid + 1);
					kmem_free(start, door_size);
					return (EMFILE);
				}
				didpp++; fpp++;
			}
			if (copyout((caddr_t)start, did_ptr, door_size)) {
				door_fd_close(start, caller_t->d_args.desc_num);
				kmem_free(start, door_size);
				return (E2BIG);
			}
			kmem_free(start, door_size);
			dr.desc_ptr = (door_desc_t *)did_ptr;
		} else {
			dr.desc_ptr = NULL;
		}
	}
	dr.pc = dp->door_pc;
	dr.cookie = dp->door_data;

	/* Is this the last server thread? */
	if (dp->door_flags & DOOR_PRIVATE) {
		door_info_t	di;

		if (dp->door_servers == NULL) {
			/* Pass information about which door pool is depleted */
			di.di_target = curproc->p_pid;
			di.di_proc = (door_ptr_t)dp->door_pc;
			di.di_data = (door_ptr_t)dp->door_data;
			di.di_uniquifier = dp->door_index;
			di.di_attributes = dp->door_flags | DOOR_LOCAL;

			if (nonnative)
				data_ptr = data_ptr - SA32(sizeof (di));
			else
				data_ptr = data_ptr - SA(sizeof (di));
			dr.nservers = 0;
			dr.door_info = (door_info_t *)data_ptr;
			if (copyout((caddr_t)&di, data_ptr, sizeof (di)) != 0) {
				/* XXX Close descriptors */
				return (E2BIG);
			}
		} else {
			dr.nservers = 1;
			dr.door_info = NULL;
		}

	} else {
		dr.nservers = (curproc->p_server_threads == NULL) ? 0 : 1;
		dr.door_info = NULL;
	}

	if (nonnative) {
		struct door_results32 dr32;

		dr32.cookie = (caddr32_t)dr.cookie;
		dr32.data_ptr = (caddr32_t)dr.data_ptr;
		dr32.data_size = dr.data_size;
		dr32.desc_ptr = (caddr32_t)dr.desc_ptr;
		dr32.desc_num = dr.desc_num;
		dr32.pc = (caddr32_t)dr.pc;
		dr32.nservers = dr.nservers;
		dr32.door_info = (caddr32_t)dr.door_info;
		if (copyout(&dr32, data_ptr - SA32(sizeof (dr32)),
		    sizeof (dr32)) != 0) {
			/* XXX Close descriptors */
			return (E2BIG);
		}
		newsp = data_ptr - SA32(sizeof (dr32)) - SA32(MINFRAME32);
	} else {
		if (copyout(&dr, data_ptr - SA(sizeof (dr)),
		    sizeof (dr)) != 0) {
			/* XXX Close descriptors */
			return (E2BIG);
		}
		newsp = data_ptr - SA(sizeof (dr)) - SA(MINFRAME) - STACK_BIAS;
	}

	/*
	 * There may be some user register windows stashed away
	 * because our user thread stack wasn't available during some
	 * kernel overflows. We don't care about this saved user
	 * state since we are resetting our stack. Make sure we
	 * don't try to push these registers out to our stack
	 * later on when returning from this system call.
	 *
	 * We have guaranteed that no new user windows will stored
	 * to the pcb save area at this point since a door server
	 * thread always does a full context switch (shuttle_switch,
	 * shuttle_resume) before making itself available for a door
	 * invocation.
	 */
	lwp_clear_uwin();
	lwp_setsp(ttolwp(curthread), newsp);
	return (0);
}

/*
 * Return the address on the stack where argument data will be stored
 */
caddr_t
door_arg_addr(caddr_t sp, size_t asize, size_t desc_size)
{
	return (sp - SA(asize) - SA(desc_size));
}

#if defined(_SYSCALL32_IMPL)
/*
 * Return the address on the 32-bit stack where argument data will be stored
 */
caddr_t
door_arg_addr32(caddr_t sp, size_t asize, size_t desc_size)
{
	return (sp - SA32(asize) - SA32(desc_size));
}
#endif
