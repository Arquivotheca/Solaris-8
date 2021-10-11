/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_DHCPMSG_H
#define	_DHCPMSG_H

#pragma ident	"@(#)dhcpmsg.h	1.2	99/07/26 SMI"

#include <sys/types.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>		/* since consumers may want to 0 errno */

/*
 * dhcpmsg.[ch] comprise the interface used to log messages, either to
 * syslog(3C), or to the screen, depending on the debug level.  see
 * dhcpmsg.c for documentation on how to use the exported functions.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * the syslog levels, while useful, do not provide enough flexibility
 * to do everything we want.  consequently, we introduce another set
 * of levels, which map to a syslog level, but also potentially add
 * additional behavior.
 */

enum {
	MSG_DEBUG,		/* LOG_DEBUG, only if debug_level is 1 */
	MSG_DEBUG2,		/* LOG_DEBUG, only if debug_level is 1 or 2 */
	MSG_INFO,		/* LOG_INFO */
	MSG_VERBOSE,		/* LOG_INFO, only if is_verbose is true */
	MSG_NOTICE,		/* LOG_NOTICE */
	MSG_WARNING,		/* LOG_WARNING */
	MSG_ERR,		/* LOG_ERR, use errno if nonzero */
	MSG_ERROR,		/* LOG_ERR */
	MSG_CRIT		/* LOG_CRIT */
};

extern void	dhcpmsg(int, const char *, ...);
extern void	dhcpmsg_init(const char *, boolean_t, boolean_t, int);
extern void	dhcpmsg_fini(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _DHCPMSG_H */
