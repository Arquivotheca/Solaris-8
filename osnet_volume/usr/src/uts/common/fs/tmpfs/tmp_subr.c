/*
 * Copyright (c) 1989-1997, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tmp_subr.c	1.28	99/11/05 SMI"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/time.h>
#include <sys/cmn_err.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/cred.h>
#include <sys/kmem.h>
#include <sys/atomic.h>
#include <sys/fs/tmp.h>
#include <sys/fs/tmpnode.h>

#define	MODESHIFT	3

int
tmp_taccess(struct tmpnode *tp, int mode, struct cred *cred)
{
	/*
	 * Superuser always gets access
	 */
	if (cred->cr_uid == 0)
		return (0);
	/*
	 * Check access based on owner, group and
	 * public permissions in tmpnode.
	 */
	if (cred->cr_uid != tp->tn_uid) {
		mode >>= MODESHIFT;
		if (groupmember(tp->tn_gid, cred) == 0)
			mode >>= MODESHIFT;
	}
	if ((tp->tn_mode & mode) == mode)
		return (0);
	return (EACCES);
}

/*
 * Decide whether it is okay to remove within a sticky directory.
 * Two conditions need to be met:  write access to the directory
 * is needed.  In sticky directories, write access is not sufficient;
 * you can remove entries from a directory only if you own the directory,
 * if you are the superuser, if you own the entry or if they entry is
 * a plain file and you have write access to that file.
 * Function returns 0 if remove access is granted.
 */

int
tmp_sticky_remove_access(struct tmpnode *dir, struct tmpnode *entry,
	struct cred *cr)
{
	if ((dir->tn_mode & S_ISVTX) && cr->cr_uid != 0 &&
	    cr->cr_uid != dir->tn_uid && cr->cr_uid != entry->tn_uid) {
		if (entry->tn_type == VREG)
			return (tmp_taccess(entry, VWRITE, cr));
		else
			return (EACCES);
	}

	return (0);
}

/*
 * Allocate zeroed memory if tmpfs_maxkmem has not been exceeded
 * or the 'musthave' flag is set.  'musthave' allocations should
 * always be subordinate to normal allocations so that tmpfs_maxkmem
 * can't be exceeded by more than a few KB.  Example: when creating
 * a new directory, the tmpnode is a normal allocation; if that
 * succeeds, the dirents for "." and ".." are 'musthave' allocations.
 */
void *
tmp_memalloc(size_t size, int musthave)
{
	static time_t last_warning;

	if (atomic_add_long_nv(&tmp_kmemspace, size) < tmpfs_maxkmem ||
	    musthave)
		return (kmem_zalloc(size, KM_SLEEP));

	atomic_add_long(&tmp_kmemspace, -size);
	if (last_warning != hrestime.tv_sec) {
		last_warning = hrestime.tv_sec;
		cmn_err(CE_WARN, "tmp_memalloc: tmpfs over memory limit");
	}
	return (NULL);
}

void
tmp_memfree(void *cp, size_t size)
{
	kmem_free(cp, size);
	atomic_add_long(&tmp_kmemspace, -size);
}

/*
 * Convert a string containing a number to a string.  If the number
 * is followed by a "k" or "K", the value is converted from kilobytes to
 * bytes.  If it is followed by an "m" or "M" it is converted from
 * megabytes to bytes.  If it doesn't have a character it is assumed
 * to be in bytes.  A return of -1 indicates a parse error.
 */

int
tmp_convnum(char *str)
{
	u_int num = 0;
	char *c;

	ASSERT(str);
	c = str;

	/*
	 * Convert str to number
	 */
	while ((*c >= '0') && (*c <= '9'))
		num = num * 10 + *c++ - '0';

	/*
	 * Terminate on null
	 */
	while (*c > 0) {
		switch (*c++) {

		/*
		 * convert from kilobytes
		 */
		case 'k':
		case 'K':
			num *= 1024;
			continue;

		/*
		 * convert from megabytes
		 */
		case 'm':
		case 'M':
			num *= 1024 * 1024;
			continue;

		default:
			return (-1);
		}
	}
	return (num);
}
