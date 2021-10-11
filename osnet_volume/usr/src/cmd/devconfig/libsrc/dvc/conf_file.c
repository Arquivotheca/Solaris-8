/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)conf_file.c 1.8 95/03/08 SMI"

#include <errno.h>
#include <fcntl.h>
#include <libintl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/mnttab.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "dvc.h"
#include "dvc_msgs.h"
#include "conf.h"
#include "dev.h"
#include "util.h"
#include "win.h"

int dvc_tmp_root = 0;
static int dvc_install = -1;
static attr_list_t *install_map;
static char *deflt = NULL;

static int
cant_write(char *dir)
{
	int fd;
	char *name;

	name = strcats(dir, ".libdvctest", NULL);
	fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd == -1)
		return (1);
	close(fd);
	remove(name);
	xfree(name);

	return (0);
}

/*
 * See if the root is mounted read-only; if so, we must be
 * doing an install.  Make sure we have the install directory
 * map to know where writes will be mapped.
 */
static void
init_install()
{

	if (cant_write("/"))
		dvc_install = 1;
	else
		dvc_install = 0;

if (dvc_verbose)
	dvc_install = 1;

	if ((dvc_install || dvc_tmp_root) &&
	    (install_map = find_typ_info("install_map")) == NULL)
			ui_error_exit(MSG(ENOINSTALLMAP));
}

/*
 *  Call mkdir for all path components that don't exist.
 */
static int
make_path(char *path)
{
	int n = strlen(path) + 1;
	char *part = (char *)xmalloc(n);
	char *pp = part;

	memset(pp, 0, n);
	for (;;) {
		if (*path == '/' || (*path == '\0' && *(path - 1) != '/'))
			if (((pp != part) && access(part, R_OK+W_OK)) &&
			    mkdir(part, 0777)) {
				xfree(part);
				return (0);
			}

		if (*path == '\0')
			break;

		*pp = *path;
		pp++;
		path++;
	}

	xfree(part);
	return (1);
}

/*
 * Return the install map for a given directory.
 * If the directory is explicitly listed, use its mapping,
 * otherwise, use the default mapping.
 */
static char *
get_install_map(char *dir)
{
	char *root = NULL;
	char *map = NULL;
	attr_list_t *alist;

	/*
	 * Install map names are relative.
	 */
	while (*dir && *dir == '/') {
		dir++;
		root++;
	}

	/*
	 * Find the mapping for this directory.
	 */
	for (alist = install_map; alist; alist = alist->next)
		if (streq(dir, alist->name)) {
			map = alist->vlist->val.string;
			break;
		} else if (streq(alist->name, DEFAULT_ATTR))
			deflt = alist->vlist->val.string;

	if (map == NULL)
		if (deflt == NULL)
			ui_error_exit(MSG(EBADINSTALLMAP));
		else
			map = deflt;

	/*
	 * Make sure the resulting map is absolute if the
	 * directory was.
	 */
	if (root)
		map = (strcats("/", map, NULL));
	else
		map = xstrdup(map);

	return (map);
}

/*
 * Get the write map for a directory.  If doing an install, get
 * the install map, otherwise return the directory itself.
 * For example
 *
 *	dir		non-install-map	install-map
 *	---		--------------	-----------
 *	/kernel/drv	/kernel/drv	/tmp/root/kernel/drv
 */
char *
get_write_map(char *dir)
{
	char *map;

	/*
	 * The first time through, see if we are doing an install.
	 */
	if (dvc_install == -1)
		init_install();

	if (dvc_install || dvc_tmp_root) {
		map = get_install_map(dir);
	} else
		map = xstrdup(dir);

	if (!make_path(map)) {
		vrb(MSG(MKDIR_ERR), dir);
		xfree(map);
		map = NULL;
	}

	return (map);
}

/*
 * Return the full (write) pathname for a file.  If we are doing an install
 * or we are in test mode, return the install path, otherwise,
 * return the original path.  In either case, make sure the path exists.
 * The caller must free the path when done.
 */
char *
get_write_path(char *dir, char *name)
{
	char *path;
	char *map;

	if ((map = get_write_map(dir)) == NULL)
		return (NULL);

	if (dvc_install || dvc_tmp_root) {
		if (*dir == '/')
			dir = (strcats(map, dir, NULL));
		else
			dir = (strcats(map, "/", dir, NULL));
		xfree(map);
	} else
		dir = map;


	if (make_path(dir)) {
		if (name)
			path = strcats(dir, "/", name, NULL);
		else
			path = xstrdup(dir);
	} else {
		vrb(MSG(MKDIR_ERR), dir);
		path = NULL;
	}


	xfree(dir);

	return (path);
}

