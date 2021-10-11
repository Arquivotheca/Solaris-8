/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)pkgtrans.c	1.14	94/10/21 SMI"	/* SVr4.0  1.15.6.3	*/

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <dirent.h>
#include <signal.h>
#include <pkginfo.h>
#include <pkgstrct.h>
#include <pkgtrans.h>
#include <pkgdev.h>
#include <devmgmt.h>
#include <pkglib.h>
#include "pkglocale.h"

extern char	*pkgdir; 		/* pkgparam.c */

/* libadm.a */
extern char	*devattr(char *device, char *attribute);
extern char	*fpkginst(char *pkg, ...);
extern int	fpkginfo(struct pkginfo *info, char *pkginst);
extern int	getvol(char *device, char *label, int options, char *prompt);
extern int	_getvol(char *device, char *label, int options, char *prompt,
			char *norewind);

/* dstream.c */
extern int	ds_ginit(char *device);
extern int	ds_close(int pkgendflg);

#define	CPIOPROC	"/usr/bin/cpio"

#define	CMDSIZE	512	/* command block size */

#define	HDR_PREFIX	"# PaCkAgE DaTaStReAm\n"
#define	HDR_SUFFIX	"# end of header\n"

#define	BLK_SIZE	512	/* size of logical block */

#define	ENTRY_MAX	40	/* max size of entry for cpio cmd or header */

#define	PKGINFO	"pkginfo"
#define	PKGINFO_SIZE	7	/* strlen(PKGINFO) */
#define	PKGMAP	"pkgmap"
#define	PKGMAP_SIZE	6	/* strlen(PKGMAP) */
#define	MAP_STAT_SIZE	60	/* 1st line of pkgmap (3 numbers & a : */

#define	INSTALL	"install"
#define	RELOC	"reloc"
#define	ROOT	"root"
#define	ARCHIVE	"archive"
#define	MSG_TRANSFER	"Transferring <%s> package instance\n"
#define	MSG_RENAME 	"\t... instance renamed <%s> on destination\n"
#define	MSG_CORRUPT \
	"Volume is corrupt or is not part of the appropriate package."

#define	ERR_TRANSFER	"unable to complete package transfer"
#define	MSG_SEQUENCE	"- volume is out of sequence"
#define	MSG_MEM		"- no memory"
#define	MSG_CMDFAIL	"- process <%s> failed, exit code %d"
#define	MSG_POPEN	"- popen of <%s> failed, errno=%d"
#define	MSG_PCLOSE	"- pclose of <%s> failed, errno=%d"
#define	MSG_BADDEV	"- invalid or unknown device <%s>"
#define	MSG_GETVOL	"- unable to obtain package volume"
#define	MSG_NOSIZE 	"- unable to obtain maximum part size from pkgmap"
#define	MSG_CHDIR	"- unable to change directory to <%s>"
#define	MSG_STATDIR	"- unable to stat <%s>"
#define	MSG_CHOWNDIR	"- unable to chown <%s>"
#define	MSG_CHMODDIR	"- unable to chmod <%s>"
#define	MSG_FSTYP	"- unable to determine filesystem type for <%s>"
#define	MSG_NOTEMP	"- unable to create or use temporary directory <%s>"
#define	MSG_SAMEDEV	"- source and destination represent the same device"
#define	MSG_NOTMPFIL	"- unable to create or use temporary file <%s>"
#define	MSG_NOPKGMAP	"- unable to open pkgmap for <%s>"
#define	MSG_BADPKGINFO	"- unable to determine contents of pkginfo file"
#define	MSG_NOPKGS	"- no packages were selected from <%s>"
#define	MSG_MKDIR	"- unable to make directory <%s>"
#define	MSG_NOEXISTS	"- package instance <%s> does not exist on source " \
			"device"
#define	MSG_EXISTS	"- no permission to overwrite existing path <%s>"
#define	MSG_DUPVERS	"- identical version of <%s> already exists on " \
			"destination device"
#define	MSG_TWODSTREAM	"- both source and destination devices cannot be a " \
			"datastream"
#define	MSG_NOSPACE	"- not enough space on device"
#define	MSG_OPEN	"- open of <%s> failed, errno=%d"
#define	MSG_STATVFS	"- statvfs(%s) failed, errno=%d"

static struct	pkgdev srcdev, dstdev;
static char	*tmpdir;
static char	*tmppath;
static char	dstinst[16];
static char 	*ids_name, *ods_name;
static int	ds_volcnt;
static int	ds_volno;
static int	compressedsize, has_comp_size;

static void	(*func)();
static void	cleanup(void);
static void	sigtrap(int signo);
static int	rd_map_size(FILE *fp, int *npts, int *maxpsz, int *cmpsize);

static int	cat_and_count();

static int	ckoverwrite(char *dir, char *inst, int options);
static int	pkgxfer(char *srcinst, int options);
static int	wdsheader(char *src, char *device, char **pkg);

