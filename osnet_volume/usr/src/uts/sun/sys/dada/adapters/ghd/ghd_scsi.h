/*
 * Copyright (c) 1996,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _GHD_SCSI_H
#define	_GHD_SCSI_H

#pragma ident	"@(#)ghd_scsi.h	1.7	98/04/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>

void		scsi_htos_3byte(uchar_t *ap, ulong_t nav);
void		scsi_htos_long(uchar_t *ap, ulong_t niv);
void		scsi_htos_short(uchar_t *ap, ushort_t nsv);
ulong_t		scsi_stoh_3byte(uchar_t *ap);
ulong_t		scsi_stoh_long(ulong_t ai);
ushort_t	scsi_stoh_short(ushort_t as);


#ifdef	__cplusplus
}
#endif

#endif /* _GHD_SCSI_H */
