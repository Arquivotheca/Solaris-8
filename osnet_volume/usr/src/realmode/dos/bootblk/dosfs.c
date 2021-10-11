/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)dosfs.c	1.15	99/02/11 SMI\n"

#include <bioserv.h>          /* BIOS interface support routines */
#include "bootblk.h"
#include "disk.h"

/*
 *[]------------------------------------------------------------[]
 * | Global defines for this file.				|
 *[]------------------------------------------------------------[]
 */
fat_cntlr_t		Fatc = {0};
static u_char *		Fat_sector_p = 0;
static u_short		Fat_sector_size = 0;
static u_long		Fat_blk_start = -1;
static u_long		Fat_blk_end = -1;

void
fs_init(struct partentry *part_tab)
{
	/*
	 * If we cannot find a Solaris boot partition, then
	 * use the active DOS which must be the partition
	 * we were loaded from.
	 */
	if (init_fat(TYPE_SOLARIS_BOOT))
		(void)init_fat(TYPE_DOS);
}

/*
 *[]------------------------------------------------------------[]
 * | init_fat -- initialize the fat controller structure. On 	|
 * | hard disks we need to search for the correct type of 	|
 * | partition.							|
 *[]------------------------------------------------------------[]
 */
int
init_fat(int type)
{
	_fdisk_t fd;			/* ---- used to look at fdisk */
        long adjust;			/* ---- partition start at blk */
        int i, rtn;

	Dprintf(DBG_FLOW, ("init_fat: type %x.\n", type));

        /* ---- Initialize the underlying bios support ---- */
        init_bios();

	/*
	 * allow the calling routine to determine if error has occured
	 * or not. See comments above Fdisk_fat() which talks about return
	 * codes. If rtn is non-zero don't allocate fat table space
	 */
	if (Fdisk_fat(type, &fd)) {
		Dprintf(DBG_FLOW, ("init_fat: Fdisk_fat failed.\n"));
		return 1;
	}

	adjust = fd.fd_start_sec;

        if (ReadSect_bios((char *)&Fatc.f_sector[0], adjust, 1)) {
                c_fatal_err("Cannot read BPB!");
		return 1;
        }

	/*
	 * Check to see if this is a valid BPB. We should see a jump
	 * instruction at the beginning of the sector.
	 */
	if ((Fatc.f_bpb.bs_jump_code[0] != 0xe9) &&
	    ((Fatc.f_bpb.bs_jump_code[0] != 0xeb) &&
	     (Fatc.f_bpb.bs_jump_code[2] != 0x90))) {
	     	c_fatal_err("Invalid BPB.  No filesystem support.");
	     	return 1;
	}

	/*
	 * Update the bios structure to reflect what has been found
	 * on the floppy or hard disk. This should only be different
	 * for floppys.
	 */
	boot_dev.u_secPerTrk = Fatc.f_bpb.bs_sectors_per_track;
	boot_dev.u_trkPerCyl = Fatc.f_bpb.bs_heads;

	/*
	 * Wait till here to init the track level caching so that
	 * we can figure out how many tracks are on the media. This is
	 * needed because a 2.88Mb floppy will report 36 sectors/track.
	 * If we allocate that amount and try to read a tracks worth on
	 * a 1.44Mb floppy (18 sectors/track) the read will fail and
	 * nothing will work.
	 */
	InitCache_bios();

        Fatc.f_adjust = adjust;
        Fatc.f_rootsec = Fatc.f_bpb.bs_num_fats *
                Fatc.f_bpb.bs_sectors_per_fat +
                Fatc.f_bpb.bs_resv_sectors;
        Fatc.f_rootlen = (Fatc.f_bpb.bs_num_root_entries *
                sizeof(_dir_entry_t)) / SECSIZ;
        Fatc.f_filesec = Fatc.f_rootsec + Fatc.f_rootlen;
        Fatc.f_dclust = CLUSTER_ROOTDIR;
        Fatc.f_flush = 0;
        Fatc.f_nxtfree = CLUSTER_FIRST;

	/*
	 * figure out the number of clusters in this partition. If the
	 * "old" sectors_in_volume has a value use it (it's only a 16bit
	 * value). Otherwise use the newer sectors_in_logical_volume.
	 * Subtract the number of clusters used by the file system overhead.
	 */
	Fatc.f_ncluster = (((u_long)Fatc.f_bpb.bs_sectors_in_volume ?
	    (u_long)Fatc.f_bpb.bs_sectors_in_volume :
	    (u_long)Fatc.f_bpb.bs_sectors_in_logical_volume) -
	    Fatc.f_filesec) /
	    (u_long)Fatc.f_bpb.bs_sectors_per_cluster;

	Fatc.f_16bit = Fatc.f_ncluster >= MAXCLST12FAT;

	/*
	 * free previous allocated fats.
	 */
	if (Fat_sector_size) {
		free_util((u_short)Fat_sector_p, Fat_sector_size);
		Fat_sector_size = 0;
	}

	/*
	 * limit size of fat table in core. this should be enough
	 * to handle all floppies which causes the biggest speed
	 * bottle necks.
	 */
	if (Fatc.f_bpb.bs_sectors_per_fat < 20) {
		Fat_sector_size = Fatc.f_bpb.bs_sectors_per_fat;
		Fat_sector_p = (char *)malloc_util(Fat_sector_size *
			SECSIZ);
		if (Fat_sector_p == (char *)0) {
			c_fatal_err("Cannot allocate enough memory for FAT table.");
		}

		if (ReadSect_bios(Fat_sector_p,
		    Fatc.f_bpb.bs_resv_sectors + Fatc.f_adjust,
		    Fatc.f_bpb.bs_sectors_per_fat)) {
			c_fatal_err("Cannot read FAT table.");
		}

		/* ---- setup to use in core fat ---- */
		Fat_blk_start = Fatc.f_bpb.bs_resv_sectors + Fatc.f_adjust;
		Fat_blk_end = Fat_blk_start + Fatc.f_bpb.bs_sectors_per_fat;
	}
	else {
		/*
		 * the fat is to big to be held in memory. Allocate enough
		 * memory so that our main program to be loaded can have
		 * all of its fat entries in memory. The second level boot
		 * program should be in contiguous blocks.
		 * Currently boot.bin = 219136 bytes.
		 * Assume 1 cluster per sector and 16bit fat.
		 * 219136 / 512 = 428 sectors / 1 = 428 clusters
		 * 428 clusters * 2 (16bit fat = 2 bytes) = 856 bytes
		 * Round that up and you need 3 sectors.
		 * NOTE: the sector size is being set to 4. This is for the
		 * case on a 12bit fat where the index would be computed to
		 * be the last byte in the last sector but would need the
		 * next byte in the following sector which isn't in the cache.
		 * By reading in one extra sector which isn't counted as part
		 * of the normal cache we cover our tail end so to speak.
		 */
		Fat_sector_size = 3;
		Fat_blk_start = Fat_blk_end = -1;
		if (!(Fat_sector_p = (char *)malloc_util((Fat_sector_size + 1) *
		    SECSIZ))) {
			c_fatal_err("Cannot allocate space for small FAT table.");
		}
	}

	Dprintf(DBG_FLOW, ("init_fat: succeeded.\n"));

	return 0;
}

