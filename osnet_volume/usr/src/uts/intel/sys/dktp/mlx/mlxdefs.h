/*
 * Copyright (c) 1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DKTP_MLX_MLXDEFS_H
#define	_SYS_DKTP_MLX_MLXDEFS_H

#pragma ident	"@(#)mlxdefs.h	1.7	99/05/20 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* Common SCSI and non-SCSI function prototypes. */
int mlx_tgt_init(dev_info_t *const, dev_info_t *const,
    scsi_hba_tran_t *const, struct scsi_device *const);
void mlx_tgt_free(dev_info_t *const, dev_info_t *const,
    scsi_hba_tran_t *const, struct scsi_device *const);
int mlx_ccbinit(mlx_hba_t *const);
int mlx_getnchn_maxtgt(mlx_t *const, mlx_ccb_t *const);
struct scsi_pkt *mlx_init_pkt(struct scsi_address *const sa,
    register struct scsi_pkt *pkt, register buf_t *const bp, int cmdlen,
    int statuslen, int tgt_priv_len,
    int flags, int (*const callback)(), const caddr_t arg);
void mlx_destroy_pkt(struct scsi_address *const sa, struct scsi_pkt *const pkt);

int mlx_getconf(mlx_t *const, mlx_ccb_t *const);
int mlx_getenquiry(mlx_t *const, mlx_ccb_t *const);
void mlx_getenq_info(mlx_t *const);
int mlx_ccb_stkinit(mlx_t *const);
mlx_dac_sd_t *mlx_in_any_sd(mlx_t *const, const uchar_t, const uchar_t);
int mlx_dont_access(mlx_t *const, const uchar_t, const uchar_t);
int mlx_init_cmd(mlx_t *const, mlx_ccb_t *const);
int mlx_sendcmd(mlx_t *const, mlx_ccb_t *const, int);
int mlx_pollstat(mlx_t *const, mlx_ccb_t *const, int);

/* SCSI function prototypes. */
struct scsi_pkt *mlx_pktalloc(struct scsi_address *const,
    int, int, int, int (*const)(), const caddr_t);
void mlx_pktfree(struct scsi_pkt *const);
struct scsi_pkt *mlx_dmaget(struct scsi_pkt *const,
    const opaque_t, int (*const)(), const caddr_t);
int mlx_getinq(mlx_hba_t *const);
int mlx_inq_init(mlx_hba_t *const);
int mlx_tgt_probe(register struct scsi_device *const,
    int (*const callback)());
int mlx_tran_tgt_init(dev_info_t *const, dev_info_t *const,
    scsi_hba_tran_t *const, struct scsi_device *const);
void mlx_chkerr(mlx_t *const mlx, register mlx_ccb_t *const ccb,
    register struct scsi_pkt *const pkt, const ushort_t status);
int mlx_capchk(char *, int, int *);

/* Non-SCSI function prototypes. */
struct cmpkt *mlx_dac_iosetup(mlx_unit_t *const, struct cmpkt *const);
struct cmpkt *mlx_dac_memsetup(mlx_unit_t *const, struct cmpkt *const,
    buf_t *const, int (*const)(), const caddr_t);
struct cmpkt *mlx_dac_pktalloc(mlx_unit_t *const, int (*const)(),
    const caddr_t);
void mlx_dac_pktfree(mlx_unit_t *const, struct cmpkt *);
int mlx_dac_transport(mlx_unit_t *const, struct cmpkt *const);
void mlx_dac_fake_inquiry(mlx_hba_t *const, int,
    struct scsi_inquiry *const);
int mlx_dac_tran_tgt_init(dev_info_t *const, dev_info_t *const,
    scsi_hba_tran_t *const, struct scsi_device *const);
void mlx_dac_memfree(const mlx_unit_t *const, struct cmpkt *const);
int mlx_dac_abort(const mlx_unit_t *const, const struct cmpkt *const);
int mlx_dac_reset(mlx_unit_t *const, int);
int mlx_dacioc(mlx_unit_t *const, int, int, int);
int mlx_dacioc_nopkt(mlx_unit_t *const, int, int, int);
struct cmpkt *mlx_dacioc_pktalloc(mlx_unit_t *const, int,
    int, int, int *const);
ushort_t mlx_dacioc_ubuf_len(mlx_t *const, const uchar_t, ushort_t);
int mlx_dacioc_getarg(mlx_t *const, mlx_unit_t *const, int,
    struct cmpkt *const, mlx_ccb_t *const, int);
int mlx_dacioc_valid_args(mlx_t *const, mlx_ccb_t *const, const ushort_t,
    mlx_dacioc_t *const);
void mlx_dacioc_callback(struct cmpkt *const);
int mlx_dacioc_done(mlx_unit_t *const, struct cmpkt *const,
    mlx_ccb_t *const, mlx_dacioc_t *const, int, int);
int mlx_dacioc_update_conf_enq(mlx_unit_t *const, mlx_ccb_t *const,
    const ushort_t);
void mlx_dacioc_pktfree(mlx_unit_t *const, struct cmpkt *,
    mlx_ccb_t *const, mlx_dacioc_t *, int);
