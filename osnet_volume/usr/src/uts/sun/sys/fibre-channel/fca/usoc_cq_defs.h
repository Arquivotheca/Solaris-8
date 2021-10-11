/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

#ifndef _SYS_FIBRE_CHANNEL_FCA_USOCCQ_DEFS_H
#define	_SYS_FIBRE_CHANNEL_FCA_USOCCQ_DEFS_H

#pragma ident	"@(#)usoc_cq_defs.h	1.2	99/10/18 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	USOC_CQE_PAYLOAD 60

/*
 * Data segment definition
 */
typedef struct fc_dataseg {
	uint32_t	fc_base;	/* Address of buffer. */
	uint32_t	fc_count;	/* Length of buffer. */
} fc_dataseg_t;
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
	uchar_t		cqe_payload[USOC_CQE_PAYLOAD];
	cq_hdr_t	cqe_hdr;
} cqe_t;

/*
 * CQ Entry types.
 */
#define	CQ_TYPE_NOP		0x00
#define	CQ_TYPE_OUTBOUND	0x01
#define	CQ_TYPE_INBOUND		0x02
#define	CQ_TYPE_SIMPLE		0x03
#define	CQ_TYPE_IO_WRITE	0x04
#define	CQ_TYPE_IO_READ		0x05
#define	CQ_TYPE_UNSOLICITED	0x06
#define	CQ_TYPE_BYPASS_DEV	0x06	/* supercedes unsolicited in SOC+ */
#define	CQ_TYPE_DIAGNOSTIC	0x07
#define	CQ_TYPE_OFFLINE		0x08
#define	CQ_TYPE_ADD_POOL	0x09	/* SOC+ enhancement */
#define	CQ_TYPE_DELETE_POOL	0x0a	/* SOC+ enhancement */
#define	CQ_TYPE_ADD_BUFFER	0x0b	/* SOC+ enhancement */
#define	CQ_TYPE_ADD_POOL_BUFFER	0x0c	/* SOC+ enhancement */
#define	CQ_TYPE_REQUEST_ABORT	0x0d	/* SOC+ enhnacement */
#define	CQ_TYPE_REQUEST_LIP	0x0e	/* SOC+ enhancement */
#define	CQ_TYPE_REPORT_MAP	0x0f	/* SOC+ enhancement */
#define	CQ_TYPE_RESPONSE	0x10
#define	CQ_TYPE_INLINE		0x20
#define	CQ_TYPE_REPORT_LESB	0x80	/* SOC+ enhancement */

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
	uint32_t	cq_address;
	uchar_t		cq_in;
	uchar_t		cq_out;
	uchar_t		cq_last_index;
	uchar_t		cq_seqno;
} usoc_cq_t;

/*
 * USOC header definition.
 */

typedef struct usoc_hdr {
	uint_t		sh_request_token;
	ushort_t	sh_flags;
	uchar_t		sh_class;
	uchar_t		sh_seg_cnt;
	uint_t		sh_byte_cnt;
} usoc_header_t;

/*
 * USOC header request packet definition.
 */

typedef struct usoc_request {
	usoc_header_t		sr_usoc_hdr;
	fc_dataseg_t		sr_dataseg[3];
	fc_frame_hdr_t		sr_fc_frame_hdr;
	cq_hdr_t		sr_cqhdr;
} usoc_request_t;

typedef	usoc_request_t usoc_header_request_t;

/*
 * USOC header response packet definition.
 */

typedef struct usoc_response {
	usoc_header_t		sr_usoc_hdr;
	uint_t			sr_usoc_status;
	fc_dataseg_t		sr_dataseg;
	uchar_t			sr_reserved[10];
	ushort_t 		sr_ncmds;
	fc_frame_hdr_t		sr_fc_frame_hdr;
	cq_hdr_t		sr_cqhdr;
} usoc_response_t;

/*
 * USOC data request packet definition.
 */

typedef struct usoc_data_request {
	usoc_header_t		sdr_usoc_hdr;
	fc_dataseg_t		sdr_dataseg[6];
	cq_hdr_t		sdr_cqhdr;
} usoc_data_request_t;

/*
 * SOC+ (only) command-only packet definitiion
 */

typedef	struct usoc_cmdonly_request {
	usoc_header_t	scr_usoc_hdr;
	uchar_t		reserved[48];
	cq_hdr_t	scr_cqhdr;
} usoc_cmdonly_request_t;

/*
 * SOC+ (only) diagnostic request packet definition
 */

typedef	struct usoc_diag_request {
	usoc_header_t	sdr_usoc_hdr;
	uint_t		sdr_diag_cmd;
	uchar_t		reserved[44];
	cq_hdr_t	sdr_cqhdr;
} usoc_diag_request_t;

#define	USOC_DIAG_NOP		0x00
#define	USOC_DIAG_INT_LOOP	0x01
#define	USOC_DIAG_EXT_LOOP	0x02
#define	USOC_DIAG_REM_LOOP	0x03
#define	USOC_DIAG_XRAM_TEST	0x04
#define	USOC_DIAG_SOC_TEST	0x05
#define	USOC_DIAG_HCB_TEST	0x06
#define	USOC_DIAG_SOCLB_TEST	0x07
#define	USOC_DIAG_SRDSLB_TEST	0x08
#define	USOC_DIAG_EXTOE_TEST	0x09

