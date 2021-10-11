/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ident "@(#)ix_alts.c	1.33	99/10/07 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/vtoc.h>
#include <sys/bootdef.h>
#include <sys/sysmacros.h>
#include <sys/dktp/altsctr.h>
#include <sys/dktp/fdisk.h>
#include <sys/booti386.h>

#include <sys/fs/hsfs_spec.h>
#include <sys/fs/hsfs_isospec.h>

#include <sys/ihandle.h>
#include <sys/salib.h>

extern int read_blocks(struct ihandle *ihp, daddr_t off, int cnt);
extern void *memcpy(void *s1, void *s2, size_t n);

void bd_freealts(struct ihandle *ihp);

/* #define	DEBUG */

#ifdef DEBUG
#define	Dprintf(x)	printf x
#else
#define	Dprintf(x)
#endif

/*
 * Stand-alone filesystem alternate sector handling routines.
 */

struct d_blk {				/* Sector re-map structure:	*/
	ulong	sec;			/* .. Good sector number	*/
	ulong	cnt;			/* .. Number of sectors		*/
};

struct alts_part {			/* Solaris alternate sector slice: */
	struct alts_parttbl	ap_tbl;	/* .. Status info */
	struct alts_ent *ap_entp;	/* .. The alternate entry array	*/
};

extern char *new_root_type;

/*
 * read_direct -- read without alternates
 *
 * This routine reads "len" bytes of data from the specified
 * sector "off"set into the indicated "buf"fer.  It DOES NOT
 * make use of the alternate sector map, hence may be used to
 * read the sector map itself!
 *
 * The "off" argument is in 512-byte sectors, not the blocksize
 * of the device.  This routine handles the block size
 * conversion.
 *
 * Returns a non-zero value if there's an I/O error.
 */

static int
read_direct(struct ihandle *ihp, daddr_t off, char *buf, int len)
{
	daddr_t block;
	unsigned int block_factor;
	unsigned int byte_offset;

	/*
	 * Convert the start sector to blocks.  Then calculate how
	 * many bytes from the block start to the requested sector
	 * start.
	 */
	block_factor = ihp->dev.disk.bps / DEV_BSIZE;
	block = off / block_factor;
	byte_offset = (off - block * block_factor) * DEV_BSIZE;

	/*
	 * Loop until all requested data has been read.
	 * "Len" gives the number of bytes to be read, "j" is
	 * the number of sectors we're reading on this
	 * iteration of the loop.
	 */
	while (len > 0) {
		int j = roundup(len, ihp->dev.disk.bps) / ihp->dev.disk.bps;

		if (read_blocks(ihp, block, j) != -1) {
			/*
			 * Return I/O error indication.  No error
			 * message has been generated as yet!
			 */
			return (-1);
		}

		block += j;
		if ((j *= ihp->dev.disk.bps) > len)
			j = len;

		(void) memcpy(buf, cache_info.active_buffer + byte_offset, j);
		buf += j;
		len -= j;
		byte_offset = 0;
	}

	return (0);
}

/*
 * get_vtoc -- read the VTOC
 *
 * This routine probes the disk for the following:
 *
 *	1).  A DOS master boot record (containing FAT *
 *	partition table).  If file system type is "pcfs" * or if no
 *	slice selector has been specified * ("dev.disk.num >= 0"),
 *	this is as far as we go.
 *
 *	2).  If (1) succeeds and we have a slice selector, we
 *	look for a Solaris partition containing a valid SVR4
 *	VTOC. If it's present, we know we have a "ufs" file
 *	system.
 *
 *	3).  If (1) fails and we could be looking for a "hsfs" file
 *	system, look for an ISO_9660 VTOC.
 *
 * Returns a pointer to the SOLARIS vtoc if we find one,
 * -1 if we decide it's a valid "pcfs" or "ufs" disk, 0 if we can't
 * find anything.  As a side effect, this routine sets the "disk.siz"
 * and "disk.par" fields of the ihandle structure.
 */

#define	cd2hd(x) ((ISO_SECTOR_SIZE / ihp->dev.disk.bps) * (x))

