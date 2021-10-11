/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sd_label.c	1.27	99/08/18 SMI"

/*
 * Routines to handle disk labels for SCSI and IDE devices
 *
 * Supports:
 *	fdisk tables
 *	Solaris BE (8-slice) vtoc's
 *	Solaris LE (16-slice) vtoc's
 */

/*
 * Includes, Declarations and Local Data
 */
#include <sys/scsi/scsi.h>
#include <sys/dkio.h>
#include <sys/vtoc.h>
#include <sys/dktp/fdisk.h>
#include <sys/scsi/targets/sddef.h>
#include "sd_ddi.h"

/*
 * Local Function Prototypes
 */

static int sd_has_max_chs_vals(struct ipart *fdp);

#if DEBUG || lint
#define	SDDEBUG
extern int sddebug;
#endif

/*
 * Ask the target device to tell us its geometry, and use this information
 * to initialize the physical geometry cache.  NOTE: this requires
 * that un->un_capacity and un->un_lbasize have already been initialized
 * for the current target and that the current values be passed as args
 * so that we don't end up ever trying to use -1 as a valid value. This
 * could happen if either value is reset while we're not holding the mutex.
 *
 * returns:
 *	TRUE	routine completed successfully. un_pgeom.g_capacity
 *		contains state; if nonzero, physical cache is valid.
 *	FALSE	routine could not complete; resource allocation problem,
 *		or SCSI error occurred.
 */
