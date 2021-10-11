/*
 * Copyright (c) 1995 Sun Microsystems, Inc. All rights reserved.
 */
 
#ident	"@(#)ieef.h	1.14	96/09/26 SMI\n"
 

/*
 *  SCB Structure - The SCB serves three purposes:
 *
 *  1. The Driver writes commands to the "command" shortword, telling the
 *     82596 which operation to perform.  The two most common commands
 *     are COMMAND UNIT START, and RECEIVE UNIT START.  
 *
 *  2. The 82596 returns chip status to the "status" shortword.
 *
 *  3. All errors are counted and stored in the SCB.
 *
 */

#define SUCCESS		(0)
#define FAILURE		(-1)
#define ETHERADDRL	(6)

#define	TRUE	1
#define FALSE	0
typedef struct scb {
	volatile short scb_status;   /* Status of 82596 */
	volatile short scb_command;  /* Which operation 82596 should perform */
	volatile long scb_cbl;	     /* Pointer to command block list */
	volatile long scb_rfa;	     /* Pointer to receive frame area */
	volatile long scb_crc;	     /* Number of crc errors */
	volatile long scb_aln;	     /* Number of alignment errors */
	volatile long scb_rsc;	     /* Number of resource errors */
	volatile long scb_ovr;	     /* Number of overrun errors */
	volatile long scb_cdt;	     /* Number of rcvcdt errors */
	volatile long scb_shf;	     /* Number of short frame errors */
	volatile long scb_timer;
} scb_t;

/* 
 *  ISCP Structure - This two word structure is only used to store the
 *  pointer to the SCB.  In this driver implementation, the SCB will always
 *  be located directly following the ISCP.
 */
typedef struct iscp {
	short iscp_busy;
	short iscp_zeros;
	long  iscp_scb;
} iscp_t;

/* 
 *  SCP Structure - This structure contains initialization information
 *  such as compatibility mode and interrupt polarity.  The address of
 *  the SCP (System Configuration Pointer) is set by using the PORT
 *  register on the 82596.
 */
typedef struct scp {
	short scp_zeros;
	short scp_sysbus;	/* Initialization data */
	short scp_unused[2];       
	long  scp_iscp;		/* Pointer to ISCP */
} scp_t;

/* 
 * This is a generic command block.  It is used to assure that enough
 * memory is allocated to hold any type of command block.  Much of the
 * time only a portion of this space will be used, and an overlay structure
 * will be referenced in order to fill-in the command-specific data
 * (such as a xmit_cmd structure)
 */
typedef struct gen_cmd {
	volatile long cmd_status;
	volatile long cmd_next;
	volatile char cmd_gen[72];
} gen_cmd_t;

/*
 * A Receive Buffer Descriptor describes a buffer that is available
 * for network data.
 */
typedef struct rbd {
	volatile long rbd_count;   /* Set by 82596 to # of bytes received */
	volatile long rbd_next;	   /* Set by the CPU */
	volatile long rbd_buffer;  /* Set by the CPU */ /* was paddr_t */
	volatile long rbd_size;	   /* Set by CPU, indicate the size of buffer */
	volatile char _far *rbd_v_buffer;
				   /* Buffer virt addr (not noticed by 596) */
} rbd_t;

/*
 * A Receive Frame Descriptor describes a "frame" of network data.
 * The RFA (Receive Frame Area) is made up of many RFDs and RBDs, and
 * one RFD can be associated with several RBDs.
 */
typedef struct rfd {
	volatile ushort rfd_status;		/* bits set by 82596 */
	volatile ushort rfd_ctlflags;	/* bits set by CPU */
	volatile long rfd_next;   /* Initially set by CPU, changed by 82596 */
	volatile long rfd_rbd;    /* Set by the 82596 (except head of list) */
	volatile long rfd_count;  /* Set by the 82596 to # of bytes received */
	volatile long data[5];    /* We don't use this in "flexible" format */
} rfd_t;

/* 
 * The Transmit Buffer Descriptor describes a buffer containing data to be
 * transmitted.  All of the values are set by the CPU.
 */
typedef struct tbd {
	volatile long tbd_size;
	volatile long tbd_next;
	volatile long tbd_buffer;	/* was paddr_t */
	volatile char _far *tbd_v_buffer;
} tbd_t;

