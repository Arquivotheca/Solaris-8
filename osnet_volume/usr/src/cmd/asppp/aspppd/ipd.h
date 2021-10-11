#ident	"@(#)ipd.h	1.1	93/05/17 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#ifndef _IPD_H
#define	_IPD_H

/* access to IP/dialup connection manager node */

#define	IPDCM		"/dev/ipdcm"
#define	IPD_PATH	"/dev/ipd"
#define	IPDPTP_PATH	"/dev/ipdptp"

extern int	ipdcm;

void	process_ipd_msg(int);

#endif	/* _IPD_H */
