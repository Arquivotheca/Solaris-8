/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_LOG_EVENT_H
#define	_SYS_LOG_EVENT_H

#pragma ident	"@(#)devfs_log_event.h	1.2	99/05/28 SMI"


#ifdef	__cplusplus
extern "C" {
#endif

/*
 * used by devfseventd  and passed to kernel thru modclt
 * XXX these two defines should be in /etc/volatile when
 * it becomes avaiable for use.
 */
#define	LOGEVENT_DOOR_UPCALL	"/dev/.devfs_log_event"

/* used by libdevfsevent */
#define	LOGEVENT_DOOR_SERVICES	"/dev/.devfs_eventmgr"

#define	LOGEVENT_BUFSIZE	1012

/*
 * For the purposes of the upcall server, the contents of this structure
 * are read-only.
 * Based on the current implementation of door_return(), the result field
 * will be filled in by the doors framework based on arguments passed to
 * door_return() call in the server which should look something like:
 *
 *	int  result;
 *
 *      result = whatever;
 *
 *	door_return(&result, sizeof(result), NULL, 0);
 *
 * XXX: This is currently entirely implementation-dependent -- there is no
 *      actual spec. The first arg of door_return of size second arg will be
 *      written to the beginning of the buffer pointed to by the data_ptr
 *      param passed by the kernel upcall.
 */
typedef struct log_event_upcall_arg {
	int	result;
	char	buf[LOGEVENT_BUFSIZE];
} log_event_upcall_arg_t;


#define	TUPLES_CONNECTOR	"="
#define	TUPLES_SEPARATOR	";"
#define	MESSAGE_SEPARATOR	"|"

/*
 * Event attributes
 */
#define	LOGEVENT_TYPE	"event_type"
#define	LOGEVENT_CLASS	"event_class"
#define	LOGEVENT_LEVEL	"event_level"
/*
 * Event classes
 */
#define	EC_DEVFS		"devfs"
#define	EC_PCMCIA		"pcmcia"
#define	EC_FAULT		"fault"

/*
 * devfs event types
 */
#define	ET_DEVFS_MINOR_CREATE	"minor_create"
#define	ET_DEVFS_MINOR_REMOVE	"minor_remove"
#define	ET_DEVFS_DEVI_ADD	"devinfo_add"
#define	ET_DEVFS_DEVI_REMOVE	"devinfo_remove"
#define	ET_DEVFS_DEVI_UNBOUND	"devinfo_unbound"
#define	ET_DEVFS_INSTANCE_MOD	"instance_mod"

/*
 * devfs event class attributes
 */
#define	DEVFS_DRIVER_NAME	"di.driver"
#define	DEVFS_PATHNAME		"di.path"
#define	DEVFS_MINOR_NAME	"mi.name"
#define	DEVFS_MINOR_NODETYPE	"mi.nodetype"
#define	DEVFS_MINOR_ISCLONE	"mi.isclone"
#define	DEVFS_MINOR_MAJNUM	"mi.majorno"
#define	DEVFS_MINOR_MINORNUM	"mi.minorno"

/*
 * fault event types
 */
#define	ET_FAULT_REPORT		"report"

/*
 * fault event class attributes
 */
#define	FAULT_DRIVER_NAME	"driver"
#define	FAULT_IMPACT		"impact"
#define	FAULT_INSTANCE		"instance"
#define	FAULT_LOCATION		"location"
#define	FAULT_MESSAGE		"message"
#define	FAULT_NEWSTATE		"newstate"
#define	FAULT_OLDSTATE		"oldstate"
#define	FAULT_PATHNAME		"path"

/*
 * eventd service codes
 */
#define	EVENTD_REGISTER		1
#define	EVENTD_UNREGISTER	2
#define	EVENTD_GETMSG		3
#define	EVENTD_LOGMSG		4

typedef struct eventd_service_request {
	int	pid;
	int	event_id;
	int	service_code;
	int	retcode;
	char	buf[LOGEVENT_BUFSIZE];
} eventd_service_t;


typedef	struct log_event_tuple {
	char	*attr;
	char	*val;
} log_event_tuple_t;


#ifdef	_KERNEL

/*
 * kernel log_event interfaces.
 */

int i_ddi_log_event(int argc, log_event_tuple_t tuples[], int flag);
void log_event_init(void);
void i_ddi_log_event_flushq(int cmd, uint_t flag);
int i_ddi_log_event_filename(char *file, uint_t flag);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LOG_EVENT_H */
