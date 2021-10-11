/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)mod.c 1.9 99/03/26 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <libintl.h>
#include <sys/modctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "dvc.h"
#include "dvc_msgs.h"
#include "conf.h"
#include "dev.h"
#include "util.h"

extern int modctl(int, int, ...); 	/* This should be in a <sys/modctl.h> */

static int
get_mod_id(char *name)
{
	int id;
	struct modinfo modinfo;

	id = -1;
	modinfo.mi_id = modinfo.mi_nextid = id;
	modinfo.mi_info = MI_INFO_ALL;
	
	for (;;) {
		if (modctl(MODINFO, id, &modinfo) < 0)
			break;

		id = modinfo.mi_id;
		modinfo.mi_name[MODMAXNAMELEN - 1] = NULL;

		if (streq(modinfo.mi_name, name))
			return (id);
	}

	return (-1);
}

void
config_mod(char* name, char *root, char *inst)
{
	int id;
	int pid;
	char cmd[64];

	/*
	 * If the module is loaded, unload it.
	 */
	if ((id = get_mod_id(name)) != -1)
		(void)modctl(MODUNLOAD, id);

	sprintf(cmd, "drvconfig -i %s -p %s -r %s", name, inst, root);
	if (system(cmd) == -1) {
		char *msg;

		msg = strcats(MSG(CONFIGERR), name, MSG(DRIVER), NULL);
		ui_notice(msg);
		xfree(msg);
	}
}

void
update_mod_conf(void)
{
	int dolinks = 0;
	char *class;
	device_info_t *dp;
	char *root = NULL;
	char *inst = NULL;
	char *devname  = NULL;

	for (dp = dev_head; dp; dp = dp->next) {
		if (!dp->modified || find_attr_val(dp->typ_alist, AUTO_ATTR))
			continue;

		class = find_attr_str(dp->typ_alist, CLASS_ATTR);
		devname   = find_attr_str(dp->typ_alist,DRVR_ATTR);


		/*
		 * If not a bogus window device, write a conf file
		 * for it and configure the device.
		 */
		if (class != NULL && (!streq(class, "win") || 
				      (devname != NULL))) {
			write_conf_file(dp);
			if (dvc_tmp_root)
				continue;
			if (root == NULL) {
				/*
				 * Make sure we have write paths for devices
				 * and path_to_inst.  
				 * The MODCONFIG requries the root to be
				 * specified relative to the current directory.
				 * Make sure we are in the right place to
				 * run drvconfig.  If the write path for
				 * devices is itself, then we are in
				 * non-install case; otherwise find the
				 * root from the returned path.
				 *
				 */
				root = get_write_path("/devices", NULL);
				if (streq(root, "/devices"))
					root[1] = NULL;
				else
					*strrchr(root, '/') = NULL;
				inst = get_write_path("/etc", "path_to_inst");
				chdir(root);

			}
			config_mod(dp->name, "devices", inst);
			dolinks++;
		}

	}

	if (root != NULL)
		xfree(root);
	if (inst != NULL)
		xfree(inst);

	if (dolinks)
		system("/usr/sbin/devlinks");
}