static void
sd_get_physical_geometry(struct scsi_disk *un, struct geom_cache *pgeom_p,
			int capacity, int lbasize)
{
	struct scsi_pkt *pkt, *pkt2;
	struct buf	*bp, *bp2;
	int		sector_size;
	int		nsect;
	int		nhead;
	int		ncyl;
	int		intrlv;
	int		length;
	int		spc;
	int		modesense_capacity;
	int		rpm;
	int		bd_len;
	int		mode_header_length;
	int		cdblen;
	int		i;
	struct mode_format *page3p;
	struct mode_geometry *page4p;
	struct mode_header *headerp;

	if (lbasize == 0) {
		if (ISCD(un))
			lbasize = 2048;
		else
			lbasize = DEV_BSIZE;
	}
	pgeom_p->g_secsize = (unsigned short)lbasize;

	/*
	 * allocate a buffer sufficiently large enough to contain
	 * either mode sense structure, (plus the generic mode sense
	 * header and one block descriptor).
	 */
	length = sizeof (struct mode_format) + MODE_PARAM_LENGTH;
	bp = scsi_alloc_consistent_buf(ROUTE, (struct buf *)NULL,
	    length, B_READ, NULL_FUNC, NULL);
	if (bp == NULL) {
		scsi_log(SD_DEVINFO, "sd_get_physical_geometry", CE_WARN,
		    "no bp for direct access device format geometry.\n");
		return;
	}

	cdblen = (SD_GRP1_2_CDBS(un)) ? CDB_GROUP2 : CDB_GROUP0;
	pkt = scsi_init_pkt(ROUTE, (struct scsi_pkt *)NULL, bp, cdblen,
	    un->un_cmd_stat_size, PP_LEN, PKT_CONSISTENT, NULL_FUNC, NULL);
	if (pkt == NULL) {
		scsi_log(SD_DEVINFO, "sd_get_physical_geometry:", CE_WARN,
		    "no memory for direct access device format geometry.\n");
		scsi_free_consistent_buf(bp);
		return;
	}

	/*
	 * retrieve mode sense page 3 - direct access device format geometry
	 */
	clrbuf(bp);
	pkt->pkt_address = (SD_SCSI_DEVP)->sd_address;
	pkt->pkt_flags = 0;
	((union scsi_cdb *)(pkt->pkt_cdbp))->scc_lun = pkt->pkt_address.a_lun;
	if (SD_GRP1_2_CDBS(un)) {
		(pkt->pkt_cdbp)[0] = SCMD_MODE_SENSE2;
		(pkt->pkt_cdbp)[7] = (char)(length >> 8);
		(pkt->pkt_cdbp)[8] = (char)length;
	} else {
		(pkt->pkt_cdbp)[0] = SCMD_MODE_SENSE;
		(pkt->pkt_cdbp)[4] = (char)length;
	}
	(pkt->pkt_cdbp)[2] = DAD_MODE_FORMAT; /* fill in the page number */

	for (i = 0; i < 3; i++) {
		if (sd_scsi_poll(un, pkt) == 0 && SCBP_C(pkt) == STATUS_GOOD)
			break;
		if (SCBP(pkt)->sts_chk) {
			if ((pkt->pkt_state & STATE_ARQ_DONE) == 0) {
				(void) sd_clear_cont_alleg(un, un->un_rqs);
			}
		}
	}
	if (i == 3) {
		/*
		 * only print this message under debug conditions,
		 * since mode sense is an optional command, and failure
		 * in this case does not necessarily denote an error.
		 */
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "sd_get_physical_geometry: mode sense page 3 failed\n");
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "reason: %s\tstatus: %s\n", scsi_rname(pkt->pkt_reason),
		    sd_sname(*(pkt->pkt_scbp)));
		scsi_destroy_pkt(pkt);
		scsi_free_consistent_buf(bp);
		return;
	}

	/*
	 * Determine size of Block Descriptors in order to locate the mode
	 * page data.  ATAPI devices return 0, SCSI devices should return
	 * MODE_BLK_DESC_LENGTH.
	 */
	headerp = (struct mode_header *)bp->b_un.b_addr;
	if (SD_GRP1_2_CDBS(un)) {
		register struct mode_header_grp2 *mhp;
		mhp = (struct mode_header_grp2 *)headerp;
		mode_header_length = MODE_HEADER_LENGTH_GRP2;
		bd_len = (mhp->bdesc_length_hi << 8) | mhp->bdesc_length_lo;
	} else {
		mode_header_length = MODE_HEADER_LENGTH;
		bd_len = ((struct mode_header *)headerp)->bdesc_length;
	}

	ASSERT(bd_len <= MODE_BLK_DESC_LENGTH);
	page3p = (struct mode_format *)((caddr_t)headerp + mode_header_length +
	    bd_len);
	if (page3p->mode_page.code != DAD_MODE_FORMAT) {
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "mode sense page code: %d\n", page3p->mode_page.code);
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "sd_get_physical_geometry: mode sense pg3 code mismatch\n");
		scsi_destroy_pkt(pkt);
		scsi_free_consistent_buf(bp);
		return;
	}

	/*
	 * use this physical geometry data only if both mode sense commands
	 * complete successfully; otherwise, revert to the logical geometry.
	 * So, we need to save everything in temporary variables.
	 */
	sector_size = (int)sd_stoh_short((uchar_t *)&(page3p->data_bytes_sect));

	/*
	 * 1243403: The NEC D38x7 drives don't support mode sense sector size.
	 */
	if (sector_size == 0) {
		if (ISCD(un))
			sector_size = 2048;
		else
			sector_size = DEV_BSIZE;
	} else
		sector_size &= ~(DEV_BSIZE - 1);

	nsect = (int)sd_stoh_short((uchar_t *)&(page3p->sect_track));
	intrlv = (int)sd_stoh_short((uchar_t *)&(page3p->interleave));

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "Mode Sense: Format "
	    "Parameters (page 3)\n");
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "mode page: %d\tnsect: %d"
	    "\tsector size: %d\n", page3p->mode_page.code, nsect,
	    sector_size);
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "interleave: %d\ttrack "
	    "skew: %d\tcylinder skew: %d\n", intrlv,
	    (int)sd_stoh_short((uchar_t *)&(page3p->track_skew)),
	    (int)sd_stoh_short((uchar_t *)&(page3p->cylinder_skew)));

	/*
	 * retrieve mode sense page 4 - rigid disk drive geometry
	 */
	length = sizeof (struct mode_geometry) + MODE_PARAM_LENGTH;
	bp2 = scsi_alloc_consistent_buf(ROUTE, (struct buf *)NULL,
	    length, B_READ, NULL_FUNC, NULL);
	if (bp2 == NULL) {
		scsi_log(SD_DEVINFO, "sd_get_physical_geometry", CE_WARN,
		    "no bp for rigid disk geometry.\n");
		scsi_destroy_pkt(pkt);
		scsi_free_consistent_buf(bp);
		return;
	}

	pkt2 = scsi_init_pkt(ROUTE, (struct scsi_pkt *)NULL, bp2, cdblen,
	    un->un_cmd_stat_size, PP_LEN, PKT_CONSISTENT, NULL_FUNC, NULL);
	if (pkt2 == NULL) {
		scsi_log(SD_DEVINFO, "sd_get_physical_geometry:", CE_WARN,
		    "no memory for rigid disk geometry.\n");
		scsi_destroy_pkt(pkt);
		scsi_free_consistent_buf(bp);
		scsi_free_consistent_buf(bp2);
		return;
	}

	clrbuf(bp2);
	pkt2->pkt_address = (SD_SCSI_DEVP)->sd_address;
	pkt2->pkt_flags = 0;
	((union scsi_cdb *)(pkt2->pkt_cdbp))->scc_lun = pkt2->pkt_address.a_lun;
	if (SD_GRP1_2_CDBS(un)) {
		(pkt2->pkt_cdbp)[0] = SCMD_MODE_SENSE2;
		(pkt2->pkt_cdbp)[7] = (char)(length >> 8);
		(pkt2->pkt_cdbp)[8] = (char)length;
	} else {
		(pkt2->pkt_cdbp)[0] = SCMD_MODE_SENSE;
		(pkt2->pkt_cdbp)[4] = (char)length;
	}
	(pkt2->pkt_cdbp)[2] = DAD_MODE_GEOMETRY; /* set page number */

	for (i = 0; i < 3; i++) {
		if (sd_scsi_poll(un, pkt2) == 0 && SCBP_C(pkt2) == STATUS_GOOD)
			break;
		if (SCBP(pkt2)->sts_chk) {
			if ((pkt2->pkt_state & STATE_ARQ_DONE) == 0) {
				(void) sd_clear_cont_alleg(un, un->un_rqs);
			}
		}
	}
	if (i == 3) {
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "sd_get_physical_geometry: mode sense page 4 failed.\n");
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "reason: %s\tstatus: %s\n", scsi_rname(pkt2->pkt_reason),
		    sd_sname(*(pkt2->pkt_scbp)));
		scsi_destroy_pkt(pkt);
		scsi_free_consistent_buf(bp);
		scsi_destroy_pkt(pkt2);
		scsi_free_consistent_buf(bp2);
		return;
	}

	/*
	 * Determine size of Block Descriptors in order to locate the mode
	 * page data.  ATAPI devices return 0, SCSI devices should return
	 * MODE_BLK_DESC_LENGTH.
	 */
	headerp = (struct mode_header *)bp2->b_un.b_addr;
	if (SD_GRP1_2_CDBS(un)) {
		register struct mode_header_grp2 *mhp;
		mhp = (struct mode_header_grp2 *)headerp;
		bd_len = (mhp->bdesc_length_hi << 8) | mhp->bdesc_length_lo;
	} else {
		bd_len = ((struct mode_header *)headerp)->bdesc_length;
	}
	ASSERT(bd_len <= MODE_BLK_DESC_LENGTH);
	page4p = (struct mode_geometry *)((caddr_t)headerp +
	    mode_header_length + bd_len);
	if (page4p->mode_page.code != DAD_MODE_GEOMETRY) {
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "mode sense page code: %d\n", page4p->mode_page.code);
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "sd_get_physical_geometry: mode sense pg4 code mismatch\n");
		scsi_destroy_pkt(pkt);
		scsi_free_consistent_buf(bp);
		scsi_destroy_pkt(pkt2);
		scsi_free_consistent_buf(bp2);
		return;
	}

	/*
	 * stash the data now, after we know that both commands completed.
	 */
	nhead = (int)page4p->heads;
	spc = nhead * nsect;
	ncyl = (page4p->cyl_ub << 16) + (page4p->cyl_mb << 8) +
	    page4p->cyl_lb;
	rpm = (int)sd_stoh_short((uchar_t *)&(page4p->rpm));
	modesense_capacity = spc * ncyl;

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "Mode Sense Geometry "
	    "Parameters (page 4)\n");
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "cylinders: %d\theads: %d"
	    "\trpm: %d\tcomputed capacity(h*s*c): %d\n", ncyl, nhead, rpm,
	    modesense_capacity);
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "pgeom_p: %p\tread "
	    "capacity: %d\n", (void *)pgeom_p, capacity);

	/*
	 * compensate if the drive's geometry is not rectangular, i.e.,
	 * the product of C * H * S returned by mode sense >= that returned
	 * by read capacity. This is an idiosyncracy of the original x86
	 * disk subsystem.
	 */
	if (modesense_capacity >= capacity) {

		SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG,
		    "sd_get_physical_geometry: adjusting acyl\n");
		SD_DEBUG2(SD_DEVINFO, sd_label, SCSI_DEBUG, "old acyl: %d\t"
		    "new acyl: %d\n", pgeom_p->g_acyl,
		    (modesense_capacity - capacity + spc - 1) / spc);

		if (sector_size != 0) {
			/* 1243403: NEC D38x7 drives don't support sec size */
			pgeom_p->g_secsize = (unsigned short)sector_size;
		}
		pgeom_p->g_nsect = (unsigned short)nsect;
		pgeom_p->g_nhead = (unsigned short)nhead;
		pgeom_p->g_capacity = capacity;
		pgeom_p->g_acyl = (modesense_capacity - pgeom_p->g_capacity +
		    spc - 1) / spc;
		pgeom_p->g_ncyl = ncyl - pgeom_p->g_acyl;
	}
	pgeom_p->g_rpm = (unsigned short)rpm;
	pgeom_p->g_intrlv = (unsigned short)intrlv;

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "physical (mode "
	    "sense) geometry reported by device:\n");
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "nsect: %d\tsector "
	    "size: %d\tinterlv: %d\n", nsect, sector_size, intrlv);
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "nhead: %d\tncyl: %d\trpm: "
	    "%d\tcapacity(ms): %d\n", nhead, ncyl, rpm, modesense_capacity);

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "sd_get_physical_"
	    "geometry: (cached)\n");
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "ncyl: %ld\tacyl: "
	    "%d\tnhead: %d\tnsect: %d\n", un->un_pgeom.g_ncyl,
	    un->un_pgeom.g_acyl, un->un_pgeom.g_nhead, un->un_pgeom.g_nsect);
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "lbasize: %d\t"
	    "capacity: %ld\tintrlv: %d\trpm: %d\n", un->un_pgeom.g_secsize,
	    un->un_pgeom.g_capacity, un->un_pgeom.g_intrlv, un->un_pgeom.g_rpm);

	scsi_destroy_pkt(pkt);
	scsi_free_consistent_buf(bp);
	scsi_destroy_pkt(pkt2);
	scsi_free_consistent_buf(bp2);
}

