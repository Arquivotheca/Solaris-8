/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ABIENV_H
#define	_ABIENV_H

#pragma ident	"@(#)abienv.h	1.1	99/05/14 SMI"

typedef	struct liblist {
	char 		*l_libname;
	void		*l_handle;
	struct liblist	*l_next;
} Liblist;

typedef struct intlist {
	char		*i_name;
	struct intlist	*i_next;
} Intlist;

extern void		appendlist(Liblist **, Liblist **,
    char const *, int);
extern void		build_env_list(Liblist **, char const *);
extern void		build_env_list1(Liblist **, Liblist **, char const *);
extern Liblist		*check_list(Liblist *, char const *);
extern char		*checkenv(char const *);
extern int		build_interceptor_path(char *, size_t, char const *);
extern char		*abibasename(char const *);

extern void		env_to_intlist(Intlist **, char const *);
extern int		check_intlist(Intlist *, char const *);

#endif /* _ABIENV_H */
