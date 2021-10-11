/*
 * Copyright (c) 1987-1990 by Sun Microsystems, Inc.
 */

#ident	"@(#)conf.c	5.96	97/10/22 SMI"	/* from SunOS4.0 5.63	*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/acct.h>
#include <sys/stream.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

/*
 * Might need to define no operation routines attach(), detach(),
 * reset(), probe(), identify() and get_dev_info().
 */

extern int nopropop();

struct cb_ops no_cb_ops = {
	nodev,		/* open		*/
	nodev,		/* close 	*/
	nodev,		/* strategy	*/
	nodev,		/* print	*/
	nodev,		/* dump		*/
	nodev,		/* read		*/
	nodev,		/* write	*/
	nodev,		/* ioctl	*/
	nodev,		/* devmap	*/
	nodev,		/* mmap		*/
	nodev,		/* segmap	*/
	nochpoll,	/* chpoll	*/
	nopropop,	/* cb_prop_op	*/
	0,		/* stream tab	*/
	D_NEW | D_MP	/* char/blk driver compatibility flag */
};

struct dev_ops nodev_ops = {
	DEVO_REV,		/* devo_rev	*/
	0,			/* refcnt	*/
	ddi_no_info,		/* info		*/
	nulldev,		/* identify	*/
	nulldev,		/* probe	*/
	ddifail,		/* attach	*/
	nodev,			/* detach	*/
	nulldev,		/* reset	*/
	&no_cb_ops,		/* character/block driver operations */
	(struct bus_ops *)0	/* bus operations for nexus drivers */
};

struct dev_ops	**devopsp;

int	devcnt;