/* 
 * This is a transmit command overlay to a generic command. 
 */
typedef struct xmit_cmd {
	volatile long xmit_status;	/* Contains results and parameters */
	volatile long xmit_next;	/* Link to the next cmd in the CBL */
	volatile long xmit_tbd;		/* Pointer to the TBD for this data */
	volatile long xmit_tcb_cnt;	/* Contains zero for flexible model */
	volatile char xmit_dest[6];
	volatile short xmit_length;
	volatile tbd_t _far *xmit_v_tbd;/* Virtual addr of tbd (for driver) */
} xmit_cmd_t;

/* 
 * Individual Address Setup command overlay.
 */
typedef struct ias_cmd {
	volatile long ias_status;	/* Same as */
	volatile long ias_next;		/* Above */
	volatile char ias_addr[6];	/* Ethernet Address */
		  /*   ushort unused; */
} ias_cmd_t;

/*
 * Configure command parameter structure
 */
typedef struct {
	ushort cnf_fifo_byte;	/* BYTE CNT, FIFO LIM (TX_FIFO) */
	ushort cnf_add_mode;	/* SRDY, SAV_BF, ADDR_LEN, AL_LOC, PREAM_LEN */
				/* INT_LPBCK, EXT_LPBK */
	ushort cnf_pri_data;	/* LIN_PRIO, ACR, BOF_MET,INTERFRAME_SPACING */
	ushort cnf_slot;	/* SLOT_TIME, RETRY NUMBER */
	ushort cnf_hrdwr;	/* PRM, BC_DIS, MANCH/NRZ,TONO_CRS, NCRC_INS */
				/* CRC, BT_STF,PAD,CRSF,CRS_SRC,CDTF,CDT_SRC */
	ushort cnf_min_len;	/* Min_FRM_LEN */
	ushort cnf_more;
} conf32_t;

/*
 * Configure command parameter structure
 */
typedef struct {
        ushort conf_bytes01;
        ushort conf_bytes23;
        ushort conf_bytes45;
        ushort conf_bytes67;
        ushort conf_bytes89;
        ushort conf_bytes1011;
        ushort conf_bytes1213;
        ushort conf_bytes1415;
        ushort conf_bytes1617;
        ushort conf_bytes1819;
} conf100_t;

/*
 * Configure command overlay
 */
typedef struct conf_cmd {
	volatile long conf_status;
	volatile long conf_next;
	volatile conf32_t conf_conf;
} conf32_cmd_t;

typedef struct conf100_cmd
{
    volatile long conf_status;
    volatile long conf_next;
    volatile conf100_t conf_conf;
} conf100_cmd_t;

/*
 * Multicast command overlay
 */
typedef struct mcs_cmd {
	volatile long mcs_status;
	volatile long mcs_next;
	volatile short mcs_count;
	volatile char mcs_addr[ETHERADDRL * 10];
} mcs_cmd_t;

/*
 * Dump command overlay
 */
typedef struct dump_cmd {
	volatile long dump_status;
	volatile long dump_next;
	volatile long dump_bufp;
} dump_cmd_t;


#define IEEF_NFRAMES	7		/* Max number of frames */
#define IEEF_NXMIT	1		/* Max number of xmit buffers */
#define IEEF_NCMDS	5		/* Max number of commands */
#define IEEF_XFRAMESIZE	1536		/* Max xmit frame size */
#define IEEF_MAXNVM_DATA (16 * 1024)

/*
 *  This structure describes the shared memory between the CPU and the
 *  82596.  One chunk of memory is allocated at initialization time, and
 *  the 82596 uses its bus mastering capabilities to share data with the
 *  CPU.
 */
struct ieef_shmem {
	volatile scp_t       ieef_scp;
	volatile iscp_t      ieef_iscp;
	volatile scb_t       ieef_scb;
	volatile gen_cmd_t   ieef_cmds[IEEF_NCMDS];
	volatile rfd_t       ieef_rfd[IEEF_NFRAMES];
	volatile rbd_t       ieef_rbd[IEEF_NFRAMES];
	volatile tbd_t       ieef_tbd[IEEF_NXMIT];
};

