/*
 * Copyright (c) 1996, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)method.h 1.18	97/11/19  SMI"

/*
 * COPYRIGHT NOTICE
 *
 * This source code is designated as Restricted Confidential Information
 * and is subject to special restrictions in a confidential disclosure
 * agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
 * this source code outside your company without OSF's specific written
 * approval.  This source code, and all copies and derivative works
 * thereof, must be returned or destroyed at request. You must retain
 * this notice on any copies which you make.
 *
 * (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED
 */
/*
 * OSF/1 1.2
 */
/* @(#)$RCSfile: method.h,v $ $Revision: 1.1.2.8 $ */
/* (OSF) $Date: 1992/03/17 22:14:36 $ */
/*
 * COMPONENT_NAME: (CMDLOC) Locale Database Commands
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.1  com/inc/sys/method.h, libcloc, bos320, 9130320 7/17/91 17:25:11
 */
#ifndef __H_METHOD
#define	__H_METHOD

/* To make available the definition of _LP64 and _ILP32 */
#include <sys/types.h>

#if defined(__sparc)
#define	MACH64	"sparcv9/"
#else
#define	MACH64	"ia64/"
#endif

#define	DEFAULT_METHOD_DIR	"/usr/lib/"
#define	DEFAULT_METHOD_NAM	"libc.so.1"

#define	DEFAULT_METHOD		DEFAULT_METHOD_DIR DEFAULT_METHOD_NAM
#define	DEFAULT_METHOD64	DEFAULT_METHOD_DIR MACH64 DEFAULT_METHOD_NAM


#define	MAX_METHOD_NAME   32

#define	CHARMAP_MBFTOWC	0x00
#define	CHARMAP_FGETWC	0x01
#define	CHARMAP_EUCPCTOWC	0x02
#define	CHARMAP_WCTOEUCPC	0x03
#define	CHARMAP_CHARMAP_INIT	0x04
#define	CHARMAP_CHARMAP_DESTRUCTOR	0x05
#define	CHARMAP_MBLEN	0x06
#define	CHARMAP_MBSTOWCS	0x07
#define	CHARMAP_MBTOWC	0x08
#define	CHARMAP_NL_LANGINFO	0x09
/*
 * Gap 0x0a
 */
#define	CHARMAP_WCSTOMBS	0x0b
#define	CHARMAP_WCSWIDTH	0x0c
#define	CHARMAP_WCTOMB	0x0d
#define	CHARMAP_WCWIDTH	0x0e
#define	CHARMAP_FGETWC_AT_NATIVE	0x40
#define	CHARMAP_MBFTOWC_AT_NATIVE	0x42
#define	CHARMAP_MBSTOWCS_AT_NATIVE	0x43
#define	CHARMAP_MBTOWC_AT_NATIVE	0x44
#define	CHARMAP_WCTOMB_AT_NATIVE	0x4a
#define	CHARMAP_WCSTOMBS_AT_NATIVE	0x48
#define	CHARMAP_WCSWIDTH_AT_NATIVE	0x4b
#define	CHARMAP_WCWIDTH_AT_NATIVE	0x4c
/*
 * MSE methods
 */
#define	CHARMAP_BTOWC	0x4e
#define	CHARMAP_WCTOB	0x4f
#define	CHARMAP_MBSINIT	0x50
#define	CHARMAP_MBRLEN	0x51
#define	CHARMAP_MBRTOWC	0x52
#define	CHARMAP_WCRTOMB	0x53
#define	CHARMAP_MBSRTOWCS	0x54
#define	CHARMAP_WCSRTOMBS	0x55
#define	CHARMAP_BTOWC_AT_NATIVE	0x56
#define	CHARMAP_WCTOB_AT_NATIVE	0x57
#define	CHARMAP_MBRTOWC_AT_NATIVE	0x58
#define	CHARMAP_WCRTOMB_AT_NATIVE	0x59
#define	CHARMAP_MBSRTOWCS_AT_NATIVE	0x5a
#define	CHARMAP_WCSRTOMBS_AT_NATIVE	0x5b

