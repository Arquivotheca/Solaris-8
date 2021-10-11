/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident   "@(#)ns_sunw.h 1.5     98/05/13 SMI"

#ifndef _NS_SUNW_H
#define _NS_SUNW_H

#ifdef SUNW_LOGLEVEL
#include <stdarg.h>

#ifdef __DEFINE_LOGLEVEL
#include <syslog.h>
int             loglevel = LOG_NOTICE;
#else
extern int      loglevel;
#define syslog  __named_syslog
#endif

void    __named_syslog(int, const char *, ...);
#endif /* SUNW_LOGLEVEL */

#ifdef SUNW_OPENFDOFFSET
#ifdef __DEFINE_OPEN_FD_OFFSET
int             open_fd_offset = 20;
#else
extern int      open_fd_offset;
#endif

int     __named_dup_fd_offset(int fd);
#endif /* SUNW_OPENFDOFFSET */

#endif /* _NS_SUNW_H */

