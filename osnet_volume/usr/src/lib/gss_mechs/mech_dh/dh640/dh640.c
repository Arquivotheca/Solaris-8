/*
 *	dh640.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)dh640.c	1.1	97/11/19 SMI"

#include "dh_gssapi.h"

static gss_OID_desc OID = {9, "\053\006\004\001\052\002\032\002\004" };
static char *MODULUS = 	"D63C7139E1F148AB6AF2DC8BE400087D"
			"65E16E007BEC44E4F46621730165C751"
			"8DDC6255AE10F52FC42270F7CF1412E0"
			"62687B40387455E51D995A8360DB3DC8"
			"5002F72379D3537E97D2B1F4A71FC5DB";

static int ROOT = 2;
static int KEYLEN = 640;
static int ALGTYPE = 0;
#define	HEX_KEY_BYTES  160

#include "../dh_common/dh_template.c"

#include "../dh_common/dh_nsl_tmpl.c"
