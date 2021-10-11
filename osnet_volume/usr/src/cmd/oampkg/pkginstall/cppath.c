/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)cppath.c	1.16	94/10/21 SMI"	/* SVr4.0 1.7 */

/*  5-20-92	added newroot function */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>
#include <locale.h>
#include <libintl.h>
#include <pkglocs.h>
#include <errno.h>
#include <fcntl.h> /* bug # 1081861 Install too slow */
#include <pkglib.h>
#include "libadm.h"
#include "libinst.h"
#include "install.h"
#include "pkginstall.h"

#define	MSG_IMPDIR	"%s <implied directory>"
#define	WRN_RENAME	"WARNING: unable to rename <%s>"
#define	WRN_RELATIVE	"attempting to rename a relative file <%s>"
#define	ERR_STAT	"cppath(): unable to stat <%s>"
#define	MSG_PROCMV	"- executing process moved to <%s>"
#define	ERR_READ	"unable to open <%s> for reading"
#define	ERR_WRITE	"unable to open <%s> for writing"
#define	ERR_OUTPUT	"error while writing file <%s>"
#define	ERR_UNLINK	"unable to unlink <%s>"
#define	ERR_LOG		"unable to open logfile <%s>"
#define	ERR_UTIME	"unable to reset access/modification time of <%s>"
#define	ERR_MKDIR	"unable to create directory <%s>"

/* bug # 1081861 */
#define	ERR_MEMORY	"malloc(%ld) for <%s> failed"
#define	ERR_INPUT	"error while reading file <%s>"

static FILE	*logfp;
static char	*linknam;
static int	errflg = 0;

/* bug # 1081861 */
static int	writefile(int ctrl, mode_t locmode, char *file);
static char	*io_buffer = NULL;
static long	io_buffer_size = -1;

extern int	debug;
extern int	silent;
extern char	*rw_block_size;

/*
 * This function installs the new file onto the system. If the ctrl
 * word is KEEPMODE or SETMODE it will set the modes of the new file
 * as well. If the ctrl is SETMODE, it will set the mode to the fourth
 * argument without screening out setuid or setgid bits, so if the
 * user has denied permission to install setuid processes, the calling
 * function has to screen those bits out of mode before calling this.
 */
int
cppath(int ctrl, char *f1, char *f2, mode_t mode)
{
	struct stat status;
	struct utimbuf times;
	/* bug # 1081861 */
	int	fd1, fd2;
	mode_t usemode;
	long	bytes_read;

	char	busylog[PATH_MAX];


	if (stat(f1, &status)) {
		progerr(gettext(ERR_STAT), f1);
		errflg++;
		return (1);
	}
	times.actime = status.st_atime;
	times.modtime = status.st_mtime;

	/* bug # 1081861 */
	if (io_buffer_size == -1) {
		if (rw_block_size) {
			if ((io_buffer_size = atoi(rw_block_size)) <= 0)
				io_buffer_size = status.st_blksize;
		} else
			io_buffer_size = status.st_blksize;
	}

	if (io_buffer == NULL) {
		if ((io_buffer = malloc(io_buffer_size)) == NULL) {
			progerr(gettext(ERR_MEMORY), io_buffer_size,
			    "io_buffer");
			errflg++;
			return (1);
		}
	}

	/* Open the source file */
	if ((fd1 = open(f1, O_RDONLY, 0666)) == -1) {
		progerr(gettext(ERR_READ), f1);
		errflg++;
		return (1);
	}

	usemode = (ctrl & SETMODE) ? mode : (status.st_mode & S_IAMB);

	/*
	 *  Get fd of newly created destination file or, if this
	 * is an overwrite,  a temporary file (linknam).
	 */
	if ((fd2 = writefile(ctrl, usemode, f2)) == -1) {
		(void) close(fd1);
		return (1);
	}

	while ((bytes_read = read(fd1, io_buffer, io_buffer_size)) > 0) {
		if (write(fd2, io_buffer, bytes_read) != bytes_read) {
			progerr(gettext(ERR_OUTPUT), f2);
			(void) close(fd1);
			(void) close(fd2);
			if (linknam)
				unlink(linknam);
			return (1);
		}
	}
	if (bytes_read < 0) {
		progerr(gettext(ERR_INPUT), f1);
		errflg++;
		(void) close(fd1);
		(void) close(fd2);
		if (linknam)
			unlink(linknam);
		return (1);
	}

	(void) close(fd1);
	(void) close(fd2);

	/* If this is an overwrite, rename temp over original. */
	if (linknam) {
		if (rename(linknam, f2)) {
			if (errno == ETXTBSY)
				logerr(gettext(MSG_PROCMV), linknam);
			else {
				progerr(gettext(ERR_OUTPUT), f2);
				errflg++;
			}
			unlink(linknam);
			if (!logfp) {
				(void) sprintf(busylog, "%s/textbusy",
				    get_PKGADM());
				if ((logfp = fopen(busylog, "a")) == NULL) {
					progerr(gettext(ERR_LOG), busylog);
					errflg++;
				} else
					(void) fprintf(logfp, "%s\n", linknam);
			} else
				(void) fprintf(logfp, "%s\n", linknam);
		}
	}
	if (utime(f2, &times)) {
		progerr(gettext(ERR_UTIME), f2);
		errflg++;
		return (1);
	}
	return (0);
}

