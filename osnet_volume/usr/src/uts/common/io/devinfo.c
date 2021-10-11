/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)devinfo.c	1.31	99/09/10 SMI"

/*
 * driver for accessing kernel devinfo tree.
 */
#include <sys/types.h>
#include <sys/pathname.h>
#include <sys/debug.h>
#include <sys/autoconf.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/modctl.h>
#include <sys/stat.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/esunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/ndi_impldefs.h>
#include <sys/devinfo_impl.h>
#include <sys/thread.h>
#include <sys/cladm.h>
#include <sys/dc_ki.h>

#ifdef DEBUG
static int di_debug;
#define	dcmn_err(args) if (di_debug >= 1) cmn_err args
#define	dcmn_err2(args) if (di_debug >= 2) cmn_err args
#define	dcmn_err3(args) if (di_debug >= 3) cmn_err args
#else
#define	dcmn_err(args) /* nothing */
#define	dcmn_err2(args) /* nothing */
#define	dcmn_err3(args) /* nothing */
#endif

/*
 * We partition the space of devinfo minor nodes equally between the full and
 * unprivileged versions of the driver.  The even-numbered minor nodes are the
 * full version, while the odd-numbered ones are the read-only version.
 */
static int di_max_opens = 32;

#define	DI_FULL_PARENT		0
#define	DI_READONLY_PARENT	1
#define	DI_NODE_SPECIES		2
#define	DI_UNPRIVILEGED_NODE(x)	(((x) % 2) != 0)

/*
 * Keep max alignment so we can move snapshot to different platforms
 */
#define	DI_ALIGN(addr)	((addr + 7l) & ~7l)

/*
 * To avoid wasting memory, make a linked list of chunks.
 * Size of each chunk is buf_size.
 */
struct di_cache {
	struct di_cache *next;	/* link to next chunk */
	char *buf;		/* contiguous kernel memory */
	size_t buf_size;	/* size of buf in bytes */
	devmap_cookie_t cook;	/* cookie from ddi_umem_alloc */
};

/*
 * This is a stack for walking the tree without using recursion.
 * When the devinfo tree height is above some small size, one
 * gets watchdog resets on sun4m.
 */
struct di_stack {
	di_off_t	*offset[MAX_TREE_DEPTH];
	struct dev_info *dip[MAX_TREE_DEPTH];
	int		depth;	/* depth of current node to be copied */
};

#define	TOP_OFFSET(stack)	((stack)->offset[(stack)->depth - 1])
#define	TOP_NODE(stack)		((stack)->dip[(stack)->depth - 1])
#define	PARENT_OFFSET(stack)	((stack)->offset[(stack)->depth - 2])
#define	EMPTY_STACK(stack)	((stack)->depth == 0)
#define	POP_STACK(stack)	(stack)->depth--
#define	PUSH_STACK(stack, node, offp) \
	ASSERT(node != NULL); \
	(stack)->dip[(stack)->depth] = (node); \
	(stack)->offset[(stack)->depth] = (offp); \
	((stack)->depth)++

/*
 * This keeps track of linked lists of dips associated with each driver
 * and the corresponding offset. Three things are stored:
 * (1) The value of each devnames.dn_head and offset in snapshot.
 * (2) The value of devi_next and offset in snapshot.
 * (3) The value of dip and offset.
 * The lists are resolved in the end by comparing pointer values.
 */
struct di_list {
	struct di_list	*next;
	di_off_t	*offp;	/* location for storing offset */
	struct dev_info *dip;	/* corresponding dip */
};

/*
 * Soft state associated with each instance of driver open.
 */
static struct di_state {
	di_off_t cache_size;	/* total # bytes in cache	*/
	struct di_cache *cache;	/* head of cache page chunks	*/
	uint_t command;		/* command from ioctl		*/
	int *drivers_held;	/* drivers held during snapshot */
	struct di_list **nxt_list;	/* dn_head & devi_next	*/
	struct di_list **dip_list;	/* dips			*/
} **di_states;

static kmutex_t di_open_lock;	/* serialize instance assignment */

static int di_open(dev_t *, int, int, cred_t *);
static int di_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int di_close(dev_t, int, int, cred_t *);
static int di_info(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int di_attach(dev_info_t *, ddi_attach_cmd_t);
static int di_detach(dev_info_t *, ddi_detach_cmd_t);

static di_off_t di_copyformat(di_off_t, struct di_state *, intptr_t, int);
static di_off_t di_snapshot(struct di_state *);
static di_off_t di_copydevnm(di_off_t *, struct di_state *);
static di_off_t di_copytree(struct dev_info *, di_off_t *, struct di_state *);
static di_off_t di_copynode(struct di_stack *, struct di_state *);
static di_off_t di_getmdata(struct ddi_minor_data *, di_off_t *,
    struct di_state *);
static void di_fixmdata(di_off_t, struct di_state *);
static di_off_t di_getppdata(struct dev_info *, di_off_t *, struct di_state *);
static di_off_t di_getdpdata(struct dev_info *, di_off_t *, struct di_state *);
static di_off_t di_getprop(struct ddi_prop *, di_off_t *,
    struct di_state *, struct dev_info *, major_t, int);
static void di_initlist(struct di_state *);
static void di_freelist(struct di_state *);
static void di_insertlist(struct di_list **, di_off_t *, struct dev_info *);
static void di_matchlist(struct di_state *);
static void di_getmem(struct di_state *, size_t);
static void di_freemem(struct di_state *);
static di_off_t di_checkmem(struct di_state *, di_off_t, size_t);
static dev_info_t *di_hold_drivers(struct di_state *, char *, int);
static void di_rele_drivers(struct di_state *);
static caddr_t di_cache_addr(struct di_state *, di_off_t);
static dev_info_t *di_path_to_devinfo(struct di_state *, char *, int);

static struct cb_ops di_cb_ops = {
	di_open,		/* open */
	di_close,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	di_ioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* prop_op */
	NULL,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

static struct dev_ops di_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	di_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	di_attach,		/* attach */
	di_detach,		/* detach */
	nodev,			/* reset */
	&di_cb_ops,		/* driver operations */
	NULL			/* bus operations */
};

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops,
	"DEVINFO Driver 1.31",
	&di_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

int
_init(void)
{
	int	error;

	mutex_init(&di_open_lock, NULL, MUTEX_DRIVER, NULL);

	error = mod_install(&modlinkage);
	if (error != 0) {
		mutex_destroy(&di_open_lock);
		return (error);
	}

	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	int	error;

	error = mod_remove(&modlinkage);
	if (error != 0) {
		return (error);
	}

	mutex_destroy(&di_open_lock);
	return (0);
}

static dev_info_t *di_dip;

/*ARGSUSED*/
static int
di_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error = DDI_FAILURE;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)di_dip;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		/*
		 * All dev_t's map to the same, single instance.
		 */
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		break;
	}

	return (error);
}

static int
di_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int error = DDI_FAILURE;

	switch (cmd) {
	case DDI_ATTACH:
		di_states = kmem_zalloc(
		    di_max_opens * sizeof (struct di_state *), KM_SLEEP);

		if (ddi_create_minor_node(dip, "devinfo", S_IFCHR,
		    DI_FULL_PARENT, DDI_PSEUDO, NULL) == DDI_FAILURE ||
		    ddi_create_minor_node(dip, "devinfo,ro", S_IFCHR,
		    DI_READONLY_PARENT, DDI_PSEUDO, NULL) == DDI_FAILURE) {
			kmem_free(di_states,
			    di_max_opens * sizeof (struct di_state *));
			ddi_remove_minor_node(dip, NULL);
			error = DDI_FAILURE;
		} else {
			di_dip = dip;
			ddi_report_dev(dip);

			error = DDI_SUCCESS;
		}
		break;
	default:
		error = DDI_FAILURE;
		break;
	}

	return (error);
}

static int
di_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int error = DDI_FAILURE;

	switch (cmd) {
	case DDI_DETACH:
		ddi_remove_minor_node(dip, NULL);
		di_dip = NULL;
		kmem_free(di_states, di_max_opens * sizeof (struct di_state *));

		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
		break;
	}

	return (error);
}

/*
 * Allow multiple opens by tweaking the dev_t such that it looks like each
 * open is getting a different minor device.  Each minor gets a separate
 * entry in the di_states[] table.  Based on the original minor number, we
 * discriminate opens of the full and read-only nodes.  If all of the instances
 * of the selected minor node are currently open, we return EAGAIN.
 */