int		pkgtrans(char *device1, char *device2, char **pkg, int options);

extern int	ds_fd;	/* open file descriptor for data stream WHERE? */

static char *root_names[] = {
	"root",
	"root.cpio",
	"root.Z",
	"root.cpio.Z",
	0
};

static char *reloc_names[] = {
	"reloc",
	"reloc.cpio",
	"reloc.Z",
	"reloc.cpio.Z",
	0
};

char	**xpkg; 	/* array of transferred packages */
int	nxpkg;

static	char *allpkg[] = {
	"all",
	NULL
};

static char *hdrbuf;
static char *pinput, *nextpinput;

int
pkghead(char *device)
{
	char	*pt;
	int	n;

	cleanup();
	if (tmppath) {
		/* remove any previous tmppath stuff */
		rrmdir(tmppath);
		free(tmppath);
		tmppath = NULL;
	}

	if (device == NULL)
		return (0);
	else if ((device[0] == '/') && !isdir(device)) {
		pkgdir = device;
		return (0);
	} else if ((pt = devattr(device, "pathname")) != NULL && !isdir(pt)) {
		pkgdir = pt;
		return (0);
	}

	/* check for datastream */
	if (n = pkgtrans(device, (char *)0, allpkg, PT_SILENT|PT_INFO_ONLY))
		return (n);
		/* pkgtrans has set pkgdir */
	return (0);
}

static char *
mgets(char *buf, int size)
{
	nextpinput = strchr(pinput, '\n');
	if (nextpinput == NULL)
		return (0);
	*nextpinput = '\0';
	if ((int)strlen(pinput) > size)
		return (0);
	(void) strncpy(buf, pinput, strlen(pinput));
	buf[strlen(pinput)] = '\0';
	pinput = nextpinput + 1;
	return (buf);
}
/*
 * Here we construct the package size summaries for the headers. The
 * pkgmap file associated with fp must be rewound to the beginning of the
 * file. Note that we read three values from pkgmap first line in order
 * to get the *actual* size if this package is compressed.
 * This returns
 *	0 : error
 *	2 : not a compressed package
 *	3 : compressed package
 * and sets has_comp_size to indicate whether or not this is a compressed
 * package.
 */
static int
rd_map_size(FILE *fp, int *npts, int *maxpsz, int *cmpsize)
{
	int n;
	char line_buffer[MAP_STAT_SIZE];

	/* First read the null terminated first line */
	if (fgets(line_buffer, MAP_STAT_SIZE, fp) == NULL) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_NOSIZE));
		(void) fclose(fp);
		ecleanup();
		return (0);
	}

	n = sscanf(line_buffer, ": %d %d %d", npts, maxpsz, cmpsize);

	if (n == 3)		/* A valid compressed package entry */
		has_comp_size = 1;
	else if (n == 2)	/* A valid standard package entry */
		has_comp_size = 0;
	else {			/* invalid entry */
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_NOSIZE));
		(void) fclose(fp);
		ecleanup();
		return (0);
	}

	return (n);
}


