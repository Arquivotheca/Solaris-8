/*
 * Copyright (c) 1993, 1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_ELX_H
#define	_SYS_ELX_H

#pragma ident	"@(#)elx.h	1.22	99/08/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Hardware specific driver declarations for the 3COM EtherLink III
 * driver conforming to the Generic LAN Driver model.
 */

/* debug flags */
#define	ELXTRACE	0x01
#define	ELXERRS		0x02
#define	ELXRECV		0x04
#define	ELXDDI		0x08
#define	ELXSEND		0x10
#define	ELXINT		0x20
#define	ELXIO		0x40
#define	ELXDDIPROBE	0x80
#define	ELXGSTAT	0x100

/* xpci functions */
#ifdef	_INB_OUTB_
#define	DDI_INB(port)		inb(port)
#define	DDI_INW(port)		inw(port)
#define	DDI_INL(port)		inl(port)
#define	DDI_OUTB(port, val)	outb(port, val)
#define	DDI_OUTW(port, val)	outw(port, val)
#define	DDI_OUTL(port, val)	outl(port, val)
#else
#define	DDI_INB(port)		ddi_io_getb(handle, (uint8_t *)(port))
#define	DDI_INW(port)		ddi_io_getw(handle, (uint16_t *)(port))
#define	DDI_INL(port)		ddi_io_getl(handle, (uint32_t *)(port))
#define	DDI_OUTB(port, val)	ddi_io_putb(handle, (uint8_t *)(port), val)
#define	DDI_OUTW(port, val)	ddi_io_putw(handle, (uint16_t *)(port), val)
#define	DDI_OUTL(port, val)	ddi_io_putl(handle, (uint32_t *)(port), val)
#endif

#if defined PCI_DDI_EMULATION

/* for Solaris 2.4 xpci version 3.0 */
#define	DDI_REPINSB(port, buf, cnt)	repinsb(port, buf, cnt)
#define	DDI_REPINSD(port, buf, cnt)	repinsd(port, buf, cnt)
#define	DDI_REPOUTSB(port, buf, cnt)	repoutsb(port, buf, cnt)
#define	DDI_REPOUTSD(port, buf, cnt)	repoutsd(port, buf, cnt)

#else	/* PCI_DDI_EMULATION */

#ifdef	_INB_OUTB_
#define	DDI_REPINSB(port, buf, cnt)	repinsb(port, buf, cnt)
#define	DDI_REPINSD(port, buf, cnt)	repinsd(port, buf, cnt)
#define	DDI_REPOUTSB(port, buf, cnt)	repoutsb(port, buf, cnt)
#define	DDI_REPOUTSD(port, buf, cnt)	repoutsd(port, buf, cnt)
#else
#define	DDI_REPINSB(port, buf, cnt)	ddi_io_rep_getb(handle, buf, \
						(uint8_t *)(port), cnt)
#define	DDI_REPINSD(port, buf, cnt)	ddi_io_rep_getl(handle, buf, \
						(uint32_t *)(port), cnt)
#define	DDI_REPOUTSB(port, buf, cnt)	ddi_io_rep_putb(handle, buf, \
						(uint8_t *)(port), cnt)
#define	DDI_REPOUTSD(port, buf, cnt)	ddi_io_rep_putl(handle, buf, \
						(uint32_t *)(port), cnt)
#endif

#endif	/* PCI_DDI_EMULATION */

/* Misc */
#define	ELXHIWAT	32768		/* driver flow control high water */
#define	ELXLOWAT	4096		/* driver flow control low water */
#define	ELXMAXPKT	1500		/* maximum media frame size */
#define	ELXMAXFRAME	1518		/* maximum with header */
#define	ELXIDNUM	0		/* should be a unique id; zero works */

#define	ELX_DMA_THRESH	1518
#define	ELX_CIP_RETRIES	0xffff
#define	MIN_EISA_ADDR	0x1000

#define	ELX_RCVMSG_PIO	1
#define	ELX_RCVMSG_DMA	2
#define	ELX_RCVMSG_FAIL	-1

/* board state */
#define	ELX_IDLE	0
#define	ELX_WAITRCV	1
#define	ELX_XMTBUSY	2
#define	ELX_ERROR	3