/*ARGSUSED*/
static int
di_open(dev_t *devp, int flag, int otyp, cred_t *credp)
{
	int m;
	minor_t minor_parent = getminor(*devp);

	if (minor_parent != DI_FULL_PARENT &&
	    minor_parent != DI_READONLY_PARENT)
		return (ENXIO);

	mutex_enter(&di_open_lock);

	for (m = minor_parent; m < di_max_opens; m += DI_NODE_SPECIES) {
		if (di_states[m] != NULL)
			continue;

		di_states[m] = kmem_zalloc(sizeof (struct di_state), KM_SLEEP);
		break;	/* It's ours. */
	}

	if (m >= di_max_opens) {
		/*
		 * maximum open instance for device reached
		 */
		mutex_exit(&di_open_lock);
		dcmn_err((CE_WARN, "devinfo: maximum devinfo open reached\n"));
		return (EAGAIN);
	}
	mutex_exit(&di_open_lock);

	ASSERT(m < di_max_opens);
	*devp = makedevice(getmajor(*devp), (minor_t)(m + DI_NODE_SPECIES));

	dcmn_err((CE_CONT, "di_open: thread = %p, assigned minor = %d\n",
		(void *)curthread, m + DI_NODE_SPECIES));

	return (0);
}

/*ARGSUSED*/
static int
di_close(dev_t dev, int flag, int otype, cred_t *cred_p)
{
	struct di_state *st;
	int m = (int)getminor(dev) - DI_NODE_SPECIES;

	st = di_states[m];
	ASSERT(m < di_max_opens && st != NULL);

	di_freemem(st);
	kmem_free(st, sizeof (struct di_state));

	/*
	 * empty slot in state table
	 */
	mutex_enter(&di_open_lock);
	di_states[m] = NULL;
	dcmn_err((CE_CONT, "di_close: thread = %p, assigned minor = %d\n",
		(void *)curthread, m + DI_NODE_SPECIES));
	mutex_exit(&di_open_lock);

	return (0);
}

/*ARGSUSED*/
static int
di_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp, int *rvalp)
{
	di_off_t off;
	struct di_all *all;
	struct di_state *st;
	int m = (int)getminor(dev) - DI_NODE_SPECIES;
	uint_t circular_count;

	int loaded;
	major_t i;
	char *drv_name;
	size_t map_size, size;
	struct di_cache *dcp;

	if (m >= di_max_opens) {
		return (ENXIO);
	}

	st = di_states[m];
	ASSERT(st != NULL);

	dcmn_err2((CE_CONT, "di_ioctl: mode = %x, cmd = %x\n", mode, cmd));

	switch (cmd) {
	case DINFOIDENT:
		/*
		 * This is called from di_init to verify that the driver
		 * opened is indeed devinfo. The purpose is to guard against
		 * sending ioctl to an unknown driver in case of an
		 * unresolved major number conflict during bfu.
		 */
		*rvalp = DI_MAGIC;
		return (0);

	case DINFOLODRV:
		/*
		 * Hold an installed driver and return the result
		 */
		if (DI_UNPRIVILEGED_NODE(m)) {
			/*
			 * Only the fully enabled instances may issue
			 * DINFOLDDRV.
			 */
			di_freemem(st);
			return (EACCES);
		}

		drv_name = kmem_zalloc(MAXPATHLEN, KM_SLEEP);
		if (ddi_copyin((void *)arg, drv_name, MAXPATHLEN,
		    mode) != 0) {
			kmem_free(drv_name, MAXPATHLEN);
			return (EFAULT);
		}

		i = ddi_name_to_major(drv_name);
		kmem_free(drv_name, MAXPATHLEN);

		if (i == (major_t)-1) {
			return (ENXIO);
		}

		loaded = CB_DRV_INSTALLED(devopsp[i]);
		if (ddi_hold_installed_driver(i) == NULL) {
			return (ENXIO);
		}

		if (loaded) {
			(void) e_ddi_deferred_attach(i, NODEV);
		}

		/*
		 * see bugid 4172199
		 */
		i_ndi_devi_config_by_major(i);
		ddi_rele_driver(i);

		return (0);

	case DINFOUSRLD:
		/*
		 * The case for copying snapshot to userland
		 *
		 * check if a snapshot exists at this minor
		 */
		if (st->cache_size == 0) {
			return (EINVAL);
		}

		map_size = ((struct di_all *)di_cache_addr(st, 0))->map_size;
		if (map_size == 0) {
			return (EFAULT);
		}

		/*
		 * copyout the snapshot
		 */
		map_size = (map_size + PAGEOFFSET) & PAGEMASK;

		/*
		 * Return the map size, so caller may do a sanity
		 * check against the return value of snapshot ioctl()
		 */
		*rvalp = (int)map_size;

		/*
		 * Copy one chunk at a time
		 */
		off = 0;
		dcp = st->cache;
		while (map_size) {
			size = dcp->buf_size;
			if (map_size <= size) {
				size = map_size;
			}

			if (ddi_copyout(di_cache_addr(st, off),
				(void *) (arg + off), size, mode) != 0) {
				return (EFAULT);
			}

			map_size -= size;
			off += size;
			dcp = dcp->next;
		}

		di_freemem(st);
		return (0);

	default:
		if ((cmd & ~0xff) != DIIOC) {
			/*
			 * Invalid ioctl command
			 */
			return (ENOTTY);
		}
		/*
		 * take a snaphsot
		 */
		st->command = cmd & 0xff;
		/*FALLTHROUGH*/
	}

	/*
	 * Obtain enough memory to hold header + rootpath.  We prevent kernel
	 * memory exhaustion by freeing any previously allocated snapshot and
	 * refusing the operation; otherwise we would be allowing ioctl(),
	 * ioctl(), ioctl(), ..., panic.
	 */
	if (st->cache_size != 0) {
		di_freemem(st);
		return (EINVAL);
	}

	size = sizeof (struct di_all) + MAXPATHLEN;
	if (size < PAGESIZE)
		size = PAGESIZE;
	di_getmem(st, size);

	all = (struct di_all *)di_cache_addr(st, 0);
	all->devcnt = devcnt;
	all->command = st->command;
	all->version = DI_SNAPSHOT_VERSION_0;

	/*
	 * Note the endianness in case we need to transport snapshot
	 * over the network.
	 */
#if defined(_LITTLE_ENDIAN)
	all->endianness = DI_LITTLE_ENDIAN;
#else
	all->endianness = DI_BIG_ENDIAN;
#endif

	/*
	 * Now, pull ioctl arguments into kernel, store at the beginning of the
	 * snapshot.
	 */
	if (copyinstr((void *)arg, all->root_path, MAXPATHLEN, &size) != 0) {
		di_freemem(st);
		return (EFAULT);
	}

	if ((st->command & (DINFOPRIVDATA | DINFOFORCE)) != 0 &&
	    DI_UNPRIVILEGED_NODE(m)) {
		/*
		 * Only the fully enabled version may force load drivers or read
		 * the parent private data from a driver.
		 */
		di_freemem(st);
		return (EACCES);
	}

	off = DI_ALIGN(sizeof (struct di_all) + size);

	/*
	 * Do we need private data?  This operation is not supported in the
	 * multidata model.
	 */
	if (st->command & DINFOPRIVDATA) {
		arg += MAXPATHLEN;	/* private data follows pathname */

#ifdef _MULTI_DATAMODEL
		switch (ddi_model_convert_from(mode & FMODELS)) {
		case DDI_MODEL_ILP32: {
			di_freemem(st);
			return (EINVAL);
		}
		case DDI_MODEL_NONE:
			if ((off = di_copyformat(off, st, arg, mode)) == 0) {
				di_freemem(st);
				return (EFAULT);
			}
			break;
		}
#else /* !_MULTI_DATAMODEL */
		if ((off = di_copyformat(off, st, arg, mode)) == 0) {
			di_freemem(st);
			return (EFAULT);
		}
#endif /* _MULTI_DATAMODEL */
	}

	all->top_devinfo = DI_ALIGN(off);

	/*
	 * actually take the snapshot
	 */
	i_ndi_block_device_tree_changes(&circular_count);

	if (*rvalp = di_snapshot(st)) {
		all->map_size = *rvalp;
	} else {
		di_freemem(st);
	}

	i_ndi_allow_device_tree_changes(circular_count);

	return (0);
}

/*
 * Allocate memory for array of ptrs to each per-driver list
 */
