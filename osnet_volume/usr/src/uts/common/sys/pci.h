/*
 * Copyright (c) 1994,1995,1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_H
#define	_SYS_PCI_H

#pragma ident	"@(#)pci.h	1.23	99/10/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * PCI Configuration Header offsets
 */
#define	PCI_CONF_VENID		0x0	/* vendor id, 2 bytes */
#define	PCI_CONF_DEVID		0x2	/* device id, 2 bytes */
#define	PCI_CONF_COMM		0x4	/* command register, 2 bytes */
#define	PCI_CONF_STAT		0x6	/* status register, 2 bytes */
#define	PCI_CONF_REVID		0x8	/* revision id, 1 byte */
#define	PCI_CONF_PROGCLASS	0x9	/* programming class code, 1 byte */
#define	PCI_CONF_SUBCLASS	0xA	/* sub-class code, 1 byte */
#define	PCI_CONF_BASCLASS	0xB	/* basic class code, 1 byte */
#define	PCI_CONF_CACHE_LINESZ	0xC	/* cache line size, 1 byte */
#define	PCI_CONF_LATENCY_TIMER	0xD	/* latency timer, 1 byte */
#define	PCI_CONF_HEADER		0xE	/* header type, 1 byte */
#define	PCI_CONF_BIST		0xF	/* builtin self test, 1 byte */
#define	PCI_CONF_BASE0		0x10	/* base register 0, 4 bytes */
#define	PCI_CONF_BASE1		0x14	/* base register 1, 4 bytes */
#define	PCI_CONF_BASE2		0x18	/* base register 2, 4 bytes */
#define	PCI_CONF_BASE3		0x1c	/* base register 3, 4 bytes */
#define	PCI_CONF_BASE4		0x20	/* base register 4, 4 bytes */
#define	PCI_CONF_BASE5		0x24	/* base register 5, 4 bytes */
#define	PCI_CONF_CIS		0x28	/* Cardbus CIS Pointer */
#define	PCI_CONF_SUBVENID	0x2c	/* Subsystem Vendor ID */
#define	PCI_CONF_SUBSYSID	0x2e	/* Subsystem ID */
#define	PCI_CONF_ROM		0x30	/* ROM base register, 4 bytes */
#define	PCI_CONF_CAP_PTR	0x34	/* capabilities pointer, 1 byte */
#define	PCI_CONF_ILINE		0x3c	/* interrupt line, 1 byte */
#define	PCI_CONF_IPIN		0x3d	/* interrupt pin, 1 byte */
#define	PCI_CONF_MIN_G		0x3e	/* minimum grant, 1 byte */
#define	PCI_CONF_MAX_L		0x3f	/* maximum grant, 1 byte */

/*
 * PCI to PCI bridge configuration space header format
 */
#define	PCI_BCNF_PRIBUS		0x18	/* primary bus number */
#define	PCI_BCNF_SECBUS		0x19	/* secondary bus number */
#define	PCI_BCNF_SUBBUS		0x1a	/* subordinate bus number */
#define	PCI_BCNF_LATENCY_TIMER	0x1b
#define	PCI_BCNF_IO_BASE_LOW	0x1c
#define	PCI_BCNF_IO_LIMIT_LOW	0x1d
#define	PCI_BCNF_SEC_STATUS	0x1e
#define	PCI_BCNF_MEM_BASE	0x20
#define	PCI_BCNF_MEM_LIMIT	0x22
#define	PCI_BCNF_PF_BASE_LOW	0x24
#define	PCI_BCNF_PF_LIMIT_LOW	0x26
#define	PCI_BCNF_PF_BASE_HIGH	0x28
#define	PCI_BCNF_PF_LIMIT_HIGH	0x2c
#define	PCI_BCNF_IO_BASE_HI	0x30
#define	PCI_BCNF_IO_LIMIT_HI	0x32
#define	PCI_BCNF_CAP_PTR	0x34
#define	PCI_BCNF_ROM		0x38
#define	PCI_BCNF_ILINE		0x3c
#define	PCI_BCNF_IPIN		0x3d
#define	PCI_BCNF_BCNTRL		0x3e

