/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_CONF_H
#define	_MDB_CONF_H

#pragma ident	"@(#)mdb_conf.h	1.1	99/08/11 SMI"

#include <sys/utsname.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _MDB

extern const char *mdb_conf_version(void);
extern const char *mdb_conf_platform(void);
extern const char *mdb_conf_isa(void);
extern void mdb_conf_uname(struct utsname *);

#endif /* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_CONF_H */
