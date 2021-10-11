/* LINTLIBRARY */
/* PROTOLIB1 */

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc. 
 * All rights reserved. 
 */ 

#pragma ident	"@(#)lintsup.c	1.2	98/09/05 SMI"

/*
 * Supplimental definitions for lint that help us avoid
 * options like `-x' that filter out things we want to
 * know about as well as things we don't.
 */

/*
 * The public interfaces are allowed to be "declared
 * but not used".
 */
#include <libelf.h>
#include <link.h>
#include "sgs.h"
#include "libld.h"
#include "arch_msg.h"
