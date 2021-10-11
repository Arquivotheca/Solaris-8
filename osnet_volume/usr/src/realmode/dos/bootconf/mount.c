/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * mount.c -- routines to handle mounting the root filesystem
 *	      and setting the bootpath property
 */

#ident "@(#)mount.c   1.32   99/04/23 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "menu.h"
#include "boot.h"
#include "biosprim.h"
#include "bop.h"
#include "debug.h"
#include "devdb.h"
#include "err.h"
#include "gettext.h"
#include "mount.h"

#define	ERRBUFSZ	500
/*
 * try_mount -- try mounting the root filesystem
 * returns 0 on failure, 1 on success
 */
int
try_mount(bef_dev *bdp, char *mnt_dev_desc)
{
	char buf[ERRBUFSZ];
	char bootpath[120];

	debug(D_FLOW, "try_mount of root\n");

	bootpath[0] = buf[0] = 0;
	get_path_from_bdp(bdp, bootpath, 0);

	if (bootpath[0] != 0) {
		if (bdp->info->dev_type == MDB_SCSI_HBA) {
			(void) sprintf(buf, "mount %s / ufs\n", bootpath);
		} else {
			(void) sprintf(buf, "mount %s / nfs\n", bootpath);
		}
		if (!Autoboot) {
			status_menu(Please_wait, "MENU_RUNNING_DVR");
		}
		out_bop(buf);

		buf[0] = 0;
		(void) in_bop(buf, ERRBUFSZ);

		if (buf[0] == ' ' && bootpath[0]) {
			/*
			 * Set bootpath property
			 */
			out_bop("dev /options\n");
			(void) sprintf(buf, "setprop bootpath %s\n", bootpath);
			out_bop(buf);

			/*
			 * Set boot-path property.  This needs to be the
			 * compatibility bootpath that will allow us to
			 * successfully boot older kernels.
			 */
			get_path_from_bdp(bdp, bootpath, 1);
			(void) sprintf(buf, "setprop boot-path %s\n", 
				bootpath);
			out_bop(buf);

			return (1); /* success */
		}
	}

	/* mount failed */
	if ((buf[1] == 0) || (buf[1] == '\r') || (buf[1] == '\n')) {
		strcpy(&buf[1], "No failure info available");
	} else {
		/*
		 * Continue reading the possibly multi-line
		 * error message into the buffer until we get an
		 * end of file or the buffer size is exceeded.
		 */
		int bufsz = ERRBUFSZ;
		char *s = buf;

		do {
			while (*s) {
				if (--bufsz == 0) {
					/*
					 * Buffer size exceeded
					 * Flush remaining input using tmp buf
					 */
					buf[ERRBUFSZ-1] = 0;
					while (in_bop(bootpath, 120)) {
						;
					}
					goto done;
				}
				if (*s == '\r') {
					*s = ' ';
				}
				s++;
			}
			/*
			 * To enable testing under dos we additionally check
			 * for a magic string "EOF" at the end of a line.
			 */
			if ((!dos_emul_boot) && /* i.e. running under dos */
			    (*(s - 4) == 'E') &&
			    (*(s - 3) == 'O') &&
			    (*(s - 2) == 'F')) {
				goto done;
			}
		} while (in_bop(s, bufsz));
	}
done:
	if (!Autoboot) {
		enter_menu("MENU_HELP_MOUNT", "MENU_MOUNT_FAIL", mnt_dev_desc,
			&buf[1]);
	}

	return (0);
}