/*
 * Ask the controller to tell us about the target device.
 */
static void
sd_get_virtual_geometry(struct scsi_disk *un, int capacity, int lbasize)
{
	struct geom_cache *lgeom_p = &un->un_lgeom;
	uint_t		geombuf;
	int		spc;

	ASSERT(mutex_owned(SD_MUTEX));

	/* set sector size, and total number of sectors */
	(void) scsi_ifsetcap(ROUTE, "sector-size", lbasize, 1);
	(void) scsi_ifsetcap(ROUTE, "total-sectors", capacity, 1);

	/* let the HBA tell us its geometry */
	geombuf = (uint_t)scsi_ifgetcap(ROUTE, "geometry", 1);
#define	UNDEFINED	-1
	if (geombuf == UNDEFINED)
		return;

	/*
	 * initialize the logical geometry cache.
	 */
	lgeom_p->g_nhead = (geombuf >> 16) & 0xffff;
	lgeom_p->g_nsect = geombuf & 0xffff;
	lgeom_p->g_secsize = DEV_BSIZE;

	lgeom_p->g_capacity = capacity << un->un_blkshf;
	spc = lgeom_p->g_nhead * lgeom_p->g_nsect;
	lgeom_p->g_ncyl = lgeom_p->g_capacity / spc;
	lgeom_p->g_acyl = 0;

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "sd_get_virtual_geometry: "
	    "(cached)\n");
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "ncyl: %ld\tacyl: %d\tnhead:"
	    " %d\tnsect: %d\n", un->un_lgeom.g_ncyl, un->un_lgeom.g_acyl,
	    un->un_lgeom.g_nhead, un->un_lgeom.g_nsect);
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "lbasize: %d\tcapacity: "
	    "%ld\tintrlv: %d\trpm: %d\n", un->un_lgeom.g_secsize,
	    un->un_lgeom.g_capacity, un->un_lgeom.g_intrlv,
	    un->un_lgeom.g_rpm);
}

