/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)ns_crypt.c 1.1     99/07/07 SMI"

#include <stdlib.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/time.h>
#include "ns_sldap.h"
#include "ns_internal.h"


void
c_setup()
{
}


char *
evalue(char *ptr)
{
	return (strdup(ptr));
}


char *
dvalue(char *ptr)
{
	return (strdup(ptr));
}