static struct dk_vtoc *
get_vtoc(struct ihandle *ihp, int prt)
{
	static struct dk_vtoc vtoc;

	/*
	 * It is safe to use read_blocks here even though we want only
	 * one sector because we know the cache is big enough for at
	 * least one block and because the sector is the first sector
	 * of the drive.
	 *
	 * NOTE: read_blocks returns -1 when it SUCCEEDS!
	 */
	if (read_blocks(ihp, (daddr_t)0, 1) != -1) {
		Dprintf(("get_vtoc: read_blocks failed for sector 0.\n"));
		goto ex;
	}

	/*
	 * We just read the first sector of the disk. See if it
	 * contains a valid DOS partition table before probing for a
	 * Solaris partition.
	 */
	Dprintf(("get_vtoc: first sector signature contains %x.\n",
		((struct mboot *)cache_info.active_buffer)->signature));
	if (((struct mboot *)cache_info.active_buffer)->signature ==
								MBB_MAGIC) {
		int k = ihp->dev.disk.num;
		int j = ((k >= 0) ? 0 : FD_NUMPART);
		struct ipart fpt[FD_NUMPART];

		/*
		 * There's a DOS FDISK signature on this disk, which
		 * means that we're either looking at a "ufs" or a
		 * "pcfs" file system.  Which one we want depends on
		 * what the caller is trying to do!
		 */

		(void) memcpy(&fpt,
		    &((struct mboot *)cache_info.active_buffer)->parts,
		    (FD_NUMPART * sizeof (struct ipart)));

		Dprintf(("get_vtoc: disk num %d, initial partition num %d\n",
			k, j));

		for (; j < FD_NUMPART; j++) {
			/*
			 * There may be up to FD_NUMPART partitions
			 * on this disk.  The first Solaris (or
			 * INTERACTIVE UNIX) partition we come to is
			 * the one we'll use.
			 */
			if (fpt[j].numsect <= 0 ||
			    (fpt[j].systid != SUNIXOS &&
			    fpt[j].systid != UNIXOS)) {
				Dprintf(("get_vtoc: skipping partition %d "
					"with id %x.\n", j, fpt[j].systid));
				continue;
			}

			/*
			 * We found a UNIX partition, now read in the vtoc
			 * struct and return its address to the caller.
			 */
			Dprintf(("get_vtoc: found UNIX partition %d with id "
				"%x.\n", j, fpt[j].systid));
			if (read_direct(ihp,
			    (fpt[j].relsect + DK_LABEL_LOC), (char *)&vtoc,
			    sizeof (vtoc))) {
				Dprintf(("get_vtoc: read_direct failed for "
					"sector %x.\n",
					fpt[j].relsect + DK_LABEL_LOC));
				goto ex;
			}

			/*
			 * The vtoc was read correctly.  Make
			 * sure it looks reasonable before
			 * verifying the slice number.
			 */
			if (vtoc.v_sanity != VTOC_SANE) {
				Dprintf(("get_vtoc: VTOC not sane: %x.\n",
					vtoc.v_sanity));
				continue;
			}

			/*
			 * The VTOC is OK, now check the requested
			 * slice. If the vtoc doesn't look good, we
			 * go on to the next FAT partition.  This is
			 * in deference to a popular shareware OS
			 * that uses the Solaris partition type to
			 * mark its swap area!
			 */
			if (vtoc.v_part[k].p_tag == 0 && prt) {
				/*
				 * Caller is trying to open a
				 * vtoc slice that hasn't
				 * been allocated yet.
				 */
				printf("%s: can't open - slice not"
				    " allocated\n", ihp->pathnm);
				return ((struct dk_vtoc *)0);
			}

			if (!prt) {
				/*
				 * Find the root partition
				 */
				for (k = 0; k < V_NUMPAR; k++)
					if (vtoc.v_part[k].p_tag == V_ROOT)
						break;
				if (k == V_NUMPAR)
					k = 0;
			}
			if (vtoc.v_part[k].p_tag != 0) {
				/*
				 * Solaris slice is allocated, so we
				 * can finally allow the open to
				 * succeed.
				 */
				ihp->dev.disk.siz = vtoc.v_part[k].p_size;
				ihp->dev.disk.par = vtoc.v_part[k].p_start;
				ihp->dev.disk.bas = fpt[j].relsect;
				ihp->dev.disk.num = k;

				if (!prt || !new_root_type) {
					/*
					 * Let caller know what the
					 * file system type is!
					 */
					new_root_type = "ufs";
				}

				return (&vtoc);
			}
		}

		if (!prt || !new_root_type) {
			/*
			 * If caller is trying to determine the device
			 * type, we know it must be "pcfs" because
			 * there's no Solaris partition on the disk!
			 */
			new_root_type = "pcfs";
			prt = 0;

		} else if (prt && *new_root_type &&
		    strcmp(new_root_type, "pcfs")) {
			/*
			 * If, on the other hand, caller is looking
			 * for something other than a DOS file system,
			 * (s)he's out of luck!
			 */
			printf("%s: can't open - no VTOC\n", ihp->pathnm);
			return ((struct dk_vtoc *)0);
		}

	} else if ((ihp->dev.disk.spt >= cd2hd(1)) &&
	    (read_blocks(ihp, cd2hd(ISO_VOLDESC_SEC), cd2hd(1)) == -1)) {
		/*
		 * We were able to read an ISO_9660 id sector from
		 * the disk.  Make sure it's really a CDROM!
		 */
		if ((ISO_DESC_TYPE(cache_info.active_buffer) == ISO_VD_PVD) &&
		    strncmp((char *)ISO_std_id(cache_info.active_buffer),
			ISO_ID_STRING, ISO_ID_STRLEN) == 0 &&
		    ISO_STD_VER(cache_info.active_buffer) == ISO_ID_VER) {
			/*
			 * This is a CDROM all right. Now what do we
			 * do with it?
			 */
			if (!prt || !new_root_type) {
				/*
				 * If caller was trying to determine
				 * the file system type, we know
				 * know that it's "hsfs"!
				 */
				new_root_type = "hsfs";
				prt = 0;

			} else if (!*new_root_type ||
			    strcmp(new_root_type, "hsfs") == 0) {
				/*
				 *  We're also OK if caller wants a 9660
				 *  file system ...
				 */
				prt = 0;
			}
		}

		if (prt) {
			/*
			 * ... but we deliver a "no VTOC" error if
			 * caller is assuming the file system is
			 * something other that "hsfs"!
			 */
			printf("%s: can't open - no hsfs VTOC\n", ihp->pathnm);
			return ((struct dk_vtoc *)0);
		}
	}

	Dprintf(("get_vtoc: falling through to ex:\n"));

ex:	if (prt) {
		/*
		 *  We were unable to read the master boot record.
		 */
		printf("%s: can't open - I/O error\n", ihp->pathnm);
		return ((struct dk_vtoc *)0);
	}

	if (!new_root_type)
		new_root_type = "";

	return ((struct dk_vtoc *)-1);
}

