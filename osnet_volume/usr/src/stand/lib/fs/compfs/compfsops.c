/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)compfsops.c 1.37       99/03/19 SMI"

/*
 *	Composite filesystem for secondary boot
 *	that uses mini-MS-DOS filesystem
 */

#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/sysmacros.h>
#include <sys/promif.h>
#include <sys/stat.h>

#include <sys/fs/pc_fs.h>
#include <sys/fs/pc_label.h>
#include <sys/fs/pc_dir.h>
#include <sys/fs/pc_node.h>

#include <sys/bootcmn.h>
#include <sys/bootvfs.h>
#include <sys/bootdebug.h>
#include <sys/filemap.h>
#include <sys/ramfile.h>
#include <sys/dosemul.h>
#include <sys/doserr.h>
#include <sys/salib.h>

#define	whitespace(C)	((unsigned char)(C) <= ' ' ? 1 : 0)
#define	NAMEBUFSIZE 256

static char mapfile[] = MAPFILE;
static struct map_entry *map_listh = 0;
static struct map_entry *map_listt = 0;

static void cpfs_delete_entries(char *);
static void cpfs_build_map(void);
static int  cpfs_mapped(char *, char **, int *);
static void map_patch_dirents(char *path, dentbuf_list *dentslist);

static void dos_text(char *p, int count);

/* #define	COMPFS_OPS_DEBUG */

#ifdef COMPFS_OPS_DEBUG
static int	cpfsdebuglevel = 0;
static int	compfsverbose = 0;
#endif

#ifdef i386
#ifdef COMPFS_MOUNT_DEBUG
static int	compfsmountdbg = 0;
#define	Dprintf(x)	if (compfsmountdbg) printf x
#else
#define	Dprintf(x)
#endif	/* COMPFS_MOUNT_DEBUG */
#endif	/* i386 */

/*
 * exported functional prototypes
 */
static int	boot_compfs_mountroot(char *str);
static int	boot_compfs_unmountroot(void);
static int	boot_compfs_open(char *filename, int flags);
static int	boot_compfs_close(int fd);
static ssize_t	boot_compfs_read(int fd, caddr_t buf, size_t size);
static off_t	boot_compfs_lseek(int, off_t, int);
static int	boot_compfs_fstat(int fd, struct stat *stp);
static void	boot_compfs_closeall(int flag);
static int	boot_compfs_getdents(int, struct dirent *, unsigned);

off_t	boot_compfs_getpos(int fd);

struct boot_fs_ops boot_compfs_ops = {
	"compfs",
	boot_compfs_mountroot,
	boot_compfs_unmountroot,
	boot_compfs_open,
	boot_compfs_close,
	boot_compfs_read,
	boot_compfs_lseek,
	boot_compfs_fstat,
	boot_compfs_closeall,
	boot_compfs_getdents
};

extern struct boot_fs_ops *get_default_fs();
extern struct boot_fs_ops *get_fs_ops_pointer();
extern struct boot_fs_ops *extendfs_ops;
extern struct boot_fs_ops *origfs_ops;

extern struct boot_fs_ops boot_pcfs_ops;

extern int boot_pcfs_write(int fd, char *b, u_int cc);
extern int dosCreate(char *fn, int mode);
extern int validvolname(char *testname);

extern int strncasecmp(char *, char *, int);
extern int strcasecmp(char *, char *);

static struct compfsfile *compfshead = NULL;
static int	compfs_filedes = 1;	/* 0 is special */

typedef struct compfsfile {
	struct compfsfile *forw;	/* singly linked */
	int	fd;		/* the fd given out to caller */
	int	ofd;		/* original filesystem fd */
	int	efd;		/* extended filesystem fd */
	int	rfd;		/* cached RAM filesystem fd */
	off_t	offset;		/* for lseek maintenance */
	off_t	decomp_offset;	/* for decomp lseek maintenance */
	int	flags;
	char	compressed;	/* is file compressed */
	int	favoredfd;	/* Copy of the fd in use for the file */
	ssize_t	(*readmethod)(int, caddr_t, size_t);  /* Read function */
	int	(*statmethod)(int, struct stat *); /* Stat function */
	off_t	(*seekmethod)(int, off_t, int);	   /* Seek function */
	/*
	 * Remaining fields are all for facilitating supporting getdents()
	 * in this screwy filesystem overlay scheme.
	 */
	dentbuf_list  *dentslist;
	char	*filnam;
} compfsfile_t;

static void	compfs_assign_fd(compfsfile_t *fptr, int new);
static void	compfs_save_fn(compfsfile_t *fptr, char *fn);
static int	is_compressed(compfsfile_t *fp, int closing);
static int	follow_link(compfsfile_t *fp);

extern int	decompress_init();
extern void	decompress_fini();
extern void	decompress();

static int	decomp_init();
static void	decomp_fini();
static void	decomp_file(compfsfile_t *);
static int	decomp_lseek(compfsfile_t *, off_t, int);
static int	decomp_fstat(compfsfile_t *, struct stat *);
static ssize_t	decomp_read(compfsfile_t *, caddr_t, size_t);
static void	decomp_close(compfsfile_t *);
static void	decomp_closeall(int);
static void	decomp_free_data();

/*
 * Global notion of the volume label assigned to the root and the current
 * volume under examination.
 *
 * PCFS will set boot_root_writable as appropriate after the first
 * successful mountroot.
 *
 * PCFS will set rootvol after the first successful mountroot.
 * All other file system types leave it unchanged from an empty string.
 *
 * PCFS will change curvol as the current volume changes.
 */
char *rootvol = "";
char *curvol = "";
int  boot_root_writable = 0;

/*
 * File ops by realmode modules need an extra layer of error reporting we
 * sometimes help observe.
 */
extern	ushort	File_doserr;

/*
 * Given an fd, do a search (in this case, linear linked list search)
 * and return the matching compfsfile pointer or NULL;
 * By design, when fd is -1, we return a free compfsfile structure (if any).
 */

static struct compfsfile *
get_fileptr(int fd)
{
	struct compfsfile *fileptr = compfshead;

	while (fileptr != NULL) {
		if (fd == fileptr->fd)
			break;
		fileptr = fileptr->forw;
	}
#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 6)
		printf("compfs: returning fileptr 0x%x\n", fileptr);
#endif
	return (fileptr);
}

