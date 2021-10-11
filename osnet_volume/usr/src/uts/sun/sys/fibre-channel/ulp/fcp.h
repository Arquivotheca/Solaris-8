/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FIBRE_CHANNEL_ULP_FCP_H
#define	_SYS_FIBRE_CHANNEL_ULP_FCP_H

#pragma ident	"@(#)fcp.h	1.1	99/07/21 SMI"

/*
 * Frame format and protocol definitions for transferring
 * commands and data between a SCSI initiator and target
 * using an FC4 serial link interface.
 *
 * this file originally taken from fc4/fcp.h
 */

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>


/*
 * FCP Device Data Frame Information Categories
 */
#define	FCP_SCSI_DATA		0x01	/* frame contains SCSI data */
#define	FCP_SCSI_CMD		0x02	/* frame contains SCSI command */
#define	FCP_SCSI_RSP		0x03	/* frame contains SCSI response */
#define	FCP_SCSI_XFER_RDY	0x05	/* frame contains xfer rdy block */


/*
 * fcp SCSI control structure
 */
typedef struct fcp_cntl {

	uchar_t	cntl_reserved_0;		/* reserved */

#if	defined(_BIT_FIELDS_HTOL)

	uchar_t	cntl_reserved_1	: 5,		/* reserved */
		cntl_qtype	: 3;		/* tagged queueing type */

	uchar_t	cntl_kill_tsk	: 1,		/* terminate task */
		cntl_clr_aca	: 1,		/* clear aca */
		cntl_reset	: 1,		/* reset */
		cntl_reserved_2	: 2,		/* reserved */
		cntl_clr_tsk	: 1,		/* clear task set */
		cntl_abort_tsk	: 1,		/* abort task set */
		cntl_reserved_3	: 1;		/* reserved */

	uchar_t	cntl_reserved_4	: 6,		/* reserved */
		cntl_read_data	: 1,		/* initiator read */
		cntl_write_data	: 1;		/* initiator write */

#elif	defined(_BIT_FIELDS_LTOH)

	uchar_t	cntl_qtype	: 3,		/* tagged queueing type */
		cntl_reserved_1	: 5;		/* reserved */

	uchar_t	cntl_reserved_3	: 1,		/* reserved */
		cntl_abort_tsk	: 1,		/* abort task set */
		cntl_clr_tsk	: 1,		/* clear task set */
		cntl_reserved_2	: 2,		/* reserved */
		cntl_reset	: 1,		/* reset */
		cntl_clr_aca	: 1,		/* clear aca */
		cntl_kill_tsk	: 1;		/* terminate task */

	uchar_t	cntl_write_data	: 1,		/* initiator write */
		cntl_read_data	: 1,		/* initiator read */
		cntl_reserved_4	: 6;		/* reserved */

#else
#error	one of _BIT_FIELDS_HTOL or _BIT_FIELDS_LTOH must be defined
#endif

} fcp_cntl_t;

/*
 * fcp SCSI control tagged queueing types - cntl_qtype
 */
#define	FCP_QTYPE_SIMPLE	0		/* simple queueing */
#define	FCP_QTYPE_HEAD_OF_Q	1		/* head of queue */
#define	FCP_QTYPE_ORDERED	2		/* ordered queueing */
#define	FCP_QTYPE_ACA_Q_TAG	4		/* ACA queueing */
#define	FCP_QTYPE_UNTAGGED	5		/* Untagged */


/*
 * fcp SCSI entity address
 *
 * ent_addr_0 is always the first and highest layer of
 * the hierarchy.  The depth of the hierarchy of addressing,
 * up to a maximum of four layers, is arbitrary and
 * device-dependent.
 */
typedef struct fcp_ent_addr {
	ushort_t ent_addr_0;		/* entity address 0 */
	ushort_t ent_addr_1;		/* entity address 1 */
	ushort_t ent_addr_2;		/* entity address 2 */
	ushort_t ent_addr_3;		/* entity address 3 */
} fcp_ent_addr_t;


/*
 * maximum size of SCSI cdb in fcp SCSI command
 */
#define	FCP_CDB_SIZE		16
#define	FCP_LUN_SIZE		8

/*
 * FCP SCSI command payload
 */
typedef struct fcp_cmd {
	fcp_ent_addr_t	fcp_ent_addr;			/* entity address */
	fcp_cntl_t	fcp_cntl;			/* SCSI options */
	uchar_t		fcp_cdb[FCP_CDB_SIZE];		/* SCSI cdb */
	int		fcp_data_len;			/* data length */
} fcp_cmd_t;


/*
 * fcp SCSI status
 */