/*
 *[]------------------------------------------------------------[]
 * | Fdisk_fat -- tries to find a fdisk partition, if the	|
 * | device is a hard disk, and returns starting sector and	|
 * | length if possible in pointer fp.				|
 * |								|
 * |	Returns 0 if fdisk partition is found or device is a 	|
 * |		floppy.						|
 * |	Returns 1 if no fdisk partition or partition type isn't	|
 * |		found.						|
 *[]------------------------------------------------------------[]
 */
int
Fdisk_fat(int type, _fdisk_p fp)
{
        u_short *p;
	int i, rtn;
	long adjust, size;
	_fdisk_p fd;
	char lbuf[SECSIZ];

        /* ---- High bit indicates hard disk, look for fdisk part ---- */
	if (boot_dev.dev_type != DT_FLOPPY) {
                if (ReadSect_bios(&lbuf[0], 0, 1)) {
                        c_fatal_err("Cannot read fdisk table.");
			return 1;
                }
                p = (u_short *)&lbuf[SECSIZ - sizeof(short)];
                if (*p == (u_short)0xaa55) {
                        fd = (_fdisk_p)&lbuf[FDISK_START];
                        for (i = 0, adjust = -1; i < FDISK_PARTS; i++, fd++) {
                                if (((type != TYPE_DOS) && (fd->fd_type == type))
||
				    ((type == TYPE_DOS) && (fd->fd_active ==
0x80))) {
					adjust = 0;
					bcopy_util((char *)fd, (char *)fp,
						sizeof(_fdisk_t));
					fp->fd_partition = i;
                                        break;
				}
                        }
                        if (adjust == -1) {
				fp->fd_start_sec = 0;
				fp->fd_part_len = 0;
				rtn = 1;
                        }
			else
				rtn = 0;
                }
                else {
                        /* ---- If no fdisk part. Assume one big part ---- */
			fp->fd_start_sec = 0;
			fp->fd_part_len = 0;	/* unknown */
			rtn  = 0;
		}
        }
        else {
		fp->fd_start_sec = 0;
		fp->fd_part_len = 0;
		rtn = 0;
	}
	return rtn;
}

/*
 *[]------------------------------------------------------------[]
 * | map_fat -- find the next cluster in the chain		|
 *[]------------------------------------------------------------[]
 */
