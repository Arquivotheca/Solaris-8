/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)ufs.c	1.15	99/03/18 SMI\n"


/*
 * UFS.C:
 *
 *  this version of "ufs.c" has been heavily hacked:
 *  1. to overcome the model problems that exist between DOS and UNIX.
 *     References to "int" are used cavalierly in both worlds, but refer to
 *     different sized objects.
 *  2. to eliminate the extra external copy of the "iob" structure used by
 *     the functions in this file.  This needed to be done so that the
 *     resulting program would fit into a single 64K data segment.
 *
 * NOTE: This resulting source code should now run safely in both worlds!
 */

/*
 * Basic file system reading code for standalone I/O system.
 * Simulates a primitive UNIX I/O system (read(), open(), seek(), etc).
 * Does not support writes.
 */

#ifdef DEBUG
    #pragma message ( __FILE__ ": << WARNING! DEBUG MODE >>" )
    #pragma comment ( user, __FILE__ ": DEBUG ON " __TIMESTAMP__ )
#endif

#pragma comment ( compiler )
#pragma comment ( user, "ufs.c	1.15	99/03/18" )


#include <sys\types.h>
#include <sys\param.h>
#include <sys\fdisk.h>
#include <sys\vnode.h>
#include <sys\fs\ufs_fs.h>
#include <sys\fs\ufs_inod.h>
#include <sys\fs\ufs_fs.h>
#include <sys\fs\ufs_fsdi.h>
#include <sys\vtoc.h>
#include <sys\dkio.h>
#include <sys\dklabel.h>
#include <sys\saio.h>
#include <bioserv.h>          /* BIOS interface support routines */
#include <bootdefs.h>         /* primary boot environment values */
#include <dev_info.h>         /* MDB extended device information */
#include <bootp2s.h>          /* primary/secondary boot interface */
#include "iob.h"
#include "bootblk.h"

#define	NULL	0

struct dirstuff {
	long loc;
	struct iob _FAR_ *io;
};

/* These are the pools of buffers, iob's, etc. */
#define	NBUFS	(NIADDR+1)
char		b[NBUFS][MAXBSIZE];
daddr_t		blknos[NBUFS];
struct iob	iob[1];

static long chk_fdisk(struct partentry *);
static void get_ctlr(char _FAR_ *);
static void get_dev_info(struct pri_to_secboot _FAR_ *, unsigned char);
static void init_p2s_parms(struct pri_to_secboot _FAR_ *);
static struct direct _FAR_ *readdir(struct dirstuff _FAR_ *);
static ino_t dlook(char _FAR_ *, struct iob _FAR_ *);
static void read_vtoc(long);

struct pri_to_secboot p2s;

extern void c_fatal_err();

struct dk_label vtoc;

#define DskRead disk
extern long DskRead();

extern struct bios_dev boot_dev;

char *ctlr_str[] = {
		     "AHA154X",
		     "AHA174X",
		     "DPT",
		     ""
		   };


char *ctlr_id[]  = {
		     "aha",
		     "eha",
		     "dpt",
		     "ata"
		   };

