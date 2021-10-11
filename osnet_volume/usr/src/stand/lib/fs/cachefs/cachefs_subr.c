/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident   "@(#)cachefs_subr.c 1.10 96/06/25 SMI"

#include <sys/sysmacros.h>
#include <sys/fcntl.h>
#include <sys/fs/ufs_fs.h>
#include <sys/stat.h>
#include <sys/promif.h>
#include <sys/bootvfs.h>
#include <sys/bootdebug.h>
#include <sys/bootcfs.h>
#include <sys/salib.h>
#include <sys/sacache.h>

extern int boot_cachefs_mountroot(char *bootdev);

int	nfs_has_cfs(char *bootdev_path);
ino_t	get_root_ino(void);
int	get_backfsinfo(char *bpath, char *backfsdev);

static char *ufsname = "ufs";

/*
 * check to see if the NFS filesystem has the Autoclient marker
 * file "/.cachefsinfo"
 * return SUCCESS if found
 * return FAILURE if: 1. root non-readable  2. /.cachefsinfo not found
 */
int
nfs_has_cfs(char *str)
{
	int	fd;

	OPS_DEBUG_CK(("nfs_has_cfs(%s)\n", str));

	if (get_fs_ops_pointer("nfs") == NULL)
		prom_panic("boot: nfs not configured\n");

	set_default_fs("nfs");

	if (mountroot(str) == -1) {
		OPS_DEBUG_CK(("boot: failed to mount root (%s)\n", str));
		return (FAILURE);
	}

	/*
	 * Try and open the /.cachefsinfo file on the back filesystem
	 */
	if ((fd = open(CFS_MONIKER, O_RDONLY)) != -1) {
		(void) close(fd);
		OPS_DEBUG_CK(("boot: (%s) has cachefs", str));
		return (SUCCESS);
	}
	return (FAILURE);
}

/*
 * Check to see if there is a ufs filesystem on the device who's path
 * is passed.  If there is a UFS filesystem, and the Autoclient
 * marker file "/.cachefsinfo" file is present, return that both a ufs
 * filesystem and the Autoclient marker files are present.
 */
int
has_ufs_fs(char *str, int *hascfsp)
{
	int fd;

	*hascfsp = 0;

	set_default_fs("ufs");

	/*
	 * mount ufs
	 * check to see if "/.cachefsinfo" exists
	 */
	if (mountroot(str) == FAILURE) {
		OPS_DEBUG_CK(("has_ufs_fs(): failed to mount root fs\n"));
		return (FAILURE);
	}

	if ((fd = open(CFS_MONIKER, O_RDONLY)) != -1) {
		(void) close(fd);
		*hascfsp = 1;
	}

	OPS_DEBUG_CK(("has_ufs_cfs(%s) no entry\n", CFS_MONIKER));
	return (SUCCESS);
}


/*
 * return the inode number of "/" from the back (nfs) filesystem
 * The back filesystem (nfs) has been mounted at this point
 */
ino_t
get_root_ino(void)
{
	int fd;
	struct stat st;
	char *rootname = "/";

	if ((fd = (*backfs_ops->fsw_open)(rootname, O_RDONLY)) == -1) {
		if (verbosemode)
			printf("ufsboot: get_root_ino: open failed.\n");
		return ((ino_t)0);
	}

	if ((*backfs_ops->fsw_fstat)(fd, &st) == -1) {
		if (verbosemode)
			printf("ufsboot: get_root_ino: stat failed.\n");
		return ((ino_t)0);
	}

	if (verbosemode)
		printf("get_root_ino: Inode number is 0x%x %d\n",
		    st.st_ino, st.st_ino);

	return (st.st_ino);
}

/*
 * Read the cachefs back filesysetem information
 *
 * return SUCCESS if backfs filesystem info is found and is supported.
 * return FAILURE otherwise.
 */