/* driver specific declarations */
typedef struct elxinstance {
	int	elx_rxbits;	/* current receiver modes */
	int	elx_mcount;	/* number of multicast references */
	int	elx_irq;	/* real IRQ to avoid problems of reset */
	int	elx_bus;	/* ISA=0, MCA=1, EISA=2, PCI=3 */
	int	elx_earlyrcv;	/* current early receive threshhold */
	mblk_t *elx_rcvbuf;	/* early receive mblk */
	ushort_t elx_rcvlen;	/* space allocated */
	ushort_t elx_softinfo;	/* EEPROM software info word */
	ushort_t elx_softinfo2;	/* EEPROM software info 2 word */
	ushort_t elx_caps;	/* EEPROM capabilities word */
	ushort_t elx_flags;
	ushort_t elx_ver;	/* 0 for 3c5x9, 1 for 3c59x */
	ushort_t elx_media;
	uchar_t	elx_speed;
	uchar_t	elx_latency;	/* interrupt latency */
	uchar_t	elx_pad;
	gld_mac_info_t *elx_mac;	/* GLD descriptor */
	long	pagesize;
	caddr_t	elx_rbuf;	/* receive buffer pointer */
	caddr_t	elx_xbuf;	/* transmit buffer pointer */
	caddr_t	elx_dma_rbuf;	/* rcv buf pointer (aligned virt addr) */
	caddr_t	elx_dma_xbuf;	/* xmt buf pointer (aligned virt addr) */
	caddr_t	elx_phy_rbuf;	/* rcv buf pointer (aligned physical addr) */
	caddr_t	elx_phy_xbuf;	/* xmt buf pointer (aligned physical addr) */
	mblk_t *elx_xmtbuf;	/* dma transmit mblk */
	int	elx_no_carrier;	/* from statistics register */
	int	elx_no_sqe;	/* from statistics register */
	int	elx_bad_ssd;	/* from statistics register */
	int	elx_media_pass;

	ddi_acc_handle_t io_handle;
} elx_t;

#define	NEW_ELX(p)	(p->elx_ver)

#ifdef	ELXDEBUG
typedef struct {
	mblk_t	*mp;
	short	action;
	short	line;
} elxlog_t;

#define	ELXLOG_LENGTH	1000

#define	ELXLOG_ALLOC		0x01
#define	ELXLOG_FREE		0x02
#define	ELXLOG_PIO_IN		0x03
#define	ELXLOG_DMA_IN		0x04
#define	ELXLOG_DMA_IN_DONE	0x05
#define	ELXLOG_BCOPY_TO		0x06
#define	ELXLOG_SEND_UP		0x07
#define	ELXLOG_DMA_SEND		0x18
#define	ELXLOG_DMA_SEND_DONE	0x19
#define	ELXLOG_BCOPY_FROM	0x1a
#define	ELXLOG_PIO_OUT		0x1b

elxlog_t	elxlog[ELXLOG_LENGTH];
unsigned long	elxlogptr;

int		elxlogflag;
#define	ELXLOG_FLAG_BROADCAST	1

#define	ELX_LOG(dmp, event) {						\
			elxlog[elxlogptr].mp = dmp; 			\
			elxlog[elxlogptr].action = event;		\
			elxlog[elxlogptr].line = __LINE__;	\
			elxlogptr = ++elxlogptr % ELXLOG_LENGTH; 	\
		}
#else
#define	ELX_LOG(dmp, event)
#endif

/* Bus type information */

#define	ELX_ISA		0
#define	ELX_EISA	2
#define	ELX_PCI		3
#define	ELX_NOBUS	-1

#define	EL_3COM_ID	0x6D50	/* 3COM's MFG ID */
#define	EL_PRODUCT_ID	0x9050	/* board ID type */
#define	EL_PRODID_MASK  0xf0ff

/* Card type information and ASIC revision */
#define	ELX_3C5X9	1
#define	ELX_3C5X9B	2

#define	PCI_3COM_VENDOR_ID	0x10b7
#define	PCI_3COM_DEVICE_ID	0x595f

/*
 * Register Window definitions
 */

/* Common to multiple windows */

