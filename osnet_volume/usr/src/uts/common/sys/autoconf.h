/*
 * Copyright (c) 1989-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_AUTOCONF_H
#define	_SYS_AUTOCONF_H

#pragma ident	"@(#)autoconf.h	1.31	99/06/04 SMI"

/* Derived from autoconf.h, SunOS 4.1.1 1.15 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This defines a parallel structure to the devops list.
 */

#include <sys/dditypes.h>
#include <sys/devops.h>
#include <sys/mutex.h>
#include <sys/thread.h>

typedef struct mta_handle mta_handle_t;	/* MT Attach handle */

struct devnames {
	char    *dn_name;		/* Name of this driver */
	int	dn_flags;		/* per-driver flags, see below */
	struct par_list *dn_pl;		/* parent list, for making devinfos */
	kmutex_t dn_lock;		/* Per driver lock (see below) */
	kcondvar_t dn_wait;		/* busy/waiting condition variable. */
	int	dn_circular;		/* Circular driver dependency count */
	kthread_id_t dn_busy_thread;	/* Thread holding busy/waiting */
	dev_info_t *dn_head;		/* Head of instance list */
	int	dn_instance;		/* Next instance no. to assign */
	void	*dn_inlist;		/* instance # nodes for this driver */
	ddi_prop_t *dn_global_prop_ptr;	/* per-driver global properties */
	mta_handle_t *dn_mta;		/* used for multi-threaded attach */
};

/*
 * dn_lock is used to protect the driver initialization/attach/loading
 * from fini/detach/unloading and also protects each drivers devops
 * reference count, the dn_head linked list of driver instances and the
 * dn_newdevs linked list of new hotplugged device nodes to be attached.
 * It protects the flags, the busy_changing counts and is used when
 * waiting for the per-driver conditon variable, when the driver is already
 * busy/changing.
 */

/*
 * Defines for flags.
 */

#define	DN_CONF_PARSED		0x1
#define	DN_DEVI_MADE		0x2
#define	DN_WALKED_TREE		0x4
#define	DN_DEVS_ATTACHED	0x8

#define	DN_BUSY_LOADING		0x10
#define	DN_BUSY_UNLOADING	0x20
#define	DN_TAKEN_GETUDEV	0x40	/* getudev() used this entry */
#define	DN_BUSY_CHANGING_BITS	0x30
#define	DN_BUSY_CHANGING(flg)	(((flg) & DN_BUSY_CHANGING_BITS) != 0)

#ifdef _KERNEL

extern struct devnames *devnamesp;
extern struct devnames orphanlist;

extern char *nullstr;
extern struct dev_ops nodev_ops, mod_nodev_ops;
extern krwlock_t devinfo_tree_lock;
extern dev_info_t *top_devinfo;

/*
 * Acquires dn_lock, as above.
 */
#define	LOCK_DEV_OPS(lp)	mutex_enter((lp))
#define	UNLOCK_DEV_OPS(lp)	mutex_exit((lp))

/*
 * Not to be used without obtaining the per-driver lock.
 */
#define	INCR_DEV_OPS_REF(opsp)	(opsp)->devo_refcnt++
#define	DECR_DEV_OPS_REF(opsp)	(opsp)->devo_refcnt--
#define	CB_DRV_INSTALLED(opsp)	((opsp) != &nodev_ops && \
				(opsp) != &mod_nodev_ops)
#define	DRV_UNLOADABLE(opsp)	((opsp)->devo_refcnt == 0)
#define	DEV_OPS_HELD(opsp)	((opsp)->devo_refcnt > 0)
#define	NEXUS_DRV(opsp)		((opsp)->devo_bus_ops != NULL)

extern int impl_proto_to_cf2(dev_info_t *);
extern void impl_rem_dev_props(dev_info_t *);
extern void impl_rem_hw_props(dev_info_t *);
extern void impl_add_dev_props(dev_info_t *);
extern void copy_prop(ddi_prop_t *, ddi_prop_t **);
extern void add_class(char *, char *);

struct bind;
extern int make_mbind(char *, int, char *, struct bind **);
extern void delete_mbind(char *, struct bind **);

extern void configure(void);
extern void setcputype(void);
extern void reset_leaves(void);

extern int impl_initnode(dev_info_t *);
extern int impl_initdev(dev_info_t *);

extern void setup_ddi(void);
extern void impl_ddi_callback_init(void);
extern void impl_fix_props(dev_info_t *, dev_info_t *, char *, int, caddr_t);
extern int impl_check_cpu(dev_info_t *);
extern int check_status(int, char *, dev_info_t *);

extern int exclude_settrap(int);
extern int exclude_level(int);

extern void attach_driver_to_hw_nodes(major_t, struct dev_ops *);
extern int impl_make_devinfos(major_t);

extern char *i_path_to_drv(char *);

/*
 * Routines for multi-threaded probe/attach
 */

extern mta_handle_t *mta_get_handle(major_t);
extern void mta_add_dip(mta_handle_t *, dev_info_t *);
extern void mta_attach_devi_list(mta_handle_t *);

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_AUTOCONF_H */
