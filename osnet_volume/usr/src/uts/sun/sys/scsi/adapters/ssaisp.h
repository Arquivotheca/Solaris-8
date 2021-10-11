/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SCSI_ADAPTERS_SSAISP_H
#define	_SYS_SCSI_ADAPTERS_SSAISP_H

#pragma ident	"@(#)ssaisp.h	1.4	98/01/06 SMI"

/*
 * Definitions for Scsi error structure returned by SSA ISP firmware
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * SCSI FCP_RSP_INFO structure
 */
typedef struct fcp_scsi_bus_err {
	uchar_t  rsp_info_type;
	uchar_t  resv;
	ushort_t isp_status;		/* See scsi_pkt.h for possible values */
	ushort_t isp_state_flags;	/* See scsi_pkt.h for possible values */
	ushort_t isp_stat_flags;	/* See scsi_pkt.h for possible values */
} fcp_scsi_bus_err_t;

/*
 * Definitions of FCP_RSP_INFO fields
 */
/* Type */
#define	FCP_RSP_SCSI_BUS_ERR		1
#define	FCP_RSP_SCSI_PORT_ERR		2
#define	FCP_RSP_SOC_ERR			3

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_ADAPTERS_SSAISP_H */
