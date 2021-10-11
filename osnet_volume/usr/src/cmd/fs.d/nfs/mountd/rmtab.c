/*
 * rmtab.c
 *
 * Copyright (c) 1988,1999 Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)rmtab.c	1.11	98/05/27 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <errno.h>
#include <rpcsvc/mount.h>
#include <sys/pathconf.h>
#include <sys/systeminfo.h>
#include <sys/utsname.h>
#include <signal.h>
#include <locale.h>
#include <unistd.h>
#include <thread.h>
#include <syslog.h>
#include "../lib/sharetab.h"
#include "hashset.h"
#include "mountd.h"

static char RMTAB[] = "/etc/rmtab";
static FILE *rmtabf = NULL;

/*
 * There is nothing magic about the value selected here. Too low,
 * and mountd might spend too much time rewriting the rmtab file.
 * Too high, it won't do it frequently enough.
 */
static int rmtab_del_thresh = 250;

#define	RMTAB_TOOMANY_DELETED()	\
	((rmtab_deleted > rmtab_del_thresh) && (rmtab_deleted > rmtab_inuse))

/*
 * mountd's version of a "struct mountlist". It is the same except
 * for the added ml_pos field.
 */
struct mntentry {
	char  *m_host;
	char  *m_path;
	long   m_pos;
};

static HASHSET mntlist;

static int mntentry_equal(const void *, const void *);
static uint32_t mntentry_hash(const void *);
static int mntlist_contains(char *, char *);
static void rmtab_delete(long);
static long rmtab_insert(char *, char *);
static void rmtab_rewrite(void);
static void rmtab_parse(char *buf);
static bool_t xdr_mntlistencode(XDR * xdrs, HASHSET * mntlist);

#define	exstrdup(s) \
	strcpy(exmalloc(strlen(s)+1), s)


static int rmtab_inuse;
static int rmtab_deleted;

static rwlock_t rmtab_lock;	/* lock to protect rmtab list */


/*
 * Check whether the given client/path combination
 * already appears in the mount list.
 */

static int
mntlist_contains(char *host, char *path)
{
	struct mntentry m;

	m.m_host = host;
	m.m_path = path;

	return (h_get(mntlist, &m) != NULL);
}


/*
 *  Add an entry to the mount list.
 *  First check whether it's there already - the client
 *  may have crashed and be rebooting.
 */

static void
mntlist_insert(char *host, char *path)
{
	if (!mntlist_contains(host, path)) {
		struct mntentry *m;

		m = exmalloc(sizeof (struct mntentry));

		m->m_host = exstrdup(host);
		m->m_path = exstrdup(path);
		m->m_pos = rmtab_insert(host, path);
		(void) h_put(mntlist, m);
	}
}

void
mntlist_new(char *host, char *path)
{
	(void) rw_wrlock(&rmtab_lock);
	mntlist_insert(host, path);
	(void) rw_unlock(&rmtab_lock);
}

/*
 * Delete an entry from the mount list.
 */

void
mntlist_delete(char *host, char *path)
{
	struct mntentry *m, mm;

	mm.m_host = host;
	mm.m_path = path;

	(void) rw_wrlock(&rmtab_lock);

	if (m = (struct mntentry *)h_get(mntlist, &mm)) {
		rmtab_delete(m->m_pos);

		(void) h_delete(mntlist, m);

		free(m->m_path);
		free(m->m_host);
		free(m);

		if (RMTAB_TOOMANY_DELETED())
			rmtab_rewrite();
	}
	(void) rw_unlock(&rmtab_lock);
}

/*
 * Delete all entries for a host from the mount list
 */

void
mntlist_delete_all(char *host)
{
	HASHSET_ITERATOR iterator;
	struct mntentry *m;

	(void) rw_wrlock(&rmtab_lock);

	iterator = h_iterator(mntlist);

	while (m = (struct mntentry *)h_next(iterator)) {
		if (strcmp(m->m_host, host))
			continue;

		rmtab_delete(m->m_pos);

		(void) h_delete(mntlist, m);

		free(m->m_path);
		free(m->m_host);
		free(m);
	}

	if (RMTAB_TOOMANY_DELETED())
		rmtab_rewrite();

	(void) rw_unlock(&rmtab_lock);
}

/*
 * Equivalent to xdr_mountlist from librpcsvc but for HASHSET
 * rather that for a linked list. It is used only to encode data
 * from HASHSET before sending it over the wire.
 */

static bool_t
xdr_mntlistencode(XDR * xdrs, HASHSET * mntlist)
{
	HASHSET_ITERATOR iterator = h_iterator(*mntlist);

	for (;;) {
		struct mntentry *m = (struct mntentry *)h_next(iterator);
		bool_t more_data = (m != NULL);

		if (!xdr_bool(xdrs, &more_data))
			return (FALSE);

		if (!more_data)
			break;

		if (!xdr_name(xdrs, &m->m_host))
			return (FALSE);

		if (!xdr_dirpath(xdrs, &m->m_path))
			return (FALSE);
	}

	return (TRUE);
}

void
mntlist_send(SVCXPRT * transp)
{
	(void) rw_rdlock(&rmtab_lock);

	errno = 0;
	if (!svc_sendreply(transp, xdr_mntlistencode, (char *)&mntlist))
		log_cant_reply(transp);

	(void) rw_unlock(&rmtab_lock);
}

/*
 * Compute a hash for an mntlist entry.
 */
#define	SPACE		(uchar_t)0x20
#define	HASH_BITS	3	/* Num. of bits to shift the hash  */
#define	HASH_MAXHOST	6	/* Max. number of characters from a hostname */
#define	HASH_MAXPATH	3	/* Max. number of characters from a pathname */