/* debug flags */
#define IEEFTRACE	0x01
#define IEEFERRS		0x02
#define IEEFRECV		0x04
#define IEEFDDI		0x08
#define IEEFSEND		0x10
#define IEEFINT		0x20

#ifdef DEBUG
#define IEEFDEBUG 1
#endif

/* Misc */
#define IEEFHIWAT	32768		/* driver flow control high water */
#define IEEFLOWAT	4096		/* driver flow control low water */
#define IEEFMAXPKT	1500		/* maximum media frame size */
#define IEEFIDNUM	0		/* should be a unique id; zero works */

/* board state */
#define IEEF_IDLE	0
#define IEEF_WAITRCV	1
#define IEEF_XMTBUSY	2
#define IEEF_ERROR	3

#define IEEF_NEWSCP 2
#define IEEF_RESET 0

#define IEEF_SHORT_ID 0xd425
#define IEEF_ID0 0xc80         
#define IEEF_ID1 0xc81         
#define IEEF_ID2 0xc82         
#define IEEF_ID3 0xc83         
#define IEEF_PC0 0xc88

#define IEEF_UNISYS_PORT 0x0
#define IEEF_UNISYS_CA 0x4
#define IEEF_UNISYS_NID 0x8

#define PLXE_PORT_OFFSET 0x0008                 /* Offset For PORT Command */
#define PLXE_CA_OFFSET 0x0000                   /* Offset For CA Command */
#define PLXE_REGISTER_1         0x0C89          /* Register 1 */
#define PLXE_ADDRESS_PROM       0x0C90          /* EISA Node Address Register */
#define PLXP_PORT_OFFSET 0x24                   /* Offset For PORT Command */
#define PLXP_CA_OFFSET 0x20                     /* Offset For CA Command */
#define PLXP_NODE_ADDR_REGISTER 0x40            /* PCI Node Address Register */
#define PLXP_INTERRUPT_CONTROL  0x00            /* PCI Intr Control Register */
#define PLXP_USER_PINS          0x04            /* User Pins Register */

#define FL32_EXTEND_INT_CONTROL 0x0430


struct ether_addr {
	unchar	addr[6];
};

/* driver specific declarations */
struct ieefinstance {
    ushort ieef_type;		  /* Type of 82596 hardware (see below) */
    ushort ieef_ioaddr;           /* Base IO Address of card */
    int ieef_port;		  /* port offset of card */
    int ieef_ca;		  /* channel attention offset of card */
    int ieef_nid;		  /* ethernet address offset of card */
    int ieef_int_control;	  /* Interrupt Control Register offset */
    int ieef_user_pins;		  /* User Pins Register offset */
    int ieef_speed;
    int ieef_num;
    ushort irq_level;             /* IRQ */
    struct ieef_shmem _far *kmem_map;  /* Ptr to virt. addr for control mem */
    long pmem_map;		  /* Absolute address of the above pointer */
    int  last_cb;                 /* Pointer to last command block */
    int  current_cb;              /* Pointer to current command block */
    int  current_frame;           /* Pointer to current receive frame */
    int  begin_frame;             /* Pointer to 1st frame after EL=1 */
    int  last_frame;              /* Pointer to frame with EL=1 */
    volatile rbd_t _far *end_rbd; /* Pointer to RBD with EOF=1 */
    int  mcs_count;               /* Counter for number of mcast addrs */
    int  ieef_framesize;          /* Size of our receive frames (from props) */
    int  ieef_nframes;            /* Number of frames (from props) */
    int  ieef_xbufsize;           /* Size of our xmit buffers (from props) */
    int  ieef_xmits;              /* Number of xmit buffers (from props) */
    char _far *rbufsv[IEEF_NFRAMES]; /* Keeps virtual addrs for rcv frames */
    long rbufsp[IEEF_NFRAMES];  /* Keeps physical addrs for rcv frames */
    char _far *xbufsv[IEEF_NXMIT];/* Keeps the virtual addrs for xmit frames */
    long xbufsp[IEEF_NXMIT];	  /* Keeps the phys. addrs for xmit frames */
    char tbdavail[IEEF_NXMIT];    /* Map of available TBDs */
    char cmdavail[IEEF_NCMDS];    /* Map of available command buffers */
    struct ether_addr mcs_addrs[10];
};

