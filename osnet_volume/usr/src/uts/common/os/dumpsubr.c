/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dumpsubr.c	1.15	99/12/04 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/mem.h>
#include <sys/mman.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/memlist.h>
#include <sys/dumphdr.h>
#include <sys/dumpadm.h>
#include <sys/ksyms.h>
#include <sys/compress.h>
#include <sys/stream.h>
#include <sys/strsun.h>
#include <sys/cmn_err.h>
#include <sys/bitmap.h>
#include <sys/modctl.h>
#include <sys/utsname.h>
#include <sys/systeminfo.h>
#include <sys/vmem.h>
#include <sys/log.h>
#include <sys/var.h>
#include <sys/debug.h>
#include <sys/sunddi.h>
#include <fs/fs_subr.h>
#include <sys/fs/snode.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>

kmutex_t	dump_lock;	/* lock for dump configuration */
dumphdr_t	*dumphdr;	/* dump header */
int		dump_conflags = DUMP_KERNEL; /* dump configuration flags */
vnode_t		*dumpvp;	/* dump device vnode pointer */
u_offset_t	dumpvp_size;	/* size of dump device, in bytes */
static u_offset_t dumpvp_limit;	/* maximum write offset */
char		*dumppath;	/* pathname of dump device */
int		dump_timeout = 60; /* timeout for dumping a page during panic */
int		dump_timeleft;	/* portion of dump_timeout remaining */

#ifdef DEBUG
int		dumpfaildebug = 1;	/* enter debugger if dump fails */
#else
int		dumpfaildebug = 0;
#endif

static ulong_t	*dump_bitmap;	/* bitmap for marking pages to dump */
static pgcnt_t	dump_bitmapsize; /* size of bitmap */
static pid_t	*dump_pids;	/* list of process IDs at dump time */
static int	dump_ioerr;	/* dump i/o error */
static offset_t	dumpvp_off;	/* current dump device offset */
static char	*dump_cmap;	/* VA for dump compression mapping */
static char	*dumpbuf_cur, *dumpbuf_start, *dumpbuf_end, *dump_cbuf;
static size_t	dumpbuf_size;

static void
dumphdr_init(void)
{
	pgcnt_t npages = 0;
	struct memlist *mp;

	ASSERT(MUTEX_HELD(&dump_lock));

	if (dumphdr == NULL) {
		dumphdr = kmem_zalloc(sizeof (dumphdr_t), KM_SLEEP);
		dumphdr->dump_magic = DUMP_MAGIC;
		dumphdr->dump_version = DUMP_VERSION;
		dumphdr->dump_wordsize = DUMP_WORDSIZE;
		dumphdr->dump_pageshift = PAGESHIFT;
		dumphdr->dump_pagesize = PAGESIZE;
		dumphdr->dump_utsname = utsname;
		(void) strcpy(dumphdr->dump_platform, platform);
		dump_cmap = vmem_alloc(heap_arena, PAGESIZE, VM_SLEEP);
		/*
		 * The dump i/o buffer must be at least one page, at most
		 * maxphys bytes, and should scale with physmem in between.
		 */
		dumpbuf_size = MAX(PAGESIZE, MIN(maxphys, physmem) & PAGEMASK);
		dumpbuf_start = kmem_alloc(dumpbuf_size, KM_SLEEP);
		dumpbuf_end = dumpbuf_start + dumpbuf_size;
		dump_cbuf = kmem_alloc(PAGESIZE, KM_SLEEP); /* compress buf */
		dump_pids = kmem_alloc(v.v_proc * sizeof (pid_t), KM_SLEEP);
	}

	for (mp = phys_install; mp != NULL; mp = mp->next)
		npages += mp->size >> PAGESHIFT;

	if (dump_bitmapsize != npages) {
		void *map = kmem_alloc(BT_BITOUL(npages) * BT_NBIPUL, KM_SLEEP);
		kmem_free(dump_bitmap, BT_BITOUL(dump_bitmapsize) * BT_NBIPUL);
		dump_bitmap = map;
		dump_bitmapsize = npages;
	}
}

