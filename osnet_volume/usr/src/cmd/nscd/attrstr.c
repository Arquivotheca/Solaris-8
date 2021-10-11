/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)attrstr.c	1.1	99/04/07 SMI"

#include <strings.h>

int
attr_strlen(char *s)
{
	return (s ? strlen(s) : 0);
}

char *
attr_strcpy(char *dest, char *src)
{
	return (strcpy(dest, src ? src : ""));
}
