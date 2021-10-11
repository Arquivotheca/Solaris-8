/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ELX_H
#define	_ELX_H

#pragma ident	"@(#)pcelx.h	1.4	99/10/27 SMI"

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
#define	ELXINIT		0x80

#ifdef DEBUG
#define	ELXDEBUG 1
#endif

/* Misc */
#define	ELXHIWAT	65536		/* driver flow control high water */
#define	ELXLOWAT	16384		/* driver flow control low water */
#define	ELXMAXPKT	1500		/* maximum media frame size */
#define	ELXMAXFRAME	1518		/* maximum with header */
#define	ELXIDNUM	0		/* should be a unique id; zero works */

/* board state */
#define	ELX_IDLE	0
#define	ELX_WAITRCV	1
#define	ELX_XMTBUSY	2
#define	ELX_ERROR	3

/* driver specific declarations */
#define	ELX_PROM_SIZE	0x20
struct elxinstance {
	int	elx_rxbits;	/* current receiver modes */
	int	elx_mcount;	/* number of multicast references */
	int	elx_irq;	/* real IRQ to avoid problems of reset */
	int	elx_latency;	/* interrupt latency */
	int	elx_latency_hi;	/* high level part */
	mblk_t *elx_rcvbuf;	/* early receive mblk */
	mblk_t *elx_xmtbuf;
	mblk_t *elx_rcvhi;
	int	elx_earlyrcv;	/* current early receive threshhold */
	int	elx_bus;
	int	elx_softinfo;	/* various flags in software info word */
	int	elx_features;	/* more flags of various types */
	int	elx_fifosize;
	/* beyond this point are PCMCIA specific variables. */
	kcondvar_t	elx_condvar;
	client_handle_t elx_handle;
	kmutex_t	elx_cslock;
	int	elx_socket;
	int	elx_config;	/* default config index */
	int	elx_config_hi;	/* config index for high power consumer */
	int	elx_config_base;
	int	elx_config_present;
	int	elx_vcc;
	int	elx_iodecode;
	int	elx_pcinfo;
	int	elx_intrstat;
	ushort_t	elx_promcopy[ELX_PROM_SIZE];
	acc_handle_t	elx_port;
	acc_handle_t	elx_portdata;
	kmutex_t	elx_intrlock;		/* hilevel interrupt mutex */
	uint_t		(*elx_intr_hi)();	/* hi level handler */
	ddi_softintr_t	elx_softid;		/* soft int trigger */
	int		elx_intr_flags;

};

/* elx_intr_flags */
#define	ELX_INTR_READY	0x01			/* ready to take interrupts */

/* Bus type information */

#define	ELX_ISA		0
#define	ELX_EISA	2
#define	ELX_PCMCIA	4
#if defined(i386)
#define	ELX_BUS_DEFAULT	ELX_EISA
#else
#define	ELX_BUS_DEFAULT	ELX_PCMCIA
#endif

#define	EL_3COM_ID	0x6D50	/* 3COM's MFG ID */
#define	EL_PRODUCT_ID	0x9050	/* board ID type */
#define	EL_PRODID_MASK  0xf0ff

/* Card type information and ASIC revision */
#define	ELX_3C5X9	1
#define	ELX_3C5X9B	2

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
#define	ELX_INTERNAL_CONFIG	0x0 /* internal configuration reg (B only) */
#define	ELX_FREE_RX_BYTES	0xa

/* Window 4 offsets */
				/* read/write */
#define	ELX_TX_DIAGNOSTIC	0x0
#define	ELX_HOST_DIAGNOSTIC	0x2
#define	ELX_FIFO_DIAGNOSTIC	0x4
#define	ELX_NET_DIAGNOSTIC	0x6
#define	ELX_CONT_STATUS		0x8
#define	ELX_MEDIA_STATUS	0xa

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
#define	ELC_SET_READ_ZERO	0x0f
#define	ELC_SET_RX_FILTER	0x10
#define	ELC_SET_RX_EARLY	0x11
#define	ELC_SET_TX_AVAIL	0x12
#define	ELC_SET_TX_START	0x13
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

