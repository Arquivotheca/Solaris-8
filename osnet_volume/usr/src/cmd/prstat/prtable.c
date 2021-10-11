/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)prtable.c 1.1     99/04/19 SMI"

#include <procfs.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <ctype.h>
#include <string.h>
#include <libintl.h>

#include "prstat.h"
#include "prutil.h"
#include "prtable.h"

static lwpid_t	*lwpid_tbl[LWPID_TBL_SZ];

void
lwpid_init()
{
	(void) memset(&lwpid_tbl, 0, sizeof (lwpid_t *) * LWPID_TBL_SZ);
}

static int
pwd_getid(char *name)
{
	struct passwd *pwd;

	if ((pwd = getpwnam(name)) == NULL)
		Die(gettext("invalid user name: %s\n"), name);
	return (pwd->pw_uid);
}

void
pwd_getname(int uid, char *name, int length)
{
	struct passwd *pwd;

	if ((pwd = getpwuid(uid)) == NULL) {
		(void) snprintf(name, length, "%d", uid);
	} else {
		(void) snprintf(name, length, "%s", pwd->pw_name);
	}
}

void
add_uid(nametbl_t *tbl, char *name)
{
	name_t *entp;

	if (tbl->n_size == tbl->n_nent) {	/* reallocation */
		if ((tbl->n_size *= 2) == 0)
			tbl->n_size = 4;	/* first time */
		tbl->n_list = Realloc(tbl->n_list, tbl->n_size*sizeof (name_t));
	}

	entp = &tbl->n_list[tbl->n_nent++];

	if (isdigit(name[0])) {
		entp->u_id = Atoi(name);
		pwd_getname(entp->u_id, entp->u_name, LOGNAME_MAX);
	} else {
		entp->u_id = pwd_getid(name);
		(void) snprintf(entp->u_name, LOGNAME_MAX, "%s", name);
	}
}

int
has_uid(nametbl_t *tbl, uid_t uid)
{
	size_t i;

	if (tbl->n_nent) {	/* do linear search if table is not empty */
		for (i = 0; i < tbl->n_nent; i++)
			if (tbl->n_list[i].u_id == uid)
				return (1);
	} else {
		return (1);	/* if table is empty return true */
	}

	return (0);		/* nothing has been found */
}

void
add_element(table_t *table, long element)
{
	if (table->t_size == table->t_nent) {
		if ((table->t_size *= 2) == 0)
			table->t_size = 4;
		table->t_list = Realloc(table->t_list,
		    table->t_size * sizeof (long));
	}
	table->t_list[table->t_nent++] = element;
}

int
has_element(table_t *table, long element)
{
	size_t i;

	if (table->t_nent) {	/* do linear search if table is not empty */
		for (i = 0; i < table->t_nent; i++)
			if (table->t_list[i] == element)
				return (1);
	} else {		/* if table is empty then */
		return (1);	/* pretend that element was found */
	}

	return (0);	/* element was not found */
}

void
lwpid_add(lwp_info_t *lwp, pid_t pid, id_t lwpid)
{
	lwpid_t	*elm = Zalloc(sizeof (lwpid_t));
	int hash = pid % LWPID_TBL_SZ;

	elm->l_pid = pid;
	elm->l_lwpid = lwpid;
	elm->l_lwp = lwp;
	elm->l_next = lwpid_tbl[hash]; /* add in front of chain */
	lwpid_tbl[hash] = elm;
}

void
lwpid_del(pid_t pid, id_t lwpid)
{
	lwpid_t	*elm, *elm_prev;
	int hash = pid % LWPID_TBL_SZ;

	elm = lwpid_tbl[hash];
	elm_prev = NULL;

	while (elm) {
		if ((elm->l_pid == pid) && (elm->l_lwpid == lwpid)) {
			if (!elm_prev)	/* first chain element */
				lwpid_tbl[hash] = elm->l_next;
			else
				elm_prev->l_next = elm->l_next;
			free(elm);
			break;
		} else {
			elm_prev = elm;
			elm = elm->l_next;
		}
	}
}

static lwpid_t *
lwpid_getptr(pid_t pid, id_t lwpid)
{
	lwpid_t *elm = lwpid_tbl[pid % LWPID_TBL_SZ];
	while (elm) {
		if ((elm->l_pid == pid) && (elm->l_lwpid == lwpid))
			return (elm);
		else
			elm = elm->l_next;
	}
	return (NULL);
}

lwp_info_t *
lwpid_get(pid_t pid, id_t lwpid)
{
	lwpid_t	*elm = lwpid_getptr(pid, lwpid);
	if (elm)
		return (elm->l_lwp);
	else
		return (NULL);
}

int
lwpid_pidcheck(pid_t pid)
{
	lwpid_t *elm;
	elm = lwpid_tbl[pid % LWPID_TBL_SZ];
	while (elm) {
		if (elm->l_pid == pid)
			return (1);
		else
			elm = elm->l_next;
	}
	return (0);
}

int
lwpid_is_active(pid_t pid, id_t lwpid)
{
	lwpid_t	*elm = lwpid_getptr(pid, lwpid);
	if (elm)
		return (elm->l_active);
	else
		return (0);
}

void
lwpid_set_active(pid_t pid, id_t lwpid)
{
	lwpid_t *elm = lwpid_getptr(pid, lwpid);
	if (elm)
		elm->l_active = LWP_ACTIVE;
}