/*
 * SOC+ (only) pool request packet definition
 */

typedef	struct usoc_pool_request {
	usoc_header_t		spr_usoc_hdr;
	uint_t			spr_pool_id;
	uint_t			spr_header_mask;
	uint_t			spr_buf_size;
	uint_t			spr_n_entries;
	uchar_t			reserved[8];
	fc_frame_hdr_t		spr_fc_frame_hdr;
	cq_hdr_t		spr_cqhdr;
} usoc_pool_request_t;

#define	USOCPR_MASK_RCTL	0x800000
#define	USOCPR_MASK_DID		0x700000
#define	USOCPR_MASK_SID		0x070000
#define	USOCPR_MASK_TYPE	0x008000
#define	USOCPR_MASK_F_CTL	0x007000
#define	USOCPR_MASK_SEQ_ID	0x000800
#define	USOCPR_MASK_D_CTL	0x000400
#define	USOCPR_MASK_SEQ_CNT	0x000300
#define	USOCPR_MASK_OX_ID	0x0000f0
#define	USOCPR_MASK_PARAMETER	0x0000f0


/*
 * Macros for flags field
 *
 * values used in both RSP's and REQ's
 */
#define	USOC_PORT_B		0x0001	/* entry to/from USOC Port B */
#define	USOC_FC_HEADER		0x0002	/* this entry contains an FC_HEADER */
/*
 *	REQ: this request is supplying buffers
 *	RSP: this pkt is unsolicited
 */
#define	USOC_UNSOLICITED	0x0080

/*
 * values used only for REQ's
 */
#define	USOC_NO_RESPONSE	0x0004	/* generate niether RSP nor INT */
#define	USOC_NO_INTR		0x0008	/* generate RSP only */
#define	USOC_XFER_RDY		0x0010	/* issue a XFRRDY packet for cmd */
#define	USOC_IGNORE_RO		0x0020	/* ignore FC_HEADER relative offset */
#define	USOC_RESP_HEADER	0x0200	/* alwyas return frame header */

/*
 * values used only for RSP's
 */
#define	USOC_COMPLETE		0x0040	/* previous CMD completed. */
#define	USOC_STATUS		0x0100	/* a USOC status change has occurred */

/*
 * usoc status values
 */
#define	USOC_OK			0x0
#define	USOC_P_RJT		0x2
#define	USOC_F_RJT		0x3
#define	USOC_P_BSY		0x4
#define	USOC_F_BSY		0x5
#define	USOC_ONLINE		0x10
#define	USOC_OLDPORT_ONLINE	0x10
#define	USOC_OFFLINE		0x11
#define	USOC_TIMEOUT		0x12
#define	USOC_OVERRUN		0x13
#define	USOC_LOOP_ONLINE	0x14
#define	USOC_UNKNOWN_CQ_TYPE	0x20	/* unknown usoc request type */
#define	USOC_BAD_SEG_CNT	0x21	/* insufficient # of segments */
#define	USOC_MAX_XCHG_EXCEEDED	0x22	/* already MAX_XCHG exchanges active */
#define	USOC_BAD_XID		0x23	/* inactive/invalid XID specified */
#define	USOC_XCHG_BUSY		0x24	/* already have a request for xchg */
#define	USOC_BAD_POOL_ID	0x25
#define	USOC_INSUFFICIENT_CQES	0x26	/* not enough CQE's in request */
#define	USOC_ALLOC_FAIL		0x27	/* when internal alloc fails */
#define	USOC_BAD_SID		0x28	/* S_ID soesn't match nport's id */
#define	USOC_NO_SEQ_INIT	0x29	/* don't have Sequence Initiative */
#define	USOC_ABORTED		0x30	/* rcvd BA_ACC for host abort request */
#define	USOC_ABORT_FAILED	0x31	/* rcvd BA_RJT for host abort request */
#define	USOC_DIAG_BUSY		0x32	/* diagnostics currently busy */
#define	USOC_DIAG_ILL_RQST	0x33	/* diagnostics illegal request */
#define	USOC_INCOMPLETE_DMA_ERROR	0x34	/* sbus dma is not completed */
#define	USOC_FC_CRC_ERROR	0x35	/* crc error detected */
#define	USOC_OPEN_FAIL		0x36	/* open to target fail */
#define	USOC_MAX_STATUS		0x37

typedef struct usoc_xlat_error {
	uint_t		usoc_status;
	uint32_t	pkt_state;
	uint32_t	pkt_reason;
}usoc_xlat_error_t;

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

#define	CQ_SOLICITED_OK		CQ_RESPONSE_0
#define	CQ_SOLICITED_BAD	CQ_RESPONSE_1
#define	CQ_UNSOLICITED		CQ_RESPONSE_2


