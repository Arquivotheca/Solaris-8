/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)env.h	1.2	97/07/28 SMI"


typedef	struct elist {
	char *			l_libname;
	struct elist *		l_next;
} Elist;

extern void		build_env_list(Elist **, const char *);
extern Elist *		check_list(Elist *, const char *);
extern char *		checkenv(const char *);