/*
 * Establish a new dump device.
 */
int
dumpinit(vnode_t *vp, char *name, int justchecking)
{
	vnode_t *cvp;
	vattr_t vattr;
	int error = 0;

	ASSERT(MUTEX_HELD(&dump_lock));

	dumphdr_init();

	cvp = common_specvp(vp);
	if (cvp == dumpvp)
		return (0);

	/*
	 * Determine whether this is a plausible dump device.  We want either:
	 * (1) a real device that's not mounted and has a cb_dump routine, or
	 * (2) a swapfile on some filesystem that has a vop_dump routine.
	 */
	if ((error = VOP_OPEN(&cvp, FREAD | FWRITE, kcred)) != 0)
		return (error);

	vattr.va_mask = AT_SIZE | AT_TYPE | AT_RDEV;
	if ((error = VOP_GETATTR(cvp, &vattr, 0, kcred)) == 0) {
		if (vattr.va_type == VBLK || vattr.va_type == VCHR) {
			if (devopsp[getmajor(vattr.va_rdev)]->
			    devo_cb_ops->cb_dump == nodev)
				error = ENOTSUP;
			else if (vfs_devismounted(vattr.va_rdev))
				error = EBUSY;
		} else {
			if (cvp->v_op->vop_dump == fs_nosys || !IS_SWAPVP(cvp))
				error = ENOTSUP;
		}
	}

	if (error == 0 && vattr.va_size < 2 * DUMP_LOGSIZE)
		error = ENOSPC;

	if (error || justchecking) {
		(void) VOP_CLOSE(cvp, FREAD | FWRITE, 1, (offset_t)0, kcred);
		return (error);
	}

	VN_HOLD(cvp);

	if (dumpvp != NULL)
		dumpfini();	/* unconfigure the old dump device */

	dumpvp = cvp;
	dumpvp_size = vattr.va_size & -DUMP_OFFSET;
	dumppath = kmem_alloc(strlen(name) + 1, KM_SLEEP);
	(void) strcpy(dumppath, name);

	cmn_err(CE_CONT, "?dump on %s size %llu MB\n", name, dumpvp_size >> 20);

	return (0);
}

void
dumpfini(void)
{
	ASSERT(MUTEX_HELD(&dump_lock));

	kmem_free(dumppath, strlen(dumppath) + 1);

	(void) VOP_CLOSE(dumpvp, FREAD | FWRITE, 1, (offset_t)0, kcred);

	VN_RELE(dumpvp);

	dumpvp = NULL;
	dumpvp_size = 0;
	dumppath = NULL;
}

static pfn_t
dump_bitnum_to_pfn(pgcnt_t bitnum)
{
	struct memlist *mp;

	for (mp = phys_install; mp != NULL; mp = mp->next) {
		if (bitnum < (mp->size >> PAGESHIFT))
			return ((mp->address >> PAGESHIFT) + bitnum);
		bitnum -= mp->size >> PAGESHIFT;
	}
	return (PFN_INVALID);
}

static pgcnt_t
dump_pfn_to_bitnum(pfn_t pfn)
{
	struct memlist *mp;
	pgcnt_t bitnum = 0;

	for (mp = phys_install; mp != NULL; mp = mp->next) {
		if (pfn >= (mp->address >> PAGESHIFT) &&
		    pfn < ((mp->address + mp->size) >> PAGESHIFT))
			return (bitnum + pfn - (mp->address >> PAGESHIFT));
		bitnum += mp->size >> PAGESHIFT;
	}
	return ((pgcnt_t)-1);
}

