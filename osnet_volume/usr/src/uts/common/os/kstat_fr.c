/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kstat_fr.c	1.49	99/09/19 SMI"

/*
 * Kernel statistics framework
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/vmsystm.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/kstat.h>
#include <sys/sysinfo.h>
#include <sys/cpuvar.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/dnlc.h>
#include <sys/var.h>
#include <sys/vmmeter.h>
#include <sys/debug.h>
#include <sys/kobj.h>
#include <vm/page.h>
#include <vm/anon.h>

/*
 * Global kstat chain, protected by kstat_chain_lock
 */

kstat_t *kstat_chain;
kmutex_t kstat_chain_lock;

/*
 * Every install/delete kstat bumps kstat_chain_id.  This is used by:
 *
 * (1)	/dev/kstat, to detect changes in the kstat chain across ioctls;
 *
 * (2)	kstat_create(), to assign a KID (kstat ID) to each new kstat.
 *	/dev/kstat uses the KID as a cookie for kstat lookups.
 */

kid_t kstat_chain_id = -1;

/*
 * We keep track of whether there are active kstat clients, so that any
 * kstat refresh daemons can tell when they're no longer needed.  For
 * example, some hardware counters wrap around reasonably quickly (less
 * than a second); to keep these up to date, a kstat's update routine
 * may fire up a refresh daemon the first time it's called.  By looking
 * at kstat_active, the daemon can determine whether it is still needed.
 *
 * kstat_active is set by the first /dev/kstat open(), and cleared by
 * the last /dev/kstat close().
 */

int kstat_active;

/*
 * Extended kstat structure -- for internal use only.  Allows us to keep
 * track of total allocation size, hash table stuff, etc.
 */

#define	KSTAT_HASH_BYNAME	0
#define	KSTAT_HASH_BYKID	1
#define	KSTAT_HASH_TYPES	2

typedef struct ekstat {
	kstat_t		ks;
	size_t		allocation_size;
	struct	ekstat	*hash_next[KSTAT_HASH_TYPES];
} ekstat_t;

/*
 * Hashing structures
 */

#define	KSTAT_HASH_SIZE		64		/* Must be power of 2 */
#define	KSTAT_HASH_MASK		(KSTAT_HASH_SIZE - 1)

static	int	kstat_namehash_func(ekstat_t *);
static	int	kstat_kidhash_func(ekstat_t *);

/* CSTYLED */
static	int	(*kstat_hash_func[KSTAT_HASH_TYPES])(ekstat_t *) = {
	kstat_namehash_func,
	kstat_kidhash_func
};

static	ekstat_t	*kstat_kidhash_table[KSTAT_HASH_SIZE];
static	ekstat_t	*kstat_namehash_table[KSTAT_HASH_SIZE];

static	ekstat_t	**kstat_hash_table[KSTAT_HASH_TYPES] = {
	kstat_namehash_table,
	kstat_kidhash_table
};

static	void	kstat_hash_insert(ekstat_t *, int);
static	void	kstat_hash_delete(ekstat_t *, int);
static	kstat_t	*kstat_hash_lookup(ekstat_t *, int);

/*
 * Various pointers we need to create kstats at boot time in kstat_init()
 */
extern	struct	ncstats	ncstats;
extern	kstat_named_t	*segmapcnt_ptr;
extern	uint_t		segmapcnt_ndata;
extern	kstat_named_t	*biostats_ptr;
extern	uint_t		biostats_ndata;
extern	kstat_named_t	*pollstats_ptr;
extern	uint_t		pollstats_ndata;

extern	int	vac;
extern	uint_t	nproc;
extern	time_t	boot_time;
extern	sysinfo_t	sysinfo;
extern	vminfo_t	vminfo;

struct {
	kstat_named_t ncpus;
	kstat_named_t lbolt;
	kstat_named_t deficit;
	kstat_named_t clk_intr;
	kstat_named_t vac;
	kstat_named_t nproc;
	kstat_named_t avenrun_1min;
	kstat_named_t avenrun_5min;
	kstat_named_t avenrun_15min;
	kstat_named_t boot_time;
} system_misc_kstat = {
	{ "ncpus",		KSTAT_DATA_UINT32 },
	{ "lbolt",		KSTAT_DATA_UINT32 },
	{ "deficit",		KSTAT_DATA_UINT32 },
	{ "clk_intr",		KSTAT_DATA_UINT32 },
	{ "vac",		KSTAT_DATA_UINT32 },
	{ "nproc",		KSTAT_DATA_UINT32 },
	{ "avenrun_1min",	KSTAT_DATA_UINT32 },
	{ "avenrun_5min",	KSTAT_DATA_UINT32 },
	{ "avenrun_15min",	KSTAT_DATA_UINT32 },
	{ "boot_time",		KSTAT_DATA_UINT32 },
};

