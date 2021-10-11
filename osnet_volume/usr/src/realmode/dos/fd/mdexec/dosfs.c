/*
 * Copyright (c) 1995 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)dosfs.c	1.13	95/08/23 SMI\n"

/*
 * DOSFS.C:
 *
 *  this version of "dosfs.c" has been heavily hacked:
 *  1. to overcome the model problems that exist between DOS and UNIX.
 *     References to "int" are used cavalierly in both worlds, but refer to
 *     different sized objects.
 *
 * NOTE: This resulting source code should now run safely in both worlds!
 *
 */

/*
 * Very basic file system for reading code in standalone I/O system.
 * Does not support writes.
 */

#ifdef FARDATA
#define	_FAR_ _far
#else
#define	_FAR_
#endif

extern MDXdebug;		/* global debug output switch */

#ifdef DEBUG
#pragma message(__FILE__ ": << WARNING! DEBUG MODE >>")
#pragma comment(user, __FILE__ ": DEBUG ON " __TIMESTAMP__)
#endif

#include <sys\types.h>
#include <sys\param.h>
#include <bioserv.h>	  /* BIOS interface support routines */
#include <bootdefs.h>	 /* primary boot environment values */
#include <dev_info.h>	 /* MDB extended device information */
#include <bootp2s.h>	  /* primary/secondary boot interface */
#include <bpb.h>		/* BIOS parameter block */

extern void displ_err();
extern char _far *mov_ptr(char _far *, long);
extern long devread(daddr_t, char _far *, long);

#define	NULL	0
#define	PC_SECSIZE	512

typedef	unsigned short	cluster_t;
typedef union {
	struct {
		u_short offp;
		u_short segp;
	} s;
	char _far *p;
	u_long l;
} seg_ptr;

struct pcnode {
	int pc_flags;			/* flags */
	DOSdirent pc_entry;		/* directory entry of file */
};

struct pcfs {
	int pcfs_flags;			/* flags */
	int pcfs_ldrv;			/* logical DOS drive number */
	int pcfs_secsize;		/* sector size in bytes */
	int pcfs_spcl;			/* sectors per cluster */
	int pcfs_spt;			/* sectors per track */
	int pcfs_fatsec;		/* number of sec per FAT */
	int pcfs_numfat;		/* number of FAT copies */
	int pcfs_rdirsec;		/* number of sec in root dir */
	daddr_t pcfs_fatstart;		/* start blkno of first FAT */
	daddr_t pcfs_rdirstart;		/* start blkno of root dir */
	daddr_t pcfs_datastart;		/* start blkno of data area */
	int pcfs_clsize;		/* cluster size in bytes */
	cluster_t pcfs_ncluster;	/* number of clusters in fs */
	int pcfs_entps;			/* number of dir entry per sector */
	u_char  *pcfs_fatp;		/* ptr to FAT data */
};

/*
 * special cluster numbers in FAT
 */
#define	PCF_FREECLUSTER		0x00	/* cluster is available */
#define	PCF_ERRORCLUSTER	0x01	/* error occurred allocating cluster */
#define	PCF_12BCLUSTER		0xFF0	/* 12-bit version of reserved cluster */
#define	PCF_RESCLUSTER		0xFFF0	/* 16-bit version of reserved cluster */
#define	PCF_BADCLUSTER		0xFFF7	/* bad cluster, do not use */
#define	PCF_LASTCLUSTER		0xFFF8	/* >= means last cluster in file */
#define	PCF_FIRSTCLUSTER	2	/* first valid cluster number */

#define	PCFS_FAT16	0x400		/* 16 bit FAT */
#define	PC_MAXFATSEC	256		/* maximum number of sectors in FAT */

#define	DOS_ID1		0xe9	/* JMP intrasegment */
#define	DOS_ID2a	0xeb	/* JMP short */
#define	DOS_ID2b	0x90
#define	DOS_SIGN	0xaa55	/* DOS signature in boot and partition */

/*
 * Media descriptor byte.
 * Found in the boot block and in the first byte of the FAT.
 * Second and third byte in the FAT must be 0xFF.
 * Note that all technical sources indicate that this means of
 * identification is extremely unreliable.
 */
