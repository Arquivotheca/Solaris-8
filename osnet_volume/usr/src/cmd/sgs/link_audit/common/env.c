/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)env.c	1.4	97/07/28 SMI"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "env.h"

static const char *	token = ":,";

void
build_env_list(Elist ** list, const char * env)
{
	char * envstr;
	char * tok;
	if ((envstr = getenv(env)) == NULL)
		return;
	envstr = strdup(envstr);
	tok = strtok(envstr, token);
	while (tok) {
		Elist *	lp;
		if ((lp = (Elist *)malloc(sizeof (Elist))) == 0) {
			(void) printf("build_list: malloc failed\n");
			exit(1);
		}
		lp->l_libname = strdup(tok);
		lp->l_next = *list;
		*list = lp;
		tok = strtok(NULL, (const char *)token);
	}
	free(envstr);
}


Elist *
check_list(Elist * list, const char * str)
{
	if (list == NULL)
		return (NULL);
	for (; list; list = list->l_next)
		if (strcmp(str, list->l_libname) == 0)
			return (list);
	return (NULL);
}

char *
checkenv(const char * env)
{
	char * envstr;
	if ((envstr = getenv(env)) == NULL)
		return (NULL);
	while (*envstr == ' ')
		envstr++;
	if (*envstr == '\0')
		return (NULL);
	return (envstr);
}