struct {
	kstat_named_t physmem;
	kstat_named_t nalloc;
	kstat_named_t nfree;
	kstat_named_t nalloc_calls;
	kstat_named_t nfree_calls;
	kstat_named_t kernelbase;
	kstat_named_t econtig;
	kstat_named_t freemem;
	kstat_named_t availrmem;
	kstat_named_t lotsfree;
	kstat_named_t cachefree;
	kstat_named_t desfree;
	kstat_named_t minfree;
	kstat_named_t fastscan;
	kstat_named_t slowscan;
	kstat_named_t nscan;
	kstat_named_t desscan;
	kstat_named_t pp_kernel;
	kstat_named_t pagesfree;
	kstat_named_t pageslocked;
	kstat_named_t pagestotal;
} system_pages_kstat = {
	{ "physmem",		KSTAT_DATA_ULONG },
	{ "nalloc",		KSTAT_DATA_ULONG },
	{ "nfree",		KSTAT_DATA_ULONG },
	{ "nalloc_calls",	KSTAT_DATA_ULONG },
	{ "nfree_calls",	KSTAT_DATA_ULONG },
	{ "kernelbase",		KSTAT_DATA_ULONG },
	{ "econtig", 		KSTAT_DATA_ULONG },
	{ "freemem", 		KSTAT_DATA_ULONG },
	{ "availrmem", 		KSTAT_DATA_ULONG },
	{ "lotsfree", 		KSTAT_DATA_ULONG },
	{ "cachefree", 		KSTAT_DATA_ULONG },
	{ "desfree", 		KSTAT_DATA_ULONG },
	{ "minfree", 		KSTAT_DATA_ULONG },
	{ "fastscan", 		KSTAT_DATA_ULONG },
	{ "slowscan", 		KSTAT_DATA_ULONG },
	{ "nscan", 		KSTAT_DATA_ULONG },
	{ "desscan", 		KSTAT_DATA_ULONG },
	{ "pp_kernel", 		KSTAT_DATA_ULONG },
	{ "pagesfree", 		KSTAT_DATA_ULONG },
	{ "pageslocked", 	KSTAT_DATA_ULONG },
	{ "pagestotal",		KSTAT_DATA_ULONG },
};

static int header_kstat_update(kstat_t *, int);
static int header_kstat_snapshot(kstat_t *, void *, int);
static int system_misc_kstat_update(kstat_t *, int);
static int system_pages_kstat_update(kstat_t *, int);

static struct {
	char	name[KSTAT_STRLEN];
	size_t	size;
	uint_t	min_ndata;
	uint_t	max_ndata;
} kstat_data_type[KSTAT_NUM_TYPES] = {
	{ "raw",		1,			0,	INT_MAX	},
	{ "name=value",		sizeof (kstat_named_t),	0,	INT_MAX	},
	{ "interrupt",		sizeof (kstat_intr_t),	1,	1	},
	{ "i/o",		sizeof (kstat_io_t),	1,	1	},
	{ "event_timer",	sizeof (kstat_timer_t),	0,	INT_MAX	},
};

/*
 * kstat_init: called at startup to create various system kstats
 */