static void
di_initlist(struct di_state *st)
{
	dcmn_err2((CE_CONT, "di_initlist:\n"));

	st->nxt_list = (struct di_list **)
		kmem_zalloc(devcnt * sizeof (struct di_list *), KM_SLEEP);
	st->dip_list = (struct di_list **)
		kmem_zalloc(devcnt * sizeof (struct di_list *), KM_SLEEP);
}

/*
 * Free memory associated with array of ptrs to each per-driver list
 */
static void
di_freelist(struct di_state *st)
{
	int i;
	struct di_list *tmp;

	dcmn_err2((CE_CONT, "di_freelist:\n"));

	/*
	 * First, free elements in each linked list
	 */
	for (i = 0; i < devcnt; i++) {
		while (st->nxt_list[i]) {
			tmp = st->nxt_list[i];
			st->nxt_list[i] = tmp->next;
			kmem_free(tmp, sizeof (struct di_list));
		}

		while (st->dip_list[i]) {
			tmp = st->dip_list[i];
			st->dip_list[i] = tmp->next;
			kmem_free(tmp, sizeof (struct di_list));
		}
	}

	/*
	 * Free the pointer array allocated in di_initlist()
	 */
	kmem_free(st->nxt_list, devcnt * sizeof (struct di_list *));
	kmem_free(st->dip_list, devcnt * sizeof (struct di_list *));
}

/*
 * Inset a pair of (offp, dip) at the head of list
 */
static void
di_insertlist(struct di_list **head, di_off_t *offp, struct dev_info *dip)
{
	struct di_list *tmp;

	dcmn_err2((CE_CONT,
		"di_insertlist: head = %p offp = %p dip = %p (%s)\n",
		(void *)head, (void *)offp, (void *)dip, dip->devi_node_name));

	tmp = (struct di_list *)kmem_alloc(sizeof (struct di_list), KM_SLEEP);
	tmp->offp = offp;
	tmp->dip = dip;
	tmp->next = *head;
	*head = tmp;
}

/*
 * Match up st->nxt_list & st->dip_list, so per driver info is
 * retained in the snapshot.
 */
static void
di_matchlist(struct di_state *st)
{
	int i;
	struct di_list *tmp, *dtmp;

	dcmn_err2((CE_CONT, "di_matchlist:\n"));

	for (i = 0; i < devcnt; i++) {
		tmp = st->nxt_list[i];

		while (tmp) {
			dcmn_err2((CE_CONT,
			    "di_matchlist: nxt_list[%d]: "
			    "dip = %p %s%d off=%x\n",
			    i, (void *)tmp->dip, (tmp->dip->devi_node_name ?
			    tmp->dip->devi_node_name: ""),
			    tmp->dip->devi_instance, *(tmp->offp)));

			/*
			 * look for matching dip in st->dip_list[i]
			 */
			*(tmp->offp) = -1;	/* set to no match */
			dtmp = st->dip_list[i];

			while (dtmp) {
				dcmn_err2((CE_CONT,
				"di_matchlist: dip_list[%d]: dip = %p %s%d "
				"off = %x\n",
				i, (void *)dtmp->dip,
				(dtmp->dip->devi_node_name ?
				dtmp->dip->devi_node_name : ""),
				dtmp->dip->devi_instance,
				*(dtmp->offp)));

				/*
				 * XXX--Can optimize further by removing
				 * the matching dip node.
				 */
				if (tmp->dip == dtmp->dip) {
					dcmn_err2((CE_CONT,
					"di_matchlist: match!\n\n"));
					*(tmp->offp) = *(dtmp->offp);
					break;
				}
				dtmp = dtmp->next;
			}

			tmp = tmp->next;
		}
	}
}

/*
 * Get a chunk of memory >= size, for the snapshot
 */
static void
di_getmem(struct di_state *st, size_t size)
{
	struct di_cache *cache = kmem_zalloc(sizeof (struct di_cache),
	    KM_SLEEP);
	/*
	 * Round up size to nearest power of 2. If it is less
	 * than st->cache_size, set it to st->cache_size (i.e.,
	 * the cache_size is doubled every time) to reduce the
	 * number of memory allocations.
	 */
	size_t tmp = 1;
	while (tmp < size) {
		tmp <<= 1;
	}
	size = (tmp > st->cache_size) ? tmp : st->cache_size;

	cache->buf = ddi_umem_alloc(size, DDI_UMEM_SLEEP, &cache->cook);
	cache->buf_size = size;

	dcmn_err2((CE_CONT, "di_getmem: cache_size=%x\n", st->cache_size));

	if (st->cache_size == 0) {	/* first chunk */
		st->cache = cache;
	} else {
		/*
		 * locate end of linked list and add a chunk at the end
		 */
		struct di_cache *dcp = st->cache;
		while (dcp->next != NULL) {
			dcp = dcp->next;
		}

		dcp->next = cache;
	}

	st->cache_size += size;
}

/*
 * Free all memory for the snapshot
 */
static void
di_freemem(struct di_state *st)
{
	struct di_cache *dcp, *tmp;

	dcmn_err2((CE_CONT, "di_freemem\n"));

	if (st->cache_size) {
		dcp = st->cache;
		while (dcp) {	/* traverse the linked list */
			tmp = dcp;
			dcp = dcp->next;
			ddi_umem_free(tmp->cook);
			kmem_free(tmp, sizeof (struct di_cache));
		}
		st->cache_size = 0;
	}
}

/*
 * Make sure there is at least "size" bytes memory left before
 * going on. Otherwise, start on a new chunk.
 */
static di_off_t
di_checkmem(struct di_state *st, di_off_t off, size_t size)
{
	dcmn_err3((CE_CONT, "di_checkmem: off=%x size=%x\n",
			off, (int)size));

	off = DI_ALIGN(off);
	if ((st->cache_size - off) < size) {
		off = st->cache_size;
		di_getmem(st, size);
	}

	return (off);
}

/*
 * Copy the private data format from ioctl arg.
 * On success, the ending offset is returned. On error 0 is returned.
 */
static di_off_t
di_copyformat(di_off_t off, struct di_state *st, intptr_t arg, int mode)
{
	di_off_t size;
	struct di_priv_data *priv;
	struct di_all *all = (struct di_all *)di_cache_addr(st, 0);

	dcmn_err2((CE_CONT, "di_copyformat: off=%x, arg=%p mode=%x\n",
		off, (void *)arg, mode));

	/*
	 * Copyin data and check version.
	 * We only handle private data version 0.
	 */
	priv = kmem_alloc(sizeof (struct di_priv_data), KM_SLEEP);
	if ((ddi_copyin((void *)arg, priv, sizeof (struct di_priv_data),
	    mode) != 0) || (priv->version != DI_PRIVDATA_VERSION_0)) {
		kmem_free(priv, sizeof (struct di_priv_data));
		return (0);
	}

	/*
	 * Save di_priv_data copied from userland in snapshot.
	 */
	all->pd_version = priv->version;
	all->n_ppdata = priv->n_parent;
	all->n_dpdata = priv->n_driver;

	/*
	 * copyin private data format, modify offset accordingly
	 */
	if (all->n_ppdata) {	/* parent private data format */
		/*
		 * check memory
		 */
		size = all->n_ppdata * sizeof (struct di_priv_format);
		off = di_checkmem(st, off, size);
		all->ppdata_format = off;
		if (ddi_copyin(priv->parent, di_cache_addr(st, off), size,
		    mode) != 0) {
			kmem_free(priv, sizeof (struct di_priv_data));
			return (0);
		}

		off += size;
	}

	if (all->n_dpdata) {	/* driver private data format */
		/*
		 * check memory
		 */
		size = all->n_ppdata * sizeof (struct di_priv_format);
		off = di_checkmem(st, off, size);
		all->dpdata_format = off;
		if (ddi_copyin(priv->driver, (caddr_t)all + off, size,
		    mode) != 0) {
			kmem_free(priv, sizeof (struct di_priv_data));
			return (0);
		}

		off += size;
	}

	kmem_free(priv, sizeof (struct di_priv_data));
	return (off);
}

/*
 * Return the real address based on the offset (off) within snapshot
 */
static caddr_t
di_cache_addr(struct di_state *st, di_off_t off)
{
	struct di_cache *dcp = st->cache;

	dcmn_err3((CE_CONT, "di_cache_addr: dcp=%p off=%x\n",
		(void *)dcp, off));

	while (off >= dcp->buf_size) {
		off -= dcp->buf_size;
		dcp = dcp->next;
	}

	dcmn_err3((CE_CONT, "di_cache_addr: new off=%x, return = %p\n",
		off, (void *)(dcp->buf + off)));

	return (dcp->buf + off);
}