/*
 * sort_alts -- sort (and collapse) alternate sector table
 *
 * If the alternate sector list is set up properly, this
 * routine will be a no-op (i.e, the alt sector list is
 * supposed to be pre-sorted with no overlapping entries).  We
 * don't really want to trust this assumpiton, however, so
 * we'll sort the list and remove all over over- lapped
 * entries.
 *
 * Returns 0 if it works, -1 (after printing an error
 * message) if we can't fix the list up.
 */

#define	swap(x, y) tmp = *(x); *(x) = *(y); *(y) = tmp

static int
sort_alts(struct ihandle *ihp)
{
	int ok = 1;
	int msg = 0;
	int done = 0;
	struct alts_ent *aep;
	int *cntp = (int *)ihp->dev.disk.alt;

	/*
	 * Bubble sort the alternate entry list and flatten
	 * out any entries that overlap one another.  We may
	 * free up entries as we process the list.  These are
	 * moved to the end of the list and flagged with a
	 * "bad_start" address of (daddr_t)-1.  When we get to
	 * the end of the list, we'll move these unused
	 * entries back to the front before starting the next
	 * sort pass.
	 */
	while (cntp && !done) {
		struct alts_ent *axp, *azp;
		int j, k = cntp[0];
		int reseq = 0;

		aep = ((struct alts_ent *)&cntp[2]) + cntp[1];
		done = ok = 1;

		/*
		 * Check each entry in the alternate sector
		 * array.  Normally, we will only have to make
		 * a single pass over this data, but some
		 * sorting may be needed if there's a lot of
		 * overlap in the map.
		 */
		while (--k > 0) {
			struct alts_ent tmp;

			ok = (aep->bad_end < aep->bad_start);
			if (ok) {
				continue;
			}

			/*
			 *  Next entry looks good.  Check it against
			 *  the previous one!
			 */
			axp = aep;  /* Pointer to previous entry */
			aep++;	    /* Pointer to current entry	 */

			if (aep->bad_start == (daddr_t)-1) {
				/*
				 * We've freed up one or more entry
				 * that could be used to fix up any
				 * overlapped entries that we missed
				 * on this pass.  Move the unused
				 * entries to the front of alts_ent
				 * list and sort again if necessary.
				 */

				/* Pointer to end of alts list	*/
				azp = aep + k++;

				/*
				 * This loop moves the alts list "k"
				 * entries to the right.
				 */
				for (j = (cntp[0] - k); j--; (azp--, aep--)) {
					azp[0] = aep[-1];
				}

				/* There are "k" less entries used   */
				cntp[0] -= k;
				/* There are "k" more unused entries */
				cntp[1] += k;
				done = ok;
				aep -= 1;
				break;

			} else if (aep->bad_start < axp->bad_start) {
				/*
				 *  A sort error (or we allocated an extra
				 *  entry which must now be sorted into its
				 *  proper position).  Swap current entry with
				 *  previous and clear the "done" flag.
				 */
				swap(axp, aep);
				reseq = 1;
				done = 0;
			} else if ((j = axp->bad_end - aep->bad_end - 1) >= 0) {
				/*
				 *  The current entry does not reach past the
				 *  end of the previous one.  We've got some
				 *  patch-up to do ...
				 */
				reseq = 1; /* This isn't supposed to happen! */

				if (aep->bad_start == axp->bad_start) {
					/*
					 * .. If the two entries start at the
					 * same position, we change the previous
					 * one to point just past the current
					 * (and swap them to maintain the sort
					 * order).
					 */
					axp->good_start +=
						(axp->bad_end-axp->bad_start-j);
					axp->bad_start = (axp->bad_end-j);
					swap(axp, aep);

				} else if (cntp[1] > 0) {
					/*
					 * .. If we have an unused entry at
					 * the front of the list, we can use
					 * it to record slopover past the end
					 * of this entry. We'll have to re-sort
					 * it though, so make sure "done" flag
					 * is clear.
					 */
					azp = ((struct alts_ent *)&cntp[2]) +
						--(cntp[1]);
					azp->bad_start = axp->bad_end - j;
					azp->bad_end = axp->bad_end;

					j = (axp->bad_end - axp->bad_start) - j;
					azp->good_start = axp->good_start + j;
					axp->bad_end = aep->bad_start - 1;
					done = 0;

				} else {
					/*
					 * We can't fix this problem on this
					 * pass!  Clear the "ok" flag to force
					 * an additional pass if we find unused
					 * entries at the end of the list.
					 */
					ok = 0;
				}

			} else if (aep->bad_start <= axp->bad_end) {
				/*
				 * There's some overlap between the
				 * current entry and the previous
				 * one.  Fixups are analogous to the
				 * case above.
				 */
				if (aep->bad_start > axp->bad_start) {
					/*
					 *  The current entry starts
					 * beyond the previous one.
					 * We can fix this by pushing
					 * the end of the previous
					 * entry back.
					 */
					axp->bad_end = (aep->bad_start - 1);
				} else {
					/*
					 * The current entry starts
					 * at the same position as
					 * the previous one.  Since we
					 * already know that it
					 * extends beyond its end, we
					 * can simply free up the
					 * previous entry!
					 */
					*axp = *aep;

					for (j = k; j-- > 0; aep++)
						aep[0] = aep[1];
					aep->bad_start = (daddr_t)-1;
					aep = axp;
				}
			}
		}

		if (reseq && !msg++) {
			/*
			 * The alternate list shouldn't be this
			 * messed up. Ignore the map and continue.
			 */
			printf("WARNING: alt sector map is scrambled\n");
		}
	}

	if (!ok || (cntp && (aep->bad_end < aep->bad_start))) {
		/*
		 * There wasn't enough free space left to flatten out the over-
		 * lapped entries.  Print an error message and free
		 * resources. Continue with the boot since it is unlikely
		 * the sector remapping happened to hit boot files. If
		 * the disk is really messed up, it will become evident
		 * very soon.
		 */
		printf("sort_alts(): alternate sector map corrupted on %s\n",
			ihp->pathnm);
		bd_freealts(ihp);
	}

	return (0);
}

