/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)dockspace.c	1.27	96/04/05 SMI"	/* SVr4.0 1.7.1.1 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <limits.h>
#include <locale.h>
#include <libintl.h>
#include <pkgstrct.h>
#include "install.h"
#include <pkglib.h>
#include "libadm.h"
#include "libinst.h"
#include "pkginstall.h"

extern struct cfextra **extlist;
extern char	pkgloc[];
extern char	pkgloc_sav[];
extern char	instdir[];
extern int	update;

#define	LSIZE		256
#define	LIM_BFREE	150L
#define	LIM_FFREE	25L

#define	WRN_STATVFS	"WARNING: unable to stat filesystem mounted on <%s>"

#define	WRN_NOBLKS	"The %s filesystem has %lu free blocks. The current " \
			"installation requires %lu blocks, which includes a " \
			"required %lu block buffer for open " \
			"deleted files. %lu more blocks are needed."

#define	WRN_NOFILES	"The %s filesystem has %lu free file nodes. The " \
			"current installation requires %lu file nodes, which " \
			"includes a required %lu file node buffer " \
			"for temporary files. %lu more file nodes " \
			"are needed."

#define	TYPE_BLCK	0
#define	TYPE_NODE	1
static void	warn(int type, char *name, ulong need, ulong avail,
			long limit);
static int	fsys_stat(int n);
static int	readmap(int *error);
static int	readspace(char *spacefile, int *error, int op);

int
dockspace(char *spacefile)
{
	struct fstable *fs_tab;
	char	old_space[PATH_MAX];
	long	bfree, ffree;
	int	i, error;

	error = 0;

	/*
	 * Also, vanilla SVr4 code used the output from popen()
	 * on the "/etc/mount" command.  However, we need to get more
	 * information about mounted filesystems, so we use the C
	 * interfaces to the mount table, which also happens to be
	 * much faster than running another process.  Since several
	 * of the pkg commands need access to the mount table, this
	 * code is now in libinst.  However, mount table info is needed
	 * at the time the base directory is determined, so the call
	 * to get the mount table information is in main.c
	 */

	if (update && pkgloc_sav && *pkgloc_sav) {
		sprintf(old_space, "%s/install/space", pkgloc_sav);
		if (!access(old_space, R_OK))
			(void) readspace(old_space, &error, -1);
	}

	if (readmap(&error) || readspace(spacefile, &error, 1))
		return (-1);

	for (i = 0; fs_tab = get_fs_entry(i); ++i) {
		if ((!fs_tab->fused) && (!fs_tab->bused))
			continue; /* not used by us */
		bfree = (long) fs_tab->bfree - (long) fs_tab->bused;
		/* bug id 1091292 */
		if (bfree < LIM_BFREE) {
			warn(TYPE_BLCK, fs_tab->name, fs_tab->bused,
				fs_tab->bfree, LIM_BFREE);
			error++;
		}
		/* bug id 1091292 */
		if ((long) fs_tab->ffree == -1L)
			continue;
		ffree = (long) fs_tab->ffree - (long) fs_tab->fused;
		if (ffree < LIM_FFREE) {
			warn(TYPE_NODE, fs_tab->name, fs_tab->fused,
				fs_tab->ffree, LIM_FFREE);
			error++;
		}
	}
	return (error);
}

static void
warn(int type, char *name, ulong need, ulong avail, long limit)
{
	logerr(gettext("WARNING:"));
	if (type == TYPE_BLCK) {
		logerr(gettext(WRN_NOBLKS), name, avail, (need + limit), limit,
		    (need + limit - avail));
	} else {
		logerr(gettext(WRN_NOFILES), name, avail, (need + limit), limit,
		    (need + limit - avail));
	}
}

