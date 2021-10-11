/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pcdisk.c	1.37	99/10/28 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/dditypes.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/open.h>
#include <sys/file.h>
#include <sys/cmn_err.h>
#include <sys/varargs.h>
#include <sys/fs/pc_label.h>

#include <sys/hdio.h>
#include <sys/dkio.h>
#include <sys/dktp/dadkio.h>

#include <sys/dklabel.h>

#include <sys/vtoc.h>


#include <sys/types.h>
#include <sys/conf.h>
#include <sys/dditypes.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/dktp/cm.h>

#include <sys/dktp/fdisk.h>

#include <sys/pccard.h>
#include <sys/pcmcia/pcata.h>

static int pcata_redo_vtoc(ata_soft_t *softp, buf_t *fdiskbp);
static buf_t *pcata_lblk_alloc(dev_t dev);


/*
 * Queue a request and call start routine.
 *
 * If the request is not a special buffer request,
 * do validation on it and generate both an absolute
 * block number (which we will leave in b_resid),
 * and a actual block count value (which we will
 * leave in av_back).
 */

int
pcata_strategy(buf_t *bp)
{
	ata_soft_t	*softp;
	ata_unit_t	*unitp;
	void		*instance;
	daddr_t		blkno;
	int		part;
	int		ret;

#ifdef ATA_DEBUG
	if (pcata_debug & DIO)
		cmn_err(CE_CONT, "_strategy\n");
#endif
	bp->b_resid = bp->b_bcount;

	if (pcata_getinfo(NULL, DDI_INFO_DEVT2INSTANCE, (void *)bp->b_edev,
	    &instance) != DDI_SUCCESS) {
		cmn_err(CE_CONT, "_strategy: pcata_getinfo ENODEV\n");
		bioerror(bp, ENODEV);
		biodone(bp);
		return (0);
	}

	if (!(softp = ddi_get_soft_state(pcata_soft, (int)instance))) {
		bioerror(bp, ENXIO);
		biodone(bp);
		return (0);
	}

	if (!(CARD_PRESENT_VALID(softp))) {
#ifdef ATA_DEBUG
		if (pcata_debug & DIO)
			cmn_err(CE_CONT, "_strategy card_state = %d bp=%p\n",
				softp->card_state,
				(void *)bp);
#endif
		bioerror(bp, ENXIO);
		biodone(bp);
		return (0);
	}

	if (bp->b_bcount & (NBPSCTR-1)) {
		bioerror(bp, ENXIO);
		biodone(bp);
		return (0);
	}

	/*
	 * pointer to structure for physical drive
	 */
	/*
	 * XXX/lcl since we don't traverse a_forw with some bits from minor
	 * (aka the UNIT macro) this means only 1 physical disk
	 * this error occurs everywhere ab_link is used!
	 */
	unitp = softp->ab_link;
	if (!unitp) {
		bioerror(bp, ENXIO);
		biodone(bp);
		return (0);
	}

#ifdef	ATA_DEBUG
	if (pcata_debug & DIO) {
		cmn_err(CE_CONT, "_strategy: bp->b_private = %p\n",
			(void *)bp->b_private);
		cmn_err(CE_CONT, "_strategy %s request for buf: %p\n",
			bp->b_flags & B_READ ? "read" : "write", (void *)bp);
	}
#endif

	/*
	 * A normal read/write command.
	 *
	 * If the transfer size would take it past the end of the
	 * partition, trim it down. Also trim it down to a multiple
	 * of the block size.
	 */
	bp->b_flags &= ~(B_DONE|B_ERROR);
	bp->av_forw = NULL;
	blkno = bp->b_blkno;
	part = LPART(bp->b_edev);

	mutex_enter(&softp->ata_mutex);

	/*
	 * Map block number within partition to absolute
	 * block number.
	 */
#ifdef ATA_DEBUG
	if (pcata_debug & DIO)
		cmn_err(CE_CONT, "_strategy  "
			"%c%d: %s block %ld mapped to %ld dev %lx\n",
			(part > 15 ? 'p' : 's'),
			(part > 15 ? part - 16 : part),
			bp->b_flags & B_READ ? "read" : "write",
			blkno,
			blkno + unitp->lbl.pmap[part].p_start,
			bp->b_edev);
#endif

	/* make sure this partition exists */
	if (unitp->lbl.pmap[part].p_size == 0) {
#ifdef ATA_DEBUG
		cmn_err(CE_CONT, "_strategy:invalid slice part=%d\n", part);
#endif
		mutex_exit(&softp->ata_mutex);
		bioerror(bp, ENXIO);
		biodone(bp);
		return (0);
	}

	/* make sure the I/O begins at a block within the partition */
	if (blkno < 0 || blkno >= unitp->lbl.pmap[part].p_size) {
#ifdef ATA_DEBUG
		cmn_err(CE_CONT, "_strategy:block number out of range\n");
#endif
		mutex_exit(&softp->ata_mutex);
		bioerror(bp, ENXIO);
		biodone(bp);
		return (0);
	}

	/* XXX/lcl check to make sure I/O doesn't go past end of partition */

	/* put block number into b_resid and number of blocks into av_back */
	bp->b_resid = bp->b_bcount;
	bp->av_back = (buf_t *)(ROUNDUP(bp->b_bcount, NBPSCTR) >> SCTRSHFT);

	blkno += unitp->lbl.pmap[part].p_start;

	ret = pcata_start(unitp, bp, blkno);
	mutex_exit(&softp->ata_mutex);

	if (ret != CTL_SEND_SUCCESS) {
		bp->b_resid = bp->b_bcount;
#ifdef ATA_DEBUG
		cmn_err(CE_CONT, "_strategy: ata_start failed bp 0x%p\n",
			(void *)bp);
#endif
		bioerror(bp, EIO);
		biodone(bp);
		return (0);
	}

	/*
	 * If the disk block to be written to is disk block 0, it would
	 * mean the partition table is changing from underneath us
	 * we shoud trap and update the in memory image.
	 * By now the buffer is mapped in and we should be able to
	 * use the contents as the new fdisk partition.
	 */
	if ((bp->b_flags & B_WRITE) && ((bp->b_flags & B_ERROR) != B_ERROR) &&
		blkno == 0) {
		if (pcata_redo_vtoc(softp, bp)) {
			bioerror(bp, EFAULT);
			biodone(bp);
			return (0);
		}
	}

	return (0);
}

