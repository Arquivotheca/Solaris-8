/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DKTP_MLX_MLX_H
#define	_SYS_DKTP_MLX_MLX_H

#pragma ident	"@(#)mlx.h	1.25	99/08/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Mylex DAC960 Host Adapter Driver Header File.  Driver private
 * interfaces, common between all the SCSI and non-SCSI instances.
 */

#if defined(ppc)
#define	static				/* vla fornow */
#define	printf	prom_printf		/* vla fornow */
#endif

#include <sys/types.h>
#include <sys/ddidmareq.h>
#include <sys/modctl.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>

#include <sys/dktp/hba.h>
#include <sys/scsi/scsi.h>
#include <sys/scsi/impl/transport.h>

#include <sys/dktp/objmgr.h>
#include <sys/dktp/dadkio.h>
#include <sys/dktp/controller.h>
#include <sys/dktp/cmpkt.h>
#include <sys/dktp/gda.h>
#include <sys/dktp/tgdk.h>
#include <sys/file.h>

#include <sys/dktp/mscsi.h>

#include <sys/pci.h>

#ifdef	PCI_DDI_EMULATION
#define	ddi_io_getb(a, b)	inb(b)
#define	ddi_io_getw(a, b)	inw(b)
#define	ddi_io_getl(a, b)	inl(b)
#define	ddi_io_putb(a, b, c)	outb(b, c)
#define	ddi_io_putw(a, b, c)	outw(b, c)
#define	ddi_io_putl(a, b, c)	outl(b, c)
#endif

#ifdef _PRE_SOLARIS_2_6
typedef unsigned short uint16_t;
#define	DDI_PUT8 ddi_putb
#define	DDI_PUT16 ddi_putw
#define	DDI_PUT32 ddi_putl
#define	DDI_GET8 ddi_getb
#define	DDI_GET16 ddi_getw
#define	DDI_GET32 ddi_getl
#else
#define	DDI_PUT8 ddi_put8
#define	DDI_PUT16 ddi_put16
#define	DDI_PUT32 ddi_put32
#define	DDI_GET8 ddi_get8
#define	DDI_GET16 ddi_get16
#define	DDI_GET32 ddi_get32
#endif

typedef	unsigned char bool_t;
typedef	ulong_t	ioadr_t;

#include <sys/dktp/mlx/mlx_dac.h>
#include <sys/dktp/mlx/mlx_scsi.h>
#include <sys/dktp/mlx/mlx_raid.h>

#define	MLX_PCI_RNUMBER			1	/* 2nd entry in "reg" array */
#define	MLX_EISA_RNUMBER		0

/* PCI V30x IO Address offsets */
#define	MLX_PCI_IO_LOCAL_DBELL		0x40   	/* Local Doorbell reg	*/
#define	MLX_PCI_IO_SYS_DBELL 		0x41   	/* System Doorbell reg 	*/
#define	MLX_PCI_IO_INTR			0x43	/* intr enable/disable reg */
#define	MLX_IO_ENABLE_INTS		1
#define	MLX_IO_DISABLE_INTS		0
/* PCI V40X Memory mapped address offsets */
#define	MLX_PCI_MEM_LOCAL_DBELL		0x20   	/* Local Doorbell reg	*/
#define	MLX_PCI_MEM_SYS_DBELL 		0x2c   	/* System Doorbell reg 	*/
#define	MLX_PCI_MEM_INTR		0x34	/* intr enable/disable reg */
#define	MLX_MEM_ENABLE_INTS		0xfb
#define	MLX_MEM_DISABLE_INTS		0xff


/* EISA ID Address offsets */
#define	MLX_EISA_CONFIG0		0xC80	/* For EISA ID 0 */
#define	MLX_EISA_CONFIG1		0xC81	/* For EISA ID 1 */
#define	MLX_EISA_CONFIG2		0xC82	/* For EISA ID 2 */
#define	MLX_EISA_CONFIG3		0xC83	/* For EISA ID 3 */
#define	MLX_EISA_LOCAL_DBELL		0xC8D   /* Local Doorbell register */
#define	MLX_EISA_SYS_DBELL 		0xC8F   /* System Doorbell register */