static int
fsys_stat(int n)
{
	struct statvfs svfsb;
	struct fstable *fs_tab;

	if (n == BADFSYS)
		return (1);

	fs_tab = get_fs_entry(n);

	/*
	 * At this point, we know we need information
	 * about a particular filesystem, so we can do the
	 * statvfs() now.  For performance reasons, we only want to
	 * stat the filesystem once, at the first time we need to,
	 * and so we can key on whether or not we have the
	 * block size for that filesystem.
	 */
	if (fs_tab->bsize != 0)
		return (0);

	if (statvfs(fs_tab->name, &svfsb)) {
		logerr(gettext(WRN_STATVFS), fs_tab->name);
		return (1);
	}

	/*
	 * statvfs returns number of fragment size blocks
	 * so will change this to number of 512 byte blocks
	 */
	fs_tab->bsize  = svfsb.f_bsize;
	fs_tab->frsize = svfsb.f_frsize;
	fs_tab->bfree  = (((long) svfsb.f_frsize > 0) ?
		howmany(svfsb.f_frsize, DEV_BSIZE) :
		howmany(svfsb.f_bsize, DEV_BSIZE)) * svfsb.f_bavail;
	fs_tab->ffree  = ((long) svfsb.f_favail > 0) ?
			    svfsb.f_favail : svfsb.f_ffree;

	return (0);
}

/*
 * This function reads all of the package objects, maps them to their target
 * filesystems and adds up the amount of space used on each. Wherever you see
 * "fsys_value", that's the apparent filesystem which could be a temporary
 * loopback mount for the purpose of constructing the client filesystem. It
 * isn't necessarily the real target filesystem. Where you see "fsys_base"
 * that's the real filesystem to which fsys_value may just refer. If this is
 * installing to a standalone or a server, fsys_value will almost always be
 * the same as fsys_base.
 */
static int
readmap(int *error)
{
	struct fstable *fs_tab;
	struct cfextra *ext;
	struct cfent *ept;
	struct stat statbuf;
	char	tpath[PATH_MAX];
	long	blk;
	int	i, n;

#if 0	/* No wanted 3/24/93 */
	/*
	 * Handle the pkgmap file, the space check in ocfile() insures that
	 * the contents file is counted by now.
	 */
	(void) sprintf(tpath, "%s/pkgmap", instdir);
	if (stat(tpath, &statbuf) != -1) {
		(void) sprintf(tpath, "%s/pkgmap", pkgloc);
		n = resolved_fsys(tpath);

		fs_tab = get_fs_entry(n);

		if (!fsys_stat(n)) {
			if (!is_remote_fs_n(n) && is_fs_writeable_n(n)) {
				fs_tab->fused++;
				fs_tab->bused += nblk(statbuf.st_size,
					fs_tab->bsize,
					fs_tab->frsize);
			}
		} else {
			(*error)++;
		}
	} else {
		(*error)++;
	}
#endif	/* 0 */

	/*
	 * Handle the installation files (ftype i) that are in the
	 * pkgmap/eptlist.
	 */
	for (i = 0; (ext = extlist[i]) != NULL; i++) {
		ept = &(ext->cf_ent);

		if (ept->ftype != 'i')
			continue;

		/*
		 * These paths are treated differently from the others
		 * since their full pathnames are not included in the
		 * pkgmap.
		 */
		if (strcmp(ept->path, "pkginfo") == 0)
			(void) sprintf(tpath, "%s/%s", pkgloc, ept->path);
		else
			(void) sprintf(tpath, "%s/install/%s", pkgloc,
			    ept->path);

		/* If we haven't done an fsys() series, do one */
		if (ext->fsys_value == BADFSYS)
			ext->fsys_value = fsys(tpath);

		/*
		 * Now check if this is a base or apparent filesystem. If
		 * it's just apparent, get the resolved filesystem entry,
		 * otherwise, base and value are the same.
		 */
		if (use_srvr_map_n(ext->fsys_value))
			ext->fsys_base = resolved_fsys(tpath);
		else
			ext->fsys_base = ext->fsys_value;

		if (fsys_stat(ext->fsys_base)) {
			(*error)++;
			continue;
		}

		/*
		 * Don't accumulate space requirements on read-only
		 * remote filesystems.
		 */
		if (is_remote_fs_n(ext->fsys_value) &&
		    !is_fs_writeable_n(ext->fsys_value))
			continue;

		fs_tab = get_fs_entry(ext->fsys_base);

		fs_tab->fused++;
		if (ept->cinfo.size != BADCONT)
			blk = nblk(ept->cinfo.size,
			    fs_tab->bsize,
			    fs_tab->frsize);
		else
			blk = 0;
		fs_tab->bused += blk;
	}

	/*
	 * Handle the other files in the eptlist.
	 */
	for (i = 0; (ext = extlist[i]) != NULL; i++) {
		ept = &(extlist[i]->cf_ent);

		if (ept->ftype == 'i')
			continue;

		/*
		 * Don't recalculate package objects that are already in the
		 * table.
		 */
		if (ext->mstat.preloaded)
			continue;

		/*
		 * Don't accumulate space requirements on read-only
		 * remote filesystems.
		 */
		if (is_remote_fs(ept->path, &(ext->fsys_value)) &&
		    !is_fs_writeable(ept->path, &(ext->fsys_value)))
			continue;

		/*
		 * Now check if this is a base or apparent filesystem. If
		 * it's just apparent, get the resolved filesystem entry,
		 * otherwise, base and value are the same.
		 */
		if (use_srvr_map_n(ext->fsys_value))
			ext->fsys_base = resolved_fsys(tpath);
		else
			ext->fsys_base = ext->fsys_value;

		/* At this point we know we have a good fsys_base. */
		if (fsys_stat(ext->fsys_base)) {
			(*error)++;
			continue;
		}

		/*
		 * We have to stat this path based upon it's real location.
		 * If this is a server-remap, ept->path isn't the real
		 * location.
		 */
		if (use_srvr_map_n(ext->fsys_value))
			strcpy(tpath, server_map(ept->path, ext->fsys_value));
		else
			strcpy(tpath, ept->path);

		fs_tab = get_fs_entry(ext->fsys_base);

		if (stat(tpath, &statbuf)) {
			/* path cannot be accessed */
			fs_tab->fused++;
			if (strchr("dxs", ept->ftype))
				blk =
				    nblk((long)fs_tab->bsize,
				    fs_tab->bsize,
				    fs_tab->frsize);
			else if (ept->cinfo.size != BADCONT)
				blk = nblk(ept->cinfo.size,
				    fs_tab->bsize,
				    fs_tab->frsize);
			else
				blk = 0;
		} else {
			/* path already exists */
			if (strchr("dxs", ept->ftype))
				blk = 0;
			else if (ept->cinfo.size != BADCONT) {
				blk = nblk(ept->cinfo.size,
				    fs_tab->bsize,
				    fs_tab->frsize);
				blk -= nblk(statbuf.st_size,
				    fs_tab->bsize,
				    fs_tab->frsize);
				/*
				 * negative blocks show room freed, but since
				 * order of installation is uncertain show
				 * 0 blocks usage
				 */
				if (blk < 0)
					blk = 0;
			} else
				blk = 0;
		}
		fs_tab->bused += blk;
	}
	return (0);
}

