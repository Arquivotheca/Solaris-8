/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)schedctl.c	1.2	98/11/03 SMI"

#include <sys/types.h>
#include <sys/lwp.h>
#include <sys/schedctl.h>
#include <sys/param.h>
#include <thread.h>
#include <synch.h>
#include <schedctl.h>
#include <stdlib.h>

static int		sc_table_enter(lwpid_t, sc_shared_t *);
static sc_shared_t	*sc_table_lookup(lwpid_t);
static void		sc_table_remove(lwpid_t);

/*
 * Table of data structures corresponding to each lwp.
 */

#define	SC_TABLE_SIZE 20

static struct sc_table_entry {
	lwpid_t		lwp;
	sc_shared_t	*addr;
	struct sc_table_entry *next;
} *sc_table[SC_TABLE_SIZE];

#define	SHTOSC(p)	((schedctl_t *)(&((sc_shared_t *)(p))->sc_preemptctl))

static mutex_t sched_lock = DEFAULTMUTEX;

schedctl_t *
schedctl_init()
{
	sc_shared_t *addr;

	if (_lwp_schedctl(SC_PREEMPT, 0, &addr) != 0) {
		return (NULL);		/* error */
	}

	/*
	 * Register the lwp and offset.
	 */
	if (sc_table_enter(_lwp_self(), addr) == 0)
		return (NULL);

	return (SHTOSC(addr));
}

schedctl_t *
schedctl_lookup()
{
	sc_shared_t *addr;

	addr = sc_table_lookup(_lwp_self());
	if (addr == NULL)
		return (NULL);
	else
		return (SHTOSC(addr));
}

void
schedctl_exit()
{
	sc_shared_t *addr;
	lwpid_t	lwp;

	lwp = _lwp_self();
	addr = sc_table_lookup(lwp);
	if (addr != NULL)
		sc_table_remove(lwp);
}

static int
sc_table_enter(lwpid_t lwp, sc_shared_t *addr)
{
	struct sc_table_entry **p;
	struct sc_table_entry *new;

	mutex_lock(&sched_lock);
	/*
	 * First look for an existing entry for this lwp.  If there
	 * is none, add one.
	 */
	for (p = &sc_table[lwp % SC_TABLE_SIZE]; *p != NULL;
	    p = &((*p)->next)) {
		if ((*p)->lwp == lwp) {
			/*
			 * lwp is already in table, just put in the
			 * new address.
			 */
			(*p)->addr = addr;
			mutex_unlock(&sched_lock);
			return (1);
		}
	}
	new = (struct sc_table_entry *)malloc(sizeof (struct sc_table_entry));
	if (new == NULL) {
		mutex_unlock(&sched_lock);
		return (0);
	}
	new->lwp = lwp;
	new->addr = addr;
	new->next = NULL;
	*p = new;
	mutex_unlock(&sched_lock);
	return (1);
}

static void
sc_table_remove(lwpid_t lwp)
{
	struct sc_table_entry **p, *next;

	mutex_lock(&sched_lock);
	for (p = &sc_table[lwp % SC_TABLE_SIZE]; *p != NULL;
	    p = &((*p)->next)) {
		if ((*p)->lwp == lwp) {
			next = (*p)->next;
			free(*p);
			*p = next;
			mutex_unlock(&sched_lock);
			return;
		}
	}
	mutex_unlock(&sched_lock);
}

static sc_shared_t *
sc_table_lookup(lwpid_t lwp)
{
	struct sc_table_entry *p;
	mutex_lock(&sched_lock);
	for (p = sc_table[lwp % SC_TABLE_SIZE]; p != NULL; p = p->next) {
		if (p->lwp == lwp) {
			mutex_unlock(&sched_lock);
			return (p->addr);
		}
	}
	mutex_unlock(&sched_lock);
	return (NULL);
}
