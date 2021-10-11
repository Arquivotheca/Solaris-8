/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)exception_list.c	1.1	99/01/11 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "exception_list.h"
#include "arch.h"


#define	FS	" \t\n"

static int
parse_exception_line(char *line, elem_list *list)
{
	char	*name, *arch;
	elem	*e;

	if ((name = strtok(line, FS)) == NULL) {
		(void) fprintf(stderr,
		    "error: Null line found in exception file\n");
		return (0);
	}

	if ((arch = strtok(NULL, FS)) == NULL) {
		(void) fprintf(stderr,
		    "error: no arch field for %s entry in exception file\n",
		    name);
		return (0);
	}

	e = (elem *) malloc(sizeof (elem));

	e->inode = 0;
	e->perm = 0;
	e->ref_cnt = 0;
	e->flag = 0;
	e->major = 0;
	e->minor = 0;
	e->link_parent = NULL;
	e->link_sib = NULL;
	e->symsrc = NULL;
	e->file_type = DIR_T;

	if ((e->arch = assign_arch(arch)) == NULL) {
		(void) fprintf(stderr,
		    "warning: Unknown architecture %s found in "
		    "exception file\n", arch);
		return (0);
	}

	(void) strcpy(e->name, name);
	add_elem(list, e);

	return (1);
}

int
read_in_exceptions(char *exception_file, elem_list *list, int verbose)
{
	FILE	*except_fp;
	char	buf[BUFSIZ];
	int	count = 0;

	exception_file = exception_file ? exception_file : EXCEPTION_FILE;

	if (verbose) {
		(void) printf("reading in exceptions from %s...\n",
		    exception_file);
	}

	if ((except_fp = fopen(exception_file, "r")) == NULL) {
		perror(exception_file);
		return (0);
	}
	while (fgets(buf, BUFSIZ, except_fp)) {
		if (buf[0] != '#')	/* allow for comments */
			count += parse_exception_line(buf, list);
	}
	if (verbose)
		(void) printf("read in %d exceptions...\n", count);

	return (count);
}