void
kstat_init(void)
{
	kstat_t *ksp;

	mutex_init(&kstat_chain_lock, NULL, MUTEX_DEFAULT, NULL);

	/*
	 * The mother of all kstats.  The first kstat in the system, which
	 * always has KID 0, has the headers for all kstats (including itself)
	 * as its data.  Thus, the kstat driver does not need any special
	 * interface to extract the kstat chain.
	 */
	kstat_chain_id = 0;
	ksp = kstat_create("unix", 0, "kstat_headers", "kstat", KSTAT_TYPE_RAW,
		0, KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_VAR_SIZE);
	if (ksp) {
		/*
		 * NOTE: In general, all variable-size kstats are required to
		 * have per-kstat locking, so that readers can determine their
		 * size and read their contents safely.  However, the header
		 * kstat can only change under kstat_chain_lock (which is
		 * always held during any kstat read/write to prevent the kstat
		 * from going away), so no additional locking is required.
		 */
		ksp->ks_update = header_kstat_update;
		ksp->ks_snapshot = header_kstat_snapshot;
		kstat_install(ksp);
	} else {
		panic("cannot create kstat 'kstat_headers'");
	}

	ksp = kstat_create("unix", 0, "kstat_types", "kstat",
		KSTAT_TYPE_NAMED, KSTAT_NUM_TYPES, 0);
	if (ksp) {
		int i;
		kstat_named_t *kn = KSTAT_NAMED_PTR(ksp);

		for (i = 0; i < KSTAT_NUM_TYPES; i++) {
			kstat_named_init(&kn[i], kstat_data_type[i].name,
				KSTAT_DATA_ULONG);
			kn[i].value.ul = i;
		}
		kstat_install(ksp);
	}

	ksp = kstat_create("unix", 0, "sysinfo", "misc", KSTAT_TYPE_RAW,
		sizeof (sysinfo_t), KSTAT_FLAG_VIRTUAL);
	if (ksp) {
		ksp->ks_data = (void *) &sysinfo;
		kstat_install(ksp);
	}

	ksp = kstat_create("unix", 0, "vminfo", "vm", KSTAT_TYPE_RAW,
		sizeof (vminfo_t), KSTAT_FLAG_VIRTUAL);
	if (ksp) {
		ksp->ks_data = (void *) &vminfo;
		kstat_install(ksp);
	}

	ksp = kstat_create("unix", 0, "segmap", "vm", KSTAT_TYPE_NAMED,
		segmapcnt_ndata, KSTAT_FLAG_VIRTUAL);
	if (ksp) {
		ksp->ks_data = (void *) segmapcnt_ptr;
		kstat_install(ksp);
	}

	ksp = kstat_create("unix", 0, "biostats", "misc", KSTAT_TYPE_NAMED,
		biostats_ndata, KSTAT_FLAG_VIRTUAL);
	if (ksp) {
		ksp->ks_data = (void *) biostats_ptr;
		kstat_install(ksp);
	}

	ksp = kstat_create("unix", 0, "ncstats", "misc", KSTAT_TYPE_RAW,
		sizeof (struct ncstats), KSTAT_FLAG_VIRTUAL);
	if (ksp) {
		ksp->ks_data = (void *) &ncstats;
		kstat_install(ksp);
	}

#ifdef VAC
	ksp = kstat_create("unix", 0, "flushmeter", "hat", KSTAT_TYPE_RAW,
		sizeof (struct flushmeter), KSTAT_FLAG_VIRTUAL);
	if (ksp) {
		ksp->ks_data = (void *) &flush_cnt;
		kstat_install(ksp);
	}
#endif	/* VAC */

	ksp = kstat_create("unix", 0, "var", "misc", KSTAT_TYPE_RAW,
		sizeof (struct var), KSTAT_FLAG_VIRTUAL);
	if (ksp) {
		ksp->ks_data = (void *) &v;
		kstat_install(ksp);
	}

	ksp = kstat_create("unix", 0, "system_misc", "misc", KSTAT_TYPE_NAMED,
		sizeof (system_misc_kstat) / sizeof (kstat_named_t),
		KSTAT_FLAG_VIRTUAL);
	if (ksp) {
		ksp->ks_data = (void *) &system_misc_kstat;
		ksp->ks_update = system_misc_kstat_update;
		kstat_install(ksp);
	}

	ksp = kstat_create("unix", 0, "system_pages", "pages", KSTAT_TYPE_NAMED,
		sizeof (system_pages_kstat) / sizeof (kstat_named_t),
		KSTAT_FLAG_VIRTUAL);
	if (ksp) {
		ksp->ks_data = (void *) &system_pages_kstat;
		ksp->ks_update = system_pages_kstat_update;
		kstat_install(ksp);
	}

	ksp = kstat_create("poll", 0, "pollstats", "misc", KSTAT_TYPE_NAMED,
	    pollstats_ndata, KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE);

	if (ksp) {
		ksp->ks_data = pollstats_ptr;
		kstat_install(ksp);
	}
}

void
kstat_set_string(char *dst, char *src)
{
	bzero(dst, KSTAT_STRLEN);
	(void) strncpy(dst, src, KSTAT_STRLEN - 1);
}

void
kstat_named_init(kstat_named_t *knp, char *name, uchar_t data_type)
{
	kstat_set_string(knp->name, name);
	knp->data_type = data_type;
}

void
kstat_timer_init(kstat_timer_t *ktp, char *name)
{
	kstat_set_string(ktp->name, name);
}

/* ARGSUSED */
static int
default_kstat_update(kstat_t *ksp, int rw)
{
	return (0);
}

static int
default_kstat_snapshot(kstat_t *ksp, void *buf, int rw)
{
	kstat_io_t *kiop;
	hrtime_t cur_time;

	ksp->ks_snaptime = cur_time = gethrtime();

	if (rw == KSTAT_WRITE) {
		if (!(ksp->ks_flags & KSTAT_FLAG_WRITABLE))
			return (EACCES);
		bcopy(buf, ksp->ks_data, ksp->ks_data_size);
		return (0);
	}

	bcopy(ksp->ks_data, buf, ksp->ks_data_size);

	/*
	 * Apply kstat type-specific data massaging
	 */
	switch (ksp->ks_type) {

	case KSTAT_TYPE_IO:
		/*
		 * Normalize time units and deal with incomplete transactions
		 */
		kiop = (kstat_io_t *)buf;

		scalehrtime(&kiop->wtime);
		scalehrtime(&kiop->wlentime);
		scalehrtime(&kiop->wlastupdate);
		scalehrtime(&kiop->rtime);
		scalehrtime(&kiop->rlentime);
		scalehrtime(&kiop->rlastupdate);

		if (kiop->wcnt != 0) {
			hrtime_t wfix = cur_time - kiop->wlastupdate;
			kiop->wtime += wfix;
			kiop->wlentime += kiop->wcnt * wfix;
		}
		kiop->wlastupdate = cur_time;
		if (kiop->rcnt != 0) {
			hrtime_t rfix = cur_time - kiop->rlastupdate;
			kiop->rtime += rfix;
			kiop->rlentime += kiop->rcnt * rfix;
		}
		kiop->rlastupdate = cur_time;
		break;
	}
	return (0);
}

