/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)memory.c	1.2	99/10/19 SMI"

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
display_memorysize(Sys_tree *tree, struct system_kstat_data *kstats,
	struct grp_info *grps, struct mem_total *memory_total)
{
	long pagesize = sysconf(_SC_PAGESIZE);
	long npages = sysconf(_SC_PHYS_PAGES);
#ifdef lint
	tree = tree;
	kstats = kstats;
	grps = grps;
	memory_total = memory_total;
#endif

	log_printf("Memory size: ", 0);
	if (pagesize == -1 || npages == -1)
		log_printf("unable to determine\n", 0);
	else {
		long long ii;
		int kbyte = 1024;
		int mbyte = 1024 * 1024;

		ii = (long long) pagesize * npages;
		if (ii >= mbyte)
			log_printf("%d Megabytes\n",
				(int)((ii+mbyte-1) / mbyte), 0);
		else
			log_printf("%d Kilobytes\n",
				(int)((ii+kbyte-1) / kbyte), 0);
	}
}

void
display_memoryconf(Sys_tree *tree, struct grp_info *grps)
{
#ifdef	lint
	tree = tree;
	grps = grps;
#endif
	/*
	 * This function is intentionally blank
	 */
}