static void
cpfs_build_map(void)
{
	int	mapfid;
	char	*bp;
	char	*bufend;
	char	*cp;
	char	buffer[PC_SECSIZE];
	char	dospath[MAXNAMELEN + 1];
	char	fspath[MAXNAMELEN + 1];
	int	rcount;
	int	dosplen, fsplen;
	int	flags = 0;
	int	mflags;
#ifdef DECOMP_DEBUG
	int	decomp_files = 0;
#endif

	if ((mapfid = (*extendfs_ops->fsw_open)(mapfile, flags)) < 0) {
#ifdef COMPFS_OPS_DEBUG
		if (cpfsdebuglevel > 2)
			printf("compfs: open %s file failed\n", mapfile);
#endif
		return;
	}

	if ((rcount =
	    (*extendfs_ops->fsw_read)(mapfid, buffer, PC_SECSIZE)) <= 0) {
		goto mapend;
	}

	bp = buffer;
	bufend = buffer + rcount;

	do {	/* for each line in map file */

		*fspath = '\0';
		fsplen = 0;
		*dospath = '\0';
		dosplen = 0;

		while (whitespace(*bp)) {
			if (++bp >= bufend) {
				if ((rcount =
				    (*extendfs_ops->fsw_read)
					(mapfid, buffer, PC_SECSIZE)) <= 0)
					goto mapend;

				bp = buffer;
				bufend = buffer + rcount;
			}
		}
		if (*bp == '#')
			/* skip over comment lines */
			goto mapskip;

		cp = fspath;
#ifdef	notdef
		if (*bp != '/') {
			/*
			 * fs pathname does not begin with '/'
			 * so prepend with current path. More Work??
			 */
		}
#endif
		while (!whitespace(*bp) && fsplen < MAXNAMELEN) {
			*cp++ = *bp++;
			if (bp >= bufend) {
				if ((rcount =
				    (*extendfs_ops->fsw_read)
				    (mapfid, buffer, PC_SECSIZE)) <= 0)
					goto mapend;

				bp = buffer;
				bufend = buffer + rcount;
			}
			fsplen++;
		}
		*cp = '\0';

		while (whitespace(*bp)) {
			if (*bp == '\n')
				goto mapskip;
			if (++bp >= bufend) {
				if ((rcount = (*extendfs_ops->fsw_read)(mapfid,
				    buffer, PC_SECSIZE)) <= 0)
					goto mapend;

				bp = buffer;
				bufend = buffer + rcount;
			}
		}

		cp = dospath;
		if (*bp != '/' && *bp != '\\' && *bp != '[' && *bp != ':') {
			/*
			 * DOS pathname does not begin with '\' or a volume
			 * name, so prepend with boot volume name and root.
			 */
			(void) strcpy(cp, rootvol);
			cp += strlen(rootvol);
			*cp++ = ':';
			*cp++ = '\\';
			dosplen = 1;
		} else if (*bp == '[' || *bp == ':') {
			/*
			 * A volume is specified, prepend it to the name
			 * with a following colon.
			 */
			bp++;
			while (*bp != ']' && *bp != ':' &&
			    dosplen < MAXNAMELEN) {
				/* Copy the volume name into the path */
				*cp++ = *bp++;
				if (bp >= bufend) {
					if ((rcount =
					    (*extendfs_ops->fsw_read)
					    (mapfid, buffer, PC_SECSIZE)) <= 0)
						goto mapend;

					bp = buffer;
					bufend = buffer + rcount;
				}
				dosplen++;
			}
			bp++; *cp++ = ':'; dosplen++;
		}

		while (!whitespace(*bp) && dosplen < MAXNAMELEN) {
			*cp++ = *bp++;
			if (bp >= bufend) {
				if ((rcount =
				    (*extendfs_ops->fsw_read)(mapfid,
					buffer, PC_SECSIZE)) <= 0)
					goto mapend;

				bp = buffer;
				bufend = buffer + rcount;
			}
			dosplen++;
		}
		*cp = '\0';

		mflags = 0;
		while (*bp != '\n') {
			if (*bp == 'c')
				mflags |= COMPFS_AUGMENT;
			else if (*bp == 't')
				mflags |= COMPFS_TEXT;
			else if (*bp == 'p')
				mflags |= COMPFS_PATH;
			else if (*bp == 'z')
				mflags |= COMPFS_DECOMP;

			if (++bp >= bufend) {
				if ((rcount =
				    (*extendfs_ops->fsw_read)
					(mapfid, buffer, PC_SECSIZE)) <= 0)
					goto mapend;

				bp = buffer;
				bufend = buffer + rcount;
			}
		}

		cpfs_add_entry(fspath, dospath, mflags);

#ifdef COMPFS_OPS_DEBUG
		if (compfsverbose || cpfsdebuglevel > 2) {
			if (mflags & COMPFS_AUGMENT)
				printf("compfs: %s augmented with %s\n",
				    fspath, dospath);
			else {
				printf("compfs: %s mapped to %s\n",
				    fspath, dospath);
			}
#ifdef DECOMP_DEBUG
			if (mflags & COMPFS_DECOMP) {
				decomp_files++;
				printf("compfs: %s is compressed\n", dospath);
			}
#endif
		}
#endif

mapskip:
		while (*bp != '\n')
			if (++bp >= bufend) {
				if ((rcount = (*extendfs_ops->fsw_read)(mapfid,
				    buffer, PC_SECSIZE)) <= 0)
					goto mapend;

				bp = buffer;
				bufend = buffer + rcount;
			}
	/*CONSTANTCONDITION*/
	} while (1);
mapend:
	(*extendfs_ops->fsw_close)(mapfid);

#ifdef DECOMP_DEBUG
	if (compfsverbose || cpfsdebuglevel > 2) {
		printf("%d of the DOS files are compressed\n", decomp_files);
	}
#endif

#ifdef COMPFS_OPS_DEBUG
	if (compfsverbose || cpfsdebuglevel > 2) {
		printf("\n");
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif
}

void
cpfs_add_entry(char *fspath, char *dospath, int flags)
{
	struct map_entry	*mlp;
	char 	*cp;
	int	fsplen, dosplen;

	fsplen = strlen(fspath);
	dosplen = strlen(dospath);

	mlp = (struct map_entry *)bkmem_alloc(
	    sizeof (struct map_entry) + fsplen + dosplen + 2);

	cp = (char *)(mlp + 1);
	bcopy(fspath, cp, fsplen + 1);
	mlp->target = cp;

	cp = (char *)(cp + fsplen + 1);
	bcopy(dospath, cp, dosplen + 1);
	mlp->source = cp;

	mlp->flags = flags;

	/*
	 * This entry overrides previous entries for the same file.
	 */
	cpfs_delete_entries(fspath);

	/*
	 * insert new entry into linked list
	 */
	mlp->link = NULL;
	if (!map_listh) {
		map_listh = mlp;
	}
	if (map_listt) {
		map_listt->link = mlp;
	}
	map_listt = mlp;

#ifdef COMPFS_OPS_DEBUG
	if (compfsverbose || cpfsdebuglevel > 2) {
		if (mlp->flags & COMPFS_AUGMENT)
			printf("compfs: %s augmented with %s\n",
			    fspath, dospath);
		else
			printf("compfs: %s mapped to %s\n",
			    fspath, dospath);
	}
#endif

}

static void
cpfs_delete_entries(char *fspath)
{
	struct map_entry	*mlp = map_listh;
	struct map_entry	*map_listp = 0;
	struct map_entry	*rem_mlp;

	while (mlp) {
		if ((strcmp(fspath, mlp->target) == 0) &&
		    (!(mlp->flags & COMPFS_PATH))) {
			if (mlp == map_listh)
				map_listh = mlp->link;
			if (mlp == map_listt)
				map_listt = map_listp;
			if (map_listp)
				map_listp->link = mlp->link;
			rem_mlp = mlp;
			mlp = mlp->link;
			bkmem_free((caddr_t)rem_mlp,
			    sizeof (struct map_entry) +
			    strlen(rem_mlp->target) +
			    strlen(rem_mlp->source) + 2);
		} else {
			map_listp = mlp;
			mlp = mlp->link;
		}
	}
}

void
cpfs_show_entries(void)
{
	struct map_entry	*mlp;

	for (mlp = map_listh; mlp; mlp = mlp->link) {
		printf("%s ", mlp->source);
		if (mlp->flags & COMPFS_AUGMENT)
			printf("EXTENDS ");
		else
			printf("REPLACES ");
		if (mlp->flags & COMPFS_TEXT)
			printf("w/ UNIX EOL ");
		printf("%s\n", mlp->target);
	}
}

static int
cpfs_mapped(char *str, char **dos_str, int *flagp)
{
	static char *strremap[2];
	struct map_entry *mlp;
	char *restp;
	int mapidx;
	int remapped = 0;
	int pathflags;
	int spcleft;

	if (!strremap[0] && !(strremap[0] = (char *)bkmem_alloc(MAXNAMELEN))) {
		/* Something's really awry */
		prom_panic("No memory to build a remapped path");
	}

	if (!strremap[1] && !(strremap[1] = (char *)bkmem_alloc(MAXNAMELEN))) {
		/* Something's really awry */
		prom_panic("No memory to build a remapped path");
	}

	strremap[0][0] = '\0';
	strremap[1][0] = '\0';
	mapidx = 1;

	/*
	 *  First apply any directory re-mappings
	 *  Note that the effects of re-mappings can be cumulative.
	 */
	for (mlp = map_listh; mlp; mlp = mlp->link) {
		if ((mlp->flags & COMPFS_PATH) &&
		    (strncmp(mlp->target, str, strlen(mlp->target))) == 0) {
#ifdef COMPFS_OPS_DEBUG
			if (cpfsdebuglevel > 4) {
				printf("(%s) matches (%s)\n", str, mlp->target);
				printf("use (%s) instead\n", mlp->source);
			}
#endif
			mapidx = (mapidx + 1)%2;
			strncpy(strremap[mapidx], mlp->source, MAXNAMELEN - 1);
			strremap[mapidx][MAXNAMELEN-1] = '\0';

			restp = &(str[strlen(mlp->target)]);
			spcleft = MAXNAMELEN - 1 - strlen(strremap[mapidx]);
			strncat(strremap[mapidx], restp, spcleft);
			strremap[mapidx][MAXNAMELEN-1] = '\0';
			pathflags = mlp->flags;
			remapped = 1;
#ifdef COMPFS_OPS_DEBUG
			if (cpfsdebuglevel > 4) {
				printf("(%s)->(%s)\n", str, strremap[mapidx]);
			}
#endif
			str = strremap[mapidx];
		}
	}

	/*
	 * Be prepared to handle an augmentation or replacement of this
	 * pathname; but also be prepared to send back the new path as
	 * is if no further mappings apply.
	 */
	if (remapped) {
		*dos_str = str;
		*flagp = pathflags;
	}

	for (mlp = map_listh; mlp; mlp = mlp->link) {

		if (strcasecmp(mlp->target, str) == 0) {
#ifdef COMPFS_OPS_DEBUG
			if (cpfsdebuglevel > 4) {
				printf("(%s)->(%s)\n", str, mlp->source);
			}
#endif
			*dos_str = mlp->source;
			*flagp = mlp->flags;
			return (1);
		}
	}

	/*
	 *  Finally, check for an implicit remapping, when a volume specifier
	 *  has been attached to the path!  Do not count the special R:
	 *  modifier that is used to request file caching.
	 */
	if (!remapped) {
		char *eov = strchr(str, ':');

		if (eov && validvolname(str) && (eov - str <= VOLLABELSIZE)) {
			*eov = '\0';
			if (strcasecmp(str, "r") != 0) {
				remapped = 1;
				*dos_str = str;
			}
			*eov = ':';
		}
	}

	return (remapped);
}

/*
 *  origfs_to_ramfs
 *	Convert a file named 'oname' to a RAMfile.
 *	Return the pointer to the created compfs file descriptor.
 */
static int
origfs_to_ramfs(char *oname, int flags)
{
	struct compfsfile *rfp;
	char *fbuf;
	int rbytes, wbytes;
	int new = 0;
	int rfd = -1;

	if ((rfp = get_fileptr(0)) == NULL) {
		if ((rfp = (compfsfile_t *)
		    bkmem_alloc(sizeof (compfsfile_t))) ==
		    (compfsfile_t *)NULL) {
			prom_panic("No memory for file descriptor");
		}
		new++;
	}

	rfp->flags = 0;
	rfp->offset = 0;
	rfp->compressed = -1;  /* Initially undetermined, 0=Not, 1=Is */
	rfp->dentslist = NULL;
	rfp->readmethod = NULL;
	rfp->statmethod = NULL;
	rfp->seekmethod = NULL;
	rfp->fd = rfp->rfd = rfp->efd = rfp->ofd = rfp->favoredfd = -1;

	/*
	 *  Attempt to open the target file first.  Can't really append
	 *  to a file that doesn't even exist.
	 */
	if ((rfp->ofd = (*origfs_ops->fsw_open)(oname, flags)) < 0) {
		if (new)
			bkmem_free((caddr_t)rfp, sizeof (compfsfile_t));
		else
			rfp->fd = 0;	/* makes descriptor reusable */
		return (rfd);
	}
	compfs_assign_fd(rfp, new);

	/*
	 *  Create an empty RAMfile.  If the file already exists, it will
	 *  be truncated.
	 */
	if ((rfd = RAMfile_create(oname, flags)) < 0) {
		/* WAAAAA!  For now we'll just have this fail */
		boot_compfs_close(rfp->fd);
		File_doserr = DOSERR_INSUFFICIENT_MEMORY;
		return (rfd);
	}

	if ((fbuf = (char *)bkmem_alloc(RAMfile_BLKSIZE))) {
		while ((rbytes =
		    boot_compfs_read(rfp->fd, fbuf, RAMfile_BLKSIZE)) > 0) {
			wbytes = RAMfile_write(rfd, fbuf, rbytes);
			if (wbytes != rbytes) {
				printf("WARNING: Write failure ");
				printf("during conversion to ");
				printf("RAM file!\n");
				break;
			}
		}
		RAMfile_clear_modbit(rfd);
		bkmem_free(fbuf, RAMfile_BLKSIZE);
		boot_compfs_close(rfp->fd);
	} else {
		(void) RAMfile_close(rfd);
		(void) RAMfile_destroy(oname);
		File_doserr = DOSERR_INSUFFICIENT_MEMORY;
		boot_compfs_close(rfp->fd);
		return (-1);
	}

	return (rfd);
}

/*
 *  extendfs_to_ramfs
 *	Convert a file named 'ename' to a RAMfile.
 *	Return the pointer to the created compfs file descriptor.
 */
static int
extendfs_to_ramfs(char *ename, int mflags, int flags)
{
	struct compfsfile *rfp;
	char *fbuf;
	int rbytes, wbytes;
	int new = 0;
	int rfd;

	if ((rfp = get_fileptr(0)) == NULL) {
		if ((rfp = (compfsfile_t *)
		    bkmem_alloc(sizeof (compfsfile_t))) ==
		    (compfsfile_t *)NULL) {
			prom_panic("No memory for file descriptor");
		}
		new++;
	}

	rfd = -1;

	rfp->flags = 0;
	rfp->offset = 0;
	rfp->compressed = -1;  /* Initially undetermined, 0=Not, 1=Is */
	rfp->dentslist = NULL;
	rfp->readmethod = NULL;
	rfp->statmethod = NULL;
	rfp->seekmethod = NULL;
	rfp->fd = rfp->rfd = rfp->efd = rfp->ofd = rfp->favoredfd = -1;

	/*
	 *  Attempt to open the target file first.  Can't really append
	 *  to a file that doesn't even exist.
	 */
	if ((rfp->efd = (*extendfs_ops->fsw_open)(ename, flags)) < 0) {
		if (new)
			bkmem_free((caddr_t)rfp, sizeof (compfsfile_t));
		else
			rfp->fd = 0;	/* makes descriptor reusable */
		return (rfd);
	}
	compfs_assign_fd(rfp, new);

	/*
	 *  Create an empty RAMfile.  If the file already exists, it will
	 *  be truncated.
	 */
	if ((rfd = RAMfile_create(ename, flags)) < 0) {
		/* WAAAAA!  For now we'll just have this fail */
		boot_compfs_close(rfp->fd);
		File_doserr = DOSERR_INSUFFICIENT_MEMORY;
		return (rfd);
	}

	if ((fbuf = (char *)bkmem_alloc(RAMfile_BLKSIZE))) {
		while ((rbytes =
		    boot_compfs_read(rfp->fd, fbuf, RAMfile_BLKSIZE)) > 0) {
			/*
			 * Handle 't' flag of mapping
			 */
			if (mflags & COMPFS_TEXT)
				dos_text(fbuf, rbytes);

			wbytes = RAMfile_write(rfd, fbuf, rbytes);
			if (wbytes != rbytes) {
				printf("WARNING: Write failure ");
				printf("during conversion to ");
				printf("RAM file!\n");
				break;
			}
		}
		RAMfile_clear_modbit(rfd);
		bkmem_free(fbuf, RAMfile_BLKSIZE);
		boot_compfs_close(rfp->fd);
	} else {
		(void) RAMfile_close(rfd);
		(void) RAMfile_destroy(ename);
		File_doserr = DOSERR_INSUFFICIENT_MEMORY;
		boot_compfs_close(rfp->fd);
		return (-1);
	}
	return (rfd);
}

/*
 *  compfs_augment_convert
 *	Merge the concatenation file with the original file into a single RAM
 *	file.
 */
static int
compfs_augment_convert(char *oname, char *ename, int flags, int mflags)
{
	compfsfile_t *nfp;
	char *fbuf;
	int rbytes, wbytes;
	int new = 0;
	int rfd;

	if (((rfd = RAMfile_open(oname, flags)) >= 0) ||
	    ((rfd = origfs_to_ramfs(oname, flags)) >= 0)) {
		if ((nfp = get_fileptr(0)) == NULL) {
			if ((nfp = (compfsfile_t *)
			    bkmem_alloc(sizeof (compfsfile_t))) ==
			    (compfsfile_t *)NULL) {
				prom_panic("No memory for file descriptor");
			}
			new++;
		}

		nfp->flags = 0;
		nfp->offset = 0;
		nfp->compressed = -1;  /* Initially undetermined, 0=Not, 1=Is */
		nfp->dentslist = NULL;
		nfp->readmethod = NULL;
		nfp->statmethod = NULL;
		nfp->seekmethod = NULL;
		nfp->fd = nfp->rfd = nfp->efd = nfp->ofd = nfp->favoredfd = -1;

		/*
		 *  Attempt to open the extension file.
		 */
		if ((nfp->rfd = RAMfile_open(ename, flags)) < 0) {
			if ((nfp->efd =
			    (*extendfs_ops->fsw_open)(ename, flags)) < 0) {
				if (new)
					bkmem_free((caddr_t)nfp,
					    sizeof (compfsfile_t));
				else
					nfp->fd = 0;  /* Mark as re-usable */
				printf("WARNING: No extension file ");
				printf("to append!\n");
				goto leave;
			}
		}
		compfs_assign_fd(nfp, new);

		/*
		 *  Append to RAMfile.
		 */
		(void) RAMfile_lseek(rfd, 0, SEEK_END);
		if ((fbuf = (char *)bkmem_alloc(RAMfile_BLKSIZE))) {
			while ((rbytes =
			    boot_compfs_read(nfp->fd,
				fbuf, RAMfile_BLKSIZE)) > 0) {
				/*
				 * Handle 't' flag of mapping
				 */
				if (mflags & COMPFS_TEXT)
					dos_text(fbuf, rbytes);

				wbytes = RAMfile_write(rfd, fbuf, rbytes);
				if (wbytes != rbytes) {
					printf("WARNING: Write failure ");
					printf("during conversion to ");
					printf("RAM file!\n");
					break;
				}
			}
			RAMfile_clear_modbit(rfd);
			bkmem_free(fbuf, RAMfile_BLKSIZE);
		} else {
			File_doserr = DOSERR_INSUFFICIENT_MEMORY;
			printf("WARNING: Append of extension file ");
			printf("failed!\n");
		}

		boot_compfs_close(nfp->fd);
leave:		RAMrewind(rfd);
	}
	return (rfd);
}

#ifdef i386
void
boot_compfs_getvolname(char *buf)
{
	char *vp;
	int i;

	vp = curvol;
	for (i = 0; i < VOLLABELSIZE && *vp; i++)
		buf[i] = *vp++;
	for (; i < VOLLABELSIZE; i++)
		buf[i] = ' ';
	buf[VOLLABELSIZE] = '\0';
}
#endif

/*
 *  This define, if set, will cause us to add leading slashes to paths
 *  that don't have them, turning a relative path into an absolute one.
 */
#define	ENFORCE_ABSOLUTE_PATH 1

/*
 *  tidy_name -- remove excess slashes and trailing slashes from the path
 *	name passed to us.
 */
static char *
tidy_name(char *str, int *bufsiz)
{
	char *np, *c, *w;

	/*
	 * The string we are going to return will have a maximum length of
	 * the string's current length plus one for the trailing null and
	 * the worst case is that we have to add a leading slash.
	 */
	*bufsiz = strlen(str)+2;

	if ((np = bkmem_alloc(*bufsiz)) == 0) {
		*bufsiz = 0;
		return (str);
	}

	c = np;

	w = str;
#ifdef ENFORCE_ABSOLUTE_PATH
	if (*w != '/' && *w != '\\') {
		char *eov = strchr(w, ':');

		if (!eov || !(validvolname(w)) ||
		    (eov - w > VOLLABELSIZE)) {
			*c++ = '/';
		} else {
			while (w <= eov) {
				*c++ = *w++;
			}
			if (*w != '/' && *w != '\\') {
				*c++ = '/';
			}
		}
	}
#endif	/* ENFORCE_ABSOLUTE_PATH */

	for (; *w; w++, c++) {
		*c = *w;
		if (*w == '/' || *w == '\\') {
			while (*w == '/' || *w == '\\')
				w++;
			w--;
		}
	}

	/* Copy the null */
	*c = *w;

	/* Now, check for trailing backslash */
	if (*np && (np != c - 1) && (*(c-1) == '/' || *(c-1) == '\\'))
		*(c-1) = '\0';

	return (np);
}

static void
chk_write_to_readonly(int *attr)
{
	if ((*attr & DOSACCESS_WRONLY) || (*attr & DOSACCESS_RDWR)) {
		if (!boot_root_writable)
			*attr |= DOSFD_RAMFILE;
	}
}

static int
boot_compfs_open(char *str, int flags)
{
	struct compfsfile *fptr;
	char	*dos_str = (char *)NULL;
	char	*tname, *name;
	int	mustconvert;
	int	ismapped;
	int	mflags = 0;
	int	tsize = 0;
	int	new = 0;

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 4) {
		printf("compfs_open(): open %s flag=0x%x\n",
			str, flags);
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif

	if ((fptr = get_fileptr(0)) == NULL) {
		if ((fptr = (compfsfile_t *)
		    bkmem_alloc(sizeof (compfsfile_t))) ==
		    (compfsfile_t *)NULL) {
			prom_panic("No memory for file descriptor");
		}
		new++;
	}

	fptr->flags = 0;
	fptr->offset = 0;
	fptr->compressed = -1;  /* Initially undetermined, 0=Not, 1=Is */
	fptr->dentslist = NULL;
	fptr->readmethod = NULL;
	fptr->statmethod = NULL;
	fptr->seekmethod = NULL;
	fptr->fd = fptr->rfd = fptr->efd = fptr->ofd = fptr->favoredfd = -1;

	/*
	 * Certain open modes are going to require existing files be
	 * converted to RAM files
	 */
	chk_write_to_readonly(&flags);
	mustconvert = flags & DOSFD_RAMFILE;

	name = tname = tidy_name(str, &tsize);

	ismapped = cpfs_mapped(name, &dos_str, &mflags);

	if (ismapped) {
		if (mflags & COMPFS_AUGMENT) {
			/*
			 * Extensions are generally small, so we go ahead
			 * and just convert them to RAMfiles (if they aren't
			 * already converted)
			 */
			if ((fptr->rfd = compfs_augment_convert(name, dos_str,
			    flags, mflags)) >= 0) {
				cpfs_delete_entries(name);
			}
		} else {
			name = dos_str;
			fptr->rfd = RAMfile_open(dos_str, flags);
			if (fptr->rfd < 0 && mustconvert) {
				fptr->rfd =
					extendfs_to_ramfs(dos_str,
					    flags, mflags);
				if (fptr->rfd >= 0) {
					RAMrewind(fptr->rfd);
					if (flags & DOSFD_NOSYNC)
						RAMfile_set_cachebit(fptr->rfd);
				}
			} else if (fptr->rfd < 0) {
				fptr->efd =
				    (*extendfs_ops->fsw_open)(dos_str, flags);
				fptr->flags |= mflags;
				/*
				 * One last chance -- they may be trying to
				 * open a directory that only exists in the
				 * RAMfs.
				 */
				if (fptr->efd < 0) {
					fptr->rfd = RAMfile_diropen(dos_str);
				}
			}
		}
	} else if (mustconvert) {
		if ((fptr->rfd = RAMfile_open(name, flags)) < 0) {
			if ((fptr->rfd =
			    origfs_to_ramfs(name, flags)) >= 0) {
				RAMrewind(fptr->rfd);
				if (flags & DOSFD_NOSYNC)
					RAMfile_set_cachebit(fptr->rfd);
			}
		}
	} else if (strncasecmp(name, "R:", 2) == 0) {
		/*
		 * Anything opened with R: should always be cached
		 * from the source file.  This allows for the case
		 * where we need to replace what we cached with something
		 * else.  Moral is that you should only ever use the R:
		 * specifier on the initial caching open.
		 */
		if ((fptr->rfd = origfs_to_ramfs(name, flags)) >= 0) {
			RAMrewind(fptr->rfd);
			RAMfile_set_cachebit(fptr->rfd);
		} else if ((fptr->rfd =
				extendfs_to_ramfs(name, flags, mflags)) >= 0) {
			RAMrewind(fptr->rfd);
			RAMfile_set_cachebit(fptr->rfd);
		}
	} else {
		if ((fptr->rfd = RAMfile_open(name, flags)) < 0) {
			fptr->ofd =
				(*origfs_ops->fsw_open)(name, flags);
		}
		/*
		 * One last chance -- they may be trying to open a
		 * directory that only exists in the RAMfs.
		 */
		if (fptr->ofd < 0 && fptr->efd < 0 && fptr->rfd < 0) {
			fptr->rfd = RAMfile_diropen(name);
		}
	}

	if (fptr->ofd < 0 && fptr->efd < 0 && fptr->rfd < 0) {
		if (new)
			bkmem_free((caddr_t)fptr, sizeof (compfsfile_t));
		else
			fptr->fd = 0;  /* mark as reusable */

		if (tsize > 0) {
			bkmem_free(tname, tsize);
		}

		return (-1);

	} else {
		fptr->flags |= flags;
		compfs_assign_fd(fptr, new);
		compfs_save_fn(fptr, name);
	}

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 2) {
		printf("compfs_open(): compfs fd = %d, origfs file \"%s\", "
			"tidied name \"%s\", fd %d, dos file \"%s\" "
			"fd %d, ram fd %d\n",
			fptr->fd, str, name, fptr->ofd, dos_str,
			fptr->efd, fptr->rfd);
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif

	if (tsize > 0) {
		bkmem_free(tname, tsize);
	}

	return (fptr->fd);
}

/*
 * compfs_lseek()
 *
 * We maintain an offset at this level for composite file system.
 * This requires us keeping track the file offsets here and
 * in read() operations in consistent with the normal semantics.
 */

/*ARGSUSED*/
static off_t
boot_compfs_lseek(int fd, off_t addr, int whence)
{
	struct compfsfile *fptr;
	off_t	newoff;
	off_t	rv;

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 4) {
		printf("compfs_lseek(): fd %d addr=%d, whence=%d\n",
			fd, addr, whence);
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif

	fptr = get_fileptr(fd);
	if (fptr == NULL)
		return (-1);

	switch (whence) {
	case SEEK_CUR:
		newoff = fptr->offset + addr;
		break;
	case SEEK_SET:
		newoff = addr;
		break;
	default:
	case SEEK_END:
#ifdef COMPFS_OPS_DEBUG
		printf("compfs_lseek(): invalid whence value %d\n", whence);
#endif
		return (-1);
	}

#ifdef COMPFS_OPS_DEBUG
	printf("compfs_lseek(%d,(%d,%d,%d))\n",
	    fptr->favoredfd, fptr->ofd, fptr->efd, fptr->rfd);
#endif

	if (is_compressed(fptr, 0)) {
		rv = decomp_lseek(fptr, newoff, SEEK_SET);
	} else {
		rv = (*fptr->seekmethod)(fptr->favoredfd, newoff, SEEK_SET);
	}

	if (rv >= 0) {
		fptr->offset = newoff;
#ifdef COMPFS_OPS_DEBUG
		if (cpfsdebuglevel > 4) {
			printf("compfs_lseek(): new offset %d\n", fptr->offset);
			if (cpfsdebuglevel & 1)
				(void) goany();
		}
#endif
		return (newoff);
	} else {
		return (rv);
	}
}

/*
 * compfs_fstat() only supports size and mode at present time.
 */

static int
boot_compfs_fstat(int fd, struct stat *stp)
{
	struct compfsfile *fptr;
	int rv;

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 4)
		printf("compfs_fstat(): fd =%d\n", fd);
#endif

	fptr = get_fileptr(fd);
	if (fptr == NULL) {
#ifdef COMPFS_OPS_DEBUG
		printf("compfs_fstat(): no such fd %d\n", fd);
		(void) goany();
#endif
		return (-1);
	}

#ifdef COMPFS_OPS_DEBUG
	printf("compfs_fstat(%d,(%d,%d,%d))\n",
	    fptr->favoredfd, fptr->ofd, fptr->efd, fptr->rfd);
#endif

	if (is_compressed(fptr, 0)) {
		rv = decomp_fstat(fptr, stp);
	} else {
		rv = (*fptr->statmethod)(fptr->favoredfd, stp);
	}

	return (rv);
}

