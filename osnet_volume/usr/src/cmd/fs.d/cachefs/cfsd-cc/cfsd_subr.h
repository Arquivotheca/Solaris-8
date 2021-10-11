// ------------------------------------------------------------
//
//			cfsd_subr.h
//
// Include file for the various common routines.
//

#pragma ident   "@(#)cfsd_subr.h 1.2     95/03/08 SMI"
// Copyright (c) 1994 by Sun Microsystems, Inc.

#ifndef CFSD_SUBR
#define	CFSD_SUBR

void subr_add_mount(cfsd_all *allp, const char *dirp, const char *idp);
void subr_cache_setup(cfsd_all *allp);
char *subr_strdup(const char *strp);


#endif /* CFSD_SUBR */