void
fs_init(struct partentry *part_tab)
{
	long sector;

	/*
	 * gather device information before continuing into secondary boot
	 *
	 */

	realp = (struct pri_to_secboot _FAR_ *)&p2s;
	memset((char _FAR_ *)&p2s, '\0', sizeof (p2s));

	sector = chk_fdisk(part_tab);
	read_vtoc(sector + 1);

	init_p2s_parms(realp);
	get_dev_info(realp, boot_dev.BIOS_code);

	Dpause(DBG_DEVINFO, 0);
	Dprintf(DBG_DEVINFO, ("dev_info dump for device 0x%x\n", boot_dev.BIOS_code));
	Dprintf(DBG_DEVINFO, ("size of dev_info structure: %d\n", sizeof(DEV_INFO)));
	Dprintf(DBG_DEVINFO, ("base_port: 0x%x\n", realp->F8.base_port));
	Dprintf(DBG_DEVINFO, ("bsize/irq_level: %d\n", realp->F8.MDBdev.scsi.bsize));
	Dprintf(DBG_DEVINFO, ("targ-lun/mem_base: 0x%x\n", realp->F8.MDBdev.net.mem_base));
	Dprintf(DBG_DEVINFO, ("pdt-dtq/mem_size: 0x%x\n", realp->F8.MDBdev.net.mem_size));
	Dprintf(DBG_DEVINFO, ("index: %d\n", realp->F8.MDBdev.net.index));
	Dprintf(DBG_DEVINFO, ("dev_type: 0x%x\n", realp->F8.dev_type));
	Dprintf(DBG_DEVINFO, ("bios_dev: 0x%x\n", realp->F8.bios_dev));
	Dprintf(DBG_DEVINFO, ("hba_id: %s\n", (char _FAR_ *)realp->F8.hba_id));
	Dpause(DBG_DEVINFO, 0);

	Dprintf(DBG_FLOW, ("boot interface info initialized\n"));

	/*
	 * initialize alternate sector information, (for non-SCSI drives)
	 *
	 */
	bd_getalts ( realp->bootfrom.ufs.alts_start );

	Dprintf(DBG_FLOW, ("alt sector information initialized\n"));
}

static long
chk_fdisk(struct partentry *part)
{
	short i;
        short inactive_sol = FD_NUMPART;

	/*
	 * Search the fdisk table sequentially to find a physical partition
	 * that satisfies two criteria:
	 *    marked as "active" (bootable) partition,
	 *    id type is "SUNIXOS".
	 * If not found, settle for one that is "SUNIXOS" but not active.
	 * Allows for a reboot from some other partition.  Note that this
	 * algorithm does not fully solve the reboot problem.  Reboot from
	 * another SUNIXOS partition will not work properly.
	 */

	for (i = 0; i < FD_NUMPART; i++) {
		if (part[i].systid == SUNIXOS) {
			if (part[i].bootid == ACTIVE) {
				break;
			}
			inactive_sol = i;
		}
	}
	if (i == FD_NUMPART && (i = inactive_sol) == FD_NUMPART) {
		c_fatal_err("Cannot find SOLARIS partition.");
	}

	realp->bootfrom.ufs.Sol_start = part[i].relsect;
	realp->bootfrom.ufs.Sol_size = part[i].numsect;
	realp->bootfrom.ufs.boot_part = i + 1;

	Dprintf(DBG_FDISK, ("start of SOLARIS partition: %ld\n",
		part[i].relsect));
	Dprintf(DBG_FDISK, ("length of SOLARIS partition: %ld\n",
		part[i].numsect));
	Dprintf(DBG_FDISK, ("partition booted: %d\n",
		realp->bootfrom.ufs.boot_part));
	Dpause(DBG_FDISK, 0);
	return (part[i].relsect);
}

/*
 * We need to provide information about our hardware environment to the
 * secondary boot.  (Used for system autoconfiguration, and construction
 * of the devinfo tree.)
 *
 * There are several available sources of information.  One is the vtoc,
 * and the INT 13h, function 08h call.  Another source for extended boot
 * devices, is the new INT 13h, function F8 call.  This call is supported
 * for devices other than 80h and 81h.
 *
 */
static void
read_vtoc(long sector)
{
	unsigned short ret;

	vtoc.dkl_vtoc.v_sanity = 0x12345678L;
	Dprintf(DBG_VTOC, ("Before trying to read VTOC, sanity = %lx.\n",
		vtoc.dkl_vtoc.v_sanity));

	ret = read_sectors(&boot_dev, sector, 1, (char far *)&vtoc);

	Dprintf(DBG_VTOC, ("VTOC read %s.  Sanity marker = %lx (%ssane).\n",
		(char _FAR_ *)(ret == 1 ? "succeeded" : "failed"),
		vtoc.dkl_vtoc.v_sanity,
		(char _FAR_ *)(vtoc.dkl_vtoc.v_sanity ==
		VTOC_SANE ? "" : "in")));

	if (ret != 1 || vtoc.dkl_vtoc.v_sanity != VTOC_SANE) {
		c_fatal_err("Cannot read SOLARIS disk label.");
	}
}