int mlx_flush_cache(dev_info_t *const, const ddi_reset_cmd_t);
int mlx_raid_nsd(mlx_t *);
int mlx_raid_level(mlx_t *, int);

/*
 * busops routines
 */
void mlx_childprop(dev_info_t *, dev_info_t *);

/*
 * devops routines
 */
int		mlx_probe(dev_info_t *devi);
int		mlx_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
int		mlx_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);
bool_t		mlx_propinit(dev_info_t *devi, mlx_unit_t *unit);
bool_t		mlx_prop_default(dev_info_t *dip, caddr_t propname,
			caddr_t propdefault);
mlx_t		*mlx_cardfind(dev_info_t *dip, mlx_unit_t *unit);
bool_t		mlx_cardinit(dev_info_t *dip, mlx_unit_t *unitp);
void		mlx_carduninit(dev_info_t *dip, mlx_unit_t *unitp);

/*
 * Interrupt routines
 */
uint_t		mlx_intr(caddr_t arg);

/*
 * External SCSA Interface
 */

int	mlx_tran_tgt_init(dev_info_t *, dev_info_t *,
			scsi_hba_tran_t *, struct scsi_device *);
int	mlx_tran_tgt_probe(struct scsi_device *, int (*)());
void	mlx_tran_tgt_free(dev_info_t *, dev_info_t *,
			scsi_hba_tran_t *, struct scsi_device *);
struct scsi_pkt *mlx_tran_init_pkt(struct scsi_address *,
			struct scsi_pkt *, struct buf *, int, int, int, int,
			int (*)(), caddr_t);
void	mlx_tran_destroy_pkt(struct scsi_address *, struct scsi_pkt *);

int	mlx_transport(struct scsi_address *, struct scsi_pkt *);
int	mlx_reset(struct scsi_address *, int);
int	mlx_abort(struct scsi_address *, struct scsi_pkt *);
int	mlx_getcap(struct scsi_address *, char *, int);
int	mlx_setcap(struct scsi_address *, char *, int, int);
int	mlx_capchk(char *, int, int *);

struct scsi_pkt *mlx_pktalloc(struct scsi_address *, int, int, int,
			int (*)(), caddr_t);
void	mlx_pktfree(struct scsi_pkt *);
struct scsi_pkt *mlx_dmaget(struct scsi_pkt *, opaque_t, int (*)(), caddr_t);
void	mlx_dmafree(struct scsi_address *, struct scsi_pkt *);
void	mlx_sync_pkt(struct scsi_address *, struct scsi_pkt *);


/*
 * Auto-config functions
 */
nops_t		*mlx_hbatype(dev_info_t *, int **, int *, bool_t);
bool_t		mlx_get_irq_pci(mlx_t *mlxp, int *regp, int reglen);
int		mlx_get_reg_pci(mlx_t *mlxp, int *regp, int reglen);
bool_t		mlx_cfg_init(dev_info_t *, mlx_t *);
uint_t		mlx_reg_prop(mlx_t *, int *, int, char *);
#if defined(i386)
bool_t		mlx_get_irq_eisa(mlx_t *mlxp, int *regp, int reglen);
int		mlx_get_reg_eisa(mlx_t *mlxp, int *regp, int reglen);
bool_t		eisa_probe(ioadr_t slotadr, ulong_t board_id);
bool_t		eisa_get_irq(ioadr_t slotadr, uchar_t *irqp);
bool_t		mlx_get_irq_mc(mlx_t *mlxp, int *regp, int reglen);
int		mlx_get_reg_mc(mlx_t *mlxp, int *regp, int reglen);
bool_t		mc_probe(ioadr_t slotadr, ulong_t board_id);
bool_t		mc_get_irq(ioadr_t slotadr, uchar_t *irqp);
#endif	/* defined(i386) */


bool_t		add_intr(dev_info_t *devi, uint_t inumber,
			ddi_iblock_cookie_t *iblockp, kmutex_t *mutexp,
			uint_t (*intr_func)(caddr_t), caddr_t intr_arg);
bool_t		mlx_find_irq(dev_info_t	*devi, uchar_t irq, int *intrp,
			int len, uint_t *inumber);
bool_t		mlx_xlate_irq_sid(mlx_t *mlxp);
bool_t		mlx_xlate_irq_no_sid(mlx_t *mlxp);
bool_t		mlx_intr_init(dev_info_t *devi, mlx_t *mlxp, caddr_t intr_arg);

/*
 * Extern variables
 */
extern ddi_dma_lim_t mlx_dma_lim;
extern kmutex_t mlx_global_mutex;
extern int mlx_pgsz;
extern int mlx_pgmsk;
extern int mlx_pgshf;
extern mlx_t *mlx_cards;

extern nops_t dac960_nops;
extern nops_t dmc960_nops;
extern nops_t dac960p_nops;

extern struct dev_ops mlx_ops;

extern int mlx_forceload;

extern int mlx_old_probe;

extern int mlx_disks_scsi;

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DKTP_MLX_MLXDEFS_H */