/* EISA IDs for Different models */
#define	MLX_EISA_ID0		0x35	/* EISA ID 0 */
#define	MLX_EISA_ID1		0x98	/* EISA ID 1 */
#define	MLX_EISA_ID2		0x00	/* EISA ID 2 */
#define	MLX_EISA_ID3		0x70	/* EISA ID 3 */
#define	MLX_EISA_ID3_MASK	0xF0	/* For any DAC960 */

/* To Get the Configured IRQ */
#define	MLX_IRQCONFIG	0xCC3	 /* offset from Base */
#define	MLX_IRQMASK	0x60
#define	MLX_IRQ15	15
#define	MLX_IRQ14	14
#define	MLX_IRQ12	12
#define	MLX_IRQ11	11
#define	MLX_IRQ10	10

/* Offsets for status information */
#define	MLX_NSG  	0xC9C	/* # of s/g elements  */
#define	MLX_STAT_ID	0xC9D	/* Command Identifier passed */
#define	MLX_STATUS	0xC9E	/* Status(LSB) for completed command */
#define	MLX_INTR_DEF1	0xC8E	/* enable/disable register */
#define	MLX_INTR_DEF2	0xC89	/* enable/disable register */
#define	MLX_MBXOFFSET	0xC90   /* offset from base */

/* Offset of command/status registers from base addr */
#define	MLX_IO_CMDREG_OFFSET	0	/* Offset from base, I/O mapped cards */
#define	MLX_IO_CMDIDREG_OFFSET	0	/* Offset from base, I/O mapped cards */
#define	MLX_IO_STATREG_OFFSET	0	/* Offset from base, I/O mapped cards */
#define	MLX_MEM_CMDREG_OFFSET	0x1000	/* Offset from base, mem mapped cards */
#define	MLX_MEM_CMDIDREG_OFFSET	0x100b	/* Offset from base, mem mapped cards */
#define	MLX_MEM_STATREG_OFFSET	0x100c	/* Offset from base, mem mapped cards */

/* PCI Command/status mailbox register offsets */
#define	MLX_MBX0	0x00	/* Command Code */
#define	MLX_MBX1	0x01	/* Command ID */
#define	MLX_MBX2	0x02	/* Block count,Chn,Testno, bytecount */
#define	MLX_MBX3	0x03	/* Tgt,Pas,bytecount	*/
#define	MLX_MBX4	0x04	/* State,Chn	*/
#define	MLX_MBX5	0x05
#define	MLX_MBX6	0x06
#define	MLX_MBX7	0x07	/* Drive */
#define	MLX_MBX8	0x08	/* Paddr */
#define	MLX_MBX9	0x09	/* Paddr */
#define	MLX_MBXA	0x0A	/* Paddr */
#define	MLX_MBXB	0x0B	/* Paddr */
#define	MLX_MBXC	0x0C	/* Scatter-Gather type	*/
#define	MLX_MBXD	0x0D	/* Command ID Passed in MLX_MBX1 */
#define	MLX_MBXE	0x0E	/* Status */
#define	MLX_MBXF	0x0F	/* Status */

#define	MLX_CMBXFREE	0x0	/* MLX Command mail box is free */
#define	MLX_STATREADY	0x1	/* MLX Command Status ready */
#define	MLX_NEWCMD	0x1	/* Put in EISA Local Doorbell for new cmd */
#define	MLX_STATCMP	0x2	/* Set LDBELL for status completion */
#define	MLX_GOTSTATUS	0x1	/* DAC960P Indicate that driver read status */

#define	MLX_MAX_RETRY		300000
#define	MLX_BLK_SIZE		512
#define	MLX_BLK_SHIFT		9
#define	MLX_MAX_NSG_V123	17	/* max number of s/g elements < V4.00 */
#define	MLX_MAX_NSG		33	/* max number of s/g elements >=V4.00 */
#define	MLX_MAXCMDS		64
#define	MLX_CARD_SCSI_ID	7	/* never changes regardless of slot */