/*
 * Compute a 32 bit hash value for an mntlist entry.
 * We consider only first HASH_MAXHOST characters from the host part.
 * We skip the first character in the path part (usually /), and we
 * consider at most HASH_MAXPATH following characters.
 * We shift the hash value by HASH_BITS after each character.
 */

static uint32_t
mntentry_hash(const void *p)
{
	struct mntentry *m = (struct mntentry *)p;
	uchar_t *s;
	uint_t i, sum = 0;

	for (i = 0, s = (uchar_t *)m->m_host; *s && i < HASH_MAXHOST; i++) {
		sum <<= HASH_BITS;
		sum += *s++ - SPACE;
	}

	/*
	 * The first character is usually '/'.
	 * Start with the next character.
	 */
	for (i = 0, s = (uchar_t *)m->m_path+1; *s && i < HASH_MAXPATH; i++) {
		sum <<= HASH_BITS;
		sum += *s++ - SPACE;
	}

	return (sum);
}

/*
 * Compare mntlist entries.
 * The comparison ignores a value of m_pos.
 */

static int
mntentry_equal(const void *p1, const void *p2)
{
	struct mntentry *m1 = (struct mntentry *)p1;
	struct mntentry *m2 = (struct mntentry *)p2;

	return ((strcmp(m1->m_host, m2->m_host) ||
		strcmp(m1->m_path, m2->m_path)) ? 0 : 1);
}

/*
 * Rewrite /etc/rmtab with a current content of mntlist.
 */
static void
rmtab_rewrite()
{
	if (rmtabf)
		(void) fclose(rmtabf);

	/* Rewrite the file. */
	if (rmtabf = fopen(RMTAB, "w+")) {
		HASHSET_ITERATOR iterator;
		struct mntentry *m;

		(void) fchmod(fileno(rmtabf),
		    (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH));
		rmtab_inuse = rmtab_deleted = 0;

		iterator = h_iterator(mntlist);

		while (m = (struct mntentry *)h_next(iterator))
			m->m_pos = rmtab_insert(m->m_host, m->m_path);
	}
}

/*
 * Parse the content of /etc/rmtab and insert the entries into mntlist.
 * The buffer s should be ended with a NUL char.
 */

static void
rmtab_parse(char *s)
{
	char  *host;
	char  *path;

host_part:
	if (*s == '#')
		goto skip_rest;

	host = s;
	for (;;) {
		switch (*s++) {
		case '\0':
			return;
		case '\n':
			goto host_part;
		case ':':
			s[-1] = '\0';
			goto path_part;
		default:
			continue;
		}
	}

path_part:
	path = s;
	for (;;) {
		switch (*s++) {
		case '\n':
			s[-1] = '\0';
			if (*host && *path)
				mntlist_insert(host, path);
			goto host_part;
		case '\0':
			if (*host && *path)
				mntlist_insert(host, path);
			return;
		default:
			continue;
		}
	}

skip_rest:
	for (;;) {
		switch (*++s) {
		case '\n':
			goto host_part;
		case '\0':
			return;
		default:
			continue;
		}
	}
}

/*
 * Read in contents of rmtab.
 * Call rmtab_parse to parse the file and store entries in mntlist.
 * Rewrites the file to get rid of unused entries.
 */

#define	RMTAB_LOADLEN	(16*2024)	/* Max bytes to read at a time */

void
rmtab_load()
{
	FILE *fp;

	(void) rwlock_init(&rmtab_lock, USYNC_THREAD, NULL);

	/*
	 * Don't need to lock the list at this point
	 * because there's only a single thread running.
	 */
	mntlist = h_create(mntentry_hash, mntentry_equal, 101, 0.75);

	if (fp = fopen(RMTAB, "r")) {
		char buf[RMTAB_LOADLEN+1];
		size_t len;

		/*
		 * Read at most RMTAB_LOADLEN bytes from /etc/rmtab.
		 * - if fread returns RMTAB_LOADLEN we can be in the middle
		 *   of a line so change the last newline character into NUL
		 *   and seek back to the next character after newline.
		 * - otherwise set NUL behind the last character read.
		 */
		while ((len = fread(buf, 1, RMTAB_LOADLEN, fp)) > 0) {
			if (len == RMTAB_LOADLEN) {
				int i;

				for (i = 1; i < len; i++) {
					if (buf[len-i] == '\n') {
						buf[len-i] = '\0';
						(void) fseek(fp, -i+1,
							    SEEK_CUR);
						goto parse;
					}
				}
			}

			/* Put a NUL character at the end of buffer */
			buf[len] = '\0';
	parse:
			rmtab_parse(buf);
		}
		(void) fclose(fp);
	}
	rmtab_rewrite();
}

/*
 * Write an entry at the current location in rmtab
 * for the given client and path.
 *
 * Returns the starting position of the entry
 * or -1 if there was an error.
 */

long
rmtab_insert(char *host, char *path)
{
	long   pos;

	if (rmtabf == NULL || fseek(rmtabf, 0L, 2) == -1) {
		return (-1);
	}
	pos = ftell(rmtabf);
	if (fprintf(rmtabf, "%s:%s\n", host, path) == EOF) {
		return (-1);
	}
	if (fflush(rmtabf) == EOF) {
		return (-1);
	}
	rmtab_inuse++;
	return (pos);
}

/*
 * Mark as unused the rmtab entry at the given offset in the file.
 */

void
rmtab_delete(long pos)
{
	if (rmtabf != NULL && pos != -1 && fseek(rmtabf, pos, 0) == 0) {
		(void) fprintf(rmtabf, "#");
		(void) fflush(rmtabf);

		rmtab_inuse--;
		rmtab_deleted++;
	}
}
