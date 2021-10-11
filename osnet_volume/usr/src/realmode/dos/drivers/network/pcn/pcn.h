/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */
 
#ident "@(#)pcn.h	1.5	97/02/27 SMI"

#ifndef _PCN_H
#define	_PCN_H	1

/*
 * Generic PC-Net/LANCE real-mode driver
 */

/*
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	This header file is provided for use with pcn.c, the source for the
 *	AMD PCnet realmode driver PCN.BEF.  PCN.BEF is used as both a working
 *	working driver and a sample network realmode driver for reference
 *	when developing realmode drivers for other network adapters.
 *
 *	This file contains only definitions used by hardware-specific code
 *	in pcn.c.  There are no constructs that need to be replicated in
 *	header files for other drivers.  Other drivers will normally have a
 *	similar header file for device-dependent definitions but use of a
 *	header file is not required by the driver framework.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */

/*
 * Refer to AMD Data sheets for the Am7990, Am79C90, Am79C960, Am79C970
 * for details on the definitions used here.
 */

/*
 * Number of I/O port addresses used for ISA devices.
 */
#define	PCN_ISA_PORT_WIDTH 0x10

/*
 * IO port address offsets
 */

#define	PCN_IO_ADDRESS	0x00	/* ether address PROM is here */
#define	PCN_IO_RDP	0x10	/* Register Data Port */
#define	PCN_IO_RAP	0x12	/* Register Address Port */
#define	PCN_IO_RESET	0x14	/* Reset */
#define	PCN_IO_IDP	0x16	/* ISA Bus Data Port */
#define	PCN_IO_VENDOR	0x18	/* Vendor specific word */

/*
 * CSR indices
 */
#define	CSR0	0
#define	CSR1	1
#define	CSR2	2
#define	CSR3	3
#define	CSR88	88
#define	CSR89	89

/*
 * CSR0: 
 */
#define	CSR0_INIT	(1<<0)
#define	CSR0_STRT	(1<<1)
#define	CSR0_STOP	(1<<2)
#define	CSR0_TDMD	(1<<3)
#define	CSR0_TXON	(1<<4)
#define	CSR0_RXON	(1<<5)
#define	CSR0_INEA	(1<<6)
#define	CSR0_INTR	(1<<7)
#define	CSR0_IDON	(1<<8)
#define	CSR0_TINT	(1<<9)
#define	CSR0_RINT	(1<<10)
#define	CSR0_MERR	(1<<11)
#define	CSR0_MISS	(1<<12)
#define	CSR0_CERR	(1<<13)
#define	CSR0_BABL	(1<<14)
#define	CSR0_ERR	(1<<15)

/*
 * Structure definitions for adapter access.
 * These structures assume no padding between members.
 */

/*
 * Initialization block
 */

struct PCN_InitBlock {
	ushort	MODE;
	ushort	PADR[3];
	ushort	LADRF[4];
	ushort	RDRA[2];
	ushort	TDRA[2];
};

/*
 * Message Descriptor
 *
 */

struct pcn_msg_desc {
	ushort MD[4];
};

/*
 * MD1:
 */
#define	MD1_ENP	    (1<<8)
#define	MD1_STP	    (1<<9)
#define	MD1_BUFF    (1<<10)	    /* for RX */
#define	MD1_DEF	    (1<<10)	    /* for TX */
#define	MD1_CRC	    (1<<11)	    /* for RX */
#define	MD1_ONE	    (1<<11)	    /* for TX */
#define	MD1_OFLO    (1<<12)	    /* for RX */
#define	MD1_MORE    (1<<12)	    /* for TX */
#define	MD1_FRAM    (1<<13)	    /* for RX */
#define	MD1_ADD_FCS (1<<13)	    /* for TX */
#define	MD1_ERR	    (1<<14)
#define	MD1_OWN	    (1<<15)

#define	MD1_TXFLAGS	(MD1_STP|MD1_ENP|MD1_OWN)

/*
 * DMA Constants
 */
#define	PCN_DMA_1_MODE_REGS	(0x0B)
#define	PCN_DMA_1_MASK_REGS	(0x0A)
#define	PCN_DMA_2_MODE_REGS	(0xD6)
#define	PCN_DMA_2_MASK_REGS	(0xD4)
#define	PCN_DMA_1_CMND_REGS	(0x08)
#define	PCN_DMA_2_CMND_REGS	(0xD0)
#define	PCN_CASCADE		(0xC0)

/*
 * PCI Constants
 */
#define	PCI_AMD_VENDOR_ID	0x1022
#define	PCI_PCNET_ID		0x2000


/*
 * Possible base address for the POS registers
 */
#define	PCN_IOBASE_ARRAY_SIZE	16

struct pcnIOBase {
	ushort	iobase;
	ushort	bustype;
	ushort	cookie;
	ushort	irq;
	ushort	dma;
};

enum {	PCN_BUS_NONE = 0,
	PCN_BUS_ISA,
	PCN_BUS_PCI };

/*
 * Ethernet constants
 */
#define	ETH_ADDR_SIZE	6
#define	ETH_MAX_TU	1514
#define	ETH_MIN_TU	64

#define	LANCE_FCS_SIZE	4

/*
 * Buffer ring definitions
 * Since this is a real-mode driver, it can be less than optimal
 * with respect to memory management.  The buffer sizes used are
 * the Ether MTU, and only four each are allocated.
 *
 * Any changes here may require changes to the code in pcn.c,
 * notably pcn_ProcessReceive(), since there are assumptions
 * there about the definitions here
 */
#define	PCN_RX_RING_VAL		2		/* log2(size) */
#define	PCN_RX_RING_SIZE	(1<<PCN_RX_RING_VAL)
#define	PCN_RX_RING_MASK	(PCN_RX_RING_SIZE-1)

#define	PCN_TX_RING_VAL		1		/* log2(size) */
#define	PCN_TX_RING_SIZE	(1<<PCN_TX_RING_VAL)
#define	PCN_TX_RING_MASK	(PCN_TX_RING_SIZE-1)

#define	PCN_RX_BUF_SIZE		(ETH_MAX_TU+LANCE_FCS_SIZE)
#define	PCN_TX_BUF_SIZE		ETH_MAX_TU


#pragma pack(16)
typedef struct pcn_instance {
	ulong	linear_base_address;
	int	tx_index;	/* where to look for send buffer */
	int	rx_index;	/* where to look for received */

	struct PCN_InitBlock	initblock;
	struct pcn_msg_desc	rx_ring[PCN_RX_RING_SIZE];
	struct pcn_msg_desc	tx_ring[PCN_TX_RING_SIZE];

	unchar	tx_buffer[PCN_TX_RING_SIZE][PCN_TX_BUF_SIZE];
	unchar	rx_buffer[PCN_RX_RING_SIZE][PCN_RX_BUF_SIZE];

	ushort	iobase;
	int	irq;
	int	dma;

	ulong	tx_no_buff;	/* how many packets dropped due to no buffer */
} pcn_instance;
#pragma pack()

#define	NextRXIndex(index)	(((index)+1)&PCN_RX_RING_MASK)
#define	NextTXIndex(index)	(((index)+1)&PCN_TX_RING_MASK)

#endif	/* _PCN_H */
