/*
 * Copyright (c) 1986-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)str_conf.c	1.37	97/04/01 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/stream.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/t_lock.h>

fmodsw_impl_t fmodsw[] =
{
	{ "",		NULL,		0 }, /* reserved for loadable modules */
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
	{ "",		NULL,		0 },
};

int	fmodcnt = sizeof (fmodsw) / sizeof (fmodsw[0]) - 1;

kmutex_t fmodsw_lock;	/* Lock for dynamic allocation of fmodsw entries */