/*
 * This routine implements the ioctl calls for the ATA
 */
#define	COPYOUT(a, b, c, f)	\
	ddi_copyout((caddr_t)(a), (caddr_t)(b), sizeof (c), f)
#define	COPYIN(a, b, c, f)	\
	ddi_copyin((caddr_t)(a), (caddr_t)(b), sizeof (c), f)

/* ARGSUSED3 */
int
pcata_ioctl(
	dev_t dev,
	int cmd,
	intptr_t arg,
	int flag,
	cred_t *cred_p,
	int *rval_p)
{
	uint32_t	data[512 / (sizeof (uint32_t))];
	void		*instance;
	ata_soft_t	*softp;
	ata_unit_t	*unitp;
	struct dk_cinfo *info;
	int		i, status;

#ifdef ATA_DEBUG
	if (pcata_debug & DIO) cmn_err(CE_CONT, "_ioctl\n");
#endif
	if (pcata_getinfo(NULL, DDI_INFO_DEVT2INSTANCE, (void *)dev,
	    &instance) != DDI_SUCCESS)
		return (ENODEV);

	if (!(softp = ddi_get_soft_state(pcata_soft, (int)instance))) {
		return (ENXIO);
	}

	if (!(CARD_PRESENT_VALID(softp))) {
		return (ENODEV);
	}

#ifdef ATA_DEBUG
	if (pcata_debug & DENT) {
		char	*cmdname;

		switch (cmd) {
		case DKIOCINFO:		cmdname = "DKIOCINFO       "; break;
		case DKIOCGGEOM:	cmdname = "DKIOCGGEOM      "; break;
		case DKIOCGAPART:	cmdname = "DKIOCGAPART     "; break;
		case DKIOCSAPART:	cmdname = "DKIOCSAPART     "; break;
		case DKIOCGVTOC:	cmdname = "DKIOCGVTOC      "; break;
		case DKIOCSVTOC:	cmdname = "DKIOCSVTOC      "; break;
		case DKIOCG_VIRTGEOM:	cmdname = "DKIOCG_VIRTGEOM "; break;
		case DKIOCG_PHYGEOM:	cmdname = "DKIOCG_PHYGEOM  "; break;
		case DKIOCEJECT:	cmdname = "DKIOCEJECT     *"; break;
		case DKIOCSGEOM:	cmdname = "DKIOCSGEOM     *"; break;
		case DKIOCSTATE:	cmdname = "DKIOCSTATE     *"; break;
		case DKIOCADDBAD:	cmdname = "DKIOCADDBAD    *"; break;
		case DKIOCGETDEF:	cmdname = "DKIOCGETDEF    *"; break;
		case DKIOCPARTINFO:	cmdname = "DKIOCPARTINFO  *"; break;
		case DIOCTL_RWCMD:	cmdname = "DIOCTL_RWCMD    "; break;
		default:		cmdname = "UNKNOWN        *"; break;
		}
		cmn_err(CE_CONT,
			"_ioctl%d: cmd %x(%s) arg %p softp %p\n",
			(int)instance, cmd, cmdname, (void *)arg,
			(void *)softp);
	}
#endif

	/*
	 * we can respond to get geom ioctl() only while the driver has
	 * not completed initialization.
	 */
	if ((softp->flags & PCATA_READY) == 0 && cmd != DKIOCG_PHYGEOM) {
		(void) pcata_readywait(softp);
		if (!(softp->flags & PCATA_READY))
			return (EFAULT);
	}

	ASSERT(softp->ab_link);
	unitp = softp->ab_link;
	bzero((caddr_t)data, sizeof (data));

	switch (cmd) {
	case DKIOCGGEOM:
	case DKIOCSGEOM:
	case DKIOCGAPART:
	case DKIOCSAPART:
	case DKIOCGVTOC:
	case DKIOCSVTOC:
		status = 0;
		mutex_enter(&unitp->lbl.mutex);
		status = pcata_lbl_ioctl(dev, cmd, arg, flag);
		mutex_exit(&unitp->lbl.mutex);
		return (status);
	}

	switch (cmd) {
	case DKIOCSTATE:
		{
			enum dkio_state state;
			if ((softp->card_state & PCATA_CARD_INSERTED) == 0)
				state = DKIO_EJECTED;
			else
				state = DKIO_INSERTED;
			if (ddi_copyout((caddr_t)&state, (caddr_t)arg,
					sizeof (int), flag)) {
				return (EFAULT);
			}
		}
		break;
	case DKIOCINFO:

		info = (struct dk_cinfo *)data;
		/*
		 * Controller Information
		 */
		info->dki_ctype = DKC_PCMCIA_ATA;
		info->dki_cnum = ddi_get_instance(softp->dip);
		(void) strcpy(info->dki_cname,
		    ddi_get_name(ddi_get_parent(softp->dip)));

		/*
		 * Unit Information
		 */
		info->dki_unit = ddi_get_instance(softp->dip);
		info->dki_slave = 0;
		(void) strcpy(info->dki_dname, "card");
		info->dki_flags = DKI_FMTVOL;
		info->dki_partition = LPART(dev);
		info->dki_maxtransfer = softp->ab_max_transfer;

		/*
		 * We can't get from here to there yet
		 */
		info->dki_addr = 0;
		info->dki_space = 0;
		info->dki_prio = 0;
		info->dki_vec = 0;

		if (COPYOUT(data, arg, struct dk_cinfo, flag))
			return (EFAULT);
		break;

	case DKIOCG_VIRTGEOM:
	case DKIOCG_PHYGEOM:

		{
		struct dk_geom dkg;
		status = 0;

		bzero((caddr_t)&dkg, sizeof (struct dk_geom));
		mutex_enter(&softp->ata_mutex);
		unitp = softp->ab_link;
		if (unitp != 0) {
			dkg.dkg_ncyl  	= unitp->au_cyl;
			dkg.dkg_acyl  	= unitp->au_acyl;
			dkg.dkg_pcyl  	= unitp->au_cyl+unitp->au_acyl;
			dkg.dkg_nhead 	= unitp->au_hd;
			dkg.dkg_nsect 	= unitp->au_sec;
		} else
			status = EFAULT;
		mutex_exit(&softp->ata_mutex);
		if (status)
			return (EFAULT);

		if (ddi_copyout((caddr_t)&dkg, (caddr_t)arg,
				sizeof (struct dk_geom), flag))
			return (EFAULT);
		else
			return (0);
		}
	case DIOCTL_RWCMD:
		{
		int	rw;
		int	status;
		struct dadkio_rwcmd	rwcmd;
		struct buf		*bp;
		struct iovec		aiov;
		struct uio		auio;

		i = sizeof (rwcmd);
		if (ddi_copyin((caddr_t)arg, (caddr_t)&rwcmd, i, flag))
			return (EFAULT);

		switch (rwcmd.cmd) {
		case DADKIO_RWCMD_READ:
			rw = B_READ;
			break;
		case DADKIO_RWCMD_WRITE:
			rw = B_WRITE;
			break;
		default:
			return (EINVAL);
		}

		bp		= getrbuf(KM_SLEEP);
		bp->b_back	= (buf_t *)&rwcmd;	/* ioctl packet */
		bp->b_private	= (void *)0xBEE;

		bzero((caddr_t)&aiov, sizeof (struct iovec));
		aiov.iov_base	= rwcmd.bufaddr;
		aiov.iov_len	= rwcmd.buflen;

		bzero((caddr_t)&auio, sizeof (struct uio));
		auio.uio_iov	= &aiov;
		auio.uio_iovcnt	= 1;
		auio.uio_resid	= rwcmd.buflen;
		auio.uio_segflg	= flag & FKIOCTL ? UIO_SYSSPACE : UIO_USERSPACE;
		auio.uio_loffset = 0;
		auio.uio_fmode	= 0;

		status = physio(pcata_strategy, bp, dev, rw, pcata_min, &auio);

		freerbuf(bp);

		return (status);
		}

	case DKIOCEJECT:
		/*
		 * Since we do not have hardware support for ejecting
		 * a pcata card, we must not support the generic eject
		 * ioctl (DKIOCEJECT) which is used for eject(1) command
		 * because it leads the user to expect behavior that is
		 * not present.
		 */
		return (ENOSYS);

	case HDKIOCSCMD:
	case HDKIOCGDIAG:
		break;
	default:
		return (ENOTTY);
	}
	return (0);
}