/*
 * (re)initialize both geometry caches; the virtual geometry information is
 * extracted from the HBA (the "geometry" capability), and the physical
 * geometry cache data is generated by issuing mode sense commands.
 */
void
sd_resync_geom_caches(struct scsi_disk *un, int capacity, int lbasize)
{
	struct geom_cache pgeom;
	struct geom_cache *pgeom_p = &pgeom;
	int spc;
	unsigned short nhead;
	unsigned short nsect;

	ASSERT(mutex_owned(SD_MUTEX));


	/*
	 * ask the controller for its logical geometry.
	 * NOTE: if the HBA does not support scsi_ifgetcap("geometry"),
	 * then the lgeom cache will be invalid.
	 */
	sd_get_virtual_geometry(un, capacity, lbasize);

	/*
	 * If either mode sense command fails, the pgeom cache is
	 * invalid; this is not necessarily an error, though, because
	 * mode sense implementation is optional.
	 *
	 * Initialize the pgeom cache from lgeom, so that if
	 * mode sense doesn't work, DKIOCG_PHYSGEOM can
	 * return reasonable values.
	 */

	if (un->un_lgeom.g_nsect == 0 || un->un_lgeom.g_nhead == 0) {
		/*
		 * XXX - perhaps this needs to be more adaptive?
		 * The rationale is that, if there's no HBA geometry
		 * from the HBA driver, any old guess is as good as any
		 * here, since this is the physical geometry.  If mode
		 * sense fails, this gives a max cylinder size for
		 * non-LBA access.
		 */
		nhead = 255;
		nsect = 63;
	} else {
		nhead = un->un_lgeom.g_nhead;
		nsect = un->un_lgeom.g_nsect;
	}
	if (ISCD(un)) {
		pgeom_p->g_nhead = 1;
		pgeom_p->g_nsect = nsect * nhead;
	} else {
		pgeom_p->g_nhead = nhead;
		pgeom_p->g_nsect = nsect;
	}
	spc = pgeom_p->g_nhead * pgeom_p->g_nsect;
	pgeom_p->g_capacity = capacity;
	pgeom_p->g_ncyl = pgeom_p->g_capacity / spc;
	pgeom_p->g_acyl = 0;

	/*
	 * retrieve fresh geometry data from the hardware, stash it
	 * here temporarily before we rebuild the incore label.
	 *
	 * We want to use the mode sense commands to derive the
	 * physical geometry of the device, but if either command
	 * fails, the logical geometry is used as the fallback for
	 * disk label geometry.
	 */
	mutex_exit(SD_MUTEX);
	sd_get_physical_geometry(un, pgeom_p, capacity, lbasize);
	mutex_enter(SD_MUTEX);

	/*
	 * Now update the real copy while holding the mutex. This
	 * way the global copy is never in an inconsistent state.
	 */
	bcopy(pgeom_p, &un->un_pgeom,  sizeof (un->un_pgeom));

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "sd_get_physical_"
	    "geometry: (cached from lgeom)\n");
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "ncyl: %ld\tacyl: "
	    "%d\tnhead: %d\tnsect: %d\n", un->un_pgeom.g_ncyl,
	    un->un_pgeom.g_acyl, un->un_pgeom.g_nhead, un->un_pgeom.g_nsect);
	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "lbasize: %d\t"
	    "capacity: %ld\tintrlv: %d\trpm: %d\n", un->un_pgeom.g_secsize,
	    un->un_pgeom.g_capacity, un->un_pgeom.g_intrlv, un->un_pgeom.g_rpm);
}

