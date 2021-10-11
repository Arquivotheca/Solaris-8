/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)check_rtld_feature.c	1.2	98/02/23 SMI"

#include <sys/types.h>
#include <elf.h>
#include <sys/machelf.h>

extern int		_START_;

Xword
_check_rtld_feature(Xword features)
{
	Dyn *		d;
	Ehdr *		ehdr;
	Phdr *		phdr;
	int		i;
	unsigned long	dtfeatures = 0;

	/*
	 * Get the .dynamic array
	 */
	ehdr = (Ehdr *)&_START_;
	/* LINTED */
	phdr = (Phdr *)(ehdr->e_phoff + (caddr_t)ehdr);
	for (i = 0; i < ehdr->e_phnum; i++) {
		if (phdr->p_type == PT_DYNAMIC)
			break;
		/* LINTED */
		phdr = (Phdr *)((caddr_t)phdr + ehdr->e_phentsize);
	}
	if (phdr->p_type == PT_DYNAMIC) {
		if (ehdr->e_type == ET_EXEC)
			d = (Dyn *)(phdr->p_vaddr);
		else
			/* LINTED */
			d = (Dyn *)((caddr_t)ehdr + phdr->p_vaddr);
		/*
		 * Find DT_FEATURE_1
		 */
		while (d->d_tag != DT_NULL) {
			if (d->d_tag == DT_FEATURE_1) {
				dtfeatures = (unsigned long)d->d_un.d_val;
				break;
			}
			d++;
		}
	}
	/*
	 * Now we compare the features against those requested and
	 * return the flags of all of those which aren't available
	 */
	return (features & (~dtfeatures));
}
