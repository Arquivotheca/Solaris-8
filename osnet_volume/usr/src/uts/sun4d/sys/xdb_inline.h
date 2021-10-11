/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#ifndef	_SYS_XDB_INLINE_H
#define	_SYS_XDB_INLINE_H

#pragma ident	"@(#)xdb_inline.h	1.11	93/03/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef long long int bw_dynadata_t;
typedef long long int bw_tag_t;
typedef long long int cc_error_t;
typedef long long int ecache_tag_t;
typedef long long int ioc_dynatag_t;
typedef long long int ioc_sbustag_t;
typedef long long int mcsr_t;
typedef long long int refmiss_t;

#ifdef	SAS
extern int sas_mem_pages(void);
#endif	SAS

extern int		dyna_bb_control_get(void);
extern int		dyna_bb_control_set(void);
extern int		dyna_bb_status1_get(void);
extern int		dyna_bb_status2_get(void);
extern int		dyna_bb_status3_get(void);
extern bw_dynadata_t	dyna_bw_dynadata_get(int device_id);
extern void		dyna_bw_dynadata_set(bw_dynadata_t value,
			    int device_id);
extern void		dyna_bw_tag_set(bw_tag_t value, int line);
extern void		dyna_cc_cntrl_set(int value);
extern cc_error_t	dyna_cc_error_get(void);
extern refmiss_t	dyna_cc_refmiss_get(void);
extern void		dyna_cc_refmiss_set(refmiss_t value);
extern int		dyna_cpu_unit(int device_id);
extern void		dyna_ecache_tag_set(ecache_tag_t value, int line);
extern void		dyna_igr_set(int level, int assert);
extern int		dyna_imr_get(void);
extern void		dyna_imr_set(int mask);
extern int		dyna_intsid_clear(int sbus_level, int mask);
extern int		dyna_intsid_get(int sbus_level);
extern void		dyna_ioc_dynatag_set(ioc_dynatag_t value, int line);
extern void		dyna_ioc_sbustag_set(ioc_sbustag_t value, int line);
extern int		dyna_ipr_clear(int level);
extern int		dyna_ipr_get(void);
extern void		dyna_itr_set(int device_id, int target_id);
extern mcsr_t		dyna_mcsr_get(int device_id);
extern void		dyna_mcsr_set(mcsr_t value, int device_id);
extern void		dyna_sbi_release(int mask, int device_id);
extern int		dyna_sbi_take(int mask, int device_id);
extern int		dyna_sbusses_devicebase(int table_bit_number);
extern int		dyna_tick_get_limit(void);
extern int		dyna_tick_set_limit(int value);
extern void		vik_dcache_clear(void);
extern void		vik_icache_clear(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_XDB_INLINE_H */
