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
#include <thread.h>
#include <rtld_db.h>
#include "libld.h"
#include "rtld.h"
#include "msg.h"

