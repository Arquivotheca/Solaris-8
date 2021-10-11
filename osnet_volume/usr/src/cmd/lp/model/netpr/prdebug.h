/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _PRDEBUG_H
#define	_PRDEBUG_H

#pragma ident	"@(#)prdebug.h	1.3	98/07/10 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	NETPRDFILE	"/tmp/netpr.debug"

#define	NETDB(fd, msg)	\
	{	\
		fprintf(fd,	\
		"%s: line:%d  %s\n", __FILE__,  __LINE__, msg);	\
		fflush(fd); \
		fsync(fd); \
	}


#define	_SZ_BUFF	100

#ifdef __cplusplus
}
#endif

#endif /* _PRDEBUG_H */
