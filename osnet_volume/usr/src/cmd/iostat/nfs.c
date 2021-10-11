/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nfs.c 1.8	99/09/10 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <sys/mnttab.h>
#include <sys/mntent.h>
#include <sys/sysmacros.h>
#include <sys/mkdev.h>
#include <sys/vfs.h>
#include <nfs/nfs.h>
#include <nfs/nfs_clnt.h>
#include "iostat.h"

extern FILE *mpt;
extern time_t mtime;
extern void init_nfs(void);
extern char *lookup_nfs_name(char *);
extern kstat_ctl_t *kc;
extern mnt_t *nfs;

static char *get_nfs_by_minor(uint_t);
static char *cur_hostname(uint_t);
static char *cur_special(char *, char *);

char *
lookup_nfs_name(char *ks)
{
	uint_t minor;
	char *host, *path;
	char *cp;
	char *rstr = 0;
	size_t len;

	if (sscanf(ks, "nfs%u", &minor) == 1) {
		cp = get_nfs_by_minor(minor);
		if (cp) {
			if (strchr(cp, ',') == NULL) {
				safe_strdup(cp, &rstr);
				return (rstr);
			}
			host = cur_hostname(minor);
			if (host) {
				if (*host) {
					path = cur_special(host, cp);
					if (path) {
						len = strlen(host);
						len += strlen(path);
						len += 2;
						safe_alloc((void **)&rstr,
						    len, 0);
						(void) snprintf(rstr, len,
						    "%s:%s", host, path);
					} else {
						safe_strdup(cp, &rstr);
					}
				} else {
					safe_strdup(ks, &rstr);
				}
				free(host);
			} else {
				safe_strdup(cp, &rstr);
			}
		}
	}
	return (rstr);
}

static char *
get_nfs_by_minor(uint_t minor)
{
	mnt_t *localnfs;

	localnfs = nfs;
	while (localnfs) {
		if (localnfs->minor == minor) {
			return (localnfs->device_name);
		}
		localnfs = localnfs->next;
	}
	return (0);
}

/*
 * Read the cur_hostname from the mntinfo kstat
 */
static char *
cur_hostname(uint_t minor)
{
	kstat_t *ksp;
	static struct mntinfo_kstat mik;
	char *rstr;

	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
		if (ksp->ks_type != KSTAT_TYPE_RAW)
			continue;
		if (ksp->ks_instance != minor)
			continue;
		if (strcmp(ksp->ks_module, "nfs"))
			continue;
		if (strcmp(ksp->ks_name, "mntinfo"))
			continue;
		if (ksp->ks_flags & KSTAT_FLAG_INVALID)
			return (NULL);
		if (kstat_read(kc, ksp, &mik) == -1)
			return (NULL);
		safe_strdup(mik.mik_curserver, &rstr);
		return (rstr);
	}
	return (NULL);
}

/*
 * Given the hostname of the mounted server, extract the server
 * mount point from the mnttab string.
 *
 * Common forms:
 *	server1,server2,server3:/path
 *	server1:/path,server2:/path
 * or a hybrid of the two
 */
static char *
cur_special(char *hostname, char *special)
{
	char *cp;
	char *path;
	size_t hlen = strlen(hostname);

	/*
	 * find hostname in string
	 */
again:
	if ((cp = strstr(special, hostname)) == NULL)
		return (NULL);

	/*
	 * hostname must be followed by ',' or ':'
	 */
	if (cp[hlen] != ',' && cp[hlen] != ':') {
		special = &cp[hlen];
		goto again;
	}

	/*
	 * If hostname is followed by a ',' eat all characters until a ':'
	 */
	cp = &cp[hlen];
	if (*cp == ',') {
		cp++;
		while (*cp != ':') {
			if (*cp == NULL)
				return (NULL);
			cp++;
		}
	}
	path = ++cp;			/* skip ':' */

	/*
	 * path is terminated by either 0, or space or ','
	 */
	while (*cp) {
		if (isspace(*cp) || *cp == ',') {
			*cp = NULL;
			return (path);
		}
		cp++;
	}
	return (path);
}
