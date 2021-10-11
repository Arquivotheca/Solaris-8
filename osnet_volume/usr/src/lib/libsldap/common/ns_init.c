/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ns_init.c	1.1	99/07/07 SMI"

#include "ns_sldap.h"
#include "ns_internal.h"

#pragma init(ns_ldap_init)

static void
ns_ldap_init()
{
	ns_ldaperror_init();
	c_setup();
	get_environment();	/* load environment debugging options */
}
