/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_USBA_TYPES_H
#define	_SYS_USBA_TYPES_H

#pragma ident	"@(#)usba_types.h	1.1	98/10/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/var.h>
#include <sys/vtrace.h>
#include <sys/sunndi.h>

/*
 * Data structure for maintaining lists
 * This data structure private to USBA and not exposed to HCD or client
 * driver or hub driver
 */
typedef struct list_entry {
	struct list_entry	*next;
	struct list_entry	*prev;
	kmutex_t		list_mutex;
} usba_list_entry_t;

_NOTE(MUTEX_PROTECTS_DATA(list_entry::list_mutex, list_entry))

/*
 * Private USBA interface files
 */
#include <sys/usb/usba/genconsole.h>
#include <sys/usb/usba/hcdi.h>
#include <sys/usb/usba/hubdi.h>

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_USBA_TYPES_H */
