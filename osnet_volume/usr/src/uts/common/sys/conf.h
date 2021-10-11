/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CONF_H
#define	_SYS_CONF_H

#pragma ident	"@(#)conf.h	1.59	99/05/26 SMI"	/* SVr4.0 11.21	*/

#include <sys/feature_tests.h>

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#include <sys/t_lock.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#define	FMNAMESZ	8 		/* used by struct fmodsw */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

#ifdef _KERNEL

/*
 * XXX  Given that drivers need to include this file,
 *	<sys/systm.h> probably shouldn't be here, as
 *	it legitimizes (aka provides prototypes for)
 *	all sorts of functions that aren't in the DKI/SunDDI
 */
#include <sys/systm.h>
#include <sys/devops.h>
#include <sys/model.h>
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <vm/as.h>

extern struct dev_ops **devopsp;

/*
 * Return streams information for the driver specified by major number or
 *   NULL if device cb_ops structure is not present.
 */
#define	STREAMSTAB(maj)	(devopsp[(maj)] == NULL ? NULL : \
	(devopsp[(maj)]->devo_cb_ops == NULL ? \
	NULL : \
	devopsp[(maj)]->devo_cb_ops->cb_str))

extern int devi_identify(dev_info_t *);
extern int devi_probe(dev_info_t *);
extern int devi_attach(dev_info_t *, ddi_attach_cmd_t);
extern int devi_detach(dev_info_t *, ddi_detach_cmd_t);
extern int devi_reset(dev_info_t *, ddi_reset_cmd_t);

extern int dev_open(dev_t *, int, int, cred_t *);
extern int dev_close(dev_t, int, int, cred_t *);

extern dev_info_t *dev_get_dev_info(dev_t, int);
extern int dev_to_instance(dev_t);

extern int bdev_strategy(struct buf *);
extern int bdev_print(dev_t, caddr_t);
extern int bdev_dump(dev_t, caddr_t, daddr_t, int);
extern int bdev_size(dev_t);
extern uint64_t bdev_Size(dev_t);

extern int cdev_read(dev_t, struct uio *, cred_t *);
extern int cdev_write(dev_t, struct uio *, cred_t *);
extern int cdev_size(dev_t);
extern uint64_t cdev_Size(dev_t);
extern int cdev_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
extern int cdev_devmap(dev_t dev, devmap_cookie_t dhp, offset_t off,
	size_t len, size_t *maplen, uint_t model);
extern int cdev_mmap(int (*)(dev_t, off_t, int),
    dev_t, off_t, int);
extern int cdev_segmap(dev_t, off_t, struct as *, caddr_t *,
    off_t, uint_t, uint_t, uint_t, cred_t *);
extern int cdev_poll(dev_t, short, int, short *, struct pollhead **);
extern int cdev_prop_op(dev_t, dev_info_t *, ddi_prop_op_t,
    int, char *, caddr_t, int *);

#endif /* _KERNEL */


/*
 * Device flags.
 *
 * Bit 0 to bit 15 are reserved for kernel.
 * Bit 16 to bit 31 are reserved for different machines.
 */

#define	D_NEW		0x00	/* new-style driver */
#define	_D_OLD		0x01	/* old-style driver (obsolete) */
#define	D_TAPE		0x08	/* Magtape device (no bdwrite when cooked) */

/*
 * MT-safety level (in DDI portion of flags).
 *
 * All drivers must be MT-safe, and must advertise this by specifying D_MP.
 *
 * The remainder of the flags apply only to STREAMS modules and drivers.
 *
 * A STREAMS driver or module can optionally select inner and outer perimeters.
 * The four mutually exclusive options that define the presence and scope
 * of the inner perimeter are:
 *	D_MTPERMOD - per module single threaded.
 *	D_MTQPAIR - per queue-pair single threaded.
 *	D_MTPERQ - per queue instance single threaded.
 *	(none of the above) - no inner perimeter restricting concurrency
 *
 * The presence	of the outer perimeter is declared with:
 *	D_MTOUTPERIM - a per-module outer perimeter. Can be combined with
 *		D_MTPERQ, D_MTQPAIR, and D_MP.
 *
 * The concurrency when entering the different STREAMS entry points can be
 * modified with:
 *	D_MTPUTSHARED - modifier for D_MTPERQ, D_MTQPAIR, and D_MTPERMOD
 *		specifying that the put procedures should not be
 *		single-threaded at the inner perimeter.
 *	_D_MTOCSHARED - EXPERIMENTAL - will be removed in a future release.
 *		Modifier for D_MTPERQ, D_MTQPAIR, and D_MTPERMOD
 *		specifying that the open and close procedures should not be
 *		single-threaded at the inner perimeter.
 *	_D_MTCBSHARED - EXPERIMENTAL - will be removed in a future release.
 *		Modifier for D_MTPERQ, D_MTQPAIR, and D_MTPERMOD
 *		specifying that the callback i.e qtimeout() procedures should
 *		not be single-threaded at the inner perimeter.
 *	D_MTOCEXCL - modifier for D_MTOUTPERIM specifying that the open and
 *		close procedures should be single-threaded at the outer
 *		perimeter.
 *	_D_QNEXTLESS - EXPERIMENTAL - will be removed in a future release.
 *		It means that the driver or module accesses its read side
 * 		q_next only thru the *next* versions canputnext/putnext etc
 * 		and not thru canput(). For drivers, there are additional
 *		requirements where it needs to cut of all its sources of
 *		threads (like interrupts) that might issue calls (eg. put,
 *		putnext, qwriter) on the closing queue before calling
 *		qprocsoff.
 */
