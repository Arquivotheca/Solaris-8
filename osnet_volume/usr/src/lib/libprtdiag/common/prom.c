/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom.c	1.3	99/11/08 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <kvm.h>
#include <varargs.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/openpromio.h>
#include <kstat.h>
#include <libintl.h>
#include <syslog.h>
#include <sys/dkio.h>
#include "pdevinfo.h"
#include "display.h"
#include "pdevinfo_sun4u.h"
#include "display_sun4u.h"
#include "libprtdiag.h"


void
disp_prom_version(Prom_node *flashprom)
{
	Prop *version;
	char *vers;		/* OBP version */
	char *temp;

	/* Look version */
	version = find_prop(flashprom, "version");

	vers = (char *)get_prop_val(version);

	if (vers != NULL) {
		log_printf("  %s   ", vers, 0);

		/*
		 * POST string follows the NULL terminated OBP
		 * version string. Do not attempt to print POST
		 * string unless node size is larger than the
		 * length of the OBP version string.
		 */
		if ((strlen(vers) + 1) < version->size) {
			temp = vers + strlen(vers) + 1;
			log_printf("%s", temp, 0);
		}
	}

	log_printf("\n", 0);
}


void
platform_disp_prom_version(Sys_tree *tree)
{
	Board_node *bnode;
	Prom_node *pnode;

	bnode = tree->bd_list;

	/* Display Prom revision header */
	log_printf("System PROM revisions:\n", 0);
	log_printf("----------------------\n", 0);

	if ((pnode = find_device(bnode, 0x1F, SBUS_NAME)) == NULL) {
		pnode = find_pci_bus(bnode->nodes, 0x1F, 1);
	}

	/*
	 * in case of platforms with multiple flashproms, find and
	 * display all proms with a "version"(OBP) property. bug 4187301
	 */
	for (pnode = dev_find_node(pnode, "flashprom"); pnode != NULL;
		pnode = dev_next_node(pnode, "flashprom")) {
		    if (find_prop(pnode, "version") != NULL) {
				disp_prom_version(pnode);
		}
	}
}