static int
readspace(char *spacefile, int *error, int op)
{
	FILE	*fp;
	char	line[LSIZE];
	long	blocks, nodes;
	int	n;

	if (spacefile == NULL)
		return (0);

	if ((fp = fopen(spacefile, "r")) == NULL) {
		progerr(gettext("unable to open spacefile %s"), spacefile);
		return (-1);
	}

	while (fgets(line, LSIZE, fp)) {
		struct fstable *fs_tab;
		char *pt, path[PATH_MAX];

		for (pt = line; isspace(*pt); /* void */)
			pt++;
		if ((*line == '#') || !*line)
			continue;

		(void) sscanf(line, "%s %ld %ld", path, &blocks, &nodes);
		mappath(2, path);
		basepath(path, get_basedir(), get_inst_root());
		canonize(path);

		n = resolved_fsys(path);
		if (fsys_stat(n)) {
			(*error)++;
			continue;
		}

		/*
		 * Don't accumulate space requirements on read-only
		 * remote filesystems. NOTE: For some reason, this
		 * used to check for !remote && read only. If this
		 * blows up later, then maybe that was correct -- JST
		 */
		 if (is_remote_fs_n(n) && !is_fs_writeable_n(n))
			continue;

		fs_tab = get_fs_entry(n);

		fs_tab->bused += (blocks * op);
		fs_tab->fused += (nodes * op);
	}
	(void) fclose(fp);
	return (0);
}
