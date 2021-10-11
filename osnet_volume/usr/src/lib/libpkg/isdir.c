/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)isdir.c	1.7	93/03/09 SMI"	/* SVr4.0  1.3	*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <archives.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "pkglocale.h"

/*
 * Defines for cpio/compression checks.
 */
#define	BIT_MASK		0x1f
#define	BLOCK_MASK		0x80
#define	ERR_ISCPIO_OPEN		"iscpio(): open(%s) failed!"
#define	ERR_ISCPIO_FSTAT	"iscpio(): fstat(%s) failed!"
#define	ERR_ISCPIO_READ		"iscpio(): read(%s) failed!"
#define	ERR_ISCPIO_NOCPIO	"iscpio(): <%s> is not a cpio archive!"

#define	MASK_CK(x, y)	(((x) & (y)) == (y))
#define	ISCOMPCPIO	((unsigned char) cm.c_mag[0] == m_h[0] && \
			(unsigned char) cm.c_mag[1] == m_h[1] && \
			(MASK_CK((unsigned char) cm.c_mag[2], BLOCK_MASK) || \
			MASK_CK((unsigned char) cm.c_mag[2], BIT_MASK)))

#define	ISCPIO		(cm.b_mag != CMN_BIN && \
			!strcmp(cm.c_mag, CMS_ASC) && \
			!strcmp(cm.c_mag, CMS_CHR) && \
			!strcmp(cm.c_mag, CMS_CRC))
int
isdir(char *path)
{
	struct stat statbuf;

	if (!stat(path, &statbuf) && ((statbuf.st_mode & S_IFMT) == S_IFDIR))
		return (0);
	return (1);
}

int
isfile(char *dir, char *file)
{
	struct stat statbuf;
	char	path[PATH_MAX];

	if (dir) {
		(void) sprintf(path, "%s/%s", dir, file);
		file = path;
	}

	if (!stat(file, &statbuf) && (statbuf.st_mode & S_IFREG))
		return (0);
	return (1);
}

int
iscpio(char *path, int *iscomp)
{
	/*
	 * Compressed File Header.
	 */
	unsigned char m_h[] = { "\037\235" };		/* 1F 9D */

	static union {
		short int	b_mag;
		char		c_mag[CMS_LEN];
	}	cm;

	struct stat	statb;
	int		fd;


	*iscomp = 0;

	if ((fd = open(path, O_RDONLY, 0)) == -1) {
		if (errno != ENOENT) {
			perror("");
			(void) fprintf(stderr, pkg_gt(ERR_ISCPIO_OPEN), path);
		}
		return (0);
	} else {
		if (fstat(fd, &statb) == -1) {
			perror("");
			(void) fprintf(stderr, pkg_gt(ERR_ISCPIO_FSTAT), path);
			(void) close(fd);
			return (0);
		} else {
			if (S_ISREG(statb.st_mode)) {	/* Must be a file */
				if (read(fd, cm.c_mag, sizeof (cm.c_mag)) !=
				    sizeof (cm.c_mag)) {
					perror("");
					(void) fprintf(stderr,
					    pkg_gt(ERR_ISCPIO_READ), path);
					(void) close(fd);
					return (0);
				}
				/*
				 * Try to determine if the file is a compressed
				 * file, if that fails, try to determine if it
				 * is a cpio archive, if that fails, then we
				 * fail!
				 */
				if (ISCOMPCPIO) {
					*iscomp = 1;
					(void) close(fd);
					return (1);
				} else if (ISCPIO) {
					(void) fprintf(stderr,
					    pkg_gt(ERR_ISCPIO_NOCPIO),
					    path);
					(void) close(fd);
					return (0);
				}
				(void) close(fd);
				return (1);
			} else {
				(void) close(fd);
				return (0);
			}
		}
	}
}
