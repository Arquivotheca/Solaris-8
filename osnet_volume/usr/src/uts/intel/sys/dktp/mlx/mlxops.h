/*
 * Copyright (c) 1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DKTP_MLX_MLXOPS_H
#define	_SYS_DKTP_MLX_MLXOPS_H

#pragma ident	"@(#)mlxops.h	1.4	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef	struct mlxops {
	char		*mlx_chip;
	bool_t	(*mlx_probe)(dev_info_t *dip, int *regp, int len,
				int *pidp, int pidlen,
				bus_t bus_type, bool_t probing);
	bool_t	(*mlx_get_irq)(mlx_t *mlxp, int *regp, int reglen);
	bool_t	(*mlx_xlate_irq)(mlx_t *mlxp);
	int	(*mlx_rnumber)(mlx_t *mlxp, int *regp, int reglen);
	void	(*mlx_reset)(mlx_t *mlxp);
	bool_t	(*mlx_init)(mlx_t *mlxp, dev_info_t *dip);
	void	(*mlx_uninit)(mlx_t *mlxp, dev_info_t *dip);
	void	(*mlx_enable)(mlx_t *mlxp);
	void	(*mlx_disable)(mlx_t *mlxp);
	uchar_t	(*mlx_cready)(mlx_t *mlxp);
	bool_t	(*mlx_csend)(mlx_t *mlxp, void *hw_ccbp);
	uchar_t	(*mlx_iready)(mlx_t *mlxp);
	uchar_t	(*mlx_get_istat)(mlx_t *mlxp, void *hw_statp, int clear);
	int	(*mlx_geometry)(mlx_t *mlxp, struct scsi_address *ap,
				ulong_t bl);
} nops_t;


/* array of ptrs to ops tables for supported chip types */
extern	nops_t	*mlx_conf[];

#define	MLX_GET_IRQ(P, regp, reglen)	((P)->ops->mlx_get_irq)(P, regp, reglen)
#define	MLX_XLATE_IRQ(P)		((P)->ops->mlx_xlate_irq)(P)
#define	MLX_RNUMBER(P, regp, reglen)	((P)->ops->mlx_rnumber)(P, regp, reglen)

#define	MLX_RESET(P)			((P)->ops->mlx_reset)(P)
#define	MLX_INIT(P, d)			((P)->ops->mlx_init)(P, d)
#define	MLX_UNINIT(P, d)		((P)->ops->mlx_uninit)(P, d)
#define	MLX_ENABLE_INTR(P)		((P)->ops->mlx_enable)(P)
#define	MLX_DISABLE_INTR(P)		((P)->ops->mlx_disable)(P)
#define	MLX_CREADY(P)			((P)->ops->mlx_cready)(P)
#define	MLX_CSEND(P, ccb)		((P)->ops->mlx_csend)(P, ccb)
#define	MLX_IREADY(P)			((P)->ops->mlx_iready)(P)
#define	MLX_GET_ISTAT(P, st, clr)	((P)->ops->mlx_get_istat)(P, st, clr)
#define	MLX_GEOMETRY(P, ap, blk)	((P)->ops->mlx_geometry)(P, ap, blk)

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DKTP_MLX_MLXOPS_H */