typedef struct fcp_status {
	ushort_t reserved_0;			/* reserved */

#if	defined(_BIT_FIELDS_HTOL)

	uchar_t	reserved_1	: 4,		/* reserved */
		resid_under	: 1,		/* resid non-zero */
		resid_over	: 1,		/* resid non-zero */
		sense_len_set	: 1,		/* sense_len non-zero */
		rsp_len_set	: 1;		/* response_len non-zero */

#elif	defined(_BIT_FIELDS_LTOH)

	uchar_t	rsp_len_set	: 1,		/* response_len non-zero */
		sense_len_set	: 1,		/* sense_len non-zero */
		resid_over	: 1,		/* resid non-zero */
		resid_under	: 1,		/* resid non-zero */
		reserved_1	: 4;		/* reserved */

#endif
	uchar_t	scsi_status;			/* status of cmd */
} fcp_status_t;


/*
 * fcp SCSI response payload
 */
typedef struct fcp_rsp {
	uint32_t	reserved_0;			/* reserved */
	uint32_t	reserved_1;			/* reserved */
	union {
		fcp_status_t	fcp_status;		/* command status */
		uint32_t	i_fcp_status;
	} fcp_u;
	uint32_t	fcp_resid;		/* resid of operation */
	uint32_t	fcp_sense_len;		/* sense data length */
	uint32_t	fcp_response_len;	/* response data length */
	/*
	 * 'm' bytes of scsi response info follow
	 * 'n' bytes of scsi sense info follow
	 */
} fcp_rsp_t;


/* MAde 256 for sonoma as it wants to give tons of sense info */
#define	FCP_MAX_RSP_IU_SIZE	256


/*
 * fcp rsp_info field format
 */

struct fcp_rsp_info {
	uchar_t		resvd1;
	uchar_t		resvd2;
	uchar_t		resvd3;
	uchar_t		rsp_code;
	uchar_t		resvd4;
	uchar_t		resvd5;
	uchar_t		resvd6;
	uchar_t		resvd7;
};

/*
 * rsp_code definitions
 */
#define		FCP_NO_FAILURE			0x0
#define		FCP_DL_LEN_MISMATCH		0x1
#define		FCP_CMND_INVALID		0x2
#define		FCP_DATA_RO_MISMATCH		0x3
#define		FCP_TASK_MGMT_NOT_SUPPTD	0x4
#define		FCP_TASK_MGMT_FAILED		0x5


#ifdef	THIS_NEEDED_YET

/*
 * fcp scsi_xfer_rdy payload
 */
typedef struct fcp_xfer_rdy {
	ulong64_t	fcp_seq_offset;		/* relative offset */
	ulong64_t	fcp_burst_len;		/* buffer space */
	ulong64_t	reserved;		/* reserved */
} fcp_xfer_rdy_t;

#endif	/* THIS_NEEDED_YET */

/*
 * fcp PRLI payload
 */
struct fcp_prli {
	uchar_t		type;
	uchar_t		resvd1;			/* rsvd by std */

#if	defined(_BIT_FIELDS_HTOL)

	uint16_t	orig_process_assoc_valid : 1,
			resp_process_assoc_valid : 1,
			establish_image_pair : 1,
			resvd2 : 13;		/* rsvd by std */

#elif	defined(_BIT_FIELDS_LTOH)

	uint16_t	resvd2 : 13;		/* rsvd by std */
			establish_image_pair : 1,
			resp_process_assoc_valid : 1,
			orig_process_assoc_valid : 1;

#endif

	uint32_t	orig_process_associator;
	uint32_t	resp_process_associator;

#if	defined(_BIT_FIELDS_HTOL)

	uint32_t	resvd3 : 25,		/* rsvd by std */
			data_overlay_allowed : 1,
			initiator_fn : 1,
			target_fn : 1,
			cmd_data_mixed : 1,
			data_resp_mixed : 1,
			read_xfer_rdy_disabled : 1,
			write_xfer_rdy_disabled : 1;

#elif	defined(_BIT_FIELDS_LTOH)

	uint32_t	write_xfer_rdy_disabled : 1;
			read_xfer_rdy_disabled : 1,
			data_resp_mixed : 1,
			cmd_data_mixed : 1,
			target_fn : 1,
			initiator_fn : 1,
			data_overlay_allowed : 1,
			resvd3 : 25;		/* rsvd by std */

#endif

};

#ifdef	THIS_NEEDED_YET

/*
 * fcp PRLI ACC payload
 */
struct fcp_prli_acc {
	uchar_t		type;
	uchar_t		resvd1;

	uint32_t	orig_process_assoc_valid : 1;
	uint32_t	resp_process_assoc_valid : 1;
	uint32_t	image_pair_establsihed : 1;
	uint32_t	resvd2 : 1;
	uint32_t	accept_response_code : 4;
	uint32_t	resvd3 : 8;
	uint32_t	orig_process_associator;
	uint32_t	resp_process_associator;
	uint32_t	resvd4 : 26;
	uint32_t	initiator_fn : 1;
	uint32_t	target_fn : 1;
	uint32_t	cmd_data_mixed : 1;
	uint32_t	data_resp_mixed : 1;
	uint32_t	read_xfer_rdy_disabled : 1;
	uint32_t	write_xfer_rdy_disabled : 1;

};

#endif	/* THIS_NEEDED_YET */


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FIBRE_CHANNEL_ULP_FCP_H */