/*
 * This is the main function that takes a snapshot of the devinfo tree
 */
static di_off_t
di_snapshot(struct di_state *st)
{
	di_off_t off;
	struct di_all *all;
	dev_info_t *rootnode;

	all = (struct di_all *)di_cache_addr(st, 0);
	dcmn_err((CE_CONT, "Taking a snapshot of devinfo tree...\n"));

	/*
	 * Hold drivers during the snapshot to prevent driver unloading.
	 * We don't care about unloaded drivers unless DINFOFORCE is specified.
	 *
	 * NOTE Drivers could be loaded during the snapshot. We will tryhold
	 *	additional drivers. If that fails, we simply ignore the driver.
	 */
	rootnode = di_hold_drivers(st, all->root_path,
	    st->command & DINFOFORCE);

	if (rootnode == NULL) {
		dcmn_err((CE_CONT, "Devinfo node %s not found\n",
		    all->root_path));
		di_rele_drivers(st);
		return (0);
	}

	/*
	 * initialize the per-driver node list
	 */
	di_initlist(st);

	/*
	 * Make sure the topology of devinfo tree does not change
	 */
	rw_enter(&(devinfo_tree_lock), RW_READER);

	/*
	 * double check whether the path still exists now we
	 * holding the lock
	 */
	rootnode = di_path_to_devinfo(st, all->root_path, 0);
	if (rootnode == NULL) {
		rw_exit(&(devinfo_tree_lock));

		dcmn_err((CE_CONT, "Devinfo node %s not found\n",
		    all->root_path));
		di_rele_drivers(st);
		di_freelist(st);
		return (0);
	}

	/*
	 * copy the device tree
	 */
	all->devnames = di_copytree(DEVI(rootnode), &all->top_devinfo, st);

	rw_exit(&(devinfo_tree_lock));

	/*
	 * copy the devnames array
	 */
	off = di_copydevnm(&all->devnames, st);

	/*
	 * Drivers can be unloaded now
	 */
	di_rele_drivers(st);

	/*
	 * fix up the per-driver node list and free unmatched nodes
	 */
	di_matchlist(st);
	di_freelist(st);

	return (off);
}

/*
 * Copy the devnames array, so we have a list of drivers in the snapshot.
 * Also makes it possible to locate the per-driver devinfo nodes.
 */
static di_off_t
di_copydevnm(di_off_t *off_p, struct di_state *st)
{
	int i;
	di_off_t off;
	size_t size;
	struct di_devnm *dnp;

	dcmn_err2((CE_CONT, "di_copydevnm: *off_p = %p\n", (void *)off_p));

	/*
	 * make sure there is some allocated memory
	 */
	size = devcnt * sizeof (struct di_devnm);
	off = di_checkmem(st, *off_p, size);
	*off_p = off;

	dcmn_err((CE_CONT, "Start copying devnamesp[%d] at offset 0x%x\n",
		devcnt, off));

	dnp = (struct di_devnm *)di_cache_addr(st, off);
	off += size;

	for (i = 0; i < devcnt; i++) {

		if (devnamesp[i].dn_name == NULL) {
			continue;
		}

		/*
		 * dn_name is not freed during driver unload or removal.
		 *
		 * There is a race condition when make_devname() changes
		 * dn_name during our strcpy. This should be rare since
		 * only add_drv does this. At any rate, we never had a
		 * problem with ddi_name_to_major(), which should have
		 * the same problem.
		 */
		dcmn_err2((CE_CONT, "di_copydevnm: %s%d, off=%x\n",
			devnamesp[i].dn_name, devnamesp[i].dn_instance,
			off));

		off = di_checkmem(st, off, strlen(devnamesp[i].dn_name) + 1);
		dnp[i].name = off;
		(void) strcpy((char *)di_cache_addr(st, off),
			devnamesp[i].dn_name);
		off += DI_ALIGN(strlen(devnamesp[i].dn_name) + 1);

		/*
		 * Ignore drivers not held
		 */
		if (!st->drivers_held[i]) {
			continue;
		}

		/*
		 * This is not used by libdevinfo, leave it for now
		 */
		dnp[i].flags = devnamesp[i].dn_flags;
		dnp[i].instance = devnamesp[i].dn_instance;

		/*
		 * get global properties
		 */
		if (devnamesp[i].dn_global_prop_ptr) {
			dnp[i].global_prop = off;
			off = di_getprop(devnamesp[i].dn_global_prop_ptr,
				&dnp[i].global_prop, st, NULL, (major_t)-1,
				DI_PROP_GLB_LIST);
		}

		/*
		 * Bit encode driver ops: & bus_ops, cb_ops, & cb_ops->cb_str
		 */
		if (CB_DRV_INSTALLED(devopsp[i])) {
			if (devopsp[i]->devo_cb_ops) {
				dnp[i].ops |= DI_CB_OPS;
				if (devopsp[i]->devo_cb_ops->cb_str)
					dnp[i].ops |= DI_STREAM_OPS;
			}
			if (NEXUS_DRV(devopsp[i])) {
				dnp[i].ops |= DI_BUS_OPS;
			}
		}

		/*
		 * Mark head of per-driver node list
		 */
		if (devnamesp[i].dn_head) {
			di_insertlist(&st->nxt_list[i], &dnp[i].head,
				(struct dev_info *)devnamesp[i].dn_head);
		}
	}

	dcmn_err((CE_CONT, "End copying devnamesp at offset 0x%x\n", off));

	return (off);
}

/*
 * Give a path component, decompose into the node name, address, and minorname
 */
static void
di_parse_name(char *name, char **drvname, char **addrname, char **minorname)
{
	char *cp, ch;
	static char nulladdrname[] = ":\0";

	dcmn_err2((CE_CONT, "di_parse_name: name = %s\n", name));

	cp = *drvname = name;
	*addrname = *minorname = NULL;
	while ((ch = *cp) != '\0') {
		if (ch == '@') {
			*addrname = ++cp;
		} else if (ch == ':') {
			*minorname = ++cp;
		}
		++cp;
	}
	if (!*addrname) {
		*addrname = &nulladdrname[1];
	}
	*((*addrname)-1) = '\0';
	if (*minorname) {
		*((*minorname)-1) = '\0';
	}
}

/*
 * di_find_child() must be called with devinfo_tree_lock held
 */
static dev_info_t *
di_find_child(dev_info_t *p, char *cname, char *caddr)
{
	char *naddr;
	dev_info_t *child = ddi_get_child(p);

	dcmn_err2((CE_CONT, "di_find_child: %s@%s\n", cname, caddr));

	while (child) {
		/*
		 * When path doesn't contain addr portion, the address
		 * can be "\0" (e.g. /pseudo) or the address pointer is
		 * NULL (e.g. no driver case /aliases, /chosen).
		 */
		if (strcmp(cname, ddi_node_name(child)) == 0 &&
		    ((((naddr = ddi_get_name_addr(child)) != NULL) &&
		    (strcmp(caddr, naddr) == 0)) ||
		    ((naddr == NULL) && (strlen(caddr) == 0)))) {
			break;
		}
		/*
		 * child is not a match, try its sibling
		 */
		child = ddi_get_next_sibling(child);
	}

	return (child);
}

/*
 * Adapted from ddi_pathname_to_dev_t(), Return NULL on failure.
 *
 * NOTE If hold_drivers = 0, call with devinfo_tree_lock held;
 *	else, call with devinfo_tree_lock not held (to avoid deadlock).
 */
