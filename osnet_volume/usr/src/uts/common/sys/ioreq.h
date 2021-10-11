/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef _SYS_IOREQ_H
#define	_SYS_IOREQ_H

#pragma ident	"@(#)ioreq.h	1.6	92/07/14 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The ioreq enables data structures to package several i/o requests
 * into one system call. It is used by the read/write+offset interface.
 */
typedef struct ioreq {
	caddr_t	ior_base;	/* buffer addr */
	int	ior_len;	/* buffer length */
	offset_t ior_offset;	/* file offset */
	int	ior_whence;
	int	ior_errno;
	int	ior_return;
	int	ior_flag;
} ioreq_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_IOREQ_H */
