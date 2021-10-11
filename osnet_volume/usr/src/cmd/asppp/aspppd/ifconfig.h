#ident	"@(#)ifconfig.h	1.3	97/01/02 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#ifndef _IFCONFIG_H
#define	_IFCONFIG_H

#include <sys/types.h>
#include "path.h"

u_long	get_if_addr(struct path *);
void	set_if_addr(struct path *, u_long);
u_long	get_if_dst_addr(struct path *);
void	set_if_dst_addr(struct path *, u_long);
u_short	get_if_mtu(struct path *);
void	set_if_mtu(struct path *, u_short);
char	*get_new_if(void);
void	mark_if_down(struct path *);
void	mark_if_up(struct path *);
boolean_t is_if_up(struct path *);
#endif	/* _IFCONFIG_H */