static dev_info_t *
di_path_to_devinfo(struct di_state *st, char *pathname, int hold_drivers)
{
	struct pathname pn;
	dev_info_t *child;
	char component[MAXNAMELEN];
	char *cname, *caddr, *minorname;
	char *path_to_parent;

	dcmn_err((CE_CONT, "di_path_to_devinfo: pathname = %s\n",
		pathname));

	if (pn_get(pathname, UIO_SYSSPACE, &pn))
		return (NULL);

	pn_skipslash(&pn);
	child = ddi_root_node();
	if (hold_drivers) {
		path_to_parent = kmem_zalloc(MAXPATHLEN, KM_SLEEP);
	}

	while (pn_pathleft(&pn) && child) {
		int loaded;
		major_t major;
		char *drvname = NULL;
		dev_info_t *parent;

		(void) pn_getcomponent(&pn, component);

		if (!hold_drivers) {
			/*
			 * The easy case: the devinfo_tree_lock is held,
			 * can search the tree without worrying about node
			 * disappearing.
			 */
			ASSERT(RW_READ_HELD(&devinfo_tree_lock));

			parent = child;
			di_parse_name(component, &cname, &caddr, &minorname);
			child = di_find_child(parent, cname, caddr);
			pn_skipslash(&pn);
			continue;
		}

		/*
		 * Need to hold hold per-driver lock as we go down the path.
		 * The imposed locking order is per-driver lock first, then
		 * the devinfo_tree_lock. A node could disappear if the
		 * the devinfo_tree_lock is not held (regardless of the
		 * state of the per-driver lock).
		 *
		 * Everytime we hold devinfo_tree_lock, must search for
		 * parent node again even though we found it in the last
		 * iteration. We do the search by calling
		 * di_path_to_devinfo() itself with hold_drivers set to 0.
		 */
		rw_enter(&(devinfo_tree_lock), RW_READER);
		/*
		 * Find parent node based on path_to_parent and
		 * update path_to_parent
		 */
		(void) strcat(path_to_parent, "/");
		parent = di_path_to_devinfo(st, path_to_parent, 0);
		if (parent == NULL) {
			child = NULL;
			rw_exit(&(devinfo_tree_lock));
			break;
		}
		(void) strcat(path_to_parent, component);

		/*
		 * Given my parent, my nodename and my unit address,
		 * find the child node and its binding name.
		 */
		di_parse_name(component, &cname, &caddr, &minorname);
		child = di_find_child(parent, cname, caddr);
		if (child == NULL) {
			rw_exit(&(devinfo_tree_lock));
			break;
		}
		/*
		 * Set drvname name to the binding name of child if
		 * possible. Otherwise, drvname defaults to cname.
		 */
		if ((drvname = ddi_binding_name(child)) == NULL) {
			drvname = cname;
		}
		rw_exit(&(devinfo_tree_lock));

		/*
		 * Now, hold the driver of the child node.
		 *
		 * If there is no driver, continue, to allow snapshot of
		 * nodes with no drivers (e.g. /aliases /chosen).
		 *
		 * If there is a driver, but we cannot hold it then bail out.
		 * Make sure we don't hold a driver multiple times
		 * (e.g. USB hid nodes may appear multiple times along
		 * a single devinfo tree branch).
		 */
		if ((major = ddi_name_to_major(drvname)) == (major_t)-1) {
			pn_skipslash(&pn);
			continue;
		}

		dcmn_err2((CE_CONT, "di_path_to_devinfo: "
			"parent=%p drvname =%s unit_address =%s\n",
			(void *)parent, drvname, caddr));


		if (st->drivers_held[(int)major] == 1) {
			/*
			 * we already hold this driver, go on to next path
			 * component
			 */
			pn_skipslash(&pn);
			continue;
		}

		loaded = CB_DRV_INSTALLED(devopsp[(int)major]);
		if (ddi_hold_installed_driver(major) == NULL) {
			child = NULL;
			break;
		}

		st->drivers_held[(int)major] = 1;
		if (loaded) {
			(void) e_ddi_deferred_attach(major, NODEV);
		}

		pn_skipslash(&pn);
	}

	if (hold_drivers) {
		kmem_free(path_to_parent, MAXPATHLEN);
	}
	pn_free(&pn);
	return (child);
}

/*
 * Return devinfo node corresponding path, or NULL on failure
 */
static dev_info_t *
di_hold_drivers(struct di_state *st, char *path, int force_load)
{
	int i;
	struct devnames *dnp;
	struct dev_ops *ops;
	dev_info_t *node;

	ASSERT(st != NULL);

	dcmn_err2((CE_CONT, "di_hold_drivers: %s, force %x\n",
			path, force_load));

	st->drivers_held = kmem_zalloc(devcnt * sizeof (int), KM_SLEEP);

	if ((node = di_path_to_devinfo(st, path, 1)) == NULL)
		return (NULL);

	/*
	 * We try to load all drivers in memory if force_load is specified
	 * and path is "/". Otherwise, we only hold drivers already in memory.
	 * XXX---This is an agreement with the devfsadm project.
	 */
	if (strcmp(path, "/"))
		force_load = 0;

	for (i = 0; i < devcnt; i++) {
		if (st->drivers_held[i]) {
			continue;
		}

		if (force_load) {
			int loaded = CB_DRV_INSTALLED(devopsp[i]);
			if (ddi_hold_installed_driver(i) != NULL) {
				st->drivers_held[i] = 1;
				if (loaded) {
					(void) e_ddi_deferred_attach
					    ((major_t)i, NODEV);
				}
			} else if (devnamesp[i].dn_name) {
				dcmn_err((CE_CONT,
					"Failed to hold driver \"%s\"\n",
					devnamesp[i].dn_name));
			}
			continue;
		}

		/*
		 * If force loading drivers is not required, we only
		 * hold drivers that are already in memory.
		 *
		 * More or less copied from ddi_hold_installed_driver(),
		 * but with the goto eliminated.
		 */
		dnp = &(devnamesp[i]);
		LOCK_DEV_OPS(&dnp->dn_lock);
		/*
		 * Wait on busy flag
		 */
		while (DN_BUSY_CHANGING(dnp->dn_flags))
			cv_wait(&(dnp->dn_wait), &(dnp->dn_lock));
		/*
		 * Driver may be available now
		 */
		if (dnp->dn_flags & DN_WALKED_TREE) {
			ops = devopsp[i];
			INCR_DEV_OPS_REF(ops);
			/*
			 * Mark driver as held
			 */
			st->drivers_held[i] = 1;
		}
		UNLOCK_DEV_OPS(&dnp->dn_lock);
	}

	return (node);
}

/*
 * Called from di_copytree with devinfo_tree_lock held
 */
static int
di_tryhold_driver(struct di_state *st, major_t major)
{
	struct devnames *dnp;
	struct dev_ops *ops;

	ASSERT(major != (major_t)-1);
	ASSERT(st != NULL);
	ASSERT(st->drivers_held != NULL);
	ASSERT(st->drivers_held[(int)major] == 0);

	dcmn_err2((CE_CONT, "di_tryhold_driver: major = %x\n", major));

	/*
	 * More or less copied from ddi_hold_installed_driver().
	 * Since we already hold devinfo_tree_lock, we can not
	 * sleep on any mutex or conditional variable.
	 *
	 * XXX To retry, one must start over at di_snapshot level
	 */
	dnp = &(devnamesp[(int)major]);
	if (mutex_tryenter(&dnp->dn_lock)) {
		if ((dnp->dn_flags & DN_WALKED_TREE) &&
		    !DN_BUSY_CHANGING(dnp->dn_flags)) {
			ops = devopsp[(int)major];
			INCR_DEV_OPS_REF(ops);
			/*
			 * Mark drivers held
			 */
			st->drivers_held[(int)major] = 1;
		}
		mutex_exit(&dnp->dn_lock);
	}

	if (st->drivers_held[(int)major] == 0)
		dcmn_err((CE_CONT, "Fail to tryhold driver major %d",
		    (int)major));

	return (st->drivers_held[(int)major]);
}


/*
 * Release drivers held during the snapshot.
 */
static void
di_rele_drivers(struct di_state *st)
{
	int i;

	ASSERT(st != NULL);
	ASSERT(st->drivers_held != NULL);

	dcmn_err2((CE_CONT, "di_rele_drivers:\n"));

	for (i = 0; i < devcnt; i++) {
		/*
		 * look at devinfo state to see which drivers were held.
		 * We don't want to release a driver we don't hold.
		 */
		if (st->drivers_held[i])
			ddi_rele_driver(i);
	}

	kmem_free(st->drivers_held, devcnt * sizeof (int));
	st->drivers_held = NULL;
}

/*
 * Copy the kernel devinfo tree. The tree and the devnames array forms
 * the entire snapshot (see also di_copydevnm).
 */
