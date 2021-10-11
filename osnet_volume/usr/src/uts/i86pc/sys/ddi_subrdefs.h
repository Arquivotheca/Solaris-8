/*
 * Copyright (c) 1994-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DDI_SUBR_H
#define	_SYS_DDI_SUBR_H

#pragma ident	"@(#)ddi_subrdefs.h	1.5	95/03/31 SMI"

/*
 * Sun DDI platform implementation subroutines definitions
 */

#include <sys/ddi.h>
#include <sys/sunddi.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

struct	impl_bus_promops {
	char 	*ib_type;
	void	*(*ib_childnode)(void *devid);
	void	*(*ib_nextnode)(void *devid);
	int	(*ib_getproplen)(void *devid, caddr_t name);
	int	(*ib_getprop)(void *devid, caddr_t name, caddr_t value);
	int	(*ib_probe)();
	uchar_t	(*ib_conf_getb)(int b, int d, int f, int r);
	ushort_t (*ib_conf_getw)(int b, int d, int f, int r);
	ulong_t (*ib_conf_getl)(int b, int d, int f, int r);
	void	(*ib_conf_putb)(int b, int d, int f, int r, uchar_t v);
	void	(*ib_conf_putw)(int b, int d, int f, int r, ushort_t v);
	void	(*ib_conf_putl)(int b, int d, int f, int r, ulong_t v);
	struct impl_bus_promops *ib_next;
};

extern void impl_bus_add_promops(struct impl_bus_promops *ops_p);
extern void impl_bus_delete_promops(struct impl_bus_promops *ops_p);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DDI_SUBR_H */
