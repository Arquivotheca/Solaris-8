/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SCSI_RESET_NOTIFY_H
#define	_SYS_SCSI_RESET_NOTIFY_H

#pragma ident	"@(#)scsi_reset_notify.h	1.1	98/10/11 SMI"

#include <sys/note.h>
#include <sys/scsi/scsi_types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * SCSI Control Information for Reset Notification.
 */

/*
 * adapter drivers use the following structure to record the notification
 * requests from target drivers.
 */
struct scsi_reset_notify_entry {
	struct scsi_address		*ap;
	void				(*callback)(caddr_t);
	caddr_t				arg;
	struct scsi_reset_notify_entry	*next;
};

#ifdef __lock_lint
_NOTE(SCHEME_PROTECTS_DATA("protected by lock passed as arg",
	scsi_reset_notify_entry::ap
	scsi_reset_notify_entry::callback
	scsi_reset_notify_entry::arg
	scsi_reset_notify_entry::next))
#endif

#ifdef	_KERNEL
#ifdef	__STDC__
extern int scsi_hba_reset_notify_setup(struct scsi_address *, int,
	void (*)(caddr_t), caddr_t, kmutex_t *,
	struct scsi_reset_notify_entry **);
extern void scsi_hba_reset_notify_tear_down(
	struct scsi_reset_notify_entry *listp);
extern void scsi_hba_reset_notify_callback(kmutex_t *mutex,
	struct scsi_reset_notify_entry **listp);
#else	/* __STDC__ */
extern int scsi_hba_reset_notify_setup();
extern void scsi_hba_reset_notify_tear_down();
extern void scsi_hba_reset_notify_callback();
#endif	/* __STDC__ */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_RESET_NOTIFY_H */
