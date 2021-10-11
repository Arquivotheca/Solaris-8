/*
 * Copyright (c) 1994-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)diskread.c	1.8	99/10/04 SMI"

#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/fs/ufs_fsdir.h>
#include <sys/fs/ufs_fs.h>
#include <sys/fs/ufs_inode.h>
#include <sys/sysmacros.h>
#include <sys/promif.h>
#include <sys/filep.h>
#include <sys/salib.h>

#ifdef sparc
static	char	prom_dev_type = -1;
#else
static	char	prom_dev_type = 0;
#endif

/*
 * unix root slice offset for PROMS that do
 * not know about fdisk partitions or Solaris
 * slices.
 * the default is 0 for machines with proms that
 * do know how to interpret solaris slices.
 */
unsigned long unix_startblk = 0;

/*
 * Exported Functions
 */
extern	int	diskread(fileid_t *filep);

/*
 *	The various flavors of PROM make this grotesque.
 */
int
diskread(fileid_t *filep)
{
	int err;
	devid_t	*devp;
	uint_t blocknum;

	/* add in offset of root slice */
	blocknum = filep->fi_blocknum + unix_startblk;

#ifdef sparc
	if (prom_dev_type == -1) {
		if (prom_getversion() == 0)
			prom_dev_type = BLOCK;
		else
			prom_dev_type = 0;
	}
#endif

	devp = filep->fi_devp;

	if ((err = prom_seek(devp->di_dcookie,
	    (unsigned long long)blocknum *
	    (unsigned long long)DEV_BSIZE)) == -1) {
#ifdef lint
		err = err;	/* needed for non-sparc versions */
#endif

#ifdef sparc
		if (prom_getversion() > 0) {
			printf("Seek error at block %x\n", blocknum);
			return (-1);
		}
		/*
		 * V0 proms will return -1 here.  That's ok since
		 * they don't really support seeks on block devices.
		 */
#endif
	}
	if ((err = prom_read(devp->di_dcookie, filep->fi_memp,
	    filep->fi_count, blocknum, prom_dev_type)) !=
	    filep->fi_count) {
#ifdef sparc
		if (prom_getversion() == 0) {
			if (err != filep->fi_count/DEV_BSIZE) {
				printf("Short read.  0x%x chars read\n",
					filep->fi_count);
				return (-1);
			}
		} else {
#endif
			printf("Short read.  0x%x chars read\n",
				filep->fi_count);
				return (-1);
#ifdef sparc
		}
#endif
	}

	return (0);
}
