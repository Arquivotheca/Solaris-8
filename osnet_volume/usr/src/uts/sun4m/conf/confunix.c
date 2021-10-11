/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)confunix.c	1.5	99/10/05 SMI"

#include <sys/bootconf.h>

struct bootobj rootfs = {
	{ { "" },	{ "" } }
};

struct bootobj frontfs = {
	{ { "" },	{ "" } }
};

struct bootobj backfs = {
	{ { "" },	{ "" } }
};

struct bootobj swapfile = {
	{ { "" },	{ "" } }
};