/*
 * bd_getmap -- build alternate sector map
 *
 * This routine reads the Solaris altenate sector partition at the
 * specified disk "off"set and converts it into an internal form that
 * can be used for mapping alternate sectors.	The resulting data
 * structure is then hung off of the "ihandle" of the disk we're trying
 * to open.
 *
 * If the alternate sector partition looks wrong, print a message
 * and free resources, but never fail. We'll get more information
 * by ignoring the alternates and continuing with the boot.
 */

static int
bd_getmap(struct ihandle *ihp, daddr_t off)
{
	int j;
	struct alts_parttbl aps; /* Build temporary alt sector map here	*/

	if (read_direct(ihp, off + ihp->dev.disk.bas,
	    (char *)&aps, sizeof (aps))) {
		/*
		 * We were unable to read all or part of the alternate sectors
		 * partition.
		 */
		printf("I/O error reading alternate sector map table on %s\n",
			ihp->pathnm);
		return (0);
	}

	/*
	 * We were able to successfully read the control information.
	 * Now convert it into the more easily used internal form.
	 */
	if (aps.alts_sanity != ALTS_SANITY) {
		/*
		 * The partition was most likely reserved and never
		 * initialized. This is apparently a normal situation,
		 * so don't print anything.
		 */
		return (0);
	}

	/*
	 * The control table looks good, check to see how many
	 * alternate sectors have been assigned to this disk.
	 */
	if ((j = aps.alts_ent_used) <= 0) {
		/*
		 * If no alternate sectors have been assigned,
		 * there's no point in building an alternate sector
		 * map. Simply return instead.
		 */
		return (0);
	}

	/*
	 * We need to dynamically allocate a buffer to hold
	 * the alternate sector map we're about to read in.
	 * The map starts at "alts_ent_base" and extends
	 * thru "alts_ent_end", but we may not be using all
	 * of that.  Anything that's left over is reserved
	 * for fixing up overlaps in the sort routine!
	 */
	j = (aps.alts_ent_end-aps.alts_ent_base+1) * ihp->dev.disk.bps;
	j = (j / sizeof (struct alts_ent)) * sizeof (struct alts_ent);

	if (ihp->dev.disk.alt = (void *)bkmem_alloc(j+(2*sizeof (int)))) {
		int *ip = (int *)ihp->dev.disk.alt;

		/*
		 * We've got a buffer, now read the alternate sector entries
		 * into it.  We put the unused entries at the front of the
		 * buffer to make it easier for the sort routine to find them.
		 */

		*ip++ = aps.alts_ent_used;
		*ip++ = (j/sizeof (struct alts_ent)) - aps.alts_ent_used;

		if (!read_direct(ihp,
		    (off + aps.alts_ent_base + ihp->dev.disk.bas),
		    (char *)ip + (ip[-1] * sizeof (struct alts_ent)),
		    (aps.alts_ent_used * sizeof (struct alts_ent)))) {

			return (sort_alts(ihp));
		}

		/*
		 * I/O error trying to read in the alternate sector
		 * mapping entries.  Free up the buffer we allocated
		 * before reporting this error.
		 */
		bkmem_free(ihp->dev.disk.alt, j);
		printf("I/O error reading alternate sector map on %s\n",
			ihp->pathnm);
	} else {
		printf("can't read alternate sectors on %s; no memory\n",
			ihp->pathnm);
	}

	return (0);
}