/* will return 0, 1, 3, or 99 */
int
pkgtrans(char *device1, char *device2, char **pkg, int options)
{
	char	*src, *dst;
	int	errflg, i, n;

	func = signal(SIGINT, sigtrap);

	/* transfer spool to appropriate device */
	if (devtype(device1, &srcdev)) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_BADDEV), device1);
		return (1);
	}
	srcdev.rdonly++;


	/* check for datastream */
	ids_name = NULL;
	if (srcdev.bdevice) {
		if (n = _getvol(srcdev.bdevice, NULL, NULL,
		    pkg_gt("Insert %v into %p."), srcdev.norewind)) {
			cleanup();
			if (n == 3)
				return (3);
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_GETVOL));
			return (1);
		}
		if (ds_readbuf(srcdev.cdevice))
			ids_name = srcdev.cdevice;
	}

	if (srcdev.cdevice && !srcdev.bdevice)
		ids_name = srcdev.cdevice;
	else if (srcdev.pathname) {
		ids_name = srcdev.pathname;
		if (access(ids_name, 0) == -1) {
			progerr(ERR_TRANSFER);
			logerr(pkg_gt(MSG_GETVOL));
			return (1);
		}
	}

	if (!ids_name && device2 == (char *)0) {
		if (n = pkgmount(&srcdev, NULL, 1, 0, 0)) {
			cleanup();
			return (n);
		}
		if (srcdev.mount && *srcdev.mount)
			pkgdir = strdup(srcdev.mount);
		return (0);
	}

	if (ids_name && device2 == (char *)0) {
		tmppath = tmpnam(NULL);
		tmppath = strdup(tmppath);
		if (tmppath == NULL) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_MEM));
			return (1);
		}
		if (mkdir(tmppath, 0755)) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_MKDIR), tmppath);
			return (1);
		}
		device2 = tmppath;
	}

	if (devtype(device2, &dstdev)) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_BADDEV), device2);
		return (1);
	}

	if ((srcdev.cdevice && dstdev.cdevice) &&
	    strcmp(srcdev.cdevice, dstdev.cdevice) == 0) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_SAMEDEV));
		return (1);
	}

	ods_name = NULL;
	if (dstdev.cdevice && !dstdev.bdevice || dstdev.pathname)
		options |= PT_ODTSTREAM;

	if (options & PT_ODTSTREAM) {
		if (!((ods_name = dstdev.cdevice) != NULL ||
		    (ods_name = dstdev.pathname) != NULL)) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_BADDEV), device2);
			return (1);
		}
		if (ids_name) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_TWODSTREAM));
			return (1);
		}
	}

	if ((srcdev.dirname && dstdev.dirname) &&
	    strcmp(srcdev.dirname, dstdev.dirname) == 0) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_SAMEDEV));
		return (1);
	}

	if ((srcdev.pathname && dstdev.pathname) &&
	    strcmp(srcdev.pathname, dstdev.pathname) == 0) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_SAMEDEV));
		return (1);
	}

	if (ids_name) {
		if (srcdev.cdevice && !srcdev.bdevice &&
		(n = _getvol(srcdev.cdevice, NULL, NULL, NULL,
		    srcdev.norewind))) {
			cleanup();
			if (n == 3)
				return (3);
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_GETVOL));
			return (1);
		}
		if (srcdev.dirname = tmpnam(NULL))
			tmpdir = srcdev.dirname = strdup(srcdev.dirname);
		if ((srcdev.dirname == NULL) || mkdir(srcdev.dirname, 0755) ||
		    chdir(srcdev.dirname)) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_NOTEMP), srcdev.dirname);
			cleanup();
			return (1);
		}
		if (ds_init(ids_name, pkg, srcdev.norewind)) {
			cleanup();
			return (1);
		}
	} else if (srcdev.mount) {
		if (n = pkgmount(&srcdev, NULL, 1, 0, 0)) {
			cleanup();
			return (n);
		}
	}

	src = srcdev.dirname;
	dst = dstdev.dirname;

	if (chdir(src)) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_CHDIR), src);
		cleanup();
		return (1);
	}

	xpkg = pkg = gpkglist(src, pkg);
	if (!pkg) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_NOPKGS), src);
		cleanup();
		return (1);
	}
	for (nxpkg = 0; pkg[nxpkg]; /* void */)
		nxpkg++; /* count */

	if (ids_name)
		ds_order(pkg); /* order requests */

	if (options & PT_ODTSTREAM) {
		char line[128];

		if (!dstdev.pathname &&
		    (n = _getvol(ods_name, NULL, DM_FORMAT, NULL,
		    dstdev.norewind))) {
			cleanup();
			if (n == 3)
				return (3);
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_GETVOL));
			return (1);
		}
		if (wdsheader(src, ods_name, pkg)) {
			cleanup();
			return (1);
		}
		ds_volno = 1; /* number of volumes in datastream */
		pinput = hdrbuf;
		/* skip past first line in header */
		(void) mgets(line, 128);
	}

	errflg = 0;

	for (i = 0; pkg[i]; i++) {
		if (!(options & PT_ODTSTREAM) && dstdev.mount) {
			if (n = pkgmount(&dstdev, NULL, 0, 0, 1)) {
				cleanup();
				return (n);
			}
		}
		if (errflg = pkgxfer(pkg[i], options)) {
			pkg[i] = NULL;
			if ((options & PT_ODTSTREAM) || (errflg != 2))
				break;
		} else if (strcmp(dstinst, pkg[i]))
			pkg[i] = strdup(dstinst);
	}

	if (!(options & PT_ODTSTREAM) && dst)
		pkgdir = strdup(dst);
	cleanup();
	return (errflg);
}

/*
 * This function concatenates append to the text described in the buf_ctrl
 * structure. This code modifies data in this structure and handles all
 * allocation issues. It returns '0' if everything was successful and '1'
 * if not.
 */
static int
cat_and_count(struct dm_buf *buf_ctrl, char *append)
{
	char *text_buffer_org;

	if (buf_ctrl->offset + (int)strlen(append) >= buf_ctrl->allocation) {
		text_buffer_org = buf_ctrl->text_buffer;	/* save old */

		/* reallocate (and maybe move) text buffer */
		if ((buf_ctrl->text_buffer =
		    (char *)realloc(buf_ctrl->text_buffer,
		    buf_ctrl->allocation + BLK_SIZE)) == NULL) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_MEM));
			free(buf_ctrl->text_buffer);
			return (1);
		}

		/* adjust insertion pointer in case the buffer was moved */
		buf_ctrl->text_insert += (buf_ctrl->text_buffer -
		    text_buffer_org);

		/* clear the new memory */
		(void) memset(buf_ctrl->text_buffer + buf_ctrl->allocation,
		    '\0', BLK_SIZE);

		buf_ctrl->allocation += BLK_SIZE;  /* adjust total allocation */
		hdrbuf = buf_ctrl->text_buffer;	/* publish the text buffer */
	}

	while (*append) {
		*(buf_ctrl->text_insert) = *append++;
		(buf_ctrl->text_insert)++;
		(buf_ctrl->offset)++;
	}

	return (0);
}

