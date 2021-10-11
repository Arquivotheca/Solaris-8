/*
 *  "@(#)pam_loc.h 1.4 96/02/02
 *
 *  Copyright 1996 Sun Microsystems, Inc.
 *
 *  All Rights reserved
 */

#ifndef _PAM_LOC_H
#define	_PAM_LOC_H

#pragma	ident	"@(#)pam_loc.h	1.2	96/04/09 SMI"	/* PAM 2.6 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PAM_MSG macro for return of internationalized text
 */

#define	PAM_MSG(pamh, number, string)\
	(char *) __pam_get_i18n_msg(pamh, "libpam", 2, number, string)

#ifdef __cplusplus
}
#endif

#endif	/* _PAM_LOC_H */
