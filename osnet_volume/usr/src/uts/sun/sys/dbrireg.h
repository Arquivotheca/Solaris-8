/*
 * Copyright (c) 1991-1992,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DBRIREG_H
#define	_SYS_DBRIREG_H

#pragma ident	"@(#)dbrireg.h	1.7	97/10/31 SMI"

/*
 * AT&T T5900FC DBRI device register and data structure definitions
 */

#ifdef __cplusplus
extern "C" {
#endif


#define	DBRI_NPIPES	(32)	/* Number of pipes per DBRI */

/*
 * Format of the Forth header which is available after reset
 */
typedef union {
	    uint32_t fcode_i[12];
	    uint16_t fcode_s[24];
	    uint8_t fcode_c[48];
} dbri_fcode_t;


/*
 * Register format
 */
typedef union {
	volatile struct {	/* ATT register definitions (r) */
		unsigned int
		    reg0,	/* Reg0 (64) - Status Register */
		    reg1,	/* Reg1 (68) - Mode and Interrupt Register */
		    reg2,	/* Reg2 (72) - Parallel Input/Output */
		    reg3,	/* Reg3 (76) - Test Mode */
		    :32,
		    :32,
		    :32,
		    :32,
		    reg8,	/* Reg8 (96) - Program Counter */
		    reg9;	/* Reg9 (100) - Interrupt Queue Pointer */
	} r;

	volatile struct {	/* mnemonic structure of registers (n) */
		unsigned int
		    sts,	/* Reg0 (64) - Status Register */
		    intr,	/* Reg1 (68) - Mode and Interrupt Register */
		    pio,	/* Reg2 (72) - Parallel Input/Output */
		    tst_mode,	/* Reg3 (76) - Test Mode */
		    :32,
		    :32,
		    :32,
		    :32,
		    cmdqp,	/* Reg8 (96) - Program Counter */
		    intrqp;	/* Reg9 (100) - Interrupt Queue Pointer */
	} n;
} dbri_reg_t;


/*
 * (sts_reg0) Status register (Register 0) defines
 */
#define	DBRI_STS_PIPEOFFSET	16		/* bit offset for pipe 0 */
#define	DBRI_STS_P		(1 << 15)	/* Queue Pointer Valid */
#define	DBRI_STS_S		(1 << 13)	/* 16-Word SBus Burst */
#define	DBRI_STS_E		(1 << 12)	/* 8-Word SBus Burst */
#define	DBRI_STS_G		(1 << 14)	/* 4-Word SBus Burst */
#define	DBRI_STS_X		(1 << 7)	/* Sanity Timer Disable */
#define	DBRI_STS_T		(1 << 6)	/* Enable TE Interface */
#define	DBRI_STS_N		(1 << 5)	/* Enable NT Interface */
#define	DBRI_STS_C		(1 << 4)	/* Enable CHI Interface */
#define	DBRI_STS_F		(1 << 3)	/* Force Sanity Timer Timeout */
#define	DBRI_STS_D		(1 << 2)	/* Disable SBus Master Mode */
#define	DBRI_STS_H		(1 << 1)	/* Halt for Analysis */
#define	DBRI_STS_R		(1)		/* Soft Reset */


/*
 * (intr_reg1) Mode and Interrupt register (Register 1) defines
 */
#define	DBRI_INTR_LIT_ENDIAN	(1 << 8)	/* Little Endian Byte Order */
#define	DBRI_INTR_MRR_ERR	(1 << 4)	/* Multiple Error Ack on SBus */
#define	DBRI_INTR_LATE_ERR	(1 << 3)	/* Multiple Late Err on SBus */
#define	DBRI_INTR_BUS_GRANT_ERR	(1 << 2)	/* Lost Bus Grant on SBus */
#define	DBRI_INTR_BURST_ERR	(1 << 1)	/* Multiple Burst Err on SBus */
#define	DBRI_INTR_REQ		(1)		/* Interrupt Request */


/*
 * (pio_reg2) Parallel Input Output register (Register 2) defines
 */
#define	DBRI_PIO_0		(1)		/* PDN */
#define	DBRI_PIO_1		(1 << 1)	/* RESET */
#define	DBRI_PIO_2		(1 << 2)	/* Internal PDN */
#define	DBRI_PIO_3		(1 << 3)	/* Data/Control Mode */
#define	DBRI_PIO0_EN		(1 << 4)	/* Parallel I/O Enable lines */
#define	DBRI_PIO1_EN		(1 << 5)
#define	DBRI_PIO2_EN		(1 << 6)
#define	DBRI_PIO3_EN		(1 << 7)


/*
 * (tst_mode_reg3) Test Mode register (Register 3) defines
 */
#define	DBRI_TST_MODE_SPEED	(1 << 2)	/* High Speed Test Mode */
#define	DBRI_TST_MODE_PIO	(1 << 3) /* PIO inputs replace receivers */



/*
 * Command Set
 */
#define	DBRI_OPSHIFT	(28)
#define	DBRI_OPMASK	(0xf0000000)
#define	DBRI_OP(x)	((((uint_t)x & DBRI_OPMASK)) >> DBRI_OPSHIFT)
#define	DBRI_OPCODE(x)	((((uint_t)x) << DBRI_OPSHIFT) & DBRI_OPMASK)

#define	DBRI_CMD_WAIT	DBRI_OPCODE(0x0)	/* Wait */
#define	DBRI_CMD_PAUSE	DBRI_OPCODE(0x1)	/* Pause */
#define	DBRI_CMD_JMP	DBRI_OPCODE(0x2)	/* Jump to new command queue */
#define	DBRI_CMD_JMP_LEN	(2)
#define	DBRI_CMD_IIQ	DBRI_OPCODE(0x3)	/* Initialize interrupt queue */
#define	DBRI_CMD_REX	DBRI_OPCODE(0x4)	/* Report cmd via interrupt  */
#define	DBRI_OPCODE_REX	(0x4)			/* Report cmd via interrupt  */
#define	DBRI_CMD_SDP	DBRI_OPCODE(0x5)	/* Set-up data pipe */
#define	DBRI_CMD_CDP	DBRI_OPCODE(0x6)	/* Continue data pipe */
#define	DBRI_CMD_DTS	DBRI_OPCODE(0x7)	/* Define time slot */
#define	DBRI_OPCODE_DTS	(0x7)			/* DTS command for bit fields */
#define	DBRI_CMD_SSP	DBRI_OPCODE(0x8)	/* Set short pipe data */
#define	DBRI_CMD_CHI	DBRI_OPCODE(0x9)	/* Set CHI global mode */
#define	DBRI_CMD_NT	DBRI_OPCODE(0xa)	/* NT command */
#define	DBRI_CMD_TE	DBRI_OPCODE(0xb)	/* TE command */
#define	DBRI_CMD_CDEC	DBRI_OPCODE(0xc)	/* Codec setup */
#define	DBRI_CMD_TEST	DBRI_OPCODE(0xd)	/* Test command */
#define	DBRI_CMD_CDM	DBRI_OPCODE(0xe)	/* CHI data mode */


/*
 * These apply to multiple commands
 */
#define	DBRI_CMDI		(1 << 27)	/* Interrupt on cmd complete */
#define	DBRI_CMD_VALUE(a)	(a & 0x07ff)	/* Command tag value */
#define	DBRI_CMD_MASK		(0xf << DBRI_OPSHIFT)	/* Command mask value */


/*
 * Set-up Data Pipe (SDP) Command bit defines
 */
#define	DBRI_SDP_COLL		(1 << 19)
#define	DBRI_SDP_IRM		(0)		/* XXX5 - don't enable UNDR */
#define	DBRI_SDP_PIPE(a)	(a & 0x1f)	/* Pipe number */

/*
 * IRM for Fixed data change interrupt (FXDT)
 */
#define	DBRI_SDP_TWOSAME	(1 << 18)	/* 2nd time value received */
#define	DBRI_SDP_CHNG		(2 << 18)	/* report data change */
#define	DBRI_SDP_ALLVAL		(3 << 18)	/* report all values */

#define	DBRI_SDP_EOL		(1 << 17)	/* End of List intr enable */
#define	DBRI_SDP_IBEG		(1 << 16)	/* Masks IBEG and IEND intrs */

#define	DBRI_SDP_TRANSPARENT	(0 << 13)	/* Transparent mode */
#define	DBRI_SDP_HDLC		(2 << 13)	/* HDLC mode */
#define	DBRI_SDP_TE_DCHAN	(3 << 13)	/* HDLC TE D Channel mode */
#define	DBRI_SDP_SERIAL		(4 << 13)	/* Serial in to out (no DMA) */
#define	DBRI_SDP_FIXED		(6 << 13)	/* Fixed Data (also no DMA) */
#define	DBRI_SDP_MODEMASK	(7 << 13)	/* Mask for SDP mode */

#define	DBRI_SDP_D		(1 << 12)	/* Transmit direction */
#define	DBRI_SDP_B		(1 << 11)	/* MSBit transmitted first */

#define	DBRI_SDP_PTR		(1 << 10)	/* Use ptr for new TD/RD */
#define	DBRI_SDP_IDL		(1 << 9)	/* Send idle following frame */
#define	DBRI_SDP_ABT		(1 << 8)	/* Set abort in data FIFO */
#define	DBRI_SDP_CLR		(1 << 7)	/* Clear pipe */
#define	DBRI_SDP_CMDMASK	(0xf << 7)

/* Continue Data Pipe (CDP) Command bit defines */
#define	DBRI_CDP_PIPE(a)	(a & 0x1f)	/* Pipe number */


/* Report EXecution, CMDI bit always should be set to one */
union dbri_rex_opcode {
	struct {
		unsigned int
		    cmd:4,		/* REX command */
		    cmdi:1,		/* command intr bit */
		    pipe:6,		/* software pipe # or special code */
		    tag:21;		/* software tag */
	} r;

	uint32_t word32;
};


/* Define Time Slot (DTS) Command structures */
union dbri_dts_opcode {
	struct {
		unsigned int
		    cmd:4,		/* DTS command */
		    cmdi:1,		/* command intr bit */
		    res:9,		/* reserved */
		    vi:1,		/* valid input tsd */
		    vo:1,		/* valid output tsd */
		    id:1,		/* insert/delete bit */
		    oldin:5,		/* old input pipe */
		    oldout:5,		/* old output pipe */
		    pipe:5;		/* current pipe */
	} r;

	uint32_t word32;
};


/*
 * Note that the r structure can be used for almost all transactions in
 * the driver as this is the common case for regular tx and rx pipe
 * setup. For the chi the special "chi" structure should be used. This
 * struct is good enough for both tx and receive chi modes.
 */
union dbri_dts_tsd {
	struct {
		unsigned int
		    len:8,	/* length */
		    cycle:10,	/* cycle */
		    di:1,	/* data invert */
		    mode:3,	/* mode */
		    mon:5,	/* monitor pipe */
		    next:5;	/* next pipe */
	} r;

	struct {
		unsigned int
		    :19,
		    mode:3,	/* mode */
		    :5,		/* monitor pipe */
		    next:5;	/* next pipe */
	} chi;

	uint32_t word32;
};


/*
 */
typedef struct dbri_dts_cmd dbri_dts_cmd_t;
struct dbri_dts_cmd {
	union dbri_dts_opcode opcode;
	union dbri_dts_tsd input_tsd;
	union dbri_dts_tsd output_tsd;
};


/*
 * Define Time Slot (DTS) Command bit defines
 */
#define	DBRI_DTS_VI		1		/* Valid Input TSD */
#define	DBRI_DTS_VO		1		/* Valid Output TSD */
#define	DBRI_DTS_ID		1		/* Add/Modify time slot */
#define	DBRI_DTS_DI		1		/* Data Invert */
#define	DBRI_DTS_SINGLE		0		/* Single channel */
#define	DBRI_DTS_MONITOR	2		/* Monitor receive pipe (tee) */
#define	DBRI_DTS_INDIRECT	3		/* Indirect for multi TS */
#define	DBRI_DTS_ALTERNATE	7		/* Alternate coding */


/*
 * Set Short Pipe (SSP) Command bit defines
 */
#define	DBRI_SSP_PIPE(a)	((a) & 0x1f)	/* Pipe number */


/*
 * Set CHI Global Mode (CHI) Command bit defines
 */
#define	DBRI_CHI_CHICM(a)	(((a) & 0xff) << 16) /* CHI clock mode */
#define	DBRI_CHI_INT		(1 << 15)	/* Intr and reporting mask */
#define	DBRI_CHI_CHIL		(1 << 14)	/* Lost frame sync intr */
#define	DBRI_CHI_OD		(1 << 13)	/* Open Drain */
#define	DBRI_CHI_FE		(1 << 12)	/* Frame edge high */
#define	DBRI_CHI_FD		(1 << 11)	/* Frame drive on rising edge */
#define	DBRI_CHI_BPF(a)		((a) & 0x7ff)	/* CHI bits per frame */


/*
 * NT and TE Command bit defines
 */
#define	DBRI_NTE_F		(1 << 17)	/* accept F bit.. (see spec) */
#define	DBRI_NTE_NBF		(1 << 16)	/* # bad frs to loose framing */

#define	DBRI_NTE_IRM_STATUS	(1 << 15)	/* IRM - force immed intr */
#define	DBRI_NTE_IRM_SBRI	(1 << 14)	/* IRM - Enable SBRI intrs */
#define	DBRI_NTE_ISNT		(1 << 13)	/* NT when set, TE otherwise */
#define	DBRI_NT_FT		(1 << 12)	/* Fixed timing */
#define	DBRI_NTE_EZ		(1 << 11)	/* Send zeros in E channel */
#define	DBRI_NTE_IFA		(1 << 10)	/* Inhibit final activation */
#define	DBRI_NTE_ACT		(1 << 9)	/* Activate interface */
#define	DBRI_NT_MFE		(1 << 8)	/* Multiframe enable  */
#define	DBRI_TE_QE		(1 << 8)	/* Q channel enable  */
#define	DBRI_NTE_RLB_D		(1 << 7)	/* Remote loop back D chnl */
#define	DBRI_NTE_RLB_B1		(1 << 6)	/* Remote loop back B1 chnl */
#define	DBRI_NTE_RLB_B2		(1 << 5)	/* Remote loop back B2 chnl */
#define	DBRI_NTE_LLB_D		(1 << 4)	/* Local loop back D chnl */
#define	DBRI_NTE_LLB_B1		(1 << 3)	/* Local loop back B1 chnl */
#define	DBRI_NTE_LLB_B2		(1 << 2)	/* Local loop back B2 chnl */
#define	DBRI_NTE_FACT		(1 << 1)	/* Force Immediate Activation */
#define	DBRI_NTE_ABV		(1)		/* Add. BiPolar Violation Det */


/*
 * Codec Setup (CDEC) Command bit defines
 */
#define	DBRI_CDEC_DISABL_CLK	(0 << 24)	/* Disable CKCOD (high) */
#define	DBRI_CDEC_2MHZ		(1 << 24)	/* 2.048 MHz CKCOD */
#define	DBRI_CDEC_1_5MHZ	(2 << 24)	/* 1.536 MHz CKCOD */
#define	DBRI_CDEC_FE_FSCOD(a)	(((a) & 0x7ff) << 12) /* Falling edge delay */
#define	DBRI_CDEC_RE_FSCOD(a)	((a) & 0x7ff) 	/* Rising edge delay */


/*
 * Test Command bit defines
 */
#define	DBRI_TST_TYPE(a)	((a) & 0x7ff)	/* Test type */
#define	DBRI_TST_RAMPTR(a)	(((a) & 0x3ff) << 16) /* RAM pointer */
#define	DBRI_TST_SIZE(a)	(((a) & 0x1f) << 11) /* Size field */
#define	DBRI_TST_ROMMON		0x5		/* ROM monitor toggle */
#define	DBRI_TST_SBUS		0x6		/* SBus test/copy */
#define	DBRI_TST_SERIAL		0x7		/* Serial Controller test */
#define	DBRI_TST_RAM_RD		0x8		/* RAM read */
#define	DBRI_TST_RAM_WT		0x9		/* RAM write */
#define	DBRI_TST_RAM_BIST	0xa		/* RAM Built in Self-Test */
#define	DBRI_TST_UCTL_BIST	0xb		/* Ucontroller BIST */
#define	DBRI_TST_ROM_DUMP	0xc		/* ROM dump */


/*
 * Set CHI Data Mode Command
 */
#define	DBRI_CDM_THI		(1 << 8)	/* Transmit data on CHIDR */
#define	DBRI_CDM_RHI		(1 << 7)	/* Receive data from CHIDX */
#define	DBRI_CDM_RCE		(1 << 6)	/* Receive clock rising edge */
#define	DBRI_CDM_CMSR		(1 << 5)	/* Clock mode 2 pulse select */
#define	DBRI_CDM_ODD		(1 << 4)	/* Strobe on odd clock count */
#define	DBRI_CDM_CMSX		(1 << 3)	/* Two clocks per bit */
#define	DBRI_CDM_XCE		(1 << 2)	/* Transmit on rising edge */
#define	DBRI_CDM_XEN		(1 << 1)	/* Transmit enable */
#define	DBRI_CDM_REN		(1)		/* Receive enable */


/*
 * structure of DBRI commands
 */
typedef struct dbri_chip_cmd {
	uint32_t opcode;
	uint32_t arg[3];		/* XXX - Constant! */
} dbri_chip_cmd_t;


struct dbri_code_sbri {	/* SBRI */
	unsigned int
	    :12,
	    :9,
	    vta:1,		/* voltage threshold adjustment */
	    berr:1,		/* detectible bit error */
	    ferr:1,		/* frame sync error */
	    mfm:1,		/* multi-frame mode */
	    fsc:1,		/* frame sync established */
	    rif4:1,		/* if TE A-bit is set */
	    rif0:1,		/* no signal received */
	    act:1,		/* command bit 9 (activate) */
	    tss:3;		/* interface activation state */
};


struct dbri_code_chil {	/* CHIL */
	unsigned int
	    :12,
	    :16,
	    overflow:1,		/* CHI receiver overflow */
	    nofs:1,		/* Ext frame sync not detected 250Us */
	    xact:1,		/* CHI transmitter active */
	    ract:1;		/* CHI receiver active */
};

struct dbri_code_rex {	/* REX */
	unsigned int
	    XXX:32;
};


typedef union {
	struct {
		unsigned int
		    ibits:2,	/* I bits */
		    channel:6,	/* channel number */
		    code:4,	/* interrupt code */
		    field:20;	/* interrupt field */
	} f;

	/*
	 * CMDI
	 */
	struct {
		unsigned int
		    :12,
		    opcode:4,	/* opcode of command */
		    value:16;	/* value field of command */
	} code_cmdi;

	/*
	 * SBRI
	 */
	struct dbri_code_sbri code_sbri;

	/*
	 * FXDT
	 */
	struct {
		unsigned int
		    :12,
		    changed_data:20;
	} code_fxdt;

	/*
	 * CHIL
	 */
	struct dbri_code_chil code_chil;

	/*
	 * REX - Report EXection
	 */
	struct dbri_code_rex	code_rex;

	uint32_t word;
} dbri_intrq_ent_t;


#define	DBRI_MAX_QWORDS		63	/* Max # of intr queue words */

/*
 * DBRI interrupt queue structure
 */
typedef struct dbri_intq {
	volatile caddr32_t nextq;	/* next interrupt queue ptr */
	volatile dbri_intrq_ent_t intr[DBRI_MAX_QWORDS]; /* intr queue words */
} dbri_intq_t;


/*
 * Interrupt ibit bits
 */
#define	DBRI_INT_IBITS	0x2 		/* Set on intr in queue structure */

/*
 * Interrupt channel bits
 */
#define	DBRI_INT_MAX_CHAN	(32)	/* Maximum pipe channel number */
#define	DBRI_INT_TE_CHAN	(32)	/* TE channel interrupt status */
#define	DBRI_INT_NT_CHAN	(34)	/* NT channel interrupt status */
#define	DBRI_INT_CHI_CHAN	(36)	/* CHI channel interrupt status */
#define	DBRI_INT_REPORT_CHAN	(38)	/* Report Command channel intr status */
#define	DBRI_INT_OTHER_CHAN	(40)	/* Other channel interrupt status */

/*
 * Interrupt code bits
 */
#define	DBRI_INT_BRDY	0x1 		/* Buffer ready for processing */
#define	DBRI_INT_MINT	0x2 		/* Marked interrupt in RD/TD */
#define	DBRI_INT_IBEG	0x3 		/* Flag to idle transition detected */
#define	DBRI_INT_IEND	0x4 		/* Idle to flag transition detected */
#define	DBRI_INT_EOL	0x5 		/* End of list */
#define	DBRI_INT_CMDI	0x6 		/* 1st word  of command read by DBRI */
#define	DBRI_INT_XCMP	0x8 		/* Transmission of frame complete */
#define	DBRI_INT_SBRI	0x9 		/* BRI status change INFO */
#define	DBRI_INT_FXDT	0xa 		/* Fixed data change */
#define	DBRI_INT_CHIL	0xb 		/* CHI lost frame sync */
#define	DBRI_INT_COLL	0xb		/* Unrecoverable D-channel collision */
#define	DBRI_INT_DBYT	0xc 		/* Dropped byte frame slip */
#define	DBRI_INT_RBYT	0xd 		/* Repeated byte frame slip */
#define	DBRI_INT_LINT	0xe 		/* Lost interrupt */
#define	DBRI_INT_UNDR	0xf 		/* DMA underrun */

/*
 * SBRI tss bit field definitions
 */
#define	DBRI_NTINFO0_G1	0x0 		/* Inactive and NOT TRYING to act */
#define	DBRI_NTINFO2_G2	0x6 		/* Sending framing */
#define	DBRI_NTINFO4_G3	0x7 		/* Sending data */
#define	DBRI_NTINFO0_G4	0x1 		/* Use illegal code for State G4 */

#define	DBRI_TEINFO0_F1	0x0 		/* Inactive and NOT TRYING to act */
#define	DBRI_TEINFO0_F8	0x2 		/* Lost framing */
#define	DBRI_TEINFO0_F3	0x3 		/* Not seeking activation */
#define	DBRI_TEINFO1_F4	0x4 		/* Sending request for activation */
#define	DBRI_TEINFO0_F5	0x5 		/* Requested act but no framing yet */
#define	DBRI_TEINFO3_F6	0x6 		/* Sending framing */
#define	DBRI_TEINFO3_F7	0x7 		/* Sending data */


/*
 * DBRI transmit message descriptor structure
 *
 * NB: dbri_md_t is not this union, it's up to the driver to define it
 */
union dbri_md {
	volatile struct {
		unsigned int
		    eof:1,	/* End of frame */
		    dcrc:1,	/* Do not append CRC */
		    :1,
		    cnt:13, 	/* Data byte count */
		    fint:1, 	/* Frame interrupt */
		    mint:1, 	/* Mark interrupt */
		    idl:1,	/* Transmit idle characters */
		    fcnt:13; 	/* Flag fill count */
		caddr32_t bufp;	/* buffer pointer */
		caddr32_t fp; /* forward descriptor pointer */
		uint_t status;	/* status */
	} tx;

	volatile struct {
		unsigned int
		    eof:1,	/* End of frame */
		    com:1,	/* Buffer complete */
		    :1,
		    cnt:13, 	/* Data byte count */
		    :8,
		    status:8; 	/* Status */
		caddr32_t bufp; /* buffer pointer */
		caddr32_t fp;	/* forward descriptor pointer */
		unsigned int
		    :16,
		    fint:1,	/* Frame interrupt */
		    mint:1,	/* Mark interrupt */
		    :1,
		    bcnt:13;	/* Buffer count */
	} rx;

	volatile uint32_t _words[4];
};


/*
 * ISDN Priority Class for D-Channel Access method.
 * (CCITT Fascicle III.8, 6.1.4)
 */
#define	DBRI_D_CLASS_1	0	/* D-channel call control priority */
#define	DBRI_D_CLASS_2	1	/* D-channel normal priority */

/*
 * Transmit descriptor status bits
 */
#define	DBRI_TMD_UNR	(1 << 3) /* Underrun */
#define	DBRI_TMD_ABT	(1 << 2) /* Abort */
#define	DBRI_TMD_TFC	(1 << 1) /* Transmit frame complete */
#define	DBRI_TMD_TBC	(1)	/* Transmit buffer complete */


/*
 * DBRI's DMA engine can handle a maximum size of 8191 bytes per fragment
 * on both transmit and receive.
 */
#define	DBRI_MAX_DMASIZE	(8191)

/*
 * Receive message descriptor status bits
 */
#define	DBRI_RMD_CRC	(1 << 7) /* CRC status */
#define	DBRI_RMD_BBC	(1 << 6) /* Bad byte count */
#define	DBRI_RMD_ABT	(1 << 5) /* Abort */
#define	DBRI_RMD_OVRN	(1 << 3) /* Overrun condition detected */

#ifdef __cplusplus
}
#endif

#endif /* !_SYS_DBRIREG_H */
