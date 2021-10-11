#ident	"@(#)route.h	1.1	93/05/17 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#ifndef _ROUTE_H
#define	_ROUTE_H

#include "path.h"

void	add_default_route(struct path *);
void	delete_default_route(struct path *);

#endif	/* _ROUTE_H */