#define	D_MTSAFE	0x0020	/* multi-threaded module or driver */
#define	_D_QNEXTLESS	0x0040	/* q_next is not accessed without a lock */
#define	_D_MTOCSHARED	0x0080	/* modify: open/close procedures are hot */
/* 0x100 - see below */
/* 0x200 - see below */
/* 0x400 - see below */
#define	D_MTOCEXCL	0x0800	/* modify: open/close are exclusive at outer */
#define	D_MTPUTSHARED	0x1000	/* modify: put procedures are hot */
#define	D_MTPERQ	0x2000	/* per queue instance single-threaded */
#define	D_MTQPAIR	0x4000	/* per queue-pair instance single-threaded */
#define	D_MTPERMOD	0x6000	/* per module single-threaded */
#define	D_MTOUTPERIM	0x8000	/* r/w outer perimeter around whole modules */
#define	_D_MTCBSHARED	0x10000	/* modify : callback procedures are hot */

/* The inner perimeter scope bits */
#define	D_MTINNER_MASK	(D_MP|D_MTPERQ|D_MTQPAIR|D_MTPERMOD)

/* Inner perimeter modification bits */
#define	D_MTINNER_MOD	(D_MTPUTSHARED|_D_MTOCSHARED|_D_MTCBSHARED)

/* Outer perimeter modification bits */
#define	D_MTOUTER_MOD	(D_MTOCEXCL)

/* All the MT flags */
#define	D_MTSAFETY_MASK (D_MTINNER_MASK|D_MTOUTPERIM|D_MTPUTSHARED|\
			D_MTINNER_MOD|D_MTOUTER_MOD)

#define	D_MP		D_MTSAFE /* ddi/dki approved flag */

#define	D_64BIT		0x200	/* Driver supports 64-bit offsets, blk nos. */

#define	D_SYNCSTR	0x400	/* Module or driver has Synchronous STREAMS */
				/* extended qinit structure */

#define	D_DEVMAP	0x100	/* Use devmap framework to mmap device */

#define	D_HOTPLUG	0x4	/* Driver is hotplug capable */

/*
 * fmodsw_impl_t is used within the kernel. fmodsw is used by
 * the modules/drivers. The information is copied from fmodsw
 * defined in the module/driver into the fmodsw_impl_t array
 * (defined in str_conf.c) during the module/driver initialization.
 */
typedef struct fmodsw_impl {
	char		f_name[FMNAMESZ+1];
	struct  streamtab *f_str;
	int		f_flag;
	kmutex_t	*f_lock;
	uint_t 		f_count;
}fmodsw_impl_t;

typedef struct fmodsw {
	char		f_name[FMNAMESZ+1];
	struct  streamtab *f_str;
	int		f_flag;
}fmodsw_t;

#ifdef _KERNEL

extern int allocate_fmodsw(char *);
extern void free_fmodsw(fmodsw_impl_t *);
extern int findmod(char *);
extern int findfmodbyindex(char *);
extern int fmod_hold(int);
extern int fmod_release(int);
#define	STATIC_STREAM		(kmutex_t *)0xffffffff
#define	LOADABLE_STREAM(s)	((s)->f_lock != STATIC_STREAM)
#define	ALLOCATED_STREAM(s)	((s)->f_lock != NULL)
#define	STREAM_INSTALLED(s)	((s)->f_str != NULL)

extern kmutex_t  fmodsw_lock;

extern fmodsw_impl_t fmodsw[];

extern int	devcnt;
extern int	fmodcnt;
#endif /* _KERNEL */

/*
 * Line discipline switch.
 */
struct linesw {
	int	(*l_open)();
	int	(*l_close)();
	int	(*l_read)();
	int	(*l_write)();
	int	(*l_ioctl)();
	int	(*l_input)();
	int	(*l_output)();
	int	(*l_mdmint)();
};

#ifdef _KERNEL
extern struct linesw linesw[];

extern int	linecnt;
#endif /* _KERNEL */
/*
 * Terminal switch
 */

struct termsw {
	int	(*t_input)();
	int	(*t_output)();
	int	(*t_ioctl)();
};

#ifdef _KERNEL
extern struct termsw termsw[];

extern int	termcnt;
#endif /* _KERNEL */

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CONF_H */