#define	ELINTR_DEFAULT		0xff
#define	ELINTR_READ_ALL		0xfe
#define	ELINTR_INTR_ALL		0xff

/* RX Filter Definitions */
#define	ELRX_IND_ADDR		0x1
#define	ELRX_MULTI_ADDR		0x2
#define	ELRX_BROAD_ADDR		0x4
#define	ELRX_PROMISCUOUS	0x8

#define	ELRX_INIT_RX_FILTER	(ELRX_IND_ADDR|ELRX_BROAD_ADDR)
#define	ELRX_LENGTH_MASK	0x7ff

#define	ELRX_OVERRUN		0x8
#define	ELRX_RUNT		0xB
#define	ELRX_FRAME		0xC
#define	ELRX_CRC		0xD
#define	ELRX_OVERSIZE		0x9
#define	ELRX_DRIBBLE		0x2
#define	ELRX_GET_ERR(x)		(((x) >> 11) & 0xF)

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

#define	GET_WINDOW(reg)		((((ulong_t)reg)>>13)&0x7)

#define	ELSTATUS_CIP		0x1000

/* Configuration Control Register */
#define	ELCONF_ENABLED		0x0001
#define	ELCONF_RESET		0x0004
#define	ELCONF_USE_INTERN	0x0100
#define	ELCONF_10BASET		0x0200
#define	ELCONF_BNC		0x1000
#define	ELCONF_AUI		0x2000
#define	ELCONF_TYPEB		0x8000

/* Configuration Address Register */
#define	ELCFGADDR_ASE		0x0080 /* autoselect media type */

/* Diagnostic register bits */
#define	ELD_MEDIA_AUI_DISABLE	0x8000
#define	ELD_MEDIA_BNC		0x4000
#define	ELD_MEDIA_INTERNAL	0x2000
#define	ELD_MEDIA_SQE		0x1000
#define	ELD_MEDIA_LB_CORRECT	0x0800
#define	ELD_MEDIA_POLARITY	0x0400
#define	ELD_MEDIA_JABBER	0x0200
#define	ELD_MEDIA_UNSQUELCH	0x0100
#define	ELD_MEDIA_LB_ENABLE	0x0080
#define	ELD_MEDIA_JABBER_ENB	0x0040
#define	ELD_MEDIA_CRS		0x0020
#define	ELD_MEDIA_COLLISION	0x0010
#define	ELD_MEDIA_SQE_ENABLE	0x0008

#define	ELD_FIFO_RX_NORM	0x8000
#define	ELD_FIFO_RX_UNDER	0x2000
#define	ELD_FIFO_RX_STATUS	0x1000
#define	ELD_FIFO_RX_OVER	0x0800
#define	ELD_FIFO_TX_OVER	0x0400

#define	ELD_NET_TX_RESET	0x0100
#define	ELD_NET_TX_XMIT		0x0200
#define	ELD_NET_RX_ENABLED	0x0400
#define	ELD_NET_TX_ENABLED	0x0800
#define	ELD_NET_EXT_LOOPBACK	0x8000

/* FIFO and other internal configuration bits */
#define	ELICONF_SIZE_MASK	0x00000007
#define	ELICONF_MEM_8K		0x0
#define	ELICONF_MEM_32K		0x2
#define	ELICONF_WIDTH		0x00000008
#define	ELICONF_WIDTH_IS_16	0x00000008
#define	ELICONF_RAMSPEED(x)	(((x) >> 4) & 0x3)
#define	ELICONF_GET_PARTITION(x)	(((x) >> 16) & 0x3)
#define	ELICONF_SET_PARTITION(value, x)	((((x) & 0x3) << 16) | \
						(value & ~(3 << 16)))
#define	ELICONF_PNP(x)		(((x) >> 18) & 0x3)

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
#define	EEPROM_CMD(cmd, arg)	(cmd | arg)

