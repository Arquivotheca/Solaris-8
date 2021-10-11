/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	DLPRIMS_H
#define	DLPRIMS_H

#pragma ident	"@(#)dlprims.h	1.1	99/04/09 SMI"

#include <sys/types.h>
#include <sys/dlpi.h>

/*
 * dlprims.[ch] provide a "simpler" interface to DLPI.  in truth, it's
 * rather grotesque, but for now it's the best we can do.  remove this
 * file once DLPI routines are provided in a library.
 */

#ifdef	__cplusplus
extern "C" {
#endif

int		dlinforeq(int, dl_info_ack_t *, size_t);
int		dlattachreq(int, t_uscalar_t);
int		dlbindreq(int, t_uscalar_t, t_uscalar_t, uint16_t, uint16_t);

#ifdef	__cplusplus
}
#endif

#endif	/* DLPRIMS_H */