int
pcata_lbl_ioctl(dev_t dev, int cmd, intptr_t arg, int flag)
{
	uint32_t data[512 / (sizeof (uint32_t))];
	void *instance;
	ata_soft_t *softp;
	ata_unit_t *unitp;
	int i;
	struct vtoc vtoc;

	if (pcata_getinfo(NULL, DDI_INFO_DEVT2INSTANCE, (void *)dev,
	    &instance) != DDI_SUCCESS)
		return (ENODEV);

	if (!(softp = ddi_get_soft_state(pcata_soft, (int)instance))) {
		return (ENXIO);
	}

	if (!(CARD_PRESENT_VALID(softp))) {
		return (ENODEV);
	}

	ASSERT(softp->ab_link);
	bzero((caddr_t)data, sizeof (data));
	unitp    = softp->ab_link;

	switch (cmd) {
	case DKIOCGGEOM:
	case DKIOCGAPART:
	case DKIOCGVTOC:
		if (pcata_update_vtoc(softp, dev))
			return (EFAULT);
	}

	switch (cmd) {
	case DKIOCGGEOM:
		{
		struct dk_geom up;

		pcdsklbl_dgtoug(&up, &unitp->lbl.ondsklbl);
		if (COPYOUT(&up, arg, struct dk_geom, flag)) {
			return (EFAULT);
		}
		break;
		}

	case DKIOCSGEOM:
		i = sizeof (struct dk_geom);
		if (ddi_copyin((caddr_t)arg, (caddr_t)data, i, flag))
			return (EFAULT);
		pcdsklbl_ugtodg((struct dk_geom *)data, &unitp->lbl.ondsklbl);
		break;

	case DKIOCGAPART:
		/*
		 * Return the map for all logical partitions.
		 */
#if defined(_MULTI_DATAMODEL)
		switch (ddi_model_convert_from(flag & FMODELS)) {
		case DDI_MODEL_ILP32: {
			struct dk_map32 dk_map32[NDKMAP];
			int	i;

			for (i = 0; i < NDKMAP; i++) {
				dk_map32[i].dkl_cylno =
					unitp->lbl.un_map[i].dkl_cylno;
				dk_map32[i].dkl_nblk =
					unitp->lbl.un_map[i].dkl_nblk;
			}
			i = NDKMAP * sizeof (struct dk_map32);
			if (ddi_copyout(dk_map32, (caddr_t)arg, i, flag))
				return (EFAULT);
			break;
		}

		case DDI_MODEL_NONE:
			i = NDKMAP * sizeof (struct dk_map);
			if (ddi_copyout((caddr_t)unitp->lbl.un_map,
			    (caddr_t)arg, i, flag))
				return (EFAULT);
			break;
		}

#else	/*  _MULTI_DATAMODEL */
		i = NDKMAP * sizeof (struct dk_map);
		if (ddi_copyout((caddr_t)unitp->lbl.un_map,
		    (caddr_t)arg, i, flag))
			return (EFAULT);
#endif	/*  _MULTI_DATAMODEL */
		break;

	case DKIOCSAPART:
		/*
		 * Set the map for all logical partitions.
		 */
#if defined(_MULTI_DATAMODEL)
		switch (ddi_model_convert_from(flag & FMODELS)) {
		case DDI_MODEL_ILP32: {
			struct dk_map32 dk_map32[NDKMAP];
			int	i;

			i = NDKMAP * sizeof (struct dk_map32);
			if (ddi_copyin((caddr_t)arg, dk_map32, i, flag))
				return (EFAULT);
			for (i = 0; i < NDKMAP; i++) {
				unitp->lbl.un_map[i].dkl_cylno =
					dk_map32[i].dkl_cylno;
				unitp->lbl.un_map[i].dkl_nblk =
					dk_map32[i].dkl_nblk;
			}
			i = NDKMAP * sizeof (struct dk_map32);
			break;
		}

		case DDI_MODEL_NONE:
			i = NDKMAP * sizeof (struct dk_map);
			if (ddi_copyout((caddr_t)unitp->lbl.un_map,
			    (caddr_t)arg, i, flag))
				return (EFAULT);
			break;
		}
		break;
#else	/*  _MULTI_DATAMODEL */
		i = NDKMAP * sizeof (struct dk_map);
		if (ddi_copyin((caddr_t)arg, (caddr_t)data, i, flag))
			return (EFAULT);
		bcopy((caddr_t)data, (caddr_t)unitp->lbl.un_map, i);
		break;
#endif	/*  _MULTI_DATAMODEL */

	case DKIOCGVTOC:
#if defined(_MULTI_DATAMODEL)
		switch (ddi_model_convert_from(flag & FMODELS)) {
		case DDI_MODEL_ILP32: {
			struct vtoc32 vtoc32;

			pcdsklbl_ondsklabel_to_vtoc(&unitp->lbl, &vtoc);
			vtoctovtoc32(vtoc, vtoc32);
			if (ddi_copyout(&vtoc32, (caddr_t)arg,
						sizeof (struct vtoc32), flag))
				return (EFAULT);
			break;
		}

		case DDI_MODEL_NONE:
			pcdsklbl_ondsklabel_to_vtoc(&unitp->lbl, &vtoc);
			if (ddi_copyout((caddr_t)&vtoc, (caddr_t)arg,
						sizeof (struct vtoc), flag))
				return (EFAULT);
			break;
		}
		return (0);
#else	/*  _MULTI_DATAMODEL */
		pcdsklbl_ondsklabel_to_vtoc(&unitp->lbl, &vtoc);
		if (ddi_copyout((caddr_t)&vtoc, (caddr_t)arg,
					sizeof (struct vtoc), flag))
			return (EFAULT);
		return (0);
#endif	/*  _MULTI_DATAMODEL */

	case DKIOCSVTOC:
#if defined(_MULTI_DATAMODEL)
		switch (ddi_model_convert_from(flag & FMODELS)) {
		case DDI_MODEL_ILP32: {
			struct vtoc32 vtoc32;

			if (ddi_copyin((caddr_t)arg, &vtoc32,
						sizeof (struct vtoc32), flag))
				return (EFAULT);
			vtoc32tovtoc(vtoc32, vtoc);

			if (pcata_write_dskvtoc(softp, dev, &unitp->lbl, &vtoc))
				return (EFAULT);
			break;
		}

		case DDI_MODEL_NONE:
			if (ddi_copyin((caddr_t)arg, (caddr_t)&vtoc,
						sizeof (struct vtoc), flag))
				return (EFAULT);

			if (pcata_write_dskvtoc(softp, dev, &unitp->lbl, &vtoc))
				return (EFAULT);

			break;
		}
#else	/*  _MULTI_DATAMODEL */
		if (ddi_copyin((caddr_t)arg, (caddr_t)&vtoc,
					sizeof (struct vtoc), flag))
			return (EFAULT);

		if (pcata_write_dskvtoc(softp, dev, &unitp->lbl, &vtoc))
			return (EFAULT);

		break;
#endif	/*  _MULTI_DATAMODEL */
	}
	return (0);
}