#define	EEPROM_PHYS_ADDR	0x00
#define	EEPROM_PROD_ID		0x03
#define	EEPROM_ADDR_CFG		0x08
#define	EEPROM_RESOURCE_CFG	0x09
#define	EEPROM_OEM_ADDR		0x0a
#define	EEPROM_SOFTINFO		0x0d
#define	EEPROM_COMPATIBILITY	0x0e
#define	EEPROM_CAPABILITIES	0x10
#define	EEPROM_SECONDARY_INFO	0x14

#define	ELS_LINKBEATDISABLE	0x4000

/* compatibility and fail levels */
#define	ELCOMPAT_FAIL_LEVEL	0
#define	ELCOMPAT_WARN_LEVEL	0
				/* exceptions to general case */
#define	ELCOMPAT_FAIL_3C589B	1
#define	ELCOMPAT_WARN_3C589B	1

#define	ELCOMPAT_FAIL(x)	(((x) >> 8) & 0xFF)
#define	ELCOMPAT_WARN(x)	((x) & 0xFF)

/* capabilities and features */
#define	ELF_TYPE_B		0x0001 /* A type B card */
#define	ELF_PCMCIA		0x0002 /* its a PCMCIA card */
/* extended card features */
#define	ELF_FIFO_8K		0x00100
#define	ELF_PROM_IN_CIS		0x00200

#define	ELF_USE_3COM_NODE	0x8000000

/* media types */
#define	ELM_TP			0x00 /* twisted pair */
#define	ELM_AUI			0x01 /* AUI (external) */
#define	ELM_BNC			0x03 /* BNC (internal) */
#define	ELM_MEDIA_MASK		0x3fffffff

/* misc. functions */
#define	SET_WINDOW(port, window) \
	csx_Put16(port, ELX_COMMAND, \
			leshort(COMMAND(ELC_SELECT_WINDOW, window&0x7)))

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

/* PCMCIA specific */
#define	ELPC_MANFID_MANF	0x0101
#define	ELPC_MANFID_CARD_589	0x0589 /* single function card */
#define	ELPC_MANFID_CARD_562	0x0562 /* multi function card */

#define	ELPC_INFO_8BIT		0x0001 /* 8-bit works */
#define	ELPC_INFO_16BIT		0x0002 /* 16-bit works */
#define	ELPC_INFO_FORCE_8BIT	0x0004

#define	ELX_REGISTERED		0x10000
#define	ELX_CONFIG_IO		0x20000
#define	ELX_CONFIG_IRQ		0x40000
#define	ELX_CONFIG_CONFIG	0x80000
#define	ELX_CARD_PRESENT	0x08000
#define	ELX_MUTEX_INIT		0x02000
#define	ELX_CARD_REMOVED	0x04000	/* stronger than just not present */
#define	ELX_NEED_INTRBIT	0x100000
#define	ELX_SOCKET_MASK		0x200000
#define	ELX_CONFIG_FAILED	0x400000
#define	ELX_INTR_PENDING	0x800000 /* intr pending low handler */
#define	ELX_IN_INTR		0x1000000 /* in the general/low handler */

#define	ELX_CS_READY		(ELX_REGISTERED|ELX_CARD_PRESENT)

#define	ELXPC_DISABLE_OTHER_FUNC	0x8

#define	ELX_ASIC_REVISION(value)	((value >> 1) & 0x1F)

/* Capabilities word definitions */

#define	ELCAP_PLUG_AND_PLAY	0x0001 /* Plug and Play supported */
#define	ELCAP_TYPE_B		0x0002
#define	ELCAP_CRC_PASS_THRU	0x0080
#define	ELCAP_POWER_MGMT	0x2000 /* has power management feature */
#define	ELX_PCMCIA_IRQ		0x3 /* this is the only one that works */

#define	PCELX_DEVICETYPE	"device_type"
#define	PCELX_DEVI_ISA_NEXNAME	"isa"
#define	PCELX_DEVI_EISA_NEXNAME	"eisa"

#ifdef	__cplusplus
}
#endif

#endif /* _ELX_H */