#define	MD_FIXED	0xF8	/* fixed disk */
#define	SS8SPT		0xFE	/* single sided 8 sectors per track */
#define	DS8SPT		0xFF	/* double sided 8 sectors per track */
#define	SS9SPT		0xFC	/* single sided 9 sectors per track */
#define	DS9SPT		0xFD	/* double sided 9 sectors per track */
#define	DS18SPT		0xF0	/* double sided 18 sectors per track */
#define	DS9_15SPT	0xF9	/* double sided 9/15 sectors per track */

#define	howmany(a, b) (((a) + (b) - 1) / (b))
#define	pc_validcl(CL)	    /* check that cluster number is legit */ \
	((CL) >= PCF_FIRSTCLUSTER && \
	    (CL) < dosfs.pcfs_ncluster + PCF_FIRSTCLUSTER)

char DirRErr[] = "Error reading directory block.";
short DirRErrSiz = sizeof (DirRErr);
char DosMErr[] = "Invalid DOS medium.";
short DosMErrSiz = sizeof (DosMErr);
char ReadFErr[] = "Error reading diskette.";
short ReadFErrSiz = sizeof (ReadFErr);

struct bios_param_blk bpb;	/* diskette BIOS parameter block */

struct pcfs dosfs;		/* for DOS filesystem on diskette */

struct pcnode pcn[1];		/* we only process one file */

u_char *xferbuf;		/* ptr to sector-aligned sector buffer */

u_char *rootdbuf;		/* ptr to sector-aligned root dir buffer */
daddr_t rootdirblk;

#define	MAXFATCOUNT	9*PC_SECSIZE	/* enough for 2.88MB@1 sec/cluster */

struct {
	u_char sectorbuf[1023];		/* unaligned sector buffers */
	u_char rootdirbuf[512];
	u_char fatbuffer[MAXFATCOUNT];
} ua;