static offset_t
dumpvp_flush(void)
{
	size_t size = P2ROUNDUP(dumpbuf_cur - dumpbuf_start, PAGESIZE);
	int err;

	if (dumpvp_off + size > dumpvp_limit) {
		dump_ioerr = ENOSPC;
	} else if (size != 0) {
		if (panicstr)
			err = VOP_DUMP(dumpvp, dumpbuf_start,
			    lbtodb(dumpvp_off), btod(size));
		else
			err = vn_rdwr(UIO_WRITE, dumpvp, dumpbuf_start, size,
			    dumpvp_off, UIO_SYSSPACE, 0, dumpvp_limit,
			    kcred, 0);
		if (err && dump_ioerr == 0)
			dump_ioerr = err;
	}
	dumpvp_off += size;
	dumpbuf_cur = dumpbuf_start;
	dump_timeleft = dump_timeout;
	return (dumpvp_off);
}

static void
dumpvp_write(const void *va, size_t size)
{
	while (size != 0) {
		size_t len = MIN(size, dumpbuf_end - dumpbuf_cur);
		if (len == 0) {
			(void) dumpvp_flush();
		} else {
			bcopy(va, dumpbuf_cur, len);
			va = (char *)va + len;
			dumpbuf_cur += len;
			size -= len;
		}
	}
}

/*ARGSUSED*/
static void
dumpvp_ksyms_write(const void *src, void *dst, size_t size)
{
	dumpvp_write(src, size);
}

/*
 * Mark 'pfn' in the bitmap and dump its translation table entry.
 */
void
dump_addpage(struct as *as, void *va, pfn_t pfn)
{
	mem_vtop_t mem_vtop;
	pgcnt_t bitnum;

	if ((bitnum = dump_pfn_to_bitnum(pfn)) != (pgcnt_t)-1) {
		if (!BT_TEST(dump_bitmap, bitnum)) {
			dumphdr->dump_npages++;
			BT_SET(dump_bitmap, bitnum);
		}
		dumphdr->dump_nvtop++;
		mem_vtop.m_as = as;
		mem_vtop.m_va = va;
		mem_vtop.m_pfn = pfn;
		dumpvp_write(&mem_vtop, sizeof (mem_vtop_t));
	}
	dump_timeleft = dump_timeout;
}

/*
 * Dump the <as, va, pfn> information for a given address space.
 * SEGOP_DUMP() will call dump_addpage() for each page in the segment.
 */
static void
dump_as(struct as *as)
{
	struct seg *seg, *prev;

	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	for (prev = NULL, seg = AS_SEGP(as, as->a_segs); seg != NULL;
	    prev = seg, seg = AS_SEGP(as, seg->s_next)) {
		if (seg->s_as != as || seg->s_ops == NULL ||
		    seg->s_prev != prev)
			break;
		SEGOP_DUMP(seg);
	}
	AS_LOCK_EXIT(as, &as->a_lock);

	if (seg != NULL)
		cmn_err(CE_WARN, "invalid segment %p in address space %p",
		    (void *)seg, (void *)as);
}

void
dump_messages(void)
{
	log_dump_t ld;
	mblk_t *mctl, *mdata;
	queue_t *q, *qlast;
	u_offset_t dumpvp_start;

	if (dumpvp == NULL || dumphdr == NULL || log_consq == NULL)
		return;

	dumpbuf_cur = dumpbuf_start;
	dumpvp_limit = dumpvp_size - DUMP_OFFSET;
	dumpvp_start = dumpvp_limit - DUMP_LOGSIZE;
	dumpvp_off = dumpvp_start;

	qlast = NULL;
	do {
		for (q = log_consq; q->q_next != qlast; q = q->q_next)
			continue;
		for (mctl = q->q_first; mctl != NULL; mctl = mctl->b_next) {
			dump_timeleft = dump_timeout;
			mdata = mctl->b_cont;
			ld.ld_magic = LOG_MAGIC;
			ld.ld_msgsize = MBLKL(mctl->b_cont);
			ld.ld_csum = checksum32(mctl->b_rptr, MBLKL(mctl));
			ld.ld_msum = checksum32(mdata->b_rptr, MBLKL(mdata));
			dumpvp_write(&ld, sizeof (ld));
			dumpvp_write(mctl->b_rptr, MBLKL(mctl));
			dumpvp_write(mdata->b_rptr, MBLKL(mdata));
		}
	} while ((qlast = q) != log_consq);

	ld.ld_magic = 0;		/* indicate end of messages */
	dumpvp_write(&ld, sizeof (ld));
	(void) dumpvp_flush();
	if (!panicstr) {
		(void) VOP_PUTPAGE(dumpvp, dumpvp_start,
		    (size_t)(dumpvp_off - dumpvp_start),
		    B_INVAL | B_FORCE, kcred);
	}
}

