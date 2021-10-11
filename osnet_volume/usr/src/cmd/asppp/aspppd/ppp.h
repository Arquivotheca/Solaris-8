#ident	"@(#)ppp.h	1.1	93/05/17 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#ifndef _PPP_H
#define	_PPP_H

#include "path.h"
#include "ppp_ioctl.h"

void	process_ppp_msg(int);
void	send_ppp_event(int, ppp_ioctl_t, pppProtocol_t);
void	start_ppp(int);
void	terminate_path(struct path *);

#endif	/* _PPP_H */
