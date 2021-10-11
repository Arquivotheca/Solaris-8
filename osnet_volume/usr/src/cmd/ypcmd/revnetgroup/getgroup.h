/*
 * Copyright 1995 Sun Microsystems Inc.
 * All rights reserved.
 */
							    

#ifndef	__GETGROUP_H
#define	__GETGROUP_H

#pragma ident	"@(#)getgroup.h	1.4	96/04/25 SMI"        /* SMI4.1 1.4 */

#ifdef	__cplusplus
extern "C" {
#endif

struct grouplist {		
	char *gl_machine;
	char *gl_name;
	char *gl_domain;
	struct grouplist *gl_nxt;
};

struct grouplist *my_getgroup();

#ifdef	__cplusplus
}
#endif

#endif	/* __GETGROUP_H */
