/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986, 1987, 1988, 1989, 1990, 1991, 1995  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)seg_nf.c	1.17	99/06/01 SMI"

/*
 * VM - segment for non-faulting loads.
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/archsystm.h>

#include <vm/page.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/vpage.h>

/*
 * Private seg op routines.
 */
static int	segnf_dup(struct seg *seg, struct seg *newseg);
static int	segnf_unmap(struct seg *seg, caddr_t addr, size_t len);
static void	segnf_free(struct seg *seg);
static faultcode_t segnf_nomap(void);
static int	segnf_setprot(struct seg *seg, caddr_t addr,
		    size_t len, u_int prot);
static int	segnf_checkprot(struct seg *seg, caddr_t addr,
		    size_t len, u_int prot);
static void	segnf_badop(void);
static int	segnf_nop(void);
static int	segnf_getprot(struct seg *seg, caddr_t addr,
		    size_t len, u_int *protv);
static u_offset_t segnf_getoffset(struct seg *seg, caddr_t addr);
static int	segnf_gettype(struct seg *seg, caddr_t addr);
static int	segnf_getvp(struct seg *seg, caddr_t addr, struct vnode **vpp);
static void	segnf_dump(struct seg *seg);
static int	segnf_pagelock(struct seg *seg, caddr_t addr, size_t len,
		    struct page ***ppp, enum lock_type type, enum seg_rw rw);
static int	segnf_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp);


struct seg_ops segnf_ops = {
	segnf_dup,
	segnf_unmap,
	segnf_free,
	(faultcode_t (*)(struct hat *, struct seg *, caddr_t, size_t,
	    enum fault_type, enum seg_rw))
		segnf_nomap,		/* fault */
	(faultcode_t (*)(struct seg *, caddr_t))
		segnf_nomap,		/* faulta */
	segnf_setprot,
	segnf_checkprot,
	(int (*)())segnf_badop,		/* kluster */
	(size_t (*)(struct seg *))NULL,	/* swapout */
	(int (*)(struct seg *, caddr_t, size_t, int, u_int))
		segnf_nop,		/* sync */
	(size_t (*)(struct seg *, caddr_t, size_t, char *))
		segnf_nop,		/* incore */
	(int (*)(struct seg *, caddr_t, size_t, int, int, u_long *, size_t))
		segnf_nop,		/* lockop */
	segnf_getprot,
	segnf_getoffset,
	segnf_gettype,
	segnf_getvp,
	(int (*)(struct seg *, caddr_t, size_t, u_int))
		segnf_nop,		/* advise */
	segnf_dump,
	segnf_pagelock,
	segnf_getmemid,
};

/*
 * vnode and page for the page of zeros we use for the nf mappings.
 */
static kmutex_t segnf_lock;
static struct vnode zvp;
static struct page **zpp;

#define	addr_to_vcolor(addr)                                            \
	(shm_alignment) ?						\
	((int)(((uintptr_t)(addr) & (shm_alignment - 1)) >> PAGESHIFT)) : 0


/*
 * Must be called from startup()
 */
void
segnf_init()
{
	mutex_init(&segnf_lock, NULL, MUTEX_DEFAULT, NULL);
}


/*
 * Create a no-fault segment.
 */
