/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#pragma ident	"@(#)verify.c	1.29	99/08/30 SMI"	/* SVr4.0  1.21.1.1	*/

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/param.h>
#ifndef SUNOS41
#include <sys/mkdev.h>
#endif
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <pkgstrct.h>
#include <pkglib.h>
#include "pkglocale.h"
/* Bug fix #1081861 Install too slow */
#include <fcntl.h>
#define	IO_BUFFER_SIZE	8192

extern	struct passwd	*cpwuid(uid_t uid);
extern	struct group	*cgrgid(gid_t gid);
extern	struct passwd	*cpwnam(char *nam);
extern	struct group	*cgrnam(char *nam);

#define	WDMSK 0177777L
#define	BUFSIZE 512
#define	DATEFMT	"%D %r"
#define	TDELTA 15*60

#define	MSG_WLDDEVNO	"NOTE: <%s> created as device (%d, %d)."

#define	WRN_QV_SIZE	"WARNING: quick verify of <%s>; wrong size."
#define	WRN_QV_MTIME	"WARNING: quick verify of <%s>; wrong mod time."

#define	ERR_UNKNOWN	"unable to determine object type"
#define	ERR_EXIST	"pathname does not exist"
#define	ERR_FTYPE	"file type <%c> expected <%c> actual"
#define	ERR_LINK	"pathname not properly linked to <%s>"
#define	ERR_SLINK	"pathname not symbolically linked to <%s>"
#define	ERR_MTIME	"modtime <%s> expected <%s> actual"
#define	ERR_SIZE	"file size <%ld> expected <%ld> actual"
#define	ERR_CKSUM	"file cksum <%ld> expected <%ld> actual"
#define	ERR_NO_CKSUM	"unable to checksum, may need to re-run command as " \
			"user \"root\""
#define	ERR_MAJMIN	"major/minor device <%d, %d> expected <%d, %d> actual"
#define	ERR_PERM	"permissions <%04o> expected <%04o> actual"
#define	ERR_GROUP	"group name <%s> expected <%s> actual"
#define	ERR_OWNER	"owner name <%s> expected <%s> actual"
#define	ERR_MODFAIL	"unable to fix modification time"
#define	ERR_LINKFAIL	"unable to create link to <%s>"
#define	ERR_LINKISDIR	"<%s> is a directory, link() not performed"
#define	ERR_SLINKFAIL	"unable to create symbolic link to <%s>"
#define	ERR_DIRFAIL	"unable to create directory"
#define	ERR_CDEVFAIL	"unable to create character-special device"
#define	ERR_BDEVFAIL	"unable to create block-special device"
#define	ERR_PIPEFAIL	"unable to create named pipe"
#define	ERR_ATTRFAIL	"unable to fix attributes"
#define	ERR_BADGRPID	"unable to determine group name for gid <%d>"
#define	ERR_BADUSRID	"unable to determine owner name for uid <%d>"
#define	ERR_BADGRPNM	"group name <%s> not found in group table(s)"
#define	ERR_BADUSRNM	"owner name <%s> not found in passwd table(s)"
#define	ERR_GETWD	"unable to determine current working directory"
#define	ERR_CHDIR	"unable to change current working directory to <%s>"
#define	ERR_RMDIR	"unable to remove existing directory at <%s>"

char	errbuf[PATH_MAX+512];

/* checksum disable switch */
int	dochecksum = 1;

/* From rrmdir.c */
extern	int	rrmdir(char *path);

/* Used only by instvol.c for quick verify */
int fverify(int fix, char *ftype, char *path, struct ainfo *ainfo,
    struct cinfo *cinfo);

static int	clear_target(char *path, char *ftype, int is_a_dir);

static int	cksumerr;
static int	nonabi_symlinks; /* bug id 4244631, not ABI compliant */
static unsigned	docksum(char *path);

struct part { short unsigned hi, lo; };
static union hilo { /* this only works right in case short is 1/2 of long */
	struct part hl;
	long	lg;
} tempa, suma;