#define IEEF_HW_FLASH	1
#define IEEF_HW_UNISYS	2
#define	IEEF_HW_UNISYS_FLASH	4
#define	IEEF_HW_EE100_EISA	8
#define	IEEF_HW_EE100_PCI	16
#define	IEEF_HW_EE100	(IEEF_HW_EE100_EISA | IEEF_HW_EE100_PCI)

/* Media types */
#define	IEEF_MEDIA_BNC	0
#define	IEEF_MEDIA_AUI	2
#define	IEEF_MEDIA_TP	3

/* 
 * SCB Command Unit STAT bits
 */
#define	SCB_CUS_MSK	0x0700	/* SCB CUS bit mask */
#define	SCB_CUS_IDLE	0x0000	/* CU idle */
#define	SCB_CUS_SUSPND	0x0100	/* CU suspended */
#define	SCB_CUS_ACTV	0x0200	/* CU active */

/* 
 * SCB Receive Unit STAT bits
 */
#define	SCB_RUS_MSK	0x0070	/* SCB RUS bit mask */
#define	SCB_RUS_IDLE	0x0000	/* RU idle */
#define SCB_RUS_SUSPND	0x0010	/* RU suspended */
#define SCB_RUS_NORESRC 0x0020	/* RU no resource */
#define	SCB_RUS_READY	0x0040	/* RU ready */

/*
 * SCB ACK bits
 * these bits are used to acknowledge an interrupt from 82586
 */
#define SCB_ACK_MSK	0xf000	/* SCB ACK bit mask */
#define SCB_ACK_CX	0x8000	/* ACK_CX,  acknowledge a completed cmd */
#define SCB_ACK_FR	0x4000	/* ACK_FR,  acknowledge a frame reception */
#define	SCB_ACK_CNA	0x2000	/* ACK_CNA, acknowledge CU not active */
#define SCB_ACK_RNR	0x1000	/* ACK_RNR, acknowledge RU not ready */

/* 
 * SCB Command Unit commands
 */
#define	SCB_CUC_MSK	0x0700	/* SCB CUC bit mask */
#define	SCB_CUC_NOP		0x0000	/* do nothing */
#define	SCB_CUC_STRT	0x0100	/* start CU */
#define	SCB_CUC_RSUM	0x0200	/* resume CU */
#define	SCB_CUC_SUSPND	0x0300	/* suspend CU */
#define	SCB_CUC_ABRT	0x0400	/* abort CU */

/* 
 * SCB Receive Unit commands 
 */
#define SCB_RUC_MSK	0x0070	/* SCB RUC bit mask */
#define	SCB_RUC_STRT	0x0010	/* start RU */
#define	SCB_RUC_RSUM	0x0020	/* resume RU */
#define	SCB_RUC_SUSPND	0x0030	/* suspend RU */
#define	SCB_RUC_ABRT	0x0040	/* abort RU */

/*
 * SCB software reset bit
 */
#define SCB_RESET	0x0080	/* RESET, reset chip same as hardware reset */

/*
 * general defines for the command and descriptor blocks
 */
#define CS_CMPLT	0x8000	/* C bit, completed */
#define CS_BUSY		0x4000	/* B bit, Busy */
#define CS_OK		0x2000	/* OK bit, error free */
#define CS_ABORT	0x1000	/* A bit, abort */
#define CS_EL		0x80000000	/* EL bit, end of list */
#define CS_SUSPND	0x40000000	/* S bit, suspend */
#define CS_INT		0x20000000	/* I bit, interrupt */
#define	CS_STAT_MSK	0x3fff	/* Command status mask */
#define CS_EOF		0x8000	/* EOF (End Of Frame) in the TBD and RBD */
#define	CS_RBD_CNT_MSK	0x3fff	/* actual count mask in RBD */

/* flag bits in the rfd_ctlflags field */
#define RF_EL       	0x8000
#define FLEXIBLE_MODE	0x0008

#define CB_SF       0x80000

#define XMIT_ERRS 0x0f000000
#define XERR_LC   0x08000000
#define XERR_NC   0x04000000
#define XERR_CTS  0x02000000
#define XERR_UND  0x01000000
#define XERR_DEF  0x00800000
#define XERR_HB   0x00400000
#define XERR_COLL 0x00200000

