/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * PCMCIA PC Card client driver master header file
 *
 * All PC Card client drivers must include this header file
 */

#ifndef _PCCARD_H
#define	_PCCARD_H

#pragma ident	"@(#)pccard.h	1.3	96/04/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kmem.h>

#ifdef	_KERNEL
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/devops.h>
#endif	/* _KERNEL */

#include <sys/dditypes.h>
#include <sys/modctl.h>

#include <sys/pctypes.h>
#include <sys/cs_types.h>
#include <sys/cis.h>
#include <sys/cis_handlers.h>
#include <sys/cs.h>

#ifdef	__cplusplus
}
#endif

#endif	/* _PCCARD_H */
