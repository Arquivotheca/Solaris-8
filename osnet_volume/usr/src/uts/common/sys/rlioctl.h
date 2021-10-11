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

#ifndef	_SYS_RLIOCTL_H
#define	_SYS_RLIOCTL_H

#pragma ident	"@(#)rlioctl.h	1.4	94/10/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef TRUE
#define	TRUE	1
#endif

#ifndef	TIOCPKT_WINDOW
#define	TIOCPKT_WINDOW	0x80
#endif

#define	TIOCPKT_FLUSHWRITE	0x02	/* flush unprocessed data */
#define	TIOCPKT_NOSTOP		0x10	/* no more ^S, ^Q */
#define	TIOCPKT_DOSTOP		0x20	/* now do ^S, ^Q */

/*
 * Rlogin protocol requests begin with two bytes of "RLOGIN_MAGIC".
 * See RFC-1282.
 */
#define	RLOGIN_MAGIC	0xff

/*
 * RL_IOC_ENABLE starts the module, inserting any (optional) data passed to
 * it at the head of the read side queue.
 */
#define	RLIOC			('r' << 8)
#define	RL_IOC_ENABLE		(RLIOC|1)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_RLIOCTL_H */
