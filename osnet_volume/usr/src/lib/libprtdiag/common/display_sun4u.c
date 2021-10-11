/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)display_sun4u.c	1.2	99/10/19 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <kvm.h>
#include <varargs.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/openpromio.h>
#include <libintl.h>
#include <syslog.h>
#include <sys/dkio.h>
#include "pdevinfo.h"
#include "display.h"
#include "pdevinfo_sun4u.h"
#include "display_sun4u.h"
#include "libprtdiag.h"


extern	int sys_clk;

int
display(Sys_tree *tree,
	Prom_node *root,
	struct system_kstat_data *kstats,
	int syserrlog)
{
	int exit_code = 0;	/* init to all OK */
	void *value;		/* used for opaque PROM data */
	struct mem_total memory_total;	/* Total memory in system */
	struct grp_info grps;	/* Info on all groups in system */

	sys_clk = -1;  /* System clock freq. (in MHz) */

	/*
	 * silently check for any types of machine errors
	 */
	exit_code = error_check(tree, kstats);

	/*
	 * Now display the machine's configuration. We do this if we
	 * are not logging or exit_code is set (machine is broke).
	 */
	if (!logging || exit_code) {
		struct utsname uts_buf;

		/*
		 * Display system banner
		 */
		(void) uname(&uts_buf);

		log_printf(
			gettext("System Configuration:  Sun Microsystems"
			"  %s %s\n"), uts_buf.machine,
				get_prop_val(find_prop(root, "banner-name")),
				0);

		/* display system clock frequency */
		value = get_prop_val(find_prop(root, "clock-frequency"));
		if (value != NULL) {
			sys_clk = ((*((int *)value)) + 500000) / 1000000;
			log_printf(gettext("System clock frequency: "
				"%d MHz\n"), sys_clk, 0);
		}

		/* Display the Memory Size */
		display_memorysize(tree, kstats, &grps, &memory_total);


		/* Display the CPU devices */
		display_cpu_devices(tree);

		/* Display the Memory configuration */
		display_memoryconf(tree, &grps);

		/* Display all the IO cards. */
		(void) display_io_devices(tree);


		/*
		 * Display any Hot plugged, disabled and failed board(s)
		 * where appropriate.
		 */
		display_hp_fail_fault(tree, kstats);

		display_diaginfo((syserrlog || (logging && exit_code)),
			root, tree, kstats);
	}

	return (exit_code);
}


int
error_check(Sys_tree *tree, struct system_kstat_data *kstats)
{
#ifdef	lint
	tree = tree;
	kstats = kstats;
#endif
	/*
	 * This function is intentionally empty
	 */
	return (0);
}

int
disp_fail_parts(Sys_tree *tree)
{
#ifdef	lint
	tree = tree;
#endif
	/*
	 * This function is intentionally empty
	 */
	return (0);
}


void
display_hp_fail_fault(Sys_tree *tree, struct system_kstat_data *kstats)
{
#ifdef	lint
	tree = tree;
	kstats = kstats;
#endif
	/*
	 * This function is intentionally empty
	 */
}

void
display_diaginfo(int flag, Prom_node *root, Sys_tree *tree,
			struct system_kstat_data *kstats)
{
#ifdef	lint
	flag = flag;
	root = root;
	tree = tree;
	kstats = kstats;
#endif
	/*
	 * This function is intentionally empty
	 */
}


void
resolve_board_types(Sys_tree *tree)
{
#ifdef	lint
	tree = tree;
#endif
	/*
	 * This function is intentionally empty
	 */
}

void
display_boardnum(int num)
{
	log_printf("%2d   ", num, 0);
}