/*
 * PCI to PCI bridge control register (0x3e) format
 */
#define	PCI_BCNF_BCNTRL_PARITY_ENABLE	0x0001
#define	PCI_BCNF_BCNTRL_SERR_ENABLE	0x0002
#define	PCI_BCNF_BCNTRL_MAST_AB_MODE	0x0020

#define	PCI_BCNF_IO_MASK	0xf0
#define	PCI_BCNF_MEM_MASK	0xfff0

/*
 * PCI command register bits
 */
#define	PCI_COMM_IO		0x0001   /* I/O access enable */
#define	PCI_COMM_MAE		0x0002   /* memory access enable */
#define	PCI_COMM_ME		0x0004   /* master enable */
#define	PCI_COMM_SPEC_CYC	0x0008
#define	PCI_COMM_MEMWR_INVAL	0x0010
#define	PCI_COMM_PALETTE_SNOOP	0x0020
#define	PCI_COMM_PARITY_DETECT	0x0040
#define	PCI_COMM_WAIT_CYC_ENAB	0x0080
#define	PCI_COMM_SERR_ENABLE	0x0100
#define	PCI_COMM_BACK2BACK_ENAB	0x0200

/*
 * PCI Interrupt pin value
 */
#define	PCI_INTA	1
#define	PCI_INTB	2
#define	PCI_INTC	3
#define	PCI_INTD	4

/*
 * PCI status register bits
 */
#define	PCI_STAT_CAP		0x0010   /* Implements Capabilities */
#define	PCI_STAT_66MHZ		0x0020   /* 66 MHz capable */
#define	PCI_STAT_UDF		0x0040   /* UDF supported */
#define	PCI_STAT_FBBC		0x0080   /* Fast Back-to-Back Capable */
#define	PCI_STAT_S_PERROR	0x0100   /* Data Parity Reported */
#define	PCI_STAT_DEVSELT	0x0600   /* Device select timing */
#define	PCI_STAT_S_TARG_AB	0x0800   /* Signaled Target Abort */
#define	PCI_STAT_R_TARG_AB	0x1000   /* Received Target Abort */
#define	PCI_STAT_R_MAST_AB	0x2000   /* Received Master Abort */
#define	PCI_STAT_S_SYSERR	0x4000   /* Signaled System Error */
#define	PCI_STAT_PERROR		0x8000   /* Detected Parity Error */

/*
 * DEVSEL timing values
 */
#define	PCI_STAT_DEVSELT_FAST	0x0000
#define	PCI_STAT_DEVSELT_MEDIUM	0x0200
#define	PCI_STAT_DEVSELT_SLOW	0x0400

/*
 * BIST values
 */
#define	PCI_BIST_SUPPORTED	0x80
#define	PCI_BIST_GO		0x40
#define	PCI_BIST_RESULT_M	0x0f
#define	PCI_BIST_RESULT_OK	0x00

/*
 * PCI class codes
 */
#define	PCI_CLASS_NONE		0x0	/* class code for pre-2.0 devices */
#define	PCI_CLASS_MASS		0x1	/* Mass storage Controller class */
#define	PCI_CLASS_NET		0x2	/* Network Controller class */
#define	PCI_CLASS_DISPLAY	0x3	/* Display Controller class */
#define	PCI_CLASS_MM		0x4	/* Multimedia Controller class */
#define	PCI_CLASS_MEM		0x5	/* Memory Controller class */
#define	PCI_CLASS_BRIDGE	0x6	/* Bridge Controller class */
#define	PCI_CLASS_COMM		0x7	/* Communications Controller class */
#define	PCI_CLASS_PERIPH	0x8	/* Peripheral Controller class */
#define	PCI_CLASS_INPUT		0x9	/* Input Device class */
#define	PCI_CLASS_DOCK		0xa	/* Docking Station class */
#define	PCI_CLASS_PROCESSOR	0xb	/* Processor class */
#define	PCI_CLASS_SERIALBUS	0xc	/* Serial Bus class */

/*
 * PCI Sub-class codes - base class 0x0 (no new devices should use this code).
 */
#define	PCI_NONE_NOTVGA		0x0	/* All devices except VGA compatible */
#define	PCI_NONE_VGA		0x1	/* VGA compatible */