/* ARGSUSED */
int
pcata_open(dev_t *dev_p, int flag, int otyp, cred_t *cred_p)
{
#ifdef MAINTAIN_OPEN_STATUS
	register dev_t dev = *dev_p;
#endif
	ata_soft_t *softp;
	void	*instance;
	int	i;

#ifdef ATA_DEBUG
	if (pcata_debug & DIO)
		cmn_err(CE_CONT, "_open\n");
#endif
	if (pcata_getinfo(NULL, DDI_INFO_DEVT2INSTANCE, (void *) *dev_p,
	    &instance) != DDI_SUCCESS)
		return (ENODEV);

	softp = ddi_get_soft_state(pcata_soft, (int)instance);

	/*
	 * open and getinfo may be called before attach completes
	 */
	for (i = 0; i < 300; i++) {
		if (softp->flags & PCATA_READY)
			break;
		drv_usecwait(10000);
	}
	if (!pcata_readywait(softp))
		return (ENXIO);

#ifdef MAINTAIN_OPEN_STATUS
	mutex_enter(&(softp)->ata_mutex);
	softp->open_flag |= (1 << LPART(dev));
	mutex_exit(&(softp)->ata_mutex);
#endif

	return (0);
}



/* ARGSUSED */
int
pcata_close(dev_t dev, int flag, int otyp, cred_t *cred_p)
{
#ifdef MAINTAIN_OPEN_STATUS
	ata_soft_t *softp;
#endif
	void	*instance;

#ifdef ATA_DEBUG
	if (pcata_debug & DIO)
		cmn_err(CE_CONT, "_close\n");
#endif
	if (pcata_getinfo(NULL, DDI_INFO_DEVT2INSTANCE, (void *) dev,
	    &instance) != DDI_SUCCESS)
		return (ENODEV);

#ifdef MAINTAIN_OPEN_STATUS
	softp = ddi_get_soft_state(pcata_soft, (int)instance);

	mutex_enter(&(softp)->ata_mutex);

	softp->open_flag &= ~(1 << LPART(dev));
	if ((softp->open_flag == 0) && (softp->card_invalid))
		softp->card_invalid = 0;

	mutex_exit(&(softp)->ata_mutex);
#endif

	return (0);
}

