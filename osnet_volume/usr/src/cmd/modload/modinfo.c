#ident	"@(#)modinfo.c 1.11 98/06/10 SMI"

/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */


#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/modctl.h>
#include <sys/errno.h>

static int wide;
static int count = 0;
static int first_mod = 1;

static char *header   = " Id Loadaddr   Size Info Rev Module Name\n";
static char *cheader  =
	" Id    Loadcnt Module Name                            State\n";

static void usage();
static void print_info(struct modinfo *mi);
static void print_cinfo(struct modinfo *mi);

/*
 * These functions are in modsubr.c
 */
void fatal(char *fmt, ...);
void error(char *fmt, ...);

/*
 * System call prototype
 */
extern int modctl(int, ...);

/*
 * Display information of all loaded modules
 */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct modinfo modinfo;
	int info_all = 1;
	int id;
	int opt;

	id = -1;	/* assume we're getting all loaded modules */

	while ((opt = getopt(argc, argv, "i:wc")) != EOF) {
		switch (opt) {
		case 'i':
			if (sscanf(optarg, "%d", &id) != 1)
				fatal("Invalid id %s\n", optarg);
			if (id == -1)
				id = 0;
			info_all = 0;
			break;
		case 'w':
			wide++;
			break;
		case 'c':
			count++;
			break;
		case '?':
		default:
			usage();
			break;
		}
	}


	/*
	 * Next id of -1 means we're getting info on all modules.
	 */
	modinfo.mi_id = modinfo.mi_nextid = id;
	if (info_all)
		modinfo.mi_info = MI_INFO_ALL;
	else
		modinfo.mi_info = MI_INFO_ONE;

	if (count)
		modinfo.mi_info |= MI_INFO_CNT;

	do {
		/*
		 * Get module information.
		 * If modinfo.mi_nextid == -1, get info about the
		 * next installed module with id > "id."
		 * Otherwise, get info about the module with id == "id."
		 */
		if (modctl(MODINFO, id, &modinfo) < 0) {
			if (!info_all)
				error("can't get module information");
			break;
		}

		if (first_mod) {
			first_mod = 0;
			(void) printf(count ? cheader : header);
		}
		if (count)
			print_cinfo(&modinfo);
		else
			print_info(&modinfo);
		/*
		 * If we're getting info about all modules, the next one
		 * we want is the one with an id greater than this one.
		 */
		id = modinfo.mi_id;
	} while (info_all);

	return (0);
}

/*
 * Display loadcounts.
 */
static void
print_cinfo(struct modinfo *mi)
{
	(void) printf("%3d %10d %-32s", mi->mi_id, mi->mi_loadcnt, mi->mi_name);
	(void) printf(" %s/%s\n",
		    mi->mi_state & MI_LOADED ? "LOADED" : "UNLOADED",
		    mi->mi_state & MI_INSTALLED ? "INSTALLED" : "UNINSTALLED");
}

/*
 * Display info about a loaded module.
 */
static void
print_info(struct modinfo *mi)
{
	register int n;
	register int p0;
	char namebuf[256];

	for (n = 0; n < MODMAXLINK; n++) {
		if (mi->mi_msinfo[n].msi_linkinfo[0] == '\0')
			break;
		(void) printf("%3d %8x %6x ",
			    mi->mi_id, mi->mi_base, mi->mi_size);

		p0 = mi->mi_msinfo[n].msi_p0;

		if (p0 != -1)
			(void) printf("%3d ", p0);
		else
			(void) printf("  - ");

		(void) printf("  %d  ", mi->mi_rev);

		mi->mi_name[MODMAXNAMELEN - 1] = '\0';
		mi->mi_msinfo[n].msi_linkinfo[MODMAXNAMELEN - 1] = '\0';

		if (wide) {
			(void) printf("%s (%s)\n", mi->mi_name,
			    mi->mi_msinfo[n].msi_linkinfo);
		} else {
			/* snprintf(3c) will always append a null character */
			(void) snprintf(namebuf, sizeof (namebuf), "%s (%s)",
					mi->mi_name,
					mi->mi_msinfo[n].msi_linkinfo);
			(void) printf("%.42s\n", namebuf);
		}
	}
}

static void
usage()
{
	fatal("usage:  modinfo [-w] [-c] [-i module-id]\n");
}
