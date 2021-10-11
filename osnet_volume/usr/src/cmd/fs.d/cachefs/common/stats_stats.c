/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident "@(#)stats_stats.c   1.12     97/12/06 SMI"

/*
 *
 *			stats_stats.c
 *
 * Routines for the `clean interface' to cachefs statistics.
 */

#include <varargs.h>
#include <libintl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/fs/cachefs_fs.h>
#include "stats.h"

static kstat_t *
stats_read_stat(stats_cookie_t *st)
{
	kstat_t *stat;

	assert(stats_good(st));
	assert(st->st_flags & ST_BOUND);

	if (((stat = kstat_lookup(st->st_kstat_cookie,
	    "cachefs", st->st_fsid, "stats")) == NULL) ||
	    (kstat_read(st->st_kstat_cookie, stat, NULL) < 0)) {
		stats_perror(st, SE_KERNEL,
		    gettext("Cannot lookup statistics"),
		    st->st_fsid);
		goto out;
	}
out:
	return (stat);
}

u_int
stats_hits(stats_cookie_t *st)
{
	kstat_t *ks;
	cachefs_stats_t *stats;
	u_int rc = 0;

	if ((ks = stats_read_stat(st)) != NULL) {
		stats = (cachefs_stats_t *) ks->ks_data;
		rc = stats->st_hits;
	} else {
		stats_perror(st, SE_KERNEL,
		    gettext("Cannot read statistics"));
	}

	return (rc);
}

u_int
stats_misses(stats_cookie_t *st)
{
	kstat_t *ks;
	cachefs_stats_t *stats;
	u_int rc = 0;

	if ((ks = stats_read_stat(st)) != NULL) {
		stats = (cachefs_stats_t *) ks->ks_data;
		rc = stats->st_misses;
	} else {
		stats_perror(st, SE_KERNEL,
		    gettext("Cannot read statistics"));
	}

	return (rc);
}

u_int
stats_passes(stats_cookie_t *st)
{
	kstat_t *ks;
	cachefs_stats_t *stats;
	u_int rc = 0;

	if ((ks = stats_read_stat(st)) != NULL) {
		stats = (cachefs_stats_t *) ks->ks_data;
		rc = stats->st_passes;
	} else {
		stats_perror(st, SE_KERNEL,
		    gettext("Cannot read statistics"));
	}

	return (rc);
}

u_int
stats_fails(stats_cookie_t *st)
{
	kstat_t *ks;
	cachefs_stats_t *stats;
	u_int rc = 0;

	if ((ks = stats_read_stat(st)) != NULL) {
		stats = (cachefs_stats_t *) ks->ks_data;
		rc = stats->st_fails;
	} else {
		stats_perror(st, SE_KERNEL,
		    gettext("Cannot read statistics"));
	}

	return (rc);
}

u_int
stats_modifies(stats_cookie_t *st)
{
	kstat_t *ks;
	cachefs_stats_t *stats;
	u_int rc = 0;

	if ((ks = stats_read_stat(st)) != NULL) {
		stats = (cachefs_stats_t *) ks->ks_data;
		rc = stats->st_modifies;
	} else {
		stats_perror(st, SE_KERNEL,
		    gettext("Cannot read statistics"));
	}

	return (rc);
}

u_int
stats_gc_count(stats_cookie_t *st)
{
	kstat_t *ks;
	cachefs_stats_t *stats;
	u_int rc = 0;

	if ((ks = stats_read_stat(st)) != NULL) {
		stats = (cachefs_stats_t *) ks->ks_data;
		rc = stats->st_gc_count;
	} else {
		stats_perror(st, SE_KERNEL,
		    gettext("Cannot read statistics"));
	}

	return (rc);
}

time_t
stats_gc_time(stats_cookie_t *st)
{
	kstat_t *ks;
	cachefs_stats_t *stats;
	time_t rc = 0;

	if ((ks = stats_read_stat(st)) != NULL) {
		stats = (cachefs_stats_t *) ks->ks_data;
		rc = stats->st_gc_time;
	} else {
		stats_perror(st, SE_KERNEL,
		    gettext("Cannot read statistics"));
	}

	return (rc);
}

time_t
stats_gc_before(stats_cookie_t *st)
{
	kstat_t *ks;
	cachefs_stats_t *stats;
	time_t rc = 0;

	if ((ks = stats_read_stat(st)) != NULL) {
		stats = (cachefs_stats_t *) ks->ks_data;
		rc = stats->st_gc_before_atime;
	} else {
		stats_perror(st, SE_KERNEL,
		    gettext("Cannot read statistics"));
	}

	return (rc);
}

time_t
stats_gc_after(stats_cookie_t *st)
{
	kstat_t *ks;
	cachefs_stats_t *stats;
	time_t rc = 0;

	if ((ks = stats_read_stat(st)) != NULL) {
		stats = (cachefs_stats_t *) ks->ks_data;
		rc = stats->st_gc_after_atime;
	} else {
		stats_perror(st, SE_KERNEL,
		    gettext("Cannot read statistics"));
	}

	return (rc);
}

int
stats_zero_stats(stats_cookie_t *st)
{
	cachefs_stats_t stats;
	kstat_t *ks;
	int rc = 0;
	void *memset();

	assert(stats_good(st));
	assert(st->st_flags & ST_BOUND);

	if ((ks = stats_read_stat(st)) == NULL) {
		rc = -1;
		goto out;
	}

	memset(&stats, '\0', sizeof (stats));
	ks->ks_data = &stats;

	if (kstat_write(st->st_kstat_cookie, ks, NULL) < 0) {
		stats_perror(st, SE_KERNEL,
		    gettext("Cannot zero statistics"));
		rc = -1;
		goto out;
	}

out:
	return (rc);
}
