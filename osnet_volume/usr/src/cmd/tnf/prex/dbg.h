/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _DBG_H
#define	_DBG_H

#pragma ident	"@(#)dbg.h	1.24	97/10/29 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
#define	DBG(x)		(x)
#else
#define	DBG(x)
#endif

#if defined(DEBUG) || defined(lint)
#include <stdio.h>
extern int	  __prb_verbose;
#endif

#ifdef __cplusplus
}
#endif

#endif /* _DBG_H */