/* Status or Error Codes */
#define	MLX_SUCCESS		0x00	/* Normal Completion */
#define	MLX_E_UNREC		0x01	/* Unrecoverable data error */
#define	MLX_E_NODRV		0x02	/* System Drive does not exist */
#define	MLX_E_RBLDONLINE	0x02	/* Attempt to rebuild online drive */
#define	MLX_E_DISKDEAD		0x02	/* SCSI Disk on Sys Drive is dead */
#define	MLX_E_BADBLK		0x03	/* Some Bad Blks Found */
#define	MLX_E_RBLDFAIL		0x04	/* New Disk Failed During Rebuild */
#define	MLX_E_SELTIMEOUT	0x0E	/* Scsi Device Timeout */
#define	MLX_E_RESET		0x0F	/* Disk Device Reset */
#define	MLX_E_NDEVATAD		0x102	/* No Device At Address Specified */
#define	MLX_E_INVALSDRV		0x105	/* A RAID 0 Drive */
#define	MLX_E_LIMIT		0x105	/* Attempt to read beyond limit */
#define	MLX_E_INVALCHN		0x105	/* Invalid Address (Channel) */
#define	MLX_E_INVALTGT		0x105	/* Invalid Address (Target) */
#define	MLX_E_NEDRV		0x105	/* Non Existent System Drive */
#define	MLX_E_NORBLDCHK		0x105	/* No REBUILD/CHECK in progress */
#define	MLX_E_CHN_BUSY		0x106 	/* Channel Busy */
#define	MLX_E_INPROGRES		0x106	/* Rebuild/Check is in progress */

#define	MLX_INV_STATUS		0xa5a5	/* pre-init values for status	*/

#pragma	pack(1)

/* Scatter-Gather format for Scatter-Gather write-read commands */
#define	MLX_SGTYPE0	0x0	/* 32 bit addr and count */
#define	MLX_SGTYPE1	0x1	/* 32 bit addr and 16 bit count */
#define	MLX_SGTYPE2	0x2	/* 32 bit count and 32 bit addr */
#define	MLX_SGTYPE3	0x3	/* 16 bit count and 32 bit addr */

typedef struct mlx_sg_element {
	union {
		struct {
			ulong_t	data01_ptr32;	/* 32 bit data pointer */
			ulong_t	data02_len32;	/* 32 bit data length  */
		} type0;
		struct {
			ulong_t	data11_ptr32;	/* 32 bit data pointer */
			ushort_t data12_len16;	/* 16 bit data length  */
		} type1;
		struct {
			ulong_t	data21_len32;	/* 32 bit data length  */
			ulong_t	data22_ptr32;	/* 32 bit data pointer */
		} type2;
		struct {
			ushort_t  data31_len32;	/* 32 bit data length  */
			ulong_t   data32_ptr16;	/* 16 bit data pointer */
		} type3;
	} fmt;
} mlx_sg_element_t;

#define	data01_ptr32	fmt.type0.data01_ptr32
#define	data02_len32	fmt.type0.data02_len32

#define	data11_ptr32	fmt.type1.data11_ptr32
#define	data12_len16	fmt.type1.data12_len16

#define	data21_len32	fmt.type2.data21_len32
#define	data22_ptr32	fmt.type2.data22_ptr32

#define	data31_len32	fmt.type3.data31_len32
#define	data32_ptr16	fmt.type3.data32_ptr16

/* Status Block */
#define	MLX_BAD_OPCODE	0x104
typedef struct mlx_stat	{
	volatile uchar_t stat_id;	/* MLX_MBXD */
	volatile ushort_t status;	/* MLX_MBXE MLX_MBXF, 0 == success */
} mlx_stat_t;

