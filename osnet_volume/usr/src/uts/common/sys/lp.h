/*
 * Copyright (c) 1992-1993, by Sun Microsystems, Inc.  All Rights Reserved.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SYS_LP_H
#define	_SYS_LP_H

#pragma ident	"@(#)lp.h	1.10	99/02/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * lp status register
 */
#define	UNBUSY		0x80
#define	READY		0x40
#define	NOPAPER		0x20
#define	ONLINE		0x10
#define	ERROR		0x08
#define	STATUS_MASK	0xB8

/*
 * lp control register
 */
#define	INTR_ON		0x10
#define	SEL		0x08
#define	RESET		0x04
#define	AUTOLF		0x02
#define	STROBE		0x01

/* register offsets from base io address */
#define	LP_DATA		0
#define	LP_STATUS	1
#define	LP_CONTROL	2

/* States: */
#define	OPEN	0x01
#define	LPPRES	0x10    /* set if parallel adapter present */


/*
 * Structures for the LP
 */

struct lp_unit {
	kmutex_t	lp_lock;
	int		flag;		/* lp is configured in */
	unsigned char	lp_state;	/* previous interrupt status */
	dev_info_t	*lp_dip;
	unsigned	data;		/* data latch address */
	unsigned	status;		/* printer status address */
	unsigned	control;	/* printer controls address */
	ddi_iblock_cookie_t lp_iblock;
	time_t		last_time;	/* output char watchdog timer */
	timeout_id_t	lp_delay;	/* timeout id */
	timeout_id_t	lp_watchdog;	/* timeout id */
	struct strtty	lp_tty;		/* tty struct for this device */
};


/*
 * flags/masks for error printing.
 * the levels are for severity
 */
#define	LPEP_L0		0	/* chatty as can be - for debug! */
#define	LPEP_L1		1	/* best for debug */
#define	LPEP_L2		2	/* minor errors - retries, etc. */
#define	LPEP_L3		3	/* major errors */
#define	LPEP_L4		4	/* catastophic errors, don't mask! */
#define	LPEP_LMAX	4	/* catastophic errors, don't mask! */

#ifdef DEBUG
#define	LPERRPRINT(l, m, args)	\
	{ if (((l) >= lperrlevel) && ((m) & lperrmask)) prom_printf args; }
#else
#define	LPERRPRINT(l, m, args)	{ }
#endif /* DEBUG */

/*
 * for each function, we can mask off its printing by clearing its bit in
 * the lperrmask.  Some functions (_init, _info) share a mask bit
 */
#define	LPEM_IDEN 0x00000001	/* lp_identify */
#define	LPEM_PROB 0x00000002	/* lp_probe */
#define	LPEM_ATTA 0x00000004	/* lp_attach */
#define	LPEM_OPEN 0x00000010	/* lpopen */
#define	LPEM_CLOS 0x00000020	/* lpclose */
#define	LPEM_OPUT 0x00000100	/* lpoput */
#define	LPEM_GOBK 0x00000200	/* lpgetoblk */
#define	LPEM_FLSH 0x00000400	/* lpflush */
#define	LPEM_PROC 0x00001000	/* lpproc */
#define	LPEM_IOCT 0x00002000	/* lpioctl */
#define	LPEM_WATC 0x00040000	/* lpwatch */
#define	LPEM_INTR 0x00080000	/* lpintr */
#define	LPEM_MODS 0x08000000	/* _init, _info, _fini */
#define	LPEM_ALL  0xFFFFFFFF	/* all */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LP_H */
