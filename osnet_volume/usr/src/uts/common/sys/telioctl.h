/*
 * "Copyright 1994 Sun Microsystems, Inc. All Rights Reserved.
 * This product and related documentation are protected by copyright
 * and distributed under licenses restricting their use, copying,
 * distribution and decompilation.  No part of this product may be
 * reproduced in any form by any means without prior written
 * authorization by Sun and its licensors, if any."
 *
 * RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the
 * Government is subject to restrictions as set forth in subparagraph
 * (c) (1) (ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 52.227-7013 and in similar clauses in the FAR and
 * NASA FAR Supplement.
 */

#ifndef	_SYS_TELIOCTL_H
#define	_SYS_TELIOCTL_H

#pragma ident	"@(#)telioctl.h	1.3	94/10/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Ioctl's to control telnet protocol module
 * (See also, logindmux.h LOGDMX_IOC_QEXCHANGE)
 *
 * TEL_IOC_ENABLE: Allow processing, and forward normal data messages.  This
 * resumes processing after telmod receives a protocol sequence which it does
 * not process itself.  If data is attached to this ioctl, telmod inserts it
 * at the head of the read queue.
 *
 * TEL_IOC_MODE: Establish the mode for data processing.  Currently binary
 * input and output are the only modes supported.
 *
 * TEL_IOC_GETBLK: When telmod is not enabled, this ioctl requests that
 * the next input message from the network to be processed is forwarded
 * through the mux to the daemon.
 */
#define	TELIOC			('n' << 8)
#define	TEL_IOC_ENABLE		(TELIOC|2)
#define	TEL_IOC_MODE		(TELIOC|3)
#define	TEL_IOC_GETBLK		(TELIOC|4)

/*
 * Bits for indicating binary input (from the net) and output (to the net).
 */
#define	TEL_BINARY_IN	1
#define	TEL_BINARY_OUT	2

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TELIOCTL_H */