static int
header_kstat_update(kstat_t *header_ksp, int rw)
{
	int nkstats = 0;
	kstat_t *ksp;

	if (rw == KSTAT_WRITE)
		return (EACCES);

	for (ksp = kstat_chain; ksp; ksp = ksp->ks_next)
		nkstats++;

	header_ksp->ks_ndata = nkstats;
	header_ksp->ks_data_size = nkstats * sizeof (kstat_t);
	return (0);
}

/*
 * Copy out the data section of kstat 0, which consists of the list
 * of all kstat headers.  These headers *must* be copied out in order
 * of increasing KID.  Currently, we achieve this by noting that new
 * kstats (highest KID) are always added to the head of kstat_chain,
 * so we can simply fill the buffer backwards.
 */
static int
header_kstat_snapshot(kstat_t *header_ksp, void *buf, int rw)
{
	kstat_t *ksp, *kbuf;

	header_ksp->ks_snaptime = gethrtime();

	if (rw == KSTAT_WRITE)
		return (EACCES);

	kbuf = (kstat_t *)buf + header_ksp->ks_ndata;	/* backward copy */
	for (ksp = kstat_chain; ksp; ksp = ksp->ks_next) {
		kbuf--;
		bcopy(ksp, kbuf, sizeof (kstat_t));
	}
	return (0);
}

/* ARGSUSED */
static int
system_misc_kstat_update(kstat_t *ksp, int rw)
{
	if (rw == KSTAT_WRITE)
		return (EACCES);
	system_misc_kstat.ncpus.value.ui32		= (uint32_t)ncpus;
	system_misc_kstat.lbolt.value.ui32		= (uint32_t)lbolt;
	system_misc_kstat.deficit.value.ui32		= (uint32_t)deficit;
	system_misc_kstat.clk_intr.value.ui32		= (uint32_t)lbolt;
	system_misc_kstat.vac.value.ui32		= (uint32_t)vac;
	system_misc_kstat.nproc.value.ui32		= (uint32_t)nproc;
	system_misc_kstat.avenrun_1min.value.ui32	= (uint32_t)avenrun[0];
	system_misc_kstat.avenrun_5min.value.ui32	= (uint32_t)avenrun[1];
	system_misc_kstat.avenrun_15min.value.ui32	= (uint32_t)avenrun[2];
	system_misc_kstat.boot_time.value.ui32		= (uint32_t)boot_time;
	return (0);
}

extern caddr_t	econtig;
extern struct vnode kvp;

/* ARGSUSED */
static int
system_pages_kstat_update(kstat_t *ksp, int rw)
{
	kobj_stat_t kobj_stat;

	if (rw == KSTAT_WRITE) {
		return (EACCES);
	}

	kobj_stat_get(&kobj_stat);
	system_pages_kstat.physmem.value.ul	= (ulong_t)physmem;
	system_pages_kstat.nalloc.value.ul	= kobj_stat.nalloc;
	system_pages_kstat.nfree.value.ul	= kobj_stat.nfree;
	system_pages_kstat.nalloc_calls.value.ul = kobj_stat.nalloc_calls;
	system_pages_kstat.nfree_calls.value.ul	= kobj_stat.nfree_calls;
	system_pages_kstat.kernelbase.value.ul	= (ulong_t)KERNELBASE;
	system_pages_kstat.econtig.value.ul	= (ulong_t)econtig;
	system_pages_kstat.freemem.value.ul	= (ulong_t)freemem;
	system_pages_kstat.availrmem.value.ul	= (ulong_t)availrmem;
	system_pages_kstat.lotsfree.value.ul	= (ulong_t)lotsfree;
	system_pages_kstat.cachefree.value.ul	= (ulong_t)cachefree;
	system_pages_kstat.desfree.value.ul	= (ulong_t)desfree;
	system_pages_kstat.minfree.value.ul	= (ulong_t)minfree;
	system_pages_kstat.fastscan.value.ul	= (ulong_t)fastscan;
	system_pages_kstat.slowscan.value.ul	= (ulong_t)slowscan;
	system_pages_kstat.nscan.value.ul	= (ulong_t)nscan;
	system_pages_kstat.desscan.value.ul	= (ulong_t)desscan;
	system_pages_kstat.pagesfree.value.ul	= (ulong_t)freemem;
	system_pages_kstat.pageslocked.value.ul	= (ulong_t)(availrmem_initial -
	    availrmem);
	system_pages_kstat.pagestotal.value.ul	= (ulong_t)total_pages;
	system_pages_kstat.pp_kernel.value.ul	= (ulong_t)(availrmem_initial -
	    availrmem - k_anoninfo.ani_mem_resv - anon_segkp_pages_locked);
	return (0);
}