/* Command Block */
typedef struct mlx_cmd {
	uchar_t opcode;		/* MLX_MBX0 or MLX_MBXCMD   */
	uchar_t cmdid;		/* MLX_MBX1 or MLX_MBXCMDID */

	union {
		struct {
			uchar_t arr[0xB]; /* MLX_MBX2-C */
		} type0;
		struct {
			uchar_t cnt;	/* MLX_MBX2 */
			uchar_t blk[4];	/* MLX_MBX3-6 */
			uchar_t drv;	/* MLX_MBX7 */
			ulong_t ptr;	/* MLX_MBX8-B */
			uchar_t sg_type; /* MLX_MBXC */
		} type1;
		struct {
			uchar_t chn;	/* MLX_MBX2 */
			uchar_t tgt;	/* MLX_MBX3 */
			uchar_t state;	/* MLX_MBX4 */
			uchar_t fill[3]; /* MLX_MBX4-7 */
			ulong_t ptr;	/* MLX_MBX8-B */
			uchar_t fill1;	/* MLX_MBXC */
		} type2;
		struct {
			uchar_t fill[6]; /* MLX_MBX2-7 */
			ulong_t ptr;	/* MLX_MBX8-B */
			uchar_t fill1;	/* MLX_MBXC */
		} type3;
		struct {
			uchar_t test;	/* MLX_MBX2 */
			uchar_t pass;	/* MLX_MBX3 */
			uchar_t chn;	/* MLX_MBX4 */
			uchar_t fill[3]; /* MLX_MBX5-7 */
			ulong_t ptr;	/* MLX_MBX8-B */
			uchar_t fill1;	/* MLX_MBXC */
		} type4;
		struct {
			uchar_t param;	/* MLX_MBX2 */
			uchar_t fill[5]; /* MLX_MBX3-7 */
			ulong_t ptr;	/* MLX_MBX8-B */
			uchar_t fill1;	/* MLX_MBXC */
		} type5;
		struct {
			ushort_t cnt;	/* MLX_MBX2-3 */
			ulong_t off;	/* MLX_MBX4-7 */
			ulong_t ptr;	/* MLX_MBX8-B */
			uchar_t fill;	/* MLX_MBXC */
		} type6;
		struct {	/* Extended Type 1 command */
			uchar_t sg_type; /* MLX_MBX2 */
			uchar_t rsvd;	/* MLX_MBX3 */
			ulong_t blk;	/* MLX_MBX4-7 */
			ulong_t ptr;	/* MLX_MBX8-B */
			uchar_t drv;	/* MLX_MBXC */
		} type11;
		struct {	/* Extended Type 1 command */
			uchar_t lcnt;	/* MLX_MBX2 */
			uchar_t hcnt:3,	/* MLX_MBX3 */
				drv:5;
			ulong_t blk;	/* MLX_MBX4-7 */
			ulong_t ptr;	/* MLX_MBX8-B */
			uchar_t sg_type; /* MLX_MBXC */
		} type21;
	} fmt;
	mlx_stat_t hw_stat;	/* MLX_MBXD-F */
} mlx_cmd_t;

#pragma	pack()

/* Command Control Block.  One per SCSI or Mylex specific command. */
#define	MLX_INVALID_CMDID	0xFF

typedef struct mlx_ccb {
	mlx_cmd_t cmd;			/* physical mlx command */

	uchar_t	intr_wanted;
	uchar_t  type;
	paddr_t paddr;			/* paddr of this mlx_ccb_t */
	long bytexfer;			/* xfer size requested */

	union {
		struct {
			mlx_dacioc_args_t da;
			ksema_t da_sema;
		} dacioc_args;
		struct {
			mlx_cdbt_t *cdbt;
			struct scsi_arq_status arq_stat;
		} scsi_args;	/* Direct CDB w/ or w/o scatter-gather */
	} args;

	union {
		struct { /* types 1 and 3 are capable of Scatter-Gather xfer */
			mlx_sg_element_t list[MLX_MAX_NSG];
			uchar_t type;				/* MLX_MBXC */
		} sg;
		struct { /* types 1 and 3 are capable of Scatter-Gather xfer */
			ushort_t cnt;
			uchar_t resvd[6];
			mlx_sg_element_t list[MLX_MAX_NSG];
			uchar_t type;				/* MLX_MBXC */
		} esg;
		struct {
			ushort_t ubuf_len;	/* expected correct ubuf_len */
			uchar_t flags;				/* see below */
		} ioc;
	} si;

	union {
		char *scratch;  	/* spare buffer space		*/
		struct  scsi_cmd *ownerp; /* owner pointer- to 	*/
					/* point back to the packet	*/
	} cc;
} mlx_ccb_t;

