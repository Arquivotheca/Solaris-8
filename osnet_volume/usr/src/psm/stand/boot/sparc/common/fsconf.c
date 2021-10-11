/*
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */
#pragma ident	"@(#)fsconf.c	1.10	97/03/27 SMI"

#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/obpdefs.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/bootcfs.h>
#include <sys/bootdebug.h>
#include <sys/promif.h>
#include <sys/salib.h>

extern int verbosemode;

/*
 * filesystem switch table, UFS, NFS, and CACHEFS
 */
extern struct boot_fs_ops boot_cachefs_ops;
extern struct boot_fs_ops boot_ufs_ops;
extern struct boot_fs_ops boot_nfs_ops;
struct boot_fs_ops *boot_fsw[] = {
	&boot_cachefs_ops,
	&boot_ufs_ops,
	&boot_nfs_ops,
};

extern void translate_v2tov0(char *bkdev, char *npath);
static int is_netboot(char *v2path);

int boot_nfsw = sizeof (boot_fsw) / sizeof (boot_fsw[0]);
int nfs_readsize = 0;

static char *ufsname = "ufs";
static char *nfsname = "nfs";
static char *cfsname = "cachefs";

/*
 * Cachefs data
 * backfs data describes the back filesystem (i.e. net disk)
 */
static char backfsdev[OBP_MAXPATHLEN];
static char frontfsdev[OBP_MAXPATHLEN];
char *backfs_dev = backfsdev;
char *backfs_fstype = NULL;
char *frontfs_dev = frontfsdev;
char *frontfs_fstype = NULL;

/*
 * set the boot filesystem ops
 * takes both the translated (v2path) and raw boot path (bpath)
 * returns the filesystem name selected to boot (and to the kernel
 */
char *
set_fstype(char *v2path, char *bpath)
{
	char netpath[OBP_MAXPATHLEN];
	char *fstype = NULL;
	int  hascfs = 0;		/* ufs has cachefs filesystem */

	/*
	 * if we are booting over the net, we may be booting either
	 * a Diskless client or a Cache-Only-Client. Prepare for
	 * either case by setting the backfs properties from the
	 * boot device
	 * Cache-Only-Clients change rootfs to "cachefs" in /etc/system,
	 */
	if (is_netboot(v2path)) {
		fstype = nfsname;
		backfs_fstype = nfsname;
		(void) strcpy(backfsdev, v2path);
		frontfs_fstype = "";
		frontfs_dev = "";
	} else {
		/*
		 * booted off a local disk
		 * if UFS, and the /.cachefsinfo file is here, this
		 * is a cachefs boot.  Read the backfs fstype and
		 * device path from the /.cachefsinfo file.
		 */
		if (has_ufs_fs(bpath, &hascfs) == SUCCESS) {
			fstype = ufsname;
			if (hascfs == 1) {
				fstype = cfsname;
				frontfs_fstype = ufsname;
				(void) strcpy(frontfsdev, v2path);
				/*
				 * if this file gets clobbered, we need
				 * to reinitialize the Autoclient cache
				 */
				if (get_backfsinfo(bpath, backfsdev) != SUCCESS)
					prom_panic("ufsboot: Corrupted"
					" cachefs info file\n"
					"Please reinitialize with:"
					"\t\"boot net -f\"\n");
			}
		} else {
			prom_panic("ufsboot: cannot determine filesystem type "
				" of root device.");
		}
	}

	if (hascfs == 0) {
		/* a 'regular' NFS or UFS boot */
		set_default_fs(fstype);
	} else {
		/*
		 * CFS boot from frontfs.
		 * Translate the backfs device pathname to a form
		 * old Proms expect, i.e. le(0,0,0), and mount the
		 * back filesytem
		 */
		if (verbosemode) {
			printf("ufsboot: cachefs\n");
			printf("\tfrontfs device=%s\n", frontfsdev);
			printf("\tbackfs device=%s\n", backfsdev);
		}
		v2path = "";
		set_default_fs(backfs_fstype);
		translate_v2tov0(backfsdev, netpath);

		/*
		 * mount the back filesystem for cache miss processing
		 */
		if (mountroot(netpath) != SUCCESS)
			prom_panic("ufsboot: Cannot not mount backfs"
				" filesystem.\n");

		/*
		 * save the front/back FS ops for cachefs and set
		 * return "cachefs" as the root fs type
		 */
		frontfs_ops = get_fs_ops_pointer(frontfs_fstype);
		backfs_ops = get_fs_ops_pointer(backfs_fstype);
		set_default_fs(cfsname);
	}
	return (fstype);
}

/*
 * determine if the device we booted from is a network device
 * old proms require that we check for specific device names in the
 * boot path
 */
static int
is_netboot(char *v2path)
{
	int node;

	/*
	 * If we have an old prom, search the boot path for network
	 * device names.
	 */
	if (prom_getversion() == 0) {	/* sun4c only */
		if (strstr(v2path, "le@") != NULL)
			return (1);
	} else {
		node = prom_finddevice(prom_bootpath());
		if (prom_devicetype(node, "network"))
			return (1);
	}
	return (0);
}
