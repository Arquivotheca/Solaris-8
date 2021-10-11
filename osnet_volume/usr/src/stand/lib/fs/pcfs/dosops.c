/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dosops.c	1.46	99/10/07 SMI"

#ifdef	notdef
/*
 * Note that if you define DEBUG here you also need to define
 * it in usr/src/psm/stand/boot/i386/common/disk.c where the
 * splatinfo() routine is defined.
 */
#define	DEBUG
#endif	/* notdef */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/bootcmn.h>
#include <sys/bootvfs.h>
#include <sys/sysmacros.h>
#include <sys/doserr.h>
#ifdef i386
#include <sys/ihandle.h>
#endif
#include <sys/fs/pc_label.h>
#include <sys/salib.h>
#include <sys/sacache.h>
#include <sys/promif.h>
#include <ctype.h>
#include "pcfilep.h"

struct boot_fs_ops boot_pcfs_ops = {
	"pcfs",
	boot_pcfs_mountroot,
	boot_pcfs_unmountroot,
	boot_pcfs_open,
	boot_pcfs_close,
	boot_pcfs_read,
	boot_pcfs_lseek,
	boot_pcfs_fstat,
	boot_pcfs_closeall,
	boot_pcfs_getdents
};

/* ---- Variables local to this file ---- */

/*
 * We allow for pcfs mounts from up to MAX_PCFS_MOUNTS different
 * physical sources.
 *
 * The pi pointers are a focal point for all information about the
 * disk and partitions.
 *
 * The _device_fd's are file descriptors for the physical devices
 * on which volumes are mounted.
 *
 * The file_table_h structure is a linked list of file descriptors
 * handed out for all volumes.
 *
 * file_num is an increasing count of file descriptors to hand
 * out.
 *
 */
static struct _fat_controller_ *pi[MAX_PCFS_MOUNTS];
static int _device_fd[MAX_PCFS_MOUNTS];
static char *Curvol[MAX_PCFS_MOUNTS];
static _file_desc_p file_table_h;
static int file_num;

static int Pcfs_cnt;    /* Count of pi structures in use. */
static char *falseroot = "pcfs";

/*
 *  Global volume indices and boot volume info.
 */
#define	sharedAC (bootpcfs == 0 && floppy0pcfs == 0)

static int bootpcfs = -1;	/* Index into pi for pcfs we booted from */
static int floppy0pcfs = -1;	/* Index into pi for pcfs on 'A' drive */
static int floppy1pcfs = -1;	/* Index into pi for pcfs on 'B' drive */

/*
 * Global notion of the volume label assigned to the root.
 * PCFS will change this on the first successful mountroot.
 */
extern char *rootvol;
extern int  boot_root_writable;

static char *bootvolume;

#ifdef DEBUG
extern int DOSsnarf_flag;
int snarf_val;
int pcfs_debug = ~DBG_VOL;
int resto_db;
#define	Clear_dbbit(y) resto_db = pcfs_debug & y, pcfs_debug &= ~y
#define	Resto_dbbit() pcfs_debug |= resto_db
#else
#define	Clear_dbbit(y)
#define	Resto_dbbit()
#endif

/* ---- local macros used by these routines ---- */
#define	MIN(a, b) ((a) < (b) ? (a) : (b))
#define	Print printf
#define	EQ(a, b) (strcmp(a, b) == 0)
#define	EQN(a, b, n) (strncmp(a, b, n) == 0)
#define	VOLEQ(a, b) \
	((strncasecmp(a, b, VOLLABELSIZE)) == 0)
#define	VOLCMP(a, b) \
	(strncasecmp(a, b, VOLLABELSIZE))

/* ---- forward declarations for various routines ---- */
static char *dirlookupvolname(int volidx);
#ifdef	DEBUG
static char *dosname(char *, char *);
#endif	/* DEBUG */
static char *vollst(char *srcvol, int normalized);
static void updatedir(_file_desc_p, void (*)(_dir_entry_p, void *), void *);
static void dosFtruncate(_file_desc_p f);
static int mountA(char *volname);
static int mountB(char *volname);
static int mountC(char *volname);
static int fat_fdisk(int, int, _fdisk_p);
static int lookuppn(char *, _dir_entry_p, _file_desc_p);
static int lookup(char *, _dir_entry_p, u_long, _file_desc_p);
static int component(char *pn, char *d, int cs);
static fat_clalloc(int volidx, int flg);
static fat_setcluster(int volidx, int c, int v);
static int loadVolume(char *path, char **volnm, char **newfn, int volhint);
int dosCreate(char *fn, int mode);
int namecmp(char *s, char *t);

/* ---- define externals here ---- */
extern caddr_t kmem_alloc(size_t);
extern void kmem_free(caddr_t, size_t);
extern void splatinfo(int fd);
extern char *popup_prompt(char *, char *);
extern int floppy_status_changed(int fd);
extern int SilentDiskFailures;
extern int read_opt;
extern int is_floppy(int fd);
extern int is_floppy0(int fd);
extern int is_floppy1(int fd);
extern void *memcpy(void *s1, void *s2, size_t n);
extern void *memset(void *s, int c, size_t n);
extern int prom_devreset(int fd);
extern int strcasecmp(char *s1, char *s2);
extern int strncasecmp(char *s1, char *s2, int n);


/*
 * []----------------------------------------------------------[]
 * | Read and cache a disk sector:				|
 * |								|
 * | This routine is used to read thru the common disk buffer	|
 * | cache. If we can't find the requested sector at in the 	|
 * | cache, we use "set_bcache" to load the cache with data 	|
 * | from disk.							|
 * |								|
 * | Returns address of a dyamically allocated buffer 		|
 * | containing the requested data; NULL if there's an I/O	|
 * | error.							|
 * []----------------------------------------------------------[]
 */
static devid_t readblock_dev;
static fileid_t readblock_fid;
static caddr_t
readblock(int volidx, int sector, int size)
{
	caddr_t bp;
	devid_t *dev;
	fileid_t *fid;

	dev = &readblock_dev;
	fid = &readblock_fid;

	dev->di_taken = 1;
	dev->di_dcookie = _device_fd[volidx];

	fid->fi_memp = 0;
	fid->fi_offset = 0;
	fid->fi_devp = dev;
	fid->fi_count = size;
	fid->fi_blocknum = sector;

	if (!(bp = get_bcache(fid))) {
		/*
		 *  Block not in cache.  Call "set_bcache" to put it there!
		 */

		bp = (set_bcache(fid) ? 0 : fid->fi_memp);
	}

	return (bp);
}

#ifdef i386
static int
arch_prom_open(int v, char *path)
{
	extern void *realp;
	void *rpx = realp;

	/*
	 * Remove primary/secondary boot communication buffer before
	 * calling prom_open to ensure that we open the boot device!
	 */
	realp = 0;
	_device_fd[v] = prom_open(path);
	realp = rpx;

	if (_device_fd[v] <= 0) {
		DPrint(DBG_ERR, ("Can't open device\n"));
		return (-1);
	} else
		return (0);
}

static int
arch_fix_spt(int v, _fdisk_p fp)
{
	struct ihandle *ihp;
	extern int read_blocks(struct ihandle *ihp, daddr_t off, int cnt);

	ihp = devp(_device_fd[v]);
	if (ihp->unit < 2) {
		/*
		 * What a tangled web we weave. If this is a floppy device
		 * we really don't know how many sectors per track are on
		 * the media. At this point the ihp->dev.disk.spt indicates
		 * the number of sectors the drive can support. On a 2.88Mb
		 * floppy that's 32. Most of the floppys we ship are 1.44Mb
		 * with 18 sectors per track. If we call prom_read the track
		 * buffering code will try to read a tracks worth and fail.
		 * Therefore, we must call read_blocks directory and find
		 * out what the device supports. Once that is done we can
		 * update ihp again.
		 */

		/*
		 * Borrow the shared cache to use as a generic I/O buffer.
		 * invalidate_cache makes sure no data is in the cache.
		 * setup_cache_buffers sets up the pointers used by
		 * read_blocks.  No subsequent call to invalidate_cache
		 * is required because there is no call to set_cache_buffer.
		 */
		invalidate_cache(NULL);
		setup_cache_buffers(ihp);

		if (read_blocks(ihp, 0, 1) != -1) {
			DPrint(DBG_ERR, ("Can't read device\n"));
			return (-1);
		} else {
			(void) memcpy((char *)pi[v]->f_sector,
				cache_info.active_buffer, SECSIZ);
			if (pi[v]->f_bpb.bs_spc == 0) {
				/*
				 * Invalid filesystem if bs_spc is 0.
				 * Better to fail here than to divide
				 * by zero in fat_init.  Should happen
				 * only for trashed filesystems or
				 * attempts to read non-filesystems.
				 */
				DPrint(DBG_ERR, ("No sectors in a cluster\n"));
				return (-1);
			}
			ihp->dev.disk.spt =
				ltohs(pi[v]->f_bpb.bs_sectors_per_track);
			ihp->dev.disk.spc = ihp->dev.disk.spt *
				ltohs(pi[v]->f_bpb.bs_heads);
		}
	} else if (prom_read(_device_fd[v], (char *)pi[v]->f_sector,
				SECSIZ, fp->fd_start_sec, 0) != SECSIZ) {
		DPrint(DBG_ERR, ("Can't read boot parameter block\n"));
		return (-1);
	}
	return (0);
}
#endif /* i386 */

static int
fat_init(int volidx, char *path, int type)
{
	_fdisk_t fd;

	if (arch_prom_open(volidx, path) ||
	    fat_fdisk(volidx, type, &fd) ||
	    arch_fix_spt(volidx, &fd))
		return (-1);

	/*
	 * Update the bios structure to reflect what has been found
	 * on the floppy or hard disk.
	 */
	pi[volidx]->f_rootsec =
	    (pi[volidx]->f_bpb.bs_num_fats * ltohs(pi[volidx]->f_bpb.bs_spf)) +
	    ltohs(pi[volidx]->f_bpb.bs_resv_sectors);

	pi[volidx]->f_rootlen =
	    (ltohs(pi[volidx]->f_bpb.bs_num_root_entries) *
	    sizeof (_dir_entry_t)) / SECSIZ;

	pi[volidx]->f_adjust = ltohi(fd.fd_start_sec);
	pi[volidx]->f_dclust = CLUSTER_ROOTDIR;
	pi[volidx]->f_filesec = pi[volidx]->f_rootsec + pi[volidx]->f_rootlen;
	pi[volidx]->f_nxtfree = CLUSTER_FIRST;

	/*
	 * figure out the number of clusters in this partition. If the
	 * "old" sectors_in_volume has a value use it (it's only a 16bit
	 * value). Otherwise use the newer sectors_in_logical_volume.
	 * Subject the number of clusters used by the file system overhead.
	 *
	 * XXX The pcfs code in the kernel adds in the offset of the partition
	 * and I've repeated it here for now. This seems wrong! It indicates
	 * more clusters than actually available. Normally this doesn't
	 * cause a problem because there's no offset on a floppy.
	 */
	pi[volidx]->f_ncluster =
	    (((u_long)ltohs(pi[volidx]->f_bpb.bs_siv) ?
		(u_long)ltohs(pi[volidx]->f_bpb.bs_siv) :
		(u_long)ltohi(pi[volidx]->f_bpb.bs_lsiv)) -
		pi[volidx]->f_filesec) /
		(u_long)pi[volidx]->f_bpb.bs_spc;
	pi[volidx]->f_16bit = pi[volidx]->f_ncluster >= CLUSTER_MAX_12;

	read_opt += 1;
	return (0);
}