#define	ccb_opcode		cmd.opcode
#define	ccb_cmdid		cmd.cmdid
#define	ccb_drv			cmd.fmt.type1.drv
#define	ccbe_drv		cmd.fmt.type11.drv
#define	ccbo_drv		cmd.fmt.type21.drv
#define	ccb_blk			cmd.fmt.type1.blk
#define	ccbe_blk		cmd.fmt.type11.blk
#define	ccbo_blk		cmd.fmt.type21.blk

#define	CCB_BLK(ccbp, v)	ccbp->ccb_blk[1] = (uchar_t)(v);	\
				ccbp->ccb_blk[2] = (uchar_t)((v)>>8);	\
				ccbp->ccb_blk[3] = (uchar_t)((v)>>16);	\
				ccbp->ccb_blk[0] = (uchar_t)((v)>>18) & 0xC0;

#define	ccb_arr			cmd.fmt.type0.arr
#define	ccb_cnt			cmd.fmt.type1.cnt
#define	ccbo_lcnt		cmd.fmt.type21.lcnt
#define	ccbo_hcnt		cmd.fmt.type21.hcnt
#define	ccb_chn			cmd.fmt.type2.chn
#define	ccb_tgt			cmd.fmt.type2.tgt
#define	ccb_dev_state		cmd.fmt.type2.state
#define	ccb_test		cmd.fmt.type4.test
#define	ccb_pass		cmd.fmt.type4.pass
#define	ccb_chan   		cmd.fmt.type4.chn
#define	ccb_param		cmd.fmt.type5.param
#define	ccb_count		cmd.fmt.type6.cnt
#define	ccb_offset		cmd.fmt.type6.off
#define	ccb_xferpaddr		cmd.fmt.type1.ptr
#define	ccb_sg_type		cmd.fmt.type1.sg_type
#define	ccbe_sg_type		cmd.fmt.type11.sg_type

#define	ccb_hw_stat		cmd.hw_stat
#define	ccb_stat_id		cmd.hw_stat.stat_id
#define	ccb_status		cmd.hw_stat.status

#define	ccb_gen_args		args.dacioc_args.da.type_gen.gen_args
#define	ccb_gen_args_len	args.dacioc_args.da.type_gen.gen_args_len
#define	ccb_xferaddr_reg	args.dacioc_args.da.type_gen.xferaddr_reg
#define	ccb_da_sema		args.dacioc_args.da_sema
#define	ccb_cdbt		args.scsi_args.cdbt
#define	ccb_arq_stat		args.scsi_args.arq_stat
#define	ccb_sg_list		si.sg.list
#define	ccbe_sg_list		si.esg.list
#define	ccbe_sg_cnt		si.esg.cnt
#define	ccb_flags		si.ioc.flags
#define	ccb_ubuf_len		si.ioc.ubuf_len
#define	ccb_scratch		cc.scratch
#define	ccb_ownerp		cc.ownerp

/* Possible values for ccb_flags field of mlx_ccb_t */
#define	MLX_CCB_DACIOC_UBUF_TO_DAC	0x1 /* data xfer from user to DAC960 */
#define	MLX_CCB_DACIOC_DAC_TO_UBUF	0x2 /* data xfer from DAC960 to user */
#define	MLX_CCB_DACIOC_NO_DATA_XFER	0x4 /* no data xfer during dacioc op */
#define	MLX_CCB_UPDATE_CONF_ENQ		0x8 /* update mlx->conf and mlx->enq */
#define	MLX_CCB_GOT_DA_SEMA		0x10 /* ccb_da_sema initialized */

/* ccb stack element */
typedef struct mlx_ccb_stk {
	/*
	 * As -1 is an invalid index and used as an indicator, and to
	 * prevent future expansion problems, such as 255 max_cmd,
	 * type short is taken for the field next instread of char.
	 */
	short next;		/* next free index in the stack */
	mlx_ccb_t *ccb;
} mlx_ccb_stk_t;

/*
 * Per DAC960 card info shared by all its the channels.
 * The head of this linked list is pointed to by mlx_cards.
 */