static int
boot_compfs_getdents(int fd, struct dirent *dep, unsigned size)
{
	unsigned int bytesleft;
	struct compfsfile *fptr;
	struct dirent *ddep, *sdep;
	dentbuf_list *db;
	int retcnt;

	fptr = get_fileptr(fd);
	if (fptr == NULL) {
#ifdef COMPFS_OPS_DEBUG
		printf("compfs_getdents(): no such fd %d\n", fd);
#endif
		return (-1);
	}

	/*
	 * If we have performed dents on this file descriptor previously,
	 * then we will a valid pointer to a list of dirent bufs.
	 */
	if (fptr->dentslist == (dentbuf_list *)NULL) {
		dentbuf_list *newl, *prev;
		int (*gdroutine)(int, struct dirent *, unsigned);
		int usefd = -1;

		if (fptr->efd >= 0) {
			usefd = fptr->efd;
			gdroutine = extendfs_ops->fsw_getdents;
		} else if (fptr->ofd >= 0) {
			usefd = fptr->ofd;
			gdroutine = origfs_ops->fsw_getdents;
		}

		if ((newl = (dentbuf_list *)
		    bkmem_alloc(sizeof (dentbuf_list))) ==
		    (dentbuf_list *)NULL) {
			prom_panic("No memory for dirent buffer");
		}

		prev = newl;
		newl->numdents = 0;
		newl->curdent = 0;
		newl->next = (dentbuf_list *)NULL;
		fptr->dentslist = newl;

		if (usefd < 0)
			goto patch;

		while ((newl->numdents =
		    gdroutine(usefd, (struct dirent *)newl->dentbuf,
			RAMDENTBUFSZ)) > 0) {
			if ((newl->next = (dentbuf_list *)
			    bkmem_alloc(sizeof (dentbuf_list))) ==
			    (dentbuf_list *)NULL) {
				prom_panic("No memory for dirent buffer");
			}
			prev = newl;
			newl = newl->next;
			newl->numdents = 0;
			newl->curdent = 0;
			newl->next = (dentbuf_list *)NULL;
		}

		if (newl->numdents < 0) {
			if (newl != prev) {
				prev->next = (dentbuf_list *)NULL;
				bkmem_free((caddr_t)newl,
				    sizeof (dentbuf_list));
			}
		}

patch:		RAMfile_patch_dirents(fptr->filnam, fptr->dentslist);
		map_patch_dirents(fptr->filnam, fptr->dentslist);
	}

	db = fptr->dentslist;
	retcnt = 0;
	bytesleft = size;
	ddep = dep;
	sdep = (struct dirent *)NULL;

	while (db) {
		if (db->numdents == 0) {
			break;
		} else if (db->curdent >= db->numdents) {
			db = db->next;
			sdep = (struct dirent *)NULL;
		} else {
			if (sdep == (struct dirent *)NULL) {
				int i;

				sdep = (struct dirent *)db->dentbuf;
				for (i = 0; i < db->curdent; i++) {
					sdep = (struct dirent *)
					    ((char *)sdep + sdep->d_reclen);
				}
			}

			if (sdep->d_reclen <= bytesleft) {
				ddep->d_ino = sdep->d_ino;
				ddep->d_off = sdep->d_off;
				ddep->d_reclen = sdep->d_reclen;
				(void) strcpy(ddep->d_name, sdep->d_name);
				retcnt++;
				bytesleft -= sdep->d_reclen;
				ddep = (struct dirent *)
					((char *)ddep + ddep->d_reclen);
				sdep = (struct dirent *)
					((char *)sdep + sdep->d_reclen);
				db->curdent++;
			} else {
				break;
			}
		}
	}

	return (retcnt);
}