/*
 * PCI Sub-class codes - base class 0x1
 */
#define	PCI_MASS_SCSI		0x0	/* SCSI bus Controller */
#define	PCI_MASS_IDE		0x1	/* IDE Controller */
#define	PCI_MASS_FD		0x2	/* floppy disk Controller */
#define	PCI_MASS_IPI		0x3	/* IPI bus Controller */
#define	PCI_MASS_RAID		0x4	/* RAID Controller */
#define	PCI_MASS_OTHER		0x80	/* Other Mass Storage Controller */

/*
 * programming interface for IDE
 */
#define	PCI_IDE_IF_NATIVE_PRI	0x01	/* primary channel is native */
#define	PCI_IDE_IF_PROG_PRI	0x02	/* primary can operate in either mode */
#define	PCI_IDE_IF_NATIVE_SEC	0x04	/* secondary channel is native */
#define	PCI_IDE_IF_PROG_SEC	0x08	/* sec. can operate in either mode */
#define	PCI_IDE_IF_MASK		0x0f	/* programming interface mask */

/*
 * PCI Sub-class codes - base class 0x2
 */
#define	PCI_NET_ENET		0x0	/* Ethernet Controller */
#define	PCI_NET_TOKEN		0x1	/* Token Ring Controller */
#define	PCI_NET_FDDI		0x2	/* FDDI Controller */
#define	PCI_NET_ATM		0x3	/* ATM Controller */
#define	PCI_NET_OTHER		0x80	/* Other Network Controller */

/*
 * PCI Sub-class codes - base class 0x3
 */
#define	PCI_DISPLAY_VGA		0x0   /* VGA device */
#define	PCI_DISPLAY_XGA		0x1   /* XGA device */
#define	PCI_DISPLAY_OTHER	0x80  /* Other Display Device */

/*
 * programming interface for display
 */
#define	PCI_DISPLAY_IF_VGA	0x0	/* VGA compatible */
#define	PCI_DISPLAY_IF_8514	0x1	/* 8514 compatible */

/*
 * PCI Sub-class codes - base class 0x4
 */
#define	PCI_MM_VIDEO		0x0   /* Video device */
#define	PCI_MM_AUDIO		0x1   /* Audio device */
#define	PCI_MM_OTHER		0x80  /* Other Multimedia Device */

/*
 * PCI Sub-class codes - base class 0x5
 */
#define	PCI_MEM_RAM		0x0   /* RAM device */
#define	PCI_MEM_FLASH		0x1   /* FLASH device */
#define	PCI_MEM_OTHER		0x80  /* Other Memory Controller */

/*
 * PCI Sub-class codes - base class 0x6
 */
#define	PCI_BRIDGE_HOST		0x0   /* Host/PCI Bridge */
#define	PCI_BRIDGE_ISA		0x1   /* PCI/ISA Bridge */
#define	PCI_BRIDGE_EISA		0x2   /* PCI/EISA Bridge */
#define	PCI_BRIDGE_MC		0x3   /* PCI/MC Bridge */
#define	PCI_BRIDGE_PCI		0x4   /* PCI/PCI Bridge */
#define	PCI_BRIDGE_PCMCIA	0x5   /* PCI/PCMCIA Bridge */
#define	PCI_BRIDGE_NUBUS	0x6   /* PCI/NUBUS Bridge */
#define	PCI_BRIDGE_CARDBUS	0x7   /* PCI/CARDBUS Bridge */
#define	PCI_BRIDGE_OTHER	0x80  /* PCI/Other Bridge Device */

/*
 * PCI Sub-class codes - base class 0x7
 */
#define	PCI_COMM_GENERIC_XT	0x0   /* XT Compatible Serial Controller */
#define	PCI_COMM_PARALLEL	0x1   /* Parallel Port Controller */
#define	PCI_COMM_OTHER		0x80  /* Other Communications Controller */

/*
 * Programming interfaces for class 0x7 / subclass 0x0 (Serial)
 */
#define	PCI_COMM_SERIAL_IF_GENERIC	0x00
#define	PCI_COMM_SERIAL_IF_16450	0x01
#define	PCI_COMM_SERIAL_IF_16550	0x02