typedef struct mlx {
	/*
	 * The following fields are initialized only once while being
	 * protected by a global lock, and read many times after that
	 * without any locks.
	 */
	struct mlxops *ops;	/* ptr to MLX SIOP ops table */
	unsigned long reg;	/* actual io address */
	int *regp;		/* copy of regs property */
	int reglen;		/* length of regs property */
	volatile uchar_t *membase; /* shared memory address */
	int rnum;		/* register number */
	int mrnum;		/* memory register number */

	ddi_acc_handle_t handle;	/* io access handle */
	ddi_acc_handle_t mhandle;	/* mem access handle */

	uchar_t initiatorid;	/* Always 7 but worth keeping this around */
	uchar_t nchn;		/* NCHN, number of channels */
	uchar_t max_tgt;	/* MAX_TGT, max # of targets PER channel */
	uchar_t max_cmd;	/* # of simultaneous cmds, including ncdb  */
	uint_t	max_blk;	/* maximum # blocks per command */
	enum {UNKNOWN, R1V22, R1V23, R1V5x, R3V0x, R4V0x} fw_version;

	uchar_t irq;		/* IRQ */
	uint_t intr_idx;	/* index of the interrupt vector */
	ddi_iblock_cookie_t iblock_cookie;
	dev_info_t *dip;	/* of the instance which cardinit'd */
	dev_info_t *idip;	/* of the instance which added intr handler */

	ulong_t	bar;		/* baseadd of card before ddi_regs_map_setup */
	ushort_t pci_local_dbell;	/* Offset of local doorbel register */
	ushort_t pci_sys_dbell;		/* Offset of system doorbel register */
	ushort_t pci_intr_mask;		/* Offset of interrupt mask register */
	uchar_t pci_enable_ints;	/* Value to enable interrupts */
	uchar_t pci_disable_ints;	/* Value to disable interrupts */
	ushort_t pci_mbx0;		/* Offsets of registers 0 -> f */
	ushort_t pci_mbx1;
	ushort_t pci_mbx2;
	ushort_t pci_mbx3;
	ushort_t pci_mbx4;
	ushort_t pci_mbx5;
	ushort_t pci_mbx6;
	ushort_t pci_mbx7;
	ushort_t pci_mbx8;
	ushort_t pci_mbx9;
	ushort_t pci_mbxa;
	ushort_t pci_mbxb;
	ushort_t pci_mbxc;
	ushort_t pci_mbxd;
	ushort_t pci_mbxe;
	ushort_t pci_mbxf;


	ksema_t scsi_ncdb_sema;	/* controls # of simultaneous DCDB's  */
	kmutex_t mutex;
	/*
	 * Access to the following need to be protected
	 *	- Only by mlx_global_mutex during attach or detach.
	 * 	- Only by the above mutex at run time.
	 */
	mlx_dac_conf_t *conf;
	uint_t	conf_size;
	mlx_dac_enquiry_t *enq;
	uint_t	enq_size;
	struct ENQUIRY2	*enq2;
	uint_t	enq2_size;

	mlx_ccb_stk_t *ccb_stk;		/* stack of max_cmd outstanding ccb's */
	mlx_ccb_stk_t *free_ccb;	/* head of free list in ccb_stk	*/
	ushort_t flags;			/* see below */
	uchar_t refcount;
	int attach_calls;
	int sgllen;			/* per unit sgllen */

	/* The following has to be protected only by the mlx_global_mutex. */
	struct mlx *next;
} mlx_t;

/* Possible values for flags field of mlx_t */
#define	MLX_CARD_CREATED	0x1
#define	MLX_GOT_ROM_CONF	0x2
#define	MLX_GOT_ENQUIRY		0x4
#define	MLX_CCB_STK_CREATED	0x8
#define	MLX_INTR_IDX_SET	0x10
#define	MLX_INTR_SET		0x20
#define	MLX_SUPPORTS_SG		0x80	/* f/w supports scatter-gather io */
#define	MLX_NO_HOT_PLUGGING	0x100
/*
 * Per channel(hba) info shared by all the units on the channel, plus one
 * extra (chn == 0) for all the System Drives.
 */