/*
 * This routine fills in the "pri_to_secboot" structure, that defines the
 * interface between the primary and secondary boot phases.
 *
 * The information contained in this structure is retrieved from several
 * sources:
 *     device geometry comes from the INT 13h, function 08h call
 *     partition information comes from the fdisk table
 *     slice (filesystem) parameters are taken from the vtoc
 *     extended device information is being returned by our INT 13h,
 *     function F8 handler.
 */
static void
init_p2s_parms(struct pri_to_secboot _FAR_ *realp)
{
   register short i, j;

   realp->magic = 0x0abe;

   /* information gleaned from get_device_geometry call */
   // TODO: what should we do for LBA devices?
   realp->bootfrom.ufs.boot_dev = boot_dev.BIOS_code;
   realp->bootfrom.ufs.secPerTrk = boot_dev.u_secPerTrk;
   realp->bootfrom.ufs.trkPerCyl = boot_dev.u_trkPerCyl;
   realp->bootfrom.ufs.ncyls = boot_dev.u_nCyl;

      Dprintf(DBG_DEVINFO, ("number of cylinders: %ld\n", boot_dev.u_nCyl));
      Dprintf(DBG_DEVINFO, ((char _FAR_ *)"secPerTrk: %d\n", boot_dev.u_secPerTrk));

   /* information excavated from the vtoc */
   /* this is a hack requested by Billt.  The vtoc sometimes has incorrect
    * information in this field, and Sherman doesn't want to change the
    * driver.  So I will hardwire it here, because getting a bogus
    * sector size really screws things up.
    */
/*   realp->bootfrom.ufs.bytPerSec = vtoc.dkl_vtoc.v_sectorsz; */
   realp->bootfrom.ufs.bytPerSec = 512;

   for ( i = 0, j = vtoc.dkl_vtoc.v_nparts; i < j; i++ ) {
      switch ( vtoc.dkl_vtoc.v_part[i].p_tag ) {

		  /* NOTE: vtoc sector addresses are always relative to the
		   * start of the partition.  Therefore, we must add "relsect"
		   * to each starting block number to derive the absolute
		   * sector number in each case.
		   */
      case V_BOOT:
		  realp->bootfrom.ufs.bslice_start = vtoc.dkl_vtoc.v_part[i].p_start
						     + realp->bootfrom.ufs.Sol_start;
		  realp->bootfrom.ufs.bslice_size = vtoc.dkl_vtoc.v_part[i].p_size;

		     Dprintf(DBG_VTOC, ("vtoc boot slice address:      0x%lx\n",
		     	vtoc.dkl_vtoc.v_part[i].p_start));
		     Dprintf(DBG_VTOC, ("boot slice starting sector: 0x%lx\n",
		     	realp->bootfrom.ufs.bslice_start));
		     Dprintf(DBG_VTOC, ("boot slice partition size:  0x%lx\n",
		     	realp->bootfrom.ufs.bslice_size));
		     Dpause(DBG_VTOC, 0);
		  break;

      case V_CACHE:
      case V_ROOT:
		  realp->bootfrom.ufs.root_start = vtoc.dkl_vtoc.v_part[i].p_start
						   + realp->bootfrom.ufs.Sol_start;
		  realp->bootfrom.ufs.root_size = vtoc.dkl_vtoc.v_part[i].p_size;
		  realp->bootfrom.ufs.root_slice = i; /* slice number from vtoc */

		     Dprintf(DBG_VTOC, ("vtoc root slice address:      0x%lx\n",
		     	vtoc.dkl_vtoc.v_part[i].p_start));
		     Dprintf(DBG_VTOC, ("root slice starting sector:   0x%lx\n",
		     	realp->bootfrom.ufs.root_start));
		     Dprintf(DBG_VTOC, ("root slice partition size:    0x%lx\n",
		     	realp->bootfrom.ufs.root_size));
		     Dprintf(DBG_VTOC, ("root slice partition number:  0x%lx\n",
		     	realp->bootfrom.ufs.root_slice));
		     Dpause(DBG_VTOC, 0);
		  break;

      case V_ALTSCTR:
		  realp->bootfrom.ufs.alts_start = vtoc.dkl_vtoc.v_part[i].p_start
						   + realp->bootfrom.ufs.Sol_start;
		  realp->bootfrom.ufs.alts_size = vtoc.dkl_vtoc.v_part[i].p_size;
		  realp->bootfrom.ufs.alts_slice = i; /* slice number from vtoc */

		     Dprintf(DBG_VTOC, ("vtoc alts slice address:      0x%lx\n",
		     	vtoc.dkl_vtoc.v_part[i].p_start));
		     Dprintf(DBG_VTOC, ("alts slice starting sector:   0x%lx\n",
		     	realp->bootfrom.ufs.alts_start));
		     Dprintf(DBG_VTOC, ("alts slice partition size:    0x%lx\n",
		     	realp->bootfrom.ufs.alts_size));
		     Dprintf(DBG_VTOC, ("alts slice partition number:  0x%lx\n",
		     	realp->bootfrom.ufs.alts_slice));
		     Dpause(DBG_VTOC, 0);
		  break;

      default:
		  break;
      }
   }
}

