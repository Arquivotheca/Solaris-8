/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)exception_list.h	1.1	99/01/11 SMI"

#if defined(sparc)
#define	EXCEPTION_FILE "/opt/onbld/etc/exception_list"
#elif defined(i386)
#define	EXCEPTION_FILE "/opt/onbld/etc/exception_list_i386"
#elif defined(__ppc)
#define	EXCEPTION_FILE "/opt/onbld/etc/exception_list_ppc"
#else
#error "Unknown instruction set"
#endif

extern int read_in_exceptions(char *, elem_list *, int);