static FILE *
make_conf_file(char *filename)
{
#define	NBAK	64
	FILE *fp;
	char *bfr;
	static char *baktab[NBAK];
	static int nbak;

	if (!access(filename, F_OK)) {
		int i;

		/*
		 * The .conf file exits, see if we need to make a backup.
		 */
		for (i = 0; i < nbak; i++)
			if (streq(filename, baktab[i]))
				break;

		if (i < nbak)
			goto write;

		/*
		 * This is the first time for this device,
		 * see if we can make the backup.
		 */
		if (nbak == NBAK) {
			bfr = strcats(MSG(CONFBKPERR), NULL);
			ui_notice(bfr);
			xfree(bfr);
			goto write;
		} else {
			char *bak = strcats(filename, ".bak", NULL);
			unlink(bak);
			if (link(filename, bak) == -1) {
				bfr = strcats(MSG(BKPERR), filename, NULL);
				ui_notice(bfr);
				xfree(bfr);
				goto write;
			}
			unlink(filename);
			baktab[nbak++] = xstrdup(filename);
		}
	}

write:
	fp = fopen(filename, "w");

	if (fp == NULL) {
		bfr = strcats(MSG(OPENERR), filename, MSG(TOWRITE), NULL);
		ui_notice(bfr);
		xfree(bfr);
	}

	return (fp);
#undef	NBAK
}

FILE *
open_conf_file(char *dir, char *name, int wr)
{
	char *conf;
	FILE *fp;

	if (!wr) {
		errno = 0;

		conf = strcats(dir, "/", name, NULL);
		if ((fp = fopen(conf, "r")) == NULL)
			vrb(MSG(READERR), conf, strerror(errno));
		xfree(conf);
	} else {
		fp = NULL;

		if ((conf = get_write_path(dir, name)) != NULL) {
			fp = make_conf_file(conf);
			xfree(conf);
		}
	}

	return (fp);
}

/*
 * Get the driver's attribute list using the common and driver
 * attribute specs.
 */
static attr_list_t *
get_drv_alist(attr_list_t *base, val_list_t *comm, val_list_t *drv)
{
	attr_list_t *alist;
	attr_list_t *ap;
	attr_list_t *last = NULL;

	alist = dup_alist(base);
	for (ap = alist; ap; ap = ap->next) {
		if ((comm && find_val_name(ap->name, comm)) ||
			find_val_name(ap->name, drv))
			last = ap;
		else {
			if (last)
				last->next = ap->next;
			else
				alist = ap->next;
		}
	}

	return (alist);
}

static attr_list_t *
get_conf_alist(device_info_t *dp)
{
	attr_list_t *ap;
	attr_list_t *alist;
	attr_list_t *typ;
	attr_list_t *last = NULL;

	alist = dup_alist(dp->dev_alist);
	typ = dp->typ_alist;

	for (ap = alist; ap; ap = ap->next) {
		if (!find_attr_val(typ, ap->name))
			if (last)
				last->next = ap->next;
			else
				alist = ap->next;
		else
			last = ap;
	}

	return (alist);
}

/*
 * See if this a window device.  Temporary fix till will have a
 * correct way of dealing with classes.
 */
static int
win_class(device_info_t *dp)
{
	if (match_attr(dp->typ_alist, "class", "win") ||
	    find_attr_val(dp->typ_alist, "__wclass__"))
		return (1);

	return (0);
}

