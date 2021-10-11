/*
 * Copyright (c) 1995 - 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This file is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify this file without charge, but are not authorized to
 * license or distribute it to anyone else except as part of a product
 * or program developed by the user.
 *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * This file is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS FILE
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even
 * if Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 * Hardware specific driver declarations for the Intel IPRB-Based Cards
 * driver conforming to the Generic LAN Driver model.
 */

#ifndef	_IPRB_H
#define	_IPRB_H

#pragma ident	"@(#)iprb.h	1.18	99/07/22 SMI"

#ifdef __cplusplus
extern "C" {
#endif


#define	MAX_DEVICES	256
/* debug flags */
#define	IPRBTRACE	0x01
#define	IPRBERRS	0x02
#define	IPRBRECV	0x04
#define	IPRBDDI		0x08
#define	IPRBSEND	0x10
#define	IPRBINT		0x20
#define	IPRBTEST	0x40

#ifdef DEBUG
#define	IPRBDEBUG 1
#endif	/* DEBUG */

/* Misc */
#define	IPRBHIWAT	65536		/* driver flow control high water */
#define	IPRBLOWAT	4096		/* driver flow control low water */
#define	IPRBMAXPKT	1500		/* maximum media frame size */
#define	IPRB_FRAMESIZE	2048		/* Allocation size of frame */
#define	IPRBIDNUM	0		/* should be a unique id;zero works */


#define	IPRB_MAX_RECVS		256	/* Maximum number of receive frames */
#define	IPRB_MAX_XMITS		256	/* Maximum number of cmd/xmit buffers */
#define	IPRB_DEFAULT_RECVS	32	/* Default receives if no .conf */
#define	IPRB_FREELIST_SIZE	48	/* Default initial size of freelist */
#define	IPRB_MAX_RXBCOPY	256	/* Max. Pkt. size to bcopy() */
#define	IPRB_DEFAULT_XMITS	128	/* Default cmds/xmits if no .conf */
#define	IPRB_MAX_FRAGS		4	/* max dma-able fragments in mblk */
					/* before we pullupmsg the message */

#define	IPRB_NULL_PTR		0xffffffffUL	/* INTEL NULL pointer */
#define	IPRB_SUB_VENDOR_ID	0x0c	/* Subsystem vendor id */
#define	IPRB_SUB_SYSTEM_ID	0x0b	/* Subsystem id */
#define	IPRB_PHYIDH		2	/* Phy id. register */
#define	IPRB_PHYIDL		3	/* Phy id. register */

#define	D101A4_REV_ID		4	/* 82558 A4 stepping */
#define	D101B0_REV_ID		5	/* 82558 B0 stepping */
#define	D101MA_REV_ID		8	/* 82559 A0 stepping */
#define	D101S_REV_ID		9	/* 82559 S */
#define	D101_MICROCODE_LENGTH   102	/* Microcode Length in DWORDS */
#define	D101M_MICROCODE_LENGTH  134	/* Microcode length in DWORDS */
#define	D101S_MICROCODE_LENGTH  134	/* Microcode length in DWORDS */


#if defined(_PRE_SOLARIS_2_6) && defined(i86pc)
#define	DMA_MEMORY_SCARCE	1
#define	IPRB_DECAY_FACTOR	20		/* 2.5.1 Slow Decay */

typedef	unsigned short		uint16_t;
typedef signed long		int32_t;
typedef signed short		int16_t;
typedef signed char		int8_t;

#define	ddi_io_get8		ddi_io_getb
#define	ddi_io_get16		ddi_io_getw
#define	ddi_io_get32		ddi_io_getl

#define	ddi_io_put8		ddi_io_putb
#define	ddi_io_put16		ddi_io_putw
#define	ddi_io_put32		ddi_io_putl

#define	pci_config_put32	pci_config_putl
#define	pci_config_put16	pci_config_putw
#define	pci_config_put8		pci_config_putb
#define	pci_config_getl		pci_config_getl
#define	pci_config_get16	pci_config_getw
#define	pci_config_get8		pci_config_getb

#endif	/* (_PRE_SOLARIS_2_6) && defined(i86pc) */

#ifdef IA64
#define	ddi_io_put8(a, b, c)	ddi_put8((a), (uint8_t *)(b), (uint8_t)(c))
#define	ddi_io_put16(a, b, c)	ddi_put16((a), (uint16_t *)(b), (uint16_t)(c))
#define	ddi_io_put32(a, b, c)	ddi_put32((a), (uint32_t *)(b), (uint32_t)(c))

#define	ddi_io_get8(a, b)	ddi_get8((a), (uint8_t *)(b))
#define	ddi_io_get16(a, b)	ddi_get16((a), (uint16_t *)(b))
#define	ddi_io_get32(a, b)	ddi_get32((a), (uint32_t *)(b))
#endif	/* IA64 */

/* A D100 individual address setup command */
struct iprb_ias_cmd {
	uint16_t ias_bits;
	uint16_t ias_cmd;
	uint32_t ias_next;
	uint8_t  addr[ETHERADDRL];
};

/* A D100 Self Test command */
struct iprb_self_test_cmd {
	uint32_t st_sign;
	uint32_t st_result;
};

/* A D100 Diagnose command */
struct iprb_diag_cmd {
	uint16_t diag_bits;
	uint16_t diag_cmd;
	uint32_t diag_next;
};

/* A D100 Load microcode command */
struct iprb_ldmc_cmd {
	uint16_t ldmc_bits;
	uint16_t ldmc_cmd;
	uint32_t ldmc_next;
	ulong_t	microcode[D101M_MICROCODE_LENGTH];
};

/* A D100 configure command */
struct iprb_cfg_cmd {
	int16_t cfg_bits;
	int16_t cfg_cmd;
	uint32_t cfg_next;
	uint8_t cfg_byte0;
	uint8_t cfg_byte1;
	uint8_t cfg_byte2;
	uint8_t cfg_byte3;
	uint8_t cfg_byte4;
	uint8_t cfg_byte5;
	uint8_t cfg_byte6;
	uint8_t cfg_byte7;
	uint8_t cfg_byte8;
	uint8_t cfg_byte9;
	uint8_t cfg_byte10;
	uint8_t cfg_byte11;
	uint8_t cfg_byte12;
	uint8_t cfg_byte13;
	uint8_t cfg_byte14;
	uint8_t cfg_byte15;
	uint8_t cfg_byte16;
	uint8_t cfg_byte17;
	uint8_t cfg_byte18;
	uint8_t cfg_byte19;
	uint8_t cfg_byte20;
	uint8_t cfg_byte21;
	uint8_t cfg_byte22;
	uint8_t cfg_byte23;
};

struct iprb_mcs_addr {
	uint8_t addr[ETHERADDRL];
};

/* Configuration bytes (from Intel) */

#define	IPRB_CFG_B0		0x16
					/* Byte 0 (byte count) default 	*/
					/* 16h is 22 bytes (max.)	*/

#define	IPRB_CFG_B1		0x88
					/* Byte 1 (fifo limits) default */
					/* bit 7 is always 1. Tx Fifo 	*/
					/* 6-4 bits recommended all 0 &	*/
					/* 3-0 bits are for Rx Fifo and	*/
					/* 82557 recommended value 1000	*/
					/* 82558 recommended value 1000	*/
					/* So it will be 10001000 = 88h	*/
#define	IPRB_CFG_B2		0
					/* Adaptive IFS default		*/
#define	IPRB_CFG_B3		0
					/* Default 			*/
					/* bit 0 MWI, bit 1 Type enable */
					/* bit 2 Read Align, bit 3 Ter-	*/
					/* minate Write on cache line.	*/
#define	IPRB_CFG_B4		0
					/* Default */
					/* bit 6-0 Rx DMA Max. Count	*/
#define	IPRB_CFG_B5		0
					/* Default */
					/* bit 6-0 Tx DMA Max. Count	*/
					/* bit 7 enables DMA Max. Byte 	*/
					/* Count. 			*/
#define	IPRB_CFG_B6		0x3a
					/* Initially was 0x3a		*/
					/* bit 0 Late SCB, bit 1 Direct */
					/* DMA=Direct RCV DMA Mode	*/
					/* bit 2 TNO Int (82557 only)	*/
					/*	 TCO Stat (82559 only)	*/
					/* bit 3 CI Int = CU Idle Intr.	*/
					/* 0 CNA Intr. 1 CI Intr.	*/
					/* bit 4 Extended TxCB		*/
					/* bit 5 Extended Stat. Count	*/
					/* bit 6 Disc. Overrun Rx Frame */
					/* bit 7 Save Bad Frames	*/
					/* It is 00111010 = 3ah (No CNA	*/
					/* Interrupts)			*/
					/* 00110010 = 32h (generate CNA */
					/* Interrupts)			*/
#define	IPRB_CFG_B6PROM		0x80
					/* It is 10000000 = 80h		*/
#define	IPRB_CFG_B7PROM		0x2
					/* bit 0 Disc. Short Rx Frames	*/
					/* bit 2-1 Underrun Retry	*/
					/* bit 6 Two Frames in Fifo	*/
					/* bit 7 Dynamic TBD		*/
					/* It is 00000010 = 2h		*/
#define	IPRB_CFG_B7NOPROM	0x3
					/* It is 00000011 = 3h		*/
#define	IPRB_CFG_B8_MII		1
					/* bit 0 503/MII		*/
					/* bit 7 CSMA Disable		*/
#define	IPRB_CFG_B8_503		0
#define	IPRB_CFG_B9		0
					/* bit 0 TCP/UDP Checksum	*/
					/* bit 4 VLAN ARP (82558 B Stp) */
					/*   or  VLAN TCO (82559)	*/
					/* bit 5 Link Status change Wak */
					/* En. Only for 82558 B Step &  */
					/* 82559			*/
					/* bit 6 ARP Wake En Only for   */
					/* 82558 B Step 		*/
					/* bit 7 MCMatch Wake En Only 	*/
					/* for 82558 B 			*/
#define	IPRB_CFG_B10		0x2e
					/* bit 3 No Source Address Inse */
					/* rtion.			*/
					/* bit 5-4 Preamble Length	*/
					/* bit 7-6 loopback		*/
#define	IPRB_CFG_B11		0
					/* bit 2-0 Linear Priority	*/
#define	IPRB_CFG_B12		0x60
					/* bit 0 Linear Priority Mode	*/
					/* bit 7-4 inner frame spacing	*/
#define	IPRB_CFG_B13		0
					/* Byte 13 & Byte 14 IP Address	*/
#define	IPRB_CFG_B14		0xf2
					/* It is 11110010 = f2h		*/
#define	IPRB_CFG_B15		0xc8
					/* bit 0 Promiscuous mode	*/
					/* bit 1 Broadcast disable	*/
					/* bit 2 Wait after Win for 	*/
					/* 82558 and 82559 only		*/
					/* bit 4 Ignore U/L		*/
					/* bit 5 CRC 16 or 32 bit 0=32	*/
					/* bit 7 CRS or CDT		*/
					/* It is 11001000 = c8h		*/
#define	IPRB_CFG_B15_PROM	1
#define	IPRB_CFG_B16		0
					/* bit 0-7 FC Delay LSByte	*/
#define	IPRB_CFG_B17		0x40
					/* bit 0-7 FC Delay MSByte	*/
#define	IPRB_CFG_B18		0xf2
					/* bit 0 Stripping Enabled	*/
					/* bit 1 Padding Enabled	*/
					/* bit 2 RCV CRC transfer	*/
					/* bit 3 Long RX OK		*/
					/* bit 4-6 Priority FC Threshld */
#define	IPRB_CFG_B19		0x80
					/* bit 0 Address Wak up (82558A */
					/* step) IA Match Wak En (82558 */
					/* B Step)			*/
					/* bit 1 Magic Packet Wake up	*/
					/* disable			*/
					/* bit 2 Full duplex Tx Flow 	*/
					/* control disable		*/
					/* bit 3 Full duplx Restop Flow */
					/* control			*/
					/* bit 4 Full duplex Restart 	*/
					/* flow control			*/
					/* bit 5 Address filtering of 	*/
					/* Full duplex Tx Flow Control	*/
					/* frames			*/
					/* bit 6 Force full duplex	*/
					/* bit 7 Full duplex pin enable */
#define	IPRB_CFG_B20		0x3f
					/* bit 5 Priority field in FC 	*/
					/* Frame			*/
					/* bit 6 Multi IA		*/
#define	IPRB_CFG_B21		0x5
					/* bit 3 Multicast all		*/
#define	IPRB_CFG_B21_MCPROM	0x8
#define	IPRB_CFG_B22		0
#define	IPRB_CFG_B23		0

/* Configuration Bit settings... */
#define	IPRB_CFG_MWI_ENABLE	0x0001	/* MWI Enabled			*/
#define	IPRB_CFG_BROADCAST_DISABLE  0x0002	/* Disables NIC to rcv  */
						/* broadcast pkts  	*/
#define	IPRB_CFG_WAW_ENABLE	0x0004	/* Enables Collision backoff 	*/
#define	IPRB_CFG_READAL_ENABLE	0x0004	/* Read Align Enabled		*/
#define	IPRB_CFG_URUN_RETRY	0x0006	/* Underrun Retries		*/
#define	IPRB_CFG_TERMWCL_ENABLE	0x0008	/* Terminate Write on Cacheline */
#define	IPRB_CFG_EXTXCB_DISABLE	0x0010	/* Disable Extended TxCB	*/
#define	IPRB_CFG_DYNTBD_ENABLE	0x0080	/* Dynamic TBD Enabled		*/
#define	IPRB_CFG_CRS_OR_CDT	0x0080	/* CRS or CDT bit set		*/
#define	IPRB_CFG_FC_DELAY_LSB	0x1f	/* Least Significant Byte of	*/
					/* flow control delay field.	*/
#define	IPRB_CFG_FC_DELAY_MSB	0x01	/* Most Significant byte of the */
					/* flow control delay field.	*/
#define	IPRB_CFG_TX_FC_DISABLE	0x0004	/* Tx FLow Control disabled	*/
#define	IPRB_CFG_FC_RESTOP	0x0008	/* Rx Flow Control Restop	*/
#define	IPRB_CFG_FC_RESTART	0x0010	/* Rx Flow Control Restart	*/
#define	IPRB_CFG_REJECT_FC	0x0020	/* Address filtering of full	*/
					/* duples Tx Flow Control frms	*/
#define	IPRB_CFG_FORCE_FDX	0x0040	/* Force Full Duplex */

#define	IPRB_MAXMCSN		64	/* Max number of multicast addrs */
#define	IPRB_MAXMCS		(IPRB_MAXMCSN*ETHERADDRL)
#define	IPRB_STASH_SIZE 	256

#define	IPRB_REAP_COMPLETE_CMDS 0
#define	IPRB_REAP_ALL_CMDS	1

#define	IPRB_DELAY_TIME		100000	/* No. of usecs to delay in	*/
					/* iprb stop_board_between reaps */
#define	IPRB_RATIO_UNDERRUNS 	100
#define	IPRB_DEFAULT_THRESHOLD 	5
#define	IPRB_MAX_THRESHOLD 	10
/* It is used in Phy errata implementation */
#define	IPRB_PHY_SPECIAL_CNTRL	0x11	/* 82555 special cntrl register	*/
#define	MDI_EQUALIZER_CSR	0x1a	/* 82555 specific equalizer rg	*/
#define	EXTENDED_SQUELCH_BIT	0x0004
#define	MDI_CR_AUTO_SELECT	0x1000
#define	MDI_CR_RESTART_AUTO_NEG	0x0200
#define	MDI_CONTROL_REG		0x00
#define	IPRB_DISABLE_AUTO_POLARITY	0x0010	/* Disabl Auto Polarity */
#define	IPRB_PHY_EOF_COUNTER	0x18	/* To check end of frame errors */
#define	IPRB_PHY_CSR		0x10	/* 82555 CSR 			*/
#define	IPRB_PHY_POLARITY_BIT	0x100	/* Phy polarity bit 		*/
#define	MDI_CR_10_100		0x2000	/* Bit 13 			*/
#define	MDI_CR_FULL_HALF	0x0100	/* Bit 8 			*/

/* A D100 multicast setup command */
struct iprb_mcs_cmd {
	int16_t mcs_bits;
	int16_t mcs_cmd;
	uint32_t mcs_next;
	int16_t mcs_count;
	uint8_t mcs_bytes[IPRB_MAXMCS];
};

struct iprb_ext_tcb {
	volatile uint32_t tbd0_buf_addr;	/* First Physical Tx 	*/
						/* Buffer Address 	*/
	volatile uint32_t tbd0_buf_cnt;		/* First Buffer's	*/
						/* Actual Byte Count 	*/
	volatile uint32_t tbd1_buf_addr;	/* Second Physical Tx	*/
						/* Buffer Address 	*/
	volatile uint32_t tbd1_buf_cnt;		/* Second Buffer's 	*/
						/* Actual Byte Count 	*/
	uint8_t data_stash[IPRB_STASH_SIZE];	/* Place to hold data 	*/
};

struct iprb_gen_tcb {
	uint32_t pad0;
	uint32_t pad1;
	uint32_t pad2;
	uint32_t pad3;
	uint8_t data_stash[IPRB_STASH_SIZE];
};

union iprb_xmit_cmd_data {
	struct iprb_gen_tcb gen_tcb;
	struct iprb_ext_tcb ext_tcb;
 };

/* A D100 transmit command */
struct iprb_xmit_cmd {
	int16_t xmit_bits;
	int16_t xmit_cmd;
	uint32_t xmit_next;
	uint32_t xmit_tbd;
	int16_t xmit_count;
	uint8_t xmit_threshold;
	uint8_t xmit_tbdnum;
	/* 82558 enhancement */
	union iprb_xmit_cmd_data data;
};

struct iprb_gen_cmd {
	int16_t gen_status;
	int16_t gen_cmd;
	uint32_t gen_next;
};

/* A generic control unit (CU) command */
union iprb_generic_cmd {
	struct iprb_ias_cmd ias_cmd;
	struct iprb_cfg_cmd cfg_cmd;
	struct iprb_mcs_cmd mcs_cmd;
	struct iprb_xmit_cmd xmit_cmd;
	struct iprb_gen_cmd gen_cmd;
	struct iprb_ldmc_cmd ldmc_cmd;
	struct iprb_diag_cmd diag_cmd;
};

/* A D100 receive frame descriptor */
struct iprb_rfd {
	uint16_t rfd_status;
	uint16_t rfd_control;
	uint32_t rfd_next;		/* DMAable address of next frame */
	uint32_t rfd_rbd;		/* DMAable address of buffer desc */
	int16_t rfd_count;		/* Actual number of bytes received */
	int16_t rfd_size;		/* # of bytes available in buffer */
	uint8_t data[ETHERMAX];		/* Data Buffer <Simplified Model> */
};

/* Statistics structure returned by D100 SCB */
struct iprb_stats {
	uint32_t iprb_stat_xmits;
	uint32_t iprb_stat_maxcol;
	uint32_t iprb_stat_latecol;
	uint32_t iprb_stat_xunderrun;
	uint32_t iprb_stat_crs;
	uint32_t iprb_stat_defer;
	uint32_t iprb_stat_onecoll;
	uint32_t iprb_stat_multicoll;
	uint32_t iprb_stat_totcoll;
	uint32_t iprb_stat_rcvs;
	uint32_t iprb_stat_crc;
	uint32_t iprb_stat_align;
	uint32_t iprb_stat_resource;
	uint32_t iprb_stat_roverrun;
	uint32_t iprb_stat_cdt;
	uint32_t iprb_stat_short;
	uint32_t iprb_stat_chkword;
};

/*
 * All the information pertaining to each Rx buffer.
 */
struct iprb_rfd_info {
	frtn_t			free_rtn;	/* desballoc() structure */
	struct iprbinstance	*iprbp;		/* My Instance */
	struct iprb_rfd_info	*next;		/* Linking into "Free List" */

	/* RFD : Rx Desc/Buffer Descriptor */
	ddi_dma_handle_t	rfd_dma_handle;	/* DMA handle of RFD */
	ddi_acc_handle_t	rfd_acc_handle;	/* Access handle of RFD */
	uint32_t		rfd_physaddr;	/* Physical Address of RFD */
	caddr_t			rfd_virtaddr;	/* Virtual Address of RFD */
};

/*
 * Store all required information for a Command (Tx) Control block.
 */
struct iprb_cmd_info {
	ddi_dma_handle_t	cmd_dma_handle;
	ddi_acc_handle_t	cmd_acc_handle;
	uint32_t		cmd_physaddr;
	caddr_t		cmd_virtaddr;
};

/* Packet data fragment descriptor. */
typedef struct {
	volatile uint32_t	data;
	volatile uint32_t	frag_len;
} frag_desc_t;

/* Tx Buffer Descriptor.  */
struct iprb_Txbuf_desc {
	frag_desc_t frag[IPRB_MAX_FRAGS];
};

/* Store all the required information for Tx Buffer Descriptor.  */
struct iprb_Txbuf_info {
	mblk_t			*mp;	/* mblk associated with Tx packet */
	ddi_dma_handle_t	tbd_dma_handle;
	ddi_acc_handle_t	tbd_acc_handle;
	uint32_t		tbd_physaddr;
	caddr_t			tbd_virtaddr;
	ddi_dma_handle_t	frag_dma_handle[IPRB_MAX_FRAGS];
	uint8_t			frag_nhandles;
};


/*
 * driver specific declarations.
 */
struct iprbinstance {
	dev_info_t 	*iprb_dip;		/* Device Information Pointer */
	ddi_acc_handle_t 	iohandle;	/* IO mapping handle */
	uint32_t 	port;			/* Port Address of card */
	char 		iprb_phyaddr;		/* Address of external PHY */
	char 		iprb_phy0exists;	/* PHY 0 is present */
	int  		speed;			/* Speed 10/100 */
	int 		full_duplex;		/* Duplex of link */
	int 		link_status;		/* Maintains Link Status */
	ushort_t 	phy_state;		/* Phy State 0,1,2 */
	ushort_t 	phy_delay;		/* Delay (3) */
	ushort_t	phy_id;			/* Phy id, chk Intel's Phy */
	uint16_t 	iprb_nrecvs;		/* Number of Rx desc in ring */
	uint16_t 	iprb_nxmits;		/* No. of Command (Tx) blocks */

	/* eeprom size */
	ushort_t 	iprb_eeprom_size;

	/* Revision Id */
	char 		iprb_revision_id;

	/* Sub vendor Id */
	ushort_t 	iprb_sub_vendor_id;
	ushort_t 	iprb_sub_system_id;

	/* Command (Tx) Control Blocks */
	union iprb_generic_cmd	*cmd_blk[IPRB_MAX_XMITS];
	struct iprb_cmd_info 	cmd_blk_info[IPRB_MAX_XMITS];

	/* Tx Buffer Descriptors */
	struct iprb_Txbuf_desc	*Txbuf_desc[IPRB_MAX_XMITS];
	struct iprb_Txbuf_info	Txbuf_info[IPRB_MAX_XMITS];

	/* Receive portion */
	uint16_t 	min_rxbuf;
	uint16_t 	max_rxbuf;
	uint16_t 	max_rxbcopy;

	/* Rx Frame Descriptors */
	struct iprb_rfd		*rfd[IPRB_MAX_RECVS];
	struct iprb_rfd_info 	*rfd_info[IPRB_MAX_RECVS];

	/* Free List */
	struct iprb_rfd_info 	*free_list;
	uint16_t 	free_buf_cnt;
	uint16_t 	rfd_cnt;
	uint16_t 	rfds_outstanding;

	kmutex_t 	freelist_mutex;		/* protection for 'Free List' */
	kmutex_t 	cmdlock;		/* TX path lock */
						/* also protects command que */
	kmutex_t 	intrlock;

	unsigned char 	macaddr[ETHERADDRL];	/* Factory mac address */
	int16_t 	iprb_first_rfd;		/* First RFD not processed */
	int16_t 	iprb_last_rfd;		/* Last RFD available to IPRB */
	int16_t 	iprb_current_rfd;	/* Next RFD to be filled */
	int16_t 	iprb_first_cmd;		/* First cmd waiting to go */
	int16_t 	iprb_last_cmd;		/* Last cmd waiting to go */
	int16_t 	iprb_current_cmd;	/* Next command avlbl for use */
	int16_t 	iprb_first_xbd;		/* First tbd waiting for DMA */
	int16_t 	iprb_last_xbd;		/* Last tbd waiting for DMA */
	struct iprb_mcs_addr 	iprb_mcs_addrs[IPRB_MAXMCSN]; /* List mc addr */
	uint8_t 	iprb_mcs_addrval[IPRB_MAXMCSN];	/* List slots used */
	ushort_t 	iprb_receive_enabled;	/* Is receiving allowed? */
	ushort_t 	iprb_initial_cmd; 	/* Is this the first command? */
	uint32_t 	RUwdog_lbolt;		/* Watch Dog Timer Val */
	int 		RUwdogID;		/* Watch Dog Process ID */
	ushort_t	iprb_extended_txcb_enabled; /* To check whether */
						/* extended TxCBs are enbld */
	ushort_t 	iprb_enhanced_tx_enable; /* To check whether Dynamic */
						/* TBDs are enabled */

	uint32_t 	RU_stat_count;	/* (Per Inst.) W dog reset counter */
	uint32_t 	iprb_stat_intr;	/* no. interrupts rec'd */
	uint32_t 	iprb_stat_norcvbuf; 	/* failed recvs no resources */
	uint32_t 	iprb_stat_frame_toolong; /* tx with frame too long */

	int 		iprb_threshold;
	int 		RU_needed;
	int 		promisc;		/* Promiscuous mode enabled */
	mii_handle_t 	mii;
	int		phy_errata_freq;	/* This is to detect how */
						/* often we need to call */
						/* for phy errata */
	boolean_t	speed_duplex_forced;
	boolean_t	do_self_test;
	boolean_t	do_diag_test;
	int		Aifs;
	int		read_align;
	int		extended_txcb;
	ushort_t	tx_ur_retry;
	boolean_t	enhanced_tx_enable;
	int		ifs;
	boolean_t	disable_broadcast;
	boolean_t	coll_backoff;
	ushort_t	flow_control;
	ushort_t	curr_cna_backoff;
	uint32_t	cpu_cycle_saver_dword_val;
	uint32_t	cpu_saver_bundle_max_dword_val;
	ushort_t	auto_polarity;
	ushort_t	implement_errata;
	ushort_t	implement_errata_counter;
};

#define	RUTIMEOUT		200 /* 2 Seconds */
#define	RUWDOGTICKS		200
#define	IPRB_TYPE_MII		0
#define	IPRB_TYPE_503		1
#define	IPRB_FALSE		0
#define	IPRB_TRUE		1
#define	IPRB_LINK_UP		1
#define	IPRB_LINK_DOWN		0
#define	IPRB_INTEL_82555	0x15

#define	IPRB_SCB_STATUS		0
#define	IPRB_SCB_CMD		2
#define	IPRB_SCB_CMD_HI		3	/* This is the later byte of */
					/* IPRB_SCB_CMD, bit 24-31 */
#define	IPRB_SCB_PTR		4
#define	IPRB_SCB_PORT		8
#define	IPRB_SCB_FLSHCTL	0xc
#define	IPRB_SCB_EECTL		0xe
#define	IPRB_SCB_MDICTL		0x10
#define	IPRB_SCB_ERCVCTL	0x14
#define	IPRB_SCB_FC_THLD_REG	0x19
#define	IPRB_SCB_FC_CMD_REG	0x1a
#define	IPRB_SCB_GENCTL		0x1c

#define	IPRB_MDI_READFRAME(phy, reg) (((reg)<<16) | (phy<<21) | (2<<26))
#define	IPRB_MDI_WRITEFRAME(phy, reg, data) \
				((data) | ((reg)<<16) | (phy<<21) | (1<<26))
#define	IPRB_MDI_READ		2
#define	IPRB_MDI_READY		0x10000000
#define	IPRB_MDI_CREG		0
#define	IPRB_MDI_SREG		1

#define	IPRB_LOAD_RUBASE	6
#define	IPRB_CU_START		0x10
#define	IPRB_CU_RESUME		0x20
#define	IPRB_CU_LOAD_DUMP_ADDR	0x40
#define	IPRB_CU_DUMPSTAT	0x50
#define	IPRB_LOAD_CUBASE	0x60
#define	IPRB_RU_START		1
#define	IPRB_RU_ABORT		4
#define	IPRB_DISABLE_INTR	0x0001
#define	IPRB_ENABLE_INTR	0x0000

#define	IPRB_GEN_SWI		(1 << 9)	/* Generate software intr */

#define	IPRB_PORT_SW_RESET	0
#define	IPRB_PORT_SELF_TEST	1
#define	IPRB_PORT_SEL_RESET	2
#define	IPRB_PORT_DUMP		3

#define	IPRB_STAT_COMPLETE	0xa005
#define	IPRB_PCI_VENID		0x8086
#define	IPRB_PCI_DEVID		0x1229

#define	IPRB_RFD_COMPLETE	0x8000
#define	IPRB_RFD_OK		0x2000
#define	IPRB_RFD_CRC_ERR	(1 << 11)
#define	IPRB_RFD_ALIGN_ERR	(1 << 10)
#define	IPRB_RFD_NO_BUF_ERR	(1 << 9)
#define	IPRB_RFD_DMA_OVERRUN	(1 << 8)
#define	IPRB_RFD_SHORT_ERR	(1 << 7)
#define	IPRB_RFD_TYPE		(1 << 5)
#define	IPRB_RFD_PHY_ERR	(1 << 4)
#define	IPRB_RFD_IA_MATCH	(1 << 1)
#define	IPRB_RFD_COLLISION	(1 << 0)
#define	IPRB_RFD_SF		0x0008		/* within rfd_control */
#define	IPRB_RFD_H		0x0010		/* within rfd_control */
#define	IPRB_RFD_SUSPEND	0x4000		/* within rfd_control */
#define	IPRB_RFD_EL		0x8000
#define	IPRB_RFD_OFFSET	((sizeof (uint16_t) * 4) + (sizeof (uint32_t) * 2))
#define	IPRB_RFD_EOF		0x8000
#define	IPRB_EL			0x80000000
#define	IPRB_RFD_CNTMSK		0x3FFF

#define	IPRB_RBD_EL		0x8000		/* with rbd_size */
#define	IPRB_RBD_EOF		0x8000		/* with rbd_count */
#define	IPRB_RBD_F		0x4000		/* with rbd_count */
#define	IPRB_DISABLE_PWR_CLKRUN	0x0000		/* Disables Ena Deep Power */
						/* Down on Link Down and */
						/* CLKRUN Dis */
						/* Table 6-11 version 0.30 */

/* Helpful defines for register access */
#define	REG8(reg, offset)	(uint8_t *)((reg) + offset)
#define	REG16(reg, offset)	(uint16_t *)((reg) + offset)
#define	REG32(reg, offset)	(uint32_t *)((reg) + offset)

/*
 * According to Intel, this could be optimized by keeping track of the
 * number of transmit overruns, and adjusting on the fly.  Apart from
 * doing that (which would mean timeouts in the driver), 0x64 is a good
 * value to use.  Intel was clear not to use the default from their header
 * file.
 */

#define	IPRB_NOP_CMD		0
#define	IPRB_IAS_CMD		1
#define	IPRB_CFG_CMD		2
#define	IPRB_MCS_CMD		3
#define	IPRB_XMIT_CMD		4
#define	IPRB_LDMC_CMD		5	/* Changed from IPRB_RCV_CMD to  */
					/* IPRB_LDMC_CMD, refer to 6.4.1 */
					/* of Software Tech Ref. Manual  */
#define	IPRB_DUMP_CMD		6
#define	IPRB_DIAG_CMD		7

/* Command Unit command word bits */
#define	IPRB_SF		0x0008		/* Simplified = 0 Flexible = 1 */
#define	IPRB_INTR	0x2000		/* Interrupt upon completion */
#define	IPRB_SUSPEND	0x4000		/* Suspend upon completion */
#define	IPRB_EOL	0x8000		/* End Of List */

/* Command unit status word bits */
#define	IPRB_CMD_OK		0x2000
#define	IPRB_CMD_UNDERRUN	0x1000
#define	IPRB_CMD_COMPLETE	0x8000

#define	IPRB_SCB_INTR_MASK	0xff00
#define	IPRB_INTR_CXTNO		0x8000
#define	IPRB_INTR_FR		0x4000
#define	IPRB_INTR_CNACI		0x2000
#define	IPRB_INTR_RNR		0x1000
#define	IPRB_INTR_MDI		0x800
#define	IPRB_INTR_SWI		0x400

#define	IPRB_EEDI			0x04
#define	IPRB_EEDO		0x08
#define	IPRB_EECS		0x02
#define	IPRB_EESK		0x01
#define	IPRB_EEPROM_READ		0x06
#define	IPRB_EEPROM_COMP_WORD	0x03
#define	IPRB_EL_BIT		0x8000

#define	IPRB_CMD_MASK 		0xff	/* Cmd portion of cmd register */

/* Default values for iprb.conf file parameters */
#define	IPRB_DEFAULT_MWI_ENABLE		1
	/* PCI 450NX chipset needs this value to be 0 	*/
#define	IPRB_DEFAULT_READAL_ENABLE		0
	/* Only for highly cache line oriented systems	*/
#define	IPRB_DEFAULT_EXTXCB_ENABLE		1
	/* Enable Extended TxCB if set to 0 on 82558/9	*/
#define	IPRB_DEFAULT_TXURRETRY		3
	/* Number of transmission retries after underrun */
#define	IPRB_DEFAULT_ENHANCED_TX		0
	/* Dynamic TBD 					*/
#define	IPRB_DEFAULT_FLOW_CONTROL		0
	/* Flow Control					*/
#define	IPRB_DEFAULT_BROADCAST_DISABLE	0
	/* 0 Enables broadcast, 1 disables it		*/
#define	IPRB_DEFAULT_WAW			0
	/* Enables Wait after Win Collision backOff */
#define	IPRB_DEFAULT_AUTO_POLARITY		2
	/* Polarity helps in fixing polarity bug */
#define	IPRB_DEFAULT_CNA_BACKOFF		16
	/* CNA Backoff value, larger value may give better perfor. */
#define	IPRB_DEFAULT_PHYERRATA_FREQUENCY	10
	/* For Cable hole errata fix... */
#define	IPRB_DEFAULT_CPU_CYCLE_SAVER		0
	/* Microcode CPU cycle saver */
#define	IPRB_DEFAULT_CPU_SAVER_BUNDLE_MAX	6
	/* Microcode CPU cycle saver for rcv bundling */
#define	IPRB_DEFAULT_DIAGNOSTICS_TEST		1
	/* Run diagnostic tests by default */
#define	IPRB_DEFAULT_SELF_TEST			1
	/* Run self test by default */
#define	IPRB_DEFAULT_FORCE_SPEED_DUPLEX		5
	/* Force spped and duplex 5 is auto negotiation. */
#define	IPRB_DEFAULT_FC_THLD			0x00
	/* Rx FIFO threshold of 0.5KB free  */
#define	IPRB_DEFAULT_FC_CMD			0x00
	/* FC Command in CSR */
#define	IPRB_10_HALF				1
#define	IPRB_10_FULL				2
#define	IPRB_100_HALF				3
#define	IPRB_100_FULL				4
#define	IPRB_AUTO_NEGOTIATION			5
#ifdef IA64
#define	IPRB_SCBWAIT(iprbp) { 						\
	register int ntries = 10000;					\
	do {								\
		if ((ddi_get8(iprbp->iohandle,				\
			iprbp->port + IPRB_SCB_CMD) & IPRB_CMD_MASK) == 0) \
			break;						\
		drv_usecwait(10);					\
	} while (--ntries > 0);						\
	if (ntries == 0)						\
		cmn_err(CE_WARN, "iprb: device never responded!\n");	\
}
#else
#define	IPRB_SCBWAIT(iprbp) { 						\
	register int ntries = 10000;					\
	do {								\
		if ((ddi_io_get8(iprbp->iohandle,			\
		    REG8(iprbp->port, IPRB_SCB_CMD)) & IPRB_CMD_MASK) == 0)\
			break;						\
		drv_usecwait(10);					\
	} while (--ntries > 0);						\
	if (ntries == 0)						\
		cmn_err(CE_WARN, "iprb: device never responded!\n");	\
}
#endif	/* IA64 */

