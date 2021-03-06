/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SCSI_ADAPTERS_IFPIO_H
#define	_SYS_SCSI_ADAPTERS_IFPIO_H

#pragma ident	"@(#)ifpio.h	1.4	99/07/29 SMI"

/*
 * Include any headers you depend on.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#define	IFP_IOC	('I' << 8)

/*
 * Get ifp device map ioctl.
 */

#define	IFPIOCGMAP		(IFP_IOC|1)	/* Get device map/wwn's */
#define	IFPIO_ADISC_ELS		(IFP_IOC|2)	/* Get ADISC info */
#define	IFPIO_FORCE_LIP		(IFP_IOC|3)	/* Force a LIP */
#define	IFPIO_LINKSTATUS	(IFP_IOC|4)	/* Link Status */
#define	IFPIO_DIAG_GET_FWREV	(IFP_IOC|5)	/* SunVTS diag get fw rev */
#define	IFPIO_DIAG_NOP		(IFP_IOC|6)	/* SunVTS diag NOOP */
#define	IFPIO_DIAG_MBOXCMDS	(IFP_IOC|7)	/* SunVTS diag mbox cmds */
#define	IFPIO_LOOPBACK_FRAME	(IFP_IOC|8)	/* Diagnostic loopback */
#define	IFPIO_DIAG_SELFTEST	(IFP_IOC|9)	/* Diagnostic selftest */
#define	IFPIO_BOARD_INFO	(IFP_IOC|10)	/* Get device id and rev's */
#define	IFPIO_FCODE_DOWNLOAD	(IFP_IOC|11)	/* Download fcode to flash */

struct ifp_board_info {
	uint16_t	ifpd_major;		/* FW major revision */
	uint16_t	ifpd_minor;		/* FW minor revision */
	uint16_t	ifpd_subminor;		/* FW subminor revision */
	uint16_t	chip_rev;		/* chip revision level */
	uint16_t	ctrl_id;		/* 2100 or 2200 */
};
typedef struct ifp_board_info ifp_board_info_t;

struct ifp_diag_fw_rev {
	uint16_t	ifpd_major;		/* FW major revision */
	uint16_t	ifpd_minor;		/* FW minor revision */
};
typedef struct ifp_diag_fw_rev ifp_diag_fw_rev_t;

struct ifp_lb_frame_cmd {
	uint16_t	options;		/* diag loop-back options */
	uint32_t	iter_cnt;		/* count of loopback ops */
	uint32_t	xfer_cnt;		/* transmit/receive xfer len */
	caddr_t	xmit_addr;			/* transmit data address */
	caddr_t	recv_addr;			/* receive data address */

	uint16_t	status;			/* completion status */
	uint16_t	crc_cnt;		/* crc error count */
	uint16_t	disparity_cnt;		/* disparity error count */
	uint16_t	frame_len_err_cnt;	/* frame length error count */
	uint32_t	fail_iter_cnt;		/* failing iteration count */
};
typedef struct ifp_lb_frame_cmd ifp_lb_frame_cmd_t;

/* defines for options field */
#define	LOOP_10BIT	0x0000		/* loopback at 10 bit interface */
#define	LOOP_1BIT	0x0001		/* loopback at 1 bit interface */
#define	LOOP_EXTERNAL	0x0002		/* loopback on external loop */
#define	LOOP_XMIT_OFF	0x0004		/* transmitter powered off */
#define	LOOP_XMIT_RAM	0x0010		/* xmit data from system ram */
#define	LOOP_RECV_RAM	0x0020		/* receive data to system ram */
#define	LOOP_ERR_STOP	0x0080		/* stop test on error */

struct ifp_diag_selftest {
	uint16_t	status;		/* completion status */
	uint16_t	test_num;	/* failing test number */
	uint16_t	fail_addr;	/* failure address */
	uint16_t	fail_data;	/* failure data */
};
typedef struct ifp_diag_selftest ifp_diag_selftest_t;

/* offset of the fcode from begining of file */
#define	FCODE_OFFSET	0x20

struct ifp_download {
	uint32_t	dl_fcode_len;	/* length of the fcode array */
	uint16_t	dl_chip_id;	/* Chip id for FCODE */
	uchar_t	dl_fcode[1];		/* the fcode */
};
typedef struct ifp_download ifp_download_t;


#define	IFP_NUM_ENTRIES_IN_MAP	127
#define	IFP_DIAG_MAX_MBOX	10

struct ifp_al_addr_pair {
	uchar_t	ifp_al_pa;
	uchar_t	ifp_hard_address;
	uchar_t	ifp_inq_dtype;
	uchar_t	ifp_node_wwn[FC_WWN_SIZE];
	uchar_t	ifp_port_wwn[FC_WWN_SIZE];
};
typedef struct ifp_al_addr_pair ifp_al_addr_pair_t;

struct ifp_al_map {
	short			ifp_count;
	ifp_al_addr_pair_t	ifp_addr_pair[IFP_NUM_ENTRIES_IN_MAP];
	ifp_al_addr_pair_t	ifp_hba_addr;
};
typedef struct ifp_al_map ifp_al_map_t;

