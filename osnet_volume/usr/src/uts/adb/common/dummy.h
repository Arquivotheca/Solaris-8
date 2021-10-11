/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _DUMMY_H
#define	_DUMMY_H

#pragma ident	"@(#)dummy.h	1.2	98/04/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Dummy structure for macrogen to determine whether it's operating in
 * LP64 mode or ILP32 mode.
 *
 * No need for any filler after int_val, 'cos we're not concerned about
 * the structure size
 */
struct __dummy {
	long	long_val;
	char	*ptr_val;
	int	int_val;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _DUMMY_H */
