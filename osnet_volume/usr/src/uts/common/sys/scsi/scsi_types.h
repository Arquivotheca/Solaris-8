/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SCSI_SCSI_TYPES_H
#define	_SYS_SCSI_SCSI_TYPES_H

#pragma ident	"@(#)scsi_types.h	1.17	96/10/15 SMI"

/*
 * Types for SCSI subsystems.
 *
 * This file picks up specific as well as generic type
 * defines, and also serves as a wrapper for many common
 * includes.
 */

#include <sys/types.h>
#include <sys/param.h>


#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	__STDC__
typedef	void *opaque_t;
#else	/* __STDC__ */
typedef	char *opaque_t;
#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#ifdef	_KERNEL
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/devops.h>
#endif	/* _KERNEL */

/*
 * Each implementation will have it's own specific set
 * of types it wishes to define.
 */

/*
 * Generally useful files to include
 */

#include <sys/scsi/scsi_params.h>
#include <sys/scsi/scsi_address.h>
#include <sys/scsi/scsi_pkt.h>
#ifdef	_KERNEL
#include <sys/scsi/conf/device.h>
#endif	/* _KERNEL */
#include <sys/scsi/scsi_ctl.h>
#include <sys/scsi/scsi_resource.h>

#ifdef	_KERNEL
#include <sys/scsi/conf/autoconf.h>
#include <sys/scsi/scsi_watch.h>
#endif	/* _KERNEL */

#include <sys/scsi/generic/commands.h>
#include <sys/scsi/generic/status.h>
#include <sys/scsi/generic/message.h>
#include <sys/scsi/generic/mode.h>


/*
 * Sun SCSI type definitions
 */

#include <sys/scsi/impl/types.h>

/*
 * For drivers which do not include these - must be last
 */
#ifdef	_KERNEL
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/stat.h>
#include <sys/sunndi.h>
#include <sys/devctl.h>
#endif	/* _KERNEL */

#endif	/* _SYS_SCSI_SCSI_TYPES_H */
