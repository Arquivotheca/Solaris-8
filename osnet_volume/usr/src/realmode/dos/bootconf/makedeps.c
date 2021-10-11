/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * makedeps.c -- build *.obj :: *.h dependencies for DOS makefiles
 */

#ident	"<@(#)makedeps.c	1.5	97/03/25 SMI>"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "dir.h"
static FILE *op;

static int
makedeps(char *file)
{
	/*
	 *  Builds (no more than) a single dependency line for the given
	 *  C source "file".  Returns a non-zero value if it doesn't work!
	 */

	FILE *ip;

	if (ip = fopen(file, "r")) {
		/*
		 *  Source file opened successfully, search for #include's!
		 */

		int name = 0;
		char obj[32];
		char *xp, *cp = obj;
		static char buf[1024];

		for (xp = file; *xp && (*xp != '.'); xp++) *cp++ = tolower(*xp);
		strcpy(cp, ".obj");

		while (fgets(buf, sizeof (buf), ip)) {
			/*
			 *  Read each line of the file, looking for #include's
			 *  beginning in column 1.
			 */

			if (strncmp(buf, "#include", 8) == 0) {
				/*
				 *  Found another #include, but we're only
				 *  interested in files enclosed in quotes!
				 */

				cp = &buf[8];
				while (*cp && isspace(*cp)) cp++;

				if (*cp == '"') {
					/*
					 *  Here's a dependency; write it to
					 *  the output file!
					 */

					for (xp = ++cp; *xp != '"'; xp++);
					*xp = 0;

					if (!name++) fprintf(op, "%s:", obj);
					fprintf(op, " %s", cp);
				}
			}
		}

		if (name) fputc('\n', op);
		fclose(ip);
		return (0);
	}

	perror(file);
	return (1);
}

main(int argc, char **argv)
{
	/*
	 *  DOS makefile dependency builder:
	 *
	 *  Since the DOS version of make does not support a ".KEEP_STATE"
	 *  feature, we use this program to generate the header file de-
	 *  pendencies.  It's pretty stupid.  All we do is read each *.c
	 *  file on the command line looking for "#include" directives naming
	 *  a *.h file enclosed in double quotes, which we assume marks a
	 *  dependency.  We don't handle nested includes!!
	 */

	int xc = 0;

	if (!--argc) {
		/*  We need file names! */
		fprintf(stderr, "usage: makedefs source-file [...]\n");
		exit(10);
	}

	if (!(op = fopen("makedeps.inc", "w"))) {
		/* Can't open output file! */
		perror("makedeps.inc");
		exit(20);
	}

	fprintf(op, "#    *** Generated automatically by makedeps ***\n");
	fprintf(op, "#    ***            Do not modify!           ***\n\n");

	while (argc--) {
		/*
		 *  For each file name listed on the comand line ...
		 */

		char *cp;

		if ((strcmp(*++argv, "*.c") == 0) ||
					    (strcmp(*argv, "*.C") == 0)) {
			/*
			 *  If we're globbing, do all C files in the
			 *  current directory!
			 */

			struct dirent *dep;
			DIR *dp = opendir(".");

			while (dep = readdir(dp)) {

				if (((cp = strrchr(dep->d_name, '.')) != 0) &&
				    ((strcmp(cp, ".c") == 0) ||
					    (strcmp(cp, ".C") == 0))) {

					xc |= makedeps(dep->d_name);
				}
			}

			closedir(dp);

		} else if (((cp = strrchr(*argv, '.')) != 0) &&
		    ((strcmp(cp, ".c") == 0) || (strcmp(cp, ".C") == 0))) {
			/*
			 *  Otherwise, check to see if the file has a ".c"
			 *  suffix.  If so, generate a dependency line for
			 *  it.
			 */

			xc |= makedeps(*argv);
		}
	}

	fclose(op);
	return (xc);
}