/*
 * Programming interfaces for class 0x7 / subclass 0x1 (Parallel)
 */
#define	PCI_COMM_PARALLEL_IF_GENERIC	0x00
#define	PCI_COMM_PARALLEL_IF_BIDIRECT	0x01
#define	PCI_COMM_PARALLEL_IF_ECP	0x02

/*
 * PCI Sub-class codes - base class 0x8
 */
#define	PCI_PERIPH_PIC		0x0   /* Generic PIC */
#define	PCI_PERIPH_DMA		0x1   /* Generic DMA Controller */
#define	PCI_PERIPH_TIMER	0x2   /* Generic System Timer Controller */
#define	PCI_PERIPH_RTC		0x3   /* Generic RTC Controller */
#define	PCI_PERIPH_OTHER	0x80  /* Other System Peripheral */

/*
 * Programming interfaces for class 0x8 / subclass 0x0 (interrupt controller)
 */
#define	PCI_PERIPH_PIC_IF_GENERIC	0x00
#define	PCI_PERIPH_PIC_IF_ISA		0x01
#define	PCI_PERIPH_PIC_IF_EISA		0x02

/*
 * Programming interfaces for class 0x8 / subclass 0x1 (DMA controller)
 */
#define	PCI_PERIPH_DMA_IF_GENERIC	0x00
#define	PCI_PERIPH_DMA_IF_ISA		0x01
#define	PCI_PERIPH_DMA_IF_EISA		0x02

/*
 * Programming interfaces for class 0x8 / subclass 0x2 (timer)
 */
#define	PCI_PERIPH_TIMER_IF_GENERIC	0x00
#define	PCI_PERIPH_TIMER_IF_ISA		0x01
#define	PCI_PERIPH_TIMER_IF_EISA	0x02

/*
 * Programming interfaces for class 0x8 / subclass 0x3 (realtime clock)
 */
#define	PCI_PERIPH_RTC_IF_GENERIC	0x00
#define	PCI_PERIPH_RTC_IF_ISA		0x01

/*
 * PCI Sub-class codes - base class 0x9
 */
#define	PCI_INPUT_KEYBOARD	0x0   /* Keyboard Controller */
#define	PCI_INPUT_DIGITIZ	0x1   /* Digitizer (Pen) */
#define	PCI_INPUT_MOUSE		0x2   /* Mouse Controller */
#define	PCI_INPUT_OTHER		0x80  /* Other Input Controller */

/*
 * PCI Sub-class codes - base class 0xa
 */
#define	PCI_DOCK_GENERIC	0x0   /* Generic Docking Station */
#define	PCI_DOCK_OTHER		0x80  /* Other Type of Docking Station */

/*
 * PCI Sub-class codes - base class 0xb
 */
#define	PCI_PROCESSOR_386	0x0   /* 386 */
#define	PCI_PROCESSOR_486	0x1   /* 486 */
#define	PCI_PROCESSOR_PENT	0x2   /* Pentium */
#define	PCI_PROCESSOR_ALPHA	0x10  /* Alpha */
#define	PCI_PROCESSOR_POWERPC	0x20  /* PowerPC */
#define	PCI_PROCESSOR_COPROC	0x40  /* Co-processor */

/*
 * PCI Sub-class codes - base class 0xc
 */
#define	PCI_SERIAL_FIRE		0x0   /* FireWire (IEEE 1394) */
#define	PCI_SERIAL_ACCESS	0x1   /* ACCESS.bus */
#define	PCI_SERIAL_SSA		0x2   /* SSA */
#define	PCI_SERIAL_USB		0x3   /* Universal Serial Bus */
#define	PCI_SERIAL_FIBRE	0x4   /* Fibre Channel */

/* PCI header decode */
#define	PCI_HEADER_MULTI	0x80  /* multi-function device */
#define	PCI_HEADER_ZERO		0x00  /* type zero PCI header */
#define	PCI_HEADER_ONE		0x01  /* type one PCI header */
#define	PCI_HEADER_PPB		PCI_HEADER_ONE  /* type one PCI to PCI Bridge */

