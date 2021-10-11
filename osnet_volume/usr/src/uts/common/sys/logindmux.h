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

#ifndef	_SYS_LOGINDMUX_H
#define	_SYS_LOGINDMUX_H

#pragma ident	"@(#)logindmux.h	1.4	97/08/12 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct protocol_arg {
	dev_t	dev;
	int	flag;
};

#ifdef _SYSCALL32
struct protocol_arg32 {
	dev32_t	dev;
	int32_t flag;
};
#endif

/*
 * Driver state values.
 */
#define	TMXOPEN	1
#define	TMXPLINK 2

/*
 * Telnet magic cookie
 */
#define	M_CTL_MAGIC_NUMBER	70

/*
 * Ioctl to establish linkage between a pty master stream and a
 * network stream.
 */
#ifndef TELIOC
#define	TELIOC			('n' << 8) /* XXX.sparker fixme */
#endif
#define	LOGDMX_IOC_QEXCHANGE	(TELIOC|1) /* ioctl for Q pair exchange */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LOGINDMUX_H */
