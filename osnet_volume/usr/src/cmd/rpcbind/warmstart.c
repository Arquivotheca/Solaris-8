/*
 * warmstart.c
 * Allows for gathering of registrations from a earlier dumped file.
 *
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#ident	"@(#)warmstart.c	1.7	98/02/18 SMI"

#include <stdio.h>
#include <errno.h>
#include <rpc/rpc.h>
#include <rpc/rpcb_prot.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef PORTMAP
#include <netinet/in.h>
#include <rpc/pmap_prot.h>
#endif
#include "rpcbind.h"
#include <sys/syslog.h>

/* These files keep the pmap_list and rpcb_list in XDR format */
#define	RPCBFILE	"/tmp/rpcbind.file"
#ifdef PORTMAP
#define	PMAPFILE	"/tmp/portmap.file"
#endif

static bool_t write_struct();
static bool_t read_struct();

FILE *
open_tmp_file(filename)
	char *filename;
{
	int fd;
	FILE *fp;
	mode_t omask;
	struct stat sbuf;

	/*
	 * Remove any existing files, and create a new one.
	 * Ensure that rpcbind is not forced to overwrite
	 * a file pointed to by a symbolic link created
	 * by an attacker.
	 * Use open O_CREAT|O_EXCL so file is not created
	 * between unlink() and open() operation.
	 */
	if (unlink(filename) == -1) {
		if (errno != ENOENT)
			return (NULL);
	}
	fd = open(filename, O_CREAT|O_EXCL|O_WRONLY, 0600);
	if (fd == -1)
		return (NULL);
	fp = fdopen(fd, "w");
	if (fp == NULL) {
		close(fd);
		return (NULL);
	}

	return (fp);
}

static bool_t
write_struct(filename, structproc, list)
	char *filename;
	xdrproc_t structproc;
	void *list;
{
	FILE *fp;
	XDR xdrs;

	fp = open_tmp_file(filename);
	if (fp == NULL) {
		int i;

		for (i = 0; i < 10; i++)
			close(i);
		fp = open_tmp_file(filename);
		if (fp == NULL) {
			syslog(LOG_ERR,
				"cannot open file = %s for writing", filename);
			syslog(LOG_ERR, "cannot save any registration");
			return (FALSE);
		}
	}
	xdrstdio_create(&xdrs, fp, XDR_ENCODE);

	if (structproc(&xdrs, list) == FALSE) {
		syslog(LOG_ERR, "rpcbind: xdr_%s: failed", filename);
		fclose(fp);
		return (FALSE);
	}
	XDR_DESTROY(&xdrs);
	fclose(fp);
	return (TRUE);
}

static bool_t
read_struct(filename, structproc, list)
	char *filename;
	xdrproc_t structproc;
	void *list;
{
	int fd;
	FILE *fp = NULL;
	XDR xdrs;
	struct stat sbuf_fstat, sbuf_lstat;

	fd = open(filename, O_RDONLY, 0600);
	if (fd == -1) {
		fprintf(stderr,
		"rpcbind: cannot open file = %s for reading\n", filename);
		goto error;
	}
	fp = fdopen(fd, "r");
	if (fp == NULL) {
		close(fd);
		fprintf(stderr,
		"rpcbind: cannot open file = %s for reading\n", filename);
		goto error;
	}
	if (fstat(fd, &sbuf_fstat) != 0) {
		fprintf(stderr,
		"rpcbind: cannot stat file = %s for reading\n", filename);
		goto error;
	}
	if ((sbuf_fstat.st_uid != 0) ||
	    (!S_ISREG(sbuf_fstat.st_mode)) ||
	    (sbuf_fstat.st_mode & S_IRWXG) ||
	    (sbuf_fstat.st_mode & S_IRWXO) ||
	    (sbuf_fstat.st_nlink != 1)) {
		fprintf(stderr,
		"rpcbind: invalid permissions on file = %s for reading\n",
			filename);
		goto error;
	}
	/*
	 * Make sure that the pathname for fstat and lstat is the same and
	 * that it's not a link.  An attacker can create symbolic or
	 * hard links and use them to gain unauthorised access to the
	 * system when rpcbind aborts or terminates on SIGINT or SIGTERM.
	 */
	if (lstat(filename, &sbuf_lstat) != 0) {
		fprintf(stderr,
		"rpcbind: cannot lstat file = %s for reading\n", filename);
		goto error;
	}
	if ((sbuf_lstat.st_uid != 0) ||
	    (!S_ISREG(sbuf_lstat.st_mode)) ||
	    (sbuf_lstat.st_mode & S_IRWXG) ||
	    (sbuf_lstat.st_mode & S_IRWXO) ||
	    (sbuf_lstat.st_nlink != 1) ||
	    (sbuf_fstat.st_dev != sbuf_lstat.st_dev) ||
	    (sbuf_fstat.st_ino != sbuf_lstat.st_ino)) {
		fprintf(stderr,
		"rpcbind: invalid lstat permissions on file = %s for reading\n",
			filename);
		goto error;
	}
	xdrstdio_create(&xdrs, fp, XDR_DECODE);

	if (structproc(&xdrs, list) == FALSE) {
		fprintf(stderr, "rpcbind: xdr_%s: failed\n", filename);
		goto error;
	}
	XDR_DESTROY(&xdrs);
	fclose(fp);
	return (TRUE);

error:	fprintf(stderr, "rpcbind: will start from scratch\n");
	if (fp != NULL)
		fclose(fp);
	return (FALSE);
}

void
write_warmstart()
{
	(void) write_struct(RPCBFILE, xdr_rpcblist_ptr, &list_rbl);
#ifdef PORTMAP
	(void) write_struct(PMAPFILE, xdr_pmaplist_ptr, &list_pml);
#endif

}

void
read_warmstart()
{
	rpcblist_ptr tmp_rpcbl = NULL;
#ifdef PORTMAP
	pmaplist_ptr tmp_pmapl = NULL;
#endif
	int ok1, ok2 = TRUE;

	ok1 = read_struct(RPCBFILE, xdr_rpcblist_ptr, &tmp_rpcbl);
	if (ok1 == FALSE)
		return;
#ifdef PORTMAP
	ok2 = read_struct(PMAPFILE, xdr_pmaplist_ptr, &tmp_pmapl);
#endif
	if (ok2 == FALSE) {
		xdr_free((xdrproc_t) xdr_rpcblist_ptr, (char *)&tmp_rpcbl);
		return;
	}
	xdr_free((xdrproc_t) xdr_rpcblist_ptr, (char *)&list_rbl);
	list_rbl = tmp_rpcbl;
#ifdef PORTMAP
	xdr_free((xdrproc_t) xdr_pmaplist_ptr, (char *)&list_pml);
	list_pml = (pmaplist *)tmp_pmapl;
#endif
}
