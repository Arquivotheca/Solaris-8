/*
 * Copyright (c) 1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LIBC_PORT_I18N_LOCPATH_H
#define	_LIBC_PORT_I18N_LOCPATH_H

#pragma ident	"@(#)_loc_path.h	1.5	99/05/19 SMI"

#include <sys/isa_defs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	_DFLT_LOC_PATH	"/usr/lib/locale/"

#define	_ICONV_PATH1	"/usr/lib/iconv/"
#define	_ICONV_PATH2	"%s%%%s.so"
#define	_WDMOD_PATH1	"/LC_CTYPE/"
#define	_WDMOD_PATH2	"wdresolve.so"
#define	_GENICONVTBL_PATH1	"geniconvtbl/binarytables/%s"
#define	_GENICONVTBL_FILE	"%s%%%s.bt"
#define	_GENICONVTBL_INT_PATH1	"geniconvtbl.so"

#ifdef _LP64

#if defined(__sparcv9)

#define	_MACH64_NAME		"sparcv9"

#elif defined(__ia64)

#define	_MACH64_NAME		"ia64"

#else  /* !defined(__sparcv9) */

#error "Unknown architecture"

#endif /* defined(__sparcv9) */

#define	_MACH64_NAME_LEN	(sizeof (_MACH64_NAME) - 1)

#define	_ICONV_PATH	_ICONV_PATH1 _MACH64_NAME "/" _ICONV_PATH2
#define	_WDMOD_PATH	_WDMOD_PATH1 _MACH64_NAME "/" _WDMOD_PATH2
#define	_GENICONVTBL_PATH	_ICONV_PATH1 _GENICONVTBL_PATH1
#define	_GENICONVTBL_INT_PATH	_ICONV_PATH1 \
			_MACH64_NAME "/" _GENICONVTBL_INT_PATH1

#else  /* !LP64 */

#define	_ICONV_PATH	_ICONV_PATH1 _ICONV_PATH2
#define	_WDMOD_PATH	_WDMOD_PATH1 _WDMOD_PATH2
#define	_GENICONVTBL_PATH	_ICONV_PATH1 _GENICONVTBL_PATH1
#define	_GENICONVTBL_INT_PATH	_ICONV_PATH1 _GENICONVTBL_INT_PATH1

#endif /* _LP64 */

#ifdef	__cplusplus
}
#endif

#endif	/* !_LIBC_PORT_I18N_LOCPATH_H */
