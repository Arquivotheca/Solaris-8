
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef	_SCSI_COM_H
#define	_SCSI_COM_H

#pragma ident	"@(#)scsi_com.h	1.4	93/03/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Common definitions for SCSI routines.
 */

/*
 * Possible error levels.
 */
#define	ERRLVL_COR	1	/* corrected error */
#define	ERRLVL_RETRY	2	/* retryable error */
#define	ERRLVL_FAULT	3	/* drive faulted */
#define	ERRLVL_FATAL	4	/* fatal error */

#ifdef	__cplusplus
}
#endif

#endif	/* _SCSI_COM_H */
