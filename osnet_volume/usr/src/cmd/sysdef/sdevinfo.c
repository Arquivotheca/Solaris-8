/*
 * Copyright (c) 1990 - 1997, Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)sdevinfo.c	1.16	97/11/13 SMI"

/*
 * For machines that support the openprom, fetch and print the list
 * of devices that the kernel has fetched from the prom or conjured up.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <libdevinfo.h>

static char *progname = "sysdef";
extern int devflag;		/* SunOS4.x devinfo compatible output */
extern int drvname_flag;	/* print out the driver name too */

static int _error(char *opt_noperror, ...);
static int dump_node(di_node_t node, void *arg);

void
sysdef_devinfo(void)
{
	di_node_t root_node;

	/* take a snapshot of kernel devinfo tree */
	if ((root_node = di_init("/", DINFOSUBTREE)) == DI_NODE_NIL) {
		exit(_error("di_init() failed."));
	}

	/*
	 * ...and call di_walk_node to report it out...
	 */
	di_walk_node(root_node, DI_WALK_CLDFIRST, NULL, dump_node);

	di_fini(root_node);
}

/*
 * print out information about this node
 */
static int
dump_node(di_node_t node, void *arg)
{
	int i;
	char *driver_name;
	di_node_t tmp;
	int indent_level = 0;

	/* find indent level */
	tmp = node;
	while ((tmp = di_parent_node(tmp)) != DI_NODE_NIL)
		indent_level++;

	/* we would start at 0, except that we skip the root node */
	if (!devflag)
		indent_level--;

	for (i = 0; i < indent_level; i++)
		(void) putchar('\t');

	if (indent_level >= 0) {
		if (devflag) {
			/*
			 * 4.x devinfo(8) compatible..
			 */
			(void) printf("Node '%s', unit #%d",
				di_node_name(node),
				di_instance(node));
			if (drvname_flag) {
				if (driver_name = di_driver_name(node)) {
					(void) printf(" (driver name: %s)",
					    driver_name);
				}
			} else if (di_state(node) & DI_DRIVER_DETACHED) {
				(void) printf(" (no driver)");
			}
		} else {
			/*
			 * prtconf(1M) compatible..
			 */
			(void) printf("%s", di_node_name(node));
			if (di_instance(node) >= 0)
				(void) printf(", instance #%d",
				    di_instance(node));
			if (drvname_flag) {
				if (driver_name = di_driver_name(node)) {
					(void) printf(" (driver name: %s)",
					    driver_name);
				}
			} else if (di_state(node) & DI_DRIVER_DETACHED) {
				(void) printf(" (driver not attached)");
			}
		}
		(void) printf("\n");
	}
	return (DI_WALK_CONTINUE);
}

/*
 * utility routines
 */

/* _error([no_perror, ] fmt [, arg ...]) */
static int
_error(char *opt_noperror, ...)
{
	int saved_errno;
	va_list ap;
	int no_perror = 0;
	char *fmt;
	extern int errno, _doprnt();

	saved_errno = errno;

	if (progname)
		(void) fprintf(stderr, "%s: ", progname);

	va_start(ap, opt_noperror);
	if (opt_noperror == NULL) {
		no_perror = 1;
		fmt = va_arg(ap, char *);
	} else
		fmt = opt_noperror;
	(void) _doprnt(fmt, ap, stderr);
	va_end(ap);

	if (no_perror)
		(void) fprintf(stderr, "\n");
	else {
		(void) fprintf(stderr, ": ");
		errno = saved_errno;
		perror("");
	}

	return (1);
}
