/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SCSI_SCSI_WATCH_H
#define	_SYS_SCSI_SCSI_WATCH_H

#pragma ident	"@(#)scsi_watch.h	1.8	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct scsi_watch_result {
	struct scsi_status		*statusp;
	struct scsi_extended_sense	*sensep;
	uchar_t				actual_sense_length;
	struct scsi_pkt			*pkt;
};

/*
 * 120 seconds is a *very* reasonable amount of time for most slow devices
 */
#define	SCSI_WATCH_IO_TIME	120

/*
 * values to pass in "flags" arg for scsi_watch_request_terminate()
 */
#define	SCSI_WATCH_TERMINATE_WAIT	0x0
#define	SCSI_WATCH_TERMINATE_NOWAIT	0x1

#define	SCSI_WATCH_TERMINATE_SUCCESS	0x0
#define	SCSI_WATCH_TERMINATE_FAIL	0x1

void scsi_watch_init();
void scsi_watch_fini();

#ifdef	__STDC__

opaque_t scsi_watch_request_submit(struct scsi_device *devp,
    int interval, int sense_length, int (*callback)(), caddr_t cb_arg);
int scsi_watch_request_terminate(opaque_t token, int flags);
void scsi_watch_resume(opaque_t token);
void scsi_watch_suspend(opaque_t token);

#else	/* __STDC__ */

opaque_t scsi_watch_request_submit();
void scsi_watch_request_terminate();
void scsi_watch_resume();
void scsi_watch_suspend();

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_SCSI_WATCH_H */
