/*
 * Copyright (c) 1992-1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_XBOXREG_H
#define	_SYS_XBOXREG_H

#pragma ident	"@(#)xboxreg.h	1.4	98/01/06 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The XBox FCode defines loads of register properties.
 *
 * 0.	write0 register		(base of PROM).
 * 1.	XAdapter		error and status register- ERRD, ERRA, status
 * 2.	XAdapter		control register CTL0
 * 3.	XAdapter		control register CTL1
 * 4.	XAdapter		error log DVMA register ELUA
 * 5.	XAdapter		error log DVMA register ELLA
 * 6.	XAdapter		<reserved>
 * 7.	XBox Controller		error and status register- ERRD, ERRA, status
 * 8.	XBox controller		control register CTL0
 * 9.	XBox controller		control register CTL1
 * 10.	XBox controller		error log DVMA register ELUA
 * 11.	XBox controller		error log DVMA register ELLA
 * 12.	XBox controller		<reserved>
 */

struct xc_errs {
	uint_t		xc_errd;	/* error descriptor */
	caddr_t		xc_erra;	/* error virtual address */
	uint_t		xc_status;	/* status register */
	uint_t		xc_ctl0;	/* ctlr reg 0 */
};

/*
 * xc_errstat_pkt: dvma'ed to main memory after an error occurred.
 */
struct xc_errstat_pkt {
	struct xc_errs	xc_errs;
	uint_t		xc_ctl1;	/* ctlr reg 1 */
	uint_t		xc_elua;	/*	*/
	uint_t		xc_prom;	/*	*/
	uint_t		xc_ella;	/*	*/
};

/*
 * we expect 6 register properties for xac and 5 for xbc
 * so we only need to map in 11 pages for full access to all registers
 * except error log enable which we can do thru write0 writes
 */

#define	EPROM_SIZE	0x10000
#define	XAC		0
#define	XBC		1
#define	N_XBOX_REGS	12

struct	xc {		/* also includes xbc */
	int			*xac_write0;
	struct xc_errs		*xac_errs;
	uint_t			*xac_ctl0;
	uint_t			*xac_ctl1;
	uint_t			*xac_elua;
	uint_t			*xac_ella;
	uint_t			*xac_donotuse;

	/*
	 * xbc space
	 */
	struct xc_errs		*xbc_errs;
	uint_t			*xbc_ctl0;
	uint_t			*xbc_ctl1;
	uint_t			*xbc_elua;
	uint_t			*xbc_ella;
	uint_t			*xbc_donotuse;
};

/*
 * defines for write0 accesses
 */
#define	XAC_CTL0_OFFSET			0x100000
#define	XAC_CTL1_OFFSET			0x110000
#define	XAC_ELUA_OFFSET			0x120000
#define	XAC_ELLA_OFFSET			0x130000
#define	XAC_ERRLOG_ENABLE_OFFSET	0x140000

#define	XBC_CTL0_OFFSET			0x500000
#define	XBC_CTL1_OFFSET			0x510000
#define	XBC_ELUA_OFFSET			0x520000
#define	XBC_ELLA_OFFSET			0x530000
#define	XBC_ERRLOG_ENABLE_OFFSET	0x540000

/*
 * error register bits
 */
#define	ERRD_MASK	0xffff0000	/* status mask */
#define	ERRD_ESDB	0x80000000	/* error status dirty bit */
#define	ERRD_ESIN	0x40000000	/* error status indicator */
#define	ERRD_XBSS	0x0f000000	/* expansion sbus slave selects */
#define	ERRD_PMREQ	0x00800000	/* parent master request */
#define	ERRD_PKTYP	0x00600000	/* packet type */
#define	ERRD_PAI	0x00100000	/* physical address info */
#define	ERRD_SBSIZ	0x00070000	/* subs size 2:0 */

#define	ERRD_STAT_BITS	"\20\40ESDN\37ESIN\30PMREQ\25PAI"

#define	ESTY_MASK	0xffff
#define	ESTY_CRTL1	0x4000	/* cable resend limit timeout error - dpr1 */
#define	ESTY_CADP1	0x2000	/* cable parity error - dpr1 */
#define	ESTY_XREA	0x1000	/* expansion sbus read error, erro ack */
#define	ESTY_XRRA	0x0800	/* expansion sbus read error, rsvd ack */
#define	ESTY_XRLE	0x0400	/* expansion sbus read error, late error */
#define	ESTY_XBTO	0x0200	/* expansion sbus timeout error */
#define	ESTY_WRZR	0x0100	/* write 0 error */
#define	ESTY_BWEA	0x0080	/* buffer write error - err ack */
#define	ESTY_BWRA	0x0040	/* buffer write error - revd ack */
#define	ESTY_BWLE	0x0020	/* buffer write error - late error */
#define	ESTY_CRLT0	0x0010	/* cable resend limit timeout error, dpr0 */
#define	ESTY_CATO	0x0008	/* cable ack timeout error */
#define	ESTY_CADP0	0x0004	/* cable parity error, dpr0 */
#define	ESTY_CSIP	0x0002	/* cable serial interrupt parity error */
#define	ESTY_CNRD	0x0001	/* child not ready error */

/*
#define	ERRD_ESTY_BITS \
"\20\17CRTL1\16CADP1\15XREA\14XRRA\13XRLE\12XBTO\11WRZR\10BWEA\07BWRA\
\06BWLE\05CRLT0\04CATO\03CADP0\02CSIP\01CNRD"
*/

