/*
 * Copyright (c) 1993 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_VUID_MOUSE_H
#define	_SYS_VUID_MOUSE_H

#pragma ident	"@(#)vuidmice.h	1.7	99/03/10 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if _KERNEL
struct MouseStateInfo {
	unsigned long	last_event_lbolt;
	uchar_t		format;
	uchar_t		state;

	uchar_t		buttons;		/* current button state */
	int		deltax;			/* delta X value */
	int		deltay;			/* delta Y value */
	uchar_t		oldbuttons;		/* previous button state */
	uchar_t		sync_byte;
	uchar_t		inited;
	uchar_t		nbuttons;
};

#define	STATEP		((struct MouseStateInfo *)qp->q_ptr)

#ifdef	VUIDM3P
#define	VUID_NAME		"vuidm3p"
#define	VUID_PUTNEXT		vuidm3p_putnext
#define	VUID_QUEUE		vuidm3p
#define	VUID_OPEN		vuidm3p_open
#endif

#ifdef	VUIDM4P
#define	VUID_NAME		"vuidm4p"
#define	VUID_PUTNEXT		vuidm4p_putnext
#define	VUID_QUEUE		vuidm4p
#define	VUID_OPEN		vuidm4p_open
#endif

#ifdef	VUIDM5P
#define	VUID_NAME		"vuidm5p"
#define	VUID_PUTNEXT		vuidm5p_putnext
#define	VUID_QUEUE		vuidm5p
#define	VUID_OPEN		vuidm5p_open
#endif

#ifdef	VUID2PS2
#define	VUID_NAME		"vuid2ps2"
#define	VUID_PUTNEXT		vuid2ps2_putnext
#define	VUID_QUEUE		vuid2ps2
#define	VUID_OPEN		vuid2ps2_open
#endif

#ifdef	VUID3PS2
#define	VUID_NAME		"vuid3ps2"
#define	VUID_PUTNEXT		vuid3ps2_putnext
#define	VUID_QUEUE		vuid3ps2
#define	VUID_OPEN		vuid3ps2_open
#endif

#ifdef	VUIDPS2
#define	VUID_NAME		"vuidps2"
#define	VUID_PUTNEXT		vuidps2_putnext
#define	VUID_QUEUE		vuidps2
#define	VUID_OPEN		vuidps2_open
#endif

#ifndef	VUID_NAME
#define	VUID_NAME		"vuidmice"
#endif

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_VUID_MOUSE_H */
