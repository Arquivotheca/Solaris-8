/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PRTABLE_H
#define	_PRTABLE_H

#pragma ident	"@(#)prtable.h	1.1	99/04/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <limits.h>
#include "prstat.h"

#define	LWPID_TBL_SZ	4096		/* hash table of lwpid_t structures */
#define	LWP_ACTIVE	1

typedef struct {
	size_t		t_size;
	size_t		t_nent;
	long		*t_list;
} table_t;

typedef struct {
	uid_t		u_id;
	char		u_name[LOGNAME_MAX+1];
} name_t;

typedef struct {
	size_t		n_size;
	size_t		n_nent;
	name_t		*n_list;
} nametbl_t;

typedef struct lwpid {			/* linked list of pointers to lwps */
	pid_t		l_pid;
	id_t		l_lwpid;
	int		l_active;
	lwp_info_t	*l_lwp;
	struct lwpid	*l_next;
} lwpid_t;

extern void pwd_getname(int, char *, int);
extern void add_uid(nametbl_t *, char *);
extern int has_uid(nametbl_t *, uid_t);
extern void add_element(table_t *, long);
extern int has_element(table_t *, long);
extern void lwpid_init();
extern void lwpid_add(lwp_info_t *, pid_t, id_t);
extern lwp_info_t *lwpid_get(pid_t, id_t);
extern int lwpid_pidcheck(pid_t);
extern void lwpid_del(pid_t, id_t);
extern void lwpid_set_active(pid_t, id_t);
extern int lwpid_is_active(pid_t, id_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _PRTABLE_H */
