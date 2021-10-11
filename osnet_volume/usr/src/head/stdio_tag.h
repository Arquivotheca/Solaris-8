/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _STDIO_TAG_H
#define	_STDIO_TAG_H

#pragma ident	"@(#)stdio_tag.h	1.3	98/04/20 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	__FILE_TAG
#if	defined(__cplusplus) && (__cplusplus < 54321L)
#define	__FILE_TAG	FILE
#else
#define	__FILE_TAG	__FILE
#endif
typedef struct __FILE_TAG __FILE;
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _STDIO_TAG_H */
