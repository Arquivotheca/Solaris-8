/*	Copyright 1991 Sun Microsystems Inc. */

#ident	"@(#)pkglocale.h	1.5	93/03/09 SMI"

#include <locale.h>
#include <libintl.h>

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
#ifdef lint
#define	pkg_gt(x)	x
#else	/* !lint */
#define	pkg_gt(x)	dgettext(TEXT_DOMAIN, x)
#endif	/* lint */