static void
get_dev_info(struct pri_to_secboot _FAR_ *realp, unsigned char dev)
{
   char _FAR_ *tp;
   register short strsize;

   tp = (char _FAR_ *)&( realp->F8 );
   strsize = sizeof ( DEV_INFO ); /* inline asm can't do sizeof */

_asm {
    mov dl, dev
    mov ah, 0F8h
    int 13h

    cmp dx, 0BEF1h         ;check for the magic cookie!
    jne noinfo             ;cannot rely on CF - BIOS usage is inconsistent

    ; do something with the information here - dev_info structure?

    mov  ax, strsize
    push ax

    push ds
    push WORD PTR tp

    push es                ;function F8h returns pointer in ES:BX.
    push bx                ;address of bdev_info structure

    call farbcopy
    add sp, 10

    push es
    push cx

    call get_ctlr
    add sp, 4

    pop si
    pop di
    leave
    ret

noinfo:
   }

    strcat((char _FAR_ *)&(realp->F8.hba_id), (char _FAR_ *)ctlr_id[IDE_TYPE]);
}

static void
get_ctlr(char _FAR_ *id)
{
   register short i;

   for ( i = 0; i < MAX_HBA_TYPE; i++ ) {
      if ( strcmp ( id, (char _FAR_ *)ctlr_str[i] ) == 0 ) {
	 bcopy ( (char _FAR_ *)ctlr_id[i],
		 (char _FAR_ *)&(realp->F8.hba_id),
		  strlen ( (char _FAR_ *)ctlr_id[i] ) + 1 );
	 return;
      }
   }

   /*
    * If we got to this point, we may be an IDE controller.
    */
   if ( realp->F8.hba_id[0] ) {
      for ( i = 0; i < sizeof ( realp->F8.hba_id ); i++ ) {
	 if ( realp->F8.hba_id[i] == 0 )
	    break;
	 /*
	  * make sure that our string is lowercase
	  */
	 if (realp->F8.hba_id[i] >= 'A' && realp->F8.hba_id[i] <= 'Z')
	 	realp->F8.hba_id[i] += 'a' - 'A';
      }
#ifdef DEBUG
      if ( BootDbg ) {
	 putstr ( "converted hba_id string: " );
	 putstr ( (char *)realp->F8.hba_id );
	 putstr ( "\r\n" );
      }
#endif
      return;
   }
   strcat ( (char _FAR_ *)&(realp->F8.hba_id),
	    (char _FAR_ *)ctlr_id[DEFAULT_HBA_TYPE] );
}

