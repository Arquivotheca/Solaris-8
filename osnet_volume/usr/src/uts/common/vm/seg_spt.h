/*
 * Copyright (c) 1993 - 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_VM_SPT_H
#define	_VM_SPT_H

#pragma ident	"@(#)seg_spt.h	1.13	99/06/24 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>

#ifdef	__cplusplus
extern "C" {
#endif



/*
 * Passed data when creating spt segment.
 */
struct  segspt_crargs {
	struct	seg	*seg_spt;
	struct anon_map *amp;
	uint_t		prot;
};

struct spt_data {
	struct vnode	*vp;
	struct anon_map	*amp;
	size_t 		realsize;
	struct page	**ppa;
	uint_t		prot;
	kmutex_t 	lock;
	size_t		pcachecnt;	/* # of times in pcache */
};

/*
 * Private data for spt_shm segment.
 */
struct sptshm_data {
	struct as	*sptas;
	struct anon_map *amp;
	size_t		softlockcnt;	/* # outstanding lock operations */
	struct seg 	*sptseg;	/* pointer to spt segment */
};

#ifdef _KERNEL

/*
 * Functions used in shm.c to call ISM.
 */
int	sptcreate(size_t size, struct seg **sptseg, struct anon_map *amp,
	    uint_t prot);
void	sptdestroy(struct as *, struct anon_map *);
int	segspt_shmattach(struct seg *, caddr_t *);

#define	isspt(sp)	((sp)->shm_sptinfo ? (sp)->shm_sptinfo->sptas : NULL)
#define	spt_on(a)	(share_page_table || ((a) & SHM_SHARE_MMU))

/*
 * This can be applied to a segment with seg->s_ops == &segspt_shmops
 * to determine the real size of the ISM segment.
 */
#define	spt_realsize(seg) (((struct spt_data *) \
	(((struct sptshm_data *)((seg)->s_data))->sptseg->s_data))->realsize)

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_SPT_H */