off_t
boot_compfs_getpos(int fd)
{
	struct compfsfile *fptr;

	fptr = get_fileptr(fd);
	if (fptr == NULL)
		return (-1);

	return (fptr->offset);
}

/*
 * Special dos-text adjustment processing:
 *  converting "\r\n" to " \n" presumably for ASCII files.
 */
static void
dos_text(char *p, int count)
{
	int i, j;
	for (i = count - 1, j = (int)p; i > 0; i--, j++)
		if (*(char *)j == '\r' && *(char *)(j + 1) == '\n')
			*(char *)j = ' ';
}

/*
 * compfs_read()
 */
static ssize_t
boot_compfs_read(int fd, caddr_t buf, size_t count)
{
	struct compfsfile *fptr;
	int	rv;

	fptr = get_fileptr(fd);
	if (fptr == NULL) {
#ifdef COMPFS_OPS_DEBUG
		printf("compfs_read(): no such fd %d\n", fd);
		(void) goany();
#endif
		return (-1);
	}

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 4) {
		printf("compfs_read(): offset at %d\n",
			fptr->offset);
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif

#ifdef COMPFS_OPS_DEBUG
	printf("compfs_read(%d,(%d,%d,%d))\n",
	    fptr->favoredfd, fptr->ofd, fptr->efd, fptr->rfd);
#endif

	if (is_compressed(fptr, 0)) {
		rv = decomp_read(fptr, buf, count);
	} else {
		rv = (*fptr->readmethod)(fptr->favoredfd, buf, count);
	}

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 4) {
		printf("compfs_read(): read returned %d\n", rv);
	}