/*
 * kstat_create: allocate and initialize a kstat structure.  Or, if a
 * dormant kstat with the specified name exists, reactivate it.
 * On failure, returns NULL.  On success, returns pointer to kstat,
 * with KSTAT_FLAG_INVALID set (to be cleared by kstat_install).
 */
kstat_t *
kstat_create(char *ks_module, int ks_instance, char *ks_name, char *ks_class,
	uchar_t ks_type, uint_t ks_ndata, uchar_t ks_flags)
{
	size_t ks_data_size, allocation_size;
	kstat_t *new;
	ekstat_t *enew;
	char namebuf[KSTAT_STRLEN + 16];

	if (kstat_chain_id == -1) {
		cmn_err(CE_NOTE, "kstat_create('%s', %d, '%s'): "
			"cannot create kstat before kstat_init()",
			ks_module, ks_instance, ks_name);
		return (NULL);
	}

	/*
	 * If ks_name == NULL, set the ks_name to <module><instance>.
	 */
	if (ks_name == NULL) {
		char buf[KSTAT_STRLEN];
		kstat_set_string(buf, ks_module);
		(void) sprintf(namebuf, "%s%d", buf, ks_instance);
		ks_name = namebuf;
	}

	/*
	 * Make sure it's a valid kstat data type
	 */
	if (ks_type >= KSTAT_NUM_TYPES) {
		cmn_err(CE_WARN, "kstat_create('%s', %d, '%s'): "
			"invalid kstat type %d",
			ks_module, ks_instance, ks_name, ks_type);
		return (NULL);
	}

	/*
	 * Don't allow persistent virtual kstats -- it makes no sense.
	 * ks_data points to garbage when the client goes away.
	 */
	if ((ks_flags & KSTAT_FLAG_PERSISTENT) &&
	    (ks_flags & KSTAT_FLAG_VIRTUAL)) {
		cmn_err(CE_WARN, "kstat_create('%s', %d, '%s'): "
			"cannot create persistent virtual kstat",
			ks_module, ks_instance, ks_name);
		return (NULL);
	}

	/*
	 * Don't allow variable-size physical kstats, since the framework's
	 * memory allocation for physical kstat data is fixed at creation time.
	 */
	if ((ks_flags & KSTAT_FLAG_VAR_SIZE) &&
	    !(ks_flags & KSTAT_FLAG_VIRTUAL)) {
		cmn_err(CE_WARN, "kstat_create('%s', %d, '%s'): "
			"cannot create variable-size physical kstat",
			ks_module, ks_instance, ks_name);
		return (NULL);
	}

	/*
	 * Make sure the number of data fields is within legal range
	 */
	if (ks_ndata < kstat_data_type[ks_type].min_ndata ||
	    ks_ndata > kstat_data_type[ks_type].max_ndata) {
		cmn_err(CE_WARN, "kstat_create('%s', %d, '%s'): "
			"ks_ndata=%d out of range [%d, %d]",
			ks_module, ks_instance, ks_name, (int)ks_ndata,
			kstat_data_type[ks_type].min_ndata,
			kstat_data_type[ks_type].max_ndata);
		return (NULL);
	}

	ks_data_size = kstat_data_type[ks_type].size * ks_ndata;

	mutex_enter(&kstat_chain_lock);

	/*
	 * If the named kstat already exists and is dormant, reactivate it.
	 */
	new = kstat_lookup_byname(ks_module, ks_instance, ks_name);
	if (new != NULL) {
		if (!(new->ks_flags & KSTAT_FLAG_DORMANT)) {
			/*
			 * The named kstat exists but is not dormant --
			 * this is a kstat namespace collision.
			 */
			mutex_exit(&kstat_chain_lock);
			cmn_err(CE_WARN, "kstat_create('%s', %d, '%s'): "
				"kstat namespace collision with KID %d",
				ks_module, ks_instance, ks_name,
				(int)new->ks_kid);
			return (NULL);
		}
		if ((strcmp(new->ks_class, ks_class) != 0) ||
		    (new->ks_type != ks_type) ||
		    (new->ks_ndata != ks_ndata) ||
		    (ks_flags & KSTAT_FLAG_VIRTUAL)) {
			/*
			 * The name is the same, but the other key parameters
			 * differ from those of the dormant kstat -- bogus.
			 */
			mutex_exit(&kstat_chain_lock);
			cmn_err(CE_WARN, "kstat_create('%s', %d, '%s'): "
				"invalid reactivation of dormant kstat",
				ks_module, ks_instance, ks_name);
			return (NULL);
		}
		/*
		 * Return dormant kstat pointer to caller.  As usual,
		 * the kstat must be marked invalid until kstat_install().
		 */
		new->ks_flags |= KSTAT_FLAG_INVALID;
		mutex_exit(&kstat_chain_lock);
		return (new);
	}

	/*
	 * Allocate memory for the new kstat header and, if this is a physical
	 * kstat, the data section.  The framework's extended kstat header
	 * keeps track of the total allocation size, so we know how much to
	 * kmem_free() in kstat_delete().  We can safely allocate the header
	 * and the data in one shot, since the header is double-aligned.
	 */
	allocation_size = sizeof (ekstat_t) +
		((ks_flags & KSTAT_FLAG_VIRTUAL) ? 0 : ks_data_size);
	if ((enew = kmem_zalloc(allocation_size, KM_NOSLEEP)) == NULL) {
		mutex_exit(&kstat_chain_lock);
		cmn_err(CE_NOTE, "kstat_create('%s', %d, '%s'): "
			"insufficient kernel memory",
			ks_module, ks_instance, ks_name);
		return (NULL);
	}
	enew->allocation_size = allocation_size;

	/*
	 * Initialize as many fields as we can.  The caller may reset
	 * ks_lock, ks_update, ks_private, and ks_snapshot as necessary.
	 * Creators of virtual kstats may also reset ks_data.  It is
	 * also up to the caller to initialize the kstat data section,
	 * if necessary.  All initialization must be complete before
	 * calling kstat_install().
	 */
	new = &enew->ks;
	new->ks_crtime		= gethrtime();
	new->ks_kid		= kstat_chain_id++;
	kstat_set_string(new->ks_module, ks_module);
	new->ks_instance	= ks_instance;
	kstat_set_string(new->ks_name, ks_name);
	new->ks_type		= ks_type;
	kstat_set_string(new->ks_class, ks_class);
	new->ks_flags		= ks_flags | KSTAT_FLAG_INVALID;
	if (ks_flags & KSTAT_FLAG_VIRTUAL)
		new->ks_data	= NULL;
	else
		new->ks_data	= (void *) (enew + 1);
	new->ks_ndata		= ks_ndata;
	new->ks_data_size	= ks_data_size;
	new->ks_snaptime	= new->ks_crtime;
	new->ks_update		= default_kstat_update;
	new->ks_private		= NULL;
	new->ks_snapshot	= default_kstat_snapshot;
	new->ks_lock		= NULL;

	/*
	 * Add new kstat to kstat chain.
	 */
	new->ks_next	= kstat_chain;
	kstat_chain	= new;

	/*
	 * Add new kstat to framework's internal hash tables.
	 */
	kstat_hash_insert(enew, KSTAT_HASH_BYKID);
	kstat_hash_insert(enew, KSTAT_HASH_BYNAME);

	mutex_exit(&kstat_chain_lock);
	return (new);
}