#define	ERRD_ESTY_BITS \
"\20\
\17cable-resend-limit\
\16cable-parity-dpr1\
\15exp-sbus-read-err-ack\
\14exp-sbus-read-err-revd-ack\
\13exp-sbus-read-late-err\
\12exp-sbus-timeout\
\11write-0-err\
\10buffer-write-err-err-ack\
\07buffer-write-err-revd-ack\
\06buffer-write-err-late-err\
\05cable-resend-limit-timeout\
\04cable-ack-timeout\
\03cable-parity-dpr0\
\02cable-serial-intr-parity\
\01child-not-ready"


/*
 * status register bits
 */
#define	STAT_INTS	0x010000 /* interrupt status */
#define	STAT_SBIR7	0x004000 /* host sbus interrupt request level */
#define	STAT_SBIR6	0x002000 /* host sbus interrupt request level */
#define	STAT_SBIR5	0x001000 /* host sbus interrupt request level */
#define	STAT_SBIR4	0x000800 /* host sbus interrupt request level */
#define	STAT_SBIR3	0x000400 /* host sbus interrupt request level */
#define	STAT_SBIR2	0x000200 /* host sbus interrupt request level */
#define	STAT_SBIR1	0x000100 /* host sbus interrupt request level */
#define	STAT_SECP	0x000080 /* sec connector present */
#define	STAT_CRDY	0x000040 /* child ready status */
#define	STAT_MODE	0x000020 /* mode input pin state */
#define	STAT_F3ST	0x000008 /* dual port ram '3' status */
#define	STAT_F2ST	0x000004 /* dual port ram '2' status */
#define	STAT_F1ST	0x000002 /* dual port ram '1' status */
#define	STAT_F0ST	0x000001 /* dual port ram '0' status */

#define	STAT_BITS	\
"\20\21INTS\17SBIR7\16SBIR6\16SBIR5\14SBIR4\13SBIR3\12SBIR2\11SBIR1\
\10SECP\07CRDY\06MODE\04F3ST\03F2ST\02F1ST\01F0ST"

/*
 * control register 0 bits
 */
#define	CTL0_ZKEY	0x0f00	 /* write 0 key */
#define	CTL0_UADM	0x00f8	 /* upper address decode map */
#define	CTL0_ILVL	0x0007	 /* interrupt level */

#define	UADM_23_23_23_23	(0)

#define	UADM_25_XX_XX_XX	(4 << 3)
#define	UADM_XX_25_XX_XX	(5 << 3)
#define	UADM_XX_XX_25_XX	(6 << 3)
#define	UADM_XX_XX_XX_25	(7 << 3)

#define	UADM_24_24_XX_XX	(8 << 3)
#define	UADM_24_XX_24_XX	(9 << 3)
#define	UADM_XX_24_24_XX	(0xa << 3)

#define	UADM_24_23_23_XX	(0xc << 3)
#define	UADM_23_24_23_XX	(0xc << 3)
#define	UADM_24_23_24_XX	(0xc << 3)

#define	UADM_26_26_26_26	(0x10 << 3)

#define	UADM_28_XX_XX_XX	(0x14 << 3)
#define	UADM_XX_28_XX_XX	(0x15 << 3)
#define	UADM_XX_XX_28_XX	(0x16 << 3)
#define	UADM_XX_XX_XX_28	(0x17 << 3)

#define	UADM_28_28_28_28	(0x18 << 3)


/*
 * resets
 */
#define	SRST_NRES		0
#define	SRST_XARS		(1 << 12)
#define	SRST_CRES		(2 << 12)
#define	SRST_HRES		(3 << 12)

/*
 * control register 1 bits
 */
#define	CTL1_CDPT1	0x8000	/* cable data parity test enable, dpr1 */
#define	CTL1_CRTE	0x4000	/* cable rerun test enable */
#define	CTL1_SRST	0x3000	/* software reset */
#define	CTL1_DVTE	0x0800	/* dvma test enable */
#define	CTL1_CIPT	0x0400	/* cable data parity test enable */
#define	CTL1_INTT	0x0200	/* interrupt test enable */
#define	CTL1_CDPT0	0x0100	/* cable data parity test enable, dpr0 */
#define	CTL1_ELDS	0x00c0	/* error log dvma size select */
#define	CTL1_XSLE	0x0020	/* expansion sbus slot select enable */
#define	CTL1_XBRE	0x0010	/* expansion sbus bus request enable */
#define	CTL1_XBIE	0x0008	/* expansion sbus interrupt enable */
#define	CTL1_ELDE	0x0004	/* error log dvma enable */
#define	CTL1_CSIE	0x0002	/* cable serial interrupt enable */
#define	CTL1_TRAN	0x0001	/* transparent enable */

#define	CTL1_BITS	\
"\20\20CDPT1\17CRTE\14DVTE\13CIPT\12INTT\11CDPT0\06XSLE\
\05XBRE\04XBIE\03ELDE\02CSIE\01TRAN"

#define	XAC_CTL1	 (CTL1_TRAN | CTL1_CSIE | CTL1_ELDE)
#define	XBC_CTL1	 (CTL1_XBIE | CTL1_ELDE | CTL1_XBRE | CTL1_XSLE)

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_XBOXREG_H */
