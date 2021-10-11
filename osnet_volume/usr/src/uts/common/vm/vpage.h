/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	Copyright (c) 1986-1990,1997-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef	_VM_VPAGE_H
#define	_VM_VPAGE_H

#pragma ident	"@(#)vpage.h	1.21	98/01/06 SMI"
/*	From:	SVr4.0	"kernel:vm/vpage.h	1.5"		*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * VM - Information per virtual page.
 */
struct vpage {
	uchar_t nvp_prot;	/* see <sys/mman.h> prot flags */
	uchar_t nvp_advice;	/* pplock & <sys/mman.h> madvise flags */
};

/*
 * This was changed from a bitfield to flags/macros in order
 * to conserve space (uchar_t bitfields are not ANSI).  This could
 * have been condensed to a uchar_t, but at the expense of complexity.
 * We've stolen a bit from the top of nvp_advice to store pplock in.
 *
 * WARNING: VPP_SETADVICE(vpp, x) evaluates vpp twice, and VPP_PLOCK(vpp)
 * returns a positive integer when the lock is held, not necessarily (1).
 */
#define	VP_ADVICE_MASK	(0x07)
#define	VP_PPLOCK_MASK	(0x80)	/* physical page locked by me */
#define	VP_PPLOCK_SHIFT	(0x07)	/* offset of lock hiding inside nvp_advice */

#define	VPP_PROT(vpp)	((vpp)->nvp_prot)
#define	VPP_ADVICE(vpp)	((vpp)->nvp_advice & VP_ADVICE_MASK)
#define	VPP_ISPPLOCK(vpp) \
	((uchar_t)((vpp)->nvp_advice & VP_PPLOCK_MASK))

#define	VPP_SETPROT(vpp, x)	((vpp)->nvp_prot = (x))
#define	VPP_SETADVICE(vpp, x) \
	((vpp)->nvp_advice = ((vpp)->nvp_advice & ~VP_ADVICE_MASK) | \
		((x) & VP_ADVICE_MASK))
#define	VPP_SETPPLOCK(vpp)	((vpp)->nvp_advice |= VP_PPLOCK_MASK)
#define	VPP_CLRPPLOCK(vpp)	((vpp)->nvp_advice &= ~VP_PPLOCK_MASK)

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_VPAGE_H */
