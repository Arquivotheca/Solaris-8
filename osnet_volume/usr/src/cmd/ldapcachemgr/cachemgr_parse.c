/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)cachemgr_parse.c 1.1     99/07/07 SMI"

/*
 *   routine to parse configuration file
 *
 *   returns -1 on error, 0 on success.  Error messages to log.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <synch.h>
#include <sys/door.h>
#include <unistd.h>
#include "cachemgr.h"

extern admin_t current_admin;

static int
cachemgr_set_integer(int * addr, char * facility, char * cachename,
		int value, int min, int max)
{
	if (value < min || value > max) {
		logit("attempted to set value of %s for %s to %d, "
		    "which is not %d <= x <= %d\n",
		    facility, cachename, value, min, max);

		return	(-1);
	}

	if ((addr != NULL) && (*addr != value)) {
		if (current_admin.debug_level)
			logit("Setting %s for %s to %d\n",
			    facility, cachename, value);
		*addr = value;

		return (1);
	}

	return (0);
}

int
cachemgr_set_dl(admin_t * ptr, int value)
{
	if (ptr == NULL)
		return (-1);

	return (cachemgr_set_integer(&(ptr->debug_level),
		"Debug level", "cachemgr", value, 0, MAXDEBUG));
}

int
cachemgr_set_ttl(ldap_stat_t * cache, char * name, int value)
{
	int result;

	if (cache == NULL)
		return (-1);

	result = cachemgr_set_integer(&(cache->ldap_ttl),
		"Time to live", name,
		value, 0, 1<<MAXBITSIZE);

	return (result);
}
