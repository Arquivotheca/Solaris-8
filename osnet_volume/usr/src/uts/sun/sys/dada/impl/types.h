/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DADA_IMPL_TYPES_H
#define	_SYS_DADA_IMPL_TYPES_H

#pragma ident	"@(#)types.h	1.5	99/04/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * local types for DADA subsystems
 */

#ifdef _KERNEL

#include	<sys/kmem.h>
#include	<sys/open.h>
#include	<sys/uio.h>
#include	<sys/sysmacros.h>

#include	<sys/buf.h>
#include	<sys/errno.h>
#include	<sys/fcntl.h>
#include	<sys/ioctl.h>

#include	<sys/conf.h>

#include	<sys/dada/impl/services.h>
#include	<sys/dada/impl/transport.h>

#include	<sys/dada/impl/commands.h>
#include	<sys/dada/impl/status.h>

#endif	/* _KERNEL */
#include 	<sys/dada/impl/udcd.h>


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DADA_IMPL_TYPES_H */
