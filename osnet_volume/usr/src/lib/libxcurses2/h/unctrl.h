/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_UNCTRL_H
#define	_UNCTRL_H

#pragma ident	"@(#)unctrl.h 1.4	98/08/08 SMI"

/*
 * unctrl.h
 *
 * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 *
 * $Header: /rd/src/libc/xcurses/rcs/unctrl.h 1.2 1995/05/25 17:57:16 ant Exp $
 */

#include <sys/isa_defs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_CHTYPE
#define	_CHTYPE
#if defined(_LP64)
typedef unsigned int	chtype;
#else
typedef unsigned long	chtype;
#endif
#endif

extern char *unctrl(chtype);

#ifdef	__cplusplus
}
#endif

#endif	/* _UNCTRL_H */
