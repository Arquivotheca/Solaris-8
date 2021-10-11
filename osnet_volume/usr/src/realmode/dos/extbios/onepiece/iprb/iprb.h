/*
 * Copyright (c) 1995, 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)iprb.h	1.6	99/07/19 SMI"
/*
 * Hardware specific driver declarations for the Intel IPRB-Based Cards
 * driver conforming to the Generic LAN Driver model.
 */
#ifndef _IPRB_H
#define	_IPRB_H 1

/* debug flags */
#define	IPRBTRACE	0x01
#define	IPRBERRS	0x02
#define	IPRBRECV	0x04
#define	IPRBDDI		0x08
#define	IPRBSEND	0x10
#define	IPRBINT		0x20
#define	ETHERADDRL	6
#define	IPRB_NFRAMES	10
#define	IPRB_XMITS	5
#define	IPRB_VENID	0x8086
#define	IPRB_DEVID	0x1229

#ifdef DEBUG
#define	IPRBDEBUG 1
#endif

/* Misc */
#define	IPRBHIWAT	32768		/* driver flow control high water */
#define	IPRBLOWAT	4096		/* driver flow control low water */
#define	IPRBMAXPKT	1500		/* maximum media frame size */
#define	IPRB_FRAMESIZE	0x800		/* Allocation size of frame */
#define	IPRBMAXCMD	1600		/* Maximum size of any command */
#define	IPRBIDNUM	0		/* should be a unique id;zero works */

/* board state */
#define	IPRB_IDLE	0
#define	IPRB_WAITRCV	1          
#define	IPRB_XMTBUSY	2
#define	IPRB_ERROR	3

#define	IPRB_MAXFRAMES		300	/* Maximum number of receive frames */
#define	IPRB_MAXMITS		100	/* Maximum number of command buffers */
#define	IPRB_DEFAULT_FRAMES	30	/* Default receives if no driver.conf */
#define	IPRB_DEFAULT_XMITS	30	/* Default commands if no driver.conf */

#pragma pack(1)

/* A generic template for all D100 commands */

struct iprb_generic_cmd
{
	short cmd_bits;
	short cmd_cmd;
	paddr_t cmd_next;
	char data[IPRBMAXCMD];
};

/* A D100 individual address setup command */

struct iprb_ias_cmd
{
	short ias_bits;
	short ias_cmd;
	paddr_t ias_next;
	char addr[ETHERADDRL];
};

/* A D100 configure command */

struct iprb_cfg_cmd
{
	short cfg_bits;
	short cfg_cmd;
	paddr_t cfg_next;
	unsigned char cfg_byte0;
	unsigned char cfg_byte1;
	unsigned char cfg_byte2;
	unsigned char cfg_byte3;
	unsigned char cfg_byte4;
	unsigned char cfg_byte5;
	unsigned char cfg_byte6;
	unsigned char cfg_byte7;
	unsigned char cfg_byte8;
	unsigned char cfg_byte9;
	unsigned char cfg_byte10;
	unsigned char cfg_byte11;
	unsigned char cfg_byte12;
	unsigned char cfg_byte13;
	unsigned char cfg_byte14;
	unsigned char cfg_byte15;
	unsigned char cfg_byte16;
	unsigned char cfg_byte17;
	unsigned char cfg_byte18;
	unsigned char cfg_byte19;
	unsigned char cfg_byte20;
	unsigned char cfg_byte21;
	unsigned char cfg_byte22;
	unsigned char cfg_byte23;
};

#define	IPRB_MII 0
#define	IPRB_503 1

/* Configuration bytes (from Intel) */

#define	IPRB_CFG_B0 0x16
#define	IPRB_CFG_B1 0x88
#define	IPRB_CFG_B2 0
#define	IPRB_CFG_B3 0
#define	IPRB_CFG_B4 0
#define	IPRB_CFG_B5 0
#define	IPRB_CFG_B6 0x3a
#define	IPRB_CFG_B6PROM 0x80
#define	IPRB_CFG_B7 3
#define	IPRB_CFG_B7PROM 0x2
#define	IPRB_CFG_B7NOPROM 0x3
#define	IPRB_CFG_B8_MII 1
#define	IPRB_CFG_B8_503 0
#define	IPRB_CFG_B9 0
#define	IPRB_CFG_B10 0x2e
#define	IPRB_CFG_B11 0
#define	IPRB_CFG_B12 0x60
#define	IPRB_CFG_B13 0
#define	IPRB_CFG_B14 0xf2
#define	IPRB_CFG_B15 0xc8
#define	IPRB_CFG_B16 0
#define	IPRB_CFG_B17 0x40
#define	IPRB_CFG_B18 0xf2
#define	IPRB_CFG_B19 0x80
#define	IPRB_CFG_B20 0x3f
#define	IPRB_CFG_B21 5
#define	IPRB_CFG_B22 0
#define	IPRB_CFG_B23 0

#define	IPRB_MAXMCS 384
#define	IPRB_MAXMCSN 64

/* A D100 transmit command */

struct iprb_xmit_cmd
{
	short xmit_bits;
	short xmit_cmd;
	paddr_t xmit_next;
	paddr_t xmit_tbd;
	short xmit_count;
	short xmit_tbd_number;
	char xmit_data[IPRB_FRAMESIZE];
};

/* A D100 RFD */

