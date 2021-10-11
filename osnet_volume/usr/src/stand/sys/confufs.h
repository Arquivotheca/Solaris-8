/*
 * Copyright (c) 1988 Sun Microsystems, Inc.
 */

#ident	"@(#)confufs.h	1.3	92/07/14 SMI" /* from SunOS 4.1 */ 

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
        int     nl_root;
};

struct binfo {
        int     ihandle;
        char    *name;
};
