/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

#ifndef CSH_SIGNAL_H
#define CSH_SIGNAL_H

#ident	"@(#)signal.h	1.2	96/05/17 SMI"	/* SVr4.0 1.2	*/

/*
 * 4.3BSD signal compatibility header
 *
 */
#define sigmask(m)	(m > 32 ? 0 : (1 << ((m)-1)))

/*
 * 4.3BSD signal vector structure used in sigvec call.
 */
struct  sigvec {
        void    (*sv_handler)();        /* signal handler */
        int     sv_mask;                /* signal mask to apply */
        int     sv_flags;               /* see signal options below */
};

#define SV_ONSTACK      0x0001  /* take signal on signal stack */
#define SV_INTERRUPT    0x0002  /* do not restart system on signal return */
#define SV_RESETHAND    0x0004  /* reset handler to SIG_DFL when signal taken */

#define sv_onstack sv_flags

/*
 * Machine dependent deta structure
 */
struct  sigcontext {
        int     sc_onstack;             /* sigstack state to restore */
        int     sc_mask;                /* signal mask to restore */
	int	sc_sp;			/* sp to restore */
	int	sc_pc;			/* pc to retore */
	int	sc_ps;			/* psl to restore */
	int	sc_eax;			/* eax to restore */
	int	sc_edx;			/* edx to restore */
};

#define SI_DFLCODE	1

#define BUS_HWERR	BUS_ADRERR	/* misc hardware error (e.g. timeout) */
#define BUS_ALIGN	BUS_ADRALN	/* hardware alignment error */

#define SEGV_NOMAP	SEGV_MAPERR	/* no mapping at the fault address */
#define SEGV_PROT	SEGV_ACCERR	/* access exceeded protections */

/*
 * The SEGV_CODE(code) will be SEGV_NOMAP, SEGV_PROT, or SEGV_OBJERR.
 * In the SEGV_OBJERR case, doing a SEGV_ERRNO(code) gives an errno value
 * reported by the underlying file object mapped at the fault address.
 */

#define SIG_NOADDR	((char *)~0)

#define	SEGV_MAKE_ERR(e) (((e) << 8) | SEGV_MAPERR)

#endif	/* CSH_SIGNAL_H */