/*
 * 82596 commands
 */
#define CS_CMD_MSK	0x70000	/* command bits mask */
#define	CS_CMD_NOP	0x00000	/* NOP */
#define	CS_CMD_IASET	0x10000	/* Individual Address Set up */
#define	CS_CMD_CONF	0x20000	/* Configure */
#define	CS_CMD_MCSET	0x30000	/* Multi-Cast Setup */
#define	CS_CMD_XMIT	0x40000	/* transmit */
#define	CS_CMD_TDR	0x50000	/* Time Domain Reflectomete */
#define CS_CMD_DUMP	0x60000	/* dump */
#define	CS_CMD_DGNS	0x70000	/* diagnose */

/* Issue a channel attention to the 596 */

#define CHANNEL_ATTENTION(ieefp) \
               outw(ieefp->ieef_ioaddr + ieefp->ieef_ca, 0)

/* 
 * Convert a physical address to a virtual address.  This only works for
 * the memory allocated by iopb_alloc (low contiguous memory) 
 */
#define IEEF_PTOV(ieefp, addr) \
        ( ((long)addr - (long)ieefp->pmem_map) + \
          ((long)ieefp->kmem_map) ) \

#ifdef DEBUG6
#define COMMAND_QUIESCE(ieefp) {\
        long w = 1000000;\
        while ((ieefp->kmem_map->ieef_scb.scb_command) && (--w));\
        if (!w) {\
            putstr("ieef: scb_command=0x");\
            puthex(ieefp->kmem_map->ieef_scb.scb_command);\
            putstr(" scb_status=0x");\
            puthex(ieefp->kmem_map->ieef_scb.scb_status);\
            putstr("\r\n");\
	}\
}
#else
#define COMMAND_QUIESCE(ieefp) {\
        long w = 1000000;\
        while ((ieefp->kmem_map->ieef_scb.scb_command) && (--w));\
}
#endif

/*
 * PCI Constants
 */
#define	PCI_COOKIE_TO_BUS(x)	((unchar)(((x)&0xff00)>>8))
#define	PCI_COOKIE_TO_DEV(x)	((unchar)(((x)&0x00f8)>>3))
#define	PCI_COOKIE_TO_FUNC(x)	((unchar)((x)&0x0007))

/*
 * Configuration stuff from Intel for 596 and 556
 */

#define MINIMUM_ETHERNET_PACKET_SIZE 60
#define DEFAULT_FIFO_THRESHOLD 0xc
#define IFS_SPACING 0x60
#define BIT_0 1
#define BIT_1 2
#define BIT_2 4
#define BIT_3 8
#define BIT_4 0x10
#define BIT_5 0x20
#define BIT_6 0x40
#define BIT_7 0x80
#define BIT_8 0x100
#define BIT_9 0x200
#define BIT_10 0x400
#define BIT_11 0x800
#define BIT_12 0x1000
#define BIT_13 0x2000
#define BIT_14 0x4000
#define BIT_15 0x8000

#define CB_CFIG_PREFETCH_BIT    BIT_7
#define CB_CFIG_NO_MONITOR_MODE (BIT_6 | BIT_7)
#define CB_CFIG_ADDRESS_LEN     (BIT_1 | BIT_2)

#define CB_CFIG_IFS             IFS_SPACING
#define CB_CFIG_MAX_RETRIES     (BIT_4 | BIT_7)

#define CB_CFIG_CI_INT          BIT_3
#define CB_CFIG_TX_THRESHOLD    BIT_5

