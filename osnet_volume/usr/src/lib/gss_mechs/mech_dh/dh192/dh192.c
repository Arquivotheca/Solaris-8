/*
 *	dh192.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)dh192.c	1.1	97/11/19 SMI"

#include "dh_gssapi.h"

static gss_OID_desc  OID = {9, "\053\006\004\001\052\002\032\002\003" };
static char *MODULUS = 	"d4a0ba0250b6fd2ec626e7ef"
			"d637df76c716e22d0944b88b";
static int ROOT = 3;
static int KEYLEN = 192;
static int ALGTYPE = 0;
#define	HEX_KEY_BYTES 48

#include "../dh_common/dh_template.c"

#include "../dh_common/dh_nsl_tmpl.c"

#if 1
#include "fakensl.c"
#endif