static int
fat_fdisk(int volidx, int type, _fdisk_p fp)
{
	u_short *p;
	int i;
	long adjust;
	_fdisk_t fd[FDISK_PARTS];

	(void) memset(fp, 0, sizeof (*fp));

	if (!is_floppy(_device_fd[volidx])) {
		/*
		 * If it's not a floppy, it must be a hard disk.  Which
		 * means it has a partition table which we read in now ...
		 */
		char ftab[SECSIZ];

		if (prom_read(_device_fd[volidx], ftab, SECSIZ, 0, 0) !=
		    SECSIZ) {
			/*
			 * Can't read in the partition table.  There's not
			 * much we can do if we can't see it!
			 */
			DPrint(DBG_ERR, ("Can't read fdisk info\n"));
			return (-1);
		}

		read_opt += 1;
		p = (u_short *)&ftab[SECSIZ - sizeof (short)];

		if (*p == (u_short)0xaa55) {
			(void) memcpy(fd, &ftab[FDISK_START], sizeof (fd));
			for (i = 0, adjust = -1; i < FDISK_PARTS; i++) {
				if (((type != TYPE_DOS) &&
				    (fd[i].fd_type == type)) ||
				    ((type == TYPE_DOS) &&
				    (fd[i].fd_active == FDISK_ACTIVE))) {
					(void) memcpy(fp, &fd[i],
						sizeof (_fdisk_t));
					fd[i].fd_partition = (u_char)i;
					adjust = 0;
					break;
				} else if (fd[i].fd_active == FDISK_ACTIVE) {
					adjust = 1;
				}
			}
			if (adjust != 0) {
#ifdef	DEBUG
				if (adjust < 0)
					DPrint(DBG_ERR,
					    ("No active partition\n"));
#endif	/* DEBUG */
				return (-1);
			}
		} else {
			/* ---- If no fdisk part. Assume one big part ---- */
			DPrint(DBG_ERR, ("Invalid FDISK partition!\n"));
			fp->fd_start_sec = 0;
			fp->fd_part_len = 0;	/* unknown */
		}
	}

	return (0);
}

/*
 * []----------------------------------------------------------[]
 * | cluster to sector mapping					|
 * |								|
 * | map a cluster to disk block taking into accout the 	|
 * | different for the root directory (r == 1).			|
 * []----------------------------------------------------------[]
 */
static int
fat_ctodb(int volidx, int blk, int r)
{
	u_int s;

	s = r ? blk + pi[volidx]->f_rootsec + pi[volidx]->f_adjust :
	    ((blk - 2) * pi[volidx]->f_bpb.bs_spc) +
	    pi[volidx]->f_filesec + pi[volidx]->f_adjust;

	DPrint(DBG_FAT, ("ctodb(%x->%x):", blk, s));
	return (s);
}

/*
 * []----------------------------------------------------------[]
 * | cluster chain mapping					|
 * | 								|
 * | find the next cluster in the chain. many things happen	|
 * | in this routine. first is that the root directory doesn't	|
 * | have entries in the fat table and its just a simple calc	|
 * | to find the next block. the other big item is figuring out	|
 * | the cluster depending on weither entries are 16bits or	|
 * | encoded 12bit entries.					|
 * []----------------------------------------------------------[]
 */
static int
fat_map(int volidx, int blk, int rootdir)
{
	u_long sectn, fat_index;
	u_char *fp;

	if (rootdir) {
		return (blk > pi[volidx]->f_rootlen ? CLUSTER_EOF : blk + 1);
	}

	/* ---- Find out what sector this cluster is in ---- */
	fat_index = (pi[volidx]->f_16bit) ? ((u_long)blk << 1) :
	    ((u_long)blk + ((unsigned)blk >> 1));

	sectn = (fat_index / SECSIZ) + ltohs(pi[volidx]->f_bpb.bs_resv_sectors)
	    + pi[volidx]->f_adjust;

	/*
	 * Read two sectors so that if our fat_index points at the last byte
	 * byte we'll have the data needed.  This is only a problem for fat12
	 * entries.
	 */
	if (!(fp = (u_char *)readblock(volidx, sectn, (2 * SECSIZ)))) {
		/*
		 * I/O error reading the index block.  Return a bad block
		 * indicator to force error recovery.
		 */
		DPrint(DBG_ERR, ("Bad read on sector %d\n", sectn));
		return (CLUSTER_BAD_16);
	}

	DPrint(DBG_FAT, ("fat(%x->", blk));
	fp += (fat_index % SECSIZ);

	if (pi[volidx]->f_16bit)
		blk = fp[0] | (fp[1] << 8);
	else {
		if (blk & 1)
		    blk = ((((signed)fp[0] & 0xf0) >> 4) & 0xf) | (fp[1] << 4);
		else
		    blk = ((fp[1] & 0xf) << 8) | fp[0];

		/*
		 * This makes compares easier because we can just compare
		 * against one value instead of two.
		 */
		if (blk >= CLUSTER_RES_12_0)
		    blk |= CLUSTER_RES_16_0;
	}
	DPrint(DBG_FAT, ("%x):", blk));
	return (blk);
}

/*
 * []----------------------------------------------------------[]
 * | alloc clusters for a file					|
 * |								|
 * | Make sure the given file descriptor has the correct number |
 * | of clusters allocated.					|
 * []----------------------------------------------------------[]
 */
static int
fat_alloc(_file_desc_p f, int cc, int flg)
{
	int	c;		/* ---- allocated cluster number */
	int	cn;		/* ---- next cluster in chain if valid */

	DPrint(DBG_FAT, ("(fat_alloc [0x%x, %d, %d])",
	    f->f_index, cc, f->f_startclust));
	/* ---- Special case for new file. ---- */
	if (!cluster_valid(f->f_startclust, 0)) {
		c = fat_clalloc(f->f_volidx, flg);
		if (!cluster_valid(c, 0)) {
			DPrint(DBG_ERR,
			    ("fat_alloc: Can't alloc first blk of file\n"));
			return (-1);
		}
		f->f_startclust = c;
		updatedir(f, 0, 0);
	}

	/*
	 * always need to subtract from the first cluster which is handled
	 * in the case above.
	 */
	cc--;

	/* ---- loop while cc > 0 allocating clusters if necessary ---- */
	c = f->f_startclust;
	while (cc-- > 0) {
		cn = fat_map(f->f_volidx, c, 0);
		if (!cluster_valid(cn, 0)) {
			cn = fat_clalloc(f->f_volidx, flg);
			if (!cluster_valid(cn, 0))
			    return (-1);
			fat_setcluster(f->f_volidx, c, cn);
		}
		c = cn;
	}
	return (0);
}

static
fat_clalloc(int volidx, int flg)
{
	int	c;		/* ---- current cluster being looked at */
	u_long	s;		/* ---- sector for zero fill stuff */
	int	i;		/* ---- loop counter for zero fill stuff */
	char	blk[SECSIZ];	/* ---- block used to zero sectors */

	/* ---- the noise from searching the fat here is just to much ---- */
	Clear_dbbit(DBG_FAT);

	/* ---- search fat table a find an unused entry ---- */
	for (c = pi[volidx]->f_nxtfree;
	    c < pi[volidx]->f_ncluster + CLUSTER_FIRST; c++) {
		if (fat_map(volidx, c, 0) == CLUSTER_AVAIL) {
			Resto_dbbit();
			DPrint(DBG_FAT, ("(clalloc 0x%x)", c));
			fat_setcluster(volidx, c, CLUSTER_EOF);
			if (flg == CLUSTER_ZEROFILL) {
				s = fat_ctodb(volidx, c, 0);
				bzero((char *)blk, SECSIZ);
				for (i = 0; i < pi[volidx]->f_bpb.bs_spc; i++) {
					if (prom_write(_device_fd[volidx],
					    blk, SECSIZ, s + i, 0) == -1) {
						DPrint(DBG_ERR,
						    ("Can't zero %d\n", c));
						return (CLUSTER_BAD_16);
					}
					release_cache(_device_fd[volidx]);
				}
			}
			pi[volidx]->f_nxtfree = c + 1;
			return (c);
		}
	}
	return (CLUSTER_AVAIL);
}

static
fat_setcluster(int volidx, int c, int v)
{
	u_long	idx;		/* ---- index into fat table */
	u_long	s;		/* ---- sector number to read if needed */
	u_char	*fp;		/* ---- pointer to fat entries */
	u_char *fat_sector_p;

	DPrint(DBG_FAT, ("(setcl %x->%x)", c, v));

	idx = pi[volidx]->f_16bit ? (u_long)c << 1 : (u_long)c + (c >> 1);
	s = idx / SECSIZ + ltohs(pi[volidx]->f_bpb.bs_resv_sectors) +
	    pi[volidx]->f_adjust;

	if ((fat_sector_p = (u_char *)readblock(volidx, s, 2 * SECSIZ)) == 0) {
		return (CLUSTER_BAD_16);
	}
	fp = &fat_sector_p[idx % SECSIZ];

	if (pi[volidx]->f_16bit) {
		*(u_short *)fp = v;
	} else {
		if (c & 1) {
			*fp = (*fp & 0x0f) | ((v << 4) & 0xf0);
			fp++;
			*fp = (v >> 4) & 0xff;
		} else {
			*fp++ = v & 0xff;
			*fp = (*fp & 0xf0) | ((v >> 8) & 0x0f);
		}
	}

	if (prom_write(_device_fd[volidx], (char *)fat_sector_p,
	    2 * SECSIZ, s, 0) == -1)
		return (CLUSTER_BAD_16);
	release_cache(_device_fd[volidx]);

	if (v == CLUSTER_AVAIL)
		pi[volidx]->f_nxtfree = CLUSTER_FIRST;
	return (v);
}