static
openi ( ino_t n, register struct iob _FAR_ *io )
{
	register struct dinode _FAR_ *dp;
   daddr_t adj_blk;
   ulong nsect;
  	long oo;

	io->i_offset = 0;
	io->i_bn = fsbtodb(&io->iob_fs, itod(&io->iob_fs, n));
	io->i_cc = io->iob_fs.fs_bsize;
	io->i_ma = io->i_buf;
	adj_blk = io->i_cyloff + io->i_bn;
	nsect = ( (ulong)(io->i_cc) / realp->bootfrom.ufs.bytPerSec );

	if ( DskRead ( adj_blk, (char _FAR_ *)io->i_buf, nsect ) != io->iob_fs.fs_bsize )
		return (0);
	dp = (struct dinode _FAR_ *)io->i_buf;
#if 0	/* simplify the macros */
	io->i_ino.i_ic = dp[itoo(&io->iob_fs, n)].di_ic;
#else
	oo = itoo(&io->iob_fs, n);
	io->i_ino.i_ic = dp[ oo ].di_ic;
#endif
	return (1);
}

static ino_t
find(register char _FAR_ *path, struct iob _FAR_ *file)
{
	register char _FAR_ *q;
	char c;
	ino_t n;

	if (path==NULL || *path=='\0')
		return(0);
	if (openi((ino_t) UFSROOTINO, file) == 0)
		return (0);
	while (*path) {
		while (*path == '/')
			path++;
		q = path;
		while (*q != '/' && *q != '\0')
			q++;
		c = *q;
		*q = '\0';

		if ((n = dlook(path, file))!=0) {
			if (c == '\0')
				break;
			if (openi(n, file) == 0)
				return (0);
			*q = c;
			path = q;
			continue;
		} else {
			return(0);
		}
	}
	return(n);
}


static daddr_t
sbmap ( register struct iob _FAR_ *io, daddr_t bn )
{
	register struct inode _FAR_ *ip;
   daddr_t adj_blk;
   ulong nsect;
	register daddr_t nb, _FAR_ *bap;
	register daddr_t _FAR_ *db;

	ip = &io->i_ino;
	db = ip->i_db;

	/*
	 * blocks 0..NDADDR are direct blocks
	 */
	if(bn < NDADDR) {
		nb = db[bn];
		return(nb);
	}

	/*
	 * addresses NIADDR have single and double indirect blocks.
	 */
	bn -= NDADDR;
	/*
	 * The realmode multiplication and division routines
	 * are not working. Apparently 1 x 0x800 is 1!
	 * which caused bug when reading indirect block.
	 * Simplify code to handle up to 1st level indirect blocks
	 * This is not much of a restriction as we have only just
	 * moved into 1st level indirect blocks.
	 * Current sizes:-
	 *		ufsboot: 		  151040 -   19 blocks
	 *		cfsboot: 		  171008 -   21 blocks
	 *		/kernel/unix:	  719490 -   87 blocks
	 *		Limit:			16875520 - 2060 blocks
	 */
	if (bn > NINDIR(&io->iob_fs)) {
		c_fatal_err("Image size too large.");
	}
	/*
	 * fetch the first indirect block address from the inode
	 */
	nb = ip->i_ib[0];
	if (nb == 0) {
		return((daddr_t)0);
	}

	/*
	 * fetch through the indirect block
	 */
	if (blknos[NIADDR] != nb) { /* in indirect buf cache? */
		io->i_bn = fsbtodb(&io->iob_fs, nb);
		io->i_ma = b[NIADDR];
		io->i_cc = io->iob_fs.fs_bsize;
		adj_blk = io->i_cyloff + io->i_bn;
		nsect = ((ulong)(io->i_cc) / realp->bootfrom.ufs.bytPerSec);

		if ( DskRead ( adj_blk, (char _FAR_ *)io->i_ma, nsect )
		    != io->i_cc )
			return((daddr_t)0);
		blknos[NIADDR] = nb;
	}
	bap = (daddr_t _FAR_ *)b[NIADDR];
	nb = bap[bn];
	return(nb);
}