#define	PCI_HEADER_TYPE_M	0x7f  /* type mask for header */

/*
 * Base register bit definitions.
 */
#define	PCI_BASE_SPACE_M    0x00000001  /* memory space indicator */
#define	PCI_BASE_SPACE_IO   0x1   /* IO space */
#define	PCI_BASE_SPACE_MEM  0x0   /* memory space */

#define	PCI_BASE_TYPE_MEM   0x0   /* 32-bit memory address */
#define	PCI_BASE_TYPE_LOW   0x2   /* less than 1Mb address */
#define	PCI_BASE_TYPE_ALL   0x4   /* 64-bit memory address */
#define	PCI_BASE_TYPE_RES   0x6   /* reserved */

#define	PCI_BASE_TYPE_M		0x00000006  /* type indicator mask */
#define	PCI_BASE_PREF_M		0x00000008  /* prefetch mask */
#define	PCI_BASE_M_ADDR_M	0xfffffff0  /* memory address mask */
#define	PCI_BASE_IO_ADDR_M	0xfffffffe  /* I/O address mask */

#define	PCI_BASE_ROM_ADDR_M	0xfffff800  /* ROM address mask */
#define	PCI_BASE_ROM_ENABLE	0x00000001  /* ROM decoder enable */

/*
 * PCI to PCI bus bridge (redundant against PCI_BCNF_*)
 */
#define	PCI_PPB_ROM		0x38	/* expansion ROM base address */
#define	PCI_PPB_BASE_NUM	0x2 	/* number of base registers */

/*
 * Capabilities linked list entry offsets
 */
#define	PCI_CAP_ID		0x0	/* capability identifier, 1 byte */
#define	PCI_CAP_NEXT_PTR	0x1	/* next entry pointer, 1 byte */

/*
 * Capability identifier values
 */
#define	PCI_CAP_ID_PM		0x1	/* power management entry */

/*
 * Capability next entry pointer values
 */
#define	PCI_CAP_NEXT_PTR_NULL	0x0	/* no more entries in the list */

/*
 * PCI power management (PM) capability entry offsets
 */
#define	PCI_PMCAP		0x2	/* PM capabilities, 2 bytes */
#define	PCI_PMCSR		0x4	/* PM control/status reg, 2 bytes */
#define	PCI_PMCSR_BSE		0x6	/* PCI-PCI bridge extensions, 1 byte */
#define	PCI_PMDATA		0x7	/* PM data, 1 byte */

/*
 * PM capabilities values - 2 bytes
 */
#define	PCI_PMCAP_VER_1_0	0x0001	/* PCI PM spec 1.0 */
#define	PCI_PMCAP_VER_1_1	0x0002	/* PCI PM spec 1.1 */
#define	PCI_PMCAP_VER_MASK	0x0007	/* version mask */
#define	PCI_PMCAP_PME_CLOCK	0x0008	/* needs PCI clock for PME */
#define	PCI_PMCAP_DSI		0x0020	/* needs device specific init */
#define	PCI_PMCAP_AUX_CUR_SELF	0x0000	/* 0 aux current - self powered */
#define	PCI_PMCAP_AUX_CUR_55mA	0x0040	/* 55 mA aux current */
#define	PCI_PMCAP_AUX_CUR_100mA	0x0080	/* 100 mA aux current */
#define	PCI_PMCAP_AUX_CUR_160mA	0x00c0	/* 160 mA aux current */
#define	PCI_PMCAP_AUX_CUR_220mA	0x0100	/* 220 mA aux current */
#define	PCI_PMCAP_AUX_CUR_270mA	0x0140	/* 270 mA aux current */
#define	PCI_PMCAP_AUX_CUR_320mA	0x0180	/* 320 mA aux current */
#define	PCI_PMCAP_AUX_CUR_375mA	0x01c0	/* 375 mA aux current */
#define	PCI_PMCAP_AUX_CUR_MASK	0x01c0	/* 3.3Vaux aux current needs */
#define	PCI_PMCAP_D1		0x0200	/* D1 state supported */
#define	PCI_PMCAP_D2		0x0400	/* D2 state supported */
#define	PCI_PMCAP_D0_PME	0x0800	/* PME from D0 */
#define	PCI_PMCAP_D1_PME	0x1000	/* PME from D1 */
#define	PCI_PMCAP_D2_PME	0x2000	/* PME from D2 */
#define	PCI_PMCAP_D3HOT_PME	0x4000	/* PME from D3hot */
#define	PCI_PMCAP_D3COLD_PME	0x8000	/* PME from D3cold */
#define	PCI_PMCAP_PME_MASK	0xf800	/* PME support mask */