#endif
	if (rv > 0) {
		fptr->offset += rv;
		if (fptr->flags & COMPFS_TEXT)
			dos_text(buf, rv);
	}

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 4) {
		printf("compfs_read(): return code %d\n", rv);
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif
	return (rv);
}

static int
boot_compfs_close(int fd)
{
	struct compfsfile *fptr;
	int rv;

	fptr = get_fileptr(fd);
	if (fptr == NULL) {
#ifdef COMPFS_OPS_DEBUG
		printf("compfs_close(): no such fd %d.\n", fd);
		(void) goany();
#endif
		return (-1);
	}

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 4) {
		printf("compfs_close(): fd=%d ofd=%d efd=%d rfd=%d\n",
			fd, fptr->ofd, fptr->efd, fptr->rfd);
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif

#ifdef COMPFS_OPS_DEBUG
	printf("compfs_close(%d,(%d,%d,%d)),",
	    fptr->favoredfd, fptr->ofd, fptr->efd, fptr->rfd);
#endif

	if (is_compressed(fptr, 1)) {
		/* May or may not be the active compressed file */
		decomp_close(fptr);
	}

	if (fptr->rfd >= 0) {
		rv = RAMfile_close(fptr->rfd);
	} else if (fptr->efd >= 0) {
		rv = (*extendfs_ops->fsw_close)(fptr->efd);
	} else {
		rv = (*origfs_ops->fsw_close)(fptr->ofd);
	}

	if (fptr->filnam != (char *)NULL) {
		bkmem_free(fptr->filnam, strlen(fptr->filnam)+1);
		fptr->filnam = (char *)NULL;
	}

	if (fptr->dentslist != (dentbuf_list *)NULL) {
		dentbuf_list *np, *dp;
		for (dp = fptr->dentslist; dp; dp = np) {
			np = dp->next;
			bkmem_free((caddr_t)dp, sizeof (dentbuf_list));
		}
		fptr->dentslist = (dentbuf_list *)NULL;
	}

	fptr->fd = 0; /* don't bother to free, make it re-usable */

	return (rv);
}

/*
 * compfs_mountroot() returns 0 on success and -1 on failure.
 */
#ifdef i386
static int
boot_compfs_mountroot(char *str)
{
	struct boot_fs_ops *tmp_ops;
	char *dev = str;
	int rc = 0, pfd;
	static int first_mount = 1;

	extern int SilentDiskFailures;
	extern int Oldstyleboot;
	extern int OpenCount;
	extern char *new_root_type;

	Dprintf(("boot_compfs_mountroot(%s), Oldstyleboot = %d, "
		"OpenCount = %d.\n", str ? str : "NULL",
		Oldstyleboot, OpenCount));
	Dprintf(("boot_compfs_mountroot: new_root_type = \"%s\"\n",
		new_root_type ? new_root_type : "NULL"));

	/*
	 * x86 boot mount is not a simple one-stage mount.  At the
	 * very least there is an initial mount of the IPL device
	 * followed by a mount of the "real" boot device.  The first
	 * mount is from the mountroot in main() (boot.c).  This mount
	 * simply makes it possible to read boot components (such as
	 * bootconf, realmode drivers etc) off the IPL device.  The "real"
	 * boot mount is requested by bootconf via bootops in response
	 * to the user selecting a boot device from the boot menu or
	 * as part of autoboot.  More mounts attempts are possible
	 * when mounts or boots fail.
	 *
	 * The extend device is chosen and opened during the first
	 * mount and remains available (provided the open and map
	 * were successful) to subsequent mounts.
	 */
	if (first_mount) {
		char *new_root_save = new_root_type;

		new_root_type = extendfs_ops->fsw_name;

		/*
		 * An "Oldstyleboot" is one where we booted from ufsbootblk
		 * or MDB.  In these cases we look for the extended fs
		 * strictly on the floppy.  The newer DevConf boots, where
		 * we were loaded from strap.com, we need to determine the
		 * appropriate device to use for the extended filesystem.
		 *
		 * Note that in the above description "Oldstyleboot"
		 * includes booting from a hard drive with no Solaris
		 * boot partition.
		 */

		if (Oldstyleboot) {
			int sv = SilentDiskFailures;

			SilentDiskFailures = 1;
			rc = (*extendfs_ops->fsw_mountroot)(FLOPPY0_NAME);
			SilentDiskFailures = sv;
			/*
			 *  On an old style boot, we've either booted from
			 *  the root filesystem or know exactly what the
			 *  device is where the root lives.  We want to make
			 *  sure then that the coming prom_open
			 *  digs out this info and sets root_fs_type
			 *  accordingly.
			 */
			new_root_save = 0;
		} else {
			extern char *prom_get_extend_name(void);
			char *extend_name = prom_get_extend_name();

			Dprintf(("boot_compfs_mountroot: extend device "
				"is \"%s\".\n", extend_name));

			rc = (*extendfs_ops->fsw_mountroot)(extend_name);
		}

		if (!rc) {
			Dprintf(("boot_compfs_mountroot: building "
				"overlay map for extend device.\n"));
			cpfs_build_map();
		}

		new_root_type = new_root_save;
	}

	Dprintf(("boot_compfs_mountroot: after extend open "
		"new_root_type = \"%s\"\n",
		new_root_type ? new_root_type : "NULL"));

	if ((pfd = prom_open(first_mount ? (char *)0 : str)) > 0) {
		/*  Find the "real" file system type */

		static char dmy[sizeof (BOOT_DEV_NAME)+4];
		extern char *systype;

		tmp_ops = get_fs_ops_pointer(new_root_type);
		if (!tmp_ops)
			return (-1);	/* can happen on IO error */

		Dprintf(("boot_compfs_mountroot: tmp_ops type is \"%s\".\n",
			tmp_ops->fsw_name));

		systype = tmp_ops->fsw_name;
		(void) prom_close(pfd);
		OpenCount--;

		if (first_mount) {
			/* Use dummy device name for mount */
			(void) sprintf(dmy, "%s \b", BOOT_DEV_NAME);
			dev = dmy;

			Dprintf(("boot_compfs_mountroot: using dummy name "
				"%s (ends with space-backspace).\n", dev));
		}

		/*
		 * XXX -- Ugh.  We need to close any files open on the root.
		 * We have to do that before we call mountroot, though,
		 * because if you do it after mounting root you can
		 * end up undoing all you just did in the mount.
		 * Of course if the mountroot fails, we're really hosed
		 * because we just closed everything on the old root!!
		 */
		if (!first_mount && origfs_ops != extendfs_ops)
			(*origfs_ops->fsw_closeall)(1);

		if (!(rc = (*tmp_ops->fsw_mountroot)(dev))) {
			first_mount = 0;
			origfs_ops = tmp_ops;
			new_root_type = "";

			Dprintf(("boot_compfs_mountroot: tmp_ops "
				"mountroot succeeded.\n"));
			return (0);
		}

		Dprintf(("boot_compfs_mountroot: tmp_ops mountroot failed.\n"));
	}

	Dprintf(("boot_compfs_mountroot: failed (at end).\n"));
	first_mount = 0;
	return (-1);
}
#else
static int
boot_compfs_mountroot(char *str)
{
	extern char *init_disketteio();

	if ((*extendfs_ops->fsw_mountroot)(init_disketteio()) == 0)
		cpfs_build_map();

	return ((*origfs_ops->fsw_mountroot)(str));
}
#endif /* !i386 */

/*
 * Unmount the root fs -- unsupported on this fstype.
 */
int
boot_compfs_unmountroot(void)
{
	return (-1);
}

static void
boot_compfs_closeall(int flag)
{	struct map_entry *mlp;
	struct compfsfile *fptr = compfshead;

	decomp_closeall(flag);
	RAMfile_closeall(flag);
	(*extendfs_ops->fsw_closeall)(flag);
	(*origfs_ops->fsw_closeall)(flag);

	while (fptr != NULL) {
		bkmem_free((caddr_t)fptr, sizeof (struct compfsfile));
		fptr = fptr->forw;
	}
	compfshead = NULL;

	for (mlp = map_listh; mlp; mlp = map_listh) {
		map_listh = mlp->link;
		bkmem_free((caddr_t) mlp, sizeof (struct map_entry) +
		    strlen(mlp->target) + strlen(mlp->source) + 2);
	}
	map_listt = NULL;

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 2) {
		printf("compfs_closeall()\n");
		if (cpfsdebuglevel & 1)
			(void) goany();
	}
#endif
}

/*
 *	The remainder of this file is the routines for implementing the
 *	decompression layer on top of an underlying fs.
 */

