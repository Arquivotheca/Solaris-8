/*
 * Copyright (c) 1992-1994 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Configuration table for standalone I/O system.
 *
 * This file contains the character and block device lists
 * for drivers supported by the boot program. Note that tpboot and copy
 * use a different link order, so that the devsw[] list in
 * the standalone library (libsa.a) supersedes this one.
 *
 * The open boot proms initialize these tables at run time, not statically,
 * so they're just empty slots. Be sure to leave enough room for all
 * possible devices; these really ought to be linked lists, but you know...
 */

#ident "@(#)confufs.c	1.3	96/04/08 SMI"

#include <sys/types.h>

/*
 * Declaration of block device list. This is vaguely analogous
 * to the block device switch bdevsw[] in the kernel's conf.h,
 * except that boot uses a boottab to collect the driver entry points.
 * Like bdevsw[], the bdevlist provides the link between the main unix
 * code and the driver.  The initialization of the device switches is in
 * the file <arch>/conf.c.
 */

struct  bdevlist {
	char	*bl_name;
	struct	boottab	*bl_driver;
	dev_t	bl_root;
};

struct ndevlist {
	char    *nl_name;
	struct  boottab *nl_driver;
	int	nl_root;
};

struct binfo {
	int	ihandle;
	char	*name;
};

/*
 * This table lists all the ufs devices.
 *
 * Beware: in the following table, the entries must appear
 * in the slot corresponding to their major device number.
 * This is because other routines index into this table
 * using the major() of the dev_t returned by getblockdev.
 */
struct bdevlist bdevlist[8];
