/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident "@(#)cachefsstat.c   1.17     97/11/03 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <varargs.h>
#include <libintl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <kstat.h>
#include <locale.h>
#include <sys/fs/cachefs_log.h>
#include "stats.h"

void usage(char *);
void pr_err(char *, ...);

static int zflag;
char *prog;

static void print_stats(stats_cookie_t *, cachefs_kstat_key_t *, int);

int
main(int argc, char **argv)
{
	int rc = 0;
	int i, c, errflg = 0;
	stats_cookie_t *sc = NULL;
	cachefs_kstat_key_t *key;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif /* TEXT_DOMAIN */
	(void) textdomain(TEXT_DOMAIN);

	if (prog = strrchr(argv[0], '/'))
		++prog;
	else
		prog = argv[0];

	while ((c = getopt(argc, argv, "z")) != EOF)
		switch (c) {
		case 'z':
			++zflag;
			break;

		case '?':
		default:
			++errflg;
			break;
		}

	if (errflg) {
		usage(NULL);
		rc = -1;
		goto out;
	}

	/*
	 * handle multiple mountpoints specified on command line
	 */

	for (i = optind; i < argc; i++) {
		if ((sc = stats_create_mountpath(argv[i], prog)) == NULL) {
			pr_err(gettext("Cannot use %s"), argv[i]);
			rc = 1;
			continue;
		}

		if (stats_inerror(sc)) {
			pr_err(stats_errorstr(sc));
			rc = stats_errno(sc);
			continue;
		}
		print_stats(sc, key = stats_getkey(sc), zflag);
		if (stats_inerror(sc)) {
			pr_err(stats_errorstr(sc));
			rc = stats_errno(sc);
		}

		stats_destroy(sc);
		free(key);
	}

	/*
	 * handle the case where no mountpoints were specified,
	 * i.e. show stats for all.
	 */

	if (optind >= argc) {
		sc = stats_create_unbound(prog);

		while ((key = stats_next(sc)) != NULL) {
			if (! key->ks_mounted) {
				free(key);
				continue;
			}

			print_stats(sc, key, zflag);
			if (stats_inerror(sc)) {
				pr_err(stats_errorstr(sc));
				rc = stats_errno(sc);
			}
			free(key);
		}
		stats_destroy(sc);
	}

out:
	return (rc);
}

static void
print_stats(stats_cookie_t *sc, cachefs_kstat_key_t *key, int zero)
{
	u_int hits, misses, passes, fails, modifies;
	u_int hitp, passtotal;
	u_int gccount;

	hits = stats_hits(sc);
	misses = stats_misses(sc);
	if (hits + misses != 0)
		hitp = (100 * hits) / (hits + misses);
	else
		hitp = 100;

	passes = stats_passes(sc);
	fails = stats_fails(sc);
	passtotal = passes + fails;

	modifies = stats_modifies(sc);

	gccount = stats_gc_count(sc);

	printf("\n    %s\n", (char *)key->ks_mountpoint);
	printf(
	    gettext("\t         cache hit rate: %5d%% (%d hits, %d misses)\n"),
	    hitp, hits, misses);
	printf(gettext("\t     consistency checks: %6d (%d pass, %d fail)\n"),
	    passtotal, passes, fails);
	printf(gettext("\t               modifies: %6d\n"), modifies);
	printf(gettext("\t     garbage collection: %6d\n"), gccount);
	if (gccount != 0) {
		time_t gctime = stats_gc_time(sc);
		time_t before = stats_gc_before(sc);
		time_t after = stats_gc_after(sc);

		if (gctime != (time_t) 0)
			printf(gettext("\tlast garbage collection: %s"),
			    ctime(&gctime));
	}

	if (zero)
		(void) stats_zero_stats(sc);
}


/*
 *
 *			usage
 *
 * Description:
 *	Prints a short usage message.
 * Arguments:
 *	msgp	message to include with the usage message
 * Returns:
 * Preconditions: */

void
usage(char *msgp)
{
	if (msgp) {
		pr_err("%s", msgp);
	}

	fprintf(stderr,
	    gettext("Usage: cachefsstat [ -z ] [ path ... ]\n"));
}

/*
 *
 *			pr_err
 *
 * Description:
 *	Prints an error message to stderr.
 * Arguments:
 *	fmt	printf style format
 *	...	arguments for fmt
 * Returns:
 * Preconditions:
 *	precond(fmt)
 */

void
pr_err(char *fmt, ...)
{
	va_list ap;

	va_start(ap);
	(void) fprintf(stderr, gettext("cachefsstat: "));
	(void) vfprintf(stderr, fmt, ap);
	(void) fprintf(stderr, "\n");
	va_end(ap);
}