static char *
make_conf_name(char *name, char *drvloc)
{
	static char *drv_path = (char *)-1;
	char *ret, *buf, *buf1, *drv;
	struct stat statbuf;
	char *cmp;
	attr_list_t *alist;

	if (install_map == NULL)
		install_map = find_typ_info("install_map");

	if (deflt == NULL)
		for (alist = install_map; alist; alist = alist->next)
			if (streq(alist->name, DEFAULT_ATTR))
				deflt = alist->vlist->val.string;

	cmp = strcats("/", deflt, NULL);
	drv_path = get_write_path(drvloc, NULL);
	if (drv_path == NULL)
		return (NULL);
	ret = strcats(drv_path, "/", name, ".conf", NULL);
	drv = strcats(drv_path, "/", name, NULL);

	/*
	 * This code checks to see of the .conf file or the associated
	 * driver exists in the mounted root file system
	 * (note, this will be a symlink  to /tmp/root/....),
	 * if it isn't there return -1.
	 */

	if (strncmp(ret, cmp, strlen(cmp)) == 0)
		buf = ret + strlen(cmp);
	else
		buf = ret;
	if (strncmp(drv, cmp, strlen(cmp)) == 0)
		buf1 = drv + strlen(cmp);
	else
		buf1 = drv;

	if ((lstat(buf, &statbuf) == -1) && (lstat(buf1, &statbuf) == -1)) {
		xfree(ret);
		return ((char *)-1);
	}
	xfree(drv);
	xfree(cmp);
	return (ret);
}

void
write_conf_file(device_info_t *dp)
{
	int win;
	FILE *fp;
	conf_list_t cf;
	val_list_t *drv;
	char *filename;
	struct stat statbuf;
	FILE *fp2;
	char *path;
	struct utsname uinfo;
	attr_list_t *alist;
	char *krn, *plt, *usr = NULL;


	uname(&uinfo);

	/*
	 * Get pathname strings
	 */

	if (install_map == NULL)
		install_map = find_typ_info("install_map");

	for (alist = install_map; alist; alist = alist->next)
		if (streq(KRN_ATTR, alist->name))
			krn = alist->vlist->val.string;
		else if (streq(PLTFRM_ATTR, alist->name))
			plt = alist->vlist->val.string;
		else if (streq(USR_ATTR, alist->name))
			usr = alist->vlist->val.string;
		else if ((deflt == NULL) && (streq(DEFAULT_ATTR, alist->name)))
			deflt = alist->vlist->val.string;

	if (krn == NULL || plt == NULL || usr == NULL)
		return;

	/*
	 * Try to find .conf file in the following directory
	 * hierarchy.  Note the <arch> field is exracted
	 * from uname.machine.  Note, make_conf_name will return a -1
	 * if it cannot find a .conf file in the appropriate directory or
	 * in the corresponding /tmp/root directory.
	 *     /usr/platform/<arch>/kernel/drv
	 *     /platform/<arch>/kernel/drv
	 *     /usr/kernel/drv, then
	 *     /kernel/drv
	 */

	path = strcats(usr, plt, "/", uinfo.machine, krn, NULL);
	if ((filename = make_conf_name(dp->name, path)) == NULL)
		return;
	if (filename == (char *) -1) {
		xfree(path);
		path = strcats(plt, "/", uinfo.machine, krn, NULL);
		if ((filename = make_conf_name(dp->name, path)) == NULL)
			return;
		if (filename == (char *) -1) {
			xfree(path);
			path = strcats(usr, krn, NULL);
			if ((filename = make_conf_name(dp->name, path)) == NULL)
				return;
			if (filename == (char *) -1) {
				xfree(path);
				if ((filename = make_conf_name(dp->name, krn))
					    == NULL)
					return;
				if (filename == (char *) -1) {
					filename = strcats(krn, "/", dp->name,
							    ".conf", NULL);
				}
			}
		}
	}




	/*
	 * If the class is `win' this device may not have a
	 * driver, so don't write a standard conf file for it.
	 */
	if (win = win_class(dp))
		drv = find_attr_val(dp->typ_alist, DRIVER_ATTR);
	else
		drv = NULL;



	if (win && drv != NULL) {
		val_list_t *comm;

		comm = find_attr_val(dp->typ_alist, COMMON_ATTR);
		memset(&cf, 0, sizeof (cf));
		cf.alist = get_drv_alist(dp->dev_alist, comm, drv);
		if (cf.alist != NULL) {
			if ((fp = make_conf_file(filename)) != NULL) {
				write_conf(fp, &cf);
				fclose(fp);
			}
		}
	} else if (!win) {
		device_info_t *dpp;

		if ((fp = make_conf_file(filename)) == NULL) {
			xfree(filename);
			return;
		}

		/*
		 * For each device matching this name,
		 * write an entry.
		 */
		for (dpp = dev_head; dpp; dpp = dpp->next)
			if (streq(dpp->name, dp->name)) {
				memset(&cf, 0, sizeof (cf));
				if (cf.alist = get_conf_alist(dpp))
					write_conf(fp, &cf);
			}

		fclose(fp);
	}

	xfree(filename);
}
