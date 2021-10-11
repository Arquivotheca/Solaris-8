/*
 * Copyright (c) 1989-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)space.c	1.59	99/10/22 SMI"

/*
 * The intent of this file is to contain any data that must remain
 * resident in the kernel.
 *
 * space_store(), space_fetch(), and space_free() have been added to
 * easily store and retrieve kernel resident data.
 * These functions are recommended rather than adding new variables to
 * this file.
 *
 * Note that it's possible for name collisions to occur.  In order to
 * prevent collisions, it's recommended that the convention in
 * PSARC/1997/389 be used.  If a collision occurs, then space_store will
 * fail.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/acct.h>
#include <sys/var.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/utsname.h>
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/sysinfo.h>
#include <sys/t_lock.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/vmem.h>
#include <sys/modhash.h>
#include <sys/cmn_err.h>

#include <sys/strredir.h>
#include <sys/kbio.h>

struct	acct	acctbuf;	/* needs to be here if acct.c is loadable */
struct	vnode	*acctvp;
kmutex_t	aclock;

struct	buf	bfreelist;	/* Head of the free list of buffers */

sysinfo_t	sysinfo;
vminfo_t	vminfo;		/* VM stats protected by sysinfolock mutex */

#ifdef	lint
int	__lintzero;		/* Alway zero for shutting up lint */
#endif

/*
 * The following describe the physical memory configuration.
 *
 *	physmem	 -  The amount of physical memory configured
 *		    in pages.  ptob(physmem) is the amount
 *		    of physical memory in bytes.  Defined in
 *		    .../os/startup.c.
 *
 *	physmax  -  The highest numbered physical page in memory.
 *
 *	maxmem	 -  Maximum available memory, in pages.  Defined
 *		    in main.c.
 *
 *	physinstalled
 *		 -  Pages of physical memory installed;
 *		    includes use by PROM/boot not counted in
 *		    physmem.
 */

pfn_t	physmax;
pgcnt_t	physinstalled;

struct var v;

#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/bootconf.h>

/*
 * Data from swapgeneric.c that must be resident.
 */
struct vnode *rootvp;
dev_t rootdev;
int netboot;
int obpdebug;
char *dhcack;	/* Used to cache ascii form of DHCPACK handed up by boot */

/*
 * Data from arp.c that must be resident.
 */
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/sockio.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/dlpi.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>

ether_addr_t etherbroadcastaddr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/*
 * Data from timod that must be resident
 */

/*
 * state transition table for TI interface
 */
#include <sys/tihdr.h>

#define	nr	127		/* not reachable */

char ti_statetbl[TE_NOEVENTS][TS_NOSTATES] = {
				/* STATES */
	/* 0  1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16 */

	{ 1, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr},
	{nr, nr, nr,  2, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr},
	{nr, nr, nr,  4, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr},
	{nr,  3, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr},
	{nr, nr, nr, nr,  3, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr},
	{nr,  0,  3, nr,  3,  3, nr, nr,  7, nr, nr, nr,  6,  7,  9, 10, 11},
	{nr, nr,  0, nr, nr,  6, nr, nr, nr, nr, nr, nr,  3, nr,  3,  3,  3},
	{nr, nr, nr, nr, nr, nr, nr, nr,  9, nr, nr, nr, nr,  3, nr, nr, nr},
	{nr, nr, nr, nr, nr, nr, nr, nr,  3, nr, nr, nr, nr,  3, nr, nr, nr},
	{nr, nr, nr, nr, nr, nr, nr, nr,  7, nr, nr, nr, nr,  7, nr, nr, nr},
	{nr, nr, nr,  5, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr},
	{nr, nr, nr, nr, nr, nr, nr,  8, nr, nr, nr, nr, nr, nr, nr, nr, nr},
	{nr, nr, nr, nr, nr, nr, 12, 13, nr, 14, 15, 16, nr, nr, nr, nr, nr},
	{nr, nr, nr, nr, nr, nr, nr, nr, nr,  9, nr, 11, nr, nr, nr, nr, nr},
	{nr, nr, nr, nr, nr, nr, nr, nr, nr,  9, nr, 11, nr, nr, nr, nr, nr},
	{nr, nr, nr, nr, nr, nr, nr, nr, nr, 10, nr,  3, nr, nr, nr, nr, nr},
	{nr, nr, nr,  7, nr, nr, nr,  7, nr, nr, nr, nr, nr, nr, nr, nr, nr},
	{nr, nr, nr, nr, nr, nr,  9, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr},
	{nr, nr, nr, nr, nr, nr, nr, nr, nr,  9, 10, nr, nr, nr, nr, nr, nr},
	{nr, nr, nr, nr, nr, nr, nr, nr, nr,  9, 10, nr, nr, nr, nr, nr, nr},
	{nr, nr, nr, nr, nr, nr, nr, nr, nr, 11,  3, nr, nr, nr, nr, nr, nr},
	{nr, nr, nr, nr, nr, nr,  3, nr, nr,  3,  3,  3, nr, nr, nr, nr, nr},
	{nr, nr, nr, nr, nr, nr, nr,  3, nr, nr, nr, nr, nr, nr, nr, nr, nr},
	{nr, nr, nr, nr, nr, nr, nr,  7, nr, nr, nr, nr, nr, nr, nr, nr, nr},
	{nr, nr, nr,  9, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr},
	{nr, nr, nr,  3, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr},
	{nr, nr, nr,  3, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr},
	{nr, nr, nr,  3, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr, nr},
};


#include <sys/sad.h>
#include <sys/tty.h>
#include <sys/ptyvar.h>

static void store_fetch_initspace();

/*
 * Allocate tunable structures at runtime.
 */