/*
 * kstat_install: activate a fully initialized kstat
 */
void
kstat_install(kstat_t *new)
{
	mutex_enter(&kstat_chain_lock);
	/*
	 * If this is a variable-size kstat, it MUST provide kstat data locking
	 * to prevent data-size races with kstat readers.  KID 0 (the header
	 * kstat) is an exception: its data is protected by kstat_chain_lock,
	 * which is always held during kstat reads.
	 */
	if ((new->ks_flags & KSTAT_FLAG_VAR_SIZE) &&
	    (new->ks_lock == NULL) && (new->ks_kid != 0)) {
		mutex_exit(&kstat_chain_lock);
		panic("kstat_install('%s', %d, '%s'): "
		    "cannot create variable-size kstat without data lock",
		    new->ks_module, new->ks_instance, new->ks_name);
	}

	if (new->ks_flags & KSTAT_FLAG_DORMANT) {
		/*
		 * We are reactivating a dormant kstat.  Initialize the
		 * caller's underlying data to the value it had when the
		 * kstat went dormant, and mark the kstat as active.
		 * Grab the provider's kstat lock if it's not already held.
		 */
		kmutex_t *lp = new->ks_lock;
		if (lp != NULL && MUTEX_NOT_HELD(lp)) {
			mutex_enter(lp);
			(void) KSTAT_UPDATE(new, KSTAT_WRITE);
			mutex_exit(lp);
		} else {
			(void) KSTAT_UPDATE(new, KSTAT_WRITE);
		}
		new->ks_flags &= ~KSTAT_FLAG_DORMANT;
	}
	new->ks_flags &= ~KSTAT_FLAG_INVALID;
	mutex_exit(&kstat_chain_lock);
}

/*
 * kstat_delete: remove a kstat from the system.  Or, if it's a persistent
 * kstat, just update the data and mark it as dormant.
 */