/*
 * PM control/status values - 2 bytes
 */
#define	PCI_PMCSR_D0			0x0000	/* power state D0 */
#define	PCI_PMCSR_D1			0x0001	/* power state D1 */
#define	PCI_PMCSR_D2			0x0002	/* power state D2 */
#define	PCI_PMCSR_D3HOT			0x0003	/* power state D3hot */
#define	PCI_PMCSR_STATE_MASK		0x0003	/* power state mask */
#define	PCI_PMCSR_PME_EN		0x0100	/* enable PME assertion */
#define	PCI_PMCSR_DSEL_D0_PWR_C		0x0000	/* D0 power consumed */
#define	PCI_PMCSR_DSEL_D1_PWR_C		0x0200	/* D1 power consumed */
#define	PCI_PMCSR_DSEL_D2_PWR_C		0x0400	/* D2 power consumed */
#define	PCI_PMCSR_DSEL_D3_PWR_C		0x0600	/* D3 power consumed */
#define	PCI_PMCSR_DSEL_D0_PWR_D		0x0800	/* D0 power dissipated */
#define	PCI_PMCSR_DSEL_D1_PWR_D		0x0a00	/* D1 power dissipated */
#define	PCI_PMCSR_DSEL_D2_PWR_D		0x0c00	/* D2 power dissipated */
#define	PCI_PMCSR_DSEL_D3_PWR_D		0x0e00	/* D3 power dissipated */
#define	PCI_PMCSR_DSEL_COM_C		0x1000	/* common power consumption */
#define	PCI_PMCSR_DSEL_MASK		0x1e00	/* data select mask */
#define	PCI_PMCSR_DSCL_UNKNOWN		0x0000	/* data scale unknown */
#define	PCI_PMCSR_DSCL_1_BY_10		0x2000	/* data scale 0.1x */
#define	PCI_PMCSR_DSCL_1_BY_100		0x4000	/* data scale 0.01x */
#define	PCI_PMCSR_DSCL_1_BY_1000	0x6000	/* data scale 0.001x */
#define	PCI_PMCSR_DSCL_MASK		0x6000	/* data scale mask */
#define	PCI_PMCSR_PME_STAT		0x8000	/* PME status */

/*
 * PM PMCSR PCI to PCI bridge support extension values - 1 byte
 */
#define	PCI_PMCSR_BSE_B2_B3	0x40	/* bridge D3hot -> secondary B2 */
#define	PCI_PMCSR_BSE_BPCC_EN	0x80	/* bus power/clock control enabled */

/*
 * other interesting PCI constants
 */
#define	PCI_BASE_NUM	6	/* num of base regs in configuration header */
#define	PCI_BASE_SIZE	4	/* size of base reg in bytes */
#define	PCI_CONF_HDR_SIZE	256	/* configuration header size */
#define	PCI_CLK_33MHZ	(33 * 1024 * 1024)	/* 33MHz clock speed */
#define	PCI_CLK_66MHZ	(66 * 1024 * 1024)	/* 33MHz clock speed */

