/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_STRUCT_RREC_COMPAT_H
#define	_STRUCT_RREC_COMPAT_H

#pragma ident	"@(#)struct_rrec_compat.h	1.1	99/01/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct rrec {
	short	r_zone;			/* zone number */
	short	r_class;		/* class number */
	short	r_type;			/* type number */
	ulong_t	r_ttl;			/* time to live */
	int	r_size;			/* size of data area */
	char	*r_data;		/* pointer to data */
};

#ifdef	__cplusplus
}
#endif

#endif	/* _STRUCT_RREC_COMPAT_H */