void
kstat_delete(kstat_t *kspdel)
{
	kmutex_t *lp = kspdel->ks_lock;

	if (lp != NULL && MUTEX_HELD(lp)) {
		panic("kstat_delete(%p): caller holds data lock %p",
		    (void *)kspdel, (void *)lp);
	}

	mutex_enter(&kstat_chain_lock);

	if (kspdel->ks_flags & KSTAT_FLAG_PERSISTENT) {
		/*
		 * Update the data one last time, so that all activity
		 * prior to going dormant has been accounted for.
		 */
		KSTAT_ENTER(kspdel);
		(void) KSTAT_UPDATE(kspdel, KSTAT_READ);
		KSTAT_EXIT(kspdel);

		/*
		 * Mark the kstat as dormant and restore caller-modifiable
		 * fields to default values, so the kstat is readable during
		 * the dormant phase.
		 */
		kspdel->ks_flags	|= KSTAT_FLAG_DORMANT;
		kspdel->ks_lock		= NULL;
		kspdel->ks_update	= default_kstat_update;
		kspdel->ks_private	= NULL;
		kspdel->ks_snapshot	= default_kstat_snapshot;
		mutex_exit(&kstat_chain_lock);
		return;
	}

	/*
	 * This is an ephemeral kstat, so we actually delete it.
	 */
	if (kspdel == kstat_chain) {
		kstat_chain = kspdel->ks_next;
	} else {
		kstat_t *ksp;
		for (ksp = kstat_chain; ksp; ksp = ksp->ks_next) {
			if (ksp->ks_next == kspdel) {
				ksp->ks_next = kspdel->ks_next;
				break;
			}
		}
		if (ksp == NULL) {
			/*
			 * The kstat doesn't exist in the kstat chain.
			 */
			mutex_exit(&kstat_chain_lock);
			cmn_err(CE_WARN, "kstat_delete(%p): "
			    "does not exist in kstat chain", (void *)kspdel);
			return;
		}
	}

	/*
	 * Delete the kstat from the framework's internal hash tables,
	 * free the allocated memory, and increment kstat_chain_id so
	 * /dev/kstat clients can detect the event.
	 */
	kstat_hash_delete((ekstat_t *)kspdel, KSTAT_HASH_BYKID);
	kstat_hash_delete((ekstat_t *)kspdel, KSTAT_HASH_BYNAME);

	kmem_free(kspdel, ((ekstat_t *)kspdel)->allocation_size);
	kstat_chain_id++;
	mutex_exit(&kstat_chain_lock);
}

/*
 * hash a kstat by (module, instance, name)
 */
static int
kstat_namehash_func(ekstat_t *eksp)
{
	char c, *cp;
	int h = eksp->ks.ks_instance;

	for (cp = eksp->ks.ks_name; (c = *cp) != 0; cp++)
		h += c;
	return (h & KSTAT_HASH_MASK);
}

/*
 * hash a kstat by KID
 */
static int
kstat_kidhash_func(ekstat_t *eksp)
{
	return ((eksp->ks.ks_kid) & KSTAT_HASH_MASK);
}

/*
 * Add a new entry to a kstat hash table
 */
static void
kstat_hash_insert(ekstat_t *enew, int hash_type)
{
	int index;

	index = (*kstat_hash_func[hash_type])(enew);
	enew->hash_next[hash_type] = kstat_hash_table[hash_type][index];
	kstat_hash_table[hash_type][index] = enew;
}

/*
 * Delete an entry from a kstat hash table
 */
static void
kstat_hash_delete(ekstat_t *edel, int hash_type)
{
	int index;
	ekstat_t *eksp;

	index = (*kstat_hash_func[hash_type])(edel);
	eksp = kstat_hash_table[hash_type][index];

	if (eksp == edel) {
		kstat_hash_table[hash_type][index] = edel->hash_next[hash_type];
	} else {
		while (eksp != NULL) {
			ekstat_t *enext = eksp->hash_next[hash_type];
			if (enext == edel) {
				eksp->hash_next[hash_type] =
				    edel->hash_next[hash_type];
				break;
			}
			eksp = enext;
		}
		if (eksp == NULL) {
			cmn_err(CE_WARN,
			    "can't find kstat %p in hash table %d",
			    (void *)edel, hash_type);
			return;
		}
	}
}

/*
 * Look up an entry in a kstat hash table.  kstat_chain_lock must be
 * held on entry.
 */
static kstat_t *
kstat_hash_lookup(ekstat_t *esrch, int hash_type)
{
	ekstat_t *eksp;
	int index;

	ASSERT(MUTEX_HELD(&kstat_chain_lock));

	index = (*kstat_hash_func[hash_type])(esrch);
	eksp = kstat_hash_table[hash_type][index];

	switch (hash_type) {

	    case KSTAT_HASH_BYNAME:
		while (eksp != NULL) {
			if (eksp->ks.ks_instance == esrch->ks.ks_instance &&
			    strcmp(eksp->ks.ks_name, esrch->ks.ks_name) == 0 &&
			    strcmp(eksp->ks.ks_module,
				esrch->ks.ks_module) == 0) {
					return (&eksp->ks);
			}
			eksp = eksp->hash_next[hash_type];
		}
		break;

	    case KSTAT_HASH_BYKID:
		while (eksp != NULL) {
			if (eksp->ks.ks_kid == esrch->ks.ks_kid) {
				return (&eksp->ks);
			}
			eksp = eksp->hash_next[hash_type];
		}
		break;
	}
	return (NULL);
}