static ino_t
dlook ( char _FAR_ *s, register struct iob _FAR_ *io )
{
	register struct direct _FAR_ *dp;
	register struct inode _FAR_ *ip;
	struct dirstuff dirp;
	register long len;

	ip = &io->i_ino;
	if (s == NULL || *s == '\0')
		return(0);
	if ((ip->i_smode&IFMT) != IFDIR) {
		return(0);
	}
	if (ip->i_size == 0) {
		return(0);
	}
	len = strlen(s);
	dirp.loc = 0;
	dirp.io = io;
	for (dp = readdir(&dirp); dp != NULL; dp = readdir(&dirp)) {
		if(dp->d_ino == 0)
			continue;
		if (dp->d_namlen == len && !(short)strcmp(s, dp->d_name))
			return(dp->d_ino);
	}
	return(0);
}


/*
 * get next entry in a directory.
 */
static struct direct _FAR_ *
readdir ( register struct dirstuff _FAR_ *dirp )
{
	register struct direct _FAR_ *dp;
	register struct iob _FAR_ *io;
	register daddr_t lbn, d;
   daddr_t adj_blk;
   ulong nsect;
	register long off;

	io = dirp->io;
	for(;;) {
		if (dirp->loc >= io->i_ino.i_size){
			return NULL;
		}
		off = blkoff(&io->iob_fs, dirp->loc);
		if (off == 0) {
			lbn = lblkno(&io->iob_fs, dirp->loc);
			d = sbmap(io, lbn);
			if(d == 0){
				return NULL;
			}
			io->i_bn = fsbtodb(&io->iob_fs, d);
			io->i_ma = io->i_buf;
			io->i_cc = blksize(&io->iob_fs, &io->i_ino, lbn);
         adj_blk = io->i_cyloff + io->i_bn;
         		nsect = ((ulong)(io->i_cc) / realp->bootfrom.ufs.bytPerSec);

			if ( DskRead ( adj_blk, (char _FAR_ *)io->i_ma, nsect ) != io->i_cc )
				return NULL;
		}
		dp = (struct direct _FAR_ *)(io->i_buf + off);
		dirp->loc += dp->d_reclen;
		if (dp->d_ino == 0)
			continue;
		return (dp);
	}
}


static long
getblock ( register struct iob _FAR_ *io )
{
	register struct fs _FAR_ *fs;
	register long off, size, diff;
   daddr_t adj_blk;
   ulong nsect;
	register daddr_t lbn;

	diff = io->i_ino.i_size - io->i_offset;
	if (diff <= 0)
		return (-1);
	fs = &io->iob_fs;
	lbn = lblkno(fs, io->i_offset);
	io->i_bn = fsbtodb(fs, sbmap(io, lbn));
	off = blkoff(fs, io->i_offset);
	size = blksize(fs, &io->i_ino, lbn);
	io->i_ma = io->i_buf;
	io->i_cc = size;
   adj_blk = io->i_cyloff + io->i_bn;
	nsect = ((ulong)(io->i_cc) / realp->bootfrom.ufs.bytPerSec);

	if ( DskRead ( adj_blk, (char _FAR_ *)io->i_ma, nsect ) != io->i_cc ) {
		Dprintf(DBG_READ, ("getblock failed.\r\n"));
		return(-1);
	}
	if (io->i_offset - off + size >= io->i_ino.i_size)
		io->i_cc = diff + off;
	io->i_cc -= off;

	io->i_ma = &io->i_buf[off];
	return(0);
}