/*
 * Dump the system.
 */
void
dumpsys(void)
{
	pfn_t pfn;
	pgcnt_t bitnum;
	int npages = 0;
	int percent_done = 0;
	uint32_t csize;
	u_offset_t total_csize = 0;
	int compress_ratio;
	proc_t *p;
	pid_t npids, pidx;

	if (dumpvp == NULL || dumphdr == NULL) {
		uprintf("skipping system dump - no dump device configured\n");
		return;
	}
	dumpbuf_cur = dumpbuf_start;

	/*
	 * Calculate the starting block for dump.  If we're dumping on a
	 * swap device, start 1/5 of the way in; otherwise, start at the
	 * beginning.  And never use the first page -- it may be a disk label.
	 */
	if (dumpvp->v_flag & VISSWAP)
		dumphdr->dump_start = P2ROUNDUP(dumpvp_size / 5, DUMP_OFFSET);
	else
		dumphdr->dump_start = DUMP_OFFSET;

	dumphdr->dump_flags = DF_VALID | DF_COMPLETE | DF_LIVE;
	dumphdr->dump_crashtime = hrestime.tv_sec;
	dumphdr->dump_npages = 0;
	dumphdr->dump_nvtop = 0;
	bzero(dump_bitmap, dump_bitmapsize);
	dump_timeleft = dump_timeout;

	if (panicstr) {
		dumphdr->dump_flags &= ~DF_LIVE;
		(void) VOP_DUMPCTL(dumpvp, DUMP_FREE, NULL);
		(void) VOP_DUMPCTL(dumpvp, DUMP_ALLOC, NULL);
		(void) vsnprintf(dumphdr->dump_panicstring, DUMP_PANICSIZE,
		    panicstr, panicargs);
	}

	uprintf("dumping to %s, offset %lld\n", dumppath, dumphdr->dump_start);

	/*
	 * Leave room for the message save area and terminal dump header.
	 */
	dumpvp_limit = dumpvp_size - DUMP_LOGSIZE - DUMP_OFFSET;

	/*
	 * Write out the symbol table.  It's no longer compressed,
	 * so its 'size' and 'csize' are equal.
	 */
	dumpvp_off = dumphdr->dump_ksyms = dumphdr->dump_start + PAGESIZE;
	dumphdr->dump_ksyms_size = dumphdr->dump_ksyms_csize =
	    ksyms_snapshot(dumpvp_ksyms_write, NULL, LONG_MAX);

	/*
	 * Write out the translation map.
	 */
	dumphdr->dump_map = dumpvp_flush();
	dump_as(&kas);
	if (dump_conflags & DUMP_ALL) {
		mutex_enter(&pidlock);
		for (npids = 0, p = practive; p != NULL; p = p->p_next)
			dump_pids[npids++] = p->p_pid;
		mutex_exit(&pidlock);
		for (pidx = 0; pidx < npids; pidx++) {
			p = sprlock(dump_pids[pidx]);
			if (p == NULL)
				continue;
			if (p->p_as != &kas) {
				mutex_exit(&p->p_lock);
				dump_as(p->p_as);
				mutex_enter(&p->p_lock);
			}
			sprunlock(p);
		}
		for (bitnum = 0; bitnum < dump_bitmapsize; bitnum++) {
			dump_timeleft = dump_timeout;
			BT_SET(dump_bitmap, bitnum);
		}
		dumphdr->dump_npages = dump_bitmapsize;
	}
	dumphdr->dump_hashmask = (1 << highbit(dumphdr->dump_nvtop - 1)) - 1;

	/*
	 * Write out the pfn table.
	 */
	dumphdr->dump_pfn = dumpvp_flush();
	for (bitnum = 0; bitnum < dump_bitmapsize; bitnum++) {
		dump_timeleft = dump_timeout;
		if (!BT_TEST(dump_bitmap, bitnum))
			continue;
		pfn = dump_bitnum_to_pfn(bitnum);
		ASSERT(pfn != PFN_INVALID);
		dumpvp_write(&pfn, sizeof (pfn_t));
	}

	/*
	 * Write out all the pages.
	 */
	dumphdr->dump_data = dumpvp_flush();
	for (bitnum = 0; bitnum < dump_bitmapsize; bitnum++) {
		dump_timeleft = dump_timeout;
		if (!BT_TEST(dump_bitmap, bitnum))
			continue;
		pfn = dump_bitnum_to_pfn(bitnum);
		ASSERT(pfn != PFN_INVALID);

		/*
		 * Map in, compress, and write out page frame 'pfn'.
		 */
		hat_devload(kas.a_hat, dump_cmap, PAGESIZE, pfn, PROT_READ,
		    HAT_LOAD_NOCONSIST);
		csize = (uint32_t)compress(dump_cmap, dump_cbuf, PAGESIZE);
		hat_unload(kas.a_hat, dump_cmap, PAGESIZE, HAT_UNLOAD);
		dumpvp_write(&csize, sizeof (uint32_t));
		dumpvp_write(dump_cbuf, csize);
		if (dump_ioerr) {
			dumphdr->dump_flags &= ~DF_COMPLETE;
			dumphdr->dump_npages = npages;
			break;
		}
		total_csize += csize;
		if (++npages * 100LL / dumphdr->dump_npages > percent_done) {
			uprintf("^\r%3d%% done", ++percent_done);
			if (!panicstr)
				delay(1);	/* let the output be sent */
		}
	}

	(void) dumpvp_flush();

	/*
	 * Write out the initial and terminal dump headers.
	 */
	dumpvp_off = dumphdr->dump_start;
	dumpvp_write(dumphdr, sizeof (dumphdr_t));
	(void) dumpvp_flush();

	dumpvp_limit = dumpvp_size;
	dumpvp_off = dumpvp_limit - DUMP_OFFSET;
	dumpvp_write(dumphdr, sizeof (dumphdr_t));
	(void) dumpvp_flush();

	compress_ratio = (int)(100LL * npages / (btopr(total_csize + 1)));

	uprintf("\r%3d%% done: %d pages dumped, compression ratio %d.%02d, ",
	    percent_done, npages, compress_ratio / 100, compress_ratio % 100);

	if (dump_ioerr == 0) {
		uprintf("dump succeeded\n");
	} else {
		uprintf("dump failed: error %d\n", dump_ioerr);
		if (panicstr && dumpfaildebug)
			debug_enter("dump failed");
	}

	/*
	 * Write out all undelivered messages.  This has to be the *last*
	 * thing we do because the dump process itself emits messages.
	 */
	if (panicstr)
		dump_messages();

	delay(2 * hz);	/* let people see the 'done' message */
	dump_timeleft = 0;
	dump_ioerr = 0;
}

/*
 * This function is called whenever the memory size, as represented
 * by the phys_install list, changes.
 */
void
dump_resize()
{
	mutex_enter(&dump_lock);
	dumphdr_init();
	mutex_exit(&dump_lock);
}