int
get_backfsinfo(char *bpath, char *backfsdev)
{
	static	char	backfstypebuf[MAXPATHLEN];

	if (cachefs_getbfs(ufsname, bpath, backfstypebuf, backfsdev) != 0) {
		OPS_DEBUG_CK(("boot: failed to get backfs information.\n"));
		backfs_fstype = NULL;
		backfsdev[0] = '\0';
		return (FAILURE);
	} else {
		backfs_fstype = backfstypebuf;
		backfs_ops = get_fs_ops_pointer(backfs_fstype);
		if (backfs_ops == NULL) {
			OPS_DEBUG_CK(("boot: backfs type (%s) not supported.\n",
			    backfstypebuf));
			backfs_fstype = NULL;
			backfsdev[0] = '\0';
			return (FAILURE);
		}
	}
	return (SUCCESS);
}

/*
 * Parse the "/.cachefsinfo" file, reading the back fileystem type (nfs)
 * and the back filesystem prom device path
 */
static int
parse_cachefsinfo(int fd, char *backfstypep, char *backfsdev)
{
	char	buf[FSTYPSZ+MAXPATHLEN+2];
	char	*cp, *cp0, *cp1;
	int	ret = SUCCESS;
	int	count;

	bzero(buf, sizeof (buf));
	count = (*frontfs_ops->fsw_read)(fd, buf, sizeof (buf));
	if (count <= 0) {
		printf("parse_cachefsinfo(): read failed\n");
		return (FAILURE);
	}

	cp0 = buf;
	do {
		if ((cp = strchr(cp0, '=')) == NULL) {
			ret = FAILURE;
			break;
		}
		*cp++ = '\0';
		if ((cp1 = strchr(cp, '\n')) == NULL) {
			ret = FAILURE;
			break;
		}

		*cp1++ = '\0';
		if (strcmp(cp0, "backfs") == 0)
			(void) strcpy(backfstypep, cp);
		else if (strcmp(cp0, "backfsdev") == 0)
			(void) strcpy(backfsdev, cp);
		else {
			ret = FAILURE;
			break;
		}
		cp0 = cp1;
	} while (count > cp1 - buf);
	if (ret != SUCCESS) {
		printf("boot:parse_cachefsinfo(): <%s> illegal format\n",
				CFS_FSINFO);
	}

	return (ret);
}

/*
 * This routine will try to mount a CacheFS/UFS filesystem
 * on the boot device read the backfs information.
 */
int
cachefs_getbfs(char *frontfstype, char *root, char *backfstypep,
	char *backfsdev)
{
	int	ret = SUCCESS;
	int	fd;

	frontfs_ops = get_fs_ops_pointer(frontfstype);
	if (frontfs_ops == NULL) {
		printf("boot: frontfs type (%s) not supported!\n", frontfstype);
		return (FAILURE);
	}

	if ((ret = boot_cachefs_mountroot(root)) != 0) {
		printf("cachefs_getbfs(): cachefs_mountroot(%s) failed\n",
			root);
		return (FAILURE);
	}

	/*
	 * Open /.cachefsinfo and parse the back filesystem type
	 * and back filesystem device (should be nfs)
	 */
	if ((fd = (*frontfs_ops->fsw_open)(CFS_FSINFO, O_RDONLY)) == -1) {
		printf("cachefs_getbfs(): open(%s) failed\n", CFS_FSINFO);
		return (FAILURE);
	}

	ret = parse_cachefsinfo(fd, backfstypep, backfsdev);

	if (ret != SUCCESS) {
		(void) (*frontfs_ops->fsw_close)(fd);
		printf("cachefs_getbfs(): parse (%s) failed\n", CFS_FSINFO);
		return (FAILURE);
	}

	OPS_DEBUG_CK(("cachefs_getbfs(): backfstype=%s\n", backfstypep));
	OPS_DEBUG_CK(("cachefs_getbfs(): backfsdev=%s\n", backfsdev));

	if ((ret = (*frontfs_ops->fsw_close)(fd)) == -1) {
		printf("cachefs_getbfs(): close(%d) failed\n", CFS_FSINFO);
		return (FAILURE);
	}
	return (SUCCESS);
}
