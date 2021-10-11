/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident "@(#)subr.c   1.4     96/02/23 SMI"

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <stdlib.h>

#include "rules.h"

char *gettext(const char *);

char *
get_fname(char *fullpath)
{
	static char buf[MAXPATHLEN];
	char *s;
	int len;

	strcpy(buf, fullpath);
	len = strlen(buf);
	if (len == 1) {
		return ((char *) NULL);
	}
	if (buf[len-1] == '/')
		buf[len-1] = (char)0;
	s = strrchr(buf, '/');
	if (s != (char *)0) {
		s++;
		return (s);
	}
	return ((char *) NULL);
}

char *
get_dirname(char *fullpath)
{
	static char buf[MAXPATHLEN];
	char *s;
	int len;

	strcpy(buf, fullpath);
	len = strlen(buf);
	if (len == 1)
		return (buf);
	if (buf[len-1] == '/')
		buf[len-1] = '\0';
	s = strrchr(buf, '/');
	if (s != (char *)0) {
		if (s != buf) {
			*s = '\0';
		} else {
			s++;
			*s = '\0';
		}
		return (buf);
	}
	return ((char *) NULL);
}

FILE *
open_rulesfile()
{
	int pid;
	char rulesnam[MAXPATHLEN];
	FILE *rfd;
	int err;

	pid = getpid();

#ifdef CFS_PK_CURD
	/*
	 * Try to creat file in current directory
	 */
	sprintf(rulesnam, "./%s.%d", TMPRULES, pid);
	rfd = fopen(rulesnam, "w");
	if (rfd != NULL) fclose(rfd);
	rfd = fopen(rulesnam, "r+");
	if (rfd != NULL) {
#ifdef DEBUG
		printf("open_rulesfile: tmp rules file = %s\n", rulesnam);
#endif /* DEBUG */
		goto unlink;
	}
#endif /* CFS_PK_CURD */

	/*
	 * try to create file in /tmp directory
	 */
	sprintf(rulesnam, "/tmp/%s.%d", TMPRULES, pid);
	rfd = fopen(rulesnam, "w");
	if (rfd != NULL) fclose(rfd);
	rfd = fopen(rulesnam, "r+");
	if (rfd != NULL) {
#ifdef DEBUG
		printf("open_rulesfile: tmp rules file = %s\n", rulesnam);
#endif /* DEBUG */
		goto unlink;
	}
	perror("cachefspack: Can't open packing rules file\n");
	exit(1);

unlink:
#ifndef DEBUG
	err = unlink(rulesnam);
	if (err < 0) {
		perror("error unlinking temporary packing rules file");
		exit(1);
	}
#endif /* ! DEBUG */

	return (rfd);
}

/*
 * mstrdup - my strdup
 *
 * This is done so there is common error processing for all strdup(s).
 */
char *
mstrdup(const char *str)
{
	char *s;

	s = strdup(str);
	if (s == (char *)0) {
		fprintf(stderr, gettext("strdup failed - no space"));
		exit(1);
	}
	return (s);
}

/*
 * mmalloc - my malloc
 *
 * This is done so there is common error processing for all malloc(s).
 */
void *
mmalloc(size_t size)
{
	void *p;

	p = malloc(size);
	if (p == NULL) {
		fprintf(stderr, gettext("malloc  failed - no space"));
		exit(1);
	}
	return (p);
}