static _file_desc_p
get_file(int fd)
{
	_file_desc_p fp = file_table_h;

	while (fp && (fp->f_desc != fd)) {
		fp = fp->f_forw;
	}
	if (fd == -1) {
		if ((fp == 0) &&
		    (fp = (_file_desc_p) kmem_alloc(sizeof (_file_desc_t)))) {
			fp->f_forw = file_table_h;
			file_table_h = fp;
		}
		if (fp != 0) {
			fp->f_desc = file_num++;
		}
	}
#ifdef	DEBUG
	if (fp)
		DPrint(DBG_GEN, ("get_file(%d, 0x%x)", fd, fp->f_index));
#endif	/* DEBUG */
	return (fp);
}

/*
 *  Check that a volume label pointed at by arg is valid.  We don't assume
 *  that the volume label is necessarily terminated by a null character.
 *  That way we can hand this routine a pointer directly into the BPB.
 *  We also want to be able to use this with other types of volume pointers
 *  so we will assume that the 'volume name' ends when we either reach
 *  a colon or the max size for a volume label.
 */
int
validvolname(char *testname)
{
	char c;
	int i, valid = 1;

	DPrint(DBG_VOL, ("validvolname:"));

	i = 0;
	while (((c = testname[i]) != ':') && (i < VOLLABELSIZE)) {
		DPrint(DBG_VOL, ("%c", c));
		if ((c < ' ') || ((c >= ':') && (c <= '>')) ||
		    ((c >= '[') && (c <= ']')) || (c == '|') ||
		    (c == '\"') || (c == '/') || (c == '+')) {
			valid = 0;
			break;
		}
		i++;
	}
	return (valid);
}

struct vollst {
	char *volname;
	struct vollst *next;
};

static struct vollst *VolumesSeen;

#define	BPB_VOLNAM(i) \
	(pi[i]->f_bpb.bs_volume_label)

static void
volnormalize(char *src, char *norm)
{
	int ct;

	/*
	 * Put volume name in normal form.
	 */
	for (ct = 0; ct < VOLLABELSIZE && src[ct] != ':'; ct++)
		norm[ct] = src[ct];

	for (; ct < VOLLABELSIZE; ct++)
		norm[ct] = ' ';

	norm[VOLLABELSIZE] = ':';
	norm[VOLLABELSIZE+1] = '\0';
}

static char *
vollst(char *srcvol, int normalized)
{
	struct vollst *vl, *nvl;
	char tmpvolnam[VOLLABELSIZE+2];
	char *volname = NULL;
	char *cmpvol;
	int ct;

	if (normalized) {
		cmpvol = srcvol;
	} else {
		volnormalize(srcvol, tmpvolnam);
		cmpvol = tmpvolnam;
	}

	vl = VolumesSeen;
	while (vl) {
		if (VOLCMP(vl->volname, cmpvol) == 0) {
			volname = vl->volname;
			break;
		} else {
			vl = vl->next;
		}
	}

	if (!volname) {
		/*
		 * First time we've seen the volume, so add it to list.
		 */
		if (normalized) {
			for (ct = 0; ct < VOLLABELSIZE; ct++)
				tmpvolnam[ct] = srcvol[ct];
			tmpvolnam[VOLLABELSIZE] = ':';
			tmpvolnam[VOLLABELSIZE+1] = '\0';
		}

		if ((volname = (char *)kmem_alloc(VOLLABELSIZE+2)) == NULL)
			prom_panic("No memory for a volume label!?");

		if ((nvl = (struct vollst *)
		    kmem_alloc(sizeof (struct vollst))) == NULL) {
			prom_panic("No memory for volume info!");
		}
		strcpy(volname, tmpvolnam);
		nvl->volname = volname;
		nvl->next = VolumesSeen;
		VolumesSeen = nvl;
	}

	return (volname);
}

/*
 *  If we can't retrieve a volume name from the disk we'll improvise one
 *  from the BPB field where the field should be.
 */
static char *
improvvolname(char *source)
{
	static char iv[VOLLABELSIZE+2];
	char c;
	int i;

	for (i = 0; i < VOLLABELSIZE; i++) {
		c = source[i];
		if ((c < ' ') || ((c >= ':') && (c <= '>')) ||
		    ((c >= '[') && (c <= ']')) || (c == '|') ||
		    (c == '\"') || (c == '/') || (c == '+')) {
			iv[i] = ' ';
		} else {
			iv[i] = source[i];
		}
	}
	iv[i++] = ':';
	iv[i] = '\0';

	return (iv);
}

static char *
getvolname(int volidx)
{
	char *volname = NULL;
	char *src = NULL;

	DPrint(DBG_VOL, ("getvolname:%d:", volidx));

	if (volidx >= 0 && volidx < MAX_PCFS_MOUNTS) {
		/* First check for a volume label in the BPB */
		if (!validvolname(src = BPB_VOLNAM(volidx))) {
			if (!(src = (char *)dirlookupvolname(volidx)))
				src = improvvolname(BPB_VOLNAM(volidx));
		}
	}

	volname = vollst(src, 0);
	DPrint(DBG_VOL, ("=%s\n", volname));
	return (volname);
}

boot_pcfs_mountroot(char *s)
{
	int dblck;

	DPrint(DBG_GEN, ("(dosMount %s)", s));

	/* ---- Allocate space for various structures. ---- */
	if (Pcfs_cnt < MAX_PCFS_MOUNTS &&
	    ((pi[Pcfs_cnt] = (_fat_controller_p)
	    kmem_alloc(sizeof (_fat_controller_t))) != NULL)) {

		if (!fat_init(Pcfs_cnt, s, TYPE_SOLARIS_BOOT)) {
			/* No error setting up file table */

			DPrint(DBG_VOL, ("After FAT initted. "));
			DRun(DBG_VOL, splatinfo(_device_fd[Pcfs_cnt]));
			DRun(DBG_VOL, goany());

			for (dblck = 0; dblck < Pcfs_cnt; dblck++) {
				if (_device_fd[Pcfs_cnt] == _device_fd[dblck]) {
					/*
					 *  This device is already mounted.
					 *  We don't need or want to devote new
					 *  resources to it.  Quietly ignore
					 *  this mount.
					 */
					(void) prom_close(_device_fd[Pcfs_cnt]);
					kmem_free((caddr_t)pi[dblck],
					    sizeof (_fat_controller_t));
					pi[dblck] = pi[Pcfs_cnt];
					pi[Pcfs_cnt] = 0;
					return (0);
				}
			}

			/*
			 *  The first successful pcfs mount is treated
			 *  as the 'boot device', upon which all unqualified
			 *  (i.e., no volume specified) files are expected
			 *  to live, if mounted at boot time.
			 *
			 *  XXX: the mountroot() entry point is overloaded
			 *  to allow multiple diskettes to be swapped.
			 *  This workaround allows ITU diskettes to
			 *  not be confused with the boot diskette.
			 *  Note however that this workaound still
			 *  does not avoid the case of a non-boot floppy
			 *  inserted at hard disk boot time.
			 *
			 *  We can write to PCFS on a floppy, so set
			 *  boot_root_writable in that case.  Note that
			 *  we can't support writes to PCFS on a boot
			 *  partition because the BEFs don't support writes.
			 */
			Curvol[Pcfs_cnt] = getvolname(Pcfs_cnt);

			if (!Pcfs_cnt && strcmp(s, FLOPPY0_NAME) &&
			    strcmp(s, FLOPPY1_NAME)) {
				bootpcfs = Pcfs_cnt;
				rootvol = bootvolume = Curvol[Pcfs_cnt];
				if (is_floppy(_device_fd[Pcfs_cnt]))
					boot_root_writable = 1;
			}

			/*
			 * The case of an override diskette with CDROM boot
			 * is not handled by the code above.  Need to avoid
			 * leaving bootvolume null.  [This issue passed
			 * unnoticed during initial cdboot development
			 * but showed up when page 0 unmapping was restored.]
			 */
			if (bootpcfs == -1 && bootvolume == 0) {
				bootpcfs = 0;
				bootvolume = Curvol[0];
			}

			if (floppy0pcfs < 0 &&
			    is_floppy0(_device_fd[Pcfs_cnt])) {
				floppy0pcfs = Pcfs_cnt;
			} else if (floppy1pcfs < 0 &&
			    is_floppy1(_device_fd[Pcfs_cnt])) {
				floppy1pcfs = Pcfs_cnt;
			}

			DPrint(DBG_VOL,
			    ("A Pcfs was mounted on device %s.\n", s));
			DPrint(DBG_VOL,
			    ("Mounted volume %s.\n", Curvol[Pcfs_cnt]));

			Pcfs_cnt++;
			DPrint(DBG_VOL, ("DosMount:%d\n", Pcfs_cnt));
			return (0);
		}

		kmem_free((caddr_t)pi[Pcfs_cnt], sizeof (_fat_controller_t));
		pi[Pcfs_cnt] = 0;
	}

	return (-1);
}

/*
 * Unmount the root fs -- unsupported for this fstype.
 */
int
boot_pcfs_unmountroot(void)
{
	return (-1);
}

int
boot_pcfs_fstat(int fd, struct stat *sp)
{
	_file_desc_p fp;
	int rtn;

	DPrint(DBG_GEN, ("(Fstat %d)", fd));
	if ((fd < 0) || !(fp = get_file(fd))) {
		rtn = -1;
	} else {
		if (fp->f_attr & DE_DIRECTORY)
			sp->st_mode = S_IFDIR;
		else if (fp->f_attr & DE_LABEL)
			/*
			 * Blatant mis-use of fields here, but there is no
			 * S_IFLAB.
			 */
			sp->st_mode = S_IFIFO;
		else
			sp->st_mode = S_IFREG;
		sp->st_size = fp->f_len;
		rtn = 0;
	}
	return (rtn);
}

