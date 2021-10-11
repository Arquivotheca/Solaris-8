/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * gettext.h -- public definitions for gettext module
 */

#ifndef	_GETTEXT_H
#define	_GETTEXT_H

#ident "@(#)gettext.h   1.5   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

void init_gettext(const char *progname);
const char *gettext(const char *message);

/*
 * Globals - common strings used multiple times
 */
extern char *Please_wait;
extern char *Auto_boot;
extern char *Auto_boot_cfg_num;
extern char *Auto_boot_timeout;

#ifdef	__cplusplus
}
#endif

#endif	/* _GETTEXT_H */