int
dosfs_init()
{
	struct bios_param_blk *bootp = &bpb;
	struct pcfs *fsp = &dosfs;
	seg_ptr xferadr;
	seg_ptr longadr;
	long	blknum;
	u_int	fatsize;
	int	error;
	int	nsect;
	int	secsize;
	u_char	*fatp;

	rootdirblk = -1;
	longadr.p = xferadr.p =  (char _far *) ua.sectorbuf;
	longadr.l = ((u_long)longadr.s.segp << 4) + longadr.s.offp;
	xferbuf = (longadr.s.offp & (PC_SECSIZE - 1) ?
	    mov_ptr(xferadr.p, (~longadr.s.offp & (PC_SECSIZE - 1)) + 1L) :
	    xferadr.p);
	rootdbuf = (u_char *) mov_ptr(xferbuf, PC_SECSIZE);
	bpb.VBytesPerSector = 512;
	bpb.VNumberOfHeads = 2;
	bpb.VSectorsPerTrack = 9;

	/* read the diskette superblock (BIOS Parameter Block) */
	if (devread(0, xferbuf, 1L) != PC_SECSIZE) {
		_asm {
		mov	di, offset DosMErr
		mov	si, DosMErrSiz
		call	displ_err
		jmp	$
		}
	}
	if (!(*xferbuf == DOS_ID1 ||
	    (*xferbuf == DOS_ID2a && xferbuf[2] == DOS_ID2b))) {
#ifdef DEBUG
		printf("Bad DOS signature\n");
#endif
		_asm {
		mov	di, offset DosMErr
		mov	si, DosMErrSiz
		call	displ_err
		jmp	$
		}
	}

	bcopy((char *)&xferbuf[3], (char *) bootp,
	    sizeof (struct bios_param_blk));

	/* get the sector size - may be more than 512 bytes */
	secsize = (int)bootp->VBytesPerSector;
	/*
	 * check for bogus sector size
	 *  - fat should be at least 1 sector
	 */
	if (secsize < 512 || (int)bootp->VSectorsPerFAT < 1 ||
	    bootp->VNumberOfFATs < 1) {
#ifdef DEBUG
		printf("Bad BPB values\n");
#endif
		_asm {
		mov	di, offset DosMErr
		mov	si, DosMErrSiz
		call	displ_err
		jmp	$
		}
	}

	switch (bootp->VMediaDescriptor) {
	default:
	case MD_FIXED:
#ifdef DEBUG
		printf("dosfs: Invalid media descriptor\n");
#endif
		_asm {
		mov	di, offset DosMErr
		mov	si, DosMErrSiz
		call	displ_err
		jmp	$
		}

	case SS8SPT:
	case DS8SPT:
	case SS9SPT:
	case DS9SPT:
	case DS18SPT:
	case DS9_15SPT:
		/*
		 * all floppy media are assumed to have 12-bit FATs
		 * and a boot block at sector 0
		 */
		fsp->pcfs_secsize = secsize;
		fsp->pcfs_entps = secsize / sizeof (DOSdirent);
		fsp->pcfs_spcl = (int)bootp->VSectorsPerCluster;
		fsp->pcfs_fatsec = (int)bootp->VSectorsPerFAT;
		fsp->pcfs_spt = (int)bootp->VSectorsPerTrack;
		fsp->pcfs_rdirsec = (int)bootp->VRootDirEntries
		    * sizeof (DOSdirent) / secsize;
		fsp->pcfs_clsize = fsp->pcfs_spcl * secsize;
		fsp->pcfs_fatstart = (daddr_t)bootp->VReservedSectors;
		fsp->pcfs_rdirstart = fsp->pcfs_fatstart +
		    (bootp->VNumberOfFATs * fsp->pcfs_fatsec);
		fsp->pcfs_datastart = fsp->pcfs_rdirstart + fsp->pcfs_rdirsec;
		fsp->pcfs_ncluster = ((long)(bootp->VTotalSectors ?
		    bootp->VTotalSectors : bootp->VTotalSectorsBig) -
		    fsp->pcfs_datastart) / fsp->pcfs_spcl;
		fsp->pcfs_numfat = (int)bootp->VNumberOfFATs;
		break;
	}

	/*
	 * Get FAT and check it for validity
	 */
	fatsize = fsp->pcfs_fatsec * fsp->pcfs_secsize;
	if (fatsize > MAXFATCOUNT) {
#ifdef DEBUG
		printf("dosfs: unusual FAT size %d bytes\n", fatsize);
#endif
		_asm {
		mov	di, offset DosMErr
		mov	si, DosMErrSiz
		call	displ_err
		jmp	$
		}
	}
	fatp = (u_char *) mov_ptr(xferbuf, 2 * PC_SECSIZE);

	if (devread((daddr_t) fsp->pcfs_fatstart, fatp,
	    (long) fsp->pcfs_fatsec) != fatsize) {
		_asm {
		mov	di, offset DosMErr
		mov	si, DosMErrSiz
		call	displ_err
		jmp	$
		}
	}
	if (*fatp != bootp->VMediaDescriptor ||
	    fatp[1] != 0xFF || fatp[2] != 0xFF) {
#ifdef DEBUG
		printf("dosfs: Bad FAT\n");
#endif
		_asm {
		mov	di, offset DosMErr
		mov	si, DosMErrSiz
		call	displ_err
		jmp	$
		}
	}
	fsp->pcfs_fatp = fatp;
	return (0);
}

/*
 * Cluster manipulation routines.
 */

/*
 * Get the next cluster in the file cluster chain.
 *	cn = current cluster number in chain
 */
static cluster_t
pc_getcluster(struct pcfs *fsp, register cluster_t cn)
{
	register u_char *fp;

	if (!pc_validcl(cn)) {
#ifdef DEBUG
		printf("dosfs: invalid cluster %d\n", cn);
#endif
		_asm {
		mov	di, offset DosMErr
		mov	si, DosMErrSiz
		call	displ_err
		jmp	$
		}
	}

	fp = fsp->pcfs_fatp + (cn + (cn >> 1));
	if (cn & 01) {
		cn = (((u_int)*fp++ & 0xf0) >> 4);
		cn += (*fp << 4);
	} else {
		cn = *fp++;
		cn += ((*fp & 0x0f) << 8);
	}
	if (cn >= PCF_12BCLUSTER)
		cn |= PCF_RESCLUSTER;
	return (cn);
}


/*
 * Convert file logical block (cluster) numbers to disk block numbers.
 * Also return number of physically contiguous blocks if asked for.
 */