#define	ELX_COMMAND		0xe /* command register - all windows */
#define	ELX_STATUS		0xe /* status/window register - all windows */

/* Window 0 offsets */
				/* read/write */
#define	ELX_EEPROM_DATA 	0xc
#define	ELX_EEPROM_CMD		0xa
#define	ELX_RESOURCE_CFG	0x8
#define	ELX_ADDRESS_CFG		0x6
#define	ELX_CONFIG_CTL		0x4
				/* Read only */
#define	ELX_PRODUCT_ID		0x2
#define	ELX_MFG_ID		0x0

/* Window 1 offsets */

				/* write only */
#define	ELX_TX_PIO		0x0
#define	ELX_TX_PIO_2		0x2

				/* read only */
#define	ELX_RX_PIO		0x0
#define	ELX_RX_PIO_2		0x2
#define	ELX_RX_ERROR		0x4
#define	ELX_RX_STATUS		0x8
#define	ELX_TX_STATUS		0xb
#define	ELX_TIMER		0xa
#define	ELX_FREE_TX_BYTES	0xc

/* Window 2 offsets */
				/* read/write */
#define	ELX_PHYS_ADDR_0		0x0
#define	ELX_PHYS_ADDR_1		0x1
#define	ELX_PHYS_ADDR_2		0x2
#define	ELX_PHYS_ADDR_3		0x3
#define	ELX_PHYS_ADDR_4		0x4
#define	ELX_PHYS_ADDR_5		0x5
#define	ELX_PHYS_ADDR		ELX_PHYS_ADDR_0

/* Window 3 offsets */
				/* read only */
#define	ELX_INTERNAL_CONFIG	0x0
#define	ELX_MAC_CONTROL		0x6
#define	ELX_RESET_OPTIONS	0x8
#define	ELX_FREE_RX_BYTES	0xa

/* Window 4 offsets */
				/* read/write */
#define	ELX_TX_DIAGNOSTIC	0x0
#define	ELX_HOST_DIAGNOSTIC	0x2
#define	ELX_FIFO_DIAGNOSTIC	0x4
#define	ELX_NET_DIAGNOSTIC	0x6
#define	ELX_CONT_STATUS		0x8
#define	ELX_PHYS_MGMT		0x8
#define	ELX_MEDIA_STATUS	0xa
#define	ELX_BAD_SSD		0xc

/* Window 5 offsets */
				/* read only */
#define	ELX_TX_START		0x0
#define	ELX_TX_AVAIL		0x2
#define	ELX_RX_EARLY		0x6
#define	ELX_RX_FILTER		0x8
#define	ELX_INTR_MASK		0xa
#define	ELX_READ_ZERO_MASK	0xc

/* Window 6 offsets */
				/* read/write */
#define	ELX_CARRIER_LOST	0x0
#define	ELX_NO_SQE		0x1
#define	ELX_TX_MULT_COLL	0x2
#define	ELX_TX_ONE_COLL		0x3
#define	ELX_TX_LATE_COLL	0x4
#define	ELX_RX_OVERRUN		0x5
#define	ELX_TX_FRAMES		0x6
#define	ELX_RX_FRAMES		0x7
#define	ELX_TX_DEFER		0x8
#define	ELX_RX_BYTES		0xa
#define	ELX_TX_BYTES		0xc

/* Window 7 offsets */

#define	ELX_MASTER_ADDR		0x0
#define	ELX_MASTER_LEN		0x6
#define	ELX_MASTER_STAT		0xc

#define	ELMS_SEND		0x1000
#define	ELMS_RECV		0x4000
#define	ELMS_MIP		0x8000
#define	ELMS_TARG_DISC		0x0008
#define	ELMS_TARG_RETRY		0x0004
#define	ELMS_TARG_ABORT		0x0002
#define	ELMS_MAST_ABORT		0x0001

#define	ELX_DMA_READ		0x0
#define	ELX_DMA_WRITE		0x1

/* Command Register Definitions */

#define	COMMAND(op, arg) ((op<<11)|(arg))