#define BM_CFIG_BYTE_COUNT      0x14
#define BM_CFIG_LOCK_DATA_RATE  BIT_1
#define BM_CFIG_SCRAMBLE_ENABLE BIT_7
#define BM_CFIG_ADDRESS_LEN     (BIT_1 | BIT_2)
#define BM_CFIG_AC_LOCATION     BIT_3
#define BM_CFIG_PREAMBLE_LEN    BIT_5
#define BM_CFIG_MAX_RETRIES     (BIT_4 | BIT_7)
#define BM_CFIG_CRS1            BIT_3
#define BM_CFIG_CRS_OR_CDT      BIT_7
#define BM_CFIG_LNGT_TYPE       BIT_0
#define BM_CFIG_LENGTH_FIELD    BIT_1
#define BM_CFIG_CRC_IN_MEM      BIT_2
#define BM_CFIG_NO_CDT_SAC      BIT_4
#define BM_CFIG_MAX_LENGTH      BIT_5
#define BM_CFIG_HASH_1          BIT_6
#define BM_CFIG_LOOPBACK_POL    BIT_7
#define BM_CFIG_TX_IFS_DEFER    BIT_2
#define BM_CFIG_EFIFO_LIMIT     0x3A
#define BM_CFIG_URUN_RETRY      (BIT_2 | BIT_3)

/* RFD Status */
#define RFD_RECEIVE_COLLISION   BIT_0
#define RFD_IA_MATCH            BIT_1
#define RFD_NO_EOP_FLAG         BIT_6
#define RFD_FRAME_TOO_SHORT     BIT_7
#define RFD_DMA_OVERRUN         BIT_8
#define RFD_NO_RESOURCES        BIT_9
#define RFD_ALIGNMENT_ERROR     BIT_10
#define RFD_CRC_ERROR           BIT_11
#define RFD_LENGTH_ERROR        BIT_12
#define RFD_STATUS_OK           BIT_13
#define RFD_STATUS_BUSY         BIT_14
#define RFD_STATUS_COMPLETE     BIT_15

#define CB_CFIG_FIFO_LIMIT      DEFAULT_FIFO_THRESHOLD

#define CB_CFIG_SLOTTIME_LOW    0x00
#define CB_CFIG_SLOTTIME_HI     0x02

#define CB_556_CFIG_DEFAULT_PARM0 (CB_CFIG_PREFETCH_BIT | BM_CFIG_BYTE_COUNT)
#define CB_556_CFIG_DEFAULT_PARM1 (CB_CFIG_NO_MONITOR_MODE | CB_CFIG_FIFO_LIMIT)

#define CB_556_CFIG_DEFAULT_PARM0_1 ( \
        (CB_556_CFIG_DEFAULT_PARM1 << 8) | CB_556_CFIG_DEFAULT_PARM0)
 
#define CB_556_CFIG_DEFAULT_PARM2_3 ( \
        (CB_CFIG_TX_THRESHOLD | CB_CFIG_CI_INT | (CB_CFIG_ADDRESS_LEN << 8)))
 
#define CB_556_CFIG_DEFAULT_PARM4_5 ( \
        (BM_CFIG_LOCK_DATA_RATE) | \
        (BM_CFIG_SCRAMBLE_ENABLE << 8))
 
#define CB_556_CFIG_DEFAULT_PARM6_7 ( \
        BM_CFIG_ADDRESS_LEN | BM_CFIG_AC_LOCATION | BM_CFIG_PREAMBLE_LEN)
 
#define CB_556_CFIG_DEFAULT_PARM8_9 CB_CFIG_IFS
 
#define CB_556_CFIG_DEFAULT_PARM10_11 ( \
        BM_CFIG_MAX_RETRIES | \
        CB_CFIG_SLOTTIME_HI | \
        ((BM_CFIG_CRS1 | BM_CFIG_CRS_OR_CDT) << 8))
 
#define CB_556_CFIG_DEFAULT_PARM12_13 (MINIMUM_ETHERNET_PACKET_SIZE     << 8)
 
#define CB_556_CFIG_DEFAULT_PARM14_15 BM_CFIG_LNGT_TYPE | \
        BM_CFIG_LENGTH_FIELD | BM_CFIG_CRC_IN_MEM | BM_CFIG_NO_CDT_SAC | \
        BM_CFIG_MAX_LENGTH | BM_CFIG_HASH_1 | BM_CFIG_LOOPBACK_POL
 
#define CB_556_CFIG_DEFAULT_PARM16_17 ( \
        0x3f | ((1 | BM_CFIG_TX_IFS_DEFER) << 8))
 
#define CB_556_CFIG_DEFAULT_PARM18_19 ( \
        BM_CFIG_EFIFO_LIMIT | (BM_CFIG_URUN_RETRY << 8))
