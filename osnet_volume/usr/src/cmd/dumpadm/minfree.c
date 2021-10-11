/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)minfree.c	1.1	98/05/01 SMI"

#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "utils.h"

static FILE *
minfree_open(const char *dir, int oflags, const char *fmode)
{
	char path[MAXPATHLEN];
	int fd;

	(void) snprintf(path, sizeof (path), "%s/minfree", dir);

	if ((fd = open(path, oflags, S_IRUSR | S_IWUSR)) >= 0)
		return (fdopen(fd, fmode));

	return (NULL);
}

int
minfree_read(const char *dir, unsigned long long *ullp)
{
	FILE *fp = minfree_open(dir, O_RDONLY, "r");

	if (fp != NULL) {
		char buf[BUFSIZ];
		int status = -1;

		if (fgets(buf, BUFSIZ, fp) != NULL) {
			if (valid_str2ull(buf, ullp))
				status = 0;
			else
				warn(gettext("\"%s/minfree\": invalid minfree "
				    "value -- %s\n"), dir, buf);
		}

		(void) fclose(fp);
		return (status);
	}

	return (-1);
}

int
minfree_write(const char *dir, unsigned long long ull)
{
	FILE *fp = minfree_open(dir, O_WRONLY | O_CREAT | O_TRUNC, "w");

	if (fp != NULL) {
		int status = fprintf(fp, "%llu\n", ull);
		(void) fclose(fp);
		return (status);
	}

	return (-1);
}

int
minfree_compute(const char *dir, char *s, unsigned long long *ullp)
{
	size_t len = strlen(s);
	unsigned long long m = 1;

	struct statvfs fsb;
	int pct;

	switch (s[len - 1]) {
	case '%':
		s[len - 1] = '\0';

		if (!valid_str2int(s, &pct) || pct > 100) {
			warn(gettext("invalid minfree %% -- %s\n"), s);
			return (-1);
		}

		if (statvfs(dir, &fsb) == -1) {
			warn(gettext("failed to statvfs %s"), dir);
			return (-1);
		}

		*ullp = fsb.f_blocks * fsb.f_frsize *
		    (u_longlong_t)pct / 100ULL / 1024ULL;

		return (0);

	case 'm':
	case 'M':
		m = 1024ULL;
		/*FALLTHRU*/

	case 'k':
	case 'K':
		s[len - 1] = '\0';

		if (valid_str2ull(s, ullp)) {
			*ullp *= m;
			return (0);
		}

		warn(gettext("invalid minfree value -- %s\n"), s);
		return (-1);

	default:
		warn(gettext("expected m, k, or %% unit after "
		    "minfree -- %s\n"), s);
		return (-1);
	}
}