static int
wdsheader(char *src, char *device, char **pkg)
{
	FILE	*fp;
	struct	dm_buf buf_ctrl;
	char	path[PATH_MAX], tmp_entry[ENTRY_MAX], tmp_file[L_tmpnam+1];
	int	i, n, nparts, maxpsize;
	int	list_fd;
	int	partcnt, totsize, block_cnt;
	struct stat statbuf;

	(void) ds_close(0);
	if (dstdev.pathname)
		ds_fd = creat(device, 0644);
	else
		ds_fd = open(device, 1);

	if (ds_fd < 0) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_OPEN), device, errno);
		return (1);
	}
	if (ds_ginit(device) < 0) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_OPEN), device, errno);
		(void) ds_close(0);
		return (1);
	}

	if ((buf_ctrl.text_buffer = (char *)malloc(BLK_SIZE)) == NULL) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_MEM));
		return (1);
	}

	hdrbuf = buf_ctrl.text_buffer;	/* publish the text buffer */

	/* clear the new memory */
	(void) memset(buf_ctrl.text_buffer, '\0', BLK_SIZE);

	/* set up the buffer control structure for the header */
	buf_ctrl.text_insert = buf_ctrl.text_buffer;
	buf_ctrl.offset = 0;
	buf_ctrl.allocation = BLK_SIZE;

	(void) cat_and_count(&buf_ctrl, HDR_PREFIX);

	nparts = maxpsize = 0;

	totsize = 0;
	for (i = 0; pkg[i]; i++)  {
		(void) sprintf(path, "%s/%s/%s", src, pkg[i], PKGINFO);
		if (stat(path, &statbuf) < 0) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_BADPKGINFO));
			ecleanup();
			return (1);
		}
		totsize += statbuf.st_size/BLK_SIZE + 1;
	}

	/*
	 * totsize contains number of blocks used by the pkginfo files
	 */
	totsize += i/4 + 1;
	if (dstdev.capacity && totsize > dstdev.capacity) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_NOSPACE));
		ecleanup();
		return (1);
	}

	ds_volcnt = 1;
	for (i = 0; pkg[i]; i++) {
		partcnt = 0;
		(void) sprintf(path, "%s/%s/%s", src, pkg[i], PKGMAP);
		if ((fp = fopen(path, "r")) == NULL) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_NOPKGMAP), pkg[i]);
			sighold(SIGINT);
			sigrelse(SIGINT);
			ecleanup();
			return (1);
		}

		/* Evaluate the first entry in pkgmap */
		n = rd_map_size(fp, &nparts, &maxpsize, &compressedsize);

		if (n == 3)	/* It's a compressed package */
			/* The header needs the *real* size */
			maxpsize = compressedsize;
		else if (n == 0)	/* pkgmap is corrupt */
			return (1);

		if (dstdev.capacity && maxpsize > dstdev.capacity) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_NOSPACE));
			(void) fclose(fp);
			ecleanup();
			return (1);
		}

		/* add pkg name, number of parts and the max part size */
		(void) sprintf(tmp_entry, "%s %d %d", pkg[i], nparts, maxpsize);
		if (cat_and_count(&buf_ctrl, tmp_entry)) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_MEM));
			(void) fclose(fp);
			ecleanup();
			return (1);
		}

		totsize += nparts * maxpsize;
		if (dstdev.capacity && dstdev.capacity < totsize) {
			int lastpartcnt = 0;
#if 0
			if (i != 0) {
				progerr(pkg_gt(ERR_TRANSFER));
				logerr(pkg_gt(MSG_NOSPACE));
				(void) fclose(fp);
				ecleanup();
				return (1);
			}
#endif	/* 0 */

			if (totsize)
				totsize -= nparts * maxpsize;
			while (partcnt < nparts) {
				while (totsize <= dstdev.capacity &&
				    partcnt <= nparts) {
					totsize +=  maxpsize;
					partcnt++;
				}
				/* partcnt == 0 means skip to next volume */
				if (partcnt)
					partcnt--;
				(void) sprintf(tmp_entry,
				    " %d", partcnt - lastpartcnt);
				if (cat_and_count(&buf_ctrl, tmp_entry)) {
					progerr(pkg_gt(ERR_TRANSFER));
					logerr(pkg_gt(MSG_MEM));
					(void) fclose(fp);
					ecleanup();
					return (1);
				}
				ds_volcnt++;
				totsize = 0;
				lastpartcnt = partcnt;
			}
			/* first parts/volume number does not count */
			ds_volcnt--;
		}

		if (cat_and_count(&buf_ctrl, "\n")) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_MEM));
			(void) fclose(fp);
			ecleanup();
			return (1);
		}

		(void) fclose(fp);
	}
	sighold(SIGINT);
	sigrelse(SIGINT);

	if (cat_and_count(&buf_ctrl, HDR_SUFFIX)) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_MEM));
		(void) fclose(fp);
		ecleanup();
		return (1);
	}

	/*
	 * The loop below assures compatibility with tapes that don't
	 * have a block size (e.g.: Exabyte) by forcing EOR at the end
	 * of each 512 bytes.
	 */
	for (block_cnt = 0; block_cnt < buf_ctrl.allocation;
	    block_cnt += BLK_SIZE)
		write(ds_fd, (buf_ctrl.text_buffer + block_cnt), BLK_SIZE);

	/*
	 * write the first cpio() archive to the datastream
	 * which should contain the pkginfo & pkgmap files
	 * for all packages
	 */
	(void) tmpnam(tmp_file);	/* temporary file name */
	if ((list_fd = open(tmp_file, O_RDWR | O_CREAT)) == -1) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_NOTMPFIL));
		return (1);
	}

	/*
	 * Create a cpio-compatible list of the requisite files in
	 * the temporary file.
	 */
	for (i = 0; pkg[i]; i++) {
		register ssize_t entry_size;

		/*
		 * Copy pkginfo and pkgmap filenames into the
		 * temporary string allowing for the first line
		 * as a special case.
		 */
		entry_size = sprintf(tmp_entry,
		    (i == 0) ? "%s/%s\n%s/%s" : "\n%s/%s\n%s/%s",
		    pkg[i], PKGINFO, pkg[i], PKGMAP);

		if (write(list_fd, tmp_entry, entry_size) != entry_size) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_NOTMPFIL));
			(void) close(list_fd);
			ecleanup();
			return (1);
		}
	}

	(void) lseek(list_fd, 0, SEEK_SET);