/* OP Codes */
#define	ELC_GLOBAL_RESET	0x00
#define	ELC_SELECT_WINDOW	0x01
#define	ELC_START_COAX		0x02
#define	ELC_RX_DISABLE		0x03
#define	ELC_RX_ENABLE		0x04
#define	ELC_RX_RESET		0x05
#define	ELC_RX_DISCARD_TOP	0x08
#define	ELC_TX_ENABLE		0x09
#define	ELC_TX_DISABLE		0x0a
#define	ELC_TX_RESET		0x0b
#define	ELC_REQ_INTR		0x0c
#define	ELC_ACK_INTR		0x0d
#define	ELC_SET_INTR		0x0e
#define	ELC_SET_READ_ZERO	0x0f	/* SetIndicationEnable(IntStatus Msk) */
#define	ELC_SET_RX_FILTER	0x10
#define	ELC_SET_RX_EARLY	0x11
#define	ELC_SET_TX_AVAIL	0x12
#define	ELC_SET_TX_START	0x13
#define	ELC_START_DMA		0x14
#define	ELC_STAT_ENABLE		0x15
#define	ELC_STAT_DISABLE	0x16
#define	ELC_STOP_COAX		0x17

/* Interrupt Bits */
#define	ELINTR_LATCH		0x01
#define	ELINTR_ADAPT_FAIL	0x02
#define	ELINTR_TX_COMPLETE	0x04
#define	ELINTR_TX_AVAIL		0x08
#define	ELINTR_RX_COMPLETE	0x10
#define	ELINTR_RX_EARLY		0x20
#define	ELINTR_INTR_REQUESTED	0x40
#define	ELINTR_UPDATE_STATS	0x80
#define	ELINTR_DMA_COMPLETE	0x100
#define	ELINTR_DMA_INPROGRESS	0x800

#define	ELINTR_DISABLE		0x1ff
#define	ELINTR_DEFAULT(d)	((d) ? 0x1de : 0xfe)

/* RX Filter Definitions */
#define	ELRX_IND_ADDR		0x1
#define	ELRX_MULTI_ADDR		0x2
#define	ELRX_BROAD_ADDR		0x4
#define	ELRX_PROMISCUOUS	0x8
#define	ELRX_INIT_RX_FILTER	(ELRX_IND_ADDR|ELRX_BROAD_ADDR)

#define	ELRX_GET_LEN(v, r)	((r) & ((v) ? 0x1fff : 0x7ff))
#define	ELX_GET_RXERR(v, r, p)	((v) ? DDI_INB((p) + ELX_RX_ERROR) : \
					(((r) >> 11) & 0x7))
#define	ELRX_OVERRUN		0x1	/* ver 0 = 0x0 */
#define	ELRX_RUNT		0x2	/* ver 0 = 0x3 */
#define	ELRX_FRAME		0x4	/* ver 0 = 0x4 */
#define	ELRX_CRC		0x8	/* ver 0 = 0x5 */
#define	ELRX_OVERSIZE		0x10	/* ver 0 = 0x1 */
#define	ELRX_DRIBBLE		0x80	/* ver 0 = 0x2 */
#define	ELRX_GET_ERR(v, r)	((v) ? (r) : elx_rxetab[(r)])

#define	ELRX_INCOMPLETE		0x8000
#define	ELRX_ERROR		0x4000
#define	ELRX_STAT_MASK		(ELRX_INCOMPLETE|ELRX_ERROR)

/* Transmit definitions */

#define	ELTX_REQINTR		0x8000
#define	ELTX_COMPLETE		0x80
#define	ELTX_INTR_REQ		0x40
#define	ELTX_MAXCOLL		0x08
#define	ELTX_UNDERRUN		0x10
#define	ELTX_JABBER		0x20
#define	ELTX_STAT_OVERFLOW	0x04
#define	ELTX_ERRORS		(ELTX_UNDERRUN|ELTX_JABBER|ELTX_STAT_OVERFLOW)

/* Status Register Definitions */

#define	ELSTATUS_CIP		0x1000
#define	ELSTATUS_DIP		0x0800

/* address configuration register */
#define	ELAC_MED_10BT		0x0000 	/* twisted pair */
#define	ELAC_MED_10B2		0xc000 	/* BNC (internal) */
#define	ELAC_MED_AUI		0x4000 	/* AUI (external) */
#define	ELAC_MED_MASK(v)	((v) ? 0xe000 : 0xc000)
#define	ELAC_AUTO_SEL		0x80