int
boot_pcfs_open(char *n, int perm)
{
	_dir_entry_t d;
	_file_desc_p f;
	char *fn, *vn;
	int volidx;

	DPrint(DBG_GEN, ("dosOpen(%s, %d)", n, perm));

	/*
	 * Parse any volume qualifier out of the name here. If the
	 * necessary volume is not present this call should
	 * ensure the right volume is present before we get back.
	 */
	DPrint(DBG_VOL, ("dosOpen:<%s>", n));
	if ((volidx = loadVolume(n, &vn, &fn, UNKNOWN_VOLUME_IDX)) < 0) {
		DPrint(DBG_ERR, ("Couldn't get volume!\n"));
		return (-1);
	}

	f = get_file(-1);

	if (!f) {
		DPrint(DBG_ERR, ("(open(%s - failed)", n));
		return (-1);
	}

	DPrint(DBG_VOL, (":v%d:", volidx));
	f->f_volidx = volidx;
	f->f_volname = Curvol[volidx];

	if (lookuppn(fn, &d, f)) {
		f->f_desc = -1;
		DPrint(DBG_ERR, ("open(lookup(%s) - failed)", n));
		return (-1);
	}

	/* ---- can't modify a readonly file or directory ---- */
	if ((perm & (FILE_WRITE|FILE_TRUNC)) &&
	    (d.d_attr & (DE_READONLY|DE_DIRECTORY))) {
		f->f_desc = -1;
		DPrint(DBG_ERR,
		    ("open(bad access %x)", d.d_attr));
		return (-1);
	}

	f->f_startclust = d.d_cluster;
	f->f_off = 0;
	f->f_len = d.d_size;
	f->f_attr = d.d_attr;

	if (perm & FILE_TRUNC)
		dosFtruncate(f);

	DPrint(DBG_GEN, ("(%d, %d)", f->f_desc, f->f_startclust));
	return (f->f_desc);
}

int
boot_pcfs_close(int fd)
{
	_file_desc_p	fp;

	DPrint(DBG_GEN, ("dosClose(%d):\n", fd));
	if ((fd >= 0) && (fp = get_file(fd))) {
		/* ---- don't free rest of space, leave it in the pool ---- */
		fp->f_desc = -1;
		return (0);
	} else {
		return (-1);
	}
}

/*ARGSUSED*/
static void
boot_pcfs_closeall(int flag)
{
	_file_desc_p	fp, fn;
	struct vollst	*vl;
	int mtct;

	DPrint(DBG_GEN, ("(dosCloseAll)"));
	fp = file_table_h;
	while (fp) {
		fn = fp->f_forw;
		kmem_free((caddr_t)fp, sizeof (_file_desc_t));
		fp = fn;
	}
	file_table_h = 0;

	for (mtct = 0; mtct < Pcfs_cnt; mtct++) {
		if (pi[mtct]) {
			kmem_free((caddr_t)pi[mtct],
			    sizeof (_fat_controller_t));
		}
		pi[mtct] = 0;
		release_cache(_device_fd[mtct]);
		(void) prom_close(_device_fd[mtct]);
	}

	Pcfs_cnt = 0;
	bootvolume = 0;
	bootpcfs = floppy0pcfs = floppy1pcfs = -1;

	vl = VolumesSeen;
	VolumesSeen = 0;
	while (vl) {
		struct vollst *next;
		next = vl->next;
		kmem_free(vl->volname, VOLLABELSIZE+2);
		kmem_free((char *)vl, sizeof (struct vollst));
		vl = next;
	}
}

off_t
boot_pcfs_lseek(int fd, off_t off, int whence)
{
	_file_desc_p	fp;

	DPrint(DBG_GEN, ("(dosLseek %d, %d, %d)", fd, off, whence));
	if ((fd < 0) || !(fp = get_file(fd))) {
		return (-1);
	}
	switch (whence) {
		case SEEK_CUR:
			fp->f_off += off;
			break;

		case SEEK_SET:
			fp->f_off = off;
			break;
		case SEEK_END:
		default:
			DPrint(DBG_ERR,
			    ("pcfs_lseek(): invalid whence value %d\n",
			    whence));
			break;
	}
	return (fp->f_off);
}

ssize_t
boot_pcfs_read(int fd, caddr_t b, size_t c)
{
	u_long		sector;
	u_int		count = 0, xfer, i;
	char		block[SECSIZ];
	u_long		off;
	u_long		blk;
	int		rd;
	int		spc;
	int		volidx;
	_file_desc_p f;

	DPrint(DBG_GEN, ("(dosread(%d, 0x%x, %d):", fd, b, c));

	if ((fd < 0) || !(f = get_file(fd))) {
		DPrint(DBG_ERR, ("Bad fd %d\n", fd));
		return (-1);
	}

	if ((volidx = loadVolume(f->f_volname, NULL, NULL, f->f_volidx)) < 0) {
		DPrint(DBG_ERR, ("Couldn't get volume %s\n", f->f_volname));
		return (-1);
	}
	f->f_volidx = volidx;
	DPrint(DBG_VOL, (":v%d:", volidx));

	spc = pi[volidx]->f_bpb.bs_spc;
	off = f->f_off;
	blk = f->f_startclust;
	rd = blk == CLUSTER_ROOTDIR ? 1 : 0;

	if ((c = MIN(f->f_len - off, c)) <= 0) {
		return (0);
	}

	while (off >= fat_bpc(volidx)) {
		blk = fat_map(volidx, blk, rd);
		off -= fat_bpc(volidx);

		if (!cluster_valid(blk, rd)) {
			DPrint(DBG_ERR, ("dosRead: Bad map entry\n"));
			return (-1);
		}
	}

	while (count < c) {
		sector = fat_ctodb(volidx, blk, rd);
		for (i = ((off / SECSIZ) % pi[volidx]->f_bpb.bs_spc);
		    i < spc; i++) {

			xfer = MIN(SECSIZ-(off%SECSIZ), c-count);
			if (!xfer) {
				/*
				 * We just transferred the last sector;
				 * break loop
				 */
				break;
			}

			if (prom_read(_device_fd[volidx], &block[0], SECSIZ,
			    sector + i, 0) < 0) {
				DPrint(DBG_ERR,
				    ("dosRead: prom_read failed\n"));
				return (-1);
			}

			(void) memcpy(b, &block[off % SECSIZ], (int)xfer);
			count += xfer;
			off += xfer;
			b += xfer;

			read_opt += 1;
		}

		if (count < c) {
			/* Move to next cluster ...	*/

			blk = fat_map(volidx, (u_short)blk, rd);
			if (!cluster_valid(blk, rd)) break;
		}
	}

	f->f_off += count;
	return (count);
}


boot_pcfs_write(int fd, char *b, u_int cc)
{
	u_int	lb;		/* ---- number of cluster to alloc */
	u_short	c;		/* ---- starting cluster for write */
	u_short	cn;		/* ---- next cluster in chain */
	u_long	off;		/* ---- file offset, used to find cluster */
	int	lc;		/* ---- current loop count */
	u_long	s;		/* ---- sector number to write */
	int	lcc;		/* ---- loop count when len is less than lc */
	int	ccout = cc;	/* ---- return count if all ok */
	int	i;		/* ---- loop counter used with s */
	char	block[SECSIZ];	/* ---- storage for rmw */
	int	volidx;		/* ---- volume where file lives */
	_file_desc_p f;

	DPrint(DBG_GEN, ("dosWrite(%d, 0x%x, %d)", fd, b, cc));
	if ((fd < 0) || !(f = get_file(fd))) {
		return (-1);
	}

	if ((volidx = loadVolume(f->f_volname, NULL, NULL, f->f_volidx)) < 0) {
		DPrint(DBG_ERR, ("Couldn't get volume %s\n", f->f_volname));
		return (-1);
	}

	f->f_volidx = volidx;
	DPrint(DBG_VOL, (":v%d:", volidx));

	if (cc == 0) {
		/*
		 * Special case of truncate for dos programs. Check to make
		 * sure that the offset is zero and just truncate the file.
		 * If not complain that we don't support truncating to a
		 * specific length yet. It shouldn't be that hard to do.
		 */

		if (f->f_off) {
			printf("Can't truncate when offset is non zero\n");
			return (-1);
		} else {
			dosFtruncate(f);
			return (0);
		}
	}

	/* ---- alloc blocks for file ---- */
	if ((f->f_off + cc) > f->f_len) {
		lb = (f->f_off + cc + (fat_bpc(volidx) - 1)) /
		    (u_long)fat_bpc(volidx);
		if (fat_alloc(f, lb, CLUSTER_NOOP)) {
			DPrint(DBG_ERR,
			    ("Failed to alloc %d cluster for file\n", lb));
			return (-1);
		}
	}

	/* ---- find starting cluster for given file offset ---- */
	c = f->f_startclust;
	off = f->f_off;
	while (off >= fat_bpc(volidx)) {
		cn = fat_map(volidx, c, 0);
		off -= fat_bpc(volidx);
		c = cn;
		/* ... could check here for valid block ... */
	}

	DPrint(DBG_WRITE, ("(doswrite %d bytes @ %d, %d)", cc, f->f_off, c));
	/* ---- loop through blocks writing out data ---- */
	while (cc) {
		lc = MIN(fat_bpc(volidx), cc);
		s = fat_ctodb(volidx, c, 0);

		/*
		 * Do we have exactly a clusters worth of data aligned
		 * on a cluster boundary? If so, we can just pass it onto
		 * the low level write routine. Else we'll have to break
		 * up the write into a read/modify/write cycle.
		 */
		if ((lc == fat_bpc(volidx)) &&
		    ((f->f_off % (u_long)fat_bpc(volidx)) == 0)) {
			if (prom_write(_device_fd[volidx], b, lc, s, 0) == -1) {
				DPrint(DBG_ERR,
				    ("Failed to write cluster %d\n", c));
				return (-1);
			}
			release_cache(_device_fd[volidx]);
			b += lc;
			f->f_off += lc;
			cc -= lc;
			DPrint(DBG_WRITE, ("(CLUSTER WRITE)"));
		} else {
			/*
			 * The file offset is someplace within the current
			 * cluster. Start off by finding which sector
			 * has the offset. Then work on a sector by sector
			 * basis until the data is transfered or
			 * we've written to all of the sectors in this cluster
			 */
			for (i = (f->f_off % (u_long)fat_bpc(volidx)) /
			    (u_long)SECSIZ; i < pi[volidx]->f_bpb.bs_spc; i++) {
				lcc = MIN(cc, SECSIZ -
				    (f->f_off % (u_long)SECSIZ));

				/*
				 * only read in block if less than full
				 * sector will be written
				 */
				if ((lcc != SECSIZ) &&
				    prom_read(_device_fd[volidx], block, SECSIZ,
				    s + i, 0) == -1) {
					DPrint(DBG_ERR,
					    ("Failed to rmw block %ld\n", s));
					return (-1);
				}

				bcopy(b, &block[f->f_off % (u_long)SECSIZ],
				    lcc);
				if (prom_write(_device_fd[volidx], block,
				    SECSIZ, s + i, 0) == -1) {
					DPrint(DBG_ERR,
					    ("Failed to write blk %ld\n", s));
					return (-1);
				}
				release_cache(_device_fd[volidx]);
				f->f_off += lcc;
				b += lcc;
				cc -= lcc;
				if (lc == 0)
					break;
			}
		}
		c = fat_map(volidx, c, 0);
		/* ... could check for valid block ... */
	}

	/*
	 * need to update the directory entry if our length has changed
	 * because of the write
	 */
	if (f->f_off > f->f_len) {
		f->f_len = f->f_off;
		updatedir(f, 0, 0);
	}

	DPrint(DBG_WRITE, ("(wrote %d)", ccout));
	return (ccout);
}

