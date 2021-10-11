#ident	"@(#)ip.h	1.1	93/05/17 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#ifndef	_IP_H
#define	_IP_H

#include "path.h"

void	start_ip(struct path *);
void	stop_ip(struct path *);

#endif	_IP_H