/*VARARGS*/
static void
reperr(char *fmt, ...)
{
	char *pt;
	va_list	ap;
	int	n;

	va_start(ap, fmt);
	if (n = strlen(errbuf)) {
		pt = errbuf + n;
		*pt++ = '\n';
		*pt = '\0';
	} else
		pt = errbuf;
	if (fmt) {
		(void) vsprintf(pt, fmt, ap);
		pt += strlen(pt);
	} else
		errbuf[0] = '\0';

	va_end(ap);
}
/*ARGSUSED*/

/* BUG # 1081861 */
void
checksum_on(void)
{
	dochecksum = 1;
}

/* BUG # 1081861 */
void
checksum_off(void)
{
	dochecksum = 0;
}

int
cverify(int fix, char *ftype, char *path, struct cinfo *cinfo)
{
	struct stat	status; 	/* file status buffer */
	struct utimbuf	times;
	unsigned	mycksum;
	int		setval, retcode;
	char		tbuf1[26], tbuf2[26];

	setval = (*ftype == '?');
	retcode = 0;
	reperr(NULL);

	if (stat(path, &status) < 0) {
		reperr(pkg_gt(ERR_EXIST));
		return (VE_EXIST);
	}

	/* -1	requires modtimes to be the same */
	/*  0   reports modtime failure */
	/*  1   fixes modtimes */
	if (setval || (cinfo->modtime == BADCONT))
		cinfo->modtime = status.st_mtime;
	else if (status.st_mtime != cinfo->modtime) {
		if (fix > 0) {
			/* reset times on the file */
			times.actime = cinfo->modtime;
			times.modtime = cinfo->modtime;
			if (utime(path, &times)) {
				reperr(pkg_gt(ERR_MODFAIL));
				retcode = VE_FAIL;
			}
		} else if (fix < 0) {
			/* modtimes must be the same */
			(void) cftime(tbuf1, DATEFMT, &cinfo->modtime);
			(void) cftime(tbuf2, DATEFMT, &status.st_mtime);
			reperr(pkg_gt(ERR_MTIME), tbuf1, tbuf2);
			retcode = VE_CONT;
		}
#if 0
		else
			retcode = VE_TIME;
#endif	/* 0 */
	}
	if (setval || (cinfo->size == BADCONT))
		cinfo->size = status.st_size;
	else if (status.st_size != cinfo->size) {
		if (!retcode /* || (retcode == VE_TIME)*/)
			retcode = VE_CONT;
		reperr(pkg_gt(ERR_SIZE), cinfo->size, status.st_size);
	}

	cksumerr = 0;
/* checksum disable swicth */
	if (dochecksum == 1) {
		mycksum = docksum(path);
	} else {
		mycksum = cinfo->cksum;
	}

	if (setval || (cinfo->cksum == BADCONT))
		cinfo->cksum = mycksum;
	else if ((mycksum != cinfo->cksum) || cksumerr) {
		if (!retcode /* || (retcode == VE_TIME)*/)
			retcode = VE_CONT;
		if (!cksumerr)
			reperr(pkg_gt(ERR_CKSUM), cinfo->cksum, mycksum);
	}

	return (retcode);
}

static unsigned
docksum(char *path)
{
	/* Bug fix #1081861 Install too slow */
	register	int	fp;
	static		char	io_buffer[IO_BUFFER_SIZE];
	char		*cp;
	long		bytes_read;

	unsigned	lsavhi, lsavlo;

	cksumerr = 0;
	/* Bug fix #1081861 Install too slow */
	if ((fp = open(path, O_RDONLY, 0)) == -1) {
		cksumerr++;
		reperr(pkg_gt(ERR_NO_CKSUM));
		return (0);
	}

	suma.lg = 0;

	while ((bytes_read = read(fp, io_buffer, sizeof (io_buffer))) > 0)
	    for (cp = io_buffer; cp < (io_buffer+bytes_read); cp++)
		    suma.lg += ((int) (*cp&0377)) & WDMSK;

	tempa.lg = (suma.hl.lo & WDMSK) + (suma.hl.hi & WDMSK);
	lsavhi = (unsigned) tempa.hl.hi;
	lsavlo = (unsigned) tempa.hl.lo;

	(void) close(fp);
	return (lsavhi+lsavlo);
}