static di_off_t
di_copytree(struct dev_info *root, di_off_t *off_p, struct di_state *st)
{
	di_off_t off;
	struct di_stack *dsp = kmem_zalloc(sizeof (struct di_stack), KM_SLEEP);

	dcmn_err((CE_CONT, "di_copytree: root = %p, *off_p = %x\n",
		(void *)root, *off_p));

	/*
	 * Push top_devinfo onto a stack
	 *
	 * The stack is necessary to avoid recursion, which can overrun
	 * the kernel stack.
	 */
	PUSH_STACK(dsp, root, off_p);

	/*
	 * As long as there is a node on the stack, copy the node.
	 * di_copynode() is responsible for pushing and popping
	 * child and silbing nodes on the stack.
	 */
	while (!EMPTY_STACK(dsp)) {
		off = di_copynode(dsp, st);
	}

	/*
	 * Free the stack structure
	 */
	kmem_free(dsp, sizeof (struct di_stack));

	return (off);
}

/*
 * This is the core function, which copies all data associated with a single
 * node into the snapshot. The amount of information is determined by the
 * ioctl command.
 */
static di_off_t
di_copynode(struct di_stack *dsp, struct di_state *st)
{
	di_off_t off;
	struct di_node *me;
	struct dev_info *node;

	dcmn_err2((CE_CONT, "di_copynode: depth = %x\n",
			dsp->depth));

	node = TOP_NODE(dsp);

	ASSERT(node != NULL);

	/*
	 * check memory usage, and fix offsets accordingly.
	 */
	off = di_checkmem(st, *(TOP_OFFSET(dsp)), sizeof (struct di_node));
	*(TOP_OFFSET(dsp)) = off;
	me = DINO(di_cache_addr(st, off));

	dcmn_err((CE_CONT, "copy node %s, instance #%d, at offset 0x%x\n",
			node->devi_node_name, node->devi_instance, off));

	/*
	 * Node parameters:
	 * self		-- offset of current node within snapshot
	 * nodeid	-- pointer to PROM node (tri-valued)
	 * state	-- hot plugging device state
	 * node_state	-- devinfo node state (CF1, CF2, etc.)
	 */
	me->self = off;
	me->instance = node->devi_instance;
	me->nodeid = node->devi_nodeid;
	me->node_class = node->devi_node_class;
	me->attributes = node->devi_node_attributes;
	me->state = node->devi_state;
	if (DDI_CF2(node)) {
		me->node_state = 1;
	}

	/*
	 * Get parent's offset in snapshot from the stack
	 * and store it in the current node
	 */
	if (dsp->depth > 1) {
		me->parent = *(PARENT_OFFSET(dsp));
	}

	/*
	 * increment offset
	 */
	off += sizeof (struct di_node);

	if (node->devi_devid) {
		off = di_checkmem(st, off, ddi_devid_sizeof(node->devi_devid));
		me->devid = off;
		bcopy(node->devi_devid, di_cache_addr(st, off),
				ddi_devid_sizeof(node->devi_devid));
		off += ddi_devid_sizeof(node->devi_devid);
	}

	if (node->devi_node_name) {
		off = di_checkmem(st, off, strlen(node->devi_node_name) + 1);
		me->node_name = off;
		(void) strcpy(di_cache_addr(st, off), node->devi_node_name);
		off += strlen(node->devi_node_name) + 1;
	}

	if (node->devi_compat_names && (node->devi_compat_length > 1)) {
		off = di_checkmem(st, off, node->devi_compat_length);
		me->compat_names = off;
		me->compat_length = node->devi_compat_length;
		bcopy(node->devi_compat_names, di_cache_addr(st, off),
			node->devi_compat_length);
		off += node->devi_compat_length;
	}

	if (node->devi_addr) {
		off = di_checkmem(st, off, strlen(node->devi_addr) + 1);
		me->address = off;
		(void) strcpy(di_cache_addr(st, off), node->devi_addr);
		off += strlen(node->devi_addr) + 1;
	}

	/*
	 * When devi_binding_name exists, the node is bound to a driver.
	 * We attempt to get driver's major number.
	 */
	if (node->devi_binding_name) {
		off = di_checkmem(st, off, strlen(node->devi_binding_name) + 1);
		me->bind_name = off;
		(void) strcpy(di_cache_addr(st, off), node->devi_binding_name);
		off += strlen(node->devi_binding_name) + 1;
		me->drv_major = ddi_name_to_major(node->devi_binding_name);
	} else {
		me->drv_major = -1;
	}

	/*
	 * Note the position of current node in snapshot and devi_next.
	 * devi_next will be resolved in di_matchlist().
	 */
	if (me->drv_major != -1) {
		di_insertlist(&st->dip_list[me->drv_major], &me->self, node);
		if (node->devi_next)
			di_insertlist(&st->nxt_list[me->drv_major], &me->next,
				node->devi_next);
	}

	/*
	 * An optimization to skip mutex_enter when not needed.
	 */
	if (!((DINFOMINOR | DINFOPROP) & st->command)) {
		goto priv_data;
	}

	/*
	 * Grab current per dev_info node lock to
	 * get minor data and properties.
	 */
	mutex_enter(&(node->devi_lock));

	if (!(DINFOMINOR & st->command)) {
		goto property;
	}

	if (node->devi_minor) {		/* minor data */
		me->minor_data = DI_ALIGN(off);
		off = di_getmdata(node->devi_minor, &me->minor_data, st);
	}

property:
	if (!(DINFOPROP & st->command)) {
		goto unlock;
	}

	if (node->devi_drv_prop_ptr) {	/* driver property list */
		me->drv_prop = DI_ALIGN(off);
		off = di_getprop(node->devi_drv_prop_ptr, &me->drv_prop, st,
			node, (major_t)me->drv_major, DI_PROP_DRV_LIST);
	}

	if (node->devi_sys_prop_ptr) {	/* system property list */
		me->sys_prop = DI_ALIGN(off);
		off = di_getprop(node->devi_sys_prop_ptr, &me->sys_prop, st,
			node, (major_t)-1, DI_PROP_SYS_LIST);
	}

	if (node->devi_hw_prop_ptr) {	/* hardware property list */
		me->hw_prop = DI_ALIGN(off);
		off = di_getprop(node->devi_hw_prop_ptr, &me->hw_prop, st,
			node, (major_t)-1, DI_PROP_HW_LIST);
	}

unlock:
	/*
	 * release current per dev_info node lock
	 */
	mutex_exit(&(node->devi_lock));

	if (node->devi_minor) {		/* minor data */
		di_fixmdata(me->minor_data, st);
	}

priv_data:
	if (!(DINFOPRIVDATA & st->command)) {
		goto pm_info;
	}

	if (node->devi_parent_data) {	/* parent private data */
		me->parent_data = DI_ALIGN(off);
		off = di_getppdata(node, &me->parent_data, st);
	}

	if (node->devi_driver_data) {	/* driver private data ??? */
		me->driver_data = DI_ALIGN(off);
		off = di_getdpdata(node, &me->driver_data, st);
	}

pm_info: /* NOT implemented */

subtree:
	if (!(DINFOSUBTREE & st->command)) {
		dsp->depth = 0;
		return (DI_ALIGN(off));
	}

child:
	/*
	 * if there is a child--push child onto stack
	 */
	if (node->devi_child) {
		me->child = DI_ALIGN(off);
		PUSH_STACK(dsp, node->devi_child, &me->child);
		return (me->child);
	}

sibling:
	/*
	 * no child node, unroll the stack till a sibling of
	 * a parent node is found or root node is reached
	 */
	POP_STACK(dsp);
	while (!EMPTY_STACK(dsp) && (node->devi_sibling == NULL)) {
		node = TOP_NODE(dsp);
		me = DINO(di_cache_addr(st, *(TOP_OFFSET(dsp))));
		POP_STACK(dsp);
	}

	if (!EMPTY_STACK(dsp)) {
		/*
		 * a sibling is found, replace top of stack by its sibling
		 */
		me->sibling = DI_ALIGN(off);
		PUSH_STACK(dsp, node->devi_sibling, &me->sibling);
		return (me->sibling);
	}

	/*
	 * DONE with all nodes
	 */
	return (DI_ALIGN(off));
}

static void
di_fixmdata(di_off_t off, struct di_state *st)
{
	struct di_minor *me;

	if ((cluster_bootflags & CLUSTER_BOOTED) == 0)
		return;

	while (off) {
		me = (struct di_minor *)di_cache_addr(st, off);

		if (DC_MAP_MINOR(&dcops, me->dev_major, me->dev_minor,
		    &me->dev_minor, me->mdclass) != 0)
			cmn_err(CE_WARN, "devinfo: "
			    "could not map minor: %d,%d class: %d",
			    me->dev_major, me->dev_minor, me->mdclass);

		off = me->next;
	}
}

