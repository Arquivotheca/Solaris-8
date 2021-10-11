/*
 * Copyright (c) 1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DKTP_SCDKWATCH_H
#define	_SYS_DKTP_SCDKWATCH_H

#pragma ident	"@(#)scdkwatch.h	1.6	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct scdk_watch_result {
	struct scsi_status		*statusp;
	struct scsi_extended_sense	*sensep;
	uchar_t				actual_sense_length;
	struct scsi_pkt			*pkt;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_SCDKWATCH_H */
