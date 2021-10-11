/*
 * Copyright (c) 1997 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Definitions for the NCR 710/810 EISA-bus Intelligent SCSI Host Adapter.
 * This file is used by an MDB driver under the SOLARIS Primary Boot Subsystem.
 *
#pragma ident	"@(#)ncrdefs.h	1.6	97/07/21 SMI"
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
 * This file is derived from the ncrdefs.h file used for the Solaris
 * NCR driver.
 */



/*
 * init routines
 */
void		ncr_saverestore(ncr_t *ncrp, nrs_t *nrsp, unchar *regbufp,
			int nregs, bool_t savethem);
void		ncr_table_init(ncr_t *ncrp, npt_t *nptp, int target,
			int lun, ulong cmdlen, struct scsi_pkt *pktp);
#ifdef SOLARIS
static	bool_t	ncr_target_init(ncr_t *ncrp);
bool_t		ncr_hba_init(ncr_t *ncrp);
void		ncr_hba_uninit(ncr_t *ncrp);
#endif



/*
 * Interrupt routines
 */
u_int		ncr_intr(caddr_t arg);
static	void	ncr_process_intr(ncr_t *ncrp, unchar istat);
static	ulong	ncr_decide(ncr_t *ncrp, ulong action);
static	ulong	ncr_ccb_decide(ncr_t *ncrp, npt_t *nptp, ulong action);
bool_t		ncr_wait_intr(ncr_t *ncrp);
static	int	ncr_setup_npt(ncr_t *ncrp, npt_t *nptp);
static	void	ncr_start_next(ncr_t *ncrp);
static	void	ncr_wait_for_reselect(ncr_t *ncrp, ulong action);
static	void	ncr_restart_current(ncr_t *ncrp, ulong action);
static	void	ncr_restart_hba(ncr_t *ncrp, ulong action);
void		ncr_queue_target(ncr_t *ncrp, npt_t *nptp);
#ifdef SOLARIS
static	npt_t	*ncr_get_target(ncr_t *ncrp);
#endif
static	ulong	ncr_check_intcode(ncr_t *ncrp, npt_t *nptp, ulong action);
ulong		ncr_parity_check(unchar phase);

#ifdef SOLARIS
/*
 * queue manipulation routines
 */
void		 ncr_addfq(ncr_t *ncrp, npt_t *nptp);
void		 ncr_addbq(ncr_t *ncrp, npt_t *nptp);
npt_t		*ncr_rmq(ncr_t *ncrp);
void		 ncr_delq(ncr_t *ncrp, npt_t *nptp);
void		 ncr_doneq_add(ncr_t *ncrp, nccb_t *nccbp);
nccb_t		*ncr_doneq_rm(ncr_t *ncrp);
void		 ncr_waitq_add(npt_t *nptp, nccb_t *nccbp);
nccb_t		*ncr_waitq_rm(npt_t *nptp);
void		 ncr_waitq_delete(npt_t *nptp, nccb_t *nccbp);

/*
 * DMA Scatter/Gather list routines
 */
void		ncr_sg_setup(ncr_t *ncrp, npt_t *nptp, nccb_t *nccbp);
void		ncr_sg_update(ncr_t *ncrp, npt_t *nptp, unchar index,
			ulong remain);
ulong		ncr_sg_residual(ncr_t *ncrp, npt_t *nptp);

/*
 * Synchronous I/O routines
 */
void		ncr_syncio_state(ncr_t *ncrp, npt_t *nptp, unchar state,
			unchar sxfer, unchar sscf);
void		ncr_syncio_reset(ncr_t *ncrp, npt_t *nptp);
void		ncr_syncio_msg_init(ncr_t *ncrp, npt_t *nptp);
static bool_t	ncr_syncio_enable(ncr_t *ncrp, npt_t *nptp);
static bool_t	ncr_syncio_respond(ncr_t *ncrp, npt_t *nptp);
ulong		ncr_syncio_decide(ncr_t *ncrp, npt_t *nptp, ulong action);
#endif

/*
 * nccb routines
 */
void		ncr_queue_ccb(ncr_t *ncrp, npt_t *nptp, unchar rqst_type);
#ifdef SOLARIS
bool_t		ncr_send_dev_reset(struct scsi_address *ap, ncr_t *ncrp
							  , npt_t *nptp);
bool_t		ncr_abort_ccb(struct scsi_address *ap, ncr_t *ncrp
						     , npt_t *nptp);
#endif
void		ncr_chkstatus(ncr_t *ncrp, npt_t *nptp, struct scsi_pkt *pktp);
void		ncr_pollret(ncr_t *ncrp, struct scsi_pkt *poll_pktp);
void		ncr_flush_lun(ncr_t *ncrp, npt_t *nptp, bool_t flush_all
					 , u_char pkt_reason, u_long pkt_state
					 , u_long pkt_statistics);
void		ncr_flush_target(ncr_t *ncrp, ushort target, bool_t flush_all
					    , u_char pkt_reason
					    , u_long pkt_state
					    , u_long pkt_statistics);
void		ncr_flush_hba(ncr_t *ncrp, bool_t flush_all, u_char pkt_reason
					 , u_long pkt_state
					 , u_long pkt_statistics);
void		ncr_set_done(ncr_t *ncrp, npt_t *nptp, struct scsi_pkt *pktp
					, u_char pkt_reason, u_long pkt_state
					, u_long pkt_statistics);

/*
 * Script functions
 */
static	int	ncr_script_offset(int func);
bool_t		ncr_script_init(ushort seg);
void		ncr_script_fini(void);


#ifdef SOLARIS
/*
 * Synchronous I/O functions
 */
static	bool_t	ncr_max_sync_rate_parse(ncr_t *ncrp, char *cp);
static	int	ncr_max_sync_lookup(char *savecp, int cnt);
bool_t		ncr_max_sync_divisor(ncr_t *ncrp, int syncioperiod,
			unchar *sxferp, unchar *sscfp);
int		ncr_period_round(ncr_t *ncrp, int syncioperiod);
void		ncr_max_sync_rate_init(ncr_t *ncrp, bool_t is710);
#endif