void
space_init(void)
{
	sad_initspace();
	pty_initspace();
	store_fetch_initspace();
}

/*
 * moved from ts.c because slp.c references it!!!!! BLECH!!!
 *
 */
#define	NKMDPRIS  40

short ts_maxkmdpri = NKMDPRIS - 1; /* maximum kernel mode ts priority */
int ts_dispatch_extended = -1; /* set in ts_getdptbl or set_platform_default */

/*
 * Previously defined in consmsconf.c ...
 */
dev_t kbddev = NODEV;
dev_t mousedev = NODEV;
dev_t stdindev = NODEV;
struct vnode *wsconsvp;

dev_t fbdev = NODEV;
struct vnode *fbvp;

/*
 * from shm.c
 */

struct shmid_ds	*shmem;		/* shared memory id pool */
struct shminfo	shminfo;	/* shared memory parameters */

/*
 * from sem.c
 */

struct semid_ds *sema;		/* semaphore id pool */
struct sem	*sem;		/* semaphore pool */
struct sem_undo	**sem_undo;	/* per process undo table */
struct sem_undo  *semunp;	/* ptr to head of undo chain */
struct sem_undo  *semfup;	/* ptr to head of free undo chain */
int		*semu;		/* undo structure pool */
struct seminfo	seminfo;	/* semaphore parameters */

/*
 * moved from cons.c because they must be resident in the kernel.
 */
vnode_t	*rconsvp;
dev_t	rconsdev;
dev_t	uconsdev = NODEV;

/*
 * This flag, when set marks rconsvp in a transition state.
 */

int	cn_conf;

/* From ip_main.c */
unsigned char ip_protox[IPPROTO_MAX];
struct ip_provider *lastprov;

/*
 * Moved from sad_conf.c because of the usual in loadable modules
 */

#ifndef NSTRPHASH
#define	NSTRPHASH	128
#endif
struct autopush **strpcache;
int strpmask = NSTRPHASH - 1;

/*
 * Moved here from wscons.c
 * Package the redirection-related routines into an ops vector of the form
 * that the redirecting driver expects.
 */
extern int wcvnget();
extern void wcvnrele();
srvnops_t	wscons_srvnops = {
	wcvnget,
	wcvnrele
};

/*
 * consconfig() in autoconf.c sets this; it's the vnode of the distinguished
 * keyboard/frame buffer combination, aka the workstation console.
 */

vnode_t *rwsconsvp;
dev_t	rwsconsdev;

/*
 * Platform console abort policy.
 * Platforms may override the default software policy, if such hardware
 * (e.g. keyswitches with a secure position) exists.
 */
int abort_enable = KIOCABORTENABLE;

/*
 * From msg.c
 */

struct msg	*msgh;			/* message headers */
struct msqid_ds	*msgque;		/* msg queue headers */
struct msglock	*msglock; 		/* locks for the message queues */
struct msg	*msgfp;			/* ptr to head of free header list */
struct msginfo	msginfo;		/* message parameters */

/* from iwscons.c */

kthread_id_t	iwscn_thread;	/* thread that is allowed to push redirm */
wcm_data_t	*iwscn_wcm_data; /* allocated data for redirm */

/* from cpc.c */
uint_t kcpc_key;	/* TSD key for CPU performance counter context */

/*
 * storing and retrieving data by string key
 *
 * this mechanism allows a consumer to store and retrieve by name a pointer
 * to some space maintained by the consumer.
 * For example, a driver or module may want to have persistent data
 * over unloading/loading cycles. The pointer is typically to some
 * kmem_alloced space and it should not be pointing to data that will
 * be destroyed when the module is unloaded.
 */
static mod_hash_t *space_hash;
static char *space_hash_name = "space_hash";
static size_t	space_hash_nchains = 8;

static void
store_fetch_initspace()
{
	space_hash = mod_hash_create_strhash(space_hash_name,
		space_hash_nchains, mod_hash_null_valdtor);
	ASSERT(space_hash);
}

int
space_store(char *key, uintptr_t ptr)
{
	char *s;
	int rval;
	size_t l;

	/* some sanity checks first */
	if (key == NULL) {
		return (-1);
	}
	l = (size_t)strlen(key);
	if (l == 0) {
		return (-1);
	}

	/* increment for null terminator */
	l++;

	/* alloc space for the string, mod_hash_insert will deallocate */
	s = kmem_alloc(l, KM_SLEEP);
	bcopy(key, s, l);

	rval = mod_hash_insert(space_hash,
		(mod_hash_key_t)s, (mod_hash_val_t)ptr);

	switch (rval) {
	case 0:
		break;
#ifdef DEBUG
	case MH_ERR_DUPLICATE:
		cmn_err(CE_WARN, "space_store: duplicate key %s", key);
		rval = -1;
		break;
	case MH_ERR_NOMEM:
		cmn_err(CE_WARN, "space_store: no mem for key %s", key);
		rval = -1;
		break;
	default:
		cmn_err(CE_WARN, "space_store: unspecified error for key %s",
		    key);
		rval = -1;
		break;
#else
	default:
		rval = -1;
		break;
#endif
	}

	return (rval);
}

uintptr_t
space_fetch(char *key)
{
	uintptr_t ptr = 0;
	mod_hash_val_t val;
	int rval;

	if (key) {
		rval = mod_hash_find(space_hash, (mod_hash_key_t)key, &val);
		if (rval == 0) {
			ptr = (uintptr_t)val;
		}
	}

	return (ptr);
}

void
space_free(char *key)
{
	if (key) {
		(void) mod_hash_destroy(space_hash, (mod_hash_key_t)key);
	}
}
