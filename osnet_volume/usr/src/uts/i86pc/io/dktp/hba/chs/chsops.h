/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef _SYS_DKTP_CHS_CHSOPS_H
#define	_SYS_DKTP_CHS_CHSOPS_H

#pragma ident	"@(#)chsops.h	1.3	99/03/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef	struct chsops {
	char		*chs_chip;
	bool_t	(*chs_probe)(dev_info_t *dip, int *regp, int len,
				int *pidp, int pidlen,
				bus_t bus_type, bool_t probing);
	bool_t	(*chs_get_irq)(chs_t *chsp, int *regp, int reglen);
	bool_t	(*chs_xlate_irq)(chs_t *chsp);
	int	(*chs_rnumber)(chs_t *chsp, int *regp, int reglen);
	void	(*chs_reset)(chs_t *chsp);
	bool_t	(*chs_init)(chs_t *chsp, dev_info_t *dip);
	void	(*chs_uninit)(chs_t *chsp, dev_info_t *dip);
	void	(*chs_enable)(chs_t *chsp);
	void	(*chs_disable)(chs_t *chsp);
	uchar_t (*chs_cready)(chs_t *chsp);
	bool_t	(*chs_csend)(chs_t *chsp, void *hw_ccbp);
	uchar_t (*chs_iready)(chs_t *chsp);
	uchar_t (*chs_get_istat)(chs_t *chsp, void *hw_statp,
					int clear);
	int	(*chs_geometry)(chs_t *chsp,
					struct scsi_address *ap, ulong_t bl);
	void	(*chs_iosetup) (chs_ccb_t *ccb, int num_segs,
					ulong_t bytes_xfer,
					off_t cofforsrtsec,
					uchar_t sdnum, int old);
	void 	(*chs_scsi_chkerr)(chs_t *const chs,
					register chs_ccb_t *const ccb,
					register struct scsi_pkt *const pkt,
					const ushort_t status);
	void	(*chs_getenq_info)(chs_t *chs);
	bool_t 	(*chs_in_any_sd)(chs_t *const chs, const uchar_t chn,
					const uchar_t tgt,
					uchar_t *raidlevel);
	bool_t 	(*chs_get_logdrv_info)(chs_t *const chs, int tgt,
					chs_ld_t *ld_info);
	bool_t	(*chs_can_physdrv_access) (chs_t *const chs,
					const uchar_t chn,
					const uchar_t tgt);
	int	(*chs_dac_check_status) (struct cmpkt *cmpktp,
					ushort_t status);
	void 	(*chs_get_scsi_item) (chs_cdbt_t *cdbt,
					chs_dcdb_uncommon_t *dcdb_portionp);

} nops_t;



/* array of ptrs to ops tables for supported chip types */
extern	nops_t	*chs_conf[];

#define	CHS_GET_IRQ(P, regp, reglen) \
			((P)->ops->chs_get_irq)(P, regp, reglen)

#define	CHS_XLATE_IRQ(P)		((P)->ops->chs_xlate_irq)(P)
#define	CHS_RNUMBER(P, regp, reglen) \
				((P)->ops->chs_rnumber)(P, regp, reglen)

#define	CHS_RESET(P)			((P)->ops->chs_reset)(P)
#define	CHS_INIT(P, d)			((P)->ops->chs_init)(P, d)
#define	CHS_UNINIT(P, d)		((P)->ops->chs_uninit)(P, d)
#define	CHS_ENABLE_INTR(P)		((P)->ops->chs_enable)(P)
#define	CHS_DISABLE_INTR(P)		((P)->ops->chs_disable)(P)
#define	CHS_CREADY(P)			((P)->ops->chs_cready)(P)
#define	CHS_CSEND(P, ccb)		((P)->ops->chs_csend)(P, ccb)
#define	CHS_IREADY(P)			((P)->ops->chs_iready)(P)
#define	CHS_GET_ISTAT(P, st, clr) \
				((P)->ops->chs_get_istat)(P, st, clr)

#define	CHS_GEOMETRY(P, ap, blk) \
				((P)->ops->chs_geometry)(P, ap, blk)

#define	CHS_IOSETUP(P, a, b, c, d, e, f) \
				((P)->ops->chs_iosetup)(a, b, c, d, e, f)

#define	CHS_SCSI_CHKERR(P, a, b, c) \
				((P)->ops->chs_scsi_chkerr)(P, a, b, c)

#define	CHS_GETENQ_INFO(P) \
				((P)->ops->chs_getenq_info)(P)

#define	CHS_IN_ANY_SD(P, c, t, r) \
				((P)->ops->chs_in_any_sd)(P, c, t, r)

#define	CHS_GET_LOGDRV_INFO(P, t, l) \
				((P)->ops->chs_get_logdrv_info)(P, t, l)

#define	CHS_CAN_PHYSDRV_ACCESS(P, c, t) \
				((P)->ops->chs_can_physdrv_access)(P, c, t)

#define	CHS_DAC_CHECK_STATUS(P, c, t) \
				((P)->ops->chs_dac_check_status)(c, t)

#define	CHS_GET_SCSI_ITEM(P, c, t) \
				((P)->ops->chs_get_scsi_item)(c, t)

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DKTP_CHS_CHSOPS_H */
