/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNATTR_UTILS_HH
#define	_FNATTR_UTILS_HH

#pragma ident	"@(#)fnattr_utils.hh	1.1	96/04/05 SMI"


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <xfn/xfn.hh>


extern char *
convert_to_char(int length, const void *);

extern void
print_attribute(const FN_attribute *, FILE * = stdout);

extern void
print_attrset(const FN_attrset *, FILE * = stdout);


#endif	/* _FNATTR_UTILS_HH */
