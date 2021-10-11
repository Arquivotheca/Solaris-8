/*
 * Copyright (c) 1997 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Definitions for the NCR 710/810 EISA-bus Intelligent SCSI Host Adapter.
 * This file is used by an MDB driver under the SOLARIS Primary Boot Subsystem.
 *
#pragma ident	"@(#)ncrops.h	1.5	97/07/21 SMI"
 */

/*
 * RESTRICTED RIGHTS
 *
 * These programs are supplied under a license.  They may be used,
 * disclosed, and/or copied only as permitted under such license
 * agreement.  Any copy must contain the above copyright notice and
 * this restricted rights notice.  Use, copying, and/or disclosure
 * of the programs is strictly prohibited unless otherwise provided
 * in the license agreement.
 */

/*
 * This file is derived from the ncrops.h file used for the Solaris NCR driver.
 */


typedef	struct ncrops {
	char		*ncr_chip;
#ifdef SOLAIRS
	bool_t	(*ncr_script_init)(void);
#endif
	bool_t	(*ncr_script_init)(ushort seg);
	void	(*ncr_reset)(ncr_t *ncrp);
	void	(*ncr_init)(ncr_t *ncrp);
#ifdef SOLARIS
	void	(*ncr_enable)(ncr_t *ncrp);
	void	(*ncr_disable)(ncr_t *ncrp);
#endif
	unchar	(*ncr_get_istat)(ncr_t *ncrp);
	void	(*ncr_halt)(ncr_t *ncrp);
#ifdef SOLARIS
	void	(*ncr_set_sigp)(ncr_t *ncrp);
	void	(*ncr_reset_sigp)(ncr_t *ncrp);
#endif
	ulong	(*ncr_get_intcode)(ncr_t *ncrp);
	void	(*ncr_check_error)(npt_t *nptp, struct scsi_pkt *pktp);
	ulong	(*ncr_dma_status)(ncr_t *ncrp);
	ulong	(*ncr_scsi_status)(ncr_t *ncrp);
#ifdef SOLARIS
	bool_t	(*ncr_save_byte_count)(ncr_t *ncrp, npt_t *nptp);
	bool_t	(*ncr_get_target)(ncr_t *ncrp, unchar *tp);
#endif
	unchar	(*ncr_encode_id)(unchar id);
	void	(*ncr_setup_script)(ncr_t *ncrp, npt_t *nptp);
	void	(*ncr_start_script)(ncr_t *ncrp, int script);
#ifdef SOLARIS
	void	(*ncr_set_syncio)(ncr_t *ncrp, npt_t *nptp);
#endif
	void	(*ncr_bus_reset)(ncr_t *ncrp);
} nops_t;

#ifdef SOLARIS
/* array of ptrs to ops tables for supported chip types */
extern	nops_t	*ncr_conf[];
#endif

#define	NCR_RESET(P)			((P)->n_ops->ncr_reset)(P)
#define	NCR_INIT(P)			((P)->n_ops->ncr_init)(P)
#ifdef SOLARIS
#define	NCR_ENABLE_INTR(P)		((P)->n_ops->ncr_enable)(P)
#define	NCR_DISABLE_INTR(P)		((P)->n_ops->ncr_disable)(P)
#endif
#define	NCR_GET_ISTAT(P)		((P)->n_ops->ncr_get_istat)(P)
#define	NCR_HALT(P)			((P)->n_ops->ncr_halt)(P)
#ifdef SOLARIS
#define	NCR_SET_SIGP(P)			((P)->n_ops->ncr_set_sigp)(P)
#define	NCR_RESET_SIGP(P)		((P)->n_ops->ncr_reset_sigp)(P)
#endif
#define	NCR_GET_INTCODE(P)		((P)->n_ops->ncr_get_intcode)(P)
#define	NCR_CHECK_ERROR(P, nptp, pktp)	((P)->n_ops->ncr_check_error)(nptp, \
						pktp)
#define	NCR_DMA_STATUS(P)		((P)->n_ops->ncr_dma_status)(P)
#define	NCR_SCSI_STATUS(P)		((P)->n_ops->ncr_scsi_status)(P)
#ifdef SOLARIS
#define	NCR_SAVE_BYTE_COUNT(P, nptp)	((P)->n_ops->ncr_save_byte_count)(P, \
						nptp)
#define	NCR_GET_TARGET(P, tp)		((P)->n_ops->ncr_get_target)(P, tp)
#endif
#define	NCR_ENCODE_ID(P, id)		((P)->n_ops->ncr_encode_id)(id)
#define	NCR_SETUP_SCRIPT(P, nptp)	((P)->n_ops->ncr_setup_script)(P, nptp)
#define	NCR_START_SCRIPT(P, script)	((P)->n_ops->ncr_start_script)(P, \
						script)
#ifdef SOLARIS
#define	NCR_SET_SYNCIO(P, nptp)		((P)->n_ops->ncr_set_syncio)(P, nptp)
#endif
#define	NCR_BUS_RESET(P)		((P)->n_ops->ncr_bus_reset)(P)

/*
 * All models of the NCR are assumed to have consistent definitions
 * of the following bits in the ISTAT register. The ISTAT register
 * can be at different offsets but these bits must be the same.
 * If this isn't true then we'll have to add functions to the
 * ncrops table to access these bits similar to how the ncr_get_intcode()
 * function is defined.
 */

#define	NB_ISTAT_CON		0x08	/* connected */
#define	NB_ISTAT_SIP		0x02	/* scsi interrupt pending */
#define	NB_ISTAT_DIP		0x01	/* dma interrupt pending */

/*
 * Max SCSI Synchronous Offset bits in the SXFER register. Zero sets
 * asynchronous mode.
 */
#define	NB_SXFER_MO		0x0f	/* max scsi offset */