static int
create_path(int dspmode, char *file)
{
	char *pt;
	int found;

	found = 0;
	for (pt = file; *pt; pt++) {
		if ((*pt == '/') && (pt != file)) {
			*pt = '\0';
			if (access(file, 0)) {
				if (mkdir(file, 0755)) {
					progerr(gettext(ERR_MKDIR), file);
					*pt = '/';
					return (1);
				}
				if (dspmode)
					echo(gettext(MSG_IMPDIR), file);
				found++;
			}
			*pt = '/';
		}
	}
	return (!found);
}

/* bug # 1081861 */
/*
 * This function either creates the new destination file or, if
 * there's a file already there, creates a temporary file under
 * the moniker linknam. It returns the file descriptor of the file
 * it openned.
 *
 * It is the responsibility of the calling function to put the
 * correct mode into locmode. If it's calling w/ KEEPMODE, this is the
 * mode of the source file, if it's calling with SETMODE, it's the
 * intended mode for the destination file (usu. from the pkgmap).
 */
static int
writefile(int ctrl, mode_t locmode, char *file)
{
	/* bug # 1081861 */
	int fd = -1;
	mode_t usemode = (ctrl & (SETMODE | KEEPMODE)) ? locmode : 0666;
	static char loc_link[PATH_MAX];

	/*
	 * If we are overwriting an existing file, arrange to replace
	 * it transparently.
	 */
	if (access(file, 0) == 0) {
		/*
		 * link the file to be copied to a temporary name in case
		 * it is executing or it is being written/used (e.g., a shell
		 * script currently being executed
		 */
		if (!RELATIVE(file)) {
			(void) sprintf(loc_link, "%sXXXXXX", file);
			(void) mktemp(loc_link);
		} else {
			logerr(gettext(WRN_RELATIVE), file);
			(void) sprintf(loc_link, "./%sXXXXXX", file);
			(void) mktemp(loc_link);
		}

		linknam = loc_link;

		/* Open the temporary file */
		if ((fd = open(linknam, O_WRONLY | O_CREAT | O_TRUNC,
		    usemode)) == -1) {
			progerr(gettext(ERR_WRITE), linknam);
			*linknam = '/0';
			errflg++;
		}
	} else {	/* We aren't overwriting. */
		linknam = NULL;
		/* bug # 1081861 */

		if ((fd = open(file, O_WRONLY | O_CREAT | O_TRUNC,
		    usemode)) == -1) {
			if (create_path(ctrl, file) == 0) {
				if ((fd = open(file,
				    O_WRONLY | O_CREAT | O_TRUNC,
				    usemode)) == -1) {
					progerr(gettext(ERR_WRITE), file);
					errflg++;
				}
			}
		}
	}

	return (fd);
}