/* Configuration Control Register */
#define	ELCONF_ENABLED		0x0001
#define	ELCONF_RESET		0x0004
#define	ELCONF_USE_INTERN	0x0100
#define	ELCONF_MED_10BT		0x0200
#define	ELCONF_MED_10B2		0x1000
#define	ELCONF_MED_AUI		0x2000
#define	ELCONF_MED_MASK		0x3200

/* Diagnostic register bits */
#define	ELD_MEDIA_AUI_DISABLE	0x8000
#define	ELD_MEDIA_BNC		0x4000
#define	ELD_MEDIA_INTERNAL	0x2000
#define	ELD_MEDIA_SQE		0x1000
#define	ELD_MEDIA_LB_DETECT	0x0800
#define	ELD_MEDIA_POLARITY	0x0400
#define	ELD_MEDIA_JABBER	0x0200
#define	ELD_MEDIA_UNSQUELCH	0x0100
#define	ELD_MEDIA_LB_ENABLE	0x0080
#define	ELD_MEDIA_JABBER_ENB	0x0040
#define	ELD_MEDIA_CRS		0x0020
#define	ELD_MEDIA_COLLISION	0x0010
#define	ELD_MEDIA_SQE_ENABLE	0x0008
#define	ELD_MEDIA_100		0x0001

#define	ELD_FIFO_RX_NORM	0x8000
#define	ELD_FIFO_RX_UNDER	0x2000
#define	ELD_FIFO_RX_STATUS	0x1000
#define	ELD_FIFO_RX_OVER	0x0800
#define	ELD_FIFO_TX_OVER	0x0400

#define	ELD_NET_TX_RESET	0x0100
#define	ELD_NET_TX_XMIT		0x0200
#define	ELD_NET_RX_ENABLED	0x0400
#define	ELD_NET_TX_ENABLED	0x0800
#define	ELD_NET_EXT_LBACK	0x8000

/* FIFO and other internal configuration bits */
#define	ELICONF_SIZE_MASK	0x00000007
#define	ELICONF_MEM_8K		0x00000000
#define	ELICONF_MEM_64K		0x00000003
#define	ELICONF_WIDTH		0x00000008
#define	ELICONF_WIDTH_IS_16	0x00000008
#define	ELICONF_RAMSPEED(x)		(((x) >> 4) & 0x3)
#define	ELICONF_GET_PARTITION(x)	(((x) >> 16) & 0x3)
#define	ELICONF_SET_PARTITION(value, x)	\
	((((x) & 0x3) << 16) | (value & ~(3 << 16)))
#define	ELICONF_PNP(x)		(((x) >> 18) & 0x3)
#define	ELICONF_AUTO_SEL	0x01000000
#define	ELICONF_MED_10BT	0x00000000
#define	ELICONF_MED_10AUI	0x00100000
#define	ELICONF_MED_10B2	0x00300000
#define	ELICONF_MED_100BTX	0x00400000
#define	ELICONF_MED_100BFX	0x00500000
#define	ELICONF_MED_MII		0x00600000
#define	ELICONF_MED_MASK	0x00700000

/* Reset options register bits */
#define	ELRO_MED_BT4	0x01		/* 100Base-T4 */
#define	ELRO_MED_BTX	0x02		/* 100Base-TX */
#define	ELRO_MED_BFX	0x04		/* 100Base-FX */
#define	ELRO_MED_10BT	0x08		/* 10Base-T */
#define	ELRO_MED_10B2	0x10		/* 10Base-coax */
#define	ELRO_MED_AUI	0x20		/* 10Base-AUI */
#define	ELRO_MED_MII	0x40		/* 100Base-MII */
#define	ELRO_MED_MASK	0x7f

/* Physical management register bits */
#define	ELPM_LTEST_DEFEAT	0x8000

#define	ELMC_FULL_DUPLEX	0x20


/* Valid IRQ levels - bit mask */
#define	ELX_VALID_IRQ	0x9EA8
#define	VALID_IRQ(irq) (ELX_VALID_IRQ & (1<<irq))
#define	GET_IRQ(value) ((value>>12)&0xF)