/*
 * This structure represents one entry of the 1275 "reg" property and
 * "assigned-addresses" property for a PCI node.  For the "reg" property, it
 * may be one of an arbitrary length array for devices with multiple address
 * windows.  For the "assigned-addresses" property, it denotes an assigned
 * physical address on the PCI bus.  It may be one entry of the six entries
 * for devices with multiple base registers.
 *
 * The physical address format is:
 *
 *             Bit#:  33222222 22221111 11111100 00000000
 *                    10987654 32109876 54321098 76543210
 *
 * pci_phys_hi cell:  np0000tt bbbbbbbb dddddfff rrrrrrrr
 * pci_phys_mid cell: hhhhhhhh hhhhhhhh hhhhhhhh hhhhhhhh
 * pci_phys_low cell: llllllll llllllll llllllll llllllll
 *
 * n          is 0 if the address is relocatable, 1 otherwise
 * p          is 1 if the addressable region is "prefetchable", 0 otherwise
 * t          is 1 if the address range is aliased
 * tt         is the type code, denoting which address space
 * bbbbbbbb   is the 8-bit bus number
 * ddddd      is the 5-bit device number
 * fff        is the 3-bit function number
 * rrrrrrrr   is the 8-bit register number
 * hh...hhh   is the 32-bit unsigned number
 * ll...lll   is the 32-bit unsigned number
 *
 * The physical size format is:
 *
 * pci_size_hi cell:  hhhhhhhh hhhhhhhh hhhhhhhh hhhhhhhh
 * pci_size_low cell: llllllll llllllll llllllll llllllll
 *
 * hh...hhh   is the 32-bit unsigned number
 * ll...lll   is the 32-bit unsigned number
 */
struct pci_phys_spec {
	uint_t pci_phys_hi;		/* child's address, hi word */
	uint_t pci_phys_mid;		/* child's address, middle word */
	uint_t pci_phys_low;		/* child's address, low word */
	uint_t pci_size_hi;		/* high word of size field */
	uint_t pci_size_low;		/* low word of size field */
};

typedef struct pci_phys_spec pci_regspec_t;

/*
 * PCI masks for pci_phy_hi of PCI 1275 address cell.
 */
#define	PCI_REG_REG_M		0x000000ff	/* register mask */
#define	PCI_REG_FUNC_M		0x00000700	/* function mask */
#define	PCI_REG_DEV_M		0x0000f800	/* device mask */
#define	PCI_REG_BUS_M		0x00ff0000	/* bus number mask */
#define	PCI_REG_ADDR_M		0x03000000	/* address space mask */
#define	PCI_REG_ALIAS_M		0x20000000	/* aliased bit mask */
#define	PCI_REG_PF_M		0x40000000	/* prefetch bit mask */
#define	PCI_REG_REL_M		0x80000000	/* relocation bit mask */

#define	PCI_REG_REG_G(x)	((x) & PCI_REG_REG_M)
#define	PCI_REG_FUNC_G(x)	(((x) & PCI_REG_FUNC_M) >> 8)
#define	PCI_REG_DEV_G(x)	(((x) & PCI_REG_DEV_M) >> 11)
#define	PCI_REG_BUS_G(x)	(((x) & PCI_REG_BUS_M) >> 16)
#define	PCI_REG_ADDR_G(x)	(((x) & PCI_REG_ADDR_M) >> 24)

/*
 * PCI bit encodings of pci_phys_hi of PCI 1275 address cell.
 */
#define	PCI_ADDR_MASK		PCI_REG_ADDR_M
#define	PCI_ADDR_CONFIG		0x00000000	/* configuration address */
#define	PCI_ADDR_IO		0x01000000	/* I/O address */
#define	PCI_ADDR_MEM32		0x02000000	/* 32-bit memory address */
#define	PCI_ADDR_MEM64		0x03000000	/* 64-bit memory address */
#define	PCI_ALIAS_B		PCI_REG_ALIAS_M	/* aliased bit */
#define	PCI_PREFETCH_B		PCI_REG_PF_M	/* prefetch bit */
#define	PCI_RELOCAT_B		PCI_REG_REL_M	/* non-relocatable bit */
#define	PCI_CONF_ADDR_MASK	0x00ffffff	/* mask for config address */

#define	PCI_HARDDEC_8514 2	/* number of reg entries for 8514 hard-decode */
#define	PCI_HARDDEC_VGA	3	/* number of reg entries for VGA hard-decode */
#define	PCI_HARDDEC_IDE	4	/* number of reg entries for IDE hard-decode */
#define	PCI_HARDDEC_IDE_PRI 2	/* number of reg entries for IDE primary */
#define	PCI_HARDDEC_IDE_SEC 2	/* number of reg entries for IDE secondary */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_H */