typedef struct usoc_request_descriptor {
	usoc_request_t	*srd_sp;
	uint_t		srd_sp_count;

	caddr_t		srd_cmd;
	uint_t		srd_cmd_count;

	caddr_t		srd_data;
	uint_t		srd_data_count;
} usoc_request_desc_t;

/*
 * Add pool request pkt - similar to the soc_pool_request above
 */
typedef	struct usoc_add_pool_request {
	usoc_header_t		uapr_usoc_hdr;	   /* soc+ header */
	uint32_t		uapr_pool_id;	   /* host supplied pool id */
	uint32_t		uapr_header_mask;  /* byte mask for Hdr */
	uint32_t		uapr_buf_size;	   /* size of each buffer */
	uint16_t		uapr_timeout;	   /* req. timeout in secs */
	uchar_t			uapr_reserved[10]; /* pad to make 64 bytes */
	fc_frame_hdr_t		uapr_fc_frame_hdr; /* frame header */
	cq_hdr_t		uapr_cqhdr;	   /* CQ header */
} usoc_add_pool_request_t;


/*
 * header mask definitions. The header mask will be a OR of these values
 */
#define	UAPR_MASK_RCTL		0x800000
#define	UAPR_MASK_DID		0x700000
#define	UAPR_MASK_SID		0x070000
#define	UAPR_MASK_F_CTL		0x00E000
#define	UAPR_MASK_TYPE		0x001000
#define	UAPR_MASK_SEQ_ID	0x000800
#define	UAPR_MASK_D_CTL		0x000400
#define	UAPR_MASK_SEQ_CNT	0x000300
#define	UAPR_MASK_OX_ID		0x0000C0
#define	UAPR_MASK_RX_ID		0x000030
#define	UAPR_MASK_PARAMETER	0x00000f

/*
 * USOC delete pool request packet definition
 */
typedef struct usoc_delete_pool_request {
	usoc_header_t		udpr_usoc_hdr;	 /* soc+ header */
	uint32_t		udpr_pool_id;	 /* pool id to delete */
	uchar_t			udpr_reserved[44]; /* pad to make 64 bytes */
	cq_hdr_t		udpr_cqhdr;	 /* CQ header */
} usoc_delete_pool_request_t;


#define	NUM_USOC_BUFS		5
#define	USOC_MAX_UBUFS		65535

/*
 * USOC add buffer request packet definition
 */
typedef struct usoc_add_buf_request {
	usoc_header_t		uabr_usoc_hdr;	 /* soc+ header */
	uint32_t 		uabr_pool_id;	 /* pool to add buffer to */
	uint16_t		uabr_nentries;	 /* no.of bufs to add to pool */
	uint16_t		pad1;
	struct buf_desc	{
		uint16_t	token;
		uint16_t	pad2;
		uint32_t	address;
	} uabr_buf_descriptor[NUM_USOC_BUFS];	 /* upto 7 buffer descriptors */
	cq_hdr_t		uabr_cqhdr;	 /* CQ header */
} usoc_add_buf_request_t;

/*
 * The tokens are defined to be poolid ORed with a 16bit sequentially
 * increasing token (so we have actually imposed a limit of (2^16 - 1) or
 * 65535 unsolicted buffers in a pool which should be large enough ;) for
 * now. Need to explore if it is possible to do this any better. Since the
 * FC4 TYPE values are defined to be 8 bits only, we can even shift the poolid
 * by 24 giving us a whopping (2^24 - 1) buffers in a pool.
 */
#define	USOC_TOKEN_SHIFT	16
#define	USOC_GET_POOLID_FROM_TOKEN(x) \
	((((uint32_t)(x)) >> USOC_TOKEN_SHIFT) << USOC_TOKEN_SHIFT)
#define	USOC_GET_POOLID_FROM_TYPE(x)	((x) << USOC_TOKEN_SHIFT)
#define	USOC_GET_TOKEN(x, y)	(((x) << USOC_TOKEN_SHIFT) | (y))
#define	USOC_GET_UCODE_TOKEN(x)	((x) & ~(USOC_GET_POOLID_FROM_TOKEN((x))))

/*
 * USOC unsolicited response structure. A response structure will be
 * returned to the host when one full unsolicited sequence has been
 * received.
 */

typedef struct usoc_unsol_response {
	usoc_header_t	unsol_resp_usoc_hdr;	/* soc+ header */
	uint32_t	unsol_resp_status;	/* status */
	uint16_t	unsol_resp_nentries;	/* no.of bufs in the sequence */
	uint16_t	unsol_resp_tokens[7];	/* tokens of buffers */
	uint16_t	unsol_resp_last_cnt;	/* no. of bytes in last buf */
	uint16_t	unsol_resp_nbuf_avail;	/* no. of bufs available */
	fc_frame_hdr_t	unsol_resp_fc_frame_hdr; /* frame header */
	cq_hdr_t	unsol_resp_cqhdr;	/* CQ Header */
} usoc_unsol_resp_t;

#ifdef __cplusplus
}
#endif

#endif /* !_SYS_FIBRE_CHANNEL_FCA_USOCCQ_DEFS_H */