static 	struct stat	status; 	/* file status buffer */
static  struct statvfs	vfsstatus;	/* filesystem status buffer */

/*
 * Remove the thing that's currently in place so we can put down the package
 * object. If we're replacing a directory with a directory, leave it alone.
 * Returns 1 if all OK and 0 if failed.
 */
static int
clear_target(char *path, char *ftype, int is_a_dir)
{
	int retcode = 1;

	if (is_a_dir) {	/* if there's a directory there already ... */
		/* ... and this isn't, ... */
		if (strchr("dx", *ftype) == NULL)
			if (rmdir(path)) {	/* try to remove it. */
				reperr(pkg_gt(ERR_RMDIR), path);
				retcode = 0;
			}
	} else
		if (unlink(path))
			if (errno != ENOENT)
				retcode = 0;	/* It didn't work. */

	return (retcode);
}

/*
 * This function verifies and (if fix > 0) fixes the attributes of the
 * file at the path provided.
 */
int
averify(int fix, char *ftype, char *path, struct ainfo *ainfo)
{
	struct group	*grp; 	/* group entry buffer */
	struct passwd	*pwd;
	int		n;
	int		setval;
	int		uid, gid;
	int		dochown;
	int		retcode;
	int		statError = 0;
	int		targ_is_dir = 0;	/* replacing a directory */
	char		myftype;
	char		buf[PATH_MAX];
	ino_t		my_ino;
	dev_t		my_dev;
	char 		cwd[MAXPATHLEN];
	char 		*cd;
	char 		*c;

	setval = (*ftype == '?');
	retcode = 0;
	reperr(NULL);

	if (*ftype == 'l') {
		if (stat(path, &status) < 0) {
			retcode = VE_EXIST;
			reperr(pkg_gt(ERR_EXIST));
		}

		my_ino = status.st_ino;
		my_dev = status.st_dev;

		/* Get copy of the current working directory */
		if (getcwd(cwd, MAXPATHLEN) == NULL) {
			reperr(pkg_gt(ERR_GETWD), ainfo->local);
			return (VE_FAIL);
		}

		/*
		 * Change to the directory in which the hard
		 * link is to be created.
		 */
		cd = strdup(path);
		c = strrchr(cd, '/');
		if (c) {
			/* bugid 4247895 */
			if (strcmp(cd, c) == 0)
				strcpy(cd, "/");
			else
				*c = NULL;

			if (chdir(cd) != 0) {
				reperr(pkg_gt(ERR_CHDIR), cd);
				return (VE_FAIL);
			}
		}
		free(cd);

		if (retcode || (status.st_nlink < 2) ||
		    (stat(ainfo->local, &status) < 0) ||
		    (my_dev != status.st_dev) || (my_ino != status.st_ino)) {
			if (fix) {
				/*
				 * Don't want to do a hard link to a
				 * directory.
				 */
				if (!isdir(ainfo->local)) {
					chdir(cwd);
					reperr(pkg_gt(ERR_LINKISDIR),
					    ainfo->local);
					return (VE_FAIL);
				}
				/* Now do the link. */
				if (!clear_target(path, ftype, targ_is_dir))
					return (VE_FAIL);

				if (link(ainfo->local, path)) {
					chdir(cwd);
					reperr(pkg_gt(ERR_LINKFAIL),
					    ainfo->local);
					return (VE_FAIL);
				}
				retcode = 0;
			} else {
				/* Go back to previous working directory */
				if (chdir(cwd) != 0)
					reperr(pkg_gt(ERR_CHDIR), cwd);

				reperr(pkg_gt(ERR_LINK), ainfo->local);
				return (VE_CONT);
			}
		}

		/* Go back to previous working directory */
		if (chdir(cwd) != 0) {
			reperr(pkg_gt(ERR_CHDIR), cwd);
			return (VE_CONT);
		}

		return (retcode);
	}

	retcode = 0;

	/* If we are to process symlinks the old way then we follow the link */
	if (nonABI_symlinks()) {
		if ((*ftype == 's') ? lstat(path, &status) :
			stat(path, &status)) {
			reperr(pkg_gt(ERR_EXIST));
			retcode = VE_EXIST;
			myftype = '?';
			statError++;
		}
	/* If not then we inspect the target of the link */
	} else {
		if ((n = lstat(path, &status)) == -1) {
			reperr(pkg_gt(ERR_EXIST));
			retcode = VE_EXIST;
			myftype = '?';
			statError++;
		}
	}
	if (!statError) {
		/* determining actual type of existing object */
		switch (status.st_mode & S_IFMT) {
		    case S_IFLNK:
			myftype = 's';
			break;

		    case S_IFIFO:
			myftype = 'p';
			break;

		    case S_IFCHR:
			myftype = 'c';
			break;

		    case S_IFDIR:
			myftype = 'd';
			targ_is_dir = 1;
			break;

		    case S_IFBLK:
			myftype = 'b';
			break;

		    case S_IFREG:
		    case 0:
			myftype = 'f';
			break;

		    default:
			reperr(pkg_gt(ERR_UNKNOWN));
			return (VE_FTYPE);
		}
	}

	if (setval)
		*ftype = myftype;
	else if (!retcode && (*ftype != myftype) &&
	    ((myftype != 'f') || !strchr("ilev", *ftype)) &&
	    ((myftype != 'd') || (*ftype != 'x'))) {
		reperr(pkg_gt(ERR_FTYPE), *ftype, myftype);
		retcode = VE_FTYPE;
	}

	if (!retcode && (*ftype == 's')) {
		/* make sure that symbolic link is correct */
		n = readlink(path, buf, PATH_MAX);
		if (n < 0) {
			reperr(pkg_gt(ERR_SLINK), ainfo->local);
			retcode = VE_CONT;
		} else if (ainfo->local != NULL) {
			buf[n] = '\0';
			if (strcmp(buf, ainfo->local)) {
				reperr(pkg_gt(ERR_SLINK), ainfo->local);
				retcode = VE_CONT;
			}
		}
	}

	if (retcode) {
		/* The path doesn't exist or is different than it should be. */
		if (fix) {
			/*
			 * Clear the way for the write. If it won't clear,
			 * there's nothing we can do.
			 */
			if (!clear_target(path, ftype, targ_is_dir))
				return (VE_FAIL);

			if (strchr("dx", *ftype)) {
				char	*pt, *p;

				/* Try to make it the easy way */
				if (mkdir(path, ainfo->mode)) {
					/*
					 * Failing that, walk through the
					 * parent directories creating
					 * whatever is needed.
					 */
					p = strdup(path);
					pt = (*p == '/') ? p+1 : p;
					do {
						if (pt = strchr(pt, '/'))
							*pt = '\0';
						if (access(p, 0) &&
						    mkdir(p, ainfo->mode))
							break;
						if (pt)
							*pt++ = '/';
					} while (pt);
					free(p);
				}
				if (stat(path, &status) < 0) {
					reperr(pkg_gt(ERR_DIRFAIL));
					return (VE_FAIL);
				}
			} else if (*ftype == 's') {
				if (symlink(ainfo->local, path)) {
					reperr(pkg_gt(ERR_SLINKFAIL),
					    ainfo->local);
					return (VE_FAIL);
				}

			} else if (*ftype == 'c') {
				int wilddevno = 0;
				/*
				 * The next three if's support 2.4 and older
				 * packages that use "?" as device numbers.
				 * This should be considered for removal by
				 * release 2.7 or so.
				 */
				if (ainfo->major == BADMAJOR) {
					ainfo->major = 0;
					wilddevno = 1;
				}

				if (ainfo->minor == BADMINOR) {
					ainfo->minor = 0;
					wilddevno = 1;
				}

				if (wilddevno) {
					wilddevno = 0;
					logerr(MSG_WLDDEVNO, path,
					    ainfo->major, ainfo->minor);
				}

				if (mknod(path, ainfo->mode | S_IFCHR,
#ifdef SUNOS41
				    makedev(ainfo->xmajor, ainfo->xminor)) ||
#else
				    makedev(ainfo->major, ainfo->minor)) ||
#endif
				    (stat(path, &status) < 0)) {
					reperr(pkg_gt(ERR_CDEVFAIL));
					return (VE_FAIL);
				}
			} else if (*ftype == 'b') {
				int wilddevno = 0;
				/*
				 * The next three if's support 2.4 and older
				 * packages that use "?" as device numbers.
				 * This should be considered for removal by
				 * release 2.7 or so.
				 */
				if (ainfo->major == BADMAJOR) {
					ainfo->major = 0;
					wilddevno = 1;
				}

				if (ainfo->minor == BADMINOR) {
					ainfo->minor = 0;
					wilddevno = 1;
				}

				if (wilddevno) {
					wilddevno = 0;
					logerr(MSG_WLDDEVNO, path,
					    ainfo->major, ainfo->minor);
				}

				if (mknod(path, ainfo->mode | S_IFBLK,
#ifdef SUNOS41
				    makedev(ainfo->xmajor, ainfo->xminor)) ||
#else
				    makedev(ainfo->major, ainfo->minor)) ||
#endif
				    (stat(path, &status) < 0)) {
					reperr(pkg_gt(ERR_BDEVFAIL));
					return (VE_FAIL);
				}
			} else if (*ftype == 'p') {
				if (mknod(path, ainfo->mode | S_IFIFO, NULL) ||
				    (stat(path, &status) < 0)) {
					reperr(pkg_gt(ERR_PIPEFAIL));
					return (VE_FAIL);
				}
			} else
				return (retcode);

		} else
			return (retcode);
	}

	if (*ftype == 's')
		return (0); /* don't check anything else */
	if (*ftype == 'i')
		return (0); /* don't check anything else */

	retcode = 0;
	if (strchr("cb", myftype)) {
#ifdef SUNOS41
		if (setval || (ainfo->xmajor < 0))
			ainfo->xmajor = ((status.st_rdev>>8)&0377);
		if (setval || (ainfo->xminor < 0))
			ainfo->xminor = (status.st_rdev&0377);
		/* check major & minor */
		if (status.st_rdev != makedev(ainfo->xmajor, ainfo->xminor)) {
			reperr(pkg_gt(ERR_MAJMIN), ainfo->xmajor,
			    ainfo->xminor,
				(status.st_rdev>>8)&0377, status.st_rdev&0377);
			retcode = VE_CONT;
		}
#else
		if (setval || (ainfo->major == BADMAJOR))
			ainfo->major = major(status.st_rdev);
		if (setval || (ainfo->minor == BADMINOR))
			ainfo->minor = minor(status.st_rdev);
		/* check major & minor */
		if (status.st_rdev != makedev(ainfo->major, ainfo->minor)) {
			reperr(pkg_gt(ERR_MAJMIN), ainfo->major, ainfo->minor,
			    major(status.st_rdev), minor(status.st_rdev));
			retcode = VE_CONT;
		}
#endif
	}

	/* compare specified mode w/ actual mode excluding sticky bit */
	if (setval || (ainfo->mode == BADMODE))
		ainfo->mode = status.st_mode & 07777;
	else if ((ainfo->mode & 06777) != (status.st_mode & 06777)) {
		if (fix) {
			if ((ainfo->mode == BADMODE) ||
			    (chmod(path, ainfo->mode) < 0))
				retcode = VE_FAIL;
		} else {
			reperr(pkg_gt(ERR_PERM), ainfo->mode,
				status.st_mode & 07777);
			if (!retcode)
				retcode = VE_ATTR;
		}
	}

	/* rewind group file */
	setgrent();
	dochown = 0;

	/* get group entry for specified group */
	if (setval || strcmp(ainfo->group, BADGROUP) == 0) {
		grp = cgrgid(status.st_gid);
		if (grp)
			(void) strcpy(ainfo->group, grp->gr_name);
		else {
			if (!retcode)
				retcode = VE_ATTR;
			reperr(pkg_gt(ERR_BADGRPID), status.st_gid);
		}
		gid = status.st_gid;
	} else if ((grp = cgrnam(ainfo->group)) == NULL) {
		reperr(pkg_gt(ERR_BADGRPNM), ainfo->group);
		if (!retcode)
			retcode = VE_ATTR;
	} else if ((gid = grp->gr_gid) != status.st_gid) {
		if (fix) {
			/* save specified GID */
			gid = grp->gr_gid;
			dochown++;
		} else {
			if ((grp = cgrgid((int)status.st_gid)) ==
			    (struct group *)NULL) {
				reperr(pkg_gt(ERR_GROUP), ainfo->group,
				    "(null)");
			} else {
				reperr(pkg_gt(ERR_GROUP), ainfo->group,
				    grp->gr_name);
			}
			if (!retcode)
				retcode = VE_ATTR;
		}
	}

	/* rewind password file */
	setpwent();

	/* get password entry for specified owner */
	if (setval || strcmp(ainfo->owner, BADOWNER) == 0) {
		pwd = cpwuid((int)status.st_uid);
		if (pwd)
			(void) strcpy(ainfo->owner, pwd->pw_name);
		else {
			if (!retcode)
				retcode = VE_ATTR;
			reperr(pkg_gt(ERR_BADUSRID), status.st_uid);
		}
		uid = status.st_uid;
	} else if ((pwd = cpwnam(ainfo->owner)) == NULL) {
		/* UID does not exist in password file */
		reperr(pkg_gt(ERR_BADUSRNM), ainfo->owner);
		if (!retcode)
			retcode = VE_ATTR;
	} else if ((uid = pwd->pw_uid) != status.st_uid) {
		/* get owner name for actual UID */
		if (fix) {
			uid = pwd->pw_uid;
			dochown++;
		} else {
			pwd = cpwuid((int)status.st_uid);
			if (pwd == NULL)
				reperr(pkg_gt(ERR_BADUSRID),
				    (int)status.st_uid);
			else
				reperr(pkg_gt(ERR_OWNER), ainfo->owner,
				    pwd->pw_name);

			if (!retcode)
				retcode = VE_ATTR;
		}
	}

	if (statvfs(path, &vfsstatus) < 0) {
		reperr(pkg_gt(ERR_EXIST));
		retcode = VE_FAIL;
	} else {
		if (dochown) {
			/* pcfs doesn't support file ownership */
			if (strcmp(vfsstatus.f_basetype, "pcfs") != 0 &&
			    chown(path, uid, gid) < 0) {
				retcode = VE_FAIL; /* chown failed */
			}
		}
	}

	if (retcode == VE_FAIL)
		reperr(pkg_gt(ERR_ATTRFAIL));
	return (retcode);
}

/*
 * This is a special fast verify which basically checks the attributes
 * and then, if all is OK, checks the size and mod time using the same
 * stat and statvfs structures.
 */
int
fverify(int fix, char *ftype, char *path, struct ainfo *ainfo,
    struct cinfo *cinfo)
{
	int retval;

	if ((retval = averify(fix, ftype, path, ainfo)) == 0) {
		if (*ftype == 'f' || *ftype == 'i') {
			if (cinfo->size != status.st_size) {
				logerr(pkg_gt(WRN_QV_SIZE), path);
				retval = VE_CONT;
			}
			/* pcfs doesn't support modification times */
			if (strcmp(vfsstatus.f_basetype, "pcfs") != 0)
				if (cinfo->modtime != status.st_mtime) {
					logerr(pkg_gt(WRN_QV_MTIME), path);
					retval = 0;
				}
		}
	}

	return (retval);
}

int
nonABI_symlinks(void)
{
	return (nonabi_symlinks);
}

void
set_nonABI_symlinks(void)
{
	nonabi_symlinks	= 1;
}
