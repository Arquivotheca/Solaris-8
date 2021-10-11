/*
 * Copyright (c) 1996,1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)abienv.c	1.1	99/05/14 SMI"

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <errno.h>
#include <fnmatch.h>
#include <apptrace.h>
#include <libintl.h>
#include "abienv.h"

static char const *strdup_sym = "strdup";
static char const *malloc_sym = "malloc";
static char const *comma = ",";

static void
bugout(char const *call)
{
	(void) fprintf(stderr, gettext("apptrace: %s failed\n"), call);
	exit(EXIT_FAILURE);
}

void
build_env_list(Liblist **list, char const *env)
{
	char *envstr;
	char *tok;

	if ((envstr = getenv(env)) == NULL)
		return;

	if ((envstr = strdup(envstr)) == NULL)
		bugout(strdup_sym);

	tok = strtok(envstr, comma);
	while (tok != NULL) {
		Liblist *lp;

		if ((lp = malloc(sizeof (Liblist))) == NULL)
			bugout(malloc_sym);

		lp->l_libname = tok;
		lp->l_next = *list;
		*list = lp;
		tok = strtok(NULL, comma);
	}
}

void
build_env_list1(Liblist **list, Liblist **listend, const char *env)
{
	char *envstr;
	char *tok;

	if ((envstr = getenv(env)) == NULL)
		return;

	/*
	 * It is possible that we have a single file name,
	 * in which case the subseqent loop will do nothing
	 */
	if (strchr(envstr, ',') == NULL) {
		appendlist(list, listend, envstr, 1);
		return;
	}

	if ((envstr = strdup(envstr)) == NULL)
		bugout(strdup_sym);

	tok = strtok(envstr, comma);
	while (tok != NULL) {
		appendlist(list, listend, tok, 1);
		tok = strtok(NULL, comma);
	}
	free(envstr);
}

void
env_to_intlist(Intlist **list, char const *env)
{
	char *envstr;
	char *tok;

	if ((envstr = getenv(env)) == NULL)
		return;

	if ((envstr = strdup(envstr)) == NULL)
		bugout(strdup_sym);

	for (tok = strtok(envstr, comma);
	    tok != NULL;
	    tok = strtok(NULL, comma)) {

		Intlist *ip;

		if ((ip = malloc(sizeof (Intlist))) == NULL)
			bugout(malloc_sym);

		if ((ip->i_name = strdup(tok)) == NULL)
			bugout(strdup_sym);

		ip->i_next = *list;
		*list = ip;
	}
	free(envstr);
}

void
appendlist(Liblist **list, Liblist **listend, const char *name, int fatal)
{
	Liblist	*lp;
	void	*handle;

	if (access(name, R_OK)) {
		if (fatal) {
			(void) fprintf(stderr, gettext("apptrace: %s: %s\n"),
			    name, strerror(errno));
			exit(EXIT_FAILURE);
		}
		return;
	}

	if ((handle = dlopen(name, RTLD_LAZY)) == NULL) {
		if (fatal) {
			(void) fprintf(stderr,
			    gettext("apptrace: dlopen on %s failed: %s\n"),
			    name, dlerror());
			exit(EXIT_FAILURE);
		}
		return;
	}

	/* OK, so now add it to the end of the list */
	if ((lp = malloc(sizeof (Liblist))) == NULL)
		bugout(malloc_sym);

	if ((lp->l_libname = strdup(name)) == NULL)
		bugout(strdup_sym);
	lp->l_handle = handle;
	lp->l_next = NULL;
	if (*listend)
		(*listend)->l_next = lp;
	if (*list == NULL)
		*list = lp;
	*listend = lp;

	(void) spf_load_stabs(name);
}

/*
 * Called abibasename() to avoid clash with basename(3C)
 * Incidentally, basename(3C) is destructive which is why
 * we are not using it instead.
 */
char *
abibasename(const char *str)
{
	char *p;

	if ((p = strrchr(str, '/')) != NULL)
		return (p + 1);
	else
		return ((char *)str);
}

Liblist *
check_list(Liblist *list, char const *str)
{
	char *basename1, *basename2, *p1, *p2;
	Liblist *ret = NULL;

	if (list == NULL)
		return (NULL);

	if ((basename2 = strdup(abibasename(str))) == NULL)
		bugout(strdup_sym);
	if ((p2 = strchr(basename2, '.')) != NULL)
		*p2 = '\0';

	for (; list; list = list->l_next) {
		/* Lose the dirname */
		if ((basename1 = strdup(abibasename(list->l_libname))) == NULL)
			bugout(strdup_sym);
		/* Lose the suffix */
		if ((p1 = strchr(basename1, '.')) != NULL)
			*p1 = '\0';
		if (fnmatch(basename1, basename2, 0) == 0) {
			ret = list;
			free(basename1);
			break;
		}
		free(basename1);
	}

	free(basename2);
	return (ret);
}

int
check_intlist(Intlist *list, char const *iface)
{
	if (list == NULL)
		return (0);

	for (; list != NULL; list = list->i_next) {
		if (fnmatch(list->i_name, iface, 0) == 0)
			return (1);
	}

	return (0);
}

char *
checkenv(char const *env)
{
	char *envstr;

	if ((envstr = getenv(env)) == NULL)
		return (NULL);
	while (*envstr == ' ')
		envstr++;
	if (*envstr == '\0')
		return (NULL);
	return (envstr);
}

int
build_interceptor_path(char *buf, size_t l, char const *path)
{
	char *p, *t, *f;
	int ret;

	/* Duplicate the path */
	if ((p = strdup(path)) == NULL)
		bugout(strdup_sym);

	/* Find the last slash, if there ain't one bug out */
	if ((t = strrchr(p, '/')) == NULL) {
		ret = 0;
		goto done;
	}

	/*
	 * Wack the slash to a null byte.
	 * Thus if we got:
	 * 	/A/B/C/D.so.1
	 * p now points to /A/B/C
	 * f is set to point to D.so.1
	 */
	*t = '\0';
	f = ++t;

#if defined(_LP64)
	/*
	 * As above except that in LP64 we'll
	 * get:
	 *	/A/B/C/sparcv9/D.so.1
	 * thus p now points to:
	 *	/A/B/C/sparcv9
	 * so we repeat the wack so that we get:
	 *	/A/B/C
	 */
	if ((t = strrchr(p, '/')) == NULL) {
		ret = 0;
		goto done;
	}
	*t = '\0';

	/*
	 * Now we can build a path name.
	 * This path is only a guess that'll be checked later
	 * in appendlist()
	 */
	ret = snprintf(buf, l, "%s/abi/sparcv9/abi_%s", p, f);
#else
	ret = snprintf(buf, l, "%s/abi/abi_%s", p, f);
#endif

done:
	free(p);
	return (ret);
}
