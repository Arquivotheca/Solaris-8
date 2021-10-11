/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_MACHINE_H
#define	_MACHINE_H

#pragma ident	"@(#)machine.h	1.2	98/01/13 SMI"

/*
 * Copyrighted as an unpublished work.
 * (c) Copyright INTERACTIVE Systems Corporation 1986, 1988, 1990
 * All rights reserved.
 *
 * RESTRICTED RIGHTS
 *
 * These programs are supplied under a license.  They may be used,
 * disclosed, and/or copied only as permitted under such license
 * agreement.  Any copy must contain the above copyright notice and
 * this restricted rights notice.  Use, copying, and/or disclosure
 * of the programs is strictly prohibited unless otherwise provided
 * in the license agreement.
 */

#ifdef	__cplusplus
extern "C" {
#endif

struct machconfig {
	char		*sigaddr;	/* Machine signature location 	*/
	unsigned char	siglen;		/* Signature length 		*/
	unsigned char	sigid[10];	/* Signature to match 		*/
	unsigned char	old_mt;		/* OLD Machine type 		*/
	unsigned char	machine;	/* Machine type 		*/
	ulong		m_flag;		/* status flag			*/
	int		(*m_entry)();	/* machine entry point		*/
};

#define	M_FLG_SRGE	1	/* sig scattered in a range of memory	*/

#define	M_ID_AT386	0
#define	M_ID_MC386	1
#define	M_ID_EISA	2

#define	SYS_MODEL() 	*(char *)0xFFFFE
#define	MODEL_AT	(unchar)0xFC
#define	MODEL_MC	(unchar)0xF8
#define	USER_START	0x100000

#define	NPTEPERPT	1024
typedef struct ptbl {
	int page[NPTEPERPT];
} ptbl_t;

/* combine later with ../../../uts/i86/sys/pte.h */
#define	PG_P 0x1  	/* page is present */
#define	PG_RW 0x2  	/* page is read/write */
#define	PG_SIZE 0x80	/* page is 4MB */
#define	PG_GLOBAL 0x100	/* page is persistent */

/*
 * keyboard controller I/O port addresses
 */

#define	KB_OUT	0x60		/* output buffer R/O */
#define	KB_IDAT	0x60		/* input buffer data write W/O */
#define	KB_STAT	0x64		/* keyboard controller status R/O */
#define	KB_ICMD	0x64		/* input buffer command write W/O */

/*
 * keyboard controller commands and flags
 */
#define	KB_INBF		0x02	/* input buffer full flag */
#define	KB_OUTBF	0x01	/* output buffer full flag */
#define	KB_GATE20	0x02	/* set this bit to allow addresses > 1Mb */
#define	KB_ROP		0xD0	/* read output port command */
#define	KB_WOP		0xD1	/* write output port command */
#define	KB_RCB		0x20	/* read command byte command */
#define	KB_WCB		0x60	/* write command byte command */
#define	KB_ENAB		0xae	/* enable keyboard interface */
#define	KB_DISAB	0x10	/* disable keyboard */
#define	KB_EOBFI	0x01	/* enable interrupt on output buffer full */
#define	KB_ACK		0xFA	/* Acknowledgement byte from keyboard */
#define	KB_RESETCPU	0xFE	/* command to reset AT386 cpu */
#define	KB_READID	0xF2	/* command to read keyboard id */
#define	KB_RESEND	0xFE	/* response from keyboard to resend data */
#define	KB_ERROR	0xFF	/* response from keyboard to resend data */
#define	KB_RESET	0xFF	/* command to reset keyboard */
/*
 * command to to enable keyboard
 * this is different from KB_ENAB above in
 * that KB_ENAB is a command to the 8042 to
 * enable the keyboard interface, not the
 * keyboard itself
 */
#define	KB_ENABLE	0xF4

/* move later into immu.h */
#ifndef	PTSIZE
#define	PTSIZE 4096
#endif

#define	ptround(p)	((int *)(((int)p + PTSIZE-1) & ~(PTSIZE-1)))
#define	FOURMEG  4194304
#define	FOURMB_PTE (PG_P | PG_RW | PG_SIZE)

#ifdef	__cplusplus
}
#endif

#endif	/* _MACHINE_H */
