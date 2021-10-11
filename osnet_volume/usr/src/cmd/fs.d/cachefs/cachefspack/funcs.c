/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident "@(#)funcs.c   1.6     98/02/03 SMI"

#include <locale.h>
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/acl.h>
#include <sys/fs/cachefs_fs.h>
#include <sys/fs/cachefs_ioctl.h>
#include <sys/errno.h>
#include <string.h>

extern int errno;

extern int verbose;

/*
 * Function used by -d option to display pathname
 */
int
prtfn(char *pathnam, char *fnam, DIR *dirp, int depth)
{
	printf("%s\n", pathnam);
	return (0);
}

/*
 * Function used by -p option to pack pathname
 */
int
packfn(char *pathnam, char *fnam, DIR *dirp, int depth)
{
	cachefsio_pack_t pack;
	int xx;
	int len;

#ifdef DEBUG
	printf("packfn: pathnam = %s", pathnam);
	fflush(stdout);
	if (fnam != NULL) {
		printf("  fnam = %s\n",  fnam);
	} else {
		printf("\n");
	}
	printf("packfn: dirp    = %x depth = %d\n", dirp, depth);
	fflush(stdout);
#endif /* DEBUG */
	if (fnam != NULL) {
		len = strlen(fnam);
		if (len >= sizeof (pack.p_name)) {
			fprintf(stderr, gettext(
			    "cachefspack: file name too long - %s\n"),
			    pathnam);
			return (-1);
		}
#ifdef DEBUG
		printf("packfn: len = %d\n", len);
		fflush(stdout);
#endif /* DEBUG */
		while (fnam[len-1] == '/') {
		    len--;
		}
		strncpy(pack.p_name, fnam, len);
	} else {
		len = 0;
	}
	pack.p_name[len] = '\0';
	pack.p_status = 0;
#ifdef DEBUG
	printf("packfn: pack.p_name = %s  pack.p_status = %x\n",
		pack.p_name, pack.p_status);
	fflush(stdout);
#endif /* DEBUG */

	xx = ioctl(dirp->dd_fd, CACHEFSIO_PACK, &pack);
#ifdef DEBUG
	printf("packfn: xx = %x  errno = %d\n", xx, errno);
	fflush(stdout);
#endif /* DEBUG */
	if (xx) {
		if (errno == ENOTTY) {
			return (0);
		}
		if (errno == ENOSYS) {
			return (0);
		}
		fprintf(stderr, gettext("cachefspack: %s -  "), pathnam);
		perror(gettext("can't pack file"));
		return (-1);
	}
	return (0);
}

/*
 * Function used by -p option to unpack pathname
 */
int
unpackfn(char *pathnam, char *fnam, DIR *dirp, int depth)
{
	cachefsio_pack_t pack;
	int xx;
	int len;

#ifdef DEBUG
	printf("unpackfn: pathnam = %s ", pathnam);
	if (fnam != NULL) {
		printf("  fnam = %s\n", fnam);
	} else {
		printf("\n");
	}
	printf("unpackfn: dirp    = %x depth = %d\n", dirp, depth);
	fflush(stdout);
#endif /* DEBUG */
	if (fnam != NULL) {
		len = strlen(fnam);
		if (len >= sizeof (pack.p_name)) {
			fprintf(stderr, gettext(
			    "cachefspack: file name too long - %s\n"), pathnam);
			return (-1);
		}
		while (fnam[len-1] == '/') {
		    len--;
		}
		strncpy(pack.p_name, fnam, len);
	} else {
		len = 0;
	}
	pack.p_name[len] = '\0';
	pack.p_status = 0;
#ifdef DEBUG
	printf("unpackfn: pack.p_name = %s  pack.p_status = %x\n",
		pack.p_name, pack.p_status);
	fflush(stdout);
#endif /* DEBUG */

	xx = ioctl(dirp->dd_fd, CACHEFSIO_UNPACK, &pack);
#ifdef DEBUG
	printf("unpackfn: pack.p_name = %s  pack.p_status = %x\n",
		pack.p_name, pack.p_status);
	fflush(stdout);
#endif /* DEBUG */
	if (xx) {
		if (errno == ENOTTY) {
			return (0);
		}
		if (errno == ENOSYS) {
			return (0);
		}
		fprintf(stderr, gettext("cachefspack: %s - "), pathnam);
		perror(gettext("can't unpack file"));
		return (-1);
	}
	return (0);
}

/*
 * Function used by -i option to print status of pathname
 */
int
inquirefn(char *pathnam, char *fnam, DIR *dirp, int depth)
{
	cachefsio_pack_t pack;
	int xx;
	int len;

#ifdef DEBUG
	printf("inquirefn: pathnam = %s ", pathnam);
	if (fnam != NULL) {
		printf("fnam = %s\n", fnam);
	} else {
		printf("\n");
	}
	printf("inquirefn: dirp    = %x depth = %d\n", dirp, depth);
	fflush(stdout);
#endif /* DEBUG */
	if (fnam != NULL) {
		len = strlen(fnam);
		if (len >= sizeof (pack.p_name)) {
			fprintf(stderr,
			    gettext("cachefspack: file name too long - %s\n"),
			    pathnam);
			return (-1);
		}
		while (fnam[len-1] == '/') {
		    len--;
		}
		strncpy(pack.p_name, fnam, len);
	} else {
		len = 0;
	}
	pack.p_name[len] = '\0';
	pack.p_status = 0;
#ifdef DEBUG
	printf("inquirefn: pack.p_name = %s  pack.p_status = %x\n",
		pack.p_name, pack.p_status);
	fflush(stdout);
#endif /* DEBUG */

	xx = ioctl(dirp->dd_fd, CACHEFSIO_PACKINFO, &pack);
#ifdef DEBUG
	printf("inquirefn: xx = %x  errno = %d\n", xx, errno);
	fflush(stdout);
#endif /* DEBUG */
	if (xx) {
		if ((errno == ENOTTY) || (errno == ENOSYS)) {
#ifdef CFS_MSG
			fprintf(stderr, gettext("cachefspack:  "));
			fprintf(stderr,
			    gettext("%s - is not in a cacheFS file system\n"),
			    pathnam);
#endif /* CFS_MSG */
			return (-1);
		}
		fprintf(stderr, gettext("cachefspack: %s - "), pathnam);
		perror(gettext("can't get info"));
		return (-2);
	}

	printf(gettext("cachefspack: file %s "), pathnam);
	printf(gettext("marked packed %s, packed %s\n"),
	    (pack.p_status & CACHEFS_PACKED_FILE) ? "YES" : "NO",
	    (pack.p_status & CACHEFS_PACKED_DATA) ? "YES" : "NO");
	if (verbose) {
		printf(gettext("    nocache %s\n"),
		    (pack.p_status & CACHEFS_PACKED_NOCACHE) ?
		    "YES" : "NO");
	}
	return (0);
}