struct iprb_rfd
{
	ulong rfd_bits;
	paddr_t rfd_next;
	paddr_t rfd_rbd;
	short rfd_count;
	short rfd_size;
	char rfd_data[IPRB_FRAMESIZE];
};
#pragma pack()

#define	IPRB_SCB_STATUS 0
#define	IPRB_SCB_CMD 2
#define	IPRB_SCB_PTR 4
#define	IPRB_SCB_PORT 8
#define	IPRB_SCB_FLSHCTL 0xc
#define	IPRB_SCB_EECTL 0xe
#define	IPRB_SCB_MDI 0x10
#define	IPRB_SCB_ERCV 0x14

#define	IPRB_MDI_READ 2
#define	IPRB_MDI_READY 0x10000000L
#define	IPRB_MDI_CREG 0
#define	IPRB_MDI_SREG 1

#define	IPRB_LOAD_RUBASE 6
#define	IPRB_CU_START 0x10
#define	IPRB_CU_RESUME 0x20
#define	IPRB_CU_DUMPSTAT 0x50
#define	IPRB_LOAD_CUBASE 0x60
#define	IPRB_RU_START 1

#define	IPRB_PORT_SW_RESET 0
#define	IPRB_PORT_SELF_TEST 1
#define	IPRB_PORT_DUMP 3
#define	PCI_CONF_COMM 0x4
#define	PCI_CONF_ILINE 0x3c
#define	PCI_CONF_BASE1 0x14

#define	IPRB_STAT_COMPLETE 0xa005

#define	IPRB_XMITSIZE 0x800

#define	IPRB_RFD_COMPLETE 0x8000
#define	IPRB_RFD_EL 0x80000000
#define	IPRB_RFD_COUNT_MASK 0x3fff

#define	IPRB_CMD_EL 0x8000;

#define	IPRB_XMIT_SUSPEND 0x4000
#define	IPRB_XMIT_INTR 0x2000
#define	IPRB_XMIT_EOF 0x8000
#define	IPRB_XMIT_CMD 4
#define	IPRB_CMD_INTR 0x2000

#define	IPRB_IAS_CMD 1
#define	IPRB_CFG_CMD 2
#define	IPRB_MCS_CMD 3

#define	IPRB_CMD_COMPLETE 0x8000

#define	IPRB_SCB_INTR_MASK 0xff00
#define	IPRB_INTR_FR 0x4000
#define	IPRB_INTR_CNACI 0x2000
#define	IPRB_INTR_RNR 0x1000
#define	IPRB_INTR_MDI 0x800
#define	IPRB_INTR_SWI 0x400

#define	IPRB_EEDI 0x04
#define	IPRB_EEDO 0x08
#define	IPRB_EECS 0x02
#define	IPRB_EESK 0x01
#define	IPRB_EEPROM_READ 0x06

#define	IPRB_STATSIZE 68

#define	CMD_BUS_MASTER 2
#define	IPRB_COMMAND_REGISTER 4

//#define	IPRB_SCBWAIT(iprbp) \
//    while ((inb(iprbp->iprb_ioaddr + IPRB_SCB_CMD)) & 0xff);
//while ((inb(iprb_ioaddr + IPRB_SCB_CMD)) & 0xff);

// while (!(ias->ias_bits & IPRB_CMD_COMPLETE)); IPRB_IASCMDCMPL...
// while (!(cfg->cfg_bits & IPRB_CMD_COMPLETE)); IPRB_CFGCMDCMPL...
/*
 * PCI Constants
 */
#define	PCI_COOKIE_TO_BUS(x)	((unchar)(((x)&0xff00)>>8))
#define	PCI_COOKIE_TO_DEV(x)	((unchar)(((x)&0x00f8)>>3))
#define	PCI_COOKIE_TO_FUNC(x)	((unchar)((x)&0x0007))

#define	FALSE 0
#define	TRUE 1
#endif

/* Vinay added this to properly calculate
 * eeprom size changes calculations for
 * address length.
 * 06/28/1999
 */
#define IPRB_EEPROM_ADDRESS_SIZE(size)  size == 64  ? 6 :               \
                                       (size == 128 ? 7 :               \
                                       (size == 256 ? 8 : 6 ))
                                                              
#define IPRB_SCBWAIT()	{ 																	\
        					register int ntries = 10000;                    				\
                        	do { 	if (((inb(iprb_ioaddr + IPRB_SCB_CMD)) & 0xff) == 0)  	\
                             			break;                                          	\
                					drv_usecwait(10);                                       \
        					} while (--ntries > 0);                                         \
        					if (ntries == 0)                                                \
                				printf("iprb: device never responded!\n");    				\
						}	

#define IPRB_IASCMDCMPL(ias){ 																\
        						register int ntries = 10000;                    			\
                        		do { 	if (ias->ias_bits & IPRB_CMD_COMPLETE)  			\
                             				break;                                          \
                						drv_usecwait(10);                                   \
        						} while (--ntries > 0);                                     \
        						if (ntries == 0)                                            \
                					printf("iprb: IAS Command did not finish...!\n");    	\
							}	
#define IPRB_CFGCMDCMPL(cfg){ 																\
        						register int ntries = 10000;                    			\
                        		do { 	if (cfg->cfg_bits & IPRB_CMD_COMPLETE)  			\
                             				break;                                          \
                						drv_usecwait(10);                                   \
        						} while (--ntries > 0);                                     \
        						if (ntries == 0)                                            \
                					printf("iprb: CFG Command did not finish...!\n");    	\
							}	