#define	DLCHUNK		(32 * 1024)
struct decomp_list {
	struct decomp_list	*dl_next;
	char			dl_data[DLCHUNK];
};

static struct decomp_list *decomp_data; /* storage list for decompressed */
					/*    data */
static int decomp_filedes;	/* fd of currently decompressed file */
static int decomp_size;		/* size of currently decompressed file */
static ssize_t (*decomp_undrread)(int, caddr_t, size_t);  /* Read function */

/*
 * decomp_lseek, decomp_read, etc. Assume that the compressed state has
 * already been determined via is_compressed().
 *
 * decomp_lseek assumes all seeks it receives are SEEK_SETs.
 */

/*ARGSUSED*/
static int
decomp_lseek(compfsfile_t *fptr, off_t addr, int whence)
{
#ifdef DECOMP_DEBUG
	if (compfsverbose || cpfsdebuglevel > 2) {
		printf("decomp_lseek: seeking to 0x%x\n", addr);
	}
#endif
	return (fptr->decomp_offset = addr);
}

static int
decomp_fstat(compfsfile_t *fptr, struct stat *stp)
{
	/*
	 * Get the mode from the compressed file, the size has to
	 * come from decompression routines.
	 */
	(*fptr->statmethod)(fptr->favoredfd, stp);

	decomp_file(fptr);
	stp->st_size = decomp_size;
#ifdef DECOMP_DEBUG
	if (compfsverbose || cpfsdebuglevel > 2) {
		printf("decomp_fstat: reporting size 0x%x\n",
			decomp_size);
	}
#endif
	return (0);
}

static void
decomp_copyout(compfsfile_t *fptr, caddr_t buf, int count)
{
	struct decomp_list *dlp;
	int rd_indx, wrt_indx, len;

	/* "seek" to the data in the decompressed data list */
	dlp = decomp_data;
	rd_indx = 0;
	while ((rd_indx + DLCHUNK) <= fptr->decomp_offset) {
		dlp = dlp->dl_next;
		rd_indx += DLCHUNK;
	}
	rd_indx = fptr->decomp_offset % DLCHUNK;

	/* copy count bytes to caller */
	wrt_indx = 0;
	do {
		len = ((DLCHUNK - rd_indx) > count) ?
			count : (DLCHUNK - rd_indx);
		bcopy(dlp->dl_data + rd_indx, buf + wrt_indx, len);
		dlp = dlp->dl_next;
		rd_indx = 0;
		count -= len;
		wrt_indx += len;
	} while (count != 0);
}

static ssize_t
decomp_read(compfsfile_t *fptr, caddr_t buf, size_t count)
{
	decomp_file(fptr);
	if (count > decomp_size - fptr->decomp_offset)
		count = decomp_size - fptr->decomp_offset;
	if ((int)count <= 0) {
#ifdef DECOMP_DEBUG
		if (compfsverbose || cpfsdebuglevel > 2) {
			printf("decomp_read: returning end-of-file\n");
		}
#endif
		return (0);
	}

	decomp_copyout(fptr, buf, count);
	fptr->decomp_offset += count;
#ifdef DECOMP_DEBUG
	if (compfsverbose || cpfsdebuglevel > 2) {
		printf("decomp_read: returning %d bytes\n", count);
	}
#endif
	return (count);
}

static void
decomp_close(compfsfile_t *fptr)
{
	if (fptr->favoredfd == decomp_filedes &&
	    fptr->readmethod == decomp_undrread) {
#ifdef DECOMP_DEBUG
		if (compfsverbose || cpfsdebuglevel > 2) {
			printf("decomp_close: closing active file\n");
		}
#endif
		decomp_free_data();
		decomp_filedes = 0;
		decomp_undrread = 0;
	}

	/*
	 * The close routine that called us will finish up by
	 * closing the underlying file.
	 */
}

/*ARGSUSED*/
void
decomp_closeall(int flag)
{
	if (decomp_filedes)
		decomp_filedes = 0;
	decomp_fini();
#ifdef DECOMP_DEBUG
	if (compfsverbose || cpfsdebuglevel > 2) {
		printf("decomp_umount: finished with decompression\n");
	}
#endif
}

static void
decomp_file(compfsfile_t *fptr)
{
	/* With luck we are already set up for this file */
	if (decomp_data && decomp_filedes == fptr->favoredfd &&
	    decomp_undrread == fptr->readmethod && decomp_size != 0) {
#ifdef DECOMP_DEBUG
		if (compfsverbose || cpfsdebuglevel > 2) {
			printf("decomp_file: already cached\n");
		}
#endif
		return;
	}

	/* Set up to look like an empty file if we fail */
	decomp_free_data();
	decomp_filedes = fptr->favoredfd;
	decomp_undrread = fptr->readmethod;
	decomp_size = 0;

	/*
	 *  If we come back to read from a compressed file that
	 *  was left open, but read another compressed file
	 *  in the meantime, the original source for the compressed
	 *  version should be re-read.
	 */
	fptr->seekmethod(decomp_filedes, 0, SEEK_SET);

	/*
	 * Initialize internal decompression bookkeeping.
	 */
	if (decomp_init() < 0) {
#ifdef DECOMP_DEBUG
		if (compfsverbose || cpfsdebuglevel > 2) {
			printf("decomp_file: decomp_init failed\n");
		}
#endif
		decomp_filedes = 0;
		decomp_undrread = 0;
		return;
	}
	decompress();
#ifdef DECOMP_DEBUG
	if (compfsverbose || cpfsdebuglevel > 2) {
		printf("decomp_file: decompressed file is 0x%x bytes\n",
		    decomp_size);
	}
#endif
}

#define	DCBSIZE		(64 * 1024)
static char *dcr_p;	/* pointer to decompression read buffer */
static int dcr_n;	/* number of bytes in decompression read buffer */
static int dcr_i;	/* index into decompression read buffer */

static int
decomp_init()
{
	if ((dcr_p == 0) && ((dcr_p = bkmem_alloc(DCBSIZE)) == 0))
		return (-1);
	if (decompress_init() < 0) {
		bkmem_free(dcr_p, DCBSIZE);
		dcr_p = 0;
		return (-1);
	}
	dcr_n = 0;
	dcr_i = 0;
	return (0);
}

static void
decomp_fini()
{
	if (dcr_p)
		bkmem_free(dcr_p, DCBSIZE);
	decomp_free_data();
	decompress_fini();
	dcr_p = 0;
}

/*
 * The following routines are callbacks from the
 * decompress function to get data from the compressed
 * input stream and to write the decompressed data
 * to the output stream. The input is buffered in big
 * chunks to keep the floppy moving along.
 *
 * The output is dumped into a buffer that must be big
 * enough for the largest file. This could be changed
 * to dynamically grow a linked list in chunks so that
 * there is no set limit other than the heap.
 */

int
decomp_getbytes(char *buffer, int count)
{
	int len, res, xfer;

	xfer = 0;	/* bytes transferred */
	res = count;	/* residual of request */
	do {
		if (dcr_n == 0) {
			dcr_n = (*decomp_undrread)
				(decomp_filedes, dcr_p, DCBSIZE);
			if (dcr_n <= 0) {
				/* EOF */
				dcr_n = 0;
				break;
			}
			dcr_i = 0;
		}
		if (res <= dcr_n) {
			len = res;
		} else {
			len = dcr_n;
		}
		bcopy(dcr_p+dcr_i, buffer+xfer, len);
		dcr_i += len;
		dcr_n -= len;
		xfer += len;
		res -= len;
	} while (res != 0);

	return (xfer);
}

int
decomp_getchar(void)
{
	char c;

	if (decomp_getbytes(&c, 1) <= 0)
		return (-1);
	return (c & 0xff);
}

/*
 * Put the decompressed bytes into a list of data buffers. The buffers
 * are allocated on the fly.
 */
void
decomp_putchar(char c)
{
	static struct decomp_list *dlp;
	static int indx, max_indx;
	struct decomp_list *old_dlp;

	if (decomp_filedes == 0) {
		/* could not get memory on previous call, dump the byte */
		return;
	}

	if (decomp_data == 0) {
		/* first call for this file */
		max_indx = 0;
	}

	if (decomp_size == max_indx) {
		/* need to link another buffer */
		old_dlp = dlp;
		dlp = (struct decomp_list *)bkmem_alloc(sizeof (*dlp));
		if (dlp == 0) {
			decomp_filedes = 0;
			decomp_size = 0;
			return;
		}
		if (decomp_data == 0) {
			decomp_data = dlp;
		} else {
			old_dlp->dl_next = dlp;
		}
		indx = 0;
		max_indx += DLCHUNK;
	}
	dlp->dl_data[indx++] = c;
	decomp_size++;
}

static void
decomp_free_data()
{
	struct decomp_list *dlp, *n_dlp;

	if (decomp_data == 0)
		return;
	dlp = decomp_data;
	while (dlp) {
		n_dlp = dlp->dl_next;
		bkmem_free((char *)dlp, sizeof (*dlp));
		dlp = n_dlp;
	}
	decomp_data = 0;
	decomp_filedes = 0;
	decomp_size = 0;
}

/*
 * is_compressed
 *	Checks if the file we are looking at is or is not compressed. Saves
 *	the result in the file pointer structure for speedy future lookups.
 */
