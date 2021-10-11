/*
 * Copyright (c) 1994,1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_IVINTR_H
#define	_SYS_IVINTR_H

#pragma ident	"@(#)ivintr.h	1.17	99/07/01 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef uint_t (*intrfunc)(caddr_t);

/*
 * Interrupt Vector Table Entry
 *
 *	The interrupt vector table is dynamically allocated during
 *	startup. An interrupt number is an index to the interrupt
 *	vector table representing unique interrupt source to the system.
 */
struct intr_vector {
	intrfunc	iv_handler;	/* interrupt handler */
	caddr_t		iv_arg;		/* interrupt argument */
	ushort_t	iv_pil;		/* interrupt request level */
	ushort_t	iv_pending;	/* pending softint flag */
	void		*iv_pad;	/* makes structure power-of-2 size */
};

extern struct intr_vector intr_vector[];

extern void add_ivintr(uint_t, uint_t, intrfunc, caddr_t);
extern void rem_ivintr(uint_t, struct intr_vector *);
extern uint_t add_softintr(uint_t, intrfunc, caddr_t);
extern void rem_softintr(uint_t);

/* Global lock which protects the interrupt distribution lists */
extern kmutex_t intr_dist_lock;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_IVINTR_H */
