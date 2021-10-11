/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

#ifndef _SYS_FIBRE_CHANNEL_FCA_USOCIO_H
#define	_SYS_FIBRE_CHANNEL_FCA_USOCIO_H

#pragma ident	"@(#)usocio.h	1.2	99/10/18 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * usocio.h - SOC+ Driver user I/O interface dfinitions
 */
#define	USOCIOC					('F'<< 8)
#define	USOCIO_GETMAP				(USOCIOC | 175)
#define	USOCIO_BYPASS_DEV			(USOCIOC | 176)
#define	USOCIO_FORCE_LIP			(USOCIOC | 177)
#define	USOCIO_ADISC_ELS			(USOCIOC | 178)
#define	USOCIO_FORCE_OFFLINE			(USOCIOC | 179)
#define	USOCIO_LOADUCODE			(USOCIOC | 180)
#define	USOCIO_DUMPXRAM				(USOCIOC | 181)
#define	USOCIO_DUMPXRAMBUF			(USOCIOC | 182)
#define	USOCIO_LINKSTATUS			(USOCIOC | 183)
#define	USOCIO_LOOPBACK_INTERNAL		(USOCIOC | 190)
#define	USOCIO_LOOPBACK_MANUAL			(USOCIOC | 191)
#define	USOCIO_NO_LOOPBACK			(USOCIOC | 192)
#define	USOCIO_LOOPBACK_FRAME			(USOCIOC | 193)
#define	USOCIO_DIAG_NOP				(USOCIOC | 194)
#define	USOCIO_DIAG_RAW				(USOCIOC | 195)
#define	USOCIO_DIAG_XRAM			(USOCIOC | 196)
#define	USOCIO_DIAG_SOC				(USOCIOC | 197)
#define	USOCIO_DIAG_HCB				(USOCIOC | 198)
#define	USOCIO_DIAG_SOCLB			(USOCIOC | 199)
#define	USOCIO_DIAG_SRDSLB			(USOCIOC | 200)
#define	USOCIO_DIAG_EXTLB			(USOCIOC | 201)
#define	USOCIO_FCODE_MCODE_VERSION		(USOCIOC | 202)
#define	USOCIO_GET_LESB				(USOCIOC | 203)

#define	USOCIO_ADD_POOL				(USOCIOC | 301)
#define	USOCIO_ADD_BUFFER			(USOCIOC | 302)
#define	USOCIO_DELETE_POOL			(USOCIOC | 303)
#define	USOCIO_SEND_FRAME			(USOCIOC | 304)
#define	USOCIO_RCV_FRAME			(USOCIOC | 305)

struct adisc_payload {
	uint_t   adisc_magic;
	uint_t   adisc_hardaddr;
	uchar_t  adisc_portwwn[8];
	uchar_t  adisc_nodewwn[8];
	uint_t   adisc_dest;
};

struct usocio_lilpmap {
	ushort_t lilp_magic;
	ushort_t lilp_myalpa;
	uchar_t  lilp_length;
	uchar_t  lilp_list[127];
};

struct fclb {
	uchar_t  outbound_frame[24];
	uchar_t  inbound_frame[24];
};

struct usoc_fm_version {
	int	fcode_ver_len;
	int	mcode_ver_len;
	int	prom_ver_len;
	char	*fcode_ver;
	char	*mcode_ver;
	char	*prom_ver;
};

struct usoc_pstats {
	uint_t   port;		/* which port */
	uint_t   requests;	/* requests issued by this soc+ */
	uint_t   sol_resps;	/* solicited responses received */
	uint_t   unsol_resps;	/* unsolicited responses received */
	uint_t   lips;		/* forced loop initialization */
	uint_t   els_sent;	/* extended link service commands issued */
	uint_t   els_rcvd;	/* extended link service commands received */
	uint_t   abts;		/* aborts attempted */
	uint_t   abts_ok;	/* aborts successful */
	uint_t   offlines;	/* changes to offline state */
	uint_t   onlines;	/* changes to online state */
	uint_t   online_loops;	/* changes to online-loop state */
	uint_t   status[64];	/* soc+ response status */
};

struct usoc_stats {
	uint_t   version;			/* version */
	char	 drvr_name[MAXNAMELEN];		/* driver name */
	char	 fw_revision[MAXNAMELEN];	/* firmware revision */
	char	 node_wwn[17];			/* node WWN */
	char	 port_wwn[2][17];		/* port WWNs */
	uint_t	 parity_chk_enabled;
	uint_t   resets;			/* chip resets */
	uint_t   reqq_intrs;	/* request queue interrupts */
	uint_t   qfulls;	/* enqueue failure due to queue full */
	struct usoc_pstats pstats[2]; /* per port kstats */
};

struct usoc_add_pool {
	uint32_t	pool_fc4type;
	uint32_t	pool_buf_size;
};

struct usoc_delete_pool {
	uint32_t	pool_buf_count;
	uint32_t	pool_fc4type;
	uint64_t	*pool_tokens;
};

struct usoc_add_buffers {
	uint32_t	pool_id;
	uint32_t	pool_buf_count;
	uint64_t	*pool_tokens;
};

struct usoc_send_frame {
	uint32_t	sft_rsvd	: 8,
			sft_d_id	:24;
	uint32_t	sft_pattern;
	uint32_t	sft_pool_id;
};

struct usoc_rcv_frame {
	uint32_t	rcv_type;
	uint32_t	rcv_size;
	caddr_t		rcv_buf;
};

#ifdef __cplusplus
}
#endif

#endif /* !_SYS_FIBRE_CHANNEL_FCA_USOCIO_H */