static int
pcata_redo_vtoc(ata_soft_t *softp, buf_t *fdiskbp)
{
	struct dk_geom	dkg;
	ata_unit_t	*unitp;
	buf_t		*bp;
	int		status;
	dev_t		dev;


	unitp = softp->ab_link;
	if (!unitp)
		return (EFAULT);

	/* given any maj/min convert to fdisk partition 0 */
	dev = makedevice(getmajor(fdiskbp->b_edev),
		PCATA_SETMINOR(softp->sn, FDISK_OFFSET));

	if ((bp = pcata_lblk_alloc(dev)) == NULL)
		return (EFAULT);

	bcopy(fdiskbp->b_un.b_addr, bp->b_un.b_addr, NBPSCTR);

	bzero((caddr_t)&dkg, sizeof (struct dk_geom));
	dkg.dkg_ncyl  	= unitp->au_cyl;
	dkg.dkg_nhead 	= unitp->au_hd;
	dkg.dkg_nsect 	= unitp->au_sec;

	status = pcfdisk_parse(bp, unitp);

	/* release buffer allocated by getrbuf */
	kmem_free(bp->b_un.b_addr, NBPSCTR);
	freerbuf(bp);

	if (status == DDI_FAILURE)
		return (EFAULT);
	return (0);
}