static int
pc_bmap(DOSdirent *dep, cluster_t lcn, daddr_t *dbnp, long *contigbp)
{
	register struct pcfs *fsp = &dosfs;
	cluster_t lcnum = lcn;
	cluster_t cn, ncn;		/* current, next cluster number */

	if (lcn >= fsp->pcfs_ncluster) {
#ifdef DEBUG
		printf("pc_bmap: WARNING bad cluster %d\n", lcn);
#endif
		return (-1);
	}

	if (!(dep->attr & _A_SUBDIR) &&
	    (lcn > howmany(dep->size, fsp->pcfs_clsize) - 1)) {
#ifdef DEBUG
		printf("pc_bmap: WARNING bad cluster lcn=%d\n", lcn);
#endif
		return (-1);
	}
	ncn = dep->start;
	do {
		cn = ncn;
		if (!pc_validcl(cn)) {
#ifdef DEBUG
			printf("pc_bmap: WARNING bad cluster chain cn=%d\n",
			    cn);
#endif
			return (-1);

		}
		ncn = pc_getcluster(fsp, cn);
	} while (lcnum--);

	*dbnp = fsp->pcfs_datastart +
	    (long)(cn - PCF_FIRSTCLUSTER) * fsp->pcfs_spcl;

	if (contigbp) {
		u_long count;

		count = fsp->pcfs_clsize;
		while ((cn + 1) == ncn && count < *contigbp &&
		    pc_validcl(ncn)) {
			count += fsp->pcfs_clsize;
			cn = ncn;
			ncn = pc_getcluster(fsp, ncn);
		}
		*contigbp = count;
	}
#ifdef DEBUG
	if (MDXdebug)
	printf("dosfs: lblock %d => sector %ld for %ld\n",
	    lcn, *dbnp, *contigbp);
#endif
	return (0);
}


/*
 *	return the nth root directory entry
 */
int
get_rdir(int nth, DOSdirent *dp)
{
	register struct pcfs *fsp = &dosfs;
	DOSdirent *pdir;
	daddr_t blknum;
	long offset;
	long rdsize;
	u_short cfbyte;
	char fname[12];


	if (nth < 0)
		return (0);
	rdsize = fsp->pcfs_rdirsec * fsp->pcfs_secsize;

	for (offset = nth * sizeof (DOSdirent),
	    pdir = (DOSdirent *)(rootdbuf + (offset & (fsp->pcfs_secsize - 1)));
	    ++nth && offset < rdsize;
	    offset += sizeof (DOSdirent), pdir++) {

		blknum = fsp->pcfs_rdirstart + offset / fsp->pcfs_secsize;
		if (blknum != rootdirblk) {
			if (!devread(blknum, rootdbuf, 1L)) {
				_asm {
					mov	di, offset DirRErr
					mov	si, DirRErrSiz
					call	displ_err
				}
				return (-1);
			}
			rootdirblk = blknum;
			pdir = (DOSdirent *)
			    (rootdbuf + (offset & (fsp->pcfs_secsize - 1)));
		}

		cfbyte = (u_short) pdir->fname[0];
		if (cfbyte == '\0')
			/* end of directory */
			break;
		if (cfbyte == ERASED_FILE ||
		    (pdir->attr & (_A_VOLID | _A_SUBDIR)))
			/* skip over erased files, vol label, and dirs */
			continue;

		/* found something! */
		*dp = *pdir;
#ifdef DEBUG
		if (MDXdebug) {
		bcopy(dp->fname, fname, 11);
		fname[11] = '\0';
		printf("dos_get_rdir: \'%s\'  start=%d  size=%ld\n",
		    fname, pdir->start, pdir->size);
		}
#endif
		return (nth);
	}
	return (0);
}


/*
 *	search the root directory for a filename
 */