/*ARGSUSED1*/
int
dosCreate(char *fn, int mode)
{
	_dir_entry_t d;
	_dir_entry_p dp;	/* ---- pointer to directory entries */
	_dir_entry_p dp0;	/* ---- base pointer of current cluster */
	_file_desc_p	fp;	/* ---- create fd if okay */
	char	*p,		/* ---- pointer to name component */
		*pdot,		/* ---- pointer to '.' in name component */
		*cfn,		/* ---- copy of filename to modify */
		*vn,		/* ---- volume name */
		*pn;		/* ---- path name */
	int	c,		/* ---- cluster of directory */
		i,		/* ---- loop counter */
		rd,		/* ---- working with root dir */
		lvc = 0,	/* ---- last valid cluster for dir */
		spc,		/* ---- sectors per cluster */
		volidx,		/* ---- which volume to place file upon */
		cfn_len;	/* ---- len of malloc'd string */
	u_long	s;		/* ---- disk sector for cluster */

	DPrint(DBG_GEN, ("dosCreate(%s, %d)", fn, mode));

	if ((volidx = loadVolume(fn, &vn, &pn, UNKNOWN_VOLUME_IDX)) < 0) {
		DPrint(DBG_ERR, ("Couldn't get volume\n"));
		return (-1);
	}

	spc = pi[volidx]->f_bpb.bs_spc;

	/* ---- if file exists truncate ---- */
	fp = get_file(-1);

	DPrint(DBG_VOL, (":v%d:", volidx));
	fp->f_volidx = volidx;
	fp->f_volname = Curvol[volidx];

	if (!lookuppn(pn, &d, fp)) {
		/*
		 * Before we truncate an existing file make sure that it's
		 * writable and not a directory.
		 */

		if (d.d_attr & (DE_READONLY|DE_DIRECTORY)) {
			fp->f_desc = -1;
		} else {
			DPrint(DBG_CREAT,
			    ("dosCreate: %s already exists\n", fn));
			dosFtruncate(fp);
		}
		return (fp->f_desc);
	}

	if (cfn = kmem_alloc(cfn_len = (strlen(pn) + 1)))
		strcpy(cfn, pn);
	else
		return (-1);

	if (((p = strrchr(cfn, '/')) != NULL) ||
	    ((p = strrchr(cfn, '\\')) != NULL)) {
		/*
		 * Need to find the parent directory which starts from the
		 * top. If there's only one slash then the file is to be
		 * created in the root directory. Otherwise we need to find
		 * the parent. NOTE: we pass in fp to the lookuppn routine
		 * when can have a side effect of setting f_index to the
		 * parent directory inode. This is not a problem because
		 * f_index is reset below when we create the entry in the
		 * parents directory.
		 */
		*p++ = '\0';
		if ((*cfn == '\0') || EQ("/", cfn) || EQ("\\", cfn)) {
			DPrint(DBG_CREAT, ("dosCreate: Using root dir\n"));
			d.d_cluster = CLUSTER_ROOTDIR;
		} else if (lookuppn(cfn, &d, fp)) {
			DPrint(DBG_ERR, ("can't find parent directory!\n"));
			kmem_free(cfn, cfn_len);
			return (-1);
		}
	} else {
		/*
		 * just create the file name in the current directory.
		 */
		d.d_cluster = pi[volidx]->f_dclust;
		p = cfn;
	}

	/* ---- search for empty slot ---- */
	c = d.d_cluster;

	/* ---- are we dealing with the root directory? ---- */
	rd = c == 0;

	while (cluster_valid(c, rd)) {
		s = fat_ctodb(volidx, c, rd);
		if (!(dp =
		    (_dir_entry_p)readblock(volidx, s, fat_bpc(volidx)))) {
			DPrint(DBG_CREAT, ("Failed to read dir\n"));
			kmem_free(cfn, cfn_len);
			return (-1);
		}
		for (dp0 = dp, i = 0; i < (DIRENTS * spc); i++, dp++) {
			if ((dp->d_name[0] == 0) ||
			    (dp->d_name[0] == (char)0xe5)) {
				fp->f_index = (s*spc*DIRENTS) + i;
				goto gotit;
			}
		}
		lvc = c;
		c = fat_map(volidx, lvc, rd);

		/*
		 *[]----------------------------------------------------[]
		 * | The root directory is fixed in size because it has |
		 * | been created using continuous blocks and doesn't 	|
		 * | appear in the fat. So only expand non-root		|
		 * |directories.					|
		 *[]----------------------------------------------------[]
		 */
		if (!rd && !cluster_valid(c, rd)) {
			c = fat_clalloc(volidx, CLUSTER_ZEROFILL);
			if (!cluster_valid(c, rd)) {
				DPrint(DBG_ERR, ("Out of space\n"));
				kmem_free(cfn, cfn_len);
				return (-1);
			}
			else
				fat_setcluster(volidx, lvc, c);
		}
	}

#ifdef	DEBUG
	if (!rd) DPrint(DBG_ERR, ("Oops. This can't happen\n"));
#endif	/* DEBUG */
	kmem_free(cfn, cfn_len);
	return (-1);

gotit:
	/* ---- add the entry ---- */
	(void) memset(dp->d_name, ' ', NAMESIZ);
	(void) memset(dp->d_ext, ' ', EXTSIZ);
	if (pdot = (char *)strchr(p, '.')) {
		*pdot++ = '\0';
		copyup(pdot, dp->d_ext, strlen(pdot));
		pdot--;
	}
	copyup(p, dp->d_name, strlen(p));

#ifdef notdef
	dp->d_attr = (mode & FILE_WRITE ? 0 : DE_READONLY) | DE_ARCHIVE;
#else
	dp->d_attr = DE_ARCHIVE;
#endif
	dp->d_time = 1;		/* ... should set the real time */
	dp->d_date = 1;		/* ... can we find the date? */
	dp->d_cluster = 0;
	dp->d_size = 0;

	DPrint(DBG_CREAT, ("dosCreate(%s, mode 0x%x, id 0x%x, sec %d):",
	    dosname(dp->d_name, dp->d_ext), mode, fp->f_index, s));

	/* ---- write out directory cluster ---- */
	if (prom_write(_device_fd[volidx], (char *)dp0,
	    fat_bpc(volidx), s, 0) == -1) {
		DPrint(DBG_ERR,
		    ("(create failed: %s: sec %d, bytes %d)",
		    dosname(dp->d_name, dp->d_ext), s, fat_bpc(volidx)));
		kmem_free(cfn, cfn_len);
		return (-1);
	}
	release_cache(_device_fd[volidx]);
	kmem_free(cfn, cfn_len);
	fp->f_startclust = dp->d_cluster;
	fp->f_off = 0;
	fp->f_len = dp->d_size;
	fp->f_attr = dp->d_attr;
	return (fp->f_desc);
}

/*ARGSUSED1*/
static void
unlink_name(_dir_entry_p dp, void *v)
{
	dp->d_name[0] = (char)0xe5;
}

/*
 * []----------------------------------------------------------[]
 * | unlink -- remove a file from the given tree. first		|
 * []----------------------------------------------------------[]
 */
dosUnlink(char *nam)
{
	_file_desc_p	f = get_file(-1);
	_dir_entry_t	d;
	char		*fn,	/* file name for volume stuff */
			*vn;	/* volume name */
	int		vol;

	DPrint(DBG_GEN, ("dosUnlink(%s)", nam));
	if ((vol = loadVolume(nam, &vn, &fn, UNKNOWN_VOLUME_IDX)) < 0) {
		DPrint(DBG_ERR, ("dosUnlink: Couldn't get volume\n"));
		return (DOSERR_PATHNOTFOUND);

	} else {
		f->f_volidx = vol;
		f->f_volname = Curvol[vol];

		if (lookuppn(fn, &d, f)) {
			DPrint(DBG_ERR, ("Unlink: failed to find %s\n", fn));
			f->f_desc = -1;
			return (DOSERR_FILENOTFOUND);

		} else if (d.d_attr & (DE_READONLY|DE_DIRECTORY)) {
			DPrint(DBG_ERR, ("Unlink: file read-only\n"));
			f->f_desc = -1;
			return (DOSERR_ACCESSDENIED);

		} else {
			DPrint(DBG_GEN, ("Unlinking: %s\n", fn));
			dosFtruncate(f);
			updatedir(f, unlink_name, 0);
			f->f_desc = -1;

			return (0);
		}
	}
}

static void
rename_file(_dir_entry_p dp, void *v)
{
	char	*t = (char *)v,
		*t1;

#ifdef DEBUG
	printf("(rename_file, t = 0x%x, ", t);
	printf("%s)", t);
	printf("(dp 0x%x, name = ", dp);
	printf("%s)", dosname(dp->d_name, dp->d_ext));
#endif

	(void) memset(dp->d_name, ' ', NAMESIZ);
	(void) memset(dp->d_ext, ' ', EXTSIZ);

	if (t1 = strchr(t, '.')) {
		*t1++ = 0;
		copyup(t1, dp->d_ext, strlen(t1));
	}
	copyup(t, dp->d_name, strlen(t));
}

dosRename(char *nam, char *t)
{
	_file_desc_p	f = get_file(-1);
	_dir_entry_t	d;
	char		*fn,	/* file name for volume stuff */
			*vn,	/* volume name */
			*f1,	/* dir entry of path fn */
			*t1;	/* dir entry of path t */
	int		vol;

	DPrint(DBG_GEN, ("dosRename(%s -> %s)", nam, t));
	if ((vol = loadVolume(nam, &vn, &fn, UNKNOWN_VOLUME_IDX)) < 0) {
		DPrint(DBG_ERR, ("Rename: Couldn't get volume\n"));
		return (DOSERR_PATHNOTFOUND);

	} else {

		f->f_volidx = vol;
		f->f_volname = Curvol[vol];
		if (!lookuppn(t, &d, f)) {
			DPrint(DBG_ERR, ("Rename: target %s exists\n", t));
			f->f_desc = -1;
			return (DOSERR_ACCESSDENIED);

		} else if (lookuppn(fn, &d, f)) {
			DPrint(DBG_ERR, ("Rename: Couldn't find %s\n", fn));
			f->f_desc = -1;
			return (DOSERR_FILENOTFOUND);

		} else {

			if (f1 = strrchr(fn, '/')) *f1++ = 0;
			if (t1 = strrchr(t, '/')) *t1++ = 0;

			if (strcmp(fn, t)) {
				/*
				 * Wimping out here. There's no reason we
				 * can't move files from one directory to
				 * another. All that needs to be done is to
				 * create the file in the other directory. Set
				 * the starting cluster to be the same as this
				 * one and then unlink the current file
				 * without freeing the block chain.
				 */

				DPrint(DBG_ERR,
				    ("Rename: files in different dir\n"));
				return (DOSERR_ACCESSDENIED);
			}

			DPrint(DBG_GEN, ("Rename %s to %s\n", f1, t1));
			updatedir(f, rename_file, t1);
			return (0);
		}
	}
}

