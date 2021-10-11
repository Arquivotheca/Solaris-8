/*
 * Copyright (c) 1995,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SOC_CQ_DEFS_H
#define	_SYS_SOC_CQ_DEFS_H

#pragma ident	"@(#)soc_cq_defs.h	1.8	98/01/06 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/fc4/fc.h>

#define	SOC_CQE_PAYLOAD 60

/*
 * define the CQ_HEADER for the soc command queue.
 */

typedef struct	cq_hdr {
	uchar_t	cq_hdr_count;
	uchar_t	cq_hdr_type;
	uchar_t	cq_hdr_flags;
	uchar_t	cq_hdr_seqno;
} cq_hdr_t;

/*
 * Command Queue entry description.
 */

typedef struct cqe {
	uchar_t		cqe_payload[SOC_CQE_PAYLOAD];
	cq_hdr_t	cqe_hdr;
} cqe_t;

/*
 * CQ Entry types.
 */

#define	CQ_TYPE_OUTBOUND	0x01
#define	CQ_TYPE_INBOUND		0x02
#define	CQ_TYPE_SIMPLE		0x03
#define	CQ_TYPE_IO_WRITE	0x04
#define	CQ_TYPE_IO_READ		0x05
#define	CQ_TYPE_UNSOLICITED	0x06
#define	CQ_TYPE_DIAGNOSTIC	0x07
#define	CQ_TYPE_OFFLINE		0x08
#define	CQ_TYPE_RESPONSE	0x10
#define	CQ_TYPE_INLINE		0x20

/*
 * CQ Entry Flags
 */

#define	CQ_FLAG_CONTINUATION	0x01
#define	CQ_FLAG_FULL		0x02
#define	CQ_FLAG_BADHEADER	0x04
#define	CQ_FLAG_BADPACKET	0x08

/*
 * CQ Descriptor Definition.
 */

typedef	struct cq {
	/*
	 * cq_address is a pointer to the head of a queue that
	 * the soc firmware reads as a 32-bit value.  It's best
	 * to keep it in XRAM where it's used relative to the
	 * start of XRAM.  In this case the low bit of the
	 * address should be set.
	 */
	uint32_t	cq_address;
	uchar_t		cq_in;
	uchar_t		cq_out;
	uchar_t		cq_last_index;
	uchar_t		cq_seqno;
} soc_cq_t;

/*
 * SOC header definition.
 */

typedef struct soc_hdr {
	/*
	 * The sh_request_token will be used to uniquely identify the
	 * request so we can find it when it comes back.  To do so we put
	 * the low 32-bits of the pointer address of the associated fibre
	 * channel request structure. We should never attempt to dereference
	 * this field as a pointer.
	 */
	uint32_t	sh_request_token;
	ushort_t 	sh_flags;
	uchar_t		sh_class;
	uchar_t		sh_seg_cnt;
	uint32_t	sh_byte_cnt;
} soc_header_t;

/*
 * SOC header request packet definition.
 */

typedef struct soc_request {
	soc_header_t		sr_soc_hdr;
	fc_dataseg_t		sr_dataseg[3];
	fc_frame_header_t	sr_fc_frame_hdr;
	cq_hdr_t		sr_cqhdr;
} soc_request_t;

/*
 * SOC header response packet definition.
 */

typedef struct soc_response {
	soc_header_t		sr_soc_hdr;
	uint_t			sr_soc_status;
	fc_dataseg_t		sr_dataseg;
	uchar_t			sr_reserved[12];
	fc_frame_header_t	sr_fc_frame_hdr;
	cq_hdr_t		sr_cqhdr;
} soc_response_t;

/*
 * SOC data request packet definition.
 */

typedef struct soc_data_request {
	soc_header_t		sdr_soc_hdr;
	fc_dataseg_t		sdr_dataseg[6];
	cq_hdr_t		sdr_cqhdr;
} soc_data_request_t;

/*
 * Macros for flags field
 *
 * values used in both RSP's and REQ's
 */
#define	SOC_PORT_B	0x0001	/* entry to/from SOC Port B */
#define	SOC_FC_HEADER	0x0002	/* this entry contains an FC_HEADER */
/*
 *	REQ: this request is supplying buffers
 *	RSP: this pkt is unsolicited
 */
#define	SOC_UNSOLICITED	0x0080

/*
 * values used only for REQ's
 */
#define	SOC_NO_RESPONSE	0x0004 /* generate niether RSP nor INT */
#define	SOC_NO_INTR	0x0008 /* generate RSP only */
#define	SOC_XFER_RDY	0x0010 /* issue a XFRRDY packet for this cmd */
#define	SOC_IGNORE_RO	0x0020 /* ignore FC_HEADER relative offset */

/*
 * values used only for RSP's
 */
#define	SOC_COMPLETE	0x0040 /* previous CMD completed. */
#define	SOC_STATUS	0x0100 /* a SOC status change has occurred */

/*
 * Macros for SOC Status indication.
 */
#define	SOC_OK			0
#define	SOC_P_RJT		2
#define	SOC_F_RJT		3
#define	SOC_P_BSY		4
#define	SOC_F_BSY		5
#define	SOC_ONLINE		0x10
#define	SOC_OFFLINE		0x11
#define	SOC_TIMEOUT		0x12
#define	SOC_OVERRUN		0x13
#define	SOC_UNKOWN_CQ_TYPE	0x20
#define	SOC_BAD_SEG_CNT		0x21
#define	SOC_MAX_XCHG_EXCEEDED	0x22
#define	SOC_BAD_XID		0x23
#define	SOC_XCHG_BUSY		0x24
#define	SOC_BAD_POOL_ID		0x25
#define	SOC_INSUFFICIENT_CQES	0x26
#define	SOC_ALLOC_FAIL		0x27
#define	SOC_BAD_SID		0x28
#define	SOC_NO_SEG_INIT		0x29


#define	CQ_SUCCESS	0x0
#define	CQ_FAILURE	0x1
#define	CQ_FULL		0x2

#define	CQ_REQUEST_0	0
#define	CQ_REQUEST_1	1
#define	CQ_REQUEST_2	2
#define	CQ_REQUEST_3	3

#define	CQ_RESPONSE_0	0
#define	CQ_RESPONSE_1	1
#define	CQ_RESPONSE_2	2
#define	CQ_RESPONSE_3	3

#define	CQ_SOLICITED	CQ_RESPONSE_0
#define	CQ_UNSOLICITED	CQ_RESPONSE_1


typedef struct soc_request_descriptor {
	soc_request_t	*srd_sp;
	uint_t		srd_sp_count;

	caddr_t		srd_cmd;
	uint_t		srd_cmd_count;

	caddr_t		srd_data;
	uint_t		srd_data_count;
} soc_request_desc_t;


#ifdef __cplusplus
}
#endif

#endif /* !_SYS_SOC_CQ_DEFS_H */