static int
rdlook(char *s, struct pcnode *pcp)
{
	register struct pcfs *fsp = &dosfs;
	DOSdirent *pdir;
	daddr_t blknum;
	long offset;
	long rdsize;
	u_short cfbyte;

#ifdef DEBUG
	if (MDXdebug)
	printf("dosfs: looking for %s\n", s);
#endif
	blknum = fsp->pcfs_rdirstart;
	rdsize = fsp->pcfs_rdirsec * fsp->pcfs_secsize;
	pdir = (DOSdirent *) rootdbuf;

	for (offset = 0; offset < rdsize;
	    pdir++, offset += sizeof (DOSdirent)) {

		if (!(offset & (fsp->pcfs_secsize - 1)) &&
		    blknum != rootdirblk) {
			if (!devread(blknum, rootdbuf, 1L)) {
				_asm {
					mov	di, offset DirRErr
					mov	si, DirRErrSiz
					call	displ_err
				}
				return (-1);
			}
			rootdirblk = blknum++;
			pdir = (DOSdirent *) rootdbuf;
		}

		cfbyte = (u_short)pdir->fname[0];
		if (cfbyte == '\0')
			/* end of directory */
			break;
		if (cfbyte == ERASED_FILE ||
		    (pdir->attr & (_A_VOLID | _A_SUBDIR)))
			/* skip over erased files, vol label, and dirs */
			continue;

		if (strncmp(pdir->fname, s,
		    sizeof (pdir->fname) + sizeof (pdir->ext)) == 0) {
			/* found it! */
			pcp->pc_entry = *pdir;
#ifdef DEBUG
			if (MDXdebug) {
			printf("dos_open: of %s.%s\n",
			    pcp->pc_entry.fname, pcp->pc_entry.ext);
			printf("dos_open: start=%d  size=%ld\n",
			    pcp->pc_entry.start, pcp->pc_entry.size);
			if (MDXdebug & 0x8000) {
				putstr("Press any key to continue ...");
				wait_key();
			}
			putstr("\r\n");
			}
#endif
			return (0);
		}
	}
	return (-1);
}

/*
 *	file to be opened must be in root directory, not in a subdirectory.
 */
long
open(char *str)
{
	long fd = 0;

	if (rdlook(str, &pcn[fd]) < 0) {
#ifdef DEBUG
		if (MDXdebug)
		printf("dosfs: open of %s failed\n", str);
#endif
		return (-1);
	}

	pcn[fd].pc_flags = 0;

#ifdef DEBUG
	if (MDXdebug)
	printf("dosfs: open of %s succeeded\n", str);
#endif

	return (fd);
}


/*
 *	Return the size of an opened MS-DOS file.
 */
long
filesize(long fdesc)
{
	return (pcn[fdesc].pc_entry.size);
}


/*
 *	Direct read into memory an entire MS-DOS file.
 *	Optimized to read contiguous clusters in one disk request.
 */
int
readfile(long fdesc, char _far *buf)
{
	register struct pcfs *fsp = &dosfs;
	DOSdirent *pdir;
	cluster_t lcn = 0;
	daddr_t dbn;
	long contigb;
	long count;
	long nsect;

	pdir = &pcn[fdesc].pc_entry;
	count = howmany(pdir->size, fsp->pcfs_clsize);

#ifdef DEBUG
	if (MDXdebug) {
	printf("dosfs: file size = %ld clusters to %08lx\n",
	    count, (char _far *) buf);
	}
#endif

	while (count > 0) {
		/* tell pc_bmap() we have room for the whole remainder */
		contigb = count * fsp->pcfs_clsize;
		if (pc_bmap(pdir, lcn, &dbn, &contigb)) {
#ifdef DEBUG
			printf("dosfs: cluster conversion failed %d\n", lcn);
#endif
			_asm {
				mov	di, offset DosMErr
				mov	si, DosMErrSiz
				call	displ_err
			}
			return (-1);
		}
		nsect = contigb / fsp->pcfs_secsize;

		if (devread(dbn, buf, nsect) != contigb) {
#ifdef DEBUG
			printf("dosfs: diskette read error on sector %d\n",
			    dbn);
#endif
			_asm {
				mov	di, offset ReadFErr
				mov	si, ReadFErrSiz
				call	displ_err
			}
			return (-1);
		}
		buf = mov_ptr(buf, contigb);
		lcn += (contigb / fsp->pcfs_clsize);
		count -= (contigb / fsp->pcfs_clsize);
	}
	return (0);
}


long
close(long fd)
{
	return (0);
}
