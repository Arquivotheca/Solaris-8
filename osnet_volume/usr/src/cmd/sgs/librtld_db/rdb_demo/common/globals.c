/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */
#pragma ident	"@(#)globals.c	1.3	96/09/10 SMI"


#include <libelf.h>
#include "rdb.h"


unsigned long		rdb_flags = 0;	/* misc flags for rdb */

struct ps_prochandle	proch;