/*
 * Copy all minor data nodes attached to a devinfo node into the snapshot.
 * It is called from di_copynode with devi_lock held.
 */
static di_off_t
di_getmdata(struct ddi_minor_data *mnode, di_off_t *off_p,
	struct di_state *st)
{
	di_off_t off;
	struct di_minor *me;

	dcmn_err2((CE_CONT, "di_getmdata:\n"));

	/*
	 * check memory first
	 */
	off = di_checkmem(st, *off_p, sizeof (struct di_minor));
	*off_p = off;

	do {
		me = (struct di_minor *)di_cache_addr(st, off);
		me->self = off;
		me->type = mnode->type;

		off += sizeof (struct di_minor);

		if (me->type != DDM_ALIAS) {
			/*
			 * Split dev_t to major/minor, so it works for
			 * both ILP32 and LP64 model
			 */
			me->dev_major = getmajor(mnode->ddm_dev);
			me->dev_minor = getminor(mnode->ddm_dev);
			me->spec_type = mnode->ddm_spec_type;
			me->mdclass = mnode->ddm_class;

			if (mnode->ddm_name) {
				off = di_checkmem(st, off,
					strlen(mnode->ddm_name) + 1);
				me->name = off;
				(void) strcpy(di_cache_addr(st, off),
					mnode->ddm_name);
				off += strlen(mnode->ddm_name) + 1;
			}

			if (mnode->ddm_node_type) {
				off = di_checkmem(st, off,
					strlen(mnode->ddm_node_type) + 1);
				me->node_type = off;
				(void) strcpy(di_cache_addr(st, off),
						mnode->ddm_node_type);
				off += strlen(mnode->ddm_node_type) + 1;
			}

		} else {		/* alias node */
			/*
			 * For alias minor node, copy info from target mnode.
			 * No need to grab ddm_adip->devi_lock (see note).
			 *
			 * NOTE: During minor node creation, the alias minor
			 *	under "clone" is created AFTER the real node
			 *	it points to (see ddi_create_minor_common()).
			 *	During node removal, the alias minor is
			 *	removed BEFORE the real minor node is.
			 *
			 *	This means if we see an alias minor, the real
			 *	minor it points to must exist.
			 */
			me->dev_major = getmajor(mnode->ddm_adev);
			me->dev_minor = getminor(mnode->ddm_adev);
			me->spec_type = mnode->ddm_aspec_type;
			me->mdclass = mnode->ddm_admp->ddm_class;

			if (mnode->ddm_aname) {
				off = di_checkmem(st, off,
					strlen(mnode->ddm_aname) + 1);
				me->name = off;
				(void) strcpy(di_cache_addr(st, off),
					mnode->ddm_aname);
				off += strlen(mnode->ddm_aname) + 1;
			}

			if (mnode->ddm_anode_type) {
				off = di_checkmem(st, off,
					strlen(mnode->ddm_anode_type) + 1);
				me->node_type = off;
				(void) strcpy(di_cache_addr(st, off),
						mnode->ddm_anode_type);
				off += strlen(mnode->ddm_anode_type) + 1;
			}
		}

		/*
		 * md can have lots of minor nodes, so need to
		 * check memory inside this loop.
		 */
		off = di_checkmem(st, off, sizeof (struct di_minor));
		me->next = off;
		mnode = mnode->next;
	} while (mnode);

	me->next = 0;

	return (off);
}

/*
 * Copy a list of properties attached to a devinfo node. Called from
 * di_copynode with devi_lock held. The major number is passed in case
 * we need to call driver's prop_op entry. The value of list indicates
 * which list we are copying. Possible values are:
 * DI_PROP_DRV_LIST, DI_PROP_SYS_LIST, DI_PROP_GLB_LIST, DI_PROP_HW_LIST
 */
static di_off_t
di_getprop(struct ddi_prop *prop, di_off_t *off_p, struct di_state *st,
	struct dev_info *dip, major_t major, int list)
{
	int off, need_prop_op = 0;
	dev_info_t *pdip;
	struct di_prop *pp;
	struct dev_ops *ops = NULL, *pops = NULL;

	dcmn_err2((CE_CONT, "di_getprop:\n"));

	ASSERT(st != NULL);

	dcmn_err((CE_CONT, "copy property list at addr %p\n", (void *)prop));

	/*
	 * Figure out if we need to call driver's prop_op entry point.
	 * The conditions are:
	 *	-- driver property list
	 *	-- driver must be attached and held
	 *	-- driver's cb_prop_op != ddi_prop_op
	 *		or parent's bus_prop_op != ddi_bus_prop_op
	 */

	if (list != DI_PROP_DRV_LIST) {
		goto getprop;
	}

	/*
	 * If driver is not attached or if major is -1, we ignore
	 * the driver property list. No one should rely on such
	 * properties.
	 */
	if (!DDI_CF2(dip) || (major == (major_t)-1)) {
		off = *off_p;
		*off_p = 0;
		return (off);
	}

	/*
	 * Check if we hold this driver.
	 *
	 * If not held, which means driver was loaded after we called
	 * di_hold_drivers()), we try to hold it now. If this fails,
	 * we forget about calling the prop_op entry point.
	 *
	 * XXX--The alternative is to start di_snapshot() all over again.
	 */
	if ((!st->drivers_held[(int)major]) &&
	    (!di_tryhold_driver(st, major))) {
		goto getprop;
	}

	/*
	 * Now we have a driver which is held. We can examine entry points
	 * and check the condition listed above.
	 */
	ops = dip->devi_ops;

	if (pdip = ddi_get_parent((dev_info_t *)dip)) {
		pops = DEVI(pdip)->devi_ops;
	}

	if ((ops->devo_cb_ops == NULL) ||
	    (ops->devo_cb_ops->cb_prop_op == ddi_prop_op)) {
		goto getprop;
	}

	if ((pops == NULL) || !(NEXUS_DRV(pops)) ||
	    (pops->devo_bus_ops->bus_prop_op == ddi_bus_prop_op)) {
		goto getprop;
	}

	/*
	 * Condition is met, we need to call cb_prop_op()
	 */
	need_prop_op = 1;

getprop:
	/*
	 * check memory availability
	 */
	off = di_checkmem(st, *off_p, sizeof (struct di_prop));
	*off_p = off;
	/*
	 * Now copy properties
	 */
	do {
		pp = (struct di_prop *)di_cache_addr(st, off);
		pp->self = off;
		/*
		 * Split dev_t to major/minor, so it works for
		 * both ILP32 and LP64 model
		 */
		pp->dev_major = getmajor(prop->prop_dev);
		pp->dev_minor = getminor(prop->prop_dev);
		pp->prop_flags = prop->prop_flags;
		pp->prop_list = list;

		/*
		 * property name
		 */
		off += sizeof (struct di_prop);
		if (prop->prop_name) {
			off = di_checkmem(st, off, strlen(prop->prop_name) + 1);
			pp->prop_name = off;
			(void) strcpy(di_cache_addr(st, off), prop->prop_name);
			off += strlen(prop->prop_name) + 1;
		}

		/*
		 * Property value can take a lot of memory.
		 *
		 * Check available memory against prop_len
		 */
		off = di_checkmem(st, off, prop->prop_len);
		pp->prop_data = off;
		pp->prop_len = prop->prop_len;

		if (prop->prop_len == 0) {
			/*
			 * boolean property, don't bother
			 */
			pp->prop_data = 0;

		} else if (!need_prop_op) {
			if (prop->prop_val == NULL) {
				dcmn_err((CE_WARN,
				    "devinfo: property fault at %p\n",
				    (void *)prop));
				pp->prop_data = -1;
			} else {
				bcopy(prop->prop_val, di_cache_addr(st, off),
				    prop->prop_len);
			}
		}

		off += DI_ALIGN(pp->prop_len);
		off = di_checkmem(st, off, sizeof (struct di_prop));
		pp->next = off;
		prop = prop->prop_next;
	} while (prop);

	pp->next = 0;

