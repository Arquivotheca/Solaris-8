/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)util.c 1.8 95/02/28 SMI"

#include <ctype.h>
#include <dirent.h>
#include <libintl.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "dvc.h"
#include "dvc_msgs.h"
#include "conf.h"
#include "util.h"

int dvc_verbose;

void
print_num(char *bfr, int val, int usint)
{
	/* if usint is true then value is an unsigned integer */
	if (val > 15 || usint)
		sprintf(bfr, "0x%x", val);
	else
		sprintf(bfr, "%d", val);
}

void
vrb(const char *fmt, ...)
{
	if (dvc_verbose) {
		va_list ap;
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}

static int
map_case(int c)
{
	if (isupper(c))
		return (c + 'a'-'A');
	return (c);
}

int
match(const char *a, const char *b)
{
	while (*b && (*b != '\n'))
	if (map_case(*a++) != map_case(*b++))
	    return (FALSE);

	return (TRUE);
}

void
error_exit(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	exit(1);
}


int
streq(char *a, char *b)
{
	if (a && b)
		return (strcmp(a, b) == 0);
	return (FALSE);
}

int
strneq(char *a, char *b, size_t n)
{
	if (a && b)
		return (strncmp(a, b, n) == 0);
	return (FALSE);

}

char *
make_filename(char *dir, char *name)
{
	if (*(dir+strlen(dir)-1) != '/')
		return (strcats(dir, "/", name, NULL));
	return (strcats(dir, name, NULL));
}

int
scan_dir(char *dirname, char *matchstring, void (*action)(char *, char *))
{
	int		cnt = 0;
	struct dirent *d;
	DIR *dp;
	char *re;

	dp = opendir(dirname);
	if (dp == NULL)
		return (0);
	re = regcmp(matchstring, NULL);

	while (d = readdir(dp)) {
		if (regex(re, d->d_name)) {
			char *fullpath = make_filename(dirname, d->d_name);
			++cnt;
			action(fullpath, d->d_name);
			xfree(fullpath);
		}
	}

	free(re);
	closedir(dp);

	return (cnt);
}

char *
dvc_home(void)
{
	static char *dir;

	if (dir)
		return (dir);

	/*
	 * Use DEVCONFIGHOME if it is set.  Otherwise look for
	 * /usr/lib/devconfig first and then /etc/devconfig.
	 */
	dir = getenv("DEVCONFIGHOME");
	if (!dir) {
		dir = "/usr/lib/devconfig";
		if (access(dir, F_OK))
			dir = NULL;
	}
	if (!dir)
		dir = "/etc/devconfig";
	return (dir);
}

void
no_def_notice(char *v_name)
{
	char *bfr;
	bfr = strcats(MSG(DEFERR), v_name, MSG(INCONFFILE), NULL);
	ui_notice(bfr);
	xfree(bfr);
}

char *
expand_abbr(char *text)
{
	char *abbr;
	static conf_list_t *cf;

	if (cf == NULL) {
		char *filename;
		FILE *fp;

		filename = strcats(dvc_home(), "/abbreviations", NULL);
		fp = fopen(filename, "r");
		if (fp == NULL)
			ui_error_exit(MSG(NOABBREV));
		cf = read_conf(fp);
		fclose(fp);
		xfree(filename);
	}

	abbr = find_attr_str(cf->alist, text);

	if (abbr == NULL)
		return (text);

	return (abbr);
}
