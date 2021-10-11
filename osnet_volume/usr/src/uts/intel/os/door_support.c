/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)door_support.c	1.9	99/05/04 SMI"

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

#if defined(__ia64)

/*
 * IA64 needs to support IA32 applications, which is a bit of a problem.
 * The problem is that "long long" on IA32 is only 4-byte aligned,
 * instead of 8-byte aligned as it is on IA64 and SPARC (both v7 and v9).
 * "struct door_desc" and "struct door_info" need special handling when
 * being passed to or from IA32 applications because they contain
 * "long long"s that are not 8-byte aligned.  We use the functions below
 * to do the dirty work in the ia64 kernel, but direct the calls to the
 * standard copyin/copyout functions as was always done on the other kernels.
 */

int copyin_desc(const door_desc_t *uaddr, door_desc_t *kaddr, size_t count);
int copyout_desc(const door_desc_t *kaddr, door_desc_t *uaddr, size_t count);
int copyout_info(const door_info_t *kaddr, door_info_t *uaddr, size_t count);

#else

#define	copyin_desc	copyin
#define	copyout_desc	copyout
#define	copyout_info	copyout

#endif

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

struct door_info32 {
	pid_t		di_target;
	uint32_t	di_proc[2];	/* improperly aligned for ia32 */
	uint32_t	di_data[2];	/* improperly aligned for ia32 */
	door_attr_t	di_attributes;
	door_id_t	di_uniquifier;
	int		di_resv[4];
};

typedef struct door_desc32 {
	door_attr_t	d_attributes;	/* Tag for union */
	union {
		/* File descriptor is passed */
		struct {
			int		d_descriptor;
			uint32_t	d_id[2];
		} d_desc;
		/* Reserved space */
		int		d_resv[5];
	} d_data;
} door_desc32_t;

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
			if (copyout_desc(start, (door_desc_t *)did_ptr,
			    door_size)) {
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
			if (copyout_info(&di, dr.door_info, sizeof (di)) != 0) {
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
	lwp_setsp(ttolwp(curthread), newsp);
	return (0);
}

/*
 * Return the address on the stack where argument data will be stored
 */
caddr_t
door_arg_addr(caddr_t sp, size_t asize, size_t desc_size)
{
	return ((caddr_t)((ulong_t)sp & ~(STACK_ALIGN - 1)) - SA(asize) -
	    SA(desc_size));
}

#if defined(_SYSCALL32_IMPL)
/*
 * Return the address on the 32-bit stack where argument data will be stored
 */
caddr_t
door_arg_addr32(caddr_t sp, size_t asize, size_t desc_size)
{
	return ((caddr_t)((ulong_t)sp & ~(STACK_ALIGN32 - 1)) - SA32(asize) -
	    SA32(desc_size));
}

int
copyin_desc(const door_desc_t *uaddr, door_desc_t *d, size_t count)
{
	if (is_ia32_process(curproc)) {
		uint_t ndid, i, retval;
		size_t count32;
		door_desc32_t *d32, *origd32;

		ndid = count / sizeof (door_desc_t);
		count32 = ndid * sizeof (door_desc32_t);
		origd32 = d32 = kmem_alloc(count32, KM_SLEEP);
		retval = copyin(uaddr, origd32, count32);
		if (retval == 0)
			for (i = 0; i < ndid; i++, d32++, d++) {
				d->d_attributes = d32->d_attributes;
				d->d_data.d_desc.d_descriptor =
				    d32->d_data.d_desc.d_descriptor;
				bcopy(d32->d_data.d_desc.d_id,
				    &d->d_data.d_desc.d_id,
				    sizeof (d->d_data.d_desc.d_id));
			}
		kmem_free(origd32, count32);
		return (retval);
	} else
		return (copyin(uaddr, d, count));
}

int
copyout_desc(const door_desc_t *d, door_desc_t *uaddr, size_t count)
{
	if (is_ia32_process(curproc)) {
		uint_t ndid, i, retval;
		size_t count32;
		door_desc32_t *d32, *origd32;

		ndid = count / sizeof (door_desc_t);
		count32 = ndid * sizeof (door_desc32_t);
		origd32 = d32 = kmem_zalloc(count32, KM_SLEEP);
		for (i = 0; i < ndid; i++, d32++, d++) {
			d32->d_attributes = d->d_attributes;
			d32->d_data.d_desc.d_descriptor =
			    d->d_data.d_desc.d_descriptor;
			bcopy(&d->d_data.d_desc.d_id, d32->d_data.d_desc.d_id,
			    sizeof (d->d_data.d_desc.d_id));
		}
		retval = copyout(origd32, uaddr, count32);
		kmem_free(origd32, count32);
		return (retval);
	} else
		return (copyout(d, uaddr, count));
}

int
copyout_info(const door_info_t *di, door_info_t *uaddr, size_t count)
{
	if (is_ia32_process(curproc)) {
		struct door_info32 di32;
		di32.di_target = di->di_target;
		bcopy(&di->di_proc, &di32.di_proc, sizeof (di->di_proc));
		bcopy(&di->di_data, &di32.di_data, sizeof (di->di_data));
		di32.di_attributes = di->di_attributes;
		di32.di_uniquifier = di->di_uniquifier;
		return (copyout(&di32, uaddr, sizeof (di32)));
	} else
		return (copyout(di, uaddr, count));
}

#endif
