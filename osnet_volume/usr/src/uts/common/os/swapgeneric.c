/* ONC_PLUS EXTRACT START */
/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)swapgeneric.c	5.119	99/07/08 SMI"
/* ONC_PLUS EXTRACT END */

/*
 * Configure root, swap and dump devices.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/vm.h>
#include <sys/reboot.h>
#include <sys/file.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/open.h>
#include <sys/mount.h>
#include <sys/kobj.h>
#include <sys/bootconf.h>
#include <sys/sysconf.h>
#include <sys/modctl.h>
#include <sys/autoconf.h>
#include <sys/debug.h>
#include <sys/fs/snode.h>
#include <fs/fs_subr.h>
#include <sys/socket.h>
#include <net/if.h>

#include <sys/mkdev.h>
#include <sys/cmn_err.h>

#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/hwconf.h>
#include <sys/dc_ki.h>

/*
 * External references
 */
extern int ddi_load_driver(char *);

/*
 * Local routines
 */
static struct vfssw *getfstype(char *, char *);
static int getphysdev(char *askfor, char *passedname);
static char *getlastfrompath(char *physdevpath);

#define	DEBUG_SWAPGENERIC	1
#ifdef DEBUG_SWAPGENERIC
static int debug_sg = 0;
#define	DPRINTF(x)	if (debug_sg) printf x;
#else
#define	DPRINTF(x)	/* thin air */
#endif /* DEBUG_SWAPGENERIC */

/*
 * Module linkage information for the kernel.
 */
