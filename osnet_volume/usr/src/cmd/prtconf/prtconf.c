/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)prtconf.c	1.24	99/05/04 SMI"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <sys/systeminfo.h>
#include "prtconf.h"

struct prt_opts opts;
struct prt_dbg dbg;

#define	INDENT_LENGTH	4

#if defined(i386) || defined(__ia64)
static char *usage = "%s [ -V | -x | -vpPD ]\n";
#else
static char *usage = "%s [ -F | -V | -x | -vpPD ]\n";
#endif

static void
setprogname(const char *name)
{
	char *p;

	if (name == NULL)
		opts.o_progname = "prtconf";
	else if (p = strrchr(name, '/'))
		opts.o_progname = (const char *) p + 1;
	else
		opts.o_progname = name;
}

void
dprintf(const char *fmt, ...)
{
	if (dbg.d_debug) {
		va_list ap;
		va_start(ap, fmt);
		(void) vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}

void
indent_to_level(int ilev)
{
	(void) printf("%*s", INDENT_LENGTH * ilev, "");
}

/*
 * debug version has two more flags:
 *	-L force load driver
 *	-M: print per driver list
 */
#ifdef	DEBUG
static const char *optstring = "DvVxpPFf:M:dL";
#else
static const char *optstring = "DvVxpPFf:";
#endif	/* DEBUG */

int
main(int argc, char *argv[])
{
	longlong_t ii;
	long pagesize, npages;
	int c, ret;
	char hw_provider[SYS_NMLN];

	setprogname(argv[0]);
	opts.o_promdev = "/dev/openprom";

	while ((c = getopt(argc, argv, optstring)) != -1)  {
		switch (c)  {
		case 'D':
			++opts.o_drv_name;
			break;
		case 'v':
			++opts.o_verbose;
			break;
		case 'p':
			++opts.o_prominfo;
			break;
		case 'f':
			opts.o_promdev = optarg;
			break;
		case 'V':
			++opts.o_promversion;
			break;
		case 'x':
			++opts.o_prom_ready64;
			break;
		case 'F':
			++opts.o_fbname;
			++opts.o_noheader;
			break;
		case 'P':
			++opts.o_pseudodevs;
			break;
#ifdef	DEBUG
		case 'M':
			dbg.d_drivername = optarg;
			++dbg.d_bydriver;
			break;
		case 'L':
			++dbg.d_forceload;
			break;
#endif	/* DEBUG */

		default:
			(void) fprintf(stderr, usage, opts.o_progname);
			return (1);
		}
	}

	(void) uname(&opts.o_uts);

	if (opts.o_fbname)
		return (do_fbname());

	if (opts.o_promversion)
		return (do_promversion());

	if (opts.o_prom_ready64)
		return (do_prom_version64());

	ret = sysinfo(SI_HW_PROVIDER, hw_provider, sizeof (hw_provider));
	/*
	 * If 0 bytes are returned (the system returns '1', for the \0),
	 * we're probably on x86, and there has been no si-hw-provider
	 * set in /etc/bootrc, so just default to Sun.
	 */
	if (ret <= 1) {
		(void) strncpy(hw_provider, "Sun Microsystems",
		    sizeof (hw_provider));
	} else {
		/*
		 * Provide backward compatibility by stripping out the _.
		 */
		if (strcmp(hw_provider, "Sun_Microsystems") == 0)
			hw_provider[3] = ' ';
	}
	(void) printf("System Configuration:  %s  %s\n", hw_provider,
	    opts.o_uts.machine);

	pagesize = sysconf(_SC_PAGESIZE);
	npages = sysconf(_SC_PHYS_PAGES);
	(void) printf("Memory size: ");
	if (pagesize == -1 || npages == -1)
		(void) printf("unable to determine\n");
	else {
		const int kbyte = 1024;
		const int mbyte = 1024 * 1024;

		ii = (longlong_t)pagesize * npages;
		if (ii >= mbyte)
			(void) printf("%d Megabytes\n",
				(int)((ii+mbyte-1) / mbyte));
		else
			(void) printf("%d Kilobytes\n",
				(int)((ii+kbyte-1) / kbyte));
	}

	if (opts.o_prominfo) {
		(void) printf("System Peripherals (PROM Nodes):\n\n");
		if (do_prominfo() == 0)
			return (0);
		(void) fprintf(stderr, "%s: Defaulting to non-PROM mode...\n",
		    opts.o_progname);
	}

	(void) printf("System Peripherals (Software Nodes):\n\n");

	(void) prtconf_devinfo();

	return (0);
}
