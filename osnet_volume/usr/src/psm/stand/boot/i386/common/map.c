/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident "@(#)map.c 1.3       97/07/14 SMI"

/* boot shell variable handling routines */

#include <sys/types.h>
#include <sys/salib.h>
#include <sys/filemap.h>

extern void cpfs_add_entry(char *, char *, int);

static void
map_usage(char *progname)
{
	printf("Usage: %s root-path new-path [cpt]\n", progname);
	printf("Where:\n");
	printf("root-path    the path or filename to be over-ridden\n");
	printf("new-path     the path or filename replacing root-path\n\n");
	printf("Optional flags:\n");
	printf("c\tthe file specified by root-path is extended\n");
	printf("\tby the contents of the new-path file.\n");
	printf("p\tthe mapping is strictly a path rewriting\n");
	printf("t\tthe mapping should enforce the unix end-of-line\n");
	printf("\tconvention in any replacement or extension text\n");
	printf("No flag implies a strict replacement of the root-path "
	    "file by the new-path file.\n");
}

void
map_cmd(int argc, char **argv)
{
	char	*ptr = argv[3];
	int	flag = 0;

	if (argc < 3 || argc > 4) {
		map_usage(argv[0]);
		return;
	}

	if (argc == 4) {
		while (ptr && *ptr) {
			if (*ptr == 'c')
				flag |= COMPFS_AUGMENT;
			else if (*ptr == 't')
				flag |= COMPFS_TEXT;
			else if (*ptr == 'p')
				flag |= COMPFS_PATH;
			else {
				map_usage(argv[0]);
				return;
			}
			ptr++;
		}
	}

	cpfs_add_entry(argv[1], argv[2], flag);
}

/*ARGSUSED*/
void
maps_cmd(int argc, char **argv)
{
	cpfs_show_entries();
}