/*
 * build_map -- build alternate track map
 *
 * This routine is called form "bd_readlts" to check to see if any of
 * the sectors in the track that's being read are marked bad.	If so,
 * we return a pointer to an array of "d_blk" entries that can be used
 * to re-map the read request.	 The "d_blk" array is dynmaically
 * allocated and contains "cnt" entries -- the caller has to free it up.
 *
 * We return a null pointer if the original request contains no bad
 * sector.s  We return "(struct d_blk *)-1" if we can't get memory.
 */

static struct d_blk *
build_map(struct ihandle *ihp, daddr_t off, int cnt)
{
	int *ip = (int *)ihp->dev.disk.alt;
	struct d_blk *dbp, *dxp = (struct d_blk *)0;
	struct alts_ent *aep = ((struct alts_ent *)&ip[2]) + ip[1];

	int j = cnt;
	int n = ip[0];

	/*
	 * Merge requested sectors with alternate sector
	 * map. "j" register counts the number of un-mapped
	 * sectors remaining, "n" counts the number of
	 * un-examimed alternate sector entries.
	 *
	 * NOTE: This code assumes that the alternate sector
	 * map is sorted in ascending sequence of "bad_start"
	 * addresses!
	 */
	while ((j > 0) && (n > 0)) {
		if (off > aep->bad_end) {
			/*
			 * If next sector offset is beyond the end of
			 * the current alt entry, advance the alt
			 * entry pointer.
			 */
			aep += 1;
			n -= 1;
		} else if ((off + j) > aep->bad_start) {
			/*
			 * All or part of the remaining request is
			 * marked bad.  We will have to re-map at
			 * least part the request.
			 */
			if (dxp == (struct d_blk *)0) {
				/*
				 * If we don't have a "d_blk" array
				 * yet, allocate it now.  We allocate
				 * one entry per sector that's being
				 * read, although this is
				 * undoubtedly too big (figuring out
				 * the minimum size required is pain
				 * in the butt!).
				 */
				dxp = dbp = (struct d_blk *)
				    bkmem_alloc(cnt * sizeof (struct d_blk));
				if (dxp == 0) {
					/*
					 *  If we can't get memory for the
					 *  sector map, we can't re-map the bad
					 *  sector.  Caller will treat this as
					 *  an I/O error.
					 */
					printf("%s: no memory to map "
					    "bad sector!\n", ihp->pathnm);
					return ((struct d_blk *)-1);
				}
			}

			if (off < aep->bad_start) {
				/*
				 * If the first few sectors of this
				 * un-mapped stuff are OK, map them to
				 * themselves and advance the current
				 * position pointers.
				 */
				dbp->sec = off;
				dbp->cnt = aep->bad_start - off;

				off = aep->bad_start;
				j -= dbp->cnt;
				dbp += 1;
			}

			dbp->sec = aep->good_start+ (off - aep->bad_start);
			dbp->cnt = (aep->bad_end - off + 1);
			if (dbp->cnt > j)
				dbp->cnt = j;

			off += dbp->cnt;
			j -= dbp->cnt;
			dbp += 1;
			aep++;
			n--;

		} else {
			/*
			 * Exit the loop prematurely if we know that
			 * the remainder of the request can't possibly
			 * match the next alts entry.
			 */
			break;
		}
	}

	if (dxp && (j > 0)) {
		/*
		 *  If we did some re-mapping and we have sectors left
		 *  unprocessed ("j" register non-zero), map all remaining
		 *  sectors to themselves.
		 */
		dbp->sec = off;
		dbp->cnt = j;
	}

	return (dxp);
}

