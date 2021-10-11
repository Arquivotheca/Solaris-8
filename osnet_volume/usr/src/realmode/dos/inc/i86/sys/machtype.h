/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)machtype.h	1.6	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		machtype(s).h
 *
 *   Description:	Solaris x86 machine-dependent datatypes.
 *
 */
/*	Copyright (c) 1988 AT&T	*/
/*	  All rights reserved. 	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_MACHTYPES_H
#define	_SYS_MACHTYPES_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Machine dependent types:
 *
 *	i386 Version
 */

typedef struct  _physadr { long r[1]; } _FAR_ *physadr;

typedef	struct	_label_t { long	val[6]; } label_t; /* XXX - check? */

typedef	unsigned char	lock_t;		/* lock work for busy wait */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHTYPES_H */