/*
 * generate a default label for those devices that do not have one, e.g.,
 * new media, removable cartridges, etc..
 */
void
sd_build_default_label(struct scsi_disk *un)
{
	uint_t	phys_spc;
	uint_t	disksize;

	ASSERT(mutex_owned(SD_MUTEX));

#if defined(_SUNOS_VTOC_8)
	if (!ISREMOVABLE(un))
		return;

	/*
	 * It's REMOVABLE media, therefore no label (on sparc, anyway).
	 *
	 * For the number of sectors per track, we put the maximum number.
	 * For rpm, we use the minimum number for the disk.
	 */
	un->un_g.dkg_ncyl = 1;
	un->un_g.dkg_acyl = 0;
	un->un_g.dkg_bcyl = 0;
	un->un_g.dkg_nhead = 1;
	un->un_g.dkg_nsect = un->un_capacity;
	un->un_g.dkg_rpm = 200;
	un->un_asciilabel[0] = '\0';

	un->un_map[0].dkl_cylno = 0;
	un->un_map[0].dkl_nblk = un->un_capacity;
	un->un_map[2].dkl_cylno = 0;
	un->un_map[2].dkl_nblk = un->un_capacity;

#elif defined(_SUNOS_VTOC_16)

	bzero(&un->un_g, sizeof (struct dk_geom));
	bzero(&un->un_vtoc, sizeof (struct dk_vtoc));
	bzero(&un->un_map, NDKMAP * (sizeof (struct dk_map)));

	if (un->un_solaris_size == 0) {
		/*
		 * got fdisk table but no solaris entry therefore
		 * don't create a default label
		 */
		un->un_gvalid = TRUE;
		return;
	}

	phys_spc = un->un_pgeom.g_nhead * un->un_pgeom.g_nsect;
	un->un_g.dkg_pcyl = un->un_solaris_size / phys_spc;
	un->un_g.dkg_acyl = DK_ACYL;
	un->un_g.dkg_ncyl = un->un_g.dkg_pcyl - DK_ACYL;
	disksize = un->un_g.dkg_ncyl * phys_spc;

	if (ISCD(un)) {
		/*
		 * CD's don't use the "heads * sectors * cyls"-type of geometry,
		 * but instead use the entire capacity of the media.
		 */
		disksize = un->un_solaris_size;
		un->un_g.dkg_nhead = 1;
		un->un_g.dkg_nsect = 1;
		un->un_g.dkg_rpm =
		    (un->un_pgeom.g_rpm == 0) ? 200 : un->un_pgeom.g_rpm;

		un->un_vtoc.v_part[0].p_start = 0;
		un->un_vtoc.v_part[0].p_size = disksize;
		un->un_vtoc.v_part[0].p_tag = V_BACKUP;
		un->un_vtoc.v_part[0].p_flag = V_UNMNT;

		un->un_map[0].dkl_cylno = 0;
		un->un_map[0].dkl_nblk = disksize;
		un->un_offset[0] = 0;

	} else {	/* hard disks and removable media cartridges */
		un->un_g.dkg_nhead = un->un_pgeom.g_nhead;
		un->un_g.dkg_nsect = un->un_pgeom.g_nsect;
		un->un_g.dkg_rpm =
		    (un->un_pgeom.g_rpm == 0) ? 3600: un->un_pgeom.g_rpm;
		un->un_vtoc.v_sectorsz = DEV_BSIZE;

		/* Add boot slice */
		un->un_vtoc.v_part[8].p_start = 0;
		un->un_vtoc.v_part[8].p_size = phys_spc;
		un->un_vtoc.v_part[8].p_tag = V_BOOT;
		un->un_vtoc.v_part[8].p_flag = V_UNMNT;

		un->un_map[8].dkl_cylno = 0;
		un->un_map[8].dkl_nblk = phys_spc;
		un->un_offset[8] = 0;

		/* Add alts slice */
		un->un_vtoc.v_part[9].p_start = phys_spc;
		un->un_vtoc.v_part[9].p_size = 2 * phys_spc;
		un->un_vtoc.v_part[9].p_tag = V_ALTSCTR;
		un->un_vtoc.v_part[9].p_flag = 0;

		un->un_map[9].dkl_cylno = 1;
		un->un_map[9].dkl_nblk = 2 * phys_spc;
		un->un_offset[9] = phys_spc;
	}

	un->un_g.dkg_apc = 0;
	un->un_vtoc.v_nparts = V_NUMPAR;

	/* Add backup slice */
	un->un_vtoc.v_part[2].p_start = 0;
	un->un_vtoc.v_part[2].p_size = disksize;
	un->un_vtoc.v_part[2].p_tag = V_BACKUP;
	un->un_vtoc.v_part[2].p_flag = V_UNMNT;

	un->un_map[2].dkl_cylno = 0;
	un->un_map[2].dkl_nblk = disksize;
	un->un_offset[2] = 0;

	un->un_vtoc.v_sanity = VTOC_SANE;
	un->un_vtoc.v_version = V_VERSION;

	(void) sprintf(un->un_vtoc.v_asciilabel, "DEFAULT cyl %d alt %d"
	    " hd %d sec %d", un->un_g.dkg_ncyl, un->un_g.dkg_acyl,
	    un->un_g.dkg_nhead, un->un_g.dkg_nsect);

#else
#error "No VTOC format defined."
#endif

	un->un_g.dkg_read_reinstruct = 0;
	un->un_g.dkg_write_reinstruct = 0;

	un->un_g.dkg_intrlv = 1;
	un->un_gvalid = TRUE;

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "Default label created: "
	    "cyl: %d\tacyl: %d\tnhead: %d\tnsect: %d\tcap: %d\n",
	    un->un_g.dkg_ncyl, un->un_g.dkg_acyl, un->un_g.dkg_nhead,
	    un->un_g.dkg_nsect, un->un_capacity);
}

