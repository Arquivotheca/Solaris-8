/*
 *  Copyright (c) 1997 Sun Microsystems, Inc.
 *  All Rights Reserved
 *
 *  dostypes.h  -- Type definitions for Microsoft C compiler:
 *
 *	These are normally provided by the <windows.h> include file, but
 *	since we're not running under windows, <windows.h> may not exist.
 *	And, of course, it doesn't exist under UNIX either.
 */

#ifndef	_DOSTYPES_H
#define	_DOSTYPES_H

#ident	"<@(#)dostypes.h	1.8	97/04/08 SMI>"

#ifdef unix		/* For unix systems, "far" keyword is ignored!	*/
#define	far
#define	cdecl
#define	interrupt

#else			/* But it has meaning under DOS!		*/
#define	far __far
#define	cdecl __cdecl
#define	interrupt __interrupt

/*
 *  We need the "far" version of the va_list definition to get printf variants
 *  to work with realmode drivers.
 */

#ifndef	_VA_LIST_DEFINED
typedef char far *va_list;
#define	_VA_LIST_DEFINED
#endif
#endif

typedef unsigned char 	BYTE;
typedef unsigned short	WORD;
typedef unsigned long	DWORD;

#define	export
#define	TRUE		1
#define	FALSE		0
#endif