typedef struct mlx_hba {
	/*
	 * The following fields are initialized only once while being
	 * protected by a global lock, and read many times after that
	 * without any locks.
	 */
	dev_info_t *dip;
	uchar_t chn;			/* channel number */
	mlx_t *mlx;			/* back ptr to the card info */
	mlx_ccb_t *ccb;			/* used only during init */
	struct scsi_inquiry *scsi_inq;	/* NULL if System-Drive hba */
	uchar_t flags;			/* see below */
	ulong_t callback_id;	/* will be protected by framework locks */

	kmutex_t mutex;
	/*
	 * Access to the following need to be protected
	 *	- Only by mlx_global_mutex during attach or detach.
	 * 	- Only by the above mutex at run time.
	 */
	caddr_t pkt_pool;
	caddr_t ccb_pool;
	ushort_t refcount;	/* # of active children */
} mlx_hba_t;

/* Possible values for flags field of mlx_hba_t */
#define	MLX_HBA_DAC		1
#define	MLX_HBA_ATTACHED	2

/* Per SCSI unit or System-Drive */
typedef struct mlx_unit {
	scsi_hba_tran_t	*scsi_tran;
	mlx_dac_unit_t dac_unit;
	mlx_hba_t *hba;				/* back ptr to the hba info */
	uint_t capacity;			/* scsi capacity */
	ddi_dma_lim_t dma_lim;
	uchar_t	scsi_auto_req	: 1,		/* auto-request sense enable */
		scsi_tagq	: 1,		/* tagged queueing enable */
		reserved	: 6;
} mlx_unit_t;

/* Convenient macros. */
#define	MLX_DAC(hba)	((hba)->flags & MLX_HBA_DAC)
#define	MLX_SCSI(hba)	(!MLX_DAC(hba))
#define	MLX_OFFSET(basep, fieldp)    ((char *)(fieldp) - (char *)(basep))
#define	MLX_MIN(a, b)	((a) <= (b) ? (a) : (b))
#define	MLX_MAX(a, b)	((a) >= (b) ? (a) : (b))

#define	MLX_EISA(mlx)	(((mlx_t *)mlx)->ops == &dac960_nops)
#define	MLX_MC(mlx)	(((mlx_t *)mlx)->ops == &dmc960_nops)
#define	MLX_PCI(mlx)	(((mlx_t *)mlx)->ops == &dac960p_nops)

/*
 * 2.4 reg layout: In 2.4, the base_port/reg bustype variable
 * must represent all possible eisa, mc, and pci cards, and
 * all channels associated with each card.
 *
 * eisa/mc hwconf entries may overlap, because they will never
 * coexist. The layout is
 *
 *	0xS0CC
 *
 *	S = slot (0x0-F for eisa, 0x0-7 for mc)
 *	CC = channel (0xFF for raid channel, 0-FE for scsi channels)
 *
 * 2.4 pci entries must have distinct values because pci/eisa boards
 * may both appear. The MLX_PCI_BUS (0x800) value is used to uniqify
 * pci entries from eisa entries. This layout restricts the 2.4 pci
 * implementation to supporting a maximum of 15 channels, and 7 pci
 * busses. The layout is
 *
 *	0xBBDFCC
 *
 *	BB = pci bus
 *	D = pci device (bits 0-3 for pci device (plus high bit of pci function)
 *	F = pci function (bits 0-2 for pci function, high bit for pci device)
 *	CC = channel (0xFF for raid channel, 0-FE for scsi channel numbers)
 *
 * Note that the 2.4 driver cannot use the mscsi bus nexus driver until
 * the 2.4 realmode framework allows relative bootpaths to be passed,
 * and the 2.4 ufsboot constructs bootpaths based on this relative bootpath.
 *
 *
 * 2.5 reg layout: In 2.5, the base_port/reg bustype variable
 * must represent all possible eisa and mc cards.
 *
 * The layout is as above, except that a single entry in the hwconf
 * file is used to represent the system driver virtual channel of each
 * card.
 *
 * The channel number for scsi channels is set by the mscsi-bus
 * property from the child mscsi hba bus nexus driver.
 *
 * eisa/mc hwconf entries may overlap, because they will never
 * coexist. The layout is as above. Note that the raid system drive
 * channel MLX_DAC_CHN_NUM (0xff) is associated with the single entry
 * in the hwconf file per card.
 *
 * 2.5 pci entries will not appear in the hwconf file. The default
 * channel value for the devinfo nodes associated with the pci card
 * will be set to MLX_DAC_CHN_NUM.
 *
 * The channel number for scsi channels is set by the mscsi-bus
 * property from the child mscsi hba bus nexus driver.
 *
 */

