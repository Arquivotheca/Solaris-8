/*
 *
 *			cfsd_subr.h
 *
 * Include file for the various common routines.
 */

#ident   "@(#)cfsd_subr.h 1.1     96/02/23 SMI"
/* Copyright (c) 1994 by Sun Microsystems, Inc. */

#ifndef CFSD_SUBR
#define	CFSD_SUBR

void subr_add_mount(cfsd_all_object_t *all_object_p, const char *dirp,
    const char *idp);
void *subr_mount_thread(void *datap);
void subr_cache_setup(cfsd_all_object_t *all_object_p);
int subr_fsck_cache(const char *cachedirp);
void subr_doexec(const char *fstype, char *newargv[], const char *progp);
void pr_err(char *fmt, ...);
char *subr_strdup(const char *strp);

#endif /* CFSD_SUBR */