/*
 * sd_read_fdisk: utility routine to read the fdisk table.
 *
 * returns:
 * 	FDISK_ERROR
 *		routine could not complete; resource allocation problem,
 *		or read error occurred.
 * 	FDISK_RESV_CONFLICT
 * 		read got a reservation conflict, so we give up
 * 	FDISK_SUCCESS
 *		routine completed successfully
 */
int
sd_read_fdisk(struct scsi_disk	*un, int (*f)(), int capacity, int lbasize)
{
	struct buf		*bp;
	struct ipart		*fdp;
	struct mboot		*mbp;
	struct ipart		fdisk[FD_NUMPART];
	int			i;
	char			sigbuf[2];
	int			iopb_retval;
	caddr_t			bufp;
	int			uidx;
	int			rval;
	int			lba;
	uint_t			solaris_offset;	/* offset to solaris part. */
	daddr_t			solaris_size;	/* size of solaris partition */

	ASSERT(mutex_owned(SD_MUTEX));

#if defined(_NO_FDISK_PRESENT)
	un->un_solaris_offset = 0;
	un->un_solaris_size = capacity;
	bzero(un->un_fmap, sizeof (struct fmap) * FD_NUMPART);
	return (FDISK_SUCCESS);

#elif defined(_FIRMWARE_NEEDS_FDISK)

	/*
	 * start off assuming no FDISK table
	 */
	solaris_offset = 0;
	solaris_size = capacity;

	mutex_exit(SD_MUTEX);
	bp = scsi_alloc_consistent_buf(ROUTE,
	    (struct buf *)NULL, lbasize, B_READ, f, NULL);
	mutex_enter(SD_MUTEX);
	if (!bp) {
		scsi_log(SD_DEVINFO, sd_label, CE_WARN, "no bp for fdisk\n");
		return (FDISK_NOMEM);
	}

	iopb_retval = sd_iopb_read_block(un, 0, bp, &bufp, f);

	if (iopb_retval == IOPB_NOMEM) {
		scsi_log(SD_DEVINFO, sd_label, CE_WARN,
			    "no memory for fdisk\n");
		scsi_free_consistent_buf(bp);
		return (FDISK_NOMEM);
	}

	if (iopb_retval == IOPB_ERROR) {
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "fdisk read error");
		scsi_free_consistent_buf(bp);
		return (FDISK_ERROR);
	}

	if (iopb_retval == IOPB_RESV_CONFLICT) {
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG,
			"fdisk reservation conflict");
		return (FDISK_RESV_CONFLICT);
	}

	mbp = (struct mboot *)bufp;

	/*
	 * The fdisk table does not begin on a 4-byte
	 * boundary within the master boot record, so we
	 * copy it to an aligned structure to avoid
	 * alignment exceptions on some processors.
	 */
	bcopy(&mbp->parts[0], fdisk, sizeof (fdisk));

	/*
	 * Check for lba support before verifying sig; sig might not be
	 * there, say on a blank disk, but the max_chs mark may still
	 * be present
	 */

	/*
	 * First, check for lba-access-ok on root node (or prom root node)
	 * if present there, don't need to search fdisk table
	 */

	if (ddi_getprop(DDI_DEV_T_ANY, ddi_root_node(), 0,
	    "lba-access-ok", 0) != 0) {
		/* all drives do LBA; don't search fdisk table */
		lba = 1;
	} else {
		/* okay, look for mark in fdisk table */
		for (fdp = fdisk, i = 0; i < FD_NUMPART; i++, fdp++) {
			/* accumulate "lba" value from all partitions */
			lba = (lba || sd_has_max_chs_vals(fdp));
		}
	}

	/*
	 * Next, look for 'no-bef-lba-access' prop on parent.
	 * Its presence means the realmode driver doesn't support
	 * LBA, so the target driver shouldn't advertise it as ok.
	 * This should be a temporary condition; one day all
	 * BEFs should support the LBA access functions.
	 */
	if (lba && (ddi_getprop(DDI_DEV_T_ANY, ddi_get_parent(SD_DEVINFO),
		    DDI_PROP_DONTPASS, "no-bef-lba-access", 0) != 0)) {
		/* BEF doesn't support LBA; don't advertise it as ok */
		lba = 0;
	}

	if (lba) {
		dev_t dev;

		dev = makedevice(ddi_name_to_major(ddi_get_name(SD_DEVINFO)),
		    ddi_get_instance(SD_DEVINFO) << SDUNIT_SHIFT);

		if (ddi_getprop(dev, SD_DEVINFO, DDI_PROP_DONTPASS,
		    "lba-access-ok", 0) == 0) {
			/* not found; create it */
			if (ddi_prop_create(dev, SD_DEVINFO, 0,
			    "lba-access-ok", (caddr_t)NULL, 0) !=
			    DDI_PROP_SUCCESS) {
				cmn_err(CE_CONT,
				    "?sd: Can't create lba property "
				    "for instance %d\n",
				    ddi_get_instance(SD_DEVINFO));
			}
		}
	}

	bcopy(&mbp->signature, sigbuf, sizeof (sigbuf));

	/*
	 * endian-independent signature check
	 */
	if ((sigbuf[1] & 0xFF) != ((MBB_MAGIC >> 8) & 0xFF) ||
	    sigbuf[0] != (MBB_MAGIC & 0xFF)) {
		SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "fdisk not present");
		bzero(un->un_fmap, sizeof (struct fmap) * FD_NUMPART);
		rval = FDISK_SUCCESS;
		goto done;
	}

