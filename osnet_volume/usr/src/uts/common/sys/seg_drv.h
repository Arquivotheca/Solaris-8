/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SEG_DRV_H
#define	_SYS_SEG_DRV_H

#pragma ident	"@(#)seg_drv.h	1.5	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Device-driver segment driver.  This interface allows device drivers
 * to interpose on any part of the system segment driver.  The device
 * driver does not need to reproduce the entire 900 lines of segment
 * driver code.
 *
 *
 * to use: a device driver provides its own segmap routine which is
 * placed in the cb_ops structure:
 *
 * static int
 * xx_segmap(dev, off, as, addrp, len, prot, maxprot, flags, cred)
 *	dev_t	dev;		device number
 *	off_t	off;		device offset provided to mmap(2)
 *	struct as *as;		process address space.
 *	caddr_t	*addrp;		returned address
 *	uint_t	len;		length provided to mmap(2)
 *	uint_t	prot;		requested access from <sys/mman.h>
 *	uint_t	maxprot;	maximum protection
 *	uint_t	flags;		flags provided to mmap(2)
 *	struct cred *cred;	user credentials
 *
 *	See segmap(9e) for more info
 *
 *
 * your device driver should perform any basic checking it wants (i.e.
 * verify the address range within the device is legitimate, verify
 * permissions and so on.  If you driver wants to reject, it can return
 * any value from <errno.h>.  Typical reasons for reject might be:
 *
 *	EINVAL:		(flags & MAP_TYPE) != MAP_SHARED
 *			in case your device does not support private maps.
 *
 *	ENXIO:		part or all of address range is invalid.  Your
 *			xx_mmap() routine *must* return valid output for
 *			all pages in this range.
 *
 * If you choose not to reject, then you call segdrv_segmap with the
 * following arguments:
 *
 * int
 * segdrv_segmap(dev, off, as, addrp, len, prot, maxprot, flags, cred,
 *		client_data, client_segops);
 *	dev_t		dev;
 *	off_t		off;
 *	struct as	*as;
 *	caddr_t		*addrp;
 *	uint_t		len;
 *	uint_t		prot;
 *	uint_t		maxprot;
 *	uint_t		flags;
 *	struct cred	*cred;
 *	caddr_t		client_data;		driver private data
 *	struct seg_ops	*client_segops;		see <vm/seg.h>
 *	int		(*xx_create)();		pointer to xx_create()
 *
 * and return whatever segdrv_segmap() returns.  segdrv_segmap() returns
 * 0 on success.  segdrv_segmap() may also return one of the following
 * error codes:
 *
 *	ENODEV:		driver has no xx_mmap() routine
 *	ENOMEM:		unable to assign address space.
 *
 * The two extra arguments, client_data and client_segops are a private
 * data structure for the segment, and your driver's ops vector.  These
 * will both be bound to the segment of address space being mapped.  Any
 * time an operation is carried out on this segment of address space, the
 * appropriate function from your ops vector will be called.  One of the
 * arguments to these functions will always be the segment.  You can get
 * your private data from the segment with
 *
 *	my_data = ((segdrv_data *)(seg->s_data))->client;
 *
 * Any item in your ops vector may be NULL, in which case segdrv will
 * perform the default function.  Otherwise, segdrv will call your
 * function first.  In most cases, your function will return either an
 * error value from <errno.h>, or one of the following:
 */

#define	SEGDRV_CONTINUE		-10	/* driver does not want to handle */
					/* this operation, use default */

#define	SEGDRV_HANDLED		-11	/* driver has handled this operation, */
					/* segdrv should handle VM */
					/* bookkeeping and return success */

#define	SEGDRV_IGNORE		-12	/* driver has handled this operation */
					/* entirely, segdrv should */
					/* immediately return success */


/*
 * Note that the SEGDRV_CONTINUE does not necessarily mean that you did
 * nothing; your driver may have done all sorts of things such as internal
 * bookkeeping, but wishes segdrv to continue normally.
 *
 * In most cases, SEGDRV_CONTINUE and SEGDRV_HANDLED are equivalent since
 * there's nothing to do but bookkeeping anyway.
 *
 * Note also that returning SEGDRV_IGNORE can be very dangerous; you must
 * make sure your driver performs various VM bookkeeping that would
 * otherwise have been handled by segdrv.  See the source code to segdrv
 * to see what's expected.  Note that this also changes significantly from
 * release to release.
 *
 * The routines that your provide in your ops vector are as follows:
 *
 * int
 * xx_create(seg, args)
 *	struct seg	*seg;
 *	void		*args;
 *
 *	Called by segdrv_segmap() after the segment has been created.
 *	You may wish, for instance, to use the opportunity to place a
 *	back-pointer from your private data structure to the segment.
 *	The "args" argument will be NULL.
 *
 *	This function is also called by segdrv_unmap() if a segment is
 *	split in two.  In this case, this function will be called for
 *	the newly-created second fragment.  This newly-created fragment
 *	will have a higher address and device offset than the
 *	original.  The client_data field from the original segment will
 *	be provided as the "args" argument.  In this case, you should
 *	duplicate this structure, and store the new copy into
 *	((segdrv_data *)(seg->s_data))->client
 *
 * int
 * xx_dup(seg, newseg)
 *	struct seg	*seg, *newseg;
 *
 *	Called when an address space is being cloned, as during
 *	fork(2).  You should make a copy of your client_private data
 *	and place it in ((segdrv_data *)(newseg->s_data))->client.
 *
 *	Note that information about the newly-created child process in
 *	a fork is *NOT* available at this time.
 *
 *	This function may not return SEGDRV_IGNORE.  It may return an
 *	errno value, although this is highly anti-social -- it causes
 *	fork(2) to fail.
 *
 * int
 * xx_unmap(seg, addr, len)
 *	struct seg	*seg;
 *	caddr_t		addr;
 *	uint_t		len;
 *
 *	Called when the user process has called munmap(2).  This is not
 *	the time to be freeing up resources or anything; xx_free() will
 *	be called momentarily.
 *
 *	It is possible that only a fragment of the segment is being
 *	unmapped.  This could result in the segment being shorter,
 *	having a higher start address and offset, or even being split
 *	into two segments.  In the latter case, xx_create() will be
 *	called
 *
 *	It's unknown what would happen if this routine returned an error
 *
 * void
 * xx_free(seg)
 *	struct seg	*seg;
 *
 *	Called when a segment is freed.  You should free up any
 *	resources you have allocated for this segment, including the
 *	client_data in ((segdrv_data *)(seg->s_data))->client
 *
 *	This function does not return anything.
 *
 * faultcode_t
 * xx_fault(hat, seg, addr, len, type, rw)
 *	struct hat	*hat;
 *	struct seg	*seg;
 *	caddr_t		addr;
 *	uint_t		len;
 *	enum fault_type	type;	<vm/seg.h>
 *	enum seg_rw	rw;	<vm/seg.h>
 *
 *	Called when a page fault happens.  'hat' is the "hardware
 *	address translation" table associated with the address space.
 *	(It is used as an argument to other functions.)  'addr' is the
 *	user address of the segment that faulted, 'len' is the length
 *	of the range, 'type' is from <vm/seg.h>:  F_INVAL is the normal
 *	case; a segment needs to be placed in the page table, 'rw'
 *	gives the type of access.  F_PROT is normally a fatal error,
 *	but your driver may chose to implement copy-on-write.
 *	F_SOFTLOCK and F_SOFTUNLOCK deal with page locking and it's
 *	simplest to return SEGDRV_CONTINUE and let segdrv handle it.
 *
 *	If your xx_fault() function returns SEGDRV_CONTINUE, segdrv
 *	will proceed as if xx_fault() didn't exist.  This may entail
 *	calling xx_mmap(), which must exist.
 *
 *	If your xx_fault() function returns SEGDRV_HANDLED, segdrv
 *	assumes that you have mapped the pages in yourself, but still
 *	computes the VM bookkeeping required.  If you fail to map pages
 *	in correctly, they will fault again immediately.  This could
 *	lead to a loop condition, so be careful out there.
 *
 *	seg_drv exports segdrv_faultpage (see below) which you can
 *	use to map in a page.
 *
 *	xx_fault() return values are defined in <vm/faultcode.h>; you may
 *	return one of
 *		FC_HWERR	hardware error
 *		FC_ALIGN	alignment error
 *		FC_NOMAP	no mapping
 *		FC_PROT		protection error
 *		FC_OBJERR	underlying object returned errno value
 *		FC_NOSUPPORT	operation not supported
 *		FC_MAKE_ERR(e)	any errno value.
 *
 *	as well as SEGDRV_CONTINUE, SEGDRV_HANDLED or SEGDRV_IGNORE.
 *
 *
 * faultcode_t
 * xx_faulta(seg, addr)
 *
 *	Asynchronous fault.  Normally no need to provide this function
 *	since it doesn't do anything anyway.
 *
 *
 * int
 * xx_setprot(seg, addr, len, prot)
 *	struct seg	*seg;
 *	caddr_t		addr;
 *	uint_t		len, prot;
 *
 *	User program wishes to change protection (see mprotect(2)).
 *	Segdrv's default behavior is to return EACCESS if the requested
 *	protection is higher than maxprot.  There's rarely any reason
 *	for a device driver to interpose on this function.
 *
 *	If xx_setprot returns SEGDRV_HANDLED, segdrv will not compare the
 *	requested protection with the maximum protection.  hat_unload
 *	and hat_chgprot will not be called.
 *
 *	Note that different address ranges within a segment can have
 *	different protections.
 *
 * int
 * xx_checkprot(seg, addr, len, prot)
 *	struct seg	*seg;
 *	caddr_t		addr;
 *	uint_t		len, prot;
 *
 *	Check protection.  Segdrv's default behavior is to return
 *	EACCES if 'prot' is higher than the minimum protection in the
 *	address range, or 0 of 'prot' is valid.
 *
 *
 * int
 * xx_getprot(seg, addr, len, protv)
 *	struct seg	*seg;
 *	caddr_t		addr;
 *	uint_t		len, *prot;
 *
 *	Return protections of segment.  Segdrv's default behavior
 *	is to return an array of protections, one per page.
 *
 * off_t
 * xx_getoffset(seg, addr)
 *
 *	Segdrv's default behavior is to return the base device offset
 *	of this segment; addr is ignored.  I'm not sure this is the
 *	right behavior, but there you have it.
 *
 *	This function must return a valid offset, it cannot return any
 *	of the SEGDRV_* values.
 *
 * int
 * xx_gettype(seg, addr)
 *
 *	Get map type, MAP_SHARED or MAP_PRIVATE.  Segdrv's default
 *	behavior is to always return the map type of the original
 *	mapping.
 *
 *	This function must return a valid type, it cannot return any
 *	of the SEGDRV_* values.
 *
 * int
 * xx_getvp(seg, addr, vpp)
 *	struct seg	*seg;
 *	caddr_t		addr;
 *	struct vnode	*vpp;
 *
 *	Return vnode pointer.  Pure bookkeeping, no reason to interpose
 *	on this.
 *
 * xx_sync(seg, addr, len, attr, flags)
 * xx_lockop(seg, addr, len, attr, op, lockmap, pos)
 * xx_advise(seg, addr, len, behav)
 * xx_dump(seg)
 *
 *	No-op for devices.  No reason to interpose on these.
 *
 * xx_incore(seg, addr, len, vec)
 *
 *	Return list of 1-byte flags indicating pages are in core.  No
 *	reason to interpose on this.
 */

	/* UTILITY ROUTINES */


/*
 * struct vpage *
 * segdrv_vpage(seg, addr)
 *	struct seg *seg;
 *	caddr_t    addr;
 *
 *	Returns the vpage pointer for the specified page within the segment.
 *	Used with segdrv_faultpage() below.  The address is not
 *	sanity-checked to make sure it's actuall within the segment.
 *
 *
 *
 * int
 * segdrv_loadpages(seg, addr, page, npages)
 *	struct seg *seg;
 *	caddr_t    addr, page;
 *	int	   npages;
 *
 *	Calls hat_devload for each page within the specified region in
 *	the segment with the correct argument.  'addr' is the virtual
 *	address within the segment (not sanity-checked).  'page' is the
 *	kernel virtual start address to be mapped in.  hat_getkpfnum is
 *	called for each page.  segdrv_loadpages() returns an errno.
 *	This function is intended to be called from your xx_fault()
 *	routine.
 *
 *	Note that hat_devload(), and by extension segdrv_loadpages() must
 *	be called with RW_READ_HELD(&seg->s_as->a_lock) true.  This will
 *	always be the case from within xx_fault() but I don't know about
 *	other times.
 *
 *
 *
 *
 * segdrv exports the function segdrv_faultpage for your use in
 * mapping pages:
 *
 * faultcode_t
 * segdrv_faultpage(
 *	struct hat *hat,		the hat
 *	struct seg *seg,		segment of interest
 *	caddr_t addr,			address in as
 *	struct vpage *vpage,		pointer to vpage for seg, addr
 *	enum fault_type type,		type of fault
 *	enum seg_rw rw,			type of access at fault
 *	struct cred *cr)		credentials
 *
 * Use the following:
 *
 *	hat	hat passed to xx_fault or seg->s_as->a_hat
 *	seg	segment to map in
 *	addr	virtual address within segment
 *	vpage	NULL if you know the segment is only one page,
 *		otherwise segdrv_vpage(seg,addr).  If you're going to be
 *		calling this in a loop, page by page, you can just increment
 *		vpage.  See the source to segdrv_fault().
 *	type	type passed to xx_fault() or any other enum fault_type
 *	rw	rw passed to xx_fault() or any other enum seg_rw
 *	cr	CRED()
 */


/*
 * Structure whose pointer is passed to the segdrv_create routine
 */
struct segdrv_crargs {
#if defined(__STDC__)
	int	(*mapfunc)(dev_t dev, off_t off, int prot);
				/* map function to call */
#else
	int	(*mapfunc)();	/* map function to call */
#endif /* __STDC__ */
						/* XXX64 */
	size_t offset;		/* starting offset */
	dev_t	dev;		/* device number */
	uint_t	flags;		/* flags from mmap(2) */
	caddr_t	client;		/* client data */
	struct seg_ops *client_segops;
	int	(*client_create)();
	uchar_t	prot;		/* protection */
	uchar_t	maxprot;	/* maximum protection */
};

/*
 * (Semi) private data maintained by the seg_drv driver per segment mapping
 *
 * The segment lock is necessary to protect fields that are modified
 * when the "read" version of the address space lock is held.  This lock
 * is not needed when the segment operation has the "write" version of
 * the address space lock (it would be redundant).
 *
 * The following fields in segdrv_data are read-only when the address
 * space is "read" locked, and don't require the segment lock:
 *
 *	vp
 *	offset
 *	mapfunc
 *	maxprot
 */
typedef struct	segdrv_data {
	kmutex_t lock;		/* protects segdrv_data */
#if defined(__STDC__)
	int	(*mapfunc)(dev_t dev, off_t off, int prot);
				/* really returns struct pte, not int */
#else
	int	(*mapfunc)();	/* really returns struct pte, not int */
#endif	/* __STDC__ */
	u_offset_t offset;	/* device offset for start of mapping */
	uint_t	flags;		/* flags from mmap(2) */
	struct	vnode *vp;	/* vnode associated with device */
	uchar_t	pageprot;	/* true if per page protections present */
	uchar_t	prot;		/* current segment prot if pageprot == 0 */
	uchar_t	maxprot;	/* maximum segment protections */
	struct vpage *vpage;	/* per-page information, if needed */
	caddr_t	client;		/* device driver private info */
	struct seg_ops *client_segops; /* client segment procs */
	int	(*client_create)();
} Segdrv_Data;


#ifdef _KERNEL

#if defined(__STDC__)

extern int segdrv_create(struct seg *, void *);
extern int segdrv_loadpages(struct seg *, caddr_t, caddr_t, int);
extern struct vpage *segdrv_vpage(struct seg *, caddr_t);

#else

extern int segdrv_create(/* seg, argsp */);
extern int segdrv_loadpages(/* seg, vaddr, paddr, n */);
extern struct vpage *segdrv_vpage(/* seg,addr */);

#endif	/* __STDC__ */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SEG_DRV_H */