long
fs_read(long fdesc, register char far *buf, long count)
{
	register long i,j;
	register struct iob _FAR_ *file;

	file = &iob[fdesc];
	if (file->i_offset + count > file->i_ino.i_size)
		count = file->i_ino.i_size - file->i_offset;
	if ((i = count) <= 0)
		return(0);
	while (i > 0) {
		if (file->i_cc <= 0) {
			if (getblock(file) == -1) {
				Dprintf(DBG_READ, ("read failed.\r\n"));
				return (0);
			}
		}
		j = (i < file->i_cc) ? i : file->i_cc;
		farbcopy(file->i_ma, buf, (u_int)j); /* u_int's same as unsigned long */
		buf += j;
		file->i_ma += j;
		file->i_offset += j;
		file->i_cc -= j;
		i -= j;
	}

      Dprintf(DBG_READ, ((char _FAR_ *)"ufs: read returned %d\n", count));

	return(count);
}


/*
 * Open a file. For the bootblock, we assume one file can be opened
 * on a ufs filesystem. The underlying device is the one we rode in on.
 */
long
fs_open(char *str)
{
	register struct iob _FAR_ *file;
   register struct pri_to_secboot _FAR_ *realp;
   daddr_t adj_blk;
   ulong nsect;
	ino_t ino;

	file = iob;
   realp = ( struct pri_to_secboot _FAR_ * )&p2s;

/* hard-wire these for now, until we can get INT 13, function 08h to work. */
	file->i_boottab = 0L;
	file->i_ctlr = TEST_CTLR;
	file->i_unit = TEST_UNIT;

	file->i_ino.i_dev = realp->bootfrom.ufs.boot_dev;
	file->i_boff = realp->bootfrom.ufs.root_slice;	/* Solaris disk slice# */
  	file->i_cyloff = realp->bootfrom.ufs.root_start;/* start of ufs root fs */

	/* Opening a file system; read the superblock. */
	file->i_ma = (char _FAR_ *)(&file->iob_fs);
	file->i_cc = SBSIZE;
	file->i_bn = SBLOCK;
	file->i_offset = 0;

   adj_blk = file->i_cyloff + file->i_bn;
	nsect = ((ulong)(file->i_cc) / realp->bootfrom.ufs.bytPerSec);

	if ( DskRead ( adj_blk, (char _FAR_ *)file->i_ma, nsect ) != SBSIZE ) {
		Dprintf(DBG_READ, ("open failed.\r\n"));
		return(-1);
	}

	if (file->iob_fs.fs_magic != FS_MAGIC) {
		static char msg[] = "Not a UFS file system.";
		prtstr_attr ( (char _FAR_ *)msg, (short) strlen ( msg ), 0, 23, 0,
                  ERROR_ATTR );

		while ( 1 )          /* don't even bother returning if this fails! */
         ;
	}
	if ((ino = find(str, file)) == 0)
		return(-1);

	if (openi(ino, file) == 0)
		return (-1);
	file->i_offset = 0;
	file->i_cc = 0;

	Dprintf(DBG_FLOW, ((char _FAR_ *)"ufs: open of %s succeeded\n",
		(char _FAR_ *)str));

	return(0);
}


long
fs_close(long fd)
{
	register struct iob _FAR_ *file;

	file = &iob[fd];
	return ( 0 );
}


off_t
fs_seek(long fdesc, register off_t addr, int whence)
{
	register struct iob _FAR_ *io;
	off_t new_offset;

	io = &iob[fdesc];
	switch (whence) {
	case 0:
		new_offset = addr;
		break;
	case 1:
		new_offset = io->i_offset + addr;
		break;
	case 2:
		new_offset = io->i_ino.i_size + addr;
		break;
	default:
		return (-1);
	}
	if (new_offset < 0) {
		new_offset = -1;
	}
	else {
		io->i_offset = new_offset;
		io->i_bn = new_offset >> DEV_BSHIFT;
		io->i_cc = 0;
	}
	return (new_offset);
}