#ifdef	DEBUG
	if (sddebug) {
		fdp = fdisk;
		cmn_err(CE_CONT, "fdisk:\n");
		cmn_err(CE_CONT, "         relsect    "
			"numsect         sysid       bootid\n");
		for (i = 0; i < FD_NUMPART; i++, fdp++) {
			cmn_err(CE_CONT,
				"    %d:  %8d   %8d     0x%08x     0x%08x\n",
				i, fdp->relsect, fdp->numsect,
				fdp->systid, fdp->bootid);
		}
	}
#endif

	/*
	 * Try to find the unix partition
	 */
	uidx = -1;
	solaris_offset = 0;
	solaris_size = 0;
	for (fdp = fdisk, i = 0; i < FD_NUMPART; i++, fdp++) {
		int	relsect;
		int	numsect;
		if (fdp->numsect == 0) {
			un->un_fmap[i].fmap_start = 0;
			un->un_fmap[i].fmap_nblk = 0;
			continue;
		}
		relsect = sd_letoh_int((uchar_t *)&(fdp->relsect));
		numsect = sd_letoh_int((uchar_t *)&(fdp->numsect));
		un->un_fmap[i].fmap_start = relsect;
		un->un_fmap[i].fmap_nblk = numsect;

		if (fdp->systid != SUNIXOS)
			continue;
		if (uidx != -1 && fdp->bootid != ACTIVE)
			continue;

		uidx = i;
		solaris_offset = un->un_fmap[uidx].fmap_start;
		solaris_size = un->un_fmap[uidx].fmap_nblk;
	}

	SD_DEBUG(SD_DEVINFO, sd_label, SCSI_DEBUG, "fdisk 0x%x 0x%lx",
		    un->un_solaris_offset, un->un_solaris_size);

	rval = FDISK_SUCCESS;

