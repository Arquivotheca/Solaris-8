/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DKTP_CHS_CHSDEFS_H
#define	_SYS_DKTP_CHS_CHSDEFS_H

#pragma ident	"@(#)chsdefs.h	1.6	99/05/20 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


void chs_process_doneq(chs_t *, Que_t *);
chs_ccb_t *chs_process_status(chs_t *, chs_stat_t *);

void QueueAdd(Que_t *, Qel_t *, void *);
void * QueueRemove(Que_t *);


/* Common SCSI and non-SCSI function prototypes. */
int chs_tgt_init(dev_info_t *const, dev_info_t *const,
    scsi_hba_tran_t *const, struct scsi_device *const);
void chs_tgt_free(dev_info_t *const, dev_info_t *const,
    scsi_hba_tran_t *const, struct scsi_device *const);
int chs_ccbinit(chs_hba_t *const);
int chs_getnchn_maxtgt(chs_t *const, chs_ccb_t *const);
struct scsi_pkt *chs_init_pkt(struct scsi_address *const sa,
    register struct scsi_pkt *pkt, register buf_t *const bp, int cmdlen,
    int statuslen, int tgt_priv_len,
    int flags, int (*const callback)(), const caddr_t arg);
void chs_destroy_pkt(struct scsi_address *const sa,
    struct scsi_pkt *const pkt);
int chs_getconf(chs_t *const, chs_ccb_t *const);
int chs_getenquiry(chs_t *const, chs_ccb_t *const);
int chs_ccb_stkinit(chs_t *const);

void chs_getenq_info_viper(chs_t *const);

int chs_dont_access(chs_t *const, const uchar_t, const uchar_t);

bool_t chs_in_any_sd_viper(chs_t *const,
    const uchar_t, const uchar_t, uchar_t *);

bool_t chs_can_physdrv_access_viper(chs_t *const,
    const uchar_t, const uchar_t);

int chs_init_cmd(chs_t *const, chs_ccb_t *const);
int chs_sendcmd(chs_t *const, chs_ccb_t *const, int);
int chs_pollstat(chs_t *const, chs_ccb_t *const, int);

/* SCSI function prototypes. */
struct scsi_pkt *chs_pktalloc(struct scsi_address *const,
    int, int, int, int (*const)(), const caddr_t);
void chs_pktfree(struct scsi_pkt *const);
struct scsi_pkt *chs_dmaget(struct scsi_pkt *const,
    const opaque_t, int (*const)(), const caddr_t);
int chs_getinq(chs_hba_t *const);
int chs_inq_init(chs_hba_t *const);
int chs_tran_tgt_init(dev_info_t *const, dev_info_t *const,
    scsi_hba_tran_t *const, struct scsi_device *const);

void chs_scsi_chkerr_viper(chs_t *const chs,
    register chs_ccb_t *const ccb, register struct scsi_pkt *const pkt,
    const ushort_t status);

int chs_dac_check_status_viper(struct cmpkt *cmpktp, ushort_t status);

void chs_get_scsi_item_viper(chs_cdbt_t *cdbt,
			chs_dcdb_uncommon_t *dcdbportion);

uchar_t chs_get_nsd(chs_t *chsp);
int chs_capchk(char *, int, int *);

/* Non-SCSI function prototypes. */
struct cmpkt *chs_dac_iosetup(chs_unit_t *const, struct cmpkt *const);
struct cmpkt *chs_dac_memsetup(chs_unit_t *const, struct cmpkt *const,
    buf_t *const, int (*const)(), const caddr_t);
struct cmpkt *chs_dac_pktalloc(chs_unit_t *const, int (*const)(),
    const caddr_t);
void chs_dac_pktfree(chs_unit_t *const, struct cmpkt *);
int chs_dac_transport(chs_unit_t *const, struct cmpkt *const);
void chs_dac_fake_inquiry(chs_hba_t *const, int,
    struct scsi_inquiry *const);

bool_t chs_get_log_drv_info_viper(chs_t *, int, chs_ld_t *);

int chs_dac_tran_tgt_init(dev_info_t *const, dev_info_t *const,
    scsi_hba_tran_t *const, struct scsi_device *const);
void chs_dac_memfree(const chs_unit_t *const, struct cmpkt *const);
int chs_dac_abort(const chs_unit_t *const, const struct cmpkt *const);
int chs_dac_reset(chs_unit_t *const, int);
int chs_dacioc(chs_unit_t *const, int, int, int);
int chs_dacioc_nopkt(chs_unit_t *const, int, int, int);
struct cmpkt *chs_dacioc_pktalloc(chs_unit_t *const, int,
    int, int, int *const);