/*
 * Find a kstat by (module, instance, name).  kstat_chain_lock must be
 * held on entry.
 */
kstat_t *
kstat_lookup_byname(char *ks_module, int ks_instance, char *ks_name)
{
	ekstat_t eks;

	ASSERT(MUTEX_HELD(&kstat_chain_lock));

	kstat_set_string(eks.ks.ks_module, ks_module);
	eks.ks.ks_instance = ks_instance;
	kstat_set_string(eks.ks.ks_name, ks_name);
	return (kstat_hash_lookup(&eks, KSTAT_HASH_BYNAME));
}

/*
 * Find a kstat by KID.  kstat_chain_lock must be held on entry.
 */
kstat_t *
kstat_lookup_bykid(kid_t ks_kid)
{
	ekstat_t eks;

	ASSERT(MUTEX_HELD(&kstat_chain_lock));

	eks.ks.ks_kid = ks_kid;
	return (kstat_hash_lookup(&eks, KSTAT_HASH_BYKID));
}

/*
 * The sparc V9 versions of these routines can be much cheaper than
 * the poor 32-bit compiler can comprehend, so they're in sparcv9_subr.s.
 * For simplicity, however, we always feed the C versions to lint.
 */
#if !defined(__sparcv9cpu) || defined(lint) || defined(__lint)

void
kstat_waitq_enter(kstat_io_t *kiop)
{
	hrtime_t new, delta;
	ulong_t wcnt;

	new = gethrtime_unscaled();
	delta = new - kiop->wlastupdate;
	kiop->wlastupdate = new;
	wcnt = kiop->wcnt++;
	if (wcnt != 0) {
		kiop->wlentime += delta * wcnt;
		kiop->wtime += delta;
	}
}

void
kstat_waitq_exit(kstat_io_t *kiop)
{
	hrtime_t new, delta;
	ulong_t wcnt;

	new = gethrtime_unscaled();
	delta = new - kiop->wlastupdate;
	kiop->wlastupdate = new;
	wcnt = kiop->wcnt--;
	ASSERT((int)wcnt > 0);
	kiop->wlentime += delta * wcnt;
	kiop->wtime += delta;
}

void
kstat_runq_enter(kstat_io_t *kiop)
{
	hrtime_t new, delta;
	ulong_t rcnt;

	new = gethrtime_unscaled();
	delta = new - kiop->rlastupdate;
	kiop->rlastupdate = new;
	rcnt = kiop->rcnt++;
	if (rcnt != 0) {
		kiop->rlentime += delta * rcnt;
		kiop->rtime += delta;
	}
}

void
kstat_runq_exit(kstat_io_t *kiop)
{
	hrtime_t new, delta;
	ulong_t rcnt;

	new = gethrtime_unscaled();
	delta = new - kiop->rlastupdate;
	kiop->rlastupdate = new;
	rcnt = kiop->rcnt--;
	ASSERT((int)rcnt > 0);
	kiop->rlentime += delta * rcnt;
	kiop->rtime += delta;
}

void
kstat_waitq_to_runq(kstat_io_t *kiop)
{
	hrtime_t new, delta;
	ulong_t wcnt, rcnt;

	new = gethrtime_unscaled();

	delta = new - kiop->wlastupdate;
	kiop->wlastupdate = new;
	wcnt = kiop->wcnt--;
	ASSERT((int)wcnt > 0);
	kiop->wlentime += delta * wcnt;
	kiop->wtime += delta;

	delta = new - kiop->rlastupdate;
	kiop->rlastupdate = new;
	rcnt = kiop->rcnt++;
	if (rcnt != 0) {
		kiop->rlentime += delta * rcnt;
		kiop->rtime += delta;
	}
}

void
kstat_runq_back_to_waitq(kstat_io_t *kiop)
{
	hrtime_t new, delta;
	ulong_t wcnt, rcnt;

	new = gethrtime_unscaled();

	delta = new - kiop->rlastupdate;
	kiop->rlastupdate = new;
	rcnt = kiop->rcnt--;
	ASSERT((int)rcnt > 0);
	kiop->rlentime += delta * rcnt;
	kiop->rtime += delta;

	delta = new - kiop->wlastupdate;
	kiop->wlastupdate = new;
	wcnt = kiop->wcnt++;
	if (wcnt != 0) {
		kiop->wlentime += delta * wcnt;
		kiop->wtime += delta;
	}
}

#endif

void
kstat_timer_start(kstat_timer_t *ktp)
{
	ktp->start_time = gethrtime();
}

void
kstat_timer_stop(kstat_timer_t *ktp)
{
	hrtime_t	etime;
	u_longlong_t	num_events;

	ktp->stop_time = etime = gethrtime();
	etime -= ktp->start_time;
	num_events = ktp->num_events;
	if (etime < ktp->min_time || num_events == 0)
		ktp->min_time = etime;
	if (etime > ktp->max_time)
		ktp->max_time = etime;
	ktp->elapsed_time += etime;
	ktp->num_events = num_events + 1;
}