/*
 * []----------------------------------------------------------[]
 * | update the given file's dir entry.				|
 * | This routine depends on the lookup() to store the file	|
 * | id into the _file_desc_p. Without which this routine	|
 * | will not work correctly.					|
 * []----------------------------------------------------------[]
 */
void
updatedir(_file_desc_p f, void (*cb)(_dir_entry_p, void *), void *arg)
{
	_dir_entry_p	dp;	/* ---- dir entry for this fp */
	int	free_dp = 0,	/* ---- dp has been malloc'd */
		i = f->f_index % (DIRENTS*pi[f->f_volidx]->f_bpb.bs_spc),
		s = f->f_index / (DIRENTS*pi[f->f_volidx]->f_bpb.bs_spc);

	/* ---- read in dir block containing dir entry for this fp ---- */
	if (dp = (_dir_entry_p)readblock(f->f_volidx, s,
	    fat_bpc(f->f_volidx))) {

		/* ---- update important info ---- */
		dp[i].d_date =	++dp[i].d_time ? 0 : 1;
		dp[i].d_size = f->f_len;
		dp[i].d_cluster = f->f_startclust;
		if (cb) {
			/*
			 * Allow the calling functions a chance to modify
			 * the directory pointer before it's written out.
			 * This is used when renaming or unlinking a file.
			 */
			_dir_entry_p ndp;

			ndp = (_dir_entry_p)kmem_alloc(fat_bpc(f->f_volidx));
			bcopy((char *)dp, (char *)ndp, fat_bpc(f->f_volidx));
			dp = ndp;
			free_dp = 1;

			(*cb)(&dp[i], arg);
		}

		DPrint(DBG_UPD,
		    ("update(inode 0x%x, name=%s, size=%ld, cluster=%d):",
		    f->f_index, dosname(dp[i].d_name, dp[i].d_ext),
		    dp[i].d_size, dp[i].d_cluster));

		/* ---- write block back out ---- */
		prom_write(_device_fd[f->f_volidx], (char *)dp,
		    fat_bpc(f->f_volidx), s, 0);
		release_cache(_device_fd[f->f_volidx]);
		if (free_dp) kmem_free((caddr_t)dp, fat_bpc(f->f_volidx));
	}
}

/*
 * []----------------------------------------------------------[]
 * | enter the directory entry in the the cache			|
 * | This code was moved from searchdir() only because of the	|
 * | indentation and SUN's requirement to have lines less the	|
 * | 80 characters. The lines were starting at about column 56	|
 * | and the code was difficult to read.			|
 * []----------------------------------------------------------[]
 */
_dir_entry_p
set_name(int volidx, int dxn, char *n, int blk0, _dir_entry_p dxp)
{
	_dir_entry_p dzp;

	/*
	 * We've allocated a buffer large enough to hold a cached directory
	 * entry. This becomes our "inode"!
	 */
	dzp = (_dir_entry_p) kmem_alloc(sizeof (_dir_entry_t));
	(void) memcpy(dzp, dxp, sizeof (_dir_entry_t));

	set_dcache(_device_fd[volidx], n, blk0, dxn);
	set_icache(_device_fd[volidx], dxn, (void *)(dzp),
	    sizeof (_dir_entry_t));

	DPrint(DBG_LOOKUP, ("set_cache(%s,%d):", n, dxn));
	return (dzp);
}

/*
 * []----------------------------------------------------------[]
 * | Search a directory:					|
 * |								|
 * | Searches the directory starting at the indicated "dir_blk"	|
 * | passing each active entry to the comparison routine at 	|
 * | "*cmp".  If the comparison routine likes the entry, we 	|
 * | return its address.  If we get to the end of the directory	|
 * |without finding an entry we like, we return a null pointer.	|
 * []----------------------------------------------------------[]
 */
static _dir_entry_p
searchdir(int volidx, u_long dir_blk,
    int (*cmp)(void *, char *, char *), void *n)
{
	int spc = pi[volidx]->f_bpb.bs_spc;
	int blk0 = (int)dir_blk;
	int rd = dir_blk == CLUSTER_ROOTDIR ? 1 : 0;
	_dir_entry_p dxp;
	int j, x, dxn, sector;

	while (cluster_valid(dir_blk, rd)) {
		/*
		 * Search the current cluster.  We read and cache the entire
		 * cluster in one fell swoop!
		 */

		sector = fat_ctodb(volidx, dir_blk, rd);

		if (!(dxp =
		    (_dir_entry_p)readblock(volidx, sector, fat_bpc(volidx)))) {
			/*
			 * Can't read directory sector.  Bail out with I/O
			 * error indication.
			 */

			return (0);
		}

		for (j = 0; j < (DIRENTS * spc); j++, dxp++) {
			/*
			 *  Check all directorie entries in this cluster ...
			 */

			if (dxp->d_name[0] == 0) {
				/*
				 * A null file name indicates the end of the
				 * active portion of the directory.
				 */

				return (0);

			} else if ((unsigned char)dxp->d_name[0] != 0xE5) {
				/*
				 * Don't bother with "erase"d entries
				 * (i.e, those whose first byte starts with
				 * 0xE5.  DOS has this crazy re-mapping rule
				 * that changes (non-erased) entries that
				 * start with 0x5 to 0xE5, but we don't
				 * support this (0xE5 isn't legal ASCII anyway,
				 * and 0x5 isn't printable).
				 */

				if ((x = (*cmp)(n, dxp->d_name, dxp->d_ext)) <=
				    0) {
					/*
					 * Found the entry we're looking for,
					 * attempt to save it in the directory
					 * cache before delivering it to our
					 * caller.
					 *
					 * NOTE: cmp routine returns zero if
					 * it wants to have the directory
					 * entry cached!
					 */
					dxn = (spc*sector*DIRENTS) + j;
					if (!x)
					    dxp = set_name(volidx, dxn, n,
						blk0, dxp);
					return (dxp);
				}
			}
		}

		/*
		 *  Find first block of next cluster (if any).
		 */

		dir_blk = fat_map(volidx, (u_short)dir_blk, rd);
	}

	return (0);
}

/*
 * dirlookupvolname
 *    Try to find a volume label in the root directory.
 *    Returns NULL if we can't find one.
 */
static char *
dirlookupvolname(int volidx)
{
	int spc = pi[volidx]->f_bpb.bs_spc;
	u_long dir_blk = CLUSTER_ROOTDIR;
	int rd = 1;
	_dir_entry_p dxp;
	int j, sector;

	DPrint(DBG_VOL, ("dirlookupvolname:%d:", volidx));
	sector = fat_ctodb(volidx, dir_blk, rd);

	if (!(dxp =
	    (_dir_entry_p)readblock(volidx, sector, fat_bpc(volidx)))) {
		/*
		 * Can't read directory sector.  Bail out with I/O
		 * error indication.
		 */
		return (0);
	}

	for (j = 0; j < (DIRENTS * spc); j++, dxp++) {
		/*
		 *  Find the first volume label
		 */
		if (dxp->d_name[0] == 0) {
			/*
			 * A null file name indicates the end of the
			 * active portion of the directory.
			 */
			return (0);
		} else if (dxp->d_attr & DE_LABEL) {
			/*
			 * A null file name indicates the end of the
			 * active portion of the directory.
			 */
			return (dxp->d_name);
		}
	}
	return (0);
}

dirNameCmp(void *v, char *name, char *ext)
{
	char *op, *fne;
	int rtn;
	char *fn = (char *)v;

	/*
	 *[]------------------------------------------------------------[]
	 * | Yuck! Special case. If current name is ".." or "." this	|
	 * | routine will not work because the following code is looking|
	 * | for "." as a seperator because the name and extension.	|
	 *[]------------------------------------------------------------[]
	 */
	if (EQ(fn, "..") && EQN(name, "..      ", NAMESIZ))
		return (0);
	if (EQ(fn, ".") && EQN(name, ".       ", NAMESIZ))
		return (0);

	if (op = (char *)strchr(fn, '.')) {
		*op = '\0';
		fne = op + 1;
	}
	else
		fne = "   ";
	if (component(fn, name, NAMESIZ) || component(fne, ext, EXTSIZ))
		rtn = 1;
	else
		rtn = 0;
	if (op) *op = '.';	/* ... restore string */
	return (rtn);
}

static int
component(char *pn, char *d, int cs)
{
	int l;

	if (namecmp(pn, d)) {
		return (1);
	}

	l = strlen(pn);
	if ((l < cs) && strncmp(&d[l], "         ", cs - l)) {
		return (1);
	}
	return (0);
}

int
namecmp(char *s, char *t)
{
	while (*s && *t)
		if (toupper(*s++) != toupper(*t++))
			return (1);
	return (0);
}

/*
 * []----------------------------------------------------------[]
 * | Look up a path name such as \Solaris\boot.rc. Need to 	|
 * | parse apart the pathname and lookup each piece.		|
 * []----------------------------------------------------------[]
 */