/* ARGSUSED */
int
segnf_create(struct seg *seg, void *argsp)
{
	u_int prot;
	pgcnt_t	vacpgs;
	u_offset_t off = 0;
	caddr_t	vaddr = NULL;
	int i, color;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * Need a page per virtual color or just 1 if no vac.
	 */
	mutex_enter(&segnf_lock);
	if (zpp == NULL) {
		struct seg kseg;

		vacpgs = 1;
		if (shm_alignment > PAGESIZE) {
			vacpgs = shm_alignment >> PAGESHIFT;
		}

		zpp = kmem_alloc(sizeof (*zpp) * vacpgs, KM_SLEEP);

		kseg.s_as = &kas;
		for (i = 0; i < vacpgs; i++, off += PAGESIZE,
		    vaddr += PAGESIZE) {
			zpp[i] = page_create_va(&zvp, off, PAGESIZE,
			    PG_WAIT | PG_NORELOC, &kseg, vaddr);
			page_io_unlock(zpp[i]);
			page_downgrade(zpp[i]);
			pagezero(zpp[i], 0, PAGESIZE);
		}
	}
	mutex_exit(&segnf_lock);

	hat_map(seg->s_as->a_hat, seg->s_base, seg->s_size, HAT_MAP);

	/*
	 * s_data can't be NULL because of ASSERTS in the common vm code.
	 */
	seg->s_ops = &segnf_ops;
	seg->s_data = seg;

	prot = PROT_READ;
	color = addr_to_vcolor(seg->s_base);
	if (seg->s_as != &kas)
		prot |= PROT_USER;
	hat_memload(seg->s_as->a_hat, seg->s_base, zpp[color],
	    prot | HAT_NOFAULT, HAT_LOAD);
	return (0);
}

/*
 * Duplicate seg and return new segment in newseg.
 */
static int
segnf_dup(struct seg *seg, struct seg *newseg)
{
	int color;
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	newseg->s_ops = seg->s_ops;
	newseg->s_data = seg->s_data;
	color = addr_to_vcolor(newseg->s_base);

	hat_memload(newseg->s_as->a_hat, newseg->s_base,
	    zpp[color], PROT_READ | PROT_USER | HAT_NOFAULT, HAT_LOAD);
	return (0);
}

/*
 * Split a segment at addr for length len.
 */
static int
segnf_unmap(register struct seg *seg, register caddr_t addr, size_t len)
{
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * Check for bad sizes
	 */
	if (addr != seg->s_base || len != PAGESIZE)
		panic("segnf_unmap");

	/*
	 * Unload any hardware translations in the range to be taken out.
	 */
	hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD_UNMAP);
	seg_free(seg);

	return (0);
}

/*
 * Free a segment.
 */
static void
segnf_free(struct seg *seg)
{
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));
}

/*
 * No faults allowed on segnf.
 */
static faultcode_t
segnf_nomap(void)
{
	return (FC_NOMAP);
}

/* ARGSUSED */
static int
segnf_setprot(struct seg *seg, caddr_t addr, size_t len, u_int prot)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));
	return (EACCES);
}

/* ARGSUSED */
static int
segnf_checkprot(struct seg *seg, caddr_t addr, size_t len, u_int prot)
{
	u_int sprot;
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	sprot = seg->s_as == &kas ?  PROT_READ : PROT_READ|PROT_USER;
	return ((prot & sprot) == prot ? 0 : EACCES);
}

static void
segnf_badop(void)
{
	panic("segnf_badop");
	/*NOTREACHED*/
}

static int
segnf_nop(void)
{
	return (0);
}

/* ARGSUSED */
static int
segnf_getprot(struct seg *seg, caddr_t addr, size_t len, u_int *protv)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));
	ASSERT(seg->s_base == addr);

	protv[0] = PROT_READ;
	return (0);
}

static u_offset_t
segnf_getoffset(struct seg *seg, caddr_t addr)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));
	ASSERT(seg->s_base == addr);

	return ((u_offset_t)0);
}

static int
segnf_gettype(struct seg *seg, caddr_t addr)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));
	ASSERT(seg->s_base == addr);

	return (MAP_SHARED);
}

static int
segnf_getvp(struct seg *seg, caddr_t addr, struct vnode **vpp)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));
	ASSERT(seg->s_base == addr);

	*vpp = &zvp;
	return (0);
}

/*
 * segnf pages are not dumped, so we just return
 */
/* ARGSUSED */
static void
segnf_dump(struct seg *seg)
{}

/*ARGSUSED*/
static int
segnf_pagelock(struct seg *seg, caddr_t addr, size_t len,
    struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	return (ENOTSUP);
}

/*ARGSUSED*/
static int
segnf_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp)
{
	return (ENODEV);
}
