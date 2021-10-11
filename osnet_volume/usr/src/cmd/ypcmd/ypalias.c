/*
 * Copyright (c) 1986-1992 by Sun Microsystems Inc.
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include "ypsym.h"

#ifdef SYSVCONFIG
extern void sysvconfig();
#endif

extern int yp_getalias();

/*
 *	Given a domain name, return its system v alias.
 *	If there is no alias name in the alias file,
 *	create one. Rule of creation is to take the 1st
 *	NAME_MAX-4 characters and concatenate the last 4 characters.
 *	If the alias in the file is too long, trim off the end.
 */

static void
mkdomain_alias(name, result)
char *name, *result;
{
	int retval;
	char tmpbuf[MAXNAMLEN] = {NULL};

	retval = yp_getalias(name, result, NAME_MAX);
	if (retval == -1) {
		if ((int)strlen(name) > NAME_MAX) {
			strncpy(result, name, NAME_MAX-4);
			strncpy(&result[NAME_MAX-4],
			    &name[strlen(name)-4], 4);
			result[NAME_MAX] = '\0';
		} else
			strcpy(result, name);
	} else if ((retval) && (int)strlen(result) > NAME_MAX) {
		strncpy(tmpbuf, result, NAME_MAX);
		strcpy(result, tmpbuf);
	}
}

/*
 *	Given a map name, return its system v alias .
 *	If there is no alias name in the alias file,
 *	create one. Rule of creation is to take the 1st
 *	MAXALIASLEN-4 characters and concatenate the last 4 characters.
 *	If the alias in the file is too long, trim off the end.
 */
static void
mkmap_alias(name, result)
char *name, *result;
{
	int retval;
	char tmpbuf[MAXNAMLEN] = {NULL};

	retval = yp_getalias(name, result, MAXALIASLEN);

	if (retval == -1) {
		if ((int)strlen(name) > MAXALIASLEN) {
			(void) strncpy(result, name, MAXALIASLEN-4);
			(void) strncpy(&result[MAXALIASLEN-4],
			    &name[strlen(name)-4], 4);
			result[MAXALIASLEN] = '\0';
		} else
			(void) strcpy(result, name);
	} else if ((retval) && (int)strlen(result) > MAXALIASLEN) {
		(void) strncpy(tmpbuf, result, MAXALIASLEN);
		(void) strcpy(result, tmpbuf);
	}
}

#ifdef MAIN

/*
 * executed only for the command ypalias
 * and not when ypbind or ypserv make use
 * of this file.
 */

static char usage[] =
"Usage:\n\
	ypalias -d domainname\n\
	ypalias mapname\n";

main(argc, argv)
char **argv;
{
	char result[MAXNAMLEN] = {NULL};

#ifdef SYSVCONFIG
	sysvconfig();
#endif
	if (argc <= 1)
		goto err;
	if (strcmp(argv[1], "-d") == 0)
		if (argc == 3) mkdomain_alias(argv[2], (char *)&result);
		else goto err;
	else if (argc == 2)
		mkmap_alias(argv[1], (char *)&result);
	else
		goto err;
	(void) printf("%s", result);
	return (0);
err:
	(void) fprintf(stderr, usage);
	return (1);
}
#endif MAIN