static struct modlmisc modlmisc = {
	&mod_miscops, "root and swap configuration 5.119"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Configure root file system.
 */
int
rootconf(void)
{
	int error;
	struct vfssw *vsw;

	DPRINTF(("rootconf: rootfs.bo_fstype %s\n", rootfs.bo_fstype));

	/*
	 * Ick.  The first time we called vfs_getvfssw() that
	 * will have used LOCK_VFS() on the vfssw_lock mutex.
	 * However, by the time we get here, vfs_mountroot() will
	 * have init'ed the mutex, and hence the following assertion.
	 */
	ASSERT(!VFSSW_LOCKED());

	/*
	 * Install cluster modules that were only loaded during
	 * loadrootmodules().
	 */
	if (error = clboot_rootconf())
		return (error);

	/*
	 * Run _init on the root filesystem (we already loaded it
	 * but we've been waiting until now to _init it) which will
	 * have the side-effect of running vsw_init() on this vfs.
	 * Because all the nfs filesystems are lumped into one
	 * module we need to special case it.
	 */
	if (strncmp(rootfs.bo_fstype, "nfs", 3) == 0) {
		if (modload("fs", "nfs") == -1) {
			RLOCK_VFSSW();
			cmn_err(CE_CONT, "Cannot initialize %s filesystem\n",
			    rootfs.bo_fstype);
			return (ENXIO);
		}
	} else {
		if (modload("fs", rootfs.bo_fstype) == -1) {
			RLOCK_VFSSW();
			cmn_err(CE_CONT, "Cannot initialize %s filesystem\n",
			    rootfs.bo_fstype);
			return (ENXIO);
		}
	}
	if (backfs.bo_fstype[0] != '\0' &&
	    modload("fs", backfs.bo_fstype) == -1) {
		RLOCK_VFSSW();
		cmn_err(CE_CONT, "Cannot initialize %s backfs\n",
		    backfs.bo_fstype);
		return (ENXIO);
	}
	if (frontfs.bo_fstype[0] != '\0' &&
	    modload("fs", frontfs.bo_fstype) == -1) {
		RLOCK_VFSSW();
		cmn_err(CE_CONT, "Cannot initialize %s frontfs\n",
		    frontfs.bo_fstype);
		return (ENXIO);
	}
	RLOCK_VFSSW();
	vsw = vfs_getvfsswbyname(rootfs.bo_fstype);
	VFS_INIT(rootvfs, vsw->vsw_vfsops, (caddr_t)0);
	VFS_HOLD(rootvfs);

	if (netboot)
		strplumb();

	/*
	 * ufs_mountroot() ends up calling getrootdev()
	 * (below) which actually triggers the _init, identify,
	 * probe and attach of the drivers that make up root device
	 * bush; these are also quietly waiting in memory.
	 */
	DPRINTF(("rootconf: calling VFS_MOUNTROOT %s\n", rootfs.bo_fstype));
	error = VFS_MOUNTROOT(rootvfs, ROOT_INIT);
	RUNLOCK_VFSSW();
	rootdev = rootvfs->vfs_dev;

	if (error)
		cmn_err(CE_CONT, "Cannot mount root on %s fstype %s\n",
		    rootfs.bo_name, rootfs.bo_fstype);
	else
		cmn_err(CE_CONT, "?root on %s fstype %s\n",
		    rootfs.bo_name, rootfs.bo_fstype);
	return (error);
}

/*
 * Under the assumption that our root file system is on a
 * disk partition, get the dev_t of the partition in question.
 *
 * By now, boot has faithfully loaded all our modules into memory, and
 * we've taken over resource management.  Before we go any further, we
 * have to fire up the device drivers and stuff we need to mount the
 * root filesystem.  That's what we do here.  Fingers crossed.
 */
dev_t
getrootdev(void)
{
	dev_t	d;

	if ((d = ddi_pathname_to_dev_t(rootfs.bo_name)) == NODEV)
		cmn_err(CE_CONT, "Cannot assemble drivers for root %s\n",
		    rootfs.bo_name);
	return (d);
}

/*
 * Under the assumption that we're swapping to a disk partition, get the
 * major/minor of the partition in question.
 */
dev_t
getswapdev(char *name)
{
	struct vnodeops *spec_getvnodeops(void);
	dev_t dev;
	static char cannot_assemble[] =
	    "Cannot assemble drivers for swap %s\n";

	if (*name == '\0') {
		/*
		 * No swap name specified, use root dev partition "b"
		 * if it is a block device, otherwise fail.
		 */
		if (rootvp->v_op == spec_getvnodeops() &&
		    (boothowto & RB_ASKNAME) == 0) {
			(void) strcpy(name, rootfs.bo_name);
			name[strlen(name) - 1] = 'b';
			if ((dev = ddi_pathname_to_dev_t(name)) == NODEV) {
				cmn_err(CE_CONT, cannot_assemble, name);
				return (NODEV);
			}
		} else {
retry:
			if (getphysdev("swap", name) != 0 ||
			    ((dev = ddi_pathname_to_dev_t(name)) == NODEV)) {
				cmn_err(CE_CONT, cannot_assemble, name);
				return (NODEV);
			}

			/*
			 * Check for swap on root device
			 */
			if (rootvp->v_op == spec_getvnodeops() &&
			    dev == rootvp->v_rdev) {
				char resp[128];

				printf("Swapping on root device, ok? ");
				gets(resp);
				if (*resp != 'y' && *resp != 'Y')
					goto retry;
			}
		}
	} else {
		if (getphysdev("swap", name) != 0 ||
		    ((dev = ddi_pathname_to_dev_t(name)) == NODEV)) {
			cmn_err(CE_CONT, cannot_assemble, name);
			return (NODEV);
		}
	}

	DPRINTF(("swapping on (%d, %d)\n",
	    (int)getmajor(dev), (int)getminor(dev)));

	return (dev);
}

/*
 * If booted with ASKNAME, prompt on the console for a filesystem
 * name and return it.
 */
void
getfsname(char *askfor, char *name)
{
	if (boothowto & RB_ASKNAME) {
		printf("%s name: ", askfor);
		gets(name);
	}
}

/*ARGSUSED1*/
static int
preload_module(struct sysparam *sysp, void *p)
{
	static char *wmesg = "forceload of %s failed";
	char *name;

	name = sysp->sys_ptr;
	if (strncmp(sysp->sys_ptr, "drv", 3) == 0) {
		if (loaddrv_hierarchy(name+4, (major_t)0) != 0)
			cmn_err(CE_WARN, wmesg, name);
		return (0);
	}

	if (modloadonly(NULL, name) < 0)
		cmn_err(CE_WARN, wmesg, name);
	return (0);
}

/* ONC_PLUS EXTRACT START */
/*
 * We want to load all the modules needed to mount the root filesystem,
 * so that when we start the ball rolling in 'getrootdev', every module
 * should already be in memory, just waiting to be init-ed.
 */
int
loadrootmodules(void)
{
	struct vfssw	*vsw;
	char		*this;
	char 		*name;
	int		err;
/* ONC_PLUS EXTRACT END */
	int		i;
	extern char	*impl_module_list[];
	extern char	*platform_pm_module_list[];

	/* Make sure that the PROM's devinfo tree has been created */
	ASSERT(ddi_root_node());

	DPRINTF(("loadrootmodules: rootfs.bo_fstype %s\n", rootfs.bo_fstype));
	DPRINTF(("loadrootmodules: rootfs.bo_name %s\n", rootfs.bo_name));
	DPRINTF(("loadrootmodules: rootfs.bo_flags %x\n", rootfs.bo_flags));

	/*
	 * zzz We need to honor what's in rootfs if it's not null.
	 * non-null means use what's there.  This way we can
	 * change rootfs with /etc/system AND with tunetool.
	 */
	if ((rootfs.bo_flags & BO_VALID) == 0) {
		/*
		 * Get the root fstype and root device path from boot.
		 */
		rootfs.bo_fstype[0] = '\0';
		rootfs.bo_name[0] = '\0';
	}

	/*
	 * This lookup will result in modloadonly-ing the root
	 * filesystem module - it gets _init-ed in rootconf()
	 */
	if ((vsw = getfstype("root", rootfs.bo_fstype)) == NULL) {
		return (ENXIO);	/* in case we have no file system types */
	}

	(void) strcpy(rootfs.bo_fstype, vsw->vsw_name);

	if (strcmp(rootfs.bo_fstype, "cachefs") == 0) {

#ifdef i86
	/*
	 * jhd 6/20/96
	 * The x86 booter lacks the ability to return the cachefs
	 * properties.  For now, read the /.cachefsbackinfo file here.
	 */
		struct bootobj *bop;
		struct _buf *fp;
		char *cp;

		/*
		 * Root is cachefs.  Look for the /.cachefsbackinfo file.
		 * if one exists, read the first two lines to glean the
		 * backfs file system type and device, respectively, and
		 * set frontfs from the boot prom.  If the file doesn't
		 * exist, then this must be a net boot, so we set backfs
		 * from the boot prom.
		 */
		fp = kobj_open_file("/.cachefsbackinfo");
		if (fp != (struct _buf *)-1) {
			int c;

			/* first line is file system type... */
			cp = backfs.bo_fstype;
			while ((c = kobj_getc(fp)) != -1 && c != '\n')
				*cp++ = c;
			*cp = '\0';
			/* ... second line is OBP name */
			cp = backfs.bo_name;
			while ((c = kobj_getc(fp)) != -1 && c != '\n')
				*cp++ = c;
			*cp = '\0';
			kobj_close_file(fp);

			backfs.bo_flags |= BO_VALID;
			rootfs.bo_name[0] = '\0';
			bop = &frontfs;
		} else {
			bop = &backfs;
		}

		/* set frontfs or backfs, as appropriate, from the PROM */
		(void) BOP_GETPROP(bootops, "fstype", bop->bo_fstype);
		/*
		 * Look for the 1275 compliant name 'bootpath' first,
		 * but make certain it has a non-NULL value as well.
		 */
		if ((BOP_GETPROP(bootops, "bootpath", bop->bo_name) == -1) ||
		    strlen(bop->bo_name) == 0)
			(void) BOP_GETPROP(bootops, "boot-path", bop->bo_name);
		bop->bo_flags |= BO_VALID;
#endif /* i86 */

		/* It's okay not to have a front filesystem type */
		if ((vsw = getfstype("front", frontfs.bo_fstype)) == NULL)
			frontfs.bo_fstype[0] = '\0';
		else
			(void) strcpy(frontfs.bo_fstype, vsw->vsw_name);
		if ((vsw = getfstype("back", backfs.bo_fstype)) == NULL) {
			/* in case we have no file system types */
			return (ENXIO);
		}
		(void) strcpy(backfs.bo_fstype, vsw->vsw_name);
	}

	/*
	 * Load the favoured drivers of the implementation.
	 * e.g. 'sbus' and possibly 'zs' (even).
	 *
	 * Called whilst boot is still loaded (because boot does
	 * the i/o for us), and DDI services are unavailable.
	 */
	for (i = 0; (this = impl_module_list[i]) != NULL; i++) {
		if ((err = loaddrv_hierarchy(this, (major_t)0)) != 0) {
			cmn_err(CE_WARN, "Cannot load drv/%s", this);
			return (err);
			/* NOTREACHED */
		}
	}
	/*
	 * Now load the platform pm modules (if any)
	 */
	for (i = 0; (this = platform_pm_module_list[i]) != NULL; i++) {
		if ((err = loaddrv_hierarchy(this, (major_t)0)) != 0) {
			cmn_err(CE_WARN, "Cannot load drv/%s", this);
			return (err);
			/* NOTREACHED */
		}
	}

loop:
	(void) getphysdev("root", rootfs.bo_name);
	if (strcmp(rootfs.bo_fstype, "cachefs") == 0) {
		if (frontfs.bo_fstype[0] != '\0')
			(void) getphysdev("front", frontfs.bo_name);
		(void) getphysdev("back", backfs.bo_name);
	}

	DPRINTF(("trying root on %s fstype %s ..\n", rootfs.bo_name,
	    rootfs.bo_fstype));

	/*
	 * Given a physical pathname, load the correct set of driver
	 * modules into memory, including all possible parents.
	 *
	 * Note that getlastfrompath() goes from leaf to root; this isn't
	 * merely idle whim, it's to cope with (a) the scsibus pseudo-nexus
	 * and (b) combined leaf and lightweight nexus drivers like 'xd'
	 *
	 * NB: The code sets the variable 'name' for error reporting.
	 */
	if (strcmp(rootfs.bo_fstype, "cachefs") == 0) {
		/*
		 * If this pathname binds to an alternate driver,
		 * use the alternate name to load the hierarchy,
		 * else, use the node name.
		 */
		name = backfs.bo_name;
		if ((this = i_path_to_drv(name)) == 0)
			this = getlastfrompath(name);
		if (this == 0)
			err = ENXIO;
		else
			err = loaddrv_hierarchy(this, (major_t)0);

		if (frontfs.bo_fstype[0] != '\0' && err == 0) {
			name = frontfs.bo_name;
			if ((this = i_path_to_drv(name)) == 0)
				this = getlastfrompath(name);
			if (this == 0)
				err = ENXIO;
			else
				err = loaddrv_hierarchy(this, (major_t)0);
		}
	} else {
		name = rootfs.bo_name;
		if ((this = i_path_to_drv(name)) == 0)
			this = getlastfrompath(name);
		if (this == 0)
			err = ENXIO;
		else
			err = loaddrv_hierarchy(this, (major_t)0);
	}

	if (err != 0) {
		cmn_err(CE_CONT, "Cannot load drivers for %s\n", name);
		goto out;
		/* NOTREACHED */
	}

	/*
	 * Preload (load-only, no init) all modules which
	 * were added to the /etc/system file with the
	 * FORCELOAD keyword.  This enables having root on
	 * layered/pseudo devices.
	 */
	(void) mod_sysctl_type(MOD_FORCELOAD, preload_module, NULL);

/* ONC_PLUS EXTRACT START */
	/*
	 * If we booted otw then load in the plumbing
	 * routine now while we still can. If we didn't
	 * boot otw then we will load strplumb in main().
	 *
	 * NFS is actually a set of modules, the core routines,
	 * a diskless helper module, rpcmod, and the tli interface.  Load
	 * them now while we still can.
	 *
	 * Because we glomb all versions of nfs into a single module
	 * we check based on the initial string "nfs".
	 *
	 * XXX: A better test for this is to see if device_type
	 * XXX: from the PROM is "network".
	 */

	if (strncmp(rootfs.bo_fstype, "nfs", 3) == 0 ||
	    strncmp(backfs.bo_fstype, "nfs", 3) == 0) {
		int	more, status, dhcacklen;
		char	*dir, *mod;

		++netboot;

		/*
		 *	Before bootops disappears, check for a "bootp-response"
		 *	property and save it. We leave room at the beginning of
		 *	saved property to cache the interface name we used to
		 *	boot the client. This context is necessary for the
		 *	user land dhcpagent to do its job properly on a multi-
		 *	homed system.
		 */
		status = BOP_GETPROPLEN(bootops, "bootp-response");
		if (status > 0) {
			dhcacklen = status + IFNAMSIZ;
			dhcack = kmem_zalloc(dhcacklen, KM_SLEEP);
			if (BOP_GETPROP(bootops, "bootp-response",
			    (uchar_t *)&dhcack[IFNAMSIZ]) == -1) {
				cmn_err(CE_WARN, "BOP_GETPROP of  "
				    "\"bootp-response\" failed\n");
				goto out;
			}
		}

		if ((err = modload("misc", "tlimod")) < 0)  {
			cmn_err(CE_CONT, "Cannot load misc/tlimod\n");
			goto out;
			/* NOTREACHED */
		}
		if ((err = modload("strmod", "rpcmod")) < 0)  {
			cmn_err(CE_CONT, "Cannot load strmod/rpcmod\n");
			goto out;
			/* NOTREACHED */
		}
		if ((err = modload("misc", "nfs_dlboot")) < 0)  {
			cmn_err(CE_CONT, "Cannot load misc/nfs_dlboot\n");
			goto out;
			/* NOTREACHED */
		}
		if ((err = modload("misc", "strplumb")) < 0)  {
			cmn_err(CE_CONT, "Cannot load misc/strplumb\n");
			goto out;
			/* NOTREACHED */
		}
		more = strplumb_get_driver_list(1, &dir, &mod);
		while (more)  {
			if (strcmp(dir, "drv") == 0)
				err = loaddrv_hierarchy(mod, (major_t)0);
			else
				err = modloadonly(dir, mod);
			if (err < 0)  {
				cmn_err(CE_CONT, "Cannot load %s/%s\n",
				    dir, mod);
				goto out;
				/* NOTREACHED */
			}
			more = strplumb_get_driver_list(0, &dir, &mod);
		}
		err = 0;
	}

	/*
	 * Preload modules needed for booting as a cluster.
	 */
	err = clboot_loadrootmodules();

out:
	if (err != 0 && (boothowto & RB_ASKNAME))
		goto loop;

	return (err);
}
/* ONC_PLUS EXTRACT END */

/*
 * Get the name of the root or swap filesystem type, and return
 * the corresponding entry in the vfs switch.
 *
 * If we're not asking the user, and we're trying to find the
 * root filesystem type, we ask boot for the filesystem
 * type that it came from and use that.  Similarly, if we're
 * trying to find the swap filesystem, we try and derive it from
 * the root filesystem type.
 *
 * If we are booting via NFS we currently have three options:
 *	nfs -	dynamically choose NFS V2 or NFS V3 (default)
 *	nfs2 -	force NFS V2
 *	nfs3 -	force NFS V3
 * Because we need to maintain backward compatibility with the naming
 * convention that the NFS V2 filesystem name is "nfs" (see vfs_conf.c)
 * we need to map "nfs" => "nfsdyn" and "nfs2" => "nfs".  The dynamic
 * nfs module will map the type back to either "nfs" or "nfs3".
 * This is only for root filesystems, all other uses such as cachefs
 * will expect that "nfs" == NFS V2.
 *
 * If the filesystem isn't already loaded, vfs_getvfssw() will load
 * it for us, but if (at the time we call it) modrootloaded is
 * still not set, it won't run the filesystems _init routine (and
 * implicitly it won't run the filesystems vsw_init() entry either).
 * We do that explicitly in rootconf().
 */
static struct vfssw *
getfstype(char *askfor, char *fsname)
{
	struct vfssw *vsw;
	static char defaultfs[BO_MAXFSNAME];
	int root = 0;

	if (strcmp(askfor, "root") == 0) {
		(void) BOP_GETPROP(bootops, "fstype", defaultfs);
		root++;
#ifndef i86
	} else if (strcmp(askfor, "front") == 0) {
		(void) BOP_GETPROP(bootops, "frontfs-fstype", defaultfs);
		DPRINTF(("getfstype: asked for frontfs-type, got (%s)\n",
		    (defaultfs[0] ? defaultfs : "NULL")));
	} else if (strcmp(askfor, "back") == 0) {
		(void) BOP_GETPROP(bootops, "backfs-fstype", defaultfs);
		DPRINTF(("getfstype: asked for backfs-type, got (%s)\n",
		    (defaultfs[0]? defaultfs : "NULL")));
#else
	} else if (strcmp(askfor, "front") == 0 ||
	    strcmp(askfor, "back") == 0) {
		(void) strcpy(defaultfs, fsname);

#endif /* !i86 */
	} else {
		(void) strcpy(defaultfs, "swapfs");
	}

	if (boothowto & RB_ASKNAME) {
		for (*fsname = '\0'; *fsname == '\0'; *fsname = '\0') {
			printf("%s filesystem type [%s]: ", askfor, defaultfs);
			gets(fsname);
			if (*fsname == '\0')
				(void) strcpy(fsname, defaultfs);
			if (strcmp(askfor, "front") == 0 && *fsname == '\0')
				return ((struct vfssw *)NULL);
			if (root) {
				if (strcmp(fsname, "nfs2") == 0)
					(void) strcpy(fsname, "nfs");
				else if (strcmp(fsname, "nfs") == 0)
					(void) strcpy(fsname, "nfsdyn");
			}
			if ((vsw = vfs_getvfssw(fsname)) != NULL)
				return (vsw);
			printf("Unknown filesystem type '%s'\n", fsname);
		}
	} else if (*fsname == '\0') {
		fsname = defaultfs;
	}
	if (*fsname == '\0') {
		return ((struct vfssw *)NULL);
	}

	if (root) {
		if (strcmp(fsname, "nfs2") == 0)
			(void) strcpy(fsname, "nfs");
		else if (strcmp(fsname, "nfs") == 0)
			(void) strcpy(fsname, "nfsdyn");
	}

	return (vfs_getvfssw(fsname));
}

/*
 * This is sort of like strtok(3) - given a full physical device
 * pathname as an argument, we return the component name
 * after the last '/'; given a null argument, we return the
 * next to last component name (and so on).
 */
static char *
getlastfrompath(char *physdevpath)
{
	static char	pathcopy[BO_MAXOBJNAME];
	static char	*p;
	char	c, *cp;

	if (physdevpath != NULL) {
		(void) strcpy(p = pathcopy, physdevpath);
		if (*p != '/')
			return (NULL);
		/*
		 * Move to the end, then walk backwards
		 * to find the char after the last '/'
		 */
		while (*p++ != '\0')
			;
		while (*--p != '/')
			;
		p++;
	}

	/*
	 * If p points to nil, we're done, otherwise p points at
	 * the next component to be returned.
	 */
	if (p == (char *)0 || *p == '\0')
		return (NULL);

	cp = p;

	/*
	 * Walk forwards until we find the tail of this component
	 */
	for (c = *p; c != '\0' && c != '@' && c != '/' && c != ':'; c = *p)
		p++;
	*p = '\0';

	/*
	 * Find the beginning of the next component name, if any.
	 */
	if ((p = cp - 1) != pathcopy) {
		for (c = *--p; c != '\0' && c != '/'; c = *p)
			p--;
		p++;
	} else
		p = 0;

	DPRINTF(("getlastfrompath: returning component '%s'\n", cp));

	return (cp);
}

/*
 * Device drivers are handled specially c.f. other modules, in that we
 * carefully examine the parent-child dependency information in the hwconf
 * files to get all the drivers we may need.
 *
 * This will be needed until instance numbers are gleaned from stable
 * storage (1075240) rather than inferred from the order of attach.
 */
int
loaddrv_hierarchy(char *drvname, major_t prev_major)
{
	struct par_list *pl;
	major_t major;
	dev_info_t *dip;
	dev_info_t *pdip;
	dev_info_t *prev_pdip;

	if (drvname == (char *)0 || *drvname == '\0') {
		/*
		 * XXX	This is wrong.  The root is called 'rootnex'.
		 *
		 * A null driver name corresponds to the root nexus.
		 * However, there's no need to load it, because it's
		 * part and parcel of the implementation modules that
		 * get loaded anyway.
		 */
		return (0);
	}

	if ((major = ddi_name_to_major(drvname)) == (major_t)-1)
		return (-1);
	drvname = ddi_major_to_name(major);	/* resolve aliases */

	DPRINTF(("loaddrv_hierarchy: %s\n", drvname));

	/*
	 * Can we load it now?
	 */
	if (ddi_load_driver(drvname) == DDI_FAILURE)
		return (-1);

	/*
	 * Check its parent-child dependencies (if any), invoking
	 * ourselves recursively where needed.  If any of the parent
	 * modules are missing, we continue on regardless, relying
	 * on the parse of the full physical pathname to ensure that
	 * we have a fully connected devinfo bush of drivers.
	 */

	pl = devnamesp[major].dn_pl;
	while (pl) {
		if (pl->par_major == (major_t)-1) {
			/*
			 * this should be a class spec, replicate here
			 */
			struct par_list *ppl;
			struct par_list *new_pl =
				impl_replicate_class_spec(pl);

			for (ppl = new_pl; ppl != NULL; ppl = ppl->par_next) {
				if (ppl->par_major == major) {
					continue;
				}
				/*
				 * Prevent exploding recursion for 'xy'
				 * and 'xyc'
				 */
				(void) loaddrv_hierarchy(
				    ddi_major_to_name(ppl->par_major),
				    (major_t)0);
			}
			impl_delete_par_list(new_pl);
		} else if (pl->par_major != major) {
			/*
			 * Prevent exploding recursion for 'xy' and 'xyc'
			 */
			(void) loaddrv_hierarchy(
			    ddi_major_to_name(pl->par_major),
			    (major_t)0);
		}
		pl = pl->par_next;
	}

	/*
	 * Lets attempt to load in possible parents that the PROM's devinfo
	 * tree tells us is there.
	 */
	dip = devnamesp[major].dn_head;
	prev_pdip = (dev_info_t *)0;
	if (dip && (prev_major != major)) {
		do {
			pdip = ddi_get_parent(dip);

			/*
			 * If we don't have a previous parent, load the
			 * current one.
			 */
			if (!prev_pdip && pdip) {
				(void) loaddrv_hierarchy(ddi_get_name(pdip),
				    major);
			/*
			 * Check to see if the current parent node
			 * is the same as the previous parent node,
			 * if it is, we don't need to load it again.
			 */
			} else if (pdip && strcmp(ddi_get_name(pdip),
			    ddi_get_name(prev_pdip))) {
				(void) loaddrv_hierarchy(ddi_get_name(pdip),
				    major);
			}

			dip = ddi_get_next(dip);
			prev_pdip = pdip;
		} while (dip);
	}

	return (0);
}

/*
 * Get a physical device name, and maybe load and attach
 * the driver.
 *
 * XXX	Need better checking of whether or not a device
 *	actually exists if the user typed in a pathname.
 *
 * XXX	Are we sure we want to expose users to this sort
 *	of physical namespace gobbledygook (now there's
 *	a word to conjure with..)
 *
 * XXX	Note that on an OBP machine, we can easily ask the
 *	prom and pretty-print some plausible set of bootable
 *	devices.  We can also user the prom to verify any
 *	such device.  Later tim.. later.
 */
static int
getphysdev(char *askfor, char *name)
{
	static char fmt[] = "Enter physical name of %s device\n[%s]: ";
	dev_t dev;
	static char defaultpath[BO_MAXOBJNAME];

	/*
	 * Establish 'default' values - we get the root device from
	 * boot, and we infer the swap device is the same but with
	 * a 'b' on the end instead of an 'a'.  A first stab at
	 * ease-of-use ..
	 */
	if (strcmp(askfor, "root") == 0) {
		/*
		 * Look for the 1275 compliant name 'bootpath' first,
		 * but make certain it has a non-NULL value as well.
		 */
		if ((BOP_GETPROP(bootops, "bootpath", defaultpath) == -1) ||
		    strlen(defaultpath) == 0) {
			if (BOP_GETPROP(bootops,
			    "boot-path", defaultpath) == -1)
				boothowto |= RB_ASKNAME | RB_VERBOSE;
		}
#ifndef i86
	} else if (strcmp(askfor, "front") == 0) {
		if (BOP_GETPROP(bootops, "frontfs-path", defaultpath) == -1)
			boothowto |= RB_ASKNAME | RB_VERBOSE;
	} else if (strcmp(askfor, "back") == 0) {
		if (BOP_GETPROP(bootops, "backfs-path", defaultpath) == -1)
			boothowto |= RB_ASKNAME | RB_VERBOSE;
#else
	} else if (strcmp(askfor, "front") == 0 ||
	    strcmp(askfor, "back") == 0) {
		(void) strcpy(defaultpath, name);
#endif
	} else {
		(void) strcpy(defaultpath, rootfs.bo_name);
		defaultpath[strlen(defaultpath) - 1] = 'b';
	}

retry:
	if (boothowto & RB_ASKNAME) {
		printf(fmt, askfor, defaultpath);
		gets(name);
	}
	if (*name == '\0')
		(void) strcpy(name, defaultpath);

	if (strcmp(askfor, "swap") == 0)   {

		/*
		 * Try to load and install the swap device driver.
		 */
		dev = ddi_pathname_to_dev_t(name);

		if (dev == (dev_t)-1)  {
			printf("Not a supported device for swap.\n");
			boothowto |= RB_ASKNAME | RB_VERBOSE;
			goto retry;
		}

		/*
		 * Ensure that we're not trying to swap on the floppy.
		 */
		if (strncmp(ddi_major_to_name(getmajor(dev)), "fd", 2) == 0) {
			printf("Too dangerous to swap on the floppy\n");
			goto broken;
		}
	}

	return (0);

broken:
	if (boothowto & RB_ASKNAME)
		goto retry;
	else
		return (-1);
}
