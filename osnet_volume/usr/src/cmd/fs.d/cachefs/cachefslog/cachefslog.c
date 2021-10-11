/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident "@(#)cachefslog.c   1.17     97/11/03 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <varargs.h>
#include <libintl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <kstat.h>
#include <locale.h>
#include <sys/fs/cachefs_log.h>
#include "stats.h"

void usage(char *);
void pr_err(char *, ...);

static int hflag = 0;
static char *fpath = NULL;
static int vflag = 0;
char *prog;

static void log_show(char *, char *);

int
main(int argc, char **argv)
{
	int rc = 0, c;
	int errflg = 0;
	stats_cookie_t *fs = NULL;
	char *logfile;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif /* TEXT_DOMAIN */
	(void) textdomain(TEXT_DOMAIN);

	if (prog = strrchr(argv[0], '/'))
		++prog;
	else
		prog = argv[0];

	while ((c = getopt(argc, argv, "hf:v")) != EOF)
		switch (c) {
		case 'h':
			if (fpath != NULL)
				++errflg;
			else
				++hflag;
			break;

		case 'f':
			if (hflag)
				++errflg;
			else
				fpath = optarg;
			break;

		case 'v':
			++vflag;
			break;

		case '?':
		default:
			++errflg;
			break;
		}

	if ((errflg) || (optind != (argc - 1))) {
		usage(NULL);
		rc = -1;
		goto out;
	}

	fs = stats_create_mountpath(argv[optind], prog);
	if (fs == NULL) {
		pr_err(gettext("Cannot initialize cachefs library\n"));
		rc = 1;
		goto out;
	}

	if (! stats_good(fs)) {
		pr_err(stats_errorstr(fs));
		rc = stats_errno(fs);
		goto out;
	}

	if ((logfile = stats_log_kernel_getname(fs)) == NULL) {
		pr_err(stats_errorstr(fs));
		rc = stats_errno(fs);
		goto out;
	}
	if ((logfile[0] == '\0') && (hflag) && (! vflag)) {
		log_show(argv[optind], logfile);
		goto out;
	}

	if (fpath != NULL) {
		if ((stats_log_kernel_setname(fs, fpath) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_MOUNT, 1) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_UMOUNT, 1) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_REMOVE, 1) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_RMDIR, 1) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_TRUNCATE, 1) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_CREATE, 1) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_MKDIR, 1) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_RENAME, 1) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_SYMLINK, 1) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_UALLOC, 1) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_CSYMLINK, 1) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_FILLDIR, 1) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_MDCREATE, 1) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_NOCACHE, 1) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_CALLOC, 1) != 0) ||
		    (stats_log_which(fs, CACHEFS_LOG_RFDIR, 1) != 0)) {
			pr_err(stats_errorstr(fs));
			rc = stats_errno(fs);
			goto out;
		}
	} else if (hflag) {
		if (stats_log_kernel_setname(fs, NULL) != 0) {
			pr_err(stats_errorstr(fs));
			rc = stats_errno(fs);
			goto out;
		}
	}

	if ((logfile = stats_log_kernel_getname(fs)) == NULL) {
		pr_err(stats_errorstr(fs));
		rc = stats_errno(fs);
		goto out;
	}

	log_show(argv[optind], logfile);

	/*
	 * if they're changing state, inform them of other filesystems
	 * that they're changing state for by way of sharing the
	 * cache.
	 *
	 * or, if they're verbose (-v flag), tell them about the
	 * others.
	 */

	if (((fpath) || (hflag) || (vflag)) && (! stats_inerror(fs))) {
		cachefs_kstat_key_t *k, *origk;
		stats_cookie_t *sc;
		int before = 0;

		origk = stats_getkey(fs);
		sc = stats_create_unbound(prog);
		if (sc == NULL) {
			pr_err(gettext("Cannot create stats object"));
			rc = 1;
			goto out;
		}

		while ((k = stats_next(sc)) != NULL) {
			if (! k->ks_mounted) {
				free(k);
				continue;
			}
			if (strcmp((char *)origk->ks_cachedir,
				(char *)k->ks_cachedir) != 0) {
				free(k);
				continue;
			}
			if (origk->ks_id == k->ks_id) {
				free(k);
				continue;
			}
			if (! before)
				printf("\n");
			before = 1;
			log_show((char *)k->ks_mountpoint, logfile);
			free(k);
		}
		free(origk);
		stats_destroy(sc);
	}

	if (stats_inerror(fs)) {
		pr_err(stats_errorstr(fs));
		rc = stats_errno(fs);
	}

out:
	stats_destroy(fs);
	return (rc);
}

static void
log_show(char *mount, char *logfile)
{
	if (logfile[0] == '\0')
		logfile = gettext("not logged");
	printf("%s: %s\n", logfile, mount);
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
	    gettext("Usage: "
	    "cachefslog [ -v ] [-h | -f <logfile>] mountpoint\n"));
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
	(void) fprintf(stderr, gettext("cachefslog: "));
	(void) vfprintf(stderr, fmt, ap);
	(void) fprintf(stderr, "\n");
	va_end(ap);
}