#define	IPRB_EEPROM_ADDRESS_SIZE(size)  size == 64  ? 6 : 		\
					(size == 128 ? 7 :		\
					(size == 256 ? 8 : 6))

#define	IS_503(phy)	((phy) == -1)
#define	PHY_MODEL(x)	(((x) >> 4) & 0x3f)	/* 6 bits 4-9 */
#define	DelayInMilliseconds(N)	drv_usecwait(N*1000)
/*
 * The following will disable or enable interrupts based on M bit setting
 * in upper SCB command word D31-D16 section 6.3.2.2 of Intel spec.
 * VA edited on May 20, 1999
 */
#ifdef IA64
#define	DisableInterrupts(iprbp)	ddi_put8(iprbp->iohandle,      \
		iprbp->port + IPRB_SCB_CMD_HI, IPRB_DISABLE_INTR);
#define	EnableInterrupts(iprbp)		ddi_put8(iprbp->iohandle,      \
		iprbp->port + IPRB_SCB_CMD_HI, IPRB_DISABLE_INTR);
#else
#define	DisableInterrupts(iprbp)	ddi_io_put8(iprbp->iohandle,   \
		REG8(iprbp->port, IPRB_SCB_CMD_HI), IPRB_DISABLE_INTR);
#define	EnableInterrupts(iprbp)		ddi_io_put8(iprbp->iohandle,   \
		REG8(iprbp->port, IPRB_SCB_CMD_HI), IPRB_ENABLE_INTR);
#endif	/* IA64 */

#ifdef __cplusplus
}
#endif

#endif	/* _IPRB_H */