#define	COLLATE_FNMATCH	0x0f
#define	COLLATE_REGCOMP	0x10
#define	COLLATE_REGERROR	0x11
#define	COLLATE_REGEXEC	0x12
#define	COLLATE_REGFREE	0x13
#define	COLLATE_STRCOLL	0x14
#define	COLLATE_STRXFRM	0x15
#define	COLLATE_WCSCOLL	0x16
#define	COLLATE_WCSXFRM	0x17
#define	COLLATE_COLLATE_INIT	0x18
#define	COLLATE_COLLATE_DESTRUCTOR	0x3c
#define	COLLATE_WCSCOLL_AT_NATIVE	0x47
#define	COLLATE_WCSXFRM_AT_NATIVE	0x49
#define	CTYPE_WCTYPE	0x19
#define	CTYPE_CTYPE_INIT	0x1a
#define	CTYPE_CTYPE_DESTRUCTOR	0x36
#define	CTYPE_ISWCTYPE	0x1b
#define	CTYPE_TOWLOWER	0x1c
#define	CTYPE_TOWUPPER	0x1d
#define	CTYPE_TRWCTYPE	0x3d
#define	CTYPE_WCTRANS	0x3e
#define	CTYPE_TOWCTRANS	0x3f
#define	CTYPE_TOWCTRANS_AT_NATIVE	0x4d
#define	CTYPE_ISWCTYPE_AT_NATIVE	0x41
#define	CTYPE_TOWLOWER_AT_NATIVE	0x45
#define	CTYPE_TOWUPPER_AT_NATIVE	0x46
#define	LOCALE_LOCALE_INIT	0x1e
#define	LOCALE_LOCALE_DESTRUCTOR	0x37
#define	LOCALE_LOCALECONV	0x1f
#define	LOCALE_NL_LANGINFO	0x20
#define	MONETARY_MONETARY_INIT	0x21
#define	MONETARY_MONETARY_DESTRUCTOR	0x38
#define	MONETARY_NL_LANGINFO	0x22
#define	MONETARY_STRFMON	0x23
/*
 * Gap 0x24..0x2a
 */
#define	NUMERIC_NUMERIC_INIT	0x2b
#define	NUMERIC_NUMERIC_DESTRUCTOR	0x39
#define	NUMERIC_NL_LANGINFO	0x2c
#define	RESP_RESP_INIT	0x2d
#define	RESP_RESP_DESTRUCTOR	0x3a
#define	RESP_NL_LANGINFO	0x2e
#define	RESP_RPMATCH	0x2f
#define	TIME_TIME_INIT	0x30
#define	TIME_TIME_DESTRUCTOR	0x3b
#define	TIME_NL_LANGINFO	0x31
#define	TIME_STRFTIME	0x32
#define	TIME_STRPTIME	0x33
#define	TIME_WCSFTIME	0x34
#define	TIME_GETDATE	0x35

#define	LAST_METHOD	0x5b


#define	SB_CODESET	0
#define	MB_CODESET	1
#define	USR_CODESET	2

#define	MX_METHOD_CLASS	3

typedef struct {
	char *method_name;			/* CLASS.component notation */
	int (*instance[MX_METHOD_CLASS])(void);	/* Entrypoint Address */
	char *c_symbol[MX_METHOD_CLASS];	/* Entrypoint (function name) */
	char *package[MX_METHOD_CLASS];	/* Package name */
	char *lib_name[MX_METHOD_CLASS];	/* library name */
	char *lib64_name[MX_METHOD_CLASS]; /* 64-bit library name */

	char *meth_proto;	/* Required calling conventions */
} method_t;

typedef struct {
	char	*library;
	char	*library64;
} library_t;

extern method_t *std_methods;
extern int method_class;	/* Controls which family of methods is used */

extern int mb_cur_max;

#define	METH_PROTO(m)	(std_methods[m].meth_proto)
#define	METH_NAME(m)    (std_methods[m].c_symbol[method_class])
#define	METH_OFFS(m)    (std_methods[m].instance[method_class])

#endif  /* __H_METHOD */
