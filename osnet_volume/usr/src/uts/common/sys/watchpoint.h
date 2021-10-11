/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_WATCHPOINT_H
#define	_SYS_WATCHPOINT_H

#pragma ident	"@(#)watchpoint.h	1.6	98/01/06 SMI"

#include <sys/types.h>
#include <vm/seg_enum.h>
#include <sys/copyops.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions for the VM implementation of watchpoints.
 * See proc(4) and <sys/procfs.h> for definitions of the user interface.
 */

/*
 * Each process with watchpoints has a linked list of watched areas.
 * The list is kept sorted by user-level virtual address.
 */
struct watched_area {
	struct watched_area *wa_forw;	/* linked list */
	struct watched_area *wa_back;
	caddr_t	wa_vaddr;	/* virtual address of watched area */
	caddr_t	wa_eaddr;	/* virtual address plus size */
	ulong_t	wa_flags;	/* watch type flags (see <sys/procfs.h>) */
};

/*
 * The list of watched areas maps into a list of pages with modified
 * protections.  The list is kept sorted by user-level virtual address.
 */
struct watched_page {
	struct watched_page *wp_forw;	/* linked list */
	struct watched_page *wp_back;
	caddr_t	wp_vaddr;	/* virtual address of this page */
	uchar_t	wp_prot;	/* modified protection bits */
	uchar_t	wp_oprot;	/* original protection bits */
	uchar_t	wp_umap[3];	/* reference counts of user pr_mappage()s */
	uchar_t	wp_kmap[3];	/* reference counts of kernel pr_mappage()s */
	ushort_t wp_flags;	/* see below */
	short	wp_read;	/* number of WA_READ areas in this page */
	short	wp_write;	/* number of WA_WRITE areas in this page */
	short	wp_exec;	/* number of WA_EXEC areas in this page */
};

/* wp_flags */
#define	WP_NOWATCH	0x01	/* protections temporarily restored */
#define	WP_SETPROT	0x02	/* SEGOP_SETPROT() needed on this page */

#ifdef	_KERNEL

struct k_siginfo;
extern	int	pr_mappage(const caddr_t, size_t, enum seg_rw, int);
extern	void	pr_unmappage(const caddr_t, size_t, enum seg_rw, int);
extern	void	setallwatch(void);
extern	int	pr_is_watchpage(caddr_t, enum seg_rw);
extern	int	pr_is_watchpoint(caddr_t *, int *, size_t, size_t *,
			enum seg_rw);
extern	void	do_watch_step(caddr_t, size_t, enum seg_rw, int, greg_t);
extern	int	undo_watch_step(struct k_siginfo *);

extern	struct copyops watch_copyops;

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_WATCHPOINT_H */