done:
	scsi_free_consistent_buf(bp);

	/*
	 * Clear the VTOC info, only if the Solaris partition entry
	 * has moved, changed size, been deleted, or if the size of
	 * the partition is too small to even fit the label sector.
	 */
	if (un->un_solaris_offset != solaris_offset ||
	    un->un_solaris_size != solaris_size ||
	    solaris_size <= DK_LABEL_LOC) {
		bzero(&un->un_g, sizeof (struct dk_geom));
		bzero(&un->un_vtoc, sizeof (struct dk_vtoc));
		bzero(&un->un_map, NDKMAP * (sizeof (struct dk_map)));
		un->un_gvalid = FALSE;
	}
	un->un_solaris_offset = solaris_offset;
	un->un_solaris_size = solaris_size;
	return (rval);

#else
#error "Fdisk table presence undetermined for this platform."
#endif
}

/* max CHS values, as they are encoded into bytes, for 1022/254/63 */
#define	LBA_MAX_SECT	(63 | ((1022 & 0x300) >> 2))
#define	LBA_MAX_CYL	(1022 & 0xFF)
#define	LBA_MAX_HEAD	(254)

static int
sd_has_max_chs_vals(struct ipart *fdp)
{
	return (fdp->begcyl == LBA_MAX_CYL &&
	    fdp->beghead == LBA_MAX_HEAD &&
	    fdp->begsect == LBA_MAX_SECT &&
	    fdp->endcyl == LBA_MAX_CYL &&
	    fdp->endhead == LBA_MAX_HEAD &&
	    fdp->endsect == LBA_MAX_SECT);
}

/*
 * sd_iopb_read_block: utility routine
 *
 * returns:
 * 	IOPB_ERROR
 * 		some unrecoverable error while reading the block, except for
 * 	IOPB_RESV_CONFLICT
 * 		reservation conflict while reading the block
 * 	IOPB_SUCCESS
 * 		successful block read; data left in bp
 *
 */
int
sd_iopb_read_block(struct scsi_disk *un, daddr_t blkno, struct buf *bp,
    caddr_t *bufp, int (*f)())
{
	register struct scsi_pkt	*pkt;
	register daddr_t		addr;
	register int			cdblen;
	int				rval = IOPB_ERROR;
	int				i;

	ASSERT(mutex_owned(SD_MUTEX));

	addr = blkno >> un->un_blkshf;
	*bufp = NULL;

	if ((addr >= (2 << 20)) || SD_GRP1_2_CDBS(un)) {
		cdblen = CDB_GROUP1;
	} else {
		cdblen = CDB_GROUP0;
	}

	mutex_exit(SD_MUTEX);
	pkt = scsi_init_pkt(ROUTE, (struct scsi_pkt *)NULL,
		bp, cdblen, 1, PP_LEN, PKT_CONSISTENT, f, NULL);
	mutex_enter(SD_MUTEX);
	if (!pkt) {
		return (IOPB_NOMEM);
	}

	clrbuf(bp);
	if (cdblen == CDB_GROUP0) {
		(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
			SCMD_READ, addr, 1, 0);
		FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
		pkt->pkt_flags = FLAG_NOPARITY;
	} else {
		(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
			SCMD_READ_G1, addr, 1, 0);
		FILL_SCSI1_LUN(SD_SCSI_DEVP, pkt);
		pkt->pkt_flags = FLAG_NOPARITY;
	}

	/*
	 * Ok, read the block via polling
	 */
	mutex_exit(SD_MUTEX);
	for (i = 0; i < 3; i++) {
		if (sd_scsi_poll(un, pkt) || SCBP_C(pkt) != STATUS_GOOD ||
		    (pkt->pkt_state & STATE_XFERRED_DATA) == 0 ||
		    (pkt->pkt_resid != 0)) {
			if (SCBP_C(pkt) == STATUS_RESERVATION_CONFLICT) {
				rval = IOPB_RESV_CONFLICT;
				break;
			}
			if (i > 2 && un->un_state != SD_STATE_NORMAL) {
				scsi_log(SD_DEVINFO, sd_label, CE_WARN,
				    "unable to read block %ld\n", blkno);
			}
			if (SCBP(pkt)->sts_chk) {
				if ((pkt->pkt_state & STATE_ARQ_DONE) == 0) {
					(void) sd_clear_cont_alleg(un,
								    un->un_rqs);
				}
			}
		} else {
			*bufp = bp->b_un.b_addr + ((blkno &
			    ((1 << un->un_blkshf) - 1)) << DEV_BSHIFT);
			rval = IOPB_SUCCESS;
			break;
		}
	}
	mutex_enter(SD_MUTEX);

	scsi_destroy_pkt(pkt);
	return (rval);
}
