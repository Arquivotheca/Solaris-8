/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */

/* The #ident directive confuses the DOS linker */
/*
#ident "@(#)stdarg.h	1.1	97/03/17 SMI"
*/

#ifndef _STDARG_H
#define	_STDARG_H

#ifndef	_VA_LIST_DEFINED
#define	_VA_LIST_DEFINED
typedef char *va_list;
#endif

#define	va_start(list, fmt) list = (((char *)(&fmt)) + sizeof (fmt))
#define	va_end(list)
#define	va_arg(list, mode) ((mode *)(list += sizeof (mode)))[-1]

#endif	/* _STDARG_H */
