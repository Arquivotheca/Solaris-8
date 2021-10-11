/*
 * Copyright (c) 1993-1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)aclmode.c 1.7 98/05/07 SMI"
/* LINTLIBARY */

/*
 * Convert ACL to/from permission bits
 */

#include <errno.h>
#include <sys/acl.h>

int
acltomode(aclent_t *aclbufp, int nentries, mode_t *modep)
{
	aclent_t		*tp;
	unsigned long		mode;
	unsigned long		grpmode;
	unsigned long		mask;
	int			which;
	int			got_mask = 0;

	*modep = 0;
	if (aclcheck(aclbufp, nentries, &which) != 0) {
		errno = EINVAL;
		return (-1);	/* errno is set in aclcheck() */
	}
	for (tp = aclbufp; nentries--; tp++) {
		if (tp->a_type == USER_OBJ) {
			mode = tp->a_perm;
			if (mode > 07)
				return (-1);
			*modep |= (mode << 6);
			continue;
		}
		if (tp->a_type == GROUP_OBJ) {
			grpmode = tp->a_perm;
			if (grpmode > 07)
				return (-1);
			continue;
		}
		if (tp->a_type == CLASS_OBJ) {
			got_mask = 1;
			mask = tp->a_perm;
			if (mask > 07)
				return (-1);
			continue;
		}
		if (tp->a_type == OTHER_OBJ) {
			mode = tp->a_perm;
			if (mode > 07)
				return (-1);
			*modep |= mode;
			continue; /* we may break here if it is sorted */
		}
	}

	/*
	 * If we have a mask then it should be intersected with the
	 * group permissions to get the effective group rights. (see
	 * bug 4091822)
	 */

	if (got_mask)
		*modep |= ((grpmode & mask) << 3);
	else
		*modep |= (grpmode << 3);
	return (0);
}



int
aclfrommode(aclent_t *aclbufp, int nentries, mode_t *modep)
{
	aclent_t		*tp;
	aclent_t		*savp;
	mode_t 			mode;
	int			which;

	if (aclcheck(aclbufp, nentries, &which) != 0) {
		errno = EINVAL;
		return (-1);	/* errno is set in aclcheck() */
	}
	for (tp = aclbufp; nentries--; tp++) {
		if (tp->a_type == USER_OBJ) {
			mode = (*modep & 0700);
			tp->a_perm = (mode >> 6);
			continue;
		}

		/*
		 * Always copy the group permissions into the GROUP_OBJ
		 * entry.  If a class entry exists, copy the group
		 * permissions into it as well.  This is consistent
		 * with making the effective group rights equal the
		 * group_perms & mask.  (see bug 4091882)
		 */
		if (tp->a_type == GROUP_OBJ || tp->a_type == CLASS_OBJ) {
			mode = (*modep & 070);
			tp->a_perm = (mode >> 3);
			continue;
		}
		if (tp->a_type == OTHER_OBJ) {
			mode = (*modep & 07);
			tp->a_perm = (o_mode_t)mode;
			continue; /* we may break here if it is sorted */
		}
	}
	return (0);
}