static int
lookuppn(char *n, _dir_entry_p dp, _file_desc_p fp)
{
	long dir_blk;
	char name[8 + 1 + 3 + 1];  /* 8 chars name, 1 for ., 3 for ext 1 null */
	char *p, *ep;
	_dir_entry_t dd;

	dir_blk = pi[fp->f_volidx]->f_dclust;
	if ((*n == '\\') || (*n == '/')) {
		/* ---- Restart the dir search at the top. ---- */
		dir_blk = CLUSTER_ROOTDIR;

		/* ---- eat directory seperators ---- */
		while ((*n == '\\') || (*n == '/'))
			n++;
		if (*n == '\0') {
			/* --- caller is opening the root! - */
			(void) memset(dp, 0, sizeof (*dp));
			dp->d_cluster = CLUSTER_ROOTDIR;
			dp->d_attr = DE_DIRECTORY;
			return (0);
		}
	}

	ep = &name[0] + sizeof (name);
	while (*n) {
		(void) memset(name, 0, sizeof (name));
		p = &name[0];
		while (*n && (*n != '\\') && (*n != '/'))
			if (p != ep)
				*p++ = *n++;
			else
				return (-1);	/* name is too long */
		if ((*n == '\\') || (*n == '/'))
			n++;
		if (lookup(name, &dd, dir_blk, fp))
			return (-1);

		/* ---- eat directory seperators ---- */
		while ((*n == '\\') || (*n == '/'))
			n++;

		/* ---- if more path dd must be a dir ---- */
		if (*n && ((dd.d_attr & DE_DIRECTORY) == 0))
			return (-1);

		/* ---- if dd is a file this next statement is a noop ---- */
		dir_blk = dd.d_cluster;
	}
	(void) memcpy(dp, &dd, sizeof (_dir_entry_t));
	return (0);
}

#ifdef DEBUG
#define	Dnbufsz (NAMESIZ+EXTSIZ+1)	/* ... +1 is for a "." */
char dn_buf[Dnbufsz+1];			/* ... +1 here is for null char */
static char *
dosname(char *n, char *e)
{
	int i;
	(void) memset(dn_buf, ' ', Dnbufsz);
	for (i = 0; i < NAMESIZ; i++) dn_buf[i] = *n++;
	for (i = NAMESIZ+1; i < Dnbufsz; i++) dn_buf[i] = *e++;
	dn_buf[NAMESIZ] = '.';
	dn_buf[Dnbufsz] = '\0';
	return (dn_buf);
}
#endif

/*
 *  Look up path name component "n":
 *
 *  If we have a directory cache entry for this component, return it
 *  imediately.   Otherwise, search the disk starting at "dir_blk" until
 *  we find the indicated entry (or we run out of directory).  Returns
 *  0 if it works (and caches the entry it found).
 */
static int
lookup(char *n, _dir_entry_p dp, u_long dir_blk, _file_desc_p fp)
{
	_dir_entry_p dxp;
	int dxn = get_dcache(_device_fd[fp->f_volidx], n, (int)dir_blk);

	if (!dxn ||
	    !(dxp = (_dir_entry_p)get_icache(_device_fd[fp->f_volidx], dxn))) {
		/*
		 * Can't find the entry we want in the directory cache.
		 * Guess we'll have to actually search the disk!
		 */

		if (!(dxp = searchdir(fp->f_volidx, dir_blk, dirNameCmp,
		    (void *)n))) {
			/*
			 * The directory search routine couldn't find the
			 * entry either! Return error indication to caller.
			 */

			return (-1);
		}
#ifdef	DEBUG
	} else {
		DPrint(DBG_LOOKUP, ("lookup(hit:%s id:%x):", n, dxn));
#endif	/* DEBUG */
	}

	fp->f_index = dxn ? dxn : get_dcache(_device_fd[fp->f_volidx],
	    n, (int)dir_blk);

	(void) memcpy(dp, dxp, sizeof (_dir_entry_t));
	return (0);
}

/*
 * Directory search info: Use to pass data from
 * .. "boot_pcfs_getdents" to its "searchdir" comparison
 * .. routine.
 */
struct dents_info {
	struct dirent *dep;
	unsigned size;
	int inum;
	int cnt;
};

/*
 *  Read DOS directory:
 *
 *  Reads directory entries from the file bound to "fd" until the "size"-
 *  byte buffer at "dep" has been exhausted or we run out of entries.
 *  Returns the number of entries read (-1 if there's an error).  The
 *  "searchdir" and "dirExtract" routines do all the real work.
 *
 *  3/12/97 -
 *  Modified to remember where it was after a previous call
 *  (not always reset to the first cluster of the directory).
 */

#define	SLOP (sizeof (struct dirent) - (int)&((struct dirent *)0)->d_name[1])

static int
boot_pcfs_getdents(int fd, struct dirent *dep, unsigned size)
{
	_file_desc_p fp;
	_dir_entry_p dxp;
	u_long	sector;
	u_long	coff, off;
	u_long	blk;
	u_int	count = 0, i;
	u_int	numents = 0;
	caddr_t	bp;
	char *name, *ext, *np;
	int namlen, extlen, len;
	int volidx;
	int rd;
	int spc;

	DPrint(DBG_GEN, ("dosGetdents:"));
	if ((fd < 0) || !(fp = get_file(fd)) || !(fp->f_attr & DE_DIRECTORY)) {
		/*
		 *  Bogus file descriptor, bail out now!
		 */
		return (-1);
	}

	if ((volidx =
	    loadVolume(fp->f_volname, NULL, NULL, fp->f_volidx)) < 0) {
		DPrint(DBG_ERR, ("Couldn't get volume %s\n", fp->f_volname));
		return (-1);
	}
	fp->f_volidx = volidx;

	spc = pi[volidx]->f_bpb.bs_spc;
	off = coff = fp->f_off;
	blk = fp->f_startclust;
	rd = blk == CLUSTER_ROOTDIR ? 1 : 0;

	while (coff >= fat_bpc(volidx)) {
		blk = fat_map(volidx, blk, rd);
		coff -= fat_bpc(volidx);

		if (!cluster_valid(blk, rd)) {
			DPrint(DBG_ERR, ("dosGetDents: Bad map entry\n"));
			return (-1);
		}
	}

	while (count < size) {
		sector = fat_ctodb(volidx, blk, rd);
		for (i = ((coff / SECSIZ) % pi[volidx]->f_bpb.bs_spc);
		    i < spc; i++) {

			if ((bp = readblock(volidx, sector + i,
			    SECSIZ)) == 0) {
				DPrint(DBG_ERR,
				    ("getdents: read failed\n"));
				fp->f_off = off;
				return (-1);
			}
			dxp = (_dir_entry_p)&bp[off % SECSIZ];

			while ((char *)dxp < ((char *)bp + SECSIZ)) {
				if (dxp->d_name[0] == 0) {
					/*
					 * A null file name indicates the
					 * end of the active portion of the
					 * directory.
					 */
					fp->f_off = off;
					return (numents);
				} else if (
				    ((unsigned char)dxp->d_name[0] == 0xE5) ||
				    (dxp->d_attr == DE_IS_LFN)) {
					/*
					 * Don't bother with "erase"d entries
					 * (i.e, those whose first byte starts
					 * with 0xE5.  DOS has this crazy
					 * re-mapping rule that changes
					 * (non-erased) entries that start
					 * with 0x5 to 0xE5, but we don't
					 * support this (0xE5 isn't legal ASCII
					 * anyway, and 0x5 isn't printable).
					 */
					coff += sizeof (*dxp);
					off += sizeof (*dxp);
					dxp++;
					continue;
				}

				/*
				 * Calculate name length.  "namlen" register
				 * holds length of the name field, "extlen"
				 * register length of the extension, and "len"
				 * the length of the concatenated pair
				 * (which includes the dot).  The total length
				 * of the output "dirent" struct is then
				 * calculated by adding the length of the
				 * fixed overhead to "len" and rounding to
				 * an appropriate alignment.
				 *
				 * NOTE: "SLOP" is the amount of space at the
				 * end of the "dirent" struct's "d_name" field
				 * that was inserted by the compiler
				 * to preserve alignment.
				 */
				for (namlen = 0;
				    (dxp->d_name[namlen] != ' ') &&
					(namlen < NAMESIZ);
				    namlen++);

				for (extlen = 0;
				    (dxp->d_ext[extlen] != ' ') &&
					(extlen < EXTSIZ);
				    extlen++);

				len = namlen + extlen + (extlen > 0);

				len = roundup((sizeof (struct dirent) +
					((len > SLOP) ? len : 0)),
				    sizeof (off_t));

				if (len > size - count) {
					/*
					 * No room at the inn.  We'll get it
					 * next time.
					 */
					fp->f_off = off;
					return (numents);
				}

				dep->d_ino = fp->f_startclust;
				dep->d_off = off;
				dep->d_reclen = (u_short)len;
				np = dep->d_name;

				name = dxp->d_name;
				while (namlen-- > 0) {
					*np++ = tolower(*name++);
				}

				ext = dxp->d_ext;
				if (extlen) {
					*np++ = '.';
					while (extlen--)
						*np++ = tolower(*ext++);
				}
				*np = '\0';

				dep = (struct dirent *)((char *)dep + len);
				numents++;
				count += len;
				coff += sizeof (*dxp);
				off += sizeof (*dxp);
				dxp++;
			}

		}

		if (count < size) {
			/* Move to next cluster ...	*/
			blk = fat_map(volidx, (u_short)blk, rd);
			if (!cluster_valid(blk, rd)) break;
		}
	}
#undef SLOP

	fp->f_off = off;
	return (numents);
}

static int
cluster_valid(long c, int rd)
{
	/* ---- cluster zero is only valid in the root dir ---- */
	return ((rd && (c == 0)) ? 1 : (c >= CLUSTER_RES_16_0 ? 0 : c));
}

static void
copyup(char *s, char *d, int cc)
{
	while (cc--) *d++ = toupper(*s++);
}

static void
dosFtruncate(_file_desc_p f)
{
	int	c;		/* ---- current cluster being freed */
	int	cn;		/* ---- next cluster in chain */

	DPrint(DBG_GEN, ("(dosFtruncate len %d, clust %d)",
	    f->f_len, f->f_startclust));
	c = f->f_startclust;
	while (cluster_valid(c, 0)) {
		cn = fat_map(f->f_volidx, c, 0);
		fat_setcluster(f->f_volidx, c, CLUSTER_AVAIL);
		c = cn;
	}
	f->f_len = 0;
	f->f_startclust = 0;
	updatedir(f, 0, 0);
}

/*
 *  reMount
 *	Attempt to accept a new pcfs volume in a floppy drive that was
 *	previously in use.
 */