#if defined(DEBUG)
#define	MLX_DEBUG 1
#endif /* DEBUG */

/* Card Types and Ids */
typedef	uchar_t	bus_t;
#define	BUS_TYPE_EISA		((bus_t)1)
#define	BUS_TYPE_MC		((bus_t)2)
#define	BUS_TYPE_PCI		((bus_t)3)

#define	DMC960_ID1		0x8F82	/* cheetah DMC960 card id. */
#define	DMC960_ID2		0x8FBB	/* passplay DMC960 card id. */

/* from dmc.h */

#define	DMC_IRQ_MASK		0xC0		/* 11000000 binary. */
#define	DMC_IRQ_MASK_SHIFT	6		/* Number of bits-to-shift */
#define	BIOS_BASE_ADDR_MASK	0x3C		/* 00111100 binary. */
#define	BBA_MASK_SHIFT		2		/* Number of bits-to-shift */
#define	IO_BASE_ADDR_MASK	0x38		/* 00111000 binary. */
#define	IOBA_MASK_SHIFT		3		/* Number of bits-to-shift */
/* dmc specific registers */

#define	CMD_PORT		0x00		/* Command Port. */
#define	ATTENTION_PORT		0x04		/* Attention Port. */
#define	SCP_REG			0x05		/* System Control Port. */
#define	ISP_REG			0x06		/* Interrupt Status Port. */
#define	CBSP_REG		0x07		/* Command Busy/Status Port. */
#define	DIIP_REG		0x08		/* Device Interrupt ID Port */

#define	DMC_RESET_ADAPTER	0x40	/* Reset DMC960 Adapter. */

#define	DMC_ENABLE_BUS_MASTERING 0x02	/* Enable Bus Mastering on DMC960. */
#define	DMC_ENABLE_INTRS	0x01	/* Enable (MIAMI) DMC960 intrs. */
#define	DMC_DISABLE_INTRS	0x00	/* Disable (MIAMI) DMC960 intrs. */
#define	DMC_CLR_ON_READ		0x40	/* Disable clear IV thru read. */

#define	DMC_INTR_VALID		0x02	/* interrupt valid (IV) bit	*/

#define	DMC_NEWCMD	0xD0	/* Put in ATTENTION port for new cmd.	*/
#define	DMC_ACKINTR	0xD1	/* "status-accepted" interrupt-ack	*/

#ifdef	DADKIO_RWCMD_READ
#define	RWCMDP	((struct dadkio_rwcmd *)(cmpkt->cp_bp->b_back))
#endif

/*
 * Handy constants
 */

/* For returns from xxxcap() functions */

#define	FALSE		0
#define	TRUE		1
#define	UNDEFINED	-1

/*
 * Handy macros
 */
#if defined(ppc)
#define	MLX_KVTOP(vaddr) \
		CPUPHYS_TO_IOPHYS(\
		((paddr_t)(hat_getkpfnum((caddr_t)(vaddr)) << (PAGESHIFT)) | \
			    ((paddr_t)(vaddr) & (PAGEOFFSET))))
#else
#define	MLX_KVTOP(vaddr)	(HBA_KVTOP((vaddr), mlx_pgshf, mlx_pgmsk))
#endif

/* Make certain the buffer doesn't cross a page boundary */
#define	PageAlignPtr(ptr, size)	\
	(caddr_t)(btop((unsigned)(ptr)) != btop((unsigned)(ptr) + (size)) ? \
	ptob(btop((unsigned)(ptr)) + 1) : (unsigned)(ptr))

/*
 * Debugging stuff
 */
#define	Byte0(x)		(x&0xff)
#define	Byte1(x)		((x>>8)&0xff)
#define	Byte2(x)		((x>>16)&0xff)
#define	Byte3(x)		((x>>24)&0xff)

/*
 * include all of the function prototype declarations
 */
#include <sys/dktp/mlx/mlxops.h>
#include <sys/dktp/mlx/mlxdefs.h>
#include <sys/dktp/mlx/debug.h>

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DKTP_MLX_MLX_H */