u_short
map_fat(u_short blk, int root)
{
        u_long sectn, fidx;
        u_char *fp;

	if (root) {
		return ((blk+1) >= Fatc.f_rootlen ? CLUSTER_EOF : blk + 1);
	}

	/* ---- Find out what sector this cluster is in ---- */
	fidx = Fatc.f_16bit ? (u_long)blk << 1 : (u_long)blk + (blk >> 1);

	sectn = (fidx / (SECSIZ * Fat_sector_size) * Fat_sector_size) +
		Fatc.f_bpb.bs_resv_sectors + Fatc.f_adjust;

	if ((sectn < Fat_blk_start) || (sectn > Fat_blk_end)) {

		DPrint(DBG_FAT_0, ("(%ld)", sectn));

		/*
		 * Avoid flushing the cache of other data and go
		 * straight to the disk.
		 * NOTE: The only time this read is done is when the fat
		 * wasn't able to be read into memory during initialization.
		 * Fat_sector_p has been allocated with enough extra memory
		 * to allow us to read in an extra sector.
		 */
	        if (read_sectors(&boot_dev, sectn, Fat_sector_size + 1,
	        		Fat_sector_p) != Fat_sector_size + 1)
			return (CLUSTER_BAD_16);

		Fat_blk_start = sectn;
		Fat_blk_end = sectn + Fat_sector_size - 1;
	}

	DPrint(DBG_FAT_0, ("fidx 0x%lx, sectn 0x%lx, cache %lx-%lx\n",
		fidx, sectn, Fat_blk_start, Fat_blk_end));

	fp = &Fat_sector_p[fidx % (SECSIZ * Fat_sector_size)];

        if (Fatc.f_16bit)
                blk = fp[0] | (fp[1] << 8);
        else {
                if (blk & 1)
                        blk = ((fp[0] & 0xf0) >> 4) |
                                (fp[1] << 4);
                else
                        blk = ((fp[1] & 0xf) << 8) | fp[0];

		/*
		 * This makes compares easier because we can just
		 * compare against one value instead of two.
		 */
		if (blk >= CLUSTER_RES_12_0)
		  blk |= CLUSTER_RES_16_0;
        }

	DPrint(DBG_FAT_0, ("[0x%x.0x%x=0x%x]", fp[0], fp[1], blk));

        /* ---- return only 16bits ---- */
        return blk;
}


/*
 *[]------------------------------------------------------------[]
 * | setcluster_fat -- set the given cluster entry in the fat	|
 * | to a specific value.					|
 *[]------------------------------------------------------------[]
 */
void
setcluster_fat(int c, int v)
{
	u_long	idx;		/* ---- index into fat table */
	u_long	s;		/* ---- sector number to read if needed */
	u_char	*fp;		/* ---- pointer to fat entries */

	idx = Fatc.f_16bit ? (u_long)c << 1 : (u_long)c + (c >> 1);

	s = (idx / (SECSIZ * (Fat_sector_size - 1)) * (Fat_sector_size - 1)) +
	    Fatc.f_bpb.bs_resv_sectors + Fatc.f_adjust;
	/* ---- do we have a fat cache? equals means no cache ---- */
	if ((s < Fat_blk_start) || (s > Fat_blk_end)) {

	        if (read_sectors(&boot_dev, s, Fat_sector_size,
	        		Fat_sector_p) != Fat_sector_size)
			return;
		Fat_blk_start = s;
		Fat_blk_end = s + Fat_sector_size - 1;
	}

	fp = &Fat_sector_p[idx % (SECSIZ * (Fat_sector_size - 1))];

	if (Fatc.f_16bit)
		*(u_short *)fp = v;
	else {
		if (c & 1) {
			*fp = (*fp & 0x0f) | ((v << 4) & 0xf0);
			fp++;
			*fp = (v >> 4) & 0xff;
		}
		else {
			*fp++ = v & 0xff;
			*fp = (*fp & 0xf0) | ((v >> 8) & 0x0f);
		}
	}

	Fatc.f_flush = 1;
	if (v == CLUSTER_AVAIL)
		Fatc.f_nxtfree = CLUSTER_FIRST;
}

/*
 *[]------------------------------------------------------------[]
 * | ctodb_fat -- convert a cluster number into a disk block	|
 *[]------------------------------------------------------------[]
 */
u_long
ctodb_fat(u_short c, int root)
{
	u_long s;

	if (root) {
		s = c + Fatc.f_rootsec + Fatc.f_adjust;
	}
	else {
		s = (((u_long)c - 2) * Fatc.f_bpb.bs_sectors_per_cluster) +
		  Fatc.f_filesec + Fatc.f_adjust;
	}

	Dprintf(DBG_FLOW, ("ctodb_fat: c %x, s %lx\n", c, s));

	return (s);
}