static int
reMount(int volidx, char *msg)
{
	extern char *new_root_type;
	char *svrt;
	char *fname;
	int again, sv, rv = 0;

	DPrint(DBG_VOL, ("reMount:%d:", volidx));

	sv = SilentDiskFailures;
	SilentDiskFailures = 1;

	if (volidx < 0 || volidx > Pcfs_cnt) {
		DPrint(DBG_VOL,
		    ("Bogus reMount request: No such volume.\n"));
		goto giveup;
	} else if (pi[volidx] == NULL || _device_fd[volidx] < 0) {
		DPrint(DBG_VOL,
		    ("Bogus reMount request: Volume not mounted.\n"));
		goto giveup;
	} else {
		DPrint(DBG_VOL, ("Before FAT initted. "));
		DRun(DBG_VOL, splatinfo(_device_fd[volidx]));
		DRun(DBG_VOL, goany());
		if (is_floppy0(_device_fd[volidx])) {
			fname = FLOPPY0_NAME;
		} else if (is_floppy1(_device_fd[volidx])) {
			fname = FLOPPY1_NAME;
		} else {
			DPrint(DBG_VOL,
			    ("Bogus reMount request: Bad device.\n"));
			goto giveup;
		}

		DPrint(DBG_VOL,
		    ("Floppy name is %s, volidx = %d\n", fname, volidx));
		release_cache(_device_fd[volidx]);
		kmem_free((caddr_t)pi[volidx], sizeof (_fat_controller_t));

		if ((pi[volidx] = (_fat_controller_p)
		    kmem_alloc(sizeof (_fat_controller_t))) == NULL) {
			DPrint(DBG_VOL, ("reMount: Out of memory.\n"));
			goto giveup;
		}

		again = MAX_DOSMOUNT_RETRIES;
		Curvol[volidx] = NO_VOLUME;
		svrt = new_root_type;
		new_root_type = falseroot;
		do {
			prom_devreset(_device_fd[volidx]);
			if (prom_close(_device_fd[volidx]) < 0) {
				DPrint(DBG_VOL,
				    ("reMount: Bad device close.\n"));
				break;
			} else {
				if (!fat_init(volidx, fname,
				    TYPE_SOLARIS_BOOT)) {

					DPrint(DBG_VOL,
					    ("After FAT re-initted. "));
					DRun(DBG_VOL,
					    splatinfo(_device_fd[volidx]));
					DRun(DBG_VOL, goany());
					Curvol[volidx] = getvolname(volidx);
					rv = 1;
					break;
				} else {
					popup_prompt("Floppy error.", msg);
				}
			}
		} while (--again);
		new_root_type = svrt;
	}

giveup:	SilentDiskFailures = sv;
	return (rv);
}

/*
 * path2VolnPath
 *	Given a path, separate out a volume name and a path.
 *	If either the volume name or path are not wanted,
 *	(indicated by a null pointer for return storage)
 *	skip figuring those out.
 */
static int
path2VolnPath(char *path, char **vn, char **fn)
{
	char *eov;

	DPrint(DBG_VOL, ("path2VolnPath: <%s>,",
	    path && *path ? path : "Empty Path"));

	eov = (char *)strchr(path, ':');

	/*
	 *  If a volume appears to have been specified, check that it's
	 *  not completely bogus.
	 */
	if (eov && (!validvolname(path) || ((eov - path) > VOLLABELSIZE))) {
		char *c = path;
		DPrint(DBG_VOL, ("Bogus volume specification: \""));
		while (c < eov) {
			/*
			 * c must be advanced outside of of DPrint. This is
			 * a macro which can be compiled out. If so, c will
			 * never advance causing an infinite loop.
			 */

			DPrint(DBG_VOL, ("%c", *c));
			c++;
		}

		DPrint(DBG_VOL, ("\"\n"));
		return (-1);
	}

	if (vn && eov) {
		*vn = vollst(path, ((eov - path) == VOLLABELSIZE));
		DPrint(DBG_VOL, ("path2VolnPath: %s\n", *vn));
	} else if (!eov) {
		*vn = bootvolume;
	}

	if (fn && eov)
		*fn = eov + 1;
	else if (!eov)
		*fn = path;

	return (0);
}


/*
 *  loadVolume
 *	Makes certain the volume upon which 'path' is expected to live
 *	is present.  If it's not present, do whatever is necessary to make
 *	it present before returning.  Return the index into pi array for
 *	the volume when finished.
 *
 */
static int
loadVolume(char *path, char **volnm, char **newfn, int volhint)
{
	extern char *curvol;
	static int lastindex = -1;
	char *needvol, *needfn;

	DPrint(DBG_VOL, ("loadVolume:[%s]:",
	    path && *path ? path : "Empty Path"));

	if (path2VolnPath(path, &needvol, &needfn) < 0)
		return (lastindex = -1);

	DPrint(DBG_VOL, ("Need volume <%s> containing <%s>, hint <%d>.\n",
	    needvol && *needvol ? needvol : "NONE?",
	    needfn && *needfn ? needfn : "NONE?", volhint));

	/*
	 *  Fill in parsed volume and path names, if the user wanted them
	 */
	if (volnm)
		*volnm = needvol;
	if (newfn)
		*newfn = needfn;

	/*
	 *  Check the hinted at volume index first of all.
	 */
	if (volhint != 	UNKNOWN_VOLUME_IDX && volhint < Pcfs_cnt) {
		if (VOLEQ(needvol, Curvol[volhint])) {
			DPrint(DBG_VOL, ("loadVolume: "));
			DPrint(DBG_VOL, ("Matched on hint %d.\n", volhint));
			curvol = Curvol[volhint];
			return (lastindex = volhint);
		}
	}

	if (VOLEQ(needvol, U_COLON) || VOLEQ(needvol, R_COLON)) {
		return (lastindex);
	} else if (!*needvol || (bootvolume && VOLEQ(needvol, bootvolume)) ||
	    (VOLEQ(needvol, C_COLON))) {
		if (mountC(bootvolume)) {
			curvol = bootvolume;
			return (lastindex = bootpcfs);
		}
	} else {
		if (needvol == NULL || VOLEQ(needvol, A_COLON)) {
			if (mountA(ANY_VOLUME)) {
				curvol = Curvol[floppy0pcfs];
				return (lastindex = floppy0pcfs);
			}
		} else if (VOLEQ(needvol, B_COLON)) {
			if (mountB(ANY_VOLUME)) {
				curvol = Curvol[floppy1pcfs];
				return (lastindex = floppy1pcfs);
			}
		} else {
			int vc;

			for (vc = 0; vc < Pcfs_cnt; vc++) {
				if (VOLEQ(needvol, Curvol[vc])) {
					DPrint(DBG_VOL, ("loadVolume: "));
					DPrint(DBG_VOL,
					    ("Matched on pcfs %d.\n", vc));
					curvol = Curvol[vc];
					return (lastindex = vc);
				}
			}
			if (mountA(needvol)) {
				curvol = Curvol[floppy0pcfs];
				return (lastindex = floppy0pcfs);
			}
		}
	}

	/* If we haven't returned by now all our mounts failed! */
	return (lastindex = -1);
}

static int
switchVolume(int volidx, char *volname)
{
	/*
	 * Basically what we have to do here is prompt the user to
	 * insert the right floppy into the right drive, then completely
	 * remount that drive.  Repeat Ad Infinitum until the right
	 * floppy gets inserted.
	 */
	char buf[80];
	int driveno = ((volidx == floppy0pcfs) ? 0 : 1);

	DPrint(DBG_VOL, ("switchVolume: %d, %s\n", volidx, volname));

	if (!VOLEQ(volname, ANY_VOLUME)) {
		if VOLEQ(volname, bootvolume) {
			(void) sprintf(buf, "Please insert the boot floppy "
			    "(volume \"%s\") in drive %c.",
			    bootvolume, driveno + 'A');
		} else {
			(void) sprintf(buf, "Please insert floppy volume "
			    "\"%s\" in drive %c.", volname, driveno + 'A');
		}
		popup_prompt("", buf);
	}

retry:	if (reMount(volidx, buf)) {
		if ((VOLEQ(volname, ANY_VOLUME) ||
		    VOLEQ(volname, Curvol[volidx])))
			return (1);
		else {
			popup_prompt("Wrong volume inserted.", buf);
			goto retry;
		}
	} else {
		popup_prompt("", "Sorry, can't mount floppy. Giving up.");
		return (0);
	}
}

static int
mountA(char *volname)
{
	extern char *new_root_type;
	char *svrt;
	int sv, rv;

	DPrint(DBG_VOL, ("mountA: %s\n", volname ? volname : "NO VOLUME!"));

	if (floppy0pcfs == -1) {
		svrt = new_root_type;
		new_root_type = falseroot;
		if ((rv = boot_pcfs_mountroot(FLOPPY0_NAME)) < 0) {
			sv = SilentDiskFailures;
			SilentDiskFailures = 1;
			popup_prompt("", "Unable to mount floppy drive A");
			SilentDiskFailures = sv;
		}
		new_root_type = svrt;

		if (rv < 0 || (rv >= 0 && VOLEQ(volname, ANY_VOLUME)))
			return (rv >= 0);
	}

	if (floppy_status_changed(_device_fd[floppy0pcfs])) {
		reMount(floppy0pcfs, "Was checking for possible new volume.");
	}

	if (!VOLEQ(volname, ANY_VOLUME) && volname != Curvol[floppy0pcfs] &&
	    !VOLEQ(volname, Curvol[floppy0pcfs])) {
		return (switchVolume(floppy0pcfs, volname));
	}

	return (1);
}

static int
mountB(char *volname)
{
	extern char *new_root_type;
	char *svrt;
	int sv, rv;

	DPrint(DBG_VOL, ("mountB: %s\n", volname ? volname : "NO VOLUME!"));

	if (floppy1pcfs == -1) {
		svrt = new_root_type;
		new_root_type = falseroot;
		if ((rv = boot_pcfs_mountroot(FLOPPY1_NAME)) < 0) {
			sv = SilentDiskFailures;
			SilentDiskFailures = 1;
			popup_prompt("", "Unable to mount floppy drive B");
			SilentDiskFailures = sv;
		}
		new_root_type = svrt;

		if (rv < 0 || (rv >= 0 && VOLEQ(volname, ANY_VOLUME)))
			return (rv >= 0);
	}

	if (floppy_status_changed(_device_fd[floppy1pcfs])) {
		reMount(floppy1pcfs, "Was checking for possible new volume.");
	}

	if (volname != Curvol[floppy1pcfs] &&
	    !VOLEQ(volname, Curvol[floppy1pcfs])) {
		return (switchVolume(floppy1pcfs, volname));
	}

	return (1);
}

static int
mountC(char *volname)
{
	DPrint(DBG_VOL, ("mountC: %s\n", volname ? volname : "NO VOLUME!"));

	if (bootpcfs == -1) {
		prom_panic("Sorry, there is no root pcfs filesystem!");
	} else if (bootpcfs == floppy0pcfs) {
		return (mountA(volname));
	} else if (bootpcfs == floppy1pcfs) {
		return (mountB(volname));
	}
	/*
	 * We fall through to here if the root is a hard drive,
	 * in which case we know the volume hasn't changed so everything
	 * is peachy.
	 */
	return (1);
}