/*
 * bd_getalts -- process Solaris VTOC
 *
 * This routine is called when we're trying to open a disk with a unix
 * file system on it.	It looks for a UNIX vtoc on the disk and sets
 * device-specific info in the "ihandle" based on information contained
 * therein.  In addition to the partition size and base, this may include
 * an alternate sector map for the disk.
 *
 * The prt flag that is passed in is an indication of the
 * nature of the disk device.  Specifically, prt is zero only in
 * the case where the disk we are examining is the initial boot
 * device. A special name is used for access to this device at the
 * beginning of the booter's execution. We don't initially know
 * what file system is expected on the boot media, but want to
 * discover that information.  Several error conditions need to
 * be silently ignored for this special case.  A strcmp in
 * open_disk() looks for the special name and sets prt accordingly.
 *
 * Returns a non-zero value (after printing an error message) if some-
 * thing goes wrong.  If the file system type is missing (or is not
 * "ufs") we let errors go unreported.
 */

int
bd_getalts(struct ihandle *ihp, int prt)
{
	int j;
	struct dk_vtoc *vtp;

	if (((vtp = get_vtoc(ihp, prt)) != NULL) &&
	    ((vtp != (struct dk_vtoc *)-1) != NULL)) {
		/*
		 * We were able to read the vtoc, now check to see if an
		 * alternate sector partition was allocated.  If so, we'll
		 * have to build an alternate sector map for this device.
		 */
		for (j = 0; j < vtp->v_nparts; j++) {
			/*
			 * Check all slices.  We know that there can be no
			 * more than one alternate slice, so we'll use the
			 * first one we find.
			 */
			if ((vtp->v_part[j].p_tag == V_ALTSCTR) &&
			    (vtp->v_part[j].p_size != 0)) {
				/*
				 * Found the alternates sector slice.  Use the
				 * "bd_getmap()" routine to build an
				 * alternates map for this device and
				 * hang it off of the "ihandle".
				 */
				return (bd_getmap(ihp, vtp->v_part[j].p_start));
			}
		}

		return (0);
	}

	/*
	 * XXX returning 0 allows the initial mount of dos solaris boot
	 * partition to succeed. This was broken, when preparing to
	 * better recover from disk errors. This mount code needs a rewrite.
	 */
	if (*new_root_type && strcmp(new_root_type, "pcfs") == 0)
		return (vtp ? 0 : -1);
	else
		return (-1);
}