ushort_t chs_dacioc_ubuf_len(chs_t *const, const uchar_t);
int chs_dacioc_getarg(chs_t *const, chs_unit_t *const, int,
    struct cmpkt *const, chs_ccb_t *const, int);
int chs_dacioc_valid_args(chs_t *const, chs_ccb_t *const,
    const ushort_t, chs_dacioc_t *const);
void chs_dacioc_callback(struct cmpkt *const);
int chs_dacioc_done(chs_unit_t *const unit, struct cmpkt *const,
    chs_ccb_t *const, chs_dacioc_t *const, int, int);
int chs_dacioc_update_conf_enq(chs_unit_t *const, chs_ccb_t *const,
    const ushort_t);
void chs_dacioc_pktfree(chs_unit_t *const, struct cmpkt *,
    chs_ccb_t *const, chs_dacioc_t *, int);
int chs_flush_cache(dev_info_t *const, const ddi_reset_cmd_t);



void chs_iosetup_viper(chs_ccb_t *, int, ulong_t, off_t, uchar_t, int);


/*
 * busops routines
 */
void chs_childprop(dev_info_t *, dev_info_t *);

/*
 * devops routines
 */
int		chs_identify(dev_info_t *devi);
int		chs_probe(dev_info_t *devi);
int		chs_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
int		chs_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);
bool_t		chs_propinit(dev_info_t *devi, chs_unit_t *unit);
bool_t		chs_prop_default(dev_info_t *dip, caddr_t propname,
			caddr_t propdefault);
chs_t	*chs_cardfind(dev_info_t *dip, chs_unit_t *unit);
bool_t		chs_cardinit(dev_info_t *dip, chs_unit_t *unitp);
void		chs_carduninit(dev_info_t *dip, chs_unit_t *unitp);

/*
 * Interrupt routines
 */
uint_t		chs_intr(caddr_t arg);

/*
 * External SCSA Interface
 */

int	chs_tran_tgt_init(dev_info_t *, dev_info_t *,
			scsi_hba_tran_t *, struct scsi_device *);
int	chs_tran_tgt_probe(struct scsi_device *, int (*)());
void	chs_tran_tgt_free(dev_info_t *, dev_info_t *,
			scsi_hba_tran_t *, struct scsi_device *);
struct scsi_pkt *chs_tran_init_pkt(struct scsi_address *,
			struct scsi_pkt *, struct buf *, int, int, int, int,
			int (*)(), caddr_t);
void	chs_tran_destroy_pkt(struct scsi_address *, struct scsi_pkt *);

int	chs_transport(struct scsi_address *, struct scsi_pkt *);
int	chs_reset(struct scsi_address *, int);
int	chs_abort(struct scsi_address *, struct scsi_pkt *);
int	chs_getcap(struct scsi_address *, char *, int);
int	chs_setcap(struct scsi_address *, char *, int, int);
int	chs_capchk(char *, int, int *);

struct scsi_pkt *chs_pktalloc(struct scsi_address *, int, int, int,
				int (*)(), caddr_t);
void	chs_pktfree(struct scsi_pkt *);
struct scsi_pkt *chs_dmaget(struct scsi_pkt *, opaque_t, int (*)(),
				caddr_t);
void	chs_dmafree(struct scsi_address *, struct scsi_pkt *);
void	chs_sync_pkt(struct scsi_address *, struct scsi_pkt *);


/*
 * Auto-config functions
 */
nops_t		*chs_hbatype(dev_info_t *, int **, int *, bool_t);
bool_t		chs_get_irq_pci(chs_t *chsp, int *regp, int reglen);
int		chs_get_reg_pci(chs_t *chsp, int *regp, int reglen);
bool_t		chs_cfg_init(chs_t *);

bool_t		add_intr(dev_info_t *devi, uint_t inumber,
			ddi_iblock_cookie_t *iblockp, kmutex_t *mutexp,
			uint_t (*intr_func)(caddr_t), caddr_t intr_arg);
bool_t		chs_find_irq(dev_info_t	*devi, uchar_t irq, int *intrp,
			int len, uint_t *inumber);
bool_t		chs_xlate_irq_sid(chs_t *chsp);
bool_t		chs_xlate_irq_no_sid(chs_t *chsp);
bool_t		chs_intr_init(dev_info_t *devi, chs_t *chsp,
			caddr_t intr_arg);

/*
 * Extern variables
 */
extern ddi_dma_lim_t chs_dma_lim;
extern ddi_dma_lim_t chs_dac_dma_lim;
extern kmutex_t chs_global_mutex;
extern int chs_pgsz;
extern int chs_pgmsk;
extern int chs_pgshf;
extern chs_t *chs_cards;

extern nops_t viper_nops;

extern struct dev_ops chs_ops;

extern int chs_forceload;

extern int chs_disks_scsi;

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DKTP_CHS_CHSDEFS_H */