static int
is_compressed(compfsfile_t *fp, int closing)
{
	int rr;

	/*
	 *  If we haven't determined yet whether the file is actually
	 *  compressed, but all we are doing is closing the file anyway,
	 *  then don't bother with trying to figure out if it is compressed.
	 *
	 *  The dos library seems to like to close directories much later
	 *  after opening them but without ever having using them.  This leads
	 *  to problems if we've swapped floppies, because we end up needing
	 *  to reinsert the floppy just to do the compression read and
	 *  all we really wanted to do was close the directory.
	 */
	if (fp->compressed < 0 && closing) {
		return (0);
	}

	if (fp->compressed < 0) {
		union {
			char s[3];
			u_char u[3];
		} h;

		/*
		 * Whether the file is compressed or uncompressed
		 * remains to be determined.  We determine so now.
		 */
		if (fp->rfd >= 0) {
			fp->favoredfd = fp->rfd;
			fp->readmethod = RAMfile_read;
			fp->statmethod = RAMfile_fstat;
			fp->seekmethod = RAMfile_lseek;
		} else if (fp->efd >= 0) {
			fp->favoredfd = fp->efd;
			fp->readmethod = extendfs_ops->fsw_read;
			fp->statmethod = extendfs_ops->fsw_fstat;
			fp->seekmethod = extendfs_ops->fsw_lseek;
		} else {
			fp->favoredfd = fp->ofd;
			fp->readmethod = origfs_ops->fsw_read;
			fp->statmethod = origfs_ops->fsw_fstat;
			fp->seekmethod = origfs_ops->fsw_lseek;
		}

		/*
		 * Look for compressed file signature bytes.
		 */
		fp->decomp_offset = 0;
		rr = (*fp->readmethod)(fp->favoredfd, h.s, 3);

		if ((rr == 3) && (h.u[0] == 0x1f) && (h.u[1] == 0x9d) &&
		    (h.u[2] >= 0x89) && (h.u[2] <= 0x90)) {
			fp->compressed = 1;
		} else if (rr == 3 && (h.u[0] == 0x40) && (h.u[1] == 0x6c) &&
		    (h.u[2] == 0x3a)) {
			fp->compressed = 0;
			if (follow_link(fp)) {
				return (fp->compressed);
			}
		} else {
			fp->compressed = 0;
		}
		(*fp->seekmethod)(fp->favoredfd, 0, SEEK_SET);
	}

	return (fp->compressed);
}

#define	MAX_INDIRECT 2

static int
follow_link(compfsfile_t *fp)
{
	static char ln[NAMEBUFSIZE];
	struct compfsfile *lfp;
	static int symlevel = 0;
	char *eol;
	int lfd, savefd, rc;

	if ((rc = (*fp->readmethod)(fp->favoredfd, ln, NAMEBUFSIZE - 1)) <= 0) {
		return (0);
	} else {
		ln[rc] = '\0';
		eol = strchr(ln, '\n');
		if (eol)
			*eol = '\0';

		eol = strchr(ln, '\r');
		if (eol)
			*eol = '\0';
	}

	if ((lfd = boot_compfs_open(ln, fp->flags)) >= 0) {
		symlevel++;
		if (symlevel > MAX_INDIRECT) {
			printf("follow_link: too many levels of indirection\n");
			boot_compfs_close(lfd);
			symlevel--;
			return (0);
		}

		savefd = fp->fd;
		(void) boot_compfs_close(savefd);

		lfp = get_fileptr(lfd);
		(void) is_compressed(lfp, 0);

		*fp = *lfp;	/* Structure Copy */

		lfp->filnam = (char *)NULL;
		lfp->dentslist = (dentbuf_list *)NULL;
		lfp->fd = 0;	/* Make interim descriptor re-usable */

		fp->fd = savefd;
		symlevel--;
		return (1);
	} else {
		return (0);
	}
}

static void
compfs_assign_fd(compfsfile_t *fptr, int new)
{
	fptr->fd = compfs_filedes++;
	/*
	 * XXX - Major kludge alert!
	 *
	 * old versions of adb use bit 0x80 in file descriptors
	 * for internal bookkeeping. This works fine under unix
	 * where descriptor numbers are re-used. This breaks
	 * on standalones (kadb) as soon as this ever increasing
	 * descriptor number hits 0x80 (128). We skip all
	 * ranges of numbers with this bit set so kadb does
	 * not get confused. Hey, kadb started it.
	 */
	if (compfs_filedes & 0x80)
		compfs_filedes += 0x80;

	if (new) {
		fptr->forw = compfshead;
		compfshead = fptr;
	}
}

static void
compfs_save_fn(compfsfile_t *fptr, char *fn)
{

	if ((fptr->filnam = (char *)bkmem_alloc(strlen(fn)+1)) !=
	    (char *)NULL) {
		(void) strcpy(fptr->filnam, fn);
	}
}

/*
 * map_patch_dirents
 *	Based on the list of current mappings, add any dirents necessary to
 *	the dirent info stored in the dentbuf_list linked list we've been
 *	handed.
 */

static
void
map_patch_dirents(char *path, dentbuf_list *dentslist)
{
	/*
	 *  The path we've been passed is the current path we're looking
	 *  up dirents for.  We need to test to see if anything in that path
	 *  exists as a virtual file (via the 'map' command).
	 *
	 *  The first step is determine if the required directory "exists"
	 *  in the mappings we have.
	 */
	struct map_entry *ml;

	/*
	 * Add any target mappings (full replacement, not extension)
	 * which should appear in the dirents.
	 */
	for (ml = map_listh; ml; ml = ml->link) {
		if (ml->flags & COMPFS_AUGMENT || ml->flags & COMPFS_PATH) {
			continue;
		}

		if (strncmp(path, ml->target, strlen(path)) == 0) {
			static char pnebuf[NAMEBUFSIZE];
			char *pne;
			int  pnelen = 0;

			pne = ml->target + strlen(path);

			/*
			 * We match up to the end of the requested directory
			 * path.  Double check that our target name is
			 * an EXACT match.  I.E., avoid the case where we
			 * think the target solaris/drivers/fish.bef is a
			 * match for a requested path solaris/driv.
			 */
			if (*pne != '/')
				continue;

			/*
			 * We have a match on the directory part.
			 * The next chunk of the target name, either up to
			 * the next / or the end of the name, is potentially
			 * a new dirent.
			 */
			while (*pne == '/')
				pne++;
			while (*pne != '/' && *pne && pnelen < NAMEBUFSIZE-1)
				pnebuf[pnelen++] = *pne++;
			pnebuf[pnelen] = '\0';

			add_dentent(dentslist, pnebuf, pnelen);
		}
	}
}

/*
 *  cpfs_isamapdir
 *	In order to get dirents, someone may try to open a directory that
 *	really only exists virtually, because it was declared as a part of
 *	a map command.  This checks if the specified path is in fact a
 *	virtual directory.  Returns 1 if it is, 0 if not.
 */
int
cpfs_isamapdir(char *path)
{
	/*
	 *  The path we've been passed is the current path we're trying to
	 *  open as a directory.  We need to test to see if the directory
	 *  exists as a virtual mapping.
	 */
	struct map_entry *ml;
	int rv = 0;

	/*
	 * Add any target mappings (full replacement, not extension)
	 * which should appear in the dirents.
	 */
	for (ml = map_listh; ml; ml = ml->link) {
		if (ml->flags & COMPFS_AUGMENT) {
			continue;
		}

		if (strncmp(path, ml->target, strlen(path)) == 0) {
			char *pne;

			pne = ml->target + strlen(path);

			/*
			 * We match up to the end of the requested directory
			 * path.  Double check that our target name is
			 * an EXACT match.  I.E., avoid the case where we
			 * think the target solaris/drivers/fish.bef is a
			 * match for a requested path solaris/driv.
			 */
			if (*pne != '/' && *pne) {
				continue;
			} else {
				rv = 1;
				break;
			}
		}
	}

	return (rv);
}

/*
 *  The create() and write() routines only attempt to do something for x86.
 */
#ifdef i386
/*
 *  ftruncate() --
 *	Truncate file at offset.  This really should be added to the fsswitch,
 *	but very few of the file systems support it.  So for now we'll leave
 *	the definition here and be extra careful about checking the state
 *	of the world before we allow it to be used.
 */
int
ftruncate(int fd, off_t where)
{
	/*
	 *  All ftruncate calls are going to arrive here.  That should not
	 *  be a problem because currently the only things that are
	 *  going to be doing creates are realmode modules running under
	 *  the Intel second level boot.
	 *
	 *  Still, we want to be very careful.  The first thing we want to
	 *  be sure of is that 'compfs' is actually the in-use file switch.
	 *  If it isn't we should bail immediately, because appropriate state
	 *  is probably not present to do any writing.
	 */
	struct compfsfile *fptr;

	if (!(get_default_fs() == &boot_compfs_ops)) {
		return (-1);
	}

	fptr = get_fileptr(fd);
	if (fptr == NULL)
		return (-1);

	/*
	 * Write operations only available on the original fs
	 * or the RAMfile fs.
	 */
	if (fptr->ofd < 0 && fptr->rfd < 0)
		return (-1);

	if (fptr->ofd >= 0 && origfs_ops == &boot_pcfs_ops) {
		/*
		 * DOS thinks a write of 0 bytes means truncate
		 * at the current offset.
		 */
		if ((*origfs_ops->fsw_lseek)(fptr->ofd,
		    where, SEEK_SET) == where) {
			fptr->offset = where;
			return (boot_pcfs_write(fptr->ofd, NULL, 0));
		}
	} else if (fptr->rfd >= 0) {
		if (RAMfile_lseek(fptr->rfd, where, SEEK_SET) == where) {
			fptr->offset = where;
			return (RAMfile_trunc_atoff(fptr->rfd));
		}
	}

	return (-1);
}

/*
 *  Create() --
 *	Create a file.  This really should be added to the fsswitch, but
 *	very few of the file systems support it.  So for now we'll leave
 *	the definition here and be extra careful about checking the state
 *	of the world before we allow it to be used.
 */