	/*
	 * If there is a need to call driver's prop_op entry,
	 * we must release driver's devi_lock, because the
	 * cb_prop_op entry point will grab it.
	 *
	 * The snapshot memory has already been allocated above,
	 * which means the length of an active property should
	 * remain fixed for this implementation to work.
	 */
	if (need_prop_op) {
		dev_t dev;
		int (*prop_op)();

		prop_op = ops->devo_cb_ops->cb_prop_op;
		pp = (struct di_prop *)di_cache_addr(st, *off_p);

		mutex_exit(&dip->devi_lock);

		do {
			int err;
			struct di_prop *tmp;

			if (pp->next) {
				tmp = (struct di_prop *)
					di_cache_addr(st, pp->next);
			} else {
				tmp = NULL;
			}

			if (pp->prop_len == 0) {
				pp = tmp;
				continue;
			}

			/*
			 * call into driver's prop_op entry point
			 *
			 * Must search DDI_DEV_T_NONE with DDI_DEV_T_ANY
			 */
			dev = makedevice(pp->dev_major, pp->dev_minor);
			if (dev == DDI_DEV_T_NONE)
				dev = DDI_DEV_T_ANY;

			dcmn_err((CE_CONT, "call prop_op"
				"(%lx, %p, PROP_LEN_AND_VAL_BUF, "
				"DDI_PROP_DONTPASS, \"%s\", %p, &%d)\n",
				dev,
				(void *)dip,
				(char *)di_cache_addr(st, pp->prop_name),
				(void *)di_cache_addr(st, pp->prop_data),
				pp->prop_len));

			if ((err = (*prop_op)(dev, (dev_info_t)dip,
			    PROP_LEN_AND_VAL_BUF, DDI_PROP_DONTPASS,
			    (char *)di_cache_addr(st, pp->prop_name),
			    di_cache_addr(st, pp->prop_data),
			    &pp->prop_len)) != DDI_PROP_SUCCESS) {
				pp->prop_data = -1;
				dcmn_err((CE_WARN,
				"devinfo: prop_op failure for \"%s\" err %d\n",
				    di_cache_addr(st, pp->prop_name), err));
			}

			pp = tmp;

		} while (pp);

		mutex_enter(&dip->devi_lock);
	}

	dcmn_err((CE_CONT, "finished property list at offset 0x%x\n", off));

	return (off);
}

/*
 * find private data format attached to a dip
 * parent = 1 to match driver name of parent dip (for parent private data)
 *	0 to match driver name of current dip (for driver private data)
 */
#define	DI_MATCH_DRIVER	0
#define	DI_MATCH_PARENT	1

struct di_priv_format *
di_match_drv_name(struct dev_info *node, struct di_state *st, int match)
{
	int i, count, len;
	char *drv_name;
	major_t major;
	struct di_all *all;
	struct di_priv_format *form;

	dcmn_err2((CE_CONT, "di_match_drv_name: node = %s, match = %x\n",
		node->devi_node_name, match));

	if (match == DI_MATCH_PARENT) {
		node = DEVI(node->devi_parent);
	}

	if (node == NULL) {
		return (NULL);
	}

	major = ddi_name_to_major(node->devi_binding_name);
	if (major == (major_t)(-1)) {
		return (NULL);
	}

	/*
	 * Match the driver name.
	 */
	drv_name = ddi_major_to_name(major);
	if ((drv_name == NULL) || *drv_name == '\0') {
		return (NULL);
	}

	/* Now get the di_priv_format array */
	all = (struct di_all *)di_cache_addr(st, 0);

	if (match == DI_MATCH_PARENT) {
		count = all->n_ppdata;
		form = (struct di_priv_format *)
			(di_cache_addr(st, 0) + all->ppdata_format);
	} else {
		count = all->n_dpdata;
		form = (struct di_priv_format *)
			((caddr_t)all + all->dpdata_format);
	}

	len = strlen(drv_name);
	for (i = 0; i < count; i++) {
		char *tmp;

		tmp = form[i].drv_name;
		while (tmp && (*tmp != '\0')) {
			if (strncmp(drv_name, tmp, len) == 0) {
				return (&form[i]);
			}
			/*
			 * Move to next driver name, skipping a white space
			 */
			if (tmp = strchr(tmp, ' ')) {
				tmp++;
			}
		}
	}

	return (NULL);
}

/*
 * The following functions copy data as specified by the format passed in.
 * To prevent invalid format from panicing the system, we call on_fault().
 * A return value of 0 indicates an error. Otherwise, the total offset
 * is returned.
 */
#define	DI_MAX_PRIVDATA	(PAGESIZE >> 1)	/* max private data size */

static di_off_t
di_getprvdata(struct di_priv_format *pdp, void *data, di_off_t *off_p,
	struct di_state *st)
{
	caddr_t pa;
	void *ptr;
	int i, size, repeat;
	di_off_t off, off0, *tmp;

	label_t ljb;

	dcmn_err2((CE_CONT, "di_getprvdata:\n"));

	/*
	 * check memory availability. Private data size is
	 * limited to DI_MAX_PRIVDATA.
	 */
	off = di_checkmem(st, *off_p, DI_MAX_PRIVDATA);

	if ((pdp->bytes <= 0) || pdp->bytes > DI_MAX_PRIVDATA) {
		goto failure;
	}

	if (!on_fault(&ljb)) {
		/* copy the struct */
		bcopy(data, di_cache_addr(st, off), pdp->bytes);
		off0 = DI_ALIGN(pdp->bytes);

		/* dereferencing pointers */
		for (i = 0; i < MAX_PTR_IN_PRV; i++) {

			if (pdp->ptr[i].size == 0) {
				goto success;	/* no more ptrs */
			}

			/*
			 * first, get the pointer content
			 */
			if ((pdp->ptr[i].offset < 0) ||
				(pdp->ptr[i].offset >
				pdp->bytes - sizeof (char *)))
				goto failure;	/* wrong offset */

			pa = di_cache_addr(st, off + pdp->ptr[i].offset);
			tmp = (di_off_t *)pa;	/* to store off_t later */

			ptr = *((void **) pa);	/* get pointer value */
			if (ptr == NULL) {	/* if NULL pointer, go on */
				continue;
			}

			/*
			 * next, find the repeat count (array dimension)
			 */
			repeat = pdp->ptr[i].len_offset;

			/*
			 * Positive value indicates a fixed sized array.
			 * 0 or negative value indicates variable sized array.
			 *
			 * For variable sized array, the variable must be
			 * an int member of the structure, with an offset
			 * equal to the absolution value of struct member.
			 */
			if (repeat > pdp->bytes - sizeof (int)) {
				goto failure;	/* wrong offset */
			}

			if (repeat >= 0) {
				repeat = *((int *)((caddr_t)data + repeat));
			} else {
				repeat = -repeat;
			}

			/*
			 * next, get the size of the object to be copied
			 */
			size = pdp->ptr[i].size * repeat;

			/*
			 * Arbitrarily limit the total size of object to be
			 * copied (1 byte to 1/4 page).
			 */
			if ((size <= 0) || (size > (DI_MAX_PRIVDATA - off0))) {
				goto failure;	/* wrong size or too big */
			}

			/*
			 * Now copy the data
			 */
			*tmp = off0;
			bcopy(ptr, di_cache_addr(st, off + off0), size);
			off0 += DI_ALIGN(size);
		}
	} else {
		goto failure;
	}

success:
	/*
	 * success if reached here
	 */
	no_fault();
	*off_p = off;

	return (off + off0);
	/*NOTREACHED*/

failure:
	/*
	 * fault occurred
	 */
	cmn_err(CE_WARN, "devinfo: fault in private data at %p\n", data);
	*off_p = -1;	/* set private data to indicate error */

	return (off);
}

/*
 * get parent private data; on error, returns original offset
 */
static di_off_t
di_getppdata(struct dev_info *node, di_off_t *off_p, struct di_state *st)
{
	int off;
	struct di_priv_format *ppdp;

	dcmn_err2((CE_CONT, "di_getppdata:\n"));

	/* find the parent data format */
	if ((ppdp = di_match_drv_name(node, st, DI_MATCH_PARENT)) == NULL) {
		off = *off_p;
		*off_p = 0;	/* set parent data to none */
		return (off);
	}

	return (di_getprvdata(ppdp, node->devi_parent_data, off_p, st));
}

/*
 * get parent private data; returns original offset
 */
static di_off_t
di_getdpdata(struct dev_info *node, di_off_t *off_p, struct di_state *st)
{
	int off;
	struct di_priv_format *dpdp;

	dcmn_err2((CE_CONT, "di_getdpdata:"));

	/* find the parent data format */
	if ((dpdp = di_match_drv_name(node, st, DI_MATCH_DRIVER)) == NULL) {
		off = *off_p;
		*off_p = 0;	/* set driver data to none */
		return (off);
	}

	return (di_getprvdata(dpdp, node->devi_driver_data, off_p, st));
}