/* EEPROM Commands and bit patterns */

#define	EEPROM_READ		0x80
#define	EEPROM_WRITE		0x40
#define	EEPROM_ERASE		0x30
#define	EEPROM_ERASE_WENB	0x00
#define	EEPROM_ERASE_WDIS	0x20
#define	EEPROM_WRITE_ALL	0x10

#define	EEPROM_BUSY		0x8000
#define	EEPROM_CMD(cmd, arg)	(cmd|arg)

#define	EEPROM_PHYS_ADDR	0x00
#define	EEPROM_NODE_ID		0x00
#define	EEPROM_PROD_ID		0x03
#define	EEPROM_ADDR_CFG		0x08
#define	EEPROM_RESOURCE_CFG	0x09
#define	EEPROM_SOFTINFO		0x0d
#define	EEPROM_SOFTINFO2	0x0f
#define	EEPROM_CAPABILITIES	0x10
#define	EEPROM_SECONDARY_INFO	0x14

#define	ELS_LINKBEATDISABLE	0x4000

/* misc. functions */
#define	GET_WINDOW(reg)		(((reg)>>13)&0x7)

#define	GET_WINDOW_P(port)	(((DDI_INW(port + ELX_STATUS))>>13)&0x7)

#define	SET_WINDOW(port, window) \
	DDI_OUTW(port + ELX_COMMAND, COMMAND(ELC_SELECT_WINDOW, window&0x7))

#define	SWTCH_WINDOW(port, nwin, win) \
		if ((win = GET_WINDOW_P(port)) != nwin) \
			SET_WINDOW(port, nwin)

#define	RESTORE_WINDOW(port, owin, win) \
		if (win != owin) \
			SET_WINDOW(port, owin)

#define	MLEN(mp)	((mp)->b_wptr - (mp)->b_rptr)

#define	ELX_ID_PORT	0x140
#define	ELX_MAX_EISABUF	(16*1024)

#define	ELX_EARLY_RECEIVE	1024 /* initial guess */

/* ISA specific functions for ID port */
#define	ELISA_RESET	0x00
#define	ELISA_ID_INIT	0xff
#define	ELISA_ID_PATLEN	255
#define	ELISA_ID_OPAT	0xCF
#define	ELISA_EEPROM	0x80
#define	ELISA_ACTIVATE	0xFF
#define	ELISA_GLOBAL_RESET	0xC0
#define	ELISA_TAG	0xD0
#define	ELISA_TEST	0xD8
#define	ELISA_SET_TAG(tag) (ELISA_TAG+(tag&0x7))
#define	ELISA_TEST_TAG(tag)(ELISA_TEST+(tag&7))

#define	ELISA_READEEPROM(promaddr) (ELISA_EEPROM|(promaddr&0x3F))
#define	ELISA_READ_DELAY (162)

/* ISA specific definitions */
#define	ELX_MAX_ISA	8	/* assume no more than this may boards */
#define	ELX_IDPORT_BASE	0x100

/* device flags */
#define	ELF_CAPS	1	/* capabilites word available */
#define	ELF_FIFO_PART	2	/* FIFO may be partitioned */
#define	ELF_AUTOSENSE	4	/* media autosense available */
#define	ELF_DMA_SEND	0x10	/* DMA send busy */
#define	ELF_DMA_RECV	0x20	/* DMA recv busy */
#define	ELF_DMA_RCVBUF	0x40	/* DMA recv buf being used */
#define	ELF_DMA_XFR	(ELF_DMA_SEND|ELF_DMA_RECV)


#define	ELX_ASIC_REVISION(value)	((value >>1) & 0xF)

/* capabilities word */

#define	ELCAP_FRG_BM_DMA	0x0040 /* fragment bus master DMA */
#define	ELX_CAN_DMA(elxp)	(elxp->elx_caps & ELCAP_FRG_BM_DMA)

/* softinfo2 word */

#define	ELSI2_NORXOVN	0x0020	/* 10Mbps rcv overrun bug (1=fixed) */

#define	ELX_KVTOP(addr)	(((long)hat_getkpfnum((caddr_t)(addr)) * \
			(long)elxp->pagesize) + ((long)(addr) & 0xfff))

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ELX_H */