int
create(char *fn, ulong attr)
{
	/*
	 *  All create calls are going to arrive here.  That should not
	 *  be a problem because currently the only things that are
	 *  going to be doing creates are realmode modules running under
	 *  the Intel second level boot.
	 *
	 *  Still, we want to be very careful.  The first thing we want to
	 *  be sure of is that 'compfs' is actually the in-use file switch.
	 *  If it isn't we should bail immediately, because appropriate state
	 *  is probably not present to do any writing.
	 */
	struct compfsfile *fptr;
	char	*dos_str = (char *)NULL;
	char	*tname, *pname, *name;
	int	ismapped;
	int	mflags = 0;
	int	tsize = 0;
	int	new = 0;

	if (!(get_default_fs() == &boot_compfs_ops)) {
		return (-1);
	}

	if ((fptr = get_fileptr(0)) == NULL) {
		if ((fptr = (compfsfile_t *)
		    bkmem_alloc(sizeof (compfsfile_t))) ==
		    (compfsfile_t *)NULL) {
			prom_panic("No memory for file descriptor");
		}
		new++;
	}

	fptr->flags = 0;
	fptr->offset = 0;
	fptr->compressed = 0;  /* Initially undetermined, 0=Not, 1=Is */
	fptr->dentslist = NULL;
	fptr->readmethod = NULL;
	fptr->statmethod = NULL;
	fptr->seekmethod = NULL;
	fptr->fd = fptr->rfd = fptr->efd = fptr->ofd = fptr->favoredfd = -1;

	pname = tname = tidy_name(fn, &tsize);

	ismapped = cpfs_mapped(tname, &dos_str, &mflags);
	if (ismapped) {
		name = dos_str;
	} else if (strncasecmp(tname, "R:", 2) == 0) {
		name = tname+2;
		attr |= (DOSFD_RAMFILE | DOSFD_NOSYNC);
	} else {
		name = tname;
	}

	/*
	 *  If the origfs doesn't support writes then we have to make
	 *  this a RAM file.
	 */
	chk_write_to_readonly((int *)&attr);

	/*
	 *  If we are talking to a pcfs as our original file system, there
	 *  is a chance we can do the writes directly.  We do not try though,
	 *  if the attributes indicate a RAMfile is wanted.
	 */
	if (((attr & DOSFD_RAMFILE) == 0) && origfs_ops == &boot_pcfs_ops) {
		if ((fptr->ofd = dosCreate(name, attr)) >= 0) {
			compfs_assign_fd(fptr, new);
			compfs_save_fn(fptr, name);
			fptr->favoredfd = fptr->ofd;
			fptr->readmethod = origfs_ops->fsw_read;
			fptr->statmethod = origfs_ops->fsw_fstat;
			fptr->seekmethod = origfs_ops->fsw_lseek;
			if (tsize > 0)
				bkmem_free(pname, tsize);
			return (fptr->fd);
		}
	}

	/*
	 *  PCFS write didn't work out or wasn't desired.  Do a RAMfile.
	 */
	if ((fptr->rfd = RAMfile_create(name, attr)) >= 0) {
		/*
		 * Seek to beginning of file.
		 */
		RAMrewind(fptr->rfd);
		fptr->flags |= attr;
		/*
		 * Create non-persistent files in cache only.
		 */
		if (fptr->flags & DOSFD_NOSYNC)
			RAMfile_set_cachebit(fptr->rfd);
		fptr->favoredfd = fptr->rfd;
		fptr->readmethod = RAMfile_read;
		fptr->statmethod = RAMfile_fstat;
		fptr->seekmethod = RAMfile_lseek;
		compfs_assign_fd(fptr, new);
		compfs_save_fn(fptr, name);
		if (tsize > 0)
			bkmem_free(pname, tsize);
		return (fptr->fd);
	} else {
		if (new)
			bkmem_free((caddr_t)fptr, sizeof (compfsfile_t));
		else
			fptr->fd = 0;  /* mark as reusable */
		if (tsize > 0)
			bkmem_free(pname, tsize);
		return (-1);
	}
}

/*
 *  Write() --
 *	Write to a file.  This really should be added to the fsswitch, but
 *	very few of the file systems support it.  So for now we'll leave
 *	the definition here and be extra careful about checking the state
 *	of the world before we allow it to be used.
 */
int
write(int fd, char *buf, int buflen)
{
	/*
	 *  All write calls are going to arrive here.  That should not
	 *  be a problem because currently the only things that are
	 *  going to be doing writes are realmode modules running under
	 *  the Intel second level boot.
	 *
	 *  Still, we want to be very careful.  The first thing we want to
	 *  be sure of is that 'compfs' is actually the in-use file switch.
	 *  If it isn't we should bail immediately, because appropriate state
	 *  is probably not present to do any writing.
	 */
	struct compfsfile *fptr;
	int ob;

	if (!(get_default_fs() == &boot_compfs_ops)) {
		return (-1);
	}
	fptr = get_fileptr(fd);
	if (fptr == NULL)
		return (-1);

#ifdef COMPFS_OPS_DEBUG
	if (cpfsdebuglevel > 2)
		printf("write:(%d)[O=%d,E=%d,R=%d]",
		    fptr->fd, fptr->ofd, fptr->efd, fptr->rfd);
#endif

	if (fptr->ofd >= 0 && origfs_ops == &boot_pcfs_ops) {
		if ((ob = boot_pcfs_write(fptr->ofd, buf, buflen)) >= 0) {
			fptr->offset += ob;
			return (ob);
		}
	} else if (fptr->efd >= 0 && extendfs_ops == &boot_pcfs_ops) {
		if ((ob = boot_pcfs_write(fptr->efd, buf, buflen)) >= 0) {
			fptr->offset += ob;
			return (ob);
		}
	} else if (fptr->rfd >= 0) {
		/*
		 * DOS thinks a write of 0 bytes means truncate
		 * at the current offset.
		 */
		if (buflen == 0)
			return (RAMfile_trunc_atoff(fptr->rfd));

		if ((ob = RAMfile_write(fptr->rfd, buf, buflen)) < 0)
			return (-1);

		fptr->offset += ob;
#ifdef i386
		/*
		 *  The bootops interface allows a realmode module to
		 *  access the booter's interpreter capabilities.  This
		 *  important capability is how the realmode configuration
		 *  assistant builds a device tree for the kernel.
		 */
		if (fptr->flags & DOSFD_BOOTOPC) {
			dosbootop(fptr->fd, buf, buflen);
		}
#endif /* i386 */
		return (ob);
	}

	return (-1);
}

/*
 *  rename() --
 *	Change the name of a file.  This really should be added to the
 *	fsswitch, but very few of the file systems support it.  So for now
 *	we'll leave the definition here and be extra careful about checking
 *	the state of the world before we allow it to be used.
 */
int
rename(char *ofn, char *nfn)
{
	/*
	 *  All rename calls are going to arrive here.  That should not
	 *  be a problem because currently the only things that are
	 *  going to be doing renames are realmode modules running under
	 *  the Intel second level boot.
	 *
	 *  Still, we want to be very careful.  The first thing we want to
	 *  be sure of is that 'compfs' is actually the in-use file switch.
	 *  If it isn't we should bail immediately, because appropriate state
	 *  is probably not present to do any writing.
	 */
	extern dosRename(char *, char *);
	char *po, *pn, *tofn, *tnfn;
	char *mn, *mo, *rn, *ro;
	int tosz, tnsz;
	int rv, mflags = 0;

	if (!(get_default_fs() == &boot_compfs_ops)) {
		return (-1);
	}

	if ((mo = (char *)bkmem_alloc(MAXPATHLEN)) == (char *)NULL) {
		File_doserr = DOSERR_INSUFFICIENT_MEMORY;
		return (-1);
	}

	po = tofn = tidy_name(ofn, &tosz);
	if (cpfs_mapped(tofn, &ro, &mflags))
		(void) strcpy(mo, ro);
	else
		(void) strcpy(mo, tofn);

	pn = tnfn = tidy_name(nfn, &tnsz);
	if (cpfs_mapped(tnfn, &rn, &mflags))
		mn = rn;
	else
		mn = tnfn;

	/*
	 *  Only PCfs supports rename
	 */
	rv = dosRename(mo, mn);
	bkmem_free(mo, MAXPATHLEN);

	if (tosz > 0)
		bkmem_free(po, tosz);

	if (tnsz > 0)
		bkmem_free(pn, tnsz);

	if (rv != 0) {
		File_doserr = (ushort)rv;
		return (-1);
	} else {
		return (rv);
	}
}

/*
 *  unlink() --
 *	Delete the named file.  This really should be added to the
 *	fsswitch, but very few of the file systems support it.  So for now
 *	we'll leave the definition here and be extra careful about checking
 *	the state of the world before we allow it to be used.
 */
int
unlink(char *fn)
{
	/*
	 *  All unlink calls are going to arrive here.  That should not
	 *  be a problem because currently the only things that are
	 *  going to be doing renames are realmode modules running under
	 *  the Intel second level boot.
	 *
	 *  Still, we want to be very careful.  The first thing we want to
	 *  be sure of is that 'compfs' is actually the in-use file switch.
	 *  If it isn't we should bail immediately, because appropriate state
	 *  is probably not present to do any writing.
	 */
	char *un, *dn;
	char *pn, *tn;
	int mflags = 0;
	int tsize = 0;
	int rv;

	if (!(get_default_fs() == &boot_compfs_ops)) {
		return (-1);
	}

	pn = tn = tidy_name(fn, &tsize);
	if (cpfs_mapped(tn, &dn, &mflags)) {
		un = dn;
	} else {
		un = tn;
	}

	/*
	 *  At one point I was going to actually do the unlink
	 *  on the PCFS if the ramfile one didn't work.  I now
	 *  think that is probably just too dangerous.  This is
	 *  how it worked in that case, if someone wants to change
	 *  it back:
	 *
	 *	extern dosUnlink(char *nam);
	 *
	 *	if ((rv = RAMfile_destroy(un)) < 0) {
	 *		rv = dosUnlink(un);
	 *	}
	 */
	if ((rv = RAMfile_destroy(un)) != 0) {
		File_doserr = (ushort)rv;
		if (tsize > 0)
			bkmem_free(pn, tsize);
		return (-1);
	} else {
		if (tsize > 0)
			bkmem_free(pn, tsize);
		return (rv);
	}
}
#else
int
ftruncate(int fd, off_t where)
{
	/*
	 * Ftruncate is currently only supported for Intel
	 */
	return (-1);
}

int
create(char *fn, ulong attr)
{
	/*
	 * Create is currently only supported for Intel
	 */
	return (-1);
}

int
write(int fd, char *buf, int buflen)
{
	/*
	 * Write is currently only supported for Intel
	 */
	return (-1);
}

int
rename(char *ofn, char *nfn)
{
	/*
	 * Rename is currently only supported for Intel
	 */
	return (-1);
}

int
unlink(char *fn)
{
	/*
	 * Unlink is currently only supported for Intel
	 */
	return (-1);
}
#endif /* i386 */