/*
 *
 */
int
pcata_update_vtoc(ata_soft_t *softp, dev_t dev)
{
	ata_unit_t	*unitp;
	buf_t		*bp;
	int		status;

	unitp = softp->ab_link;
	if (!unitp)
		return (EFAULT);

	/* given any maj/min convert to fdisk partition 0 */
	dev = makedevice(getmajor(dev),
		PCATA_SETMINOR(softp->sn, FDISK_OFFSET));

	if ((bp = pcata_lblk_alloc(dev)) == NULL)
		return (EFAULT);

	/*
	 * The dev is passed here for use later by the dsklbl_rdvtoc()
	 * and pcata_dsklbl_read_label() to check for card present before
	 * calling biowait.
	 */
	status = pcfdisk_read(bp, unitp);

	/* release buffer allocated by getrbuf */
	kmem_free(bp->b_un.b_addr, NBPSCTR);
	freerbuf(bp);

	if (status == DDI_FAILURE)
		return (EFAULT);
	return (0);
}

static buf_t *
pcata_lblk_alloc(dev_t dev)
{
	buf_t *bp;
	char	*secbuf;

	/* allocate memory to hold disk label */
	secbuf = kmem_zalloc(NBPSCTR, KM_SLEEP);
	if (!secbuf)
		return (NULL);

	/* allocate a buf_t to manage the disk label block */
	bp = getrbuf(KM_SLEEP);
	if (!bp) {
		kmem_free(secbuf, NBPSCTR);
		return (NULL);
	}

	/* initialize the buf_t */
	bp->b_edev = dev;
	bp->b_dev  = cmpdev(dev);
	bp->b_flags |= B_BUSY;
	bp->b_resid = 0;
	bp->b_bcount = NBPSCTR;
	bp->b_un.b_addr = (caddr_t)secbuf;

	return (bp);
}


int
pcata_write_dskvtoc(ata_soft_t *softp, dev_t dev, dsk_label_t *lblp,
		struct vtoc *vtocp)
{
	buf_t *bp;
	int	status;

	dev = makedevice(getmajor(dev),
		PCATA_SETMINOR(softp->sn, FDISK_OFFSET));

	if ((bp = pcata_lblk_alloc(dev)) == NULL)
		return (EFAULT);

#ifdef ATA_DEBUG
	cmn_err(CE_CONT, "_write_dskvtoc: edev = %lx dev = %x\n",
		bp->b_edev,
		bp->b_dev);
#endif


	bp->b_edev = dev; /* used by probe_for_card() */
	status = pcdsklbl_wrvtoc(lblp, vtocp, bp);

	/* release buffer allocated by getrbuf */
	kmem_free(bp->b_un.b_addr, NBPSCTR);
	freerbuf(bp);

	return (status);
}