#ifndef SUNOS41
	(void) sprintf(tmp_entry, "%s -ocD -C %d", CPIOPROC, (int)BLK_SIZE);
#else
	(void) sprintf(tmp_entry, "%s -oc -C %d", CPIOPROC, (int)BLK_SIZE);
#endif

	if (n = esystem(tmp_entry, list_fd, ds_fd)) {
		rpterr();
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_CMDFAIL), tmp_entry, n);
		(void) close(list_fd);
		(void) unlink(tmp_file);
		cleanup();
		return (1);
	}
	(void) close(list_fd);
	(void) unlink(tmp_file);
	return (0);
}

static int
ckoverwrite(char *dir, char *inst, int options)
{
	char	path[PATH_MAX];

	(void) sprintf(path, "%s/%s", dir, inst);
	if (access(path, 0) == 0) {
		if (options & PT_OVERWRITE)
			return (rrmdir(path));
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_EXISTS), path);
		return (1);
	}
	return (0);
}

static int
pkgxfer(char *srcinst, int options)
{
	struct pkginfo info;
	FILE	*fp, *pp;
	char	*pt, *src, *dst;
	char	dstdir[PATH_MAX],
		temp[PATH_MAX],
		srcdir[PATH_MAX],
		cmd[CMDSIZE],
		pkgname[16];
	int	i, n, part, nparts, maxpartsize, curpartcnt, iscomp;
	char	volnos[128], tmpvol[128];
	struct	statvfs svfsb;
	long	free_blocks;
	struct	stat	srcstat;

	info.pkginst = NULL; /* required initialization */

	/*
	 * when this routine is entered, the first part of
	 * the package to transfer is already available in
	 * the directory indicated by 'src' --- unless the
	 * source device is a datstream, in which case only
	 * the pkginfo and pkgmap files are available in 'src'
	 */
	src = srcdev.dirname;
	dst = dstdev.dirname;

	if (!(options & PT_SILENT))
		(void) fprintf(stderr, pkg_gt(MSG_TRANSFER), srcinst);
	(void) strcpy(dstinst, srcinst);

	if (!(options & PT_ODTSTREAM)) {
		/* destination is a (possibly mounted) directory */
		(void) sprintf(dstdir, "%s/%s", dst, dstinst);

		/*
		 * need to check destination directory to assure
		 * that we will not be duplicating a package which
		 * already resides there (though we are allowed to
		 * overwrite the same version)
		 */
		pkgdir = src;
		if (fpkginfo(&info, srcinst)) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_NOEXISTS), srcinst);
			(void) fpkginfo(&info, NULL);
			return (1);
		}
		pkgdir = dst;

		(void) strcpy(temp, srcinst);
		if (pt = strchr(temp, '.'))
			*pt = '\0';
		(void) strcat(temp, ".*");

		if (pt = fpkginst(temp, info.arch, info.version)) {
			/*
			 * the same instance already exists, although
			 * its pkgid might be different
			 */
			if (options & PT_OVERWRITE) {
				(void) strcpy(dstinst, pt);
				(void) sprintf(dstdir, "%s/%s", dst, dstinst);
			} else {
				progerr(pkg_gt(ERR_TRANSFER));
				logerr(pkg_gt(MSG_DUPVERS), srcinst);
				(void) fpkginfo(&info, NULL);
				(void) fpkginst(NULL);
				return (2);
			}
		} else if (options & PT_RENAME) {
			/*
			 * find next available instance by appending numbers
			 * to the package abbreviation until the instance
			 * does not exist in the destination directory
			 */
			if (pt = strchr(temp, '.'))
				*pt = '\0';
			for (i = 2; (access(dstdir, 0) == 0); i++) {
				(void) sprintf(dstinst, "%s.%d", temp, i);
				(void) sprintf(dstdir, "%s/%s", dst, dstinst);
			}
		} else if (options & PT_OVERWRITE) {
			/*
			 * we're allowed to overwrite, but there seems
			 * to be no valid package to overwrite, and we are
			 * not allowed to rename the destination, so act
			 * as if we weren't given permission to overwrite
			 * --- this keeps us from removing a destination
			 * instance which is named the same as the source
			 * instance, but really reflects a different pkg!
			 */
			options &= (~PT_OVERWRITE);
		}
		(void) fpkginfo(&info, NULL);
		(void) fpkginst(NULL);

		if (ckoverwrite(dst, dstinst, options))
			return (2);

		if (isdir(dstdir) && mkdir(dstdir, 0755)) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_MKDIR), dstdir);
			return (1);
		}

		(void) sprintf(srcdir, "%s/%s", src, srcinst);
		if (stat(srcdir, &srcstat) != -1) {
			if (chown(dstdir, srcstat.st_uid, srcstat.st_gid) ==
			    -1) {
				progerr(pkg_gt(ERR_TRANSFER));
				logerr(pkg_gt(MSG_CHOWNDIR), dstdir);
				return (1);
			}
			if (chmod(dstdir, (srcstat.st_mode & S_IAMB)) == -1) {
				progerr(pkg_gt(ERR_TRANSFER));
				logerr(pkg_gt(MSG_CHMODDIR), dstdir);
				return (1);
			}
		} else {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_STATDIR), srcdir);
			return (1);
		}
	}

	if (!(options & PT_SILENT) && strcmp(dstinst, srcinst))
		(void) fprintf(stderr, pkg_gt(MSG_RENAME), dstinst);

	(void) sprintf(srcdir, "%s/%s", src, srcinst);
	if (chdir(srcdir)) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_CHDIR), srcdir);
		return (1);
	}

	if (ids_name) {	/* unpack the datatstream into a directory */
		/*
		 * transfer pkginfo & pkgmap first
		 */
		(void) sprintf(cmd, "%s -pudm %s", CPIOPROC, dstdir);
		if ((pp = epopen(cmd, "w")) == NULL) {
			rpterr();
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_POPEN), cmd, errno);
			return (1);
		}
		(void) fprintf(pp, "%s\n%s\n", PKGINFO, PKGMAP);
		sighold(SIGINT);
		if (epclose(pp)) {
			sigrelse(SIGINT);
			rpterr();
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_PCLOSE), cmd, errno);
			return (1);
		}
		sigrelse(SIGINT);

		if (options & PT_INFO_ONLY)
			return (0); /* don't transfer objects */

		if (chdir(dstdir)) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_CHDIR), dstdir);
			return (1);
		}

		/*
		 * for each part of the package, use cpio() to
		 * unpack the archive into the destination directory
		 */
		nparts = ds_findpkg(srcdev.cdevice, srcinst);
		if (nparts < 0) {
			progerr(pkg_gt(ERR_TRANSFER));
			return (1);
		}
		for (part = 1; part <= nparts; /* void */) {
			if (ds_getpkg(srcdev.cdevice, part, dstdir)) {
				progerr(pkg_gt(ERR_TRANSFER));
				return (1);
			}
			part++;
			if (dstdev.mount) {
				(void) chdir("/");
				if (pkgumount(&dstdev))
					return (1);
				if (part <= nparts) {
					if (n = pkgmount(&dstdev, NULL, part+1,
					    nparts, 1))
						return (n);
					if (ckoverwrite(dst, dstinst, options))
						return (1);
					if (isdir(dstdir) &&
					    mkdir(dstdir, 0755)) {
						progerr(
						    pkg_gt(ERR_TRANSFER));
						logerr(pkg_gt(MSG_MKDIR),
						    dstdir);
						return (1);
					}
					/*
					 * since volume is removable, each part
					 * must contain a duplicate of the
					 * pkginfo file to properly identify the
					 * volume
					 */
					if (chdir(srcdir)) {
						progerr(
						    pkg_gt(ERR_TRANSFER));
						logerr(pkg_gt(MSG_CHDIR),
						    srcdir);
						return (1);
					}
					if ((pp = epopen(cmd, "w")) == NULL) {
						rpterr();
						progerr(
						    pkg_gt(ERR_TRANSFER));
						logerr(pkg_gt(MSG_POPEN),
						    cmd, errno);
						return (1);
					}
					(void) fprintf(pp, "pkginfo");
					if (epclose(pp)) {
						rpterr();
						progerr(
						    pkg_gt(ERR_TRANSFER));
						logerr(pkg_gt(MSG_PCLOSE),
						    cmd, errno);
						return (1);
					}
					if (chdir(dstdir)) {
						progerr(
						    pkg_gt(ERR_TRANSFER));
						logerr(pkg_gt(MSG_CHDIR),
						    dstdir);
						return (1);
					}
				}
			}
		}
		return (0);
	}

	if ((fp = fopen(PKGMAP, "r")) == NULL) {
		progerr(pkg_gt(ERR_TRANSFER));
		logerr(pkg_gt(MSG_NOPKGMAP), srcinst);
		return (1);
	}

	nparts = 1;
	if (!rd_map_size(fp, &nparts, &maxpartsize, &compressedsize))
		return (1);
	else
		(void) fclose(fp);

	if (srcdev.mount) {
		if (ckvolseq(srcdir, 1, nparts)) {
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_SEQUENCE));
			return (1);
		}
	}

	/* write each part of this package */
	if (options & PT_ODTSTREAM) {
		char line[128];
		(void) mgets(line, 128);
		curpartcnt = -1;
		if (sscanf(line, "%s %d %d %[ 0-9]", &pkgname, &nparts,
		    &maxpartsize, volnos) == 4) {
			sscanf(volnos, "%d %[ 0-9]", &curpartcnt, tmpvol);
			strcpy(volnos, tmpvol);
		}
	}

	for (part = 1; part <= nparts; /* void */) {
		if (curpartcnt == 0 && (options & PT_ODTSTREAM)) {
			char prompt[128];
			int index;
			ds_volno++;
			(void) ds_close(0);
			(void) sprintf(prompt,
			    pkg_gt("Insert %%v %d of %d into %%p"),
			    ds_volno, ds_volcnt);
			if (n = getvol(ods_name, NULL, DM_FORMAT, prompt))
				return (n);
			if ((ds_fd = open(dstdev.cdevice, 1)) < 0) {
				progerr(pkg_gt(ERR_TRANSFER));
				logerr(pkg_gt(MSG_OPEN), dstdev.cdevice,
				    errno);
				return (1);
			}
			if (ds_ginit(dstdev.cdevice) < 0) {
				progerr(pkg_gt(ERR_TRANSFER));
				logerr(pkg_gt(MSG_OPEN), dstdev.cdevice,
				    errno);
				(void) ds_close(0);
				return (1);
			}

			(void) sscanf(volnos, "%d %[ 0-9]", &index, tmpvol);
			(void) strcpy(volnos, tmpvol);
			curpartcnt += index;
		}

		if (options & PT_INFO_ONLY)
			nparts = 0;

		if (part == 1) {
			(void) sprintf(cmd, "find %s %s", PKGINFO, PKGMAP);
			if (nparts && (isdir(INSTALL) == 0)) {
				(void) strcat(cmd, " ");
				(void) strcat(cmd, INSTALL);
			}
		} else
			(void) sprintf(cmd, "find %s", PKGINFO);

		if (nparts > 1) {
			(void) sprintf(temp, "%s.%d", RELOC, part);
			if (iscpio(temp, &iscomp) || isdir(temp) == 0) {
				(void) strcat(cmd, " ");
				(void) strcat(cmd, temp);
			}
			(void) sprintf(temp, "%s.%d", ROOT, part);
			if (iscpio(temp, &iscomp) || isdir(temp) == 0) {
				(void) strcat(cmd, " ");
				(void) strcat(cmd, temp);
			}
			(void) sprintf(temp, "%s.%d", ARCHIVE, part);
			if (isdir(temp) == 0) {
				(void) strcat(cmd, " ");
				(void) strcat(cmd, temp);
			}
		} else if (nparts) {
			for (i = 0; reloc_names[i] != NULL; i++) {
				if (iscpio(reloc_names[i], &iscomp) ||
				    isdir(reloc_names[i]) == 0) {
					(void) strcat(cmd, " ");
					(void) strcat(cmd, reloc_names[i]);
				}
			}
			for (i = 0; root_names[i] != NULL; i++) {
				if (iscpio(root_names[i], &iscomp) ||
				    isdir(root_names[i]) == 0) {
					(void) strcat(cmd, " ");
					(void) strcat(cmd, root_names[i]);
				}
			}
			if (isdir(ARCHIVE) == 0) {
				(void) strcat(cmd, " ");
				(void) strcat(cmd, ARCHIVE);
			}
		}
		if (options & PT_ODTSTREAM) {
#ifndef SUNOS41
			(void) sprintf(cmd+strlen(cmd),
			    " -print | %s -ocD -C %d",
#else
			(void) sprintf(cmd+strlen(cmd),
			    " -print | %s -oc -C %d",
#endif
				CPIOPROC, (int)BLK_SIZE);
		} else {
			if (statvfs(dstdir, &svfsb) == -1) {
				progerr(pkg_gt(ERR_TRANSFER));
				logerr(pkg_gt(MSG_STATVFS), dstdir, errno);
				return (1);
			}

			free_blocks = (((long)svfsb.f_frsize > 0) ?
			    howmany(svfsb.f_frsize, DEV_BSIZE) :
			    howmany(svfsb.f_bsize, DEV_BSIZE)) * svfsb.f_bavail;

			if ((has_comp_size ? compressedsize : maxpartsize) >
			    free_blocks) {
				progerr(pkg_gt(ERR_TRANSFER));
				logerr(pkg_gt(MSG_NOSPACE));
				return (1);
			}
			(void) sprintf(cmd+strlen(cmd), " -print | %s -pdum %s",
				CPIOPROC, dstdir);
		}


		n = esystem(cmd, -1, (options & PT_ODTSTREAM) ? ds_fd : -1);
		if (n) {
			rpterr();
			progerr(pkg_gt(ERR_TRANSFER));
			logerr(pkg_gt(MSG_CMDFAIL), cmd, n);
			return (1);
		}

		part++;
		if (srcdev.mount && (nparts > 1)) {
			/* unmount current source volume */
			(void) chdir("/");
			if (pkgumount(&srcdev))
				return (1);
			/* loop until volume is mounted successfully */
			while (part <= nparts) {
				/* read only */
				n = pkgmount(&srcdev, NULL, part, nparts, 1);
				if (n)
					return (n);
				if (chdir(srcdir)) {
					progerr(pkg_gt(ERR_TRANSFER));
					logerr(pkg_gt(MSG_CORRUPT), srcdir);
					(void) chdir("/");
					pkgumount(&srcdev);
					continue;
				}
				if (ckvolseq(srcdir, part, nparts)) {
					(void) chdir("/");
					pkgumount(&srcdev);
					continue;
				}
				break;
			}
		}
		if (!(options & PT_ODTSTREAM) && dstdev.mount) {
			/* unmount current volume */
			if (pkgumount(&dstdev))
				return (1);
			/* loop until next volume is mounted successfully */
			while (part <= nparts) {
				/* writable */
				n = pkgmount(&dstdev, NULL, part, nparts, 1);
				if (n)
					return (n);
				if (ckoverwrite(dst, dstinst, options))
					continue;
				if (isdir(dstdir) && mkdir(dstdir, 0755)) {
					progerr(pkg_gt(ERR_TRANSFER));
					logerr(pkg_gt(MSG_MKDIR), dstdir);
					continue;
				}
				break;
			}
		}

		if ((options & PT_ODTSTREAM) && part <= nparts) {
			if (curpartcnt >= 0 && part > curpartcnt) {
				char prompt[128];
				int index;
				ds_volno++;
				if (ds_close(0))
					return (1);
				(void) sprintf(prompt,
				    pkg_gt("Insert %%v %d of %d into %%p"),
				    ds_volno, ds_volcnt);
				if (n = getvol(ods_name, NULL, DM_FORMAT,
				    prompt))
					return (n);
				if ((ds_fd = open(dstdev.cdevice, 1)) < 0) {
					progerr(pkg_gt(ERR_TRANSFER));
					logerr(pkg_gt(MSG_OPEN),
					    dstdev.cdevice, errno);
					return (1);
				}
				if (ds_ginit(dstdev.cdevice) < 0) {
					progerr(pkg_gt(ERR_TRANSFER));
					logerr(pkg_gt(MSG_OPEN),
					    dstdev.cdevice, errno);
					(void) ds_close(0);
					return (1);
				}

				(void) sscanf(volnos, "%d %[ 0-9]", &index,
				    tmpvol);
				(void) strcpy(volnos, tmpvol);
				curpartcnt += index;
			}
		}

	}
	return (0);
}

static void
sigtrap(int signo)
{

	cleanup();

	if (tmppath) {
		rrmdir(tmppath);
		free(tmppath);
		tmppath = NULL;
	}
	if (func && (func != SIG_DFL) && (func != SIG_IGN))
		/* must have been an interrupt handler */
		(*func)(signo);
}

static void
cleanup(void)
{
	chdir("/");
	if (tmpdir) {
		rrmdir(tmpdir);
		free(tmpdir);
		tmpdir = NULL;
	}
	if (srcdev.mount && !ids_name)
		pkgumount(&srcdev);
	if (dstdev.mount && !ods_name)
		pkgumount(&dstdev);
	(void) ds_close(1);
}