/*
 * bd_freealts -- free alternate sector map
 *
 * This routine is called when a disk containing a ufs file
 * system is closed.  If an alternate sector map has been allocated
 * to this device, this routine frees it up!
 */

void
bd_freealts(struct ihandle *ihp)
{
	int *ip = (int *)ihp->dev.disk.alt;

	if (ip != (int *)0) {
		/*
		 * "ip" points to the alternate sector map in the "ihandle"
		 * struct.  Free the space associated with this data.
		 */
		bkmem_free((caddr_t)ip,
		    ((ip[0]+ip[1])*sizeof (struct alts_ent))+(2*sizeof (int)));
		ihp->dev.disk.alt = 0;
	}
}

/*
 * bd_readalts -- read with alt sector map
 *
 * This routine behaves much like the "read_blocks" routine
 * in "disk.c", except that it performs any alternate sector
 * mapping that might be required.  Returns -1 if it works,
 * failing sector number if there's an I/O error.
 *
 * Note: bd_readalts args are in sectors, read_blocks args
 * are in device blocks.
 */

int
bd_readalts(struct ihandle *ihp, daddr_t off, int cnt)
{
	struct d_blk *dbp;
	int spb = ihp->dev.disk.bps / DEV_BSIZE;

	if (ihp->dev.disk.alt && (dbp = build_map(ihp, off, cnt))) {
		/*
		 * Disk has an alternate sector map, and one or more of the
		 * sectors that the caller is trying to read have been
		 * re-mapped.  Break the read request down into multiple
		 * calls to "read_blocks" based on the number of "dbp"
		 * entries filled in by "build_map".
		 */
		char *track, *cp;
		int rc = -1, n = cnt;
		caddr_t buf = (caddr_t)dbp;

		if ((dbp  == (struct d_blk *)-1) ||
		    !(track = cp =
		    (char *)bkmem_alloc(cnt * ihp->dev.disk.bps))) {
			/*
			 * We were unable to get enough memory to map a bad
			 * sector!  Treat this as an I/O error on the first
			 * sector.
			 */
			return ((int)off);
		}

		/*
		 * The "n" register counts number of sectors we've
		 * read already, "dbp" advances thru the re-map
		 * entries.  We read each cluster with a recursive
		 * call, which allows alternate sectors to
		 * themselves be remapped ...
		 */
		while ((n > 0) && (rc == -1)) {
			static int recur = 0;

			if (recur > 4) {
				/*
				 * ... but there are limits to how deep
				 * we're allowed to recur.  Remapping an
				 * alternate after it's been assigned
				 * is OK once or twice, but five times?
				 * Something is not right, so bail out before
				 * we run off the end of the stack.
				 */
				printf("%s: cycle in alternate sector map\n",
				    ihp->pathnm);
				rc = (int)off;
				break;
			}

			recur++;  /* Increment recursion count */
			rc = bd_readalts(ihp, dbp->sec, dbp->cnt);
			recur--;  /* Decrement recursion count */

			if (rc == -1) {
				/*
				 * Read was successful, copy the data we just
				 * read from the device-specific cache into
				 * our temporary track buffer and update
				 * pointers accordingly.
				 */
				(void) memcpy(cp, cache_info.active_buffer,
				    dbp->cnt * ihp->dev.disk.bps);
				cp += (dbp->cnt * ihp->dev.disk.bps);
				n -= dbp->cnt;
				dbp++;
			}
		}

		/*
		 * Copy data from the track buffer back into the device
		 * cache, then free up our temporary buffers.
		 */
		(void) memcpy(cache_info.active_buffer, track,
		    cnt * ihp->dev.disk.bps);
		bkmem_free(buf, cnt * sizeof (struct d_blk));
		bkmem_free(track, cnt * ihp->dev.disk.bps);
		return (rc);
	}

	return (read_blocks(ihp, (off + ihp->dev.disk.bas) / spb, cnt / spb));
}
