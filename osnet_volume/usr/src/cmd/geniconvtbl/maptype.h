/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_ICONV_TM_MAPTYPE_H
#define	_ICONV_TM_MAPTYPE_H

#pragma ident	"@(#)maptype.h	1.2	99/05/25 SMI"


#ifdef	__cplusplus
extern "C" {
#endif

#include "itmcomp.h"

static struct {
	char	*name;
	itmc_map_type_t	type;
} map_type_name[] = {
	{"automatic",	ITMC_MAP_AUTOMATIC},
	{"index",	ITMC_MAP_SIMPLE_INDEX},
	{"hash",	ITMC_MAP_SIMPLE_HASH},
	{"binary",	ITMC_MAP_BINARY_SEARCH},
	{"dense",	ITMC_MAP_DENSE_ENCODING},
	{NULL,		ITMC_MAP_UNKNOWN},
};


#ifdef	__cplusplus
}
#endif

#endif /* !_ICONV_TM_MAPTYPE_H */
