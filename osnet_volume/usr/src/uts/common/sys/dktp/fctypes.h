/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_DKTP_FCTYPES_H
#define	_SYS_DKTP_FCTYPES_H

#pragma ident	"@(#)fctypes.h	1.6	94/09/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	DMULT_MAXCNT	2
#define	DUPLX_MAXCNT	2

#define	ds_kstat	ds_cmn.dsc_kstat
#define	ds_mutex	ds_cmn.dsc_mutex
#define	ds_tgcomobjp	ds_cmn.dsc_tgcomobjp

struct  fc_data_cmn {
	kstat_t		*dsc_kstat;
	kmutex_t	dsc_mutex;
	opaque_t	dsc_tgcomobjp;
};

struct	fc_data {
	struct fc_data_cmn ds_cmn;

	short		ds_flag;
	short		ds_outcnt;
	struct diskhd	ds_tab;
	opaque_t	ds_queobjp;
};

#define	ds_actf 	ds_tab.b_actf
#define	ds_actl 	ds_tab.b_actl
#define	ds_waitcnt	ds_tab.b_bcount
#define	ds_bp 		ds_actf

struct  fc_que  {
	struct fc_que   *next;
	opaque_t	fc_qobjp;
	struct buf	*fc_bp;
	short		fc_outcnt;
	short		fc_maxcnt;
};

struct	duplx_data {
	struct fc_data_cmn ds_cmn;

	struct fc_que   ds_readq;
	struct fc_que   ds_writeq;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_FCTYPES_H */