struct adisc_payload {
	uint_t	adisc_hardaddr;
	uchar_t	adisc_portwwn[8];
	uchar_t	adisc_nodewwn[8];
	uint_t	adisc_dest;
};


struct rls_payload {
	uint_t	rls_portno;
	uint_t	rls_linkfail;
	uint_t	rls_syncfail;
	uint_t	rls_sigfail;
	uint_t	rls_primitiverr;
	uint_t	rls_invalidword;
	uint_t	rls_invalidcrc;
};
typedef struct rls_payload rls_payload_t;

struct ifp_target_stats {
	int	logouts_recvd;
					/*
					 * unsolicited LOGOs recvd from
					 * target
					 */
	int	task_mgmt_failures;
	int	data_ro_mismatches;
	int	dl_len_mismatches;
};
typedef struct ifp_target_stats ifp_target_stats_t;

struct ifp_stats {
	int	version;		/* version of this struct */
	int	lip_count;		/* lips forced by ifp */
	int	ncmds;			/* outstanding commands */
	ifp_target_stats_t tstats[127]; /* per target stats */
};
typedef struct ifp_stats ifp_stats_t;

/* XXX temp hack to get sf/socal ioctls used by luxadm to work with ifp */

#if !defined(SFIOCGMAP)
#define	SFIOCGMAP		((0xda << 8)|1)
#endif
#if !defined(FCIO_GETMAP)
#define	FCIO_GETMAP		(('F' << 8)|175)
struct lilpmap {
	ushort_t lilp_magic;
	ushort_t lilp_myalpa;
	uchar_t  lilp_length;
	uchar_t  lilp_list[127];
};
#endif

/* This is the max loopback transfer size */
#define	MAX_LOOPBACK		65536


#if !defined(FCIO_FORCE_LIP)
#define	FCIO_FORCE_LIP		(('F' << 8)|177)
#endif
#if !defined(FCIO_LINKSTATUS)
#define	FCIO_LINKSTATUS		(('F' << 8)|183)
#endif
#if !defined(FCIO_FCODE_MCODE_VERSION)
#define	FCIO_FCODE_MCODE_VERSION	(('F' << 8)|202)
#endif
struct ifp_fm_version {
	int	fcode_ver_len;
	int	mcode_ver_len;
	int	prom_ver_len;
	char	*fcode_ver;
	char	*mcode_ver;
	char	*prom_ver;
};

/* XXX end temp hack to get sf/socal ioctls used by luxadm to work with ifp */
struct ifp_diag_mbox {
	uint16_t	ifp_in_mbox[8];  /* in regs, from ISP */
	uint16_t	ifp_out_mbox[8]; /* out regs, to ISP */
};
typedef struct ifp_diag_mbox ifp_diag_mbox_t;

struct ifp_diag_regs {
	uint16_t	ifpd_mailbox[8];
	uint16_t	ifpd_hccr;
	uint16_t	ifpd_bus_sema;
	uint16_t	ifpd_isr;
	uint16_t	ifpd_icr;
	uint16_t	ifpd_icsr;
	uint16_t	ifpd_cdma_count;
	uint32_t	ifpd_cdma_addr;
	uint16_t	ifpd_cdma_status;
	uint16_t	ifpd_cdma_control;
	uint32_t	ifpd_rdma_count;
	uint32_t	ifpd_rdma_addr;
	uint16_t	ifpd_rdma_status;
	uint16_t	ifpd_rdma_control;
	uint32_t	ifpd_tdma_count;
	uint32_t	ifpd_tdma_addr;
	uint16_t	ifpd_tdma_status;
	uint16_t	ifpd_tdma_control;
	uint16_t	ifpd_risc_reg[16];
	uint16_t	ifpd_risc_psr;
	uint16_t	ifpd_risc_ivr;
	uint16_t	ifpd_risc_pcr;
	uint16_t	ifpd_risc_rar0;
	uint16_t	ifpd_risc_rar1;
	uint16_t	ifpd_risc_lcr;
	uint16_t	ifpd_risc_pc;
	uint16_t	ifpd_risc_mtr;
	uint16_t	ifpd_risc_sp;
	uint16_t	ifpd_request_in;
	uint16_t	ifpd_request_out;
	uint16_t	ifpd_response_in;
	uint16_t	ifpd_response_out;
	void		*ifpd_current_req_ptr;
	void		*ifpd_base_req_ptr;
	void		*ifpd_current_resp_ptr;
	void		*ifpd_base_resp_ptr;
};
typedef struct ifp_diag_regs ifp_diag_regs_t;

struct ifp_diag_cmd {
	int		ifp_cmds_count;		/* number of cmds */
	int		ifp_cmds_done;		/* number of cmds done */
	ifp_diag_regs_t	ifp_regs;		/* reg dump area */
	ifp_diag_mbox_t	ifp_mbox[IFP_DIAG_MAX_MBOX];	/* mbox values */
};
typedef struct ifp_diag_cmd ifp_diag_cmd_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_ADAPTERS_IFPIO_H */
