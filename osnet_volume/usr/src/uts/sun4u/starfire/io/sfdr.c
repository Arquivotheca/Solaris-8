/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sfdr.c	1.70	99/10/08 SMI"

/*
 * PSM-DR layer of DR driver for Starfire platform.
 */

#include <sys/debug.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/dditypes.h>
#include <sys/devops.h>
#include <sys/modctl.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ndi_impldefs.h>
#include <sys/stat.h>
#include <sys/kmem.h>
#include <sys/vmem.h>
#include <sys/processor.h>
#include <sys/cpuvar.h>
#include <sys/cpupart.h>
#include <sys/mem_config.h>
#include <sys/ddi_impldefs.h>
#include <sys/systm.h>
#include <sys/machsystm.h>
#include <sys/autoconf.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/x_call.h>
#include <sys/promif.h>
#include <sys/membar.h>
#include <vm/seg_kmem.h>
#include <sys/starfire.h>	/* plat_max_... decls */

#include <sys/dr.h>
#include <sys/sfdr.h>

/*
 * This is necessary because the CPU support needs
 * to call cvc_assign_cpu0.
 */
#ifndef	lint
static char _depends_on[] = "drv/cvc";
#endif	/* lint */

extern struct memlist	*phys_install;
extern struct cpu	*SIGBCPU;

#define	MAX_PROP_LEN	33

#ifdef DEBUG
uint_t	dr_debug = 0;
#endif /* DEBUG */

char	*state_str[] = {
	"EMPTY", "OCCUPIED", "CONNECTED", "UNCONFIGURED",
	"PARTIAL", "CONFIGURED", "RELEASE", "UNREFERENCED",
	"FATAL"
};
char	*nt_str[] = {
	"UNKNOWN", "CPU", "MEM", "IO"
};

#define	SFDR_CMD_STR(c) \
	(((c) == DR_CMD_CONNECT)	? "CONNECT"	: \
	((c) == DR_CMD_DISCONNECT)	? "DISCONNECT"	: \
	((c) == DR_CMD_CONFIGURE)	? "CONFIGURE"	: \
	((c) == DR_CMD_UNCONFIGURE)	? "UNCONFIGURE"	: \
	((c) == DR_CMD_RELEASE)		? "RELEASE"	: \
	((c) == DR_CMD_CANCEL)		? "CANCEL"	: \
	((c) == DR_CMD_IOCTL)		? "IOCTL"	: \
	((c) == DR_CMD_STATUS)		? "STATUS"	: "unknown")

/*
 * XXX
 * The following table is ONLY intended for debug
 * purposes as an easy way to locate the board structs.
 */
sfdr_board_t	*sfdr_board_table[16];
#ifdef DEBUG
/*
 * Create per-device attachment points.
 */
static int	sfdr_dev_attachment_points = 1;
#else
static int	sfdr_dev_attachment_points = 0;
#endif /* DEBUG */

/*
 * Defines and structures for Starfire device tree naming and mapping
 * to PIM node types
 */

/* name properties for some Starfire device nodes */
#define	MEM_DEVNAME		"mem-unit"
#define	CPU_DEVNAME		"SUNW,UltraSPARC"
#define	IO_SBUS_DEVNAME		"sbus"
#define	IO_PCI_DEVNAME		"pci"

/* struct for device name to dr_nodetype_t mapping */
typedef	struct {
	char		*s_devname;
	dr_nodetype_t	s_nodetype;
} sfdr_devname_t;


/* global struct to map starfire device attributes - name:dr_nodetype_t */
static	sfdr_devname_t	sfdr_devattr[] = {
	{ MEM_DEVNAME,		DR_NT_MEM },
	{ CPU_DEVNAME,		DR_NT_CPU },
	{ IO_SBUS_DEVNAME,	DR_NT_IO },
	{ IO_PCI_DEVNAME,	DR_NT_IO },
	{ NULL,			DR_NT_UNKNOWN }	/* last item must be blank */
};
/* defines to access the attribute struct */
#define	SFDR_NT(i)		sfdr_devattr[i].s_nodetype
#define	SFDR_DEVNAME(i)		sfdr_devattr[i].s_devname

/*
 * State transition table.  States valid transitions for "board" state.
 * Recall that non-zero return value terminates operation, however
 * the herrno value is what really indicates an error , if any.
 */
static int
_cmd2index(int c)
{
	/*
	 * Translate DR CMD to index into sfdr_state_transition.
	 */
	switch (c) {
	case DR_CMD_CONNECT:		return (0);
	case DR_CMD_DISCONNECT:		return (1);
	case DR_CMD_CONFIGURE:		return (2);
	case DR_CMD_UNCONFIGURE:	return (3);
	case DR_CMD_RELEASE:		return (4);
	case DR_CMD_CANCEL:		return (5);
	default:			return (-1);
	}
}

#define	CMD2INDEX(c)	_cmd2index(c)

static struct sfdr_state_trans {
	int	x_cmd;
	struct {
		int	x_rv;		/* return value of pre_op */
		int	x_err;		/* errno, if any */
	} x_op[SFDR_NUM_STATES];
} sfdr_state_transition[] = {
	{ DR_CMD_CONNECT,
		{
			{ 0, 0 },	/* empty */
			{ 0, 0 },	/* occupied */
			{ -1, 0 },	/* connected */
			{ -1, 0 },	/* unconfigured */
			{ -1, 0 },	/* partial */
			{ -1, 0 },	/* configured */
			{ -1, EINVAL },	/* release */
			{ -1, EINVAL },	/* unreferenced */
			{ -1, EIO },	/* fatal */
		}
	},
	{ DR_CMD_DISCONNECT,
		{
			{ -1, 0 },	/* empty */
			{ 0, 0 },	/* occupied */
			{ 0, 0 },	/* connected */
			{ 0, 0 },	/* unconfigured */
			{ -1, EINVAL },	/* partial */
			{ -1, EINVAL },	/* configured */
			{ -1, EINVAL },	/* release */
			{ -1, EINVAL },	/* unreferenced */
			{ -1, EIO },	/* fatal */
		}
	},
	{ DR_CMD_CONFIGURE,
		{
			{ -1, EINVAL },	/* empty */
			{ -1, EINVAL },	/* occupied */
			{ 0, 0 },	/* connected */
			{ 0, 0 },	/* unconfigured */
			{ 0, 0 },	/* partial */
			{ -1, 0 },	/* configured */
			{ -1, EINVAL },	/* release */
			{ -1, EINVAL },	/* unreferenced */
			{ -1, EIO },	/* fatal */
		}
	},
	{ DR_CMD_UNCONFIGURE,
		{
			{ -1, 0 },	/* empty */
			{ -1, 0 },	/* occupied */
			{ -1, 0 },	/* connected */
			{ -1, 0 },	/* unconfigured */
			{ -1, EINVAL },	/* partial */
			{ -1, EINVAL },	/* configured */
			{ 0, 0 },	/* release */
			{ 0, 0 },	/* unreferenced */
			{ -1, EIO },	/* fatal */
		}
	},
	{ DR_CMD_RELEASE,
		{
			{ -1, 0 },	/* empty */
			{ -1, 0 },	/* occupied */
			{ -1, 0 },	/* connected */
			{ -1, 0 },	/* unconfigured */
			{ 0, 0 },	/* partial */
			{ 0, 0 },	/* configured */
			{ -1, 0 },	/* release */
			{ -1, 0 },	/* unreferenced */
			{ -1, EIO },	/* fatal */
		}
	},
	{ DR_CMD_CANCEL,
		{
			{ -1, 0 },	/* empty */
			{ -1, 0 },	/* occupied */
			{ -1, 0 },	/* connected */
			{ -1, 0 },	/* unconfigured */
			{ 0, 0 },	/* partial */
			{ 0, 0 },	/* configured */
			{ 0, 0 },	/* release */
			{ 0, 0 },	/* unreferenced */
			{ -1, EIO },	/* fatal */
		}
	},
};

int _sfdr_err2errno_table[] = {
	0,		/* SFDR_ERR_NOERROR */
	EPROTO,		/* SFDR_ERR_INTERNAL */
	EIO,		/* SFDR_ERR_SUSPEND */
	EIO,		/* SFDR_ERR_RESUME */
	EIO,		/* SFDR_ERR_UNSAFE */
	EBUSY,		/* SFDR_ERR_UTHREAD */
	EBUSY,		/* SFDR_ERR_RTTHREAD */
	EBUSY,		/* SFDR_ERR_KTHREAD */
	ENODEV,		/* SFDR_ERR_OSFAILURE */
	EEXIST,		/* SFDR_ERR_OUTSTANDING */
	EPROTO,		/* SFDR_ERR_CONFIG */
	ENOMEM,		/* SFDR_ERR_NOMEM */
	EINVAL,		/* SFDR_ERR_PROTO */
	EBUSY,		/* SFDR_ERR_BUSY */
	ENODEV,		/* SFDR_ERR_NODEV */
	EINVAL,		/* SFDR_ERR_INVAL */
	EINVAL,		/* SFDR_ERR_STATE */
	EIO,		/* SFDR_ERR_PROBE */
	EIO,		/* SFDR_ERR_DEPROBE */
	EIO,		/* SFDR_ERR_HW_INTERCONNECT */
	EBUSY,		/* SFDR_ERR_OFFLINE */
	EINVAL,		/* SFDR_ERR_ONLINE */
	ENOMEM,		/* SFDR_ERR_POWERON */
	EBUSY,		/* SFDR_ERR_POWEROFF */
	EIO,		/* SFDR_ERR_JUGGLE_BOOTPROC */
	EIO,		/* SFDR_ERR_CANCEL */
};

/*
 * Global R/W lock to synchronize access across
 * multiple boards.  Users wanting multi-board access
 * must grab WRITE lock, others must grab READ lock.
 */
krwlock_t	sfdr_grwlock;
/*
 * Head of the boardlist used as a reference point for
 * locating PSM board structs.
 */
sfdr_board_t	*sfdr_boardlist;
/*
 * VA area used during CPU shutdown.
 */
caddr_t		sfdr_shutdown_va;
/*
 * Encapsulates list of DR-unsafe devices.
 */
sfdr_unsafe_devs_t	sfdr_unsafe_devs;

/*
 * Required/Expected functions.
 */
static dr_handle_t	*sfdr_get_handle(dev_t dev, void *softsp, intptr_t arg,
					int (*init_handle)(dr_handle_t *,
								void *,
								board_t *,
								dr_error_t *),
					void *init_arg);
static void		sfdr_release_handle(dr_handle_t *hp,
					void (*deinit_handle)(dr_handle_t *));
static int		sfdr_pre_op(dr_handle_t *hp);
static void		sfdr_post_op(dr_handle_t *hp);
static int		sfdr_probe_board(dr_handle_t *hp);
static int		sfdr_deprobe_board(dr_handle_t *hp);
static int		sfdr_connect(dr_handle_t *hp);
static int		sfdr_disconnect(dr_handle_t *hp);
static dr_devlist_t	*sfdr_get_attach_devlist(dr_handle_t *hp,
					int32_t *devnump, int32_t pass);
static int		sfdr_pre_attach_devlist(dr_handle_t *hp,
					dr_devlist_t *devlist, int32_t devnum);
static int		sfdr_post_attach_devlist(dr_handle_t *hp,
					dr_devlist_t *devlist, int32_t devnum);
static dr_devlist_t	*sfdr_get_release_devlist(dr_handle_t *hp,
					int32_t *devnump, int32_t pass);
static int		sfdr_pre_release_devlist(dr_handle_t *hp,
					dr_devlist_t *devlist, int32_t devnum);
static int		sfdr_post_release_devlist(dr_handle_t *hp,
					dr_devlist_t *devlist, int32_t devnum);
static boolean_t	sfdr_release_done(dr_handle_t *hp,
					dr_nodetype_t nodetype, dnode_t nodeid);
static dr_devlist_t	*sfdr_get_detach_devlist(dr_handle_t *hp,
					int32_t *devnump, int32_t pass);
static int		sfdr_pre_detach_devlist(dr_handle_t *hp,
					dr_devlist_t *devlist, int32_t devnum);
static int		sfdr_post_detach_devlist(dr_handle_t *hp,
					dr_devlist_t *devlist, int32_t devnum);
static int		sfdr_detach_mem(dr_handle_t *hp, dnode_t nodeid);
processorid_t		sfdr_get_cpuid(dr_handle_t *hp, dnode_t nodeid);
static int		sfdr_make_nodes(dev_info_t *dip);
static int		sfdr_cancel(dr_handle_t *hp);
static int		sfdr_status(dr_handle_t *hp);
static int		sfdr_ioctl(dr_handle_t *hp);
static int		sfdr_get_memhandle(dr_handle_t *hp, dnode_t nodeid,
					memhandle_t *mhp);
/*
 * Support functions.
 */
static sfdr_devset_t	sfdr_dev2devset(dev_t dev);
static int		sfdr_cancel_devset(dr_handle_t *hp,
					sfdr_devset_t devset);
static int		sfdr_copyin_ioarg(int mode, sfdr_ioctl_arg_t *iap,
					void *arg);
static int		sfdr_copyout_ioarg(int mode, sfdr_ioctl_arg_t *iap,
					void *arg);
static int		sfdr_check_transition(sfdr_board_t *sbp,
					sfdr_devset_t *devsetp,
					struct sfdr_state_trans *transp);
static dr_devlist_t	*sfdr_get_devlist(dr_handle_t *hp,
					sfdr_board_t *sbp,
					dr_nodetype_t nodetype,
					int max_units, uint_t uset,
					int *count, int present_only);
static int		sfdr_check_io_refs(dr_handle_t *hp,
					dr_devlist_t devlist[], int devnum);
static int		sfdr_check_io_attached(dev_info_t *dip, void *arg);
static int		sfdr_check_unit_attached(dnode_t nodeid,
					dr_nodetype_t nodetype);
static int		sfdr_cpu_status(dr_handle_t *hp, sfdr_devset_t devset,
					sfdr_dev_stat_t *dsp);
static int		sfdr_mem_status(dr_handle_t *hp, sfdr_devset_t devset,
					sfdr_dev_stat_t *dsp);
static int		sfdr_io_status(dr_handle_t *hp, sfdr_devset_t devset,
					sfdr_dev_stat_t *dsp);
static int		sfdr_make_dev_nodes(sfdr_board_t *sbp,
					sfdr_devset_t devset);
static int		sfdr_remove_dev_nodes(sfdr_board_t *sbp,
					sfdr_devset_t devset);
static int		sfdr_ioctl_cmd(dr_handle_t *hp, int cmd,
					void *arg);
static int		sfdr_init_devlists(sfdr_board_t *sbp);
static void		sfdr_init_cpu_unit(sfdr_board_t *sbp, int unit);
static void		sfdr_init_io_unit(sfdr_board_t *sbp, int unit);
static void		sfdr_board_discovery(sfdr_board_t *sbp);
static void		sfdr_board_init(sfdr_board_t *sbp, dev_info_t *dip,
					int bd);
static void		sfdr_board_destroy(sfdr_board_t *sbp);
static void		board_init(board_t *bp);
static void		board_destroy(board_t *bp);

extern int		sfdr_test_suspend(dr_handle_t *hp,
					sfdr_ioctl_arg_t *iap);
extern void		sfhw_dump_pdainfo(dr_handle_t *hp);

extern int		sfdr_test_cage(dr_handle_t *hp,
					sfdr_ioctl_arg_t *iap);

/*
 * CPU ops
 */
extern int		sfdr_pre_attach_cpu(dr_handle_t *hp,
					dr_devlist_t *devlist, int devnum);
extern int		sfdr_post_attach_cpu(dr_handle_t *hp,
					dr_devlist_t *devlist, int devnum);
extern int		sfdr_pre_release_cpu(dr_handle_t *hp,
					dr_devlist_t *devlist, int devnum);
extern int		sfdr_pre_detach_cpu(dr_handle_t *hp,
					dr_devlist_t *devlist, int devnum);
extern int		sfdr_post_detach_cpu(dr_handle_t *hp,
					dr_devlist_t *devlist, int devnum);
/*
 * MEM ops
 */
extern int		sfdr_pre_attach_mem(dr_handle_t *hp,
					dr_devlist_t *devlist, int devnum);
extern int		sfdr_post_attach_mem(dr_handle_t *hp,
					dr_devlist_t *devlist, int devnum);
extern int		sfdr_pre_release_mem(dr_handle_t *hp,
					dr_devlist_t *devlist, int devnum);
extern int		sfdr_pre_detach_mem(dr_handle_t *hp,
					dr_devlist_t *devlist, int devnum);
extern int		sfdr_post_detach_mem(dr_handle_t *hp,
					dr_devlist_t *devlist, int devnum);
/*
 * IO ops
 */
static int		sfdr_pre_attach_io(dr_handle_t *hp,
					dr_devlist_t *devlist, int devnum);
static int		sfdr_pre_detach_io(dr_handle_t *hp,
					dr_devlist_t *devlist, int devnum);
static int		sfdr_post_detach_io(dr_handle_t *hp,
					dr_devlist_t *devlist, int devnum);


static dr_ops_t sfdr_ops = {
	sfdr_get_handle,		/* handle */
	sfdr_release_handle,
	sfdr_pre_op,
	sfdr_post_op,

	sfdr_probe_board,		/* probe */
	sfdr_deprobe_board,

	sfdr_connect,			/* connect */
	sfdr_disconnect,

	sfdr_get_attach_devlist,	/* attach */
	sfdr_pre_attach_devlist,
	sfdr_post_attach_devlist,

	sfdr_get_release_devlist,	/* release */
	sfdr_pre_release_devlist,
	sfdr_post_release_devlist,
	sfdr_release_done,

	sfdr_get_detach_devlist,	/* detach */
	sfdr_pre_detach_devlist,
	sfdr_post_detach_devlist,

	sfdr_cancel,			/* cancel */

	sfdr_status,			/* status */

	sfdr_ioctl,			/* ioctl */

	sfdr_get_memhandle,		/* misc */
	sfdr_get_memlist,
	sfdr_detach_mem,
	sfdr_get_devtype,
	sfdr_get_cpuid,
	sfdr_make_nodes
};

/*
 * Autoconfiguration data structures
 */

extern struct mod_ops mod_miscops;

static struct modlmisc modlmisc = {
	&mod_miscops,
	"Dyn Recfg [E10K] (1.62)",
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modlmisc,
	NULL
};

int
_init(void)
{
	rw_init(&sfdr_grwlock, NULL, RW_DEFAULT, NULL);

	sfdr_shutdown_va = vmem_alloc(heap_arena, PAGESIZE, VM_SLEEP);

	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	int	err;

	if ((err = mod_remove(&modlinkage)) != 0)
		return (err);

	vmem_free(heap_arena, sfdr_shutdown_va, PAGESIZE);
	sfdr_shutdown_va = NULL;

	rw_destroy(&sfdr_grwlock);

	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * PSM-DR required routines for Starfire.
 */
/*ARGSUSED*/
int
dr_platform_init(void **softspp, dev_info_t *dip, dr_ops_t **opsp)
{
	int		b, rv;
	static fn_t	f = "dr_platform_init";

	PR_ALL("%s...\n", f);

	SFDR_LOCK_EXCL();

	if ((softspp == NULL) || (*softspp != NULL)) {
		SFDR_UNLOCK();
		return (-1);
	}

	sfdr_boardlist = GETSTRUCT(sfdr_board_t, MAX_BOARDS);

	for (b = 0; b < MAX_BOARDS; b++) {
		sfdr_board_init(BSLOT2MACHBD(b), dip, b);
		sfdr_board_table[b] = BSLOT2MACHBD(b);
	}

	*softspp = (void *)sfdr_boardlist;

	if (opsp != NULL)
		*opsp = &sfdr_ops;

	/*
	 * Init registered unsafe devs.
	 */
	sfdr_unsafe_devs.devnames = NULL;
	rv = ddi_prop_lookup_string_array(DDI_DEV_T_ANY, dip,
		DDI_PROP_DONTPASS | DDI_PROP_NOTPROM,
		"unsupported-io-drivers", &sfdr_unsafe_devs.devnames,
		&sfdr_unsafe_devs.ndevs);

	if (rv != DDI_PROP_SUCCESS)
		sfdr_unsafe_devs.ndevs = 0;

	SFDR_UNLOCK();

	return (0);
}

/*ARGSUSED*/
int
dr_platform_fini(void **softspp)
{
	int		b;
	static fn_t	f = "dr_platform_fini";

	PR_ALL("%s...\n", f);

	SFDR_LOCK_EXCL();

	if ((softspp == NULL) || (*softspp == NULL)) {
		SFDR_UNLOCK();
		return (-1);
	}

	ASSERT(sfdr_boardlist == (sfdr_board_t *)*softspp);

	for (b = 0; b < MAX_BOARDS; b++) {
		sfdr_board_destroy(BSLOT2MACHBD(b));
		sfdr_board_table[b] = NULL;
	}

	FREESTRUCT(sfdr_boardlist, sfdr_board_t, MAX_BOARDS);

	sfdr_boardlist = NULL;

	if (sfdr_unsafe_devs.devnames != NULL) {
		ddi_prop_free(sfdr_unsafe_devs.devnames);
		sfdr_unsafe_devs.devnames = NULL;
	}

	SFDR_UNLOCK();

	return (0);
}

static sfdr_devset_t
sfdr_dev2devset(dev_t dev)
{
	sfdr_devset_t	devset;

	devset = 0;

	if (DEV_IS_ALLBOARD(dev) || DEV_IS_CPU(dev)) {
		if (DEV_IS_ALLUNIT(dev))
			devset |= DEVSET(DR_NT_CPU, DEVSET_ANYUNIT);
		else
			devset |= DEVSET(DR_NT_CPU, DEV2UNIT(dev));
	}
	if (DEV_IS_ALLBOARD(dev) || DEV_IS_MEM(dev)) {
		if (DEV_IS_ALLUNIT(dev))
			devset |= DEVSET(DR_NT_MEM, DEVSET_ANYUNIT);
		else
			devset |= DEVSET(DR_NT_MEM, DEV2UNIT(dev));
	}
	if (DEV_IS_ALLBOARD(dev) || DEV_IS_IO(dev)) {
		if (DEV_IS_ALLUNIT(dev))
			devset |= DEVSET(DR_NT_IO, DEVSET_ANYUNIT);
		else
			devset |= DEVSET(DR_NT_IO, DEV2UNIT(dev));
	}

	return (devset);
}

/*
 * PSM-DR support routines for Starfire.
 */
/*ARGSUSED*/
static dr_handle_t *
sfdr_get_handle(dev_t dev, void *softsp, intptr_t arg,
		int (*init_handle)(dr_handle_t *, void *,
					board_t *, dr_error_t *),
		void *init_arg)
{
	dr_handle_t	*hp;
	board_t		*bp;
	dr_error_t	*ep;
	sfdr_error_t	*sep;
	sfdr_handle_t	*shp;
	sfdr_board_t	*sbp;
	int		bd;
	static fn_t	f = "sfdr_get_handle";

	PR_ALL("%s...\n", f);

	SFDR_LOCK_EXCL();

	if (softsp == NULL) {
		SFDR_UNLOCK();
		return (NULL);
	}

	bd = GETSLOT(dev);
	sbp = BSLOT2MACHBD(bd);
	bp = BSLOT2BD(bd);

	DR_BOARD_LOCK(bp);

	/*
	 * Brandnew handle.
	 */
	shp = GETSTRUCT(sfdr_handle_t, 1);
	shp->sh_arg = (void *)arg;

	hp = MACHHD2HD(shp);
	/*
	 * Initialize PIL portion of handle.
	 */
	ep = MACHERR2ERR(&shp->sh_err);

	if ((*init_handle)(hp, init_arg, bp, ep)) {
		FREESTRUCT(shp, sfdr_handle_t, 1);
		DR_BOARD_UNLOCK(bp);
		SFDR_UNLOCK();
		return (NULL);
	}

	shp->sh_devset = sfdr_dev2devset(hp->h_dev);
	shp->sh_next = sbp->sb_handle;
	sbp->sb_handle = shp;
	SFDR_BOARD_REF_INCR(sbp);

	sep = ERR2MACHERR(ep);
	SFDR_ALLOC_ERR(sep);

	/*
	 * Leave with the locks held.
	 */

	return (hp);
}

/*
 * Must return with board lock and global (PSM) rwlock dropped.
 */
/*ARGSUSED*/
static void
sfdr_release_handle(dr_handle_t *hp, void (*deinit_handle)(dr_handle_t *))
{
	sfdr_handle_t	*shp, **shpp;
	sfdr_board_t	*sbp;
	sfdr_error_t	*sep;
	board_t		*bp;
	static fn_t	f = "sfdr_release_handle";

	PR_ALL("%s...\n", f);

	if (hp == NULL)
		return;

	bp = hp->h_bd;

	ASSERT(SFDR_LOCK_HELD());
	ASSERT(DR_BOARD_LOCK_HELD(bp));

	shp = HD2MACHHD(hp);

	if (SFDR_HANDLE_REF(shp) != 0) {
		PR_ALL("%s: handle(dev = 0x%x) still in use (ref = %d)\n",
			f, (uint_t)hp->h_dev, SFDR_HANDLE_REF(shp));
		DR_BOARD_UNLOCK(bp);
		SFDR_UNLOCK();
		return;
	}

	sbp = BD2MACHBD(bp);
	sep = ERR2MACHERR(hp->h_err);

	/*
	 * Locate the handle in the board's reference list.
	 */
	for (shpp = &sbp->sb_handle;
		(*shpp) && ((*shpp) != shp);
		shpp = &((*shpp)->sh_next))
		;

	if (*shpp == NULL) {
		cmn_err(CE_WARN,
			"sfdr:%s: handle not found in board %d ref list "
			"(ref = %d)", f, sbp->sb_num, SFDR_BOARD_REF(sbp));
	} else {
		*shpp = shp->sh_next;

		SFDR_BOARD_REF_DECR(sbp);
	}

	ASSERT(SFDR_GET_ERRSTR(sep));

	/*
	 * Call PIL to free up any per-handle resources it
	 * may have allocated.
	 */
	(*deinit_handle)(hp);

	SFDR_FREE_ERR(sep);

	FREESTRUCT(shp, sfdr_handle_t, 1);

	DR_BOARD_UNLOCK(bp);
	SFDR_UNLOCK();
}

static int
sfdr_copyin_ioarg(int mode, sfdr_ioctl_arg_t *iap, void *arg)
{
	static fn_t	f = "sfdr_copyin_ioarg";

	if ((iap == NULL) || (arg == NULL))
		return (EINVAL);

	bzero((caddr_t)iap, sizeof (sfdr_ioctl_arg_t));

#ifdef _MULTI_DATAMODEL
	if (ddi_model_convert_from(mode & FMODELS) == DDI_MODEL_ILP32) {
		sfdr_ioctl_arg32_t	ioarg32;

		if (ddi_copyin((void *)arg, (void *)&ioarg32,
				sizeof (sfdr_ioctl_arg32_t), mode)) {
			cmn_err(CE_WARN,
				"sfdr:%s: (32bit) failed to copyin "
				"ioctl-cmd-arg", f);
			return (EFAULT);
		}
		iap->i_cpuid = ioarg32.i_cpuid;
		iap->i_major = ioarg32.i_major;
		iap->i_flags = ioarg32.i_flags;
	} else
#endif /* _MULTI_DATAMODEL */
	if (ddi_copyin((void *)arg, (void *)iap,
			sizeof (sfdr_ioctl_arg_t), mode) != 0) {
		cmn_err(CE_WARN,
			"sfdr:%s: failed to copyin ioctl-cmd-arg", f);
		return (EFAULT);
	}

	return (0);
}

static int
sfdr_copyout_ioarg(int mode, sfdr_ioctl_arg_t *iap, void *arg)
{
	static fn_t	f = "sfdr_copyout_ioarg";

	if ((iap == NULL) || (arg == NULL))
		return (EINVAL);

#ifdef _MULTI_DATAMODEL
	if (ddi_model_convert_from(mode & FMODELS) == DDI_MODEL_ILP32) {
		sfdr_ioctl_arg32_t	ioarg32;

		ioarg32.i_cpuid = iap->i_cpuid;
		ioarg32.i_major = iap->i_major;
		ioarg32.i_flags = iap->i_flags;
		ioarg32.i_isref = iap->i_isref;
		ioarg32.i_pim.ierr_num = iap->i_pim.ierr_num;
		bcopy((caddr_t)iap->i_pim.ierr_str,
			(caddr_t)ioarg32.i_pim.ierr_str,
			sizeof (ioarg32.i_pim.ierr_str));
		ioarg32.i_psm.ierr_num = iap->i_psm.ierr_num;
		bcopy((caddr_t)iap->i_psm.ierr_str,
			(caddr_t)ioarg32.i_psm.ierr_str,
			sizeof (ioarg32.i_psm.ierr_str));

		if (ddi_copyout((void *)&ioarg32, (void *)arg,
				sizeof (sfdr_ioctl_arg32_t), mode)) {
			cmn_err(CE_WARN,
				"sfdr:%s: failed to copyout ioctl-cmd-arg",
				f);
			return (EFAULT);
		}
	} else
#endif /* _MULTI_DATAMODEL */
	if (ddi_copyout((void *)iap, (void *)arg,
			sizeof (sfdr_ioctl_arg_t), mode) != 0) {
		cmn_err(CE_WARN,
			"sfdr:%s: failed to copyout ioctl-cmd-arg", f);
		return (EFAULT);
	}

	return (0);
}

/*
 * State transition policy is that if at least one
 * device cannot make the transition, then none of
 * the requested devices are allowed to transition.
 *
 * Returns the state that is in error, if any.
 */
static int
sfdr_check_transition(sfdr_board_t *sbp, sfdr_devset_t *devsetp,
			struct sfdr_state_trans *transp)
{
	int	s, ut;
	int	state_err = 0;
	sfdr_devset_t	devset;
	static fn_t	f = "sfdr_check_transition";

	devset = *devsetp;

	if (DEVSET_IN_SET(devset, DR_NT_CPU, DEVSET_ANYUNIT)) {
		for (ut = 0; ut < MAX_CPU_UNITS_PER_BOARD; ut++) {
			if (DEVSET_IN_SET(devset, DR_NT_CPU, ut) == 0)
				continue;
			s = (int)SFDR_DEVICE_STATE(sbp, DR_NT_CPU, ut);
			if (transp->x_op[s].x_rv) {
				if (!state_err)
					state_err = s;
				DEVSET_DEL(devset, DR_NT_CPU, ut);
			}
		}
	}
	if (DEVSET_IN_SET(devset, DR_NT_MEM, DEVSET_ANYUNIT)) {
		for (ut = 0; ut < MAX_MEM_UNITS_PER_BOARD; ut++) {
			if (DEVSET_IN_SET(devset, DR_NT_MEM, ut) == 0)
				continue;
			s = (int)SFDR_DEVICE_STATE(sbp, DR_NT_MEM, ut);
			if (transp->x_op[s].x_rv) {
				if (!state_err)
					state_err = s;
				DEVSET_DEL(devset, DR_NT_MEM, ut);
			}
		}
	}
	if (DEVSET_IN_SET(devset, DR_NT_IO, DEVSET_ANYUNIT)) {
		for (ut = 0; ut < MAX_IO_UNITS_PER_BOARD; ut++) {
			if (DEVSET_IN_SET(devset, DR_NT_IO, ut) == 0)
				continue;
			s = (int)SFDR_DEVICE_STATE(sbp, DR_NT_IO, ut);
			if (transp->x_op[s].x_rv) {
				if (!state_err)
					state_err = s;
				DEVSET_DEL(devset, DR_NT_IO, ut);
			}
		}
	}

	PR_ALL("%s: requested devset = 0x%x, final devset = 0x%x\n",
		f, (uint_t)*devsetp, (uint_t)devset);

	*devsetp = devset;
	/*
	 * If there are some remaining components for which
	 * this state transition is valid, then allow them
	 * through, otherwise if none are left then return
	 * the state error.
	 */
	return (devset ? 0 : state_err);
}

/*
 * pre-op entry point must DR_SET_ERRNO(), if needed.
 * Return valueu of non-zero indicates failure.
 */
/*ARGSUSED*/
static int
sfdr_pre_op(dr_handle_t *hp)
{
	int		rv = 0, t;
	int		dblocked = 0;
	int		cmd, serr = 0;
	char		errstr[80];
	sfdr_devset_t	devset;
	sfdr_board_t	*sbp = BD2MACHBD(hp->h_bd);
	sfdr_handle_t	*shp = HD2MACHHD(hp);
	sfdr_error_t	*sep = HD2MACHERR(hp);
	dr_error_t	*ep = DR_HD2ERR(hp);
	static fn_t	f = "sfdr_pre_op";

	cmd = hp->h_cmd;
	devset = shp->sh_devset;

	PR_ALL("%s (cmd = %s)...\n", f, (uint_t)SFDR_CMD_STR(cmd));

	ASSERT(SFDR_EXCL_LOCK_HELD());
	ASSERT(DR_BOARD_LOCK_HELD(hp->h_bd));

	/*
	 * Always turn on these bits ala Sunfire DR.
	 */
	hp->h_flags |= DR_FLAG_DEVI_REMOVE;
	hp->h_flags |= DR_FLAG_DEVI_FORCE;

	/*
	 * If we're dealing with memory at all, then we have
	 * to keep the "exclusive" global lock held.  This is
	 * necessary since we will probably need to look at
	 * multiple board structs.  Otherwise, we only have
	 * to deal with the board in question and so can drop
	 * the global lock to "shared".
	 */
	if (DEVSET_IN_SET(devset, DR_NT_MEM, DEVSET_ANYUNIT) == 0)
		SFDR_LOCK_DOWNGRADE();

	/*
	 * Check for any outstanding errors unless this is
	 * a status command in which case he's coming to
	 * potentially get the error.
	 */
	if (SFDR_BOARD_ERROR_PENDING(sbp)) {
		if (cmd != DR_CMD_STATUS) {
			/*
			 * As long as there are outstanding
			 * errors on the particular board in
			 * question, the caller must issue
			 * a STATUS to retrieve the error
			 * before any further DR operations
			 * can be performed.
			 */
			sprintf(errstr, "%s: outstanding error", f);
			SFDR_SET_ERR_STR(sep, SFDR_ERR_OUTSTANDING, errstr);
			/*
			 * Setting serr non-zero will cause it
			 * to effectively drop through and just
			 * copy out the error (outstanding) to
			 * the user.
			 */
			serr = SFDR_ERR2ERRNO(SFDR_GET_ERR(sep));
			DR_SET_ERRNO(ep, serr);
			cmn_err(CE_WARN,
				"sfdr:%s: outstanding errors for board %d",
				f, sbp->sb_num);
		}
	}

	/*
	 * Check for valid state transitions.
	 */
	if (!serr && ((t = CMD2INDEX(cmd)) != -1)) {
		struct sfdr_state_trans	*transp;
		int			state_err;

		transp = &sfdr_state_transition[t];
		ASSERT(transp->x_cmd == cmd);

		state_err = sfdr_check_transition(sbp, &devset, transp);

		if (state_err < 0) {
			/*
			 * Invalidate device.
			 */
			DR_SET_ERRNO(ep, EINVAL);
			serr = -1;
			PR_ALL("%s: invalid devset (0x%x)\n",
				f, (uint_t)devset);
		} else if (state_err != 0) {
			/*
			 * State transition is not a valid one.
			 */
			DR_SET_ERRNO(ep, transp->x_op[state_err].x_err);
			serr = transp->x_op[state_err].x_rv;
			PR_ALL("%s: invalid state %s(%d) for cmd %s(%d)\n",
				f, state_str[state_err], state_err,
				SFDR_CMD_STR(cmd), cmd);
		}
		if (serr && sep) {
			/*
			 * A state transition error occurred.
			 */
			if (serr < 0) {
				sprintf(errstr, "%s: invalid device", f);
				SFDR_SET_ERR_STR(sep, SFDR_ERR_INVAL, errstr);
			} else {
				sprintf(errstr,
					"%s: invalid state transition", f);
				SFDR_SET_ERR_STR(sep, SFDR_ERR_STATE, errstr);
			}
		} else {
			shp->sh_devset = devset;
		}
	}

	switch (cmd) {
	case DR_CMD_CONFIGURE:
	case DR_CMD_UNCONFIGURE:
		if (!serr && !rv) {
			PR_ALL("%s: calling "
				"i_ndi_block_device_tree_changes()...\n", f);
			i_ndi_block_device_tree_changes(&shp->sh_ndi);
			dblocked = 1;
		}
		/*FALLTHROUGH*/

	case DR_CMD_CONNECT:
	case DR_CMD_DISCONNECT:
	case DR_CMD_RELEASE:
	case DR_CMD_CANCEL:
		if (shp->sh_arg) {
			shp->sh_iap = GETSTRUCT(sfdr_ioctl_arg_t, 1);
			rv = sfdr_copyin_ioarg(hp->h_mode, shp->sh_iap,
						shp->sh_arg);
			if (rv) {
				if (!serr) {
					DR_SET_ERRNO(ep, rv);
				}
				FREESTRUCT(shp->sh_iap, sfdr_ioctl_arg_t, 1);
				shp->sh_iap = NULL;
				rv = -1;
			}
		}
		break;

	default:
		break;
	}

	if (serr && !rv && shp->sh_iap) {
		/*
		 * There was a state error.  We successfully copied
		 * in the ioctl argument, so let's fill in the
		 * error and copy it back out.  The pre-op routine
		 * will cause the PIM layer to immediately return.
		 */
		if (sep != NULL) {
			dr_error_t	*ep = MACHERR2ERR(sep);

			SFDR_SET_IOCTL_ERR(&shp->sh_iap->i_pim,
						DR_GET_ERRNO(ep),
						DR_GET_ERRSTR(ep));
			SFDR_SET_IOCTL_ERR(&shp->sh_iap->i_psm,
						SFDR_GET_ERR(sep),
						SFDR_GET_ERRSTR(sep));
		}
		(void) sfdr_copyout_ioarg(hp->h_mode, shp->sh_iap, shp->sh_arg);
		FREESTRUCT(shp->sh_iap, sfdr_ioctl_arg_t, 1);
		shp->sh_iap = NULL;
		rv = -1;
	}

	if (rv && dblocked) {
		PR_ALL("%s: calling i_ndi_allow_device_tree_changes()...\n",
			f);
		i_ndi_allow_device_tree_changes(shp->sh_ndi);
		shp->sh_ndi = 0;
	}

	return (rv);
}

/*ARGSUSED*/
static void
sfdr_post_op(dr_handle_t *hp)
{
	int		cmd;
	sfdr_error_t	*sep;
	sfdr_handle_t	*shp = HD2MACHHD(hp);
	static fn_t	f = "sfdr_post_op";

	cmd = hp->h_cmd;

	PR_ALL("%s (cmd = %s)...\n", f, (uint_t)SFDR_CMD_STR(cmd));

	sep = HD2MACHERR(hp);
	if (DR_GET_ERRNO(DR_HD2ERR(hp)) == 0) {
		/*
		 * Translate SFDR specific error to errno value
		 * for PIM layer only if the PIM errno hasn't
		 * already been set.
		 */
		DR_SET_ERRNO(DR_HD2ERR(hp),
				SFDR_ERR2ERRNO(SFDR_GET_ERR(sep)));
	}

	switch (cmd) {
	case DR_CMD_CONFIGURE:
	case DR_CMD_UNCONFIGURE:
		PR_ALL("%s: calling i_ndi_allow_device_tree_changes()...\n",
			f);
		i_ndi_allow_device_tree_changes(shp->sh_ndi);
		shp->sh_ndi = 0;
		/*FALLTHROUGH*/

	case DR_CMD_CONNECT:
	case DR_CMD_DISCONNECT:
	case DR_CMD_RELEASE:
	case DR_CMD_CANCEL:
		if (shp->sh_arg) {
			if (sep != NULL) {
				dr_error_t	*ep = MACHERR2ERR(sep);

				SFDR_SET_IOCTL_ERR(&shp->sh_iap->i_pim,
						DR_GET_ERRNO(ep),
						DR_GET_ERRSTR(ep));
				SFDR_SET_IOCTL_ERR(&shp->sh_iap->i_psm,
						SFDR_GET_ERR(sep),
						SFDR_GET_ERRSTR(sep));
			}
			(void) sfdr_copyout_ioarg(hp->h_mode, shp->sh_iap,
						shp->sh_arg);
			FREESTRUCT(shp->sh_iap, sfdr_ioctl_arg_t, 1);
			shp->sh_iap = NULL;
		}
		break;

	default:
		break;
	}
}

/*ARGSUSED*/
static int
sfdr_probe_board(dr_handle_t *hp)
{
	sfdr_handle_t	*shp = HD2MACHHD(hp);
	sfdr_board_t    *sbp = BD2MACHBD(hp->h_bd);
	int		cpuid;
	int		retval = SFDR_OBP_PROBE_GOOD;
	char		errstr[80];
	cpuset_t	cset;
	static fn_t	f = "sfdr_probe_board";

	PR_ALL("%s...\n", f);

	cpuid = shp->sh_iap ? shp->sh_iap->i_cpuid : -1;

	if ((cpuid < 0) || (cpuid >= max_ncpus)) {
		sprintf(errstr, "%s: invalid cpuid (%d)", f, cpuid);
		PR_ALL("%s\n", errstr);
		SFDR_SET_ERR_STR(HD2MACHERR(hp), SFDR_ERR_INVAL, errstr);
		return (-1);
	}

	cmn_err(CE_CONT,
		"DR: PROM attach board %d (cpu %d)\n",
		sbp->sb_num, cpuid);

	/*
	 * OBP disables traps during the board probe.
	 * So, in order to prevent cross-call/cross-trap timeouts,
	 * and thus panics, we effectively block anybody from
	 * issuing xc's/xt's by simply doing a xc_attention.
	 * In the previous version of Starfire DR (2.6), a timeout
	 * suspension mechanism was implemented in the send-mondo
	 * assembly.  That mechanism is unnecessary with the
	 * existence of xc_attention/xc_dismissed.
	 */
	cset = cpu_ready_set;
	xc_attention(cset);

	prom_interpret("dr-get-board", (uintptr_t)cpuid,
			(uintptr_t)&retval, 0, 0, 0);

	xc_dismissed(cset);

	if (retval == SFDR_OBP_PROBE_GOOD) {
		SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_OCCUPIED);
		return (0);
	} else {
		sprintf(errstr, "%s: probe failed for board %d",
			f, sbp->sb_num);
		SFDR_SET_ERR_STR(HD2MACHERR(hp), SFDR_ERR_PROBE, errstr);
		return (-1);
	}
}

/*ARGSUSED*/
static int
sfdr_deprobe_board(dr_handle_t *hp)
{
	sfdr_board_t	*sbp;
	sfdr_error_t	*sep;
	int		retval;
	cpuset_t	cset;
	char		errstr[80];
	static fn_t	f = "sfdr_deprobe_board";

	PR_ALL("%s...\n", f);

	sbp = BD2MACHBD(hp->h_bd);
	sep = HD2MACHERR(hp);

	PR_ALL("%s: board %d\n", f, sbp->sb_num);

	if (SFDR_BOARD_STATE(sbp) != SFDR_STATE_OCCUPIED) {
		PR_ALL("%s: invalid state %s(%d) for de-probe\n",
			f, state_str[SFDR_BOARD_STATE(sbp)],
			(int)SFDR_BOARD_STATE(sbp));
		sprintf(errstr, "%s: invalid state to deprobe board %d",
			f, sbp->sb_num);
		SFDR_SET_ERR_STR(sep, SFDR_ERR_STATE, errstr);
		return (-1);
	}

	ASSERT(SFDR_DEVS_PRESENT(sbp) == 0);

	cmn_err(CE_CONT, "DR: PROM detach board %d\n", sbp->sb_num);

	cset = cpu_ready_set;
	xc_attention(cset);

	prom_interpret("dr-detach-board", (uintptr_t)sbp->sb_num,
			(uintptr_t)&retval, 0, 0, 0);

	xc_dismissed(cset);

	PR_ALL("%s:%d: retval: %d\n", f, sbp->sb_num, retval);

	if (retval == SFDR_OBP_PROBE_GOOD) {
		SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_EMPTY);
		return (0);
	} else if (retval == SFDR_OBP_PROBE_BAD) {
		sprintf(errstr, "%s: deprobe failed for board %d",
			f, sbp->sb_num);
		SFDR_SET_ERR_STR(sep, SFDR_ERR_DEPROBE, errstr);
		return (-1);
	} else {
		return (0);
	}
}

/*ARGSUSED*/
static int
sfdr_init_devlists(sfdr_board_t *sbp)
{
	static dnode_t	child, firstchild;
	static char	name[OBP_MAXDRVNAME];
	int		i, ndev = 0;
	dnode_t		*devlist;
	int		bnum, pbnum;
	static fn_t	f = "sfdr_init_devlists";

	PR_ALL("%s (board = %d)...\n", f, sbp->sb_num);

	SFDR_DEVS_DISCONNECT(sbp, (uint_t)-1);
	bnum = sbp->sb_num;

	/*
	 * Clear out old entries, if any.
	 */
	for (i = 0; i < MAX_CPU_UNITS_PER_BOARD; i++)
		sbp->sb_devlist[NIX(DR_NT_CPU)][i] = (dnode_t)0;

	for (i = 0; i < MAX_MEM_UNITS_PER_BOARD; i++)
		sbp->sb_devlist[NIX(DR_NT_MEM)][i] = (dnode_t)0;

	for (i = 0; i < MAX_IO_UNITS_PER_BOARD; i++)
		sbp->sb_devlist[NIX(DR_NT_IO)][i] = (dnode_t)0;

	/*
	 * Find the obp top nodes for cpus, mem and I/O
	 */
	firstchild = prom_childnode(prom_rootnode());
	for (child = firstchild; child != OBP_NONODE;
			child = prom_nextnode(child)) {

		(void) prom_getprop(child, OBP_BOARDNUM, (caddr_t)&pbnum);
		if (pbnum != bnum)
			continue;

		(void) prom_getprop(child, OBP_NAME, (caddr_t)name);

		for (i = 0; SFDR_NT(i) != DR_NT_UNKNOWN; i++) {
			int	unit;

			if (strcmp(name, SFDR_DEVNAME(i)) != 0)
				continue;

			unit = sfdr_get_unit(child, SFDR_NT(i));

			devlist = sbp->sb_devlist[NIX(SFDR_NT(i))];

			SFDR_DEV_SET_PRESENT(sbp, SFDR_NT(i), unit);

			devlist[unit] = child;

			ndev++;

			break;
		}
	}
	return (ndev);

}

static void
sfdr_init_cpu_unit(sfdr_board_t *sbp, int unit)
{
	sfdr_state_t	new_state;
	sfdr_cpu_unit_t	*cp;
	int		cpuid;
	dnode_t		nodeid;

	if (SFDR_DEV_IS_ATTACHED(sbp, DR_NT_CPU, unit)) {
		new_state = SFDR_STATE_CONFIGURED;
	} else if (SFDR_DEV_IS_PRESENT(sbp, DR_NT_CPU, unit)) {
		new_state = SFDR_STATE_CONNECTED;
	} else {
		new_state = SFDR_STATE_EMPTY;
	}
	SFDR_DEVICE_TRANSITION(sbp, DR_NT_CPU, unit, new_state);

	nodeid = sbp->sb_devlist[NIX(DR_NT_CPU)][unit];

	cp = SFDR_GET_BOARD_CPUUNIT(sbp, unit);
	cpuid = sfdr_get_cpuid(MACHBD2HD(sbp), nodeid);

	cp->sbc_cpu_id = cpuid;
	mutex_enter(&cpu_lock);
	if ((cpuid >= 0) && cpu[cpuid])
		cp->sbc_cpu_status = cpu_status(cpu[cpuid]);
	else
		cp->sbc_cpu_status = P_BAD;
	mutex_exit(&cpu_lock);
}

static void
sfdr_init_io_unit(sfdr_board_t *sbp, int unit)
{
	sfdr_state_t	new_state;

	if (SFDR_DEV_IS_ATTACHED(sbp, DR_NT_IO, unit)) {
		new_state = SFDR_STATE_CONFIGURED;
	} else if (SFDR_DEV_IS_PRESENT(sbp, DR_NT_IO, unit)) {
		new_state = SFDR_STATE_CONNECTED;
	} else {
		new_state = SFDR_STATE_EMPTY;
	}
	SFDR_DEVICE_TRANSITION(sbp, DR_NT_IO, unit, new_state);
}

/*
 * Only do work if called to operate on an entire board
 * which doesn't already have components present.
 */
static int
sfdr_connect(dr_handle_t *hp)
{
	sfdr_board_t	*sbp = BD2MACHBD(hp->h_bd);
	sfdr_error_t	*sep = HD2MACHERR(hp);
	static fn_t	f = "sfdr_connect";

	PR_ALL("%s...\n", f);

	if (SFDR_DEVS_PRESENT(sbp)) {
		/*
		 * Board already has devices present.
		 */
		PR_ALL("%s: devices already present (0x%x)\n",
			f, SFDR_DEVS_PRESENT(sbp));
		return (0);
	}
	if (!DEV_IS_ALLBOARD(hp->h_dev)) {
		/*
		 * Connect is only relevant when operating
		 * on entire board.
		 */
		PR_ALL("%s: no-op on individual components\n", f);
		return (0);
	}

	if (!(sfdr_init_devlists(sbp))) {
		char	errstr[80];

		sprintf(errstr, "%s: no devices present on board %d",
			f, sbp->sb_num);
		cmn_err(CE_WARN, "sfdr:%s", errstr);
		SFDR_SET_ERR_STR(sep, SFDR_ERR_NODEV, errstr);
		return (-1);
	} else {
		int	i;
		/*
		 * Make nodes for the individual components
		 * on the board.
		 * First we need to initialize mem-unit
		 * section of board structure.
		 */
		for (i = 0; i < MAX_MEM_UNITS_PER_BOARD; i++)
			sfdr_init_mem_unit(sbp, i);
		/*
		 * Initialize sb_cpu sections.
		 */
		for (i = 0; i < MAX_CPU_UNITS_PER_BOARD; i++)
			sfdr_init_cpu_unit(sbp, i);
		/*
		 * Initialize sb_io sections.
		 */
		for (i = 0; i < MAX_IO_UNITS_PER_BOARD; i++)
			sfdr_init_io_unit(sbp, i);

		(void) sfdr_make_dev_nodes(sbp, SFDR_DEVS_PRESENT(sbp));
		SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_CONNECTED);
		return (0);
	}
}

static int
sfdr_disconnect(dr_handle_t *hp)
{
	int		i;
	sfdr_devset_t	devset;
	sfdr_board_t	*sbp;
	static fn_t	f = "sfdr_disconnect";

	PR_ALL("%s...\n", f);

	sbp = BD2MACHBD(hp->h_bd);

	/*
	 * Only devices which are present, but
	 * unattached can be disconnected.
	 */
	devset = HD2MACHHD(hp)->sh_devset & SFDR_DEVS_PRESENT(sbp) &
			SFDR_DEVS_UNATTACHED(sbp);

	if (devset == 0) {
		char	errstr[80];

		sprintf(errstr, "%s: no devices present on board %d",
			f, sbp->sb_num);
		/*
		 * Return non-zero so that PIM layer doesn't
		 * go through deprobe.
		 */
		PR_ALL("%s\n", errstr);
		SFDR_SET_ERR_STR(HD2MACHERR(hp), SFDR_ERR_NODEV, errstr);
		return (1);
	}

	sfdr_remove_dev_nodes(sbp, devset);
	SFDR_DEVS_DISCONNECT(sbp, devset);

	ASSERT((SFDR_DEVS_ATTACHED(sbp) & devset) == 0);

	/*
	 * Update per-device state transitions.
	 */
	for (i = 0; i < MAX_CPU_UNITS_PER_BOARD; i++)
		if (DEVSET_IN_SET(devset, DR_NT_CPU, i)) {
			if (sfdr_disconnect_cpu(hp, i) == 0) {
				SFDR_DEVICE_TRANSITION(sbp, DR_NT_CPU, i,
							SFDR_STATE_EMPTY);
			}
		}
	for (i = 0; i < MAX_MEM_UNITS_PER_BOARD; i++)
		if (DEVSET_IN_SET(devset, DR_NT_MEM, i)) {
			if (sfdr_disconnect_mem(hp, i) == 0) {
				SFDR_DEVICE_TRANSITION(sbp, DR_NT_MEM, i,
							SFDR_STATE_EMPTY);
			}
		}
	for (i = 0; i < MAX_IO_UNITS_PER_BOARD; i++)
		if (DEVSET_IN_SET(devset, DR_NT_IO, i)) {
			if (sfdr_disconnect_io(hp, i) == 0) {
				SFDR_DEVICE_TRANSITION(sbp, DR_NT_IO, i,
							SFDR_STATE_EMPTY);
			}
		}

	/*
	 * Once all the components on a board have been disconnect
	 * the board's state can transition to disconnected and
	 * we can allow the deprobe to take place.
	 */
	if (SFDR_DEVS_PRESENT(sbp) == 0) {
		SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_OCCUPIED);
		return (0);
	} else {
		return (-1);
	}
}

/*ARGSUSED*/
int
sfdr_disconnect_io(dr_handle_t *hp, int unit)
{
	return (0);
}

/*
 * upaid = IBBBBPp
 *	I = I/O
 *	BBBB = board #
 *	Pp = port #
 */
int
sfdr_get_unit(dnode_t nodeid, dr_nodetype_t nodetype)
{
	int	unit = -1;
	int	upaid;

	switch (nodetype) {
	case DR_NT_CPU:
		if (prom_getprop(nodeid, "upa-portid", (caddr_t)&upaid) < 0)
			unit = -1;
		else
			unit = upaid & 3;
		break;

	case DR_NT_MEM:
		unit = 0;
		break;

	case DR_NT_IO:
		if (prom_getprop(nodeid, "upa-portid", (caddr_t)&upaid) < 0) {
			unit = -1;
		} else {
			ASSERT(upaid & 0x40);
			unit = upaid & 1;
		}
		break;

	default:
		break;
	}

	return (unit);
}

int
sfdr_get_board(dnode_t nodeid)
{
	int	board;

	if (prom_getprop(nodeid, "board#", (caddr_t)&board) < 0)
		return (-1);
	else
		return (board);
}

/*
 * Return a list of the nodeid's of devices that are
 * either present and attached, or present only but
 * not yet attached for the given board.
 */
/*ARGSUSED*/
dr_devlist_t *
sfdr_get_devlist(dr_handle_t *hp, sfdr_board_t *sbp, dr_nodetype_t nodetype,
		int max_units, uint_t uset, int *count, int present_only)
{
	int		i, ix;
	dr_devlist_t	*ret_devlist;
	dnode_t		*devlist;

	*count = 0;
	ret_devlist = GETSTRUCT(dr_devlist_t, max_units);
	devlist = sbp->sb_devlist[NIX(nodetype)];
	/*
	 * Turn into binary value since we're going
	 * to be using XOR for a comparison.
	 * if (present_only) then
	 *	dev must be PRESENT, but NOT ATTACHED.
	 * else
	 *	dev must be PRESENT AND ATTACHED.
	 * endif
	 */
	if (present_only)
		present_only = 1;

	for (i = ix = 0; (i < max_units) && uset; i++) {
		int	ut, is_present, is_attached;
		dnode_t	nodeid;

		if ((nodeid = devlist[i]) == (dnode_t)0)
			continue;
		ut = sfdr_get_unit(nodeid, nodetype);
		if ((uset & (1 << ut)) == 0)
			continue;
		uset &= ~(1 << ut);
		is_present = SFDR_DEV_IS_PRESENT(sbp, nodetype, ut) ? 1 : 0;
		is_attached = SFDR_DEV_IS_ATTACHED(sbp, nodetype, ut) ? 1 : 0;

		if (is_present && (present_only ^ is_attached)) {
			ret_devlist[ix].dv_nodeid = nodeid;
			DR_ALLOC_ERR(&ret_devlist[ix].dv_error);
			ix++;
		}
	}

	if ((*count = ix) == 0) {
		FREESTRUCT(ret_devlist, dr_devlist_t, max_units);
		ret_devlist = NULL;
	}

	return (ret_devlist);
}

/*ARGSUSED*/
static dr_devlist_t *
sfdr_get_attach_devlist(dr_handle_t *hp, int32_t *devnump, int32_t pass)
{
	sfdr_board_t	*sbp;
	uint_t		uset;
	sfdr_devset_t	devset;
	dr_devlist_t	*attach_devlist;
	static int	next_pass = 1;
	static fn_t	f = "sfdr_get_attach_devlist";

	PR_ALL("%s (pass = %d)...\n", f, pass);

	sbp = BD2MACHBD(hp->h_bd);
	devset = HD2MACHHD(hp)->sh_devset;

	*devnump = 0;
	attach_devlist = NULL;

	/*
	 * We switch on next_pass for the cases where a board
	 * does not contain a particular type of component.
	 * In these situations we don't want to return NULL
	 * prematurely.  We need to check other devices and
	 * we don't want to check the same type multiple times.
	 * For example, if there were no cpus, then on pass 1
	 * we would drop through and return the memory nodes.
	 * However, on pass 2 we would switch back to the memory
	 * nodes thereby returning them twice!  Using next_pass
	 * forces us down to the end (or next item).
	 */
	if (pass == 1)
		next_pass = 1;

	switch (next_pass) {
	case 1:		/* DR_NT_CPU */
		if (DEVSET_IN_SET(devset, DR_NT_CPU, DEVSET_ANYUNIT)) {
			uset = DEVSET_GET_UNITSET(devset, DR_NT_CPU);

			attach_devlist = sfdr_get_devlist(hp, sbp, DR_NT_CPU,
						MAX_CPU_UNITS_PER_BOARD,
						uset, devnump, 1);

			DEVSET_DEL(devset, DR_NT_CPU, DEVSET_ANYUNIT);
			if (!devset || attach_devlist) {
				next_pass = 2;
				return (attach_devlist);
			}
			/*
			 * If the caller is interested in the entire
			 * board, but there aren't any cpus, then just
			 * fall through to check for the next component.
			 */
		}
		/*FALLTHROUGH*/

	case 2:		/* DR_NT_MEM */
		if (DEVSET_IN_SET(devset, DR_NT_MEM, DEVSET_ANYUNIT)) {
			uset = DEVSET_GET_UNITSET(devset, DR_NT_MEM);

			attach_devlist = sfdr_get_devlist(hp, sbp, DR_NT_MEM,
						MAX_MEM_UNITS_PER_BOARD,
						uset, devnump, 1);

			DEVSET_DEL(devset, DR_NT_MEM, DEVSET_ANYUNIT);
			if (!devset || attach_devlist) {
				next_pass = 3;
				return (attach_devlist);
			}
			/*
			 * If the caller is interested in the entire
			 * board, but there isn't any memory, then
			 * just fall through to next component.
			 */
		}
		/*FALLTHROUGH*/

	case 3:		/* DR_NT_IO */
		next_pass = -1;
		if (DEVSET_IN_SET(devset, DR_NT_IO, DEVSET_ANYUNIT)) {
			uset = DEVSET_GET_UNITSET(devset, DR_NT_IO);

			attach_devlist = sfdr_get_devlist(hp, sbp, DR_NT_IO,
						MAX_IO_UNITS_PER_BOARD,
						uset, devnump, 1);

			DEVSET_DEL(devset, DR_NT_IO, DEVSET_ANYUNIT);
			if (!devset || attach_devlist) {
				next_pass = 4;
				return (attach_devlist);
			}
		}
		/*FALLTHROUGH*/

	default:
		*devnump = 0;
		return (NULL);
	}
	/*NOTREACHED*/
}

/*ARGSUSED*/
static int
sfdr_pre_attach_devlist(dr_handle_t *hp, dr_devlist_t *devlist, int32_t devnum)
{
	int		max_units = 0, rv = 0;
	dr_nodetype_t	nodetype;
	static fn_t	f = "sfdr_pre_attach_devlist";

	/*
	 * On Starfire, all entries in a devlist[] are
	 * of the same nodetype.
	 */
	nodetype = sfdr_get_devtype(hp, devlist[0].dv_nodeid);

	PR_ALL("%s (nt = %s(%d), num = %d)...\n",
		f, nt_str[(int)nodetype], (int)nodetype, devnum);

	switch (nodetype) {
	case DR_NT_CPU:
		max_units = MAX_CPU_UNITS_PER_BOARD;
		rv = sfdr_pre_attach_cpu(hp, devlist, devnum);
		break;

	case DR_NT_MEM:
		max_units = MAX_MEM_UNITS_PER_BOARD;
		rv = sfdr_pre_attach_mem(hp, devlist, devnum);
		break;

	case DR_NT_IO:
		max_units = MAX_IO_UNITS_PER_BOARD;
		rv = sfdr_pre_attach_io(hp, devlist, devnum);
		break;

	default:
		rv = -1;
		break;
	}

	if (rv && max_units) {
		int	i;
		/*
		 * Need to clean up dynamically allocated devlist
		 * if pre-op is going to fail.
		 */
		for (i = 0; i < max_units; i++) {
			if (DR_ERR_ISALLOC(&devlist[i].dv_error)) {
				DR_FREE_ERR(&devlist[i].dv_error);
			}
		}
		FREESTRUCT(devlist, dr_devlist_t, max_units);
	}

	/*
	 * If an error occurred, return "continue"
	 * indication so that we can continue attaching
	 * as much as possible.
	 */
	return (rv ? -1 : 0);
}

/*ARGSUSED*/
static int
sfdr_post_attach_devlist(dr_handle_t *hp, dr_devlist_t *devlist,
			int32_t devnum)
{
	int		i, max_units = 0, rv = 0;
	sfdr_devset_t	devs_unattached, devs_present;
	dr_nodetype_t	nodetype;
	sfdr_board_t	*sbp;
	static fn_t	f = "sfdr_post_attach_devlist";

	sbp = BD2MACHBD(hp->h_bd);
	nodetype = sfdr_get_devtype(hp, devlist[0].dv_nodeid);

	PR_ALL("%s (nt = %s(%d), num = %d)...\n",
		f, nt_str[(int)nodetype], (int)nodetype, devnum);

	/*
	 * Need to free up devlist[] created earlier in
	 * sfdr_get_attach_devlist().
	 */
	switch (nodetype) {
	case DR_NT_CPU:
		max_units = MAX_CPU_UNITS_PER_BOARD;
		rv = sfdr_post_attach_cpu(hp, devlist, devnum);
		break;

	case DR_NT_MEM:
		max_units = MAX_MEM_UNITS_PER_BOARD;
		rv = sfdr_post_attach_mem(hp, devlist, devnum);
		break;

	case DR_NT_IO:
		max_units = MAX_IO_UNITS_PER_BOARD;
		break;

	default:
		rv = -1;
		break;
	}

	for (i = 0; i < devnum; i++) {
		int		unit;
		dr_error_t	*ep;

		ep = &devlist[i].dv_error;

		if (DR_GET_ERRNO(ep)) {
			dr_error_t	*hep = DR_HD2ERR(hp);

			if (DR_GET_ERRNO(hep) == 0) {
				DR_SET_ERRNO(hep, DR_GET_ERRNO(ep));
				DR_SET_ERRSTR(hep, DR_GET_ERRSTR(ep));
			}
			PR_ALL("%s: PIM ERROR (%d, %s)\n",
				f, DR_GET_ERRNO(ep), DR_GET_ERRSTR(ep));
		}

		unit = sfdr_check_unit_attached(devlist[i].dv_nodeid,
						nodetype);
		if (unit == -1) {
			PR_ALL("%s: ERROR (nt=%s, b=%d, u=%d) not attached\n",
				f, nt_str[(int)nodetype], sbp->sb_num, i);
			continue;
		}

		SFDR_DEV_SET_ATTACHED(sbp, nodetype, unit);
		SFDR_DEVICE_TRANSITION(sbp, nodetype, unit,
						SFDR_STATE_CONFIGURED);
	}

	if (rv) {
		PR_ALL("%s: ERROR (pim=%d, psm=%d) during ATTACH\n",
			f, DR_GET_ERRNO(DR_HD2ERR(hp)),
			SFDR_GET_ERR(HD2MACHERR(hp)));
	}

	devs_present = SFDR_DEVS_PRESENT(sbp);
	devs_unattached = SFDR_DEVS_UNATTACHED(sbp);

	switch (SFDR_BOARD_STATE(sbp)) {
	case SFDR_STATE_CONNECTED:
	case SFDR_STATE_UNCONFIGURED:
		ASSERT(devs_present);

		if (devs_unattached == 0) {
			/*
			 * All devices finally attached.
			 */
			SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_CONFIGURED);
		} else if (devs_present != devs_unattached) {
			/*
			 * Only some devices are fully attached.
			 */
			SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_PARTIAL);
		}
		break;

	case SFDR_STATE_PARTIAL:
		ASSERT(devs_present);
		/*
		 * All devices finally attached.
		 */
		if (devs_unattached == 0) {
			SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_CONFIGURED);
		}
		break;

	default:
		break;
	}

	if (max_units && devlist) {
		int	i;

		for (i = 0; i < max_units; i++) {
			if (DR_ERR_ISALLOC(&devlist[i].dv_error)) {
				DR_FREE_ERR(&devlist[i].dv_error);
			}
		}
		FREESTRUCT(devlist, dr_devlist_t, max_units);
	}

	/*
	 * Our policy is to attach all components that are
	 * possible, thus we always return "success" on the
	 * pre and post operations.
	 */
	return (0);
}

/*
 * We only need to "release" cpu and memory devices.
 */
/*ARGSUSED*/
static dr_devlist_t *
sfdr_get_release_devlist(dr_handle_t *hp, int32_t *devnump, int32_t pass)
{
	sfdr_board_t	*sbp;
	uint_t		uset;
	sfdr_devset_t	devset;
	dr_devlist_t	*release_devlist;
	static int	next_pass = 1;
	static fn_t	f = "sfdr_get_release_devlist";

	PR_ALL("%s (pass = %d)...\n", f, pass);

	sbp = BD2MACHBD(hp->h_bd);
	devset = HD2MACHHD(hp)->sh_devset;

	*devnump = 0;
	release_devlist = NULL;

	/*
	 * We switch on next_pass for the cases where a board
	 * does not contain a particular type of component.
	 * In these situations we don't want to return NULL
	 * prematurely.  We need to check other devices and
	 * we don't want to check the same type multiple times.
	 * For example, if there were no cpus, then on pass 1
	 * we would drop through and return the memory nodes.
	 * However, on pass 2 we would switch back to the memory
	 * nodes thereby returning them twice!  Using next_pass
	 * forces us down to the end (or next item).
	 */
	if (pass == 1)
		next_pass = 1;

	switch (next_pass) {
	case 1:		/* DR_NT_CPU */
		if (DEVSET_IN_SET(devset, DR_NT_CPU, DEVSET_ANYUNIT)) {
			uset = DEVSET_GET_UNITSET(devset, DR_NT_CPU);

			release_devlist = sfdr_get_devlist(hp, sbp,
						DR_NT_CPU,
						MAX_CPU_UNITS_PER_BOARD,
						uset, devnump, 0);

			DEVSET_DEL(devset, DR_NT_CPU, DEVSET_ANYUNIT);
			if (!devset || release_devlist) {
				next_pass = 2;
				return (release_devlist);
			}
			/*
			 * If the caller is interested in the entire
			 * board, but there aren't any cpus, then just
			 * fall through to check for the next component.
			 */
		}
		/*FALLTHROUGH*/

	case 2:		/* DR_NT_MEM */
		if (DEVSET_IN_SET(devset, DR_NT_MEM, DEVSET_ANYUNIT)) {
			uset = DEVSET_GET_UNITSET(devset, DR_NT_MEM);

			release_devlist = sfdr_get_devlist(hp, sbp,
						DR_NT_MEM,
						MAX_MEM_UNITS_PER_BOARD,
						uset, devnump, 0);

			DEVSET_DEL(devset, DR_NT_MEM, DEVSET_ANYUNIT);
			if (!devset || release_devlist) {
				next_pass = 3;
				return (release_devlist);
			}
			/*
			 * If the caller is interested in the entire
			 * board, but there isn't any memory, then
			 * just fall through to next component.
			 */
		}
		/*FALLTHROUGH*/

	case 3:		/* DR_NT_IO */
		next_pass = -1;
		if (DEVSET_IN_SET(devset, DR_NT_IO, DEVSET_ANYUNIT)) {
			uset = DEVSET_GET_UNITSET(devset, DR_NT_IO);

			release_devlist = sfdr_get_devlist(hp, sbp,
						DR_NT_IO,
						MAX_IO_UNITS_PER_BOARD,
						uset, devnump, 0);

			DEVSET_DEL(devset, DR_NT_IO, DEVSET_ANYUNIT);
			if (!devset || release_devlist) {
				next_pass = 4;
				return (release_devlist);
			}
		}
		/*FALLTHROUGH*/

	default:
		*devnump = 0;
		return (NULL);
	}
	/*NOTREACHED*/
}

/*ARGSUSED*/
static int
sfdr_pre_release_devlist(dr_handle_t *hp, dr_devlist_t *devlist,
			int32_t devnum)
{
	int		max_units = 0, rv = 0;
	dr_nodetype_t	nodetype;
	static fn_t	f = "sfdr_pre_release_devlist";

	nodetype = sfdr_get_devtype(hp, devlist[0].dv_nodeid);

	PR_ALL("%s (nt = %s(%d), num = %d)...\n",
		f, nt_str[(int)nodetype], (int)nodetype, devnum);

	/*
	 * There is only a pre-release for memory nodes.
	 */
	switch (nodetype) {
	case DR_NT_CPU:
		max_units = MAX_CPU_UNITS_PER_BOARD;
		rv = sfdr_pre_release_cpu(hp, devlist, devnum);
		break;

	case DR_NT_MEM:
		max_units = MAX_MEM_UNITS_PER_BOARD;
		rv = sfdr_pre_release_mem(hp, devlist, devnum);
		break;

	case DR_NT_IO:
		max_units = MAX_IO_UNITS_PER_BOARD;
		break;

	default:
		rv = -1;
		break;
	}

	/*
	 * Update reference count in handle, since we now
	 * have outstanding events for the given handle.
	 */
	SFDR_HANDLE_REF_ADD(HD2MACHHD(hp), devnum);

	if (rv && max_units) {
		int	i;
		/*
		 * Need to clean up dynamically allocated devlist
		 * if pre-op is going to fail.
		 */
		for (i = 0; i < max_units; i++) {
			if (DR_ERR_ISALLOC(&devlist[i].dv_error)) {
				DR_FREE_ERR(&devlist[i].dv_error);
			}
		}
		FREESTRUCT(devlist, dr_devlist_t, max_units);
	}

	return (rv ? -1 : 0);
}

/*ARGSUSED*/
static int
sfdr_post_release_devlist(dr_handle_t *hp, dr_devlist_t *devlist,
			int32_t devnum)
{
	int		i, max_units = 0;
	dr_nodetype_t	nodetype;
	sfdr_board_t	*sbp = BD2MACHBD(hp->h_bd);
	static fn_t	f = "sfdr_post_release_devlist";

	nodetype = sfdr_get_devtype(hp, devlist[0].dv_nodeid);

	PR_ALL("%s (nt = %s(%d), num = %d)...\n",
		f, nt_str[(int)nodetype], (int)nodetype, devnum);

	/*
	 * Need to free up devlist[] created earlier in
	 * sfdr_get_release_devlist().
	 */
	switch (nodetype) {
	case DR_NT_CPU:
		max_units = MAX_CPU_UNITS_PER_BOARD;
		break;

	case DR_NT_MEM:
		max_units = MAX_MEM_UNITS_PER_BOARD;
		break;

	case DR_NT_IO:
		max_units = MAX_IO_UNITS_PER_BOARD;
		break;

	default:
		{
			char	errstr[80];

			sprintf(errstr, "%s: invalid nodetype (%d)",
				f, (int)nodetype);
			SFDR_SET_ERR_STR(HD2MACHERR(hp), SFDR_ERR_INVAL,
					errstr);
		}
		break;
	}

	for (i = 0; i < devnum; i++) {
		int		unit;
		dr_error_t	*ep;

		ep = &devlist[i].dv_error;

		if (DR_GET_ERRNO(ep)) {
			dr_error_t	*hep = DR_HD2ERR(hp);

			if (DR_GET_ERRNO(hep) == 0) {
				DR_SET_ERRNO(hep, DR_GET_ERRNO(ep));
				DR_SET_ERRSTR(hep, DR_GET_ERRSTR(ep));
			}
			continue;
		}

		unit = sfdr_get_unit(devlist[i].dv_nodeid, nodetype);
		if (unit == -1)
			continue;

		SFDR_DEV_SET_RELEASED(sbp, nodetype, unit);
		SFDR_DEVICE_TRANSITION(sbp, nodetype, unit, SFDR_STATE_RELEASE);
	}

	if (DR_GET_ERRNO(DR_HD2ERR(hp))) {
		PR_ALL("%s: ERROR (pim=%d, psm=%d) during RELEASE\n",
			f, DR_GET_ERRNO(DR_HD2ERR(hp)),
			SFDR_GET_ERR(HD2MACHERR(hp)));
	}

	/*
	 * If we're not already in the RELEASE state for this
	 * board and we now have released all that were previously
	 * attached, then transfer the board to the RELEASE state.
	 */
	if ((SFDR_BOARD_STATE(sbp) != SFDR_STATE_RELEASE) &&
		(SFDR_DEVS_RELEASED(sbp) == SFDR_DEVS_ATTACHED(sbp))) {
		SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_RELEASE);
	}

	if (max_units && devlist) {
		int	i;

		for (i = 0; i < max_units; i++) {
			if (DR_ERR_ISALLOC(&devlist[i].dv_error)) {
				DR_FREE_ERR(&devlist[i].dv_error);
			}
		}
		FREESTRUCT(devlist, dr_devlist_t, max_units);
	}

	return (DR_GET_ERRNO(DR_HD2ERR(hp)) ? -1 : 0);
}

int
sfdr_release_dev_done(dr_handle_t *hp, dr_nodetype_t nodetype, int unit)
{
	sfdr_board_t	*sbp = BD2MACHBD(hp->h_bd);

	if (SFDR_DEVICE_STATE(sbp, nodetype, unit) == SFDR_STATE_RELEASE) {
		ASSERT(SFDR_DEV_IS_RELEASED(sbp, nodetype, unit));
		SFDR_DEV_SET_UNREFERENCED(sbp, nodetype, unit);
		SFDR_DEVICE_TRANSITION(sbp, nodetype, unit,
					SFDR_STATE_UNREFERENCED);
		return (0);
	} else {
		return (-1);
	}
}

/*
 * Must return with board lock and global rwlock held
 * if returning B_TRUE, otherwise drop locks before return.
 */
/*ARGSUSED*/
static boolean_t
sfdr_release_done(dr_handle_t *hp, dr_nodetype_t nodetype, dnode_t nodeid)
{
	int		unit;
	sfdr_board_t	*sbp;
	dr_error_t	*ep;
	sfdr_error_t	*sep;
	sfdr_devset_t	errset;
	static fn_t	f = "sfdr_release_done";

	PR_ALL("%s...\n", f);

	SFDR_LOCK_SHARED();
	DR_BOARD_LOCK(hp->h_bd);

	sbp = BD2MACHBD(hp->h_bd);

	if ((unit = sfdr_get_unit(nodeid, nodetype)) < 0) {
		cmn_err(CE_WARN,
			"sfdr:%s: unable to get unit for nodeid (0x%x)",
			f, (uint_t)nodeid);
		/*
		 * XXX - Is this considered fatal?
		 */
		DR_BOARD_UNLOCK(hp->h_bd);
		SFDR_UNLOCK();
		return (B_FALSE);
	}

	ep = DR_HD2ERR(hp);
	sep = HD2MACHERR(hp);
	if (!sbp->sb_lastop.l_cmd && (SFDR_GET_ERR(sep) ||
					DR_GET_ERRNO(ep))) {
		/*
		 * Some error occurred.  Need to save off in board's
		 * error structure for later reference.
		 */
		PR_ALL("%s: error occurred (cmd=0x%x, dev=0x%x, "
			"pim=%d, psm=%d)\n",
			f, hp->h_cmd, (uint_t)hp->h_dev,
			DR_GET_ERRNO(ep), SFDR_GET_ERR(sep));

		sbp->sb_lastop.l_cmd = hp->h_cmd;
		sbp->sb_lastop.l_dev = hp->h_dev;
		/*
		 * Copy in the errors from the handle.
		 */
		SFDR_SET_IOCTL_ERR(&sbp->sb_lastop.l_pimerr,
					DR_GET_ERRNO(ep),
					DR_GET_ERRSTR(ep));
		SFDR_SET_IOCTL_ERR(&sbp->sb_lastop.l_psmerr,
					SFDR_GET_ERR(sep),
					SFDR_GET_ERRSTR(sep));
	}

	/*
	 * If there is an outstanding error, then sb_lastop
	 * will be set.
	 */
	if (SFDR_GET_ERR(sep) || DR_GET_ERRNO(ep))
		errset = sfdr_dev2devset(sbp->sb_lastop.l_dev);
	else
		errset = 0;

	/*
	 * If the device in error is the one coming
	 * through here, then we need to "cancel"
	 * its operation.
	 */
	if (errset && DEVSET_IN_SET(errset, nodetype, unit)) {
		(void) sfdr_cancel_devset(hp, DEVSET(nodetype, unit));
	} else {
		/*
		 * Transfer the device which just completed its release
		 * to the UNREFERENCED state.
		 */
		switch (nodetype) {
		case DR_NT_MEM:
			sfdr_release_mem_done(hp, unit);
			break;

		default:
			(void) sfdr_release_dev_done(hp, nodetype, unit);
			break;
		}
	}

	/*
	 * If the entire board was released and all components
	 * unreferenced then transfer it to the UNREFERENCED state.
	 */
	if ((SFDR_BOARD_STATE(sbp) == SFDR_STATE_RELEASE) &&
		(SFDR_DEVS_RELEASED(sbp) == SFDR_DEVS_UNREFERENCED(sbp))) {
		SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_UNREFERENCED);
	}

	SFDR_HANDLE_REF_DECR(HD2MACHHD(hp));

	/*
	 * Return with board lock & global rwlock held.
	 */
	return (B_TRUE);
}

/*ARGSUSED*/
static dr_devlist_t *
sfdr_get_detach_devlist(dr_handle_t *hp, int32_t *devnump, int32_t pass)
{
	sfdr_board_t	*sbp;
	uint_t		uset;
	sfdr_devset_t	devset;
	dr_devlist_t	*detach_devlist;
	static int	next_pass = 1;
	static fn_t	f = "sfdr_get_detach_devlist";

	PR_ALL("%s (pass = %d)...\n", f, pass);

	sbp = BD2MACHBD(hp->h_bd);
	devset = HD2MACHHD(hp)->sh_devset;

	*devnump = 0;
	detach_devlist = NULL;

	/*
	 * We switch on next_pass for the cases where a board
	 * does not contain a particular type of component.
	 * In these situations we don't want to return NULL
	 * prematurely.  We need to check other devices and
	 * we don't want to check the same type multiple times.
	 * For example, if there were no cpus, then on pass 1
	 * we would drop through and return the memory nodes.
	 * However, on pass 2 we would switch back to the memory
	 * nodes thereby returning them twice!  Using next_pass
	 * forces us down to the end (or next item).
	 */
	if (pass == 1)
		next_pass = 1;

	switch (next_pass) {
	case 1:		/* DR_NT_IO */
		if (DEVSET_IN_SET(devset, DR_NT_IO, DEVSET_ANYUNIT)) {
			uset = DEVSET_GET_UNITSET(devset, DR_NT_IO);

			detach_devlist = sfdr_get_devlist(hp, sbp,
						DR_NT_IO,
						MAX_IO_UNITS_PER_BOARD,
						uset, devnump, 0);

			DEVSET_DEL(devset, DR_NT_IO, DEVSET_ANYUNIT);
			if (!devset || detach_devlist) {
				next_pass = 2;
				return (detach_devlist);
			}
			/*
			 * If the caller is interested in the entire
			 * board, but there isn't any io, then just
			 * fall through to check for the next component.
			 */
		}
		/*FALLTHROUGH*/

	case 2:		/* DR_NT_CPU */
		if (DEVSET_IN_SET(devset, DR_NT_CPU, DEVSET_ANYUNIT)) {
			uset = DEVSET_GET_UNITSET(devset, DR_NT_CPU);

			detach_devlist = sfdr_get_devlist(hp, sbp,
						DR_NT_CPU,
						MAX_CPU_UNITS_PER_BOARD,
						uset, devnump, 0);

			DEVSET_DEL(devset, DR_NT_CPU, DEVSET_ANYUNIT);
			if (!devset || detach_devlist) {
				next_pass = 3;
				return (detach_devlist);
			}
			/*
			 * If the caller is interested in the entire
			 * board, but there aren't any cpus, then
			 * just fall through to next component.
			 */
		}
		/*FALLTHROUGH*/

	case 3:		/* DR_NT_MEM */
		next_pass = -1;
		if (DEVSET_IN_SET(devset, DR_NT_MEM, DEVSET_ANYUNIT)) {
			uset = DEVSET_GET_UNITSET(devset, DR_NT_MEM);

			detach_devlist = sfdr_get_devlist(hp, sbp,
						DR_NT_MEM,
						MAX_MEM_UNITS_PER_BOARD,
						uset, devnump, 0);

			DEVSET_DEL(devset, DR_NT_MEM, DEVSET_ANYUNIT);
			if (!devset || detach_devlist) {
				next_pass = 4;
				return (detach_devlist);
			}
		}
		/*FALLTHROUGH*/

	default:
		*devnump = 0;
		return (NULL);
	}
	/*NOTREACHED*/
}

/*ARGSUSED*/
static int
sfdr_pre_detach_devlist(dr_handle_t *hp, dr_devlist_t *devlist, int32_t devnum)
{
	int		max_units = 0, rv = 0;
	dr_nodetype_t	nodetype;
	static fn_t	f = "sfdr_pre_detach_devlist";

	nodetype = sfdr_get_devtype(hp, devlist[0].dv_nodeid);

	PR_ALL("%s (nt = %s(%d), num = %d)...\n",
		f, nt_str[(int)nodetype], (int)nodetype, devnum);

	switch (nodetype) {
	case DR_NT_CPU:
		max_units = MAX_CPU_UNITS_PER_BOARD;
		rv = sfdr_pre_detach_cpu(hp, devlist, devnum);
		break;

	case DR_NT_MEM:
		max_units = MAX_MEM_UNITS_PER_BOARD;
		rv = sfdr_pre_detach_mem(hp, devlist, devnum);
		break;

	case DR_NT_IO:
		max_units = MAX_IO_UNITS_PER_BOARD;
		rv = sfdr_pre_detach_io(hp, devlist, devnum);
		break;

	default:
		rv = -1;
		break;
	}

	if (rv && max_units) {
		int	i;
		/*
		 * Need to clean up dynamically allocated devlist
		 * if pre-op is going to fail.
		 */
		for (i = 0; i < max_units; i++) {
			if (DR_ERR_ISALLOC(&devlist[i].dv_error)) {
				DR_FREE_ERR(&devlist[i].dv_error);
			}
		}
		FREESTRUCT(devlist, dr_devlist_t, max_units);
	}

	/*
	 * We want to continue attempting to detach
	 * other components.
	 */
	return (rv ? -1 : 0);
}

/*ARGSUSED*/
static int
sfdr_post_detach_devlist(dr_handle_t *hp, dr_devlist_t *devlist,
			int32_t devnum)
{
	int		i, max_units = 0, rv = 0;
	dr_nodetype_t	nodetype;
	sfdr_board_t	*sbp;
	sfdr_state_t	bstate;
	static fn_t	f = "sfdr_post_detach_devlist";

	sbp = BD2MACHBD(hp->h_bd);
	nodetype = sfdr_get_devtype(hp, devlist[0].dv_nodeid);

	PR_ALL("%s (nt = %s(%d), num = %d)...\n",
		f, nt_str[(int)nodetype], (int)nodetype, devnum);

	/*
	 * Need to free up devlist[] created earlier in
	 * sfdr_get_detach_devlist().
	 */
	switch (nodetype) {
	case DR_NT_CPU:
		max_units = MAX_CPU_UNITS_PER_BOARD;
		rv = sfdr_post_detach_cpu(hp, devlist, devnum);
		break;

	case DR_NT_MEM:
		max_units = MAX_MEM_UNITS_PER_BOARD;
		rv = sfdr_post_detach_mem(hp, devlist, devnum);
		break;

	case DR_NT_IO:
		max_units = MAX_IO_UNITS_PER_BOARD;
		rv = sfdr_post_detach_io(hp, devlist, devnum);
		break;

	default:
		rv = -1;
		break;
	}

	for (i = 0; i < devnum; i++) {
		int		unit, achk;
		dr_error_t	*ep;

		ep = &devlist[i].dv_error;

		if (DR_GET_ERRNO(ep)) {
			dr_error_t	*hep = DR_HD2ERR(hp);

			if (DR_GET_ERRNO(hep) == 0) {
				DR_SET_ERRNO(hep, DR_GET_ERRNO(ep));
				DR_SET_ERRSTR(hep, DR_GET_ERRSTR(ep));
			}
			PR_ALL("%s: PIM ERROR (%d, %s)\n",
				f, DR_GET_ERRNO(ep), DR_GET_ERRSTR(ep));
		}

		unit = sfdr_get_unit(devlist[i].dv_nodeid, nodetype);
		if (unit == -1)
			continue;

		achk = sfdr_check_unit_attached(devlist[i].dv_nodeid,
						nodetype);
		if (achk >= 0) {
			/*
			 * Device is still attached probably due
			 * to an error.  Need to keep track of it.
			 */
			PR_ALL("%s: ERROR (nt=%s, b=%d, u=%d) not detached\n",
				f, nt_str[(int)nodetype], sbp->sb_num, i);
			continue;
		}

		SFDR_DEV_CLR_ATTACHED(sbp, nodetype, unit);
		SFDR_DEV_CLR_RELEASED(sbp, nodetype, unit);
		SFDR_DEV_CLR_UNREFERENCED(sbp, nodetype, unit);
		SFDR_DEVICE_TRANSITION(sbp, nodetype, unit,
						SFDR_STATE_UNCONFIGURED);
	}

	bstate = SFDR_BOARD_STATE(sbp);
	if (bstate != SFDR_STATE_UNCONFIGURED) {
		if (SFDR_DEVS_PRESENT(sbp) == SFDR_DEVS_UNATTACHED(sbp)) {
			/*
			 * All devices are finally detached.
			 */
			SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_UNCONFIGURED);
		} else if ((SFDR_BOARD_STATE(sbp) != SFDR_STATE_PARTIAL) &&
				SFDR_DEVS_ATTACHED(sbp)) {
			/*
			 * Some devices remain attached.
			 */
			SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_PARTIAL);
		}
	}

	if (rv) {
		PR_ALL("%s: ERROR (pim=%d, psm=%d) during DETACH\n",
			f, DR_GET_ERRNO(DR_HD2ERR(hp)),
			SFDR_GET_ERR(HD2MACHERR(hp)));
	}

	if (max_units && devlist) {
		int	i;

		for (i = 0; i < max_units; i++) {
			if (DR_ERR_ISALLOC(&devlist[i].dv_error)) {
				DR_FREE_ERR(&devlist[i].dv_error);
			}
		}
		FREESTRUCT(devlist, dr_devlist_t, max_units);
	}

	return (DR_GET_ERRNO(DR_HD2ERR(hp)) ? -1 : 0);
}

/*ARGSUSED*/
static int
sfdr_pre_attach_io(dr_handle_t *hp, dr_devlist_t devlist[], int devnum)
{
	int		d;
	int		unit;
	sfdr_board_t	*sbp;
	dnode_t		nodeid;

	sbp = BD2MACHBD(hp->h_bd);

	for (d = 0; d < devnum; d++) {
		if ((nodeid = devlist[d].dv_nodeid) == (dnode_t)0)
			continue;

		if ((unit = sfdr_get_unit(nodeid, DR_NT_IO)) < 0)
			continue;

		cmn_err(CE_CONT,
			"DR: OS attach io-unit (%d.%d)\n",
			sbp->sb_num, unit);
	}

	return (0);
}

/*ARGSUSED*/
static int
sfdr_pre_detach_io(dr_handle_t *hp, dr_devlist_t *devlist, int devnum)
{
	int		d;
	int		unit;
	sfdr_board_t	*sbp;
	dnode_t		nodeid;
	static fn_t	f = "sfdr_pre_detach_io";

	PR_IO("%s...\n", f);

	if (devnum <= 0)
		return (-1);

	/* fail if any I/O devices are referenced */
	if (sfdr_check_io_refs(hp, devlist, devnum) > 0) {
		PR_IO("%s: failed - I/O devices ref'd\n", f);
		return (-1);
	}

	sbp = BD2MACHBD(hp->h_bd);

	for (d = 0; d < devnum; d++) {
		if ((nodeid = devlist[d].dv_nodeid) == (dnode_t)0)
			continue;

		if ((unit = sfdr_get_unit(nodeid, DR_NT_IO)) < 0)
			continue;

		cmn_err(CE_CONT,
			"DR: OS detach io-unit (%d.%d)\n",
			sbp->sb_num, unit);
	}

	return (0);
}

/*ARGSUSED*/
static int
sfdr_post_detach_io(dr_handle_t *hp, dr_devlist_t *devlist, int devnum)
{
	return (0);
}

/*
 * Return the unit number of the respective nodeid if
 * it's found to be attached.
 */
/*ARGSUSED*/
static int
sfdr_check_unit_attached(dnode_t nodeid, dr_nodetype_t nodetype)
{
	int		unit, board;
	processorid_t	cpuid;
	uint64_t	basepa, endpa;
	struct memlist	*ml;
	extern struct memlist	*phys_install;
	static fn_t	f = "sfdr_check_unit_attached";

	if (nodeid == (dnode_t)0) {
		PR_ALL("%s: ERROR, null nodeid for nodetype %s(%d)\n",
			f, nt_str[(int)nodetype]);
		return (-1);
	}

	if ((unit = sfdr_get_unit(nodeid, nodetype)) < 0)
		return (-1);

	if ((board = sfdr_get_board(nodeid)) < 0)
		return (-1);

	switch (nodetype) {
	case DR_NT_CPU:
		cpuid = (board * MAX_CPU_UNITS_PER_BOARD) + unit;
		mutex_enter(&cpu_lock);
		if (cpu_get(cpuid) == NULL)
			unit = -1;
		mutex_exit(&cpu_lock);
		break;

	case DR_NT_MEM:
		if (sfhw_get_base_physaddr(nodeid, &basepa) != 0) {
			unit = -1;
			break;
		}
		/*
		 * basepa may not be on a alignment boundary, make it so.
		 */
		endpa = mc_get_mem_alignment();
		basepa &= ~(endpa - 1);
		endpa += basepa;
		/*
		 * Check if base address is in phys_install.
		 */
		memlist_read_lock();
		for (ml = phys_install; ml; ml = ml->next)
			if ((endpa <= ml->address) ||
					(basepa >= (ml->address + ml->size)))
				continue;
			else
				break;
		memlist_read_unlock();
		if (ml == NULL)
			unit = -1;
		break;

	case DR_NT_IO:
		ddi_walk_devs(ddi_root_node(), sfdr_check_io_attached,
				(void *)&nodeid);
		if (nodeid != (dnode_t)0)
			unit = -1;
		break;

	default:
		PR_ALL("%s: unexpected nodetype(%d) for nodeid 0x%x\n",
			f, (int)nodetype, (uint_t)nodeid);
		unit = -1;
		break;
	}

	return (unit);
}

/*
 * Locate the memory handle stored in the board represented
 * by the DR handle, which is being used to operate on the
 * memory represented by nodeid.  It's important to distinguish
 * this since the given nodeid may _not_ be the nodeid representing
 * the memory that's actually going away, e.g. during a copy-rename.
 * However, the given (source) nodeid does maintain the memhandle
 * that represents the actually memory (target) nodeid that's going
 * away.
 */
/*ARGSUSED*/
static int
sfdr_get_memhandle(dr_handle_t *hp, dnode_t nodeid, memhandle_t *mhp)
{
	int		unit;
	sfdr_board_t	*sbp;
	dnode_t		*devlist;
	static fn_t	f = "sfdr_get_memhandle";

	PR_MEM("%s...\n", f);

	sbp = BD2MACHBD(hp->h_bd);
	devlist = sbp->sb_devlist[NIX(DR_NT_MEM)];

	for (unit = 0; unit < MAX_MEM_UNITS_PER_BOARD; unit++) {
		sfdr_mem_unit_t		*mp;

		mp = SFDR_GET_BOARD_MEMUNIT(sbp, unit);

		if ((devlist[unit] == nodeid) ||
				(mp->sbm_target_nodeid == nodeid)) {
			*mhp = mp->sbm_memhandle;
			return (0);
		}
	}

	return (-1);
}

/*
 * If detaching node contains memory that is "non-permanent"
 * then the memory adr's are simply cleared.  If the memory
 * is non-relocatable, then do a copy-rename.
 */
/*ARGSUSED*/
static int
sfdr_detach_mem(dr_handle_t *hp, dnode_t nodeid)
{
	int			rv, s_unit, t_unit;
	sfdr_board_t		*s_sbp, *t_sbp;
	sfdr_error_t		*sep;
	sfdr_mem_unit_t		*s_mp, *t_mp;
	memdelstat_t		mdst;
	struct memlist		*s_ml, *t_ml;
	char			errstr[80];
	extern void		cpu_flush_ecache();
	static fn_t		f = "sfdr_detach_mem";

	PR_MEM("%s...\n", f);

	sep = HD2MACHERR(hp);
	s_sbp = BD2MACHBD(hp->h_bd);
	s_unit = sfdr_get_unit(nodeid, DR_NT_MEM);
	s_mp = SFDR_GET_BOARD_MEMUNIT(s_sbp, s_unit);

	if (s_mp->sbm_target_nodeid == (dnode_t)0) {
		sprintf(errstr,
			"%s: protocol error: mem-unit (%d.%d) not released",
			f, s_sbp->sb_num, s_unit);
		cmn_err(CE_WARN, "sfdr:%s", errstr);
		SFDR_SET_ERR_STR(sep, SFDR_ERR_PROTO, errstr);
		return (-1);
	}

	t_sbp = s_sbp + (s_mp->sbm_target_board - s_sbp->sb_num);
	t_unit = sfdr_get_unit(s_mp->sbm_target_nodeid, DR_NT_MEM);
	ASSERT(t_sbp->sb_num == s_mp->sbm_target_board);
	t_mp = SFDR_GET_BOARD_MEMUNIT(t_sbp, t_unit);

	if ((t_mp->sbm_flags & SFDR_MFLAG_TARGET) == 0) {
		sprintf(errstr,
			"%s: protocol error: mem-unit (%d.%d) missing "
			"target indicator", f, t_sbp->sb_num, t_unit);
		PR_MEM("%s\n", f, errstr);
		SFDR_SET_ERR_STR(sep, SFDR_ERR_PROTO, errstr);
		return (-1);
	}

	/*
	 * Note that if a copy-rename is occurring then
	 * the memory state of the local board really represents
	 * that of the target board since we're really detaching
	 * the memory of the target board.
	 */
	if (SFDR_DEVICE_STATE(s_sbp, DR_NT_MEM, s_unit) !=
						SFDR_STATE_UNREFERENCED) {
		PR_MEM("%s: mem-unit %d.%d state %s(%d) != expected %s(%d)\n",
			f, s_sbp->sb_num, s_unit,
			state_str[SFDR_DEVICE_STATE(s_sbp, DR_NT_MEM, s_unit)],
			SFDR_DEVICE_STATE(s_sbp, DR_NT_MEM, s_unit),
			state_str[SFDR_STATE_UNREFERENCED],
			SFDR_STATE_UNREFERENCED);
		sprintf(errstr,
			"%s: invalid state transition for mem-unit (%d.%d)",
			f, s_sbp->sb_num, s_unit);
		SFDR_SET_ERR_STR(sep, SFDR_ERR_STATE, errstr);
		return (-1);
	}

	/*
	 * Note that we're interested in the delete status
	 * of the target which is not necessarily the same
	 * as the source memory node.  The source's memhandle
	 * will always represent the memory that's ultimately
	 * going away.
	 */
	rv = kphysm_del_status(s_mp->sbm_memhandle, &mdst);
	PR_MEM("%s: kphysm_del_status = %d\n", f, rv);

	switch (rv) {
	case KPHYSM_OK:
	case KPHYSM_ENOTRUNNING:
		PR_MEM("%s: kphysm_del_status = OK(%d) or NOTRUNNING(%d)\n",
			f, KPHYSM_OK, KPHYSM_ENOTRUNNING);
		break;

	default:
		if (s_mp->sbm_flags & SFDR_MFLAG_RELDONE) {
			PR_MEM("%s: memory release DONE\n", f);
			break;
		}
		sprintf(errstr, "%s: kphysm_del_status", f);
		SFDR_SET_ERR_STR(sep, SFDR_ERR_PROTO, errstr);
		return (-1);
	}

	if (((rv == KPHYSM_OK) && (mdst.managed != mdst.collected)) ||
			!(s_mp->sbm_flags & SFDR_MFLAG_RELDONE)) {
		sprintf(errstr, "%s: mem-unit (%d.%d) release in-progress",
			f, t_sbp->sb_num, t_unit);
		cmn_err(CE_WARN, "sfdr:%s", errstr);
		SFDR_SET_ERR_STR(sep, SFDR_ERR_BUSY, errstr);
		return (-1);
	}

	ASSERT(t_mp->sbm_mlist);
	ASSERT(s_mp->sbm_mlist);

	if ((t_ml = t_mp->sbm_mlist) == NULL) {
		/*
		 * No memlist?!
		 */
		sprintf(errstr,
			"%s: internal error: no memlist for board %d",
			f, t_sbp->sb_num);
		cmn_err(CE_WARN, "sfdr:%s", errstr);
		SFDR_SET_ERR_STR(sep, SFDR_ERR_INTERNAL, errstr);
		return (-1);
	}

	affinity_set(CPU_CURRENT);

	/*
	 * Scrub the target memory.  This will cause all cachelines
	 * referencing the memory to only be in the local cpu's
	 * ecache.
	 */
	sfdr_memscrub(t_ml);

	/*
	 * Now let's flush our ecache thereby removing all references
	 * to the target (detaching) memory from all ecache's in
	 * system.
	 */
	cpu_flush_ecache();
	/*
	 * Delay 100 usec out of paranoia to insure everything
	 * (hardware queues) has drained before we start reprogramming
	 * the hardware.
	 */
	DELAY(100);

	/*
	 * If no copy-rename was needed, then we're done!
	 */
	if ((s_mp->sbm_flags & SFDR_MFLAG_MEMMOVE) == 0) {
		/*
		 * Reprogram interconnect hardware and disable
		 * memory controllers for memory node that's going away.
		 */
		ASSERT(s_sbp == t_sbp);
		if (sfhw_deprogram_memctrl(nodeid, t_sbp->sb_num) < 0) {
			sprintf(errstr,
				"%s: failed to deprogram hardware for "
				"board %d",
				f, t_sbp->sb_num);
			cmn_err(CE_WARN, "sfdr:%s\n", errstr);
			affinity_clear();
			SFDR_SET_ERR_STR(sep, SFDR_ERR_HW_INTERCONNECT,
					errstr);
			return (-1);
		} else {
			affinity_clear();
			PR_MEM("%s: COMPLETED memory DETACH (board %d)\n",
				f, t_sbp->sb_num);
			return (0);
		}
	}

	if ((s_ml = s_mp->sbm_mlist) == NULL) {
		/*
		 * There should be a memlist by the time
		 * we reach here.
		 */
		sprintf(errstr,
			"%s: internal error: no memlist for board %d",
			f, s_sbp->sb_num);
		cmn_err(CE_WARN, "sfdr:%s", errstr);
		affinity_clear();
		SFDR_SET_ERR_STR(sep, SFDR_ERR_INTERNAL, errstr);
		return (-1);
	}

	if ((rv = sfdr_move_memory(hp, s_ml)) != 0) {
		if (SFDR_GET_ERR(sep) == SFDR_ERR_UNSAFE) {
			sprintf(errstr,
				"%s: UNSAFE device(s): "
				"cannot quiesce OS", f);
			SFDR_SET_ERR_STR(sep, SFDR_ERR_UNSAFE, errstr);
		} else {
			sprintf(errstr,
				"%s: failed memory move for board %d",
				f, s_sbp->sb_num);
			SFDR_SET_ERR_STR(sep, SFDR_ERR_INTERNAL, errstr);
		}
	}

	affinity_clear();

	PR_MEM("%s: %s memory COPY-RENAME (board %d -> %d)\n",
		f, rv ? "FAILED" : "COMPLETED",
		s_sbp->sb_num, t_sbp->sb_num);

	return (rv);
}

/*ARGSUSED*/
static int
sfdr_cpu_status(dr_handle_t *hp, sfdr_devset_t devset, sfdr_dev_stat_t *dsp)
{
	int		c, cix;
	int		bd;
	sfdr_board_t	*sbp;
	sfdr_cpu_stat_t	*csp;

	sbp = BD2MACHBD(hp->h_bd);
	bd = sbp->sb_num;

	/*
	 * Only look for requested devices that are actually present.
	 */
	devset &= SFDR_DEVS_PRESENT(sbp);
	for (c = cix = 0; c < MAX_CPU_UNITS_PER_BOARD; c++) {
		processorid_t	id;
		dnode_t		nodeid;

		if (DEVSET_IN_SET(devset, DR_NT_CPU, c) == 0)
			continue;

		nodeid = sbp->sb_devlist[NIX(DR_NT_CPU)][c];
		if (nodeid == (dnode_t)0)
			continue;

		csp = &dsp->d_cpu;

		bzero((caddr_t)csp, sizeof (*csp));
		csp->cs_type = DR_NT_CPU;
		csp->cs_dstate = SFDR_DEVICE_STATE(sbp, DR_NT_CPU, c);

		id = (bd * MAX_CPU_UNITS_PER_BOARD) + c;
		csp->cs_cpuid = id;
		csp->cs_isbootproc = (SIGBCPU->cpu_id == id) ? 1 : 0;

		cix++;
		dsp++;
	}

	return (cix);
}

/*
 * NOTE: This routine is only partially smart about multiple
 *	 mem-units.  Need to make mem-status structure smart
 *	 about them also.
 */
/*ARGSUSED*/
static int
sfdr_mem_status(dr_handle_t *hp, sfdr_devset_t devset, sfdr_dev_stat_t *dsp)
{
	int		m, mix, t_unit;
	memdelstat_t	mdst;
	memquery_t	mq;
	sfdr_board_t	*sbp, *t_sbp = NULL;
	sfdr_mem_unit_t	*mp, *t_mp;
	sfdr_mem_stat_t	*msp;
	extern int	kcage_on;
	static fn_t	f = "sfdr_mem_status";

	sbp = BD2MACHBD(hp->h_bd);

	devset &= SFDR_DEVS_PRESENT(sbp);

	for (m = mix = 0; m < MAX_MEM_UNITS_PER_BOARD; m++) {
		int	rv;
		dnode_t	nodeid;

		if (DEVSET_IN_SET(devset, DR_NT_MEM, m) == 0)
			continue;

		nodeid = sbp->sb_devlist[NIX(DR_NT_MEM)][m];
		if (nodeid == (dnode_t)0)
			continue;

		mp = t_mp = SFDR_GET_BOARD_MEMUNIT(sbp, m);
		if ((mp->sbm_target_nodeid != (dnode_t)0) &&
			(mp->sbm_target_board != sbp->sb_num)) {
			/*
			 * A target board exists indicating a
			 * delete is in progress.  In this
			 * case we're interested in the mem
			 * status of the target, which is
			 * not necessarily the same as the
			 * mem-unit which we're current
			 * querying for status.
			 */
			t_unit = sfdr_get_unit(mp->sbm_target_nodeid,
						DR_NT_MEM);
			ASSERT(mp->sbm_target_board >= 0);
			t_sbp = sbp + (mp->sbm_target_board - sbp->sb_num);
			ASSERT(t_sbp->sb_num == mp->sbm_target_board);

			t_mp = SFDR_GET_BOARD_MEMUNIT(t_sbp, t_unit);
		}

		msp = &dsp->d_mem;

		bzero((caddr_t)msp, sizeof (*msp));
		msp->ms_type = DR_NT_MEM;
		msp->ms_dstate = SFDR_DEVICE_STATE(sbp, DR_NT_MEM, m);
		msp->ms_cage_enabled = kcage_on;

		/*
		 * XXX - need to fill in ms_pageslost.
		 */
		msp->ms_basepfn = (uint_t)t_mp->sbm_basepfn;

		rv = kphysm_del_status(t_mp->sbm_memhandle, &mdst);

		if (rv != KPHYSM_OK) {
			PR_MEM("%s: kphysm_del_status() = %d\n", f, rv);
			msp->ms_totpages += (uint_t)t_mp->sbm_npages;
			/*
			 * If we're UNREFERENCED or UNCONFIGURED,
			 * then the number of detached pages is
			 * however many pages are on the board.
			 * I.e. detached = not in use by OS.
			 */
			switch (msp->ms_dstate) {
			case SFDR_STATE_UNREFERENCED:
			case SFDR_STATE_UNCONFIGURED:
				msp->ms_detpages = msp->ms_totpages;
				break;

			default:
				break;
			}
		} else {
			msp->ms_totpages += (uint_t)mdst.phys_pages;
			/*
			 * Any pages above managed is "free", i.e. it's
			 * collected.
			 */
			msp->ms_detpages += (uint_t)(mdst.collected +
							mdst.phys_pages -
							mdst.managed);
		}

		rv = kphysm_del_span_query(mp->sbm_basepfn,
						mp->sbm_npages, &mq);
		if (rv == KPHYSM_OK) {
			msp->ms_mananged_pages = (uint_t)mq.managed;
			msp->ms_noreloc_pages = (uint_t)mq.nonrelocatable;
			msp->ms_noreloc_first = (uint_t)mq.first_nonrelocatable;
			msp->ms_noreloc_last = (uint_t)mq.last_nonrelocatable;
		} else {
			PR_MEM("%s: kphysm_del_span_query() = %d\n",
				f, rv);
		}

		mix++;
		dsp++;
	}

	return (mix);
}

/*ARGSUSED*/
static int
sfdr_io_status(dr_handle_t *hp, sfdr_devset_t devset, sfdr_dev_stat_t *dsp)
{
	int		i, ix;
	sfdr_board_t	*sbp;
	sfdr_io_stat_t	*isp;
	sfdr_error_t	*sep;
	static fn_t	f = "sfdr_io_status";

	sbp = BD2MACHBD(hp->h_bd);

	/*
	 * Only look for requested devices that are actually present.
	 */
	devset &= SFDR_DEVS_PRESENT(sbp);

	sep = HD2MACHERR(hp);

	for (i = ix = 0; i < MAX_IO_UNITS_PER_BOARD; i++) {
		dnode_t		nodeid;
		dev_info_t	*dip;

		if (DEVSET_IN_SET(devset, DR_NT_IO, i) == 0)
			continue;

		nodeid = sbp->sb_devlist[NIX(DR_NT_IO)][i];
		if (nodeid == (dnode_t)0)
			continue;

		isp = &dsp->d_io;

		bzero((caddr_t)isp, sizeof (*isp));
		isp->is_type = DR_NT_IO;
		isp->is_dstate = SFDR_DEVICE_STATE(sbp, DR_NT_IO, i);

		dip = e_ddi_nodeid_to_dip(ddi_root_node(), nodeid);
		if (dip != NULL) {
			int	j, idx, refcount = 0;
			/*
			 * We use a dummy handle in which to collect
			 * the major numbers of unsafe devices.
			 */
			sfdr_check_devices(dip, &refcount, hp);

			isp->is_referenced = (refcount == 0) ? 0 : 1;

			if (SFDR_GET_ERR(sep) == SFDR_ERR_UNSAFE) {
				idx = SFDR_ERR_INT_IDX(sep);
				for (j = 0; (j < idx) &&
						(j < SFDR_MAX_UNSAFE); j++) {
					isp->is_unsafe_list[j] =
						SFDR_GET_ERR_INT(sep, j+1);
				}
				isp->is_unsafe_count = idx;
			} else {
				isp->is_unsafe_count = 0;
			}

			PR_IO("%s: board %d io.%d, refcount = %d, "
				"unsafecount = %d\n",
				f, sbp->sb_num, i, refcount,
				isp->is_unsafe_count);
			/*
			 * Reset error field since we don't care about
			 * errors at this level.  The unsafe devices
			 * will be reported in the structure.
			 */
			SFDR_SET_ERR(sep, SFDR_ERR_NOERROR);
			SFDR_SET_ERRSTR(sep, "");
		}
		ix++;
		dsp++;
	}

	return (ix);
}

/*ARGSUSED*/
static int
sfdr_cancel(dr_handle_t *hp)
{
	sfdr_board_t	*sbp = BD2MACHBD(hp->h_bd);
	sfdr_devset_t	devset;

	/*
	 * Only devices which have been "released" are
	 * subject to cancellation.
	 */
	devset = HD2MACHHD(hp)->sh_devset & SFDR_DEVS_RELEASED(sbp);

	return (sfdr_cancel_devset(hp, devset));
}

/*ARGSUSED*/
static int
sfdr_cancel_devset(dr_handle_t *hp, sfdr_devset_t devset)
{
	int		i;
	sfdr_board_t	*sbp = BD2MACHBD(hp->h_bd);
	static fn_t	f = "sfdr_cancel_devset";

	PR_ALL("%s...\n", f);

	/*
	 * Nothing to do for CPUs or IO other than change back
	 * their state.
	 */
	for (i = 0; i < MAX_CPU_UNITS_PER_BOARD; i++) {
		if (!DEVSET_IN_SET(devset, DR_NT_CPU, i))
			continue;
		if (sfdr_cancel_cpu(hp, i) == 0) {
			SFDR_DEVICE_TRANSITION(sbp, DR_NT_CPU, i,
						SFDR_STATE_CONFIGURED);
		} else {
			SFDR_DEVICE_TRANSITION(sbp, DR_NT_CPU, i,
						SFDR_STATE_FATAL);
		}
	}
	for (i = 0; i < MAX_IO_UNITS_PER_BOARD; i++) {
		if (!DEVSET_IN_SET(devset, DR_NT_IO, i))
			continue;
		SFDR_DEVICE_TRANSITION(sbp, DR_NT_IO, i,
					SFDR_STATE_CONFIGURED);
	}
	for (i = 0; i < MAX_MEM_UNITS_PER_BOARD; i++) {
		if (!DEVSET_IN_SET(devset, DR_NT_MEM, i))
			continue;
		if (sfdr_cancel_mem(hp, i) == 0) {
			SFDR_DEVICE_TRANSITION(sbp, DR_NT_MEM, i,
						SFDR_STATE_CONFIGURED);
		} else {
			SFDR_DEVICE_TRANSITION(sbp, DR_NT_MEM, i,
						SFDR_STATE_FATAL);
		}
	}

	PR_ALL("%s: unreleasing devset (0x%x)\n", f, (uint_t)devset);

	SFDR_DEVS_CANCEL(sbp, devset);

	if (SFDR_DEVS_RELEASED(sbp) == 0) {
		sfdr_state_t	new_state;
		/*
		 * If the board no longer has any released devices
		 * than transfer it back to the CONFIG/PARTIAL state.
		 */
		if (SFDR_DEVS_ATTACHED(sbp) == SFDR_DEVS_PRESENT(sbp))
			new_state = SFDR_STATE_CONFIGURED;
		else
			new_state = SFDR_STATE_PARTIAL;
		if (SFDR_BOARD_STATE(sbp) != new_state) {
			SFDR_BOARD_TRANSITION(sbp, new_state);
		}
	}

	return (0);
}

/*ARGSUSED*/
static int
sfdr_status(dr_handle_t *hp)
{
	int		nstat, mode;
	sfdr_handle_t	*shp;
	sfdr_devset_t	devset = 0;
	sfdr_stat_t	dstat;
	sfdr_dev_stat_t	*devstatp = NULL;
	sfdr_board_t	*sbp;
	static fn_t	f = "sfdr_status";

	PR_ALL("%s...\n", f);

	mode = hp->h_mode;
	shp = HD2MACHHD(hp);
	devset = shp->sh_devset;
	sbp = BD2MACHBD(hp->h_bd);

	bzero((caddr_t)&dstat, sizeof (dstat));

	dstat.s_board = sbp->sb_num;
	dstat.s_bstate = SFDR_BOARD_STATE(sbp);
	dstat.s_pbstate = SFDR_BOARD_PSTATE(sbp);

	if (SFDR_BOARD_ERROR_PENDING(sbp)) {
		/*
		 * There's an outstanding error on this board.
		 * Copy it out.
		 */
		bcopy((caddr_t)&sbp->sb_lastop,
			(caddr_t)&dstat.s_lastop,
			sizeof (sbp->sb_lastop));
		bzero((caddr_t)&sbp->sb_lastop, sizeof (sbp->sb_lastop));

		PR_ALL("%s: found outstanding error "
			"(cmd = %s, dev = 0x%x)...\n",
			f, SFDR_CMD_STR(dstat.s_lastop.l_cmd),
			(uint_t)dstat.s_lastop.l_dev);
	}
	dstat.s_nstat = nstat = 0;

	devset &= SFDR_DEVS_PRESENT(sbp);
	if (devset == 0) {
		/*
		 * No device chosen.
		 */
		PR_ALL("%s: no device present\n", f);
		return (0);
	}

	devstatp = &dstat.s_stat[0];

	if (DEVSET_IN_SET(devset, DR_NT_CPU, DEVSET_ANYUNIT))
		if ((nstat = sfdr_cpu_status(hp, devset, devstatp)) > 0) {
			dstat.s_nstat += nstat;
			devstatp += nstat;
		}

	if (DEVSET_IN_SET(devset, DR_NT_MEM, DEVSET_ANYUNIT))
		if ((nstat = sfdr_mem_status(hp, devset, devstatp)) > 0) {
			dstat.s_nstat += nstat;
			devstatp += nstat;
		}

	if (DEVSET_IN_SET(devset, DR_NT_IO, DEVSET_ANYUNIT))
		if ((nstat = sfdr_io_status(hp, devset, devstatp)) > 0) {
			dstat.s_nstat += nstat;
			devstatp += nstat;
		}

#ifdef _MULTI_DATAMODEL
	if (ddi_model_convert_from(mode & FMODELS) == DDI_MODEL_ILP32) {
		int		i, j;
		sfdr_stat32_t	dstat32;

		dstat32.s_board = (int32_t)dstat.s_board;
		dstat32.s_bstate = (int32_t)dstat.s_bstate;
		dstat32.s_pbstate = (int32_t)dstat.s_pbstate;

		dstat32.s_lastop.l_cmd = (int32_t)dstat.s_lastop.l_cmd;
		dstat32.s_lastop.l_dev = (uint32_t)dstat.s_lastop.l_dev;
		bcopy((caddr_t)&dstat.s_lastop.l_pimerr,
			(caddr_t)&dstat32.s_lastop.l_pimerr,
			sizeof (dstat32.s_lastop.l_pimerr));
		bcopy((caddr_t)&dstat.s_lastop.l_psmerr,
			(caddr_t)&dstat32.s_lastop.l_psmerr,
			sizeof (dstat32.s_lastop.l_psmerr));

		dstat32.s_nstat = (int32_t)dstat.s_nstat;
		for (i = 0; i < dstat.s_nstat; i++) {
			sfdr_cpu_stat32_t	*csp32;
			sfdr_mem_stat32_t	*msp32;
			sfdr_io_stat32_t	*isp32;
			sfdr_cpu_stat_t		*csp;
			sfdr_mem_stat_t		*msp;
			sfdr_io_stat_t		*isp;

			switch (dstat.s_stat[i].d_common.c_type) {
			case DR_NT_CPU:
				csp = &dstat.s_stat[i].d_cpu;
				csp32 = &dstat32.s_stat[i].d_cpu;

				csp32->cs_type = (int32_t)csp->cs_type;
				csp32->cs_dstate = (int32_t)csp->cs_dstate;
				csp32->cs_isbootproc =
						(int32_t)csp->cs_isbootproc;
				csp32->cs_cpuid = (int32_t)csp->cs_cpuid;
				break;

			case DR_NT_MEM:
				msp = &dstat.s_stat[i].d_mem;
				msp32 = &dstat32.s_stat[i].d_mem;

				msp32->ms_type = (int32_t)msp->ms_type;
				msp32->ms_dstate = (int32_t)msp->ms_dstate;
				msp32->ms_basepfn =
						(uint32_t)msp->ms_basepfn;
				msp32->ms_totpages =
						(uint32_t)msp->ms_totpages;
				msp32->ms_detpages =
						(uint32_t)msp->ms_detpages;
				msp32->ms_pageslost =
						(int32_t)msp->ms_pageslost;
				msp32->ms_mananged_pages =
					(uint32_t)msp->ms_mananged_pages;
				msp32->ms_noreloc_pages =
					(uint32_t)msp->ms_noreloc_pages;
				msp32->ms_noreloc_first =
					(uint32_t)msp->ms_noreloc_first;
				msp32->ms_noreloc_last =
					(uint32_t)msp->ms_noreloc_last;
				msp32->ms_cage_enabled =
					(int32_t)msp->ms_cage_enabled;
				break;

			case DR_NT_IO:
				isp = &dstat.s_stat[i].d_io;
				isp32 = &dstat32.s_stat[i].d_io;

				isp32->is_type = (int32_t)isp->is_type;
				isp32->is_dstate = (int32_t)isp->is_dstate;
				isp32->is_unsafe_count =
					(int32_t)isp->is_unsafe_count;
				isp32->is_referenced =
						(int32_t)isp->is_referenced;
				for (j = 0; j < SFDR_MAX_UNSAFE; j++)
					isp32->is_unsafe_list[j] =
						(int32_t)isp->is_unsafe_list[j];
				break;

			default:
				cmn_err(CE_PANIC,
					"sfdr:%s: unknown dev type (%d)", f,
					(int)dstat.s_stat[i].d_common.c_type);
				/*NOTREACHED*/
			}
		}
		if (ddi_copyout((void *)&dstat32, shp->sh_arg,
				sizeof (dstat32), mode) != 0) {
			cmn_err(CE_WARN,
				"sfdr:%s: failed to copyout status "
				"for board %d", f, sbp->sb_num);
			DR_SET_ERRNO(DR_HD2ERR(hp), EFAULT);
			return (-1);
		}
	} else
#endif /* _MULTI_DATAMODEL */
	if (ddi_copyout((void *)&dstat, shp->sh_arg,
			sizeof (dstat), mode) != 0) {
		cmn_err(CE_WARN,
			"sfdr:%s: failed to copyout status for board %d",
			f, sbp->sb_num);
		DR_SET_ERRNO(DR_HD2ERR(hp), EFAULT);
		return (-1);
	}

	return (0);
}

/*ARGSUSED*/
dr_nodetype_t
sfdr_get_devtype(dr_handle_t *hp, dnode_t nodeid)
{
	sfdr_board_t	*sbp = hp ? BD2MACHBD(hp->h_bd) : NULL;
	sfdr_state_t	bstate;
	dnode_t		*devlist;
	int		i, n;
	static char	name[OBP_MAXDRVNAME];
	static fn_t	f = "sfdr_get_devtype";

	PR_ALL("%s...\n", f);

	bstate = sbp ? SFDR_BOARD_STATE(sbp) : SFDR_STATE_EMPTY;
	/*
	 * if the board's connected or configured, search the
	 * devlists.  Otherwise check the device tree
	 */
	switch (bstate) {

	case SFDR_STATE_CONNECTED:
	case SFDR_STATE_CONFIGURED:
	case SFDR_STATE_UNREFERENCED:
	case SFDR_STATE_UNCONFIGURED:
		devlist = sbp->sb_devlist[NIX(DR_NT_CPU)];
		for (n = 0; n < MAX_CPU_UNITS_PER_BOARD; n++)
			if (devlist[n] == nodeid)
				return (DR_NT_CPU);

		devlist = sbp->sb_devlist[NIX(DR_NT_MEM)];
		for (n = 0; n < MAX_MEM_UNITS_PER_BOARD; n++)
			if (devlist[n] == nodeid)
				return (DR_NT_MEM);

		devlist = sbp->sb_devlist[NIX(DR_NT_IO)];
		for (n = 0; n < MAX_IO_UNITS_PER_BOARD; n++)
			if (devlist[n] == nodeid)
				return (DR_NT_IO);
		/*FALLTHROUGH*/

	default:
		(void) prom_getprop(nodeid, OBP_NAME, (caddr_t)name);

		for (i = 0; SFDR_NT(i) != DR_NT_UNKNOWN; i++) {
			if (strcmp(name, SFDR_DEVNAME(i)) != 0)
				continue;
			return (SFDR_NT(i));
		}

		break;
	}
	return (DR_NT_UNKNOWN);
}

/*ARGSUSED*/
processorid_t
sfdr_get_cpuid(dr_handle_t *hp, dnode_t nodeid)
{
	int		upaid;
	char		type[MAX_PROP_LEN];
	static fn_t	f = "sfdr_get_cpuid";

	PR_CPU("%s...\n", f);

	if (prom_getproplen(nodeid, "device_type") < MAX_PROP_LEN)
		(void) prom_getprop(nodeid, "device_type", (caddr_t)type);
	else
		return ((processorid_t)-1);

	if (strcmp(type, "cpu") != 0)
		return ((processorid_t)-1);

	(void) prom_getprop(nodeid, "upa-portid", (caddr_t)&upaid);

	return ((processorid_t)upaid);
}

/*ARGSUSED*/
static int
sfdr_make_nodes(dev_info_t *dip)
{
	int		bd, instance, rv;
	int		minor_num;
	auto char	filename[20];
	static fn_t	f = "sfdr_make_nodes";

	instance = ddi_get_instance(dip);

	PR_ALL("%s (dip = %s@%d)...\n", f, ddi_get_name(dip), instance);

	for (bd = 0; bd < MAX_BOARDS; bd++) {
		(void) sprintf(filename, "slot%d", bd);

		minor_num = DR_MAKE_MINOR(instance, SLOT2DEV(bd));

		rv = ddi_create_minor_node(dip, filename, S_IFCHR,
					minor_num, DDI_NT_ATTACHMENT_POINT,
					NULL);
		if (rv == DDI_FAILURE) {
			cmn_err(CE_WARN,
				"sfdr:%s:%d: failed to create "
				"minor node (%s, 0x%x)",
				f, instance, filename, minor_num);
			return (-1);
		}
	}

	return (0);
}

static int
sfdr_make_dev_nodes(sfdr_board_t *sbp, sfdr_devset_t devset)
{
	int		i, bd, ncpu, nmem, nio, instance, rv;
	int		minor_num;
	dev_info_t	*dip;
	auto char	filename[20];
	static fn_t	f = "sfdr_make_dev_nodes";

	if (sfdr_dev_attachment_points == 0)
		return (0);

	ncpu = nmem = nio = 0;
	bd = sbp->sb_num;
	dip = sbp->sb_dip;
	instance = ddi_get_instance(dip);

	PR_ALL("%s: (dip = %s@%d, devset = 0x%x)...\n",
		f, ddi_get_name(dip), instance, devset);

	/*
	 * Check for cpu devices.
	 */
	for (i = 0; i < MAX_CPU_UNITS_PER_BOARD; i++) {
		if (!DEVSET_IN_SET(devset, DR_NT_CPU, i))
			continue;
		ncpu++;
		(void) sprintf(filename, "slot%d.cpu%d", bd, i);

		minor_num = DR_MAKE_MINOR(instance, CPUUNIT2DEV(bd, i + 1));

		PR_ALL("%s: make_node(%s) [0x%x(%d)]\n",
			f, filename, minor_num, minor_num);

		rv = ddi_create_minor_node(dip, filename, S_IFCHR, minor_num,
					DDI_NT_ATTACHMENT_POINT, NULL);
		if (rv == DDI_FAILURE)
			cmn_err(CE_WARN,
				"sfdr:%s:%d: failed to create minor node "
				"(%s, 0x%x)",
				f, instance, filename, minor_num);
	}
	if (ncpu > 0) {
		/*
		 * If we created at least one cpu unit attachment
		 * point, then we need allcpu.  Entry represents
		 * ALL cpus for a given board.
		 */
		(void) sprintf(filename, "slot%d.allcpu", bd);

		minor_num = DR_MAKE_MINOR(instance, ALLCPU2DEV(bd));

		PR_ALL("%s: make_node(%s) [0x%x(%d)]\n",
			f, filename, minor_num, minor_num);

		rv = ddi_create_minor_node(dip, filename, S_IFCHR, minor_num,
					DDI_NT_ATTACHMENT_POINT, NULL);
		if (rv == DDI_FAILURE)
			cmn_err(CE_WARN,
				"sfdr:%s:%d: failed to create minor node "
				"(%s, 0x%x)",
				f, instance, filename, minor_num);
	}

	/*
	 * Check for mem device.
	 * There is only one on Starfire, but we'll be general about it!
	 */
	for (i = 0; i < MAX_MEM_UNITS_PER_BOARD; i++) {
		if (!DEVSET_IN_SET(devset, DR_NT_MEM, i))
			continue;
		nmem++;
		(void) sprintf(filename, "slot%d.mem%d", bd, i);

		minor_num = DR_MAKE_MINOR(instance, MEMUNIT2DEV(bd, i + 1));

		PR_ALL("%s: make_node(%s) [0x%x(%d)]\n",
			f, filename, minor_num, minor_num);

		rv = ddi_create_minor_node(dip, filename, S_IFCHR, minor_num,
					DDI_NT_ATTACHMENT_POINT, NULL);
		if (rv == DDI_FAILURE)
			cmn_err(CE_WARN,
				"sfdr:%s:%d: failed to create minor node "
				"(%s, 0x%x)",
				f, instance, filename, minor_num);
	}
	if (nmem > 0) {
		/*
		 * Entry to represent ALL mem for a given board.
		 */
		(void) sprintf(filename, "slot%d.allmem", bd);

		minor_num = DR_MAKE_MINOR(instance, ALLMEM2DEV(bd));

		PR_ALL("%s: make_node(%s) [0x%x(%d)]\n",
			f, filename, minor_num, minor_num);

		rv = ddi_create_minor_node(dip, filename, S_IFCHR, minor_num,
					DDI_NT_ATTACHMENT_POINT, NULL);
		if (rv == DDI_FAILURE)
			cmn_err(CE_WARN,
				"sfdr:%s:%d: failed to create minor node "
				"(%s, 0x%x)",
				f, instance, filename, minor_num);
		nmem++;
	}

	/*
	 * Check for io devices.
	 */
	for (i = 0; i < MAX_IO_UNITS_PER_BOARD; i++) {
		if (!DEVSET_IN_SET(devset, DR_NT_IO, i))
			continue;
		nio++;
		(void) sprintf(filename, "slot%d.io%d", bd, i);

		minor_num = DR_MAKE_MINOR(instance, IOUNIT2DEV(bd, i + 1));

		PR_ALL("%s: make_node(%s) [0x%x(%d)]\n",
			f, filename, minor_num, minor_num);

		rv = ddi_create_minor_node(dip, filename, S_IFCHR, minor_num,
					DDI_NT_ATTACHMENT_POINT, NULL);
		if (rv == DDI_FAILURE)
			cmn_err(CE_WARN,
				"sfdr:%s:%d: failed to create minor node "
				"(%s, 0x%x)",
				f, instance, filename, minor_num);
	}
	if (nio > 0) {
		(void) sprintf(filename, "slot%d.allio", bd);

		minor_num = DR_MAKE_MINOR(instance, ALLIO2DEV(bd));

		PR_ALL("%s: make_node(%s) [0x%x(%d)]\n",
			f, filename, minor_num, minor_num);

		rv = ddi_create_minor_node(dip, filename, S_IFCHR, minor_num,
					DDI_NT_ATTACHMENT_POINT, NULL);
		if (rv == DDI_FAILURE)
			cmn_err(CE_WARN,
				"sfdr:%s:%d: failed to create minor node "
				"(%s, 0x%x)",
				f, instance, filename, minor_num);
	}

	/*
	 * Need to announce node's presence.
	 */
	if ((ncpu + nmem + nio) > 0)
		ddi_report_dev(dip);

	return (0);
}

static int
sfdr_remove_dev_nodes(sfdr_board_t *sbp, sfdr_devset_t devset)
{
	int		i, n, bd;
	sfdr_devset_t	devpresent;
	dev_info_t	*dip;
	auto char	filename[20];
	static fn_t	f = "sfdr_remove_dev_nodes";

	if (sfdr_dev_attachment_points == 0)
		return (0);

	devpresent = SFDR_DEVS_PRESENT(sbp);
	dip = sbp->sb_dip;
	bd = sbp->sb_num;

	PR_ALL("%s: (devset = 0x%x)...\n", f, devset);

	/*
	 * Check for cpu devices.
	 */
	devpresent &= ~devset;
	n = 0;
	for (i = 0; i < MAX_CPU_UNITS_PER_BOARD; i++) {
		/*
		 * Check if any cpus left.
		 */
		if (DEVSET_IN_SET(devpresent, DR_NT_CPU, i))
			n++;
		if (!DEVSET_IN_SET(devset, DR_NT_CPU, i))
			continue;
		(void) sprintf(filename, "slot%d.cpu%d", bd, i);
		PR_ALL("%s: remove_node(%s)\n", f, filename);

		ddi_remove_minor_node(dip, filename);
	}
	if (n == 0) {
		/*
		 * No cpu components left, so get rid of "allcpu".
		 */
		(void) sprintf(filename, "slot%d.allcpu", bd);
		PR_ALL("%s: remove_node(%s)\n", f, filename);

		ddi_remove_minor_node(dip, filename);
	}
	/*
	 * Check for mem devices.
	 */
	n = 0;
	for (i = 0; i < MAX_MEM_UNITS_PER_BOARD; i++) {
		/*
		 * Check if any mems left (only one on starfire).
		 */
		if (DEVSET_IN_SET(devpresent, DR_NT_MEM, i))
			n++;
		if (!DEVSET_IN_SET(devset, DR_NT_MEM, i))
			continue;
		(void) sprintf(filename, "slot%d.mem%d", bd, i);
		PR_ALL("%s: remove_node(%s)\n", f, filename);

		ddi_remove_minor_node(dip, filename);
	}
	if (n == 0) {
		/*
		 * No mem components left, so get rid of "allmem".
		 */
		(void) sprintf(filename, "slot%d.allmem", bd);
		PR_ALL("%s: remove_node(%s)\n", f, filename);

		ddi_remove_minor_node(dip, filename);
	}
	/*
	 * Check for io devices.
	 */
	n = 0;
	for (i = 0; i < MAX_IO_UNITS_PER_BOARD; i++) {
		/*
		 * Check if any io left.
		 */
		if (DEVSET_IN_SET(devpresent, DR_NT_IO, i))
			n++;
		if (!DEVSET_IN_SET(devset, DR_NT_IO, i))
			continue;
		(void) sprintf(filename, "slot%d.io%d", bd, i);
		PR_ALL("%s: remove_node(%s)\n", f, filename);

		ddi_remove_minor_node(dip, filename);
	}
	if (n == 0) {
		/*
		 * No io components left, so get rid of "allio".
		 */
		(void) sprintf(filename, "slot%d.allio", bd);
		PR_ALL("%s: remove_node(%s)\n", f, filename);

		ddi_remove_minor_node(dip, filename);
	}

	return (0);
}

/*ARGSUSED*/
static int
sfdr_ioctl(dr_handle_t *hp)
{
	int		rv, mode;
	sfdr_ioctl_t	sfio;
	sfdr_handle_t	*shp;
	sfdr_board_t	*sbp;
	static fn_t	f = "sfdr_ioctl";

	PR_ALL("%s...\n", f);

	mode = hp->h_mode;
	shp = HD2MACHHD(hp);
	sbp = BD2MACHBD(hp->h_bd);

#ifdef _MULTI_DATAMODEL
	if (ddi_model_convert_from(mode & FMODELS) == DDI_MODEL_ILP32) {
		sfdr_ioctl32_t	sfio32;

		if (ddi_copyin((void *)shp->sh_arg, &sfio32,
				sizeof (sfdr_ioctl32_t), mode) != 0) {
			cmn_err(CE_WARN,
				"sfdr:%s: (32bit) failed to copyin arg "
				"for board %d", f, sbp->sb_num);
			return (EFAULT);
		}
		sfio.sfio_cmd = sfio32.sfio_cmd;
		sfio.sfio_arg = (void *)sfio32.sfio_arg;
	} else
#endif /* _MULTI_DATAMODEL */
	if (ddi_copyin((void *)shp->sh_arg, (void *)&sfio,
			sizeof (sfdr_ioctl_t), mode) != 0) {
		cmn_err(CE_WARN,
			"sfdr:%s: failed to copyin arg for board %d",
			f, sbp->sb_num);
		return (EFAULT);
	}

	rv = sfdr_ioctl_cmd(hp, sfio.sfio_cmd, sfio.sfio_arg);

	return (rv);
}

/*
 * Starfire DR support functions.
 */
/*ARGSUSED*/
static int
sfdr_ioctl_cmd(dr_handle_t *hp, int cmd, void *arg)
{
	sfdr_handle_t	*shp;
	sfdr_ioctl_arg_t	*iap = NULL;
	static fn_t	f = "sfdr_ioctl_cmd";

	PR_ALL("%s...\n", f);

	shp = HD2MACHHD(hp);

	if (arg) {
		int	rv;

		shp->sh_iap = iap = GETSTRUCT(sfdr_ioctl_arg_t, 1);
		rv = sfdr_copyin_ioarg(hp->h_mode, shp->sh_iap, arg);
		if (rv != 0) {
			FREESTRUCT(shp->sh_iap, sfdr_ioctl_arg_t, 1);
			shp->sh_iap = NULL;
			return (rv);
		}
		PR_ALL("%s (cmd = %s, cpuid = %d, maj = %d, "
			"flags = 0x%x)...\n",
			f, SFDR_CMD_STR(cmd), iap->i_cpuid,
			iap->i_major, iap->i_flags);
	} else {
		PR_ALL("%s (cmd = %s)...\n", f, (uint_t)SFDR_CMD_STR(cmd));
	}

	switch (cmd) {
	case SFDR_TEST_SUSPEND:
		(void) sfdr_test_suspend(hp, iap);
		break;

	case SFDR_JUGGLE_BOOTPROC:
		mutex_enter(&cp_list_lock);
		mutex_enter(&cpu_lock);
		(void) sfdr_juggle_bootproc(hp, iap->i_cpuid);
		mutex_exit(&cpu_lock);
		mutex_exit(&cp_list_lock);
		break;

	case SFDR_DUMP_PDAINFO:
		sfhw_dump_pdainfo(hp);
		break;

	case SFDR_TEST_CAGE:
		(void) sfdr_test_cage(hp, iap);
		break;

	default:
		break;
	}

	if (iap != NULL) {
		(void) sfdr_copyout_ioarg(hp->h_mode, shp->sh_iap, arg);
		FREESTRUCT(shp->sh_iap, sfdr_ioctl_arg_t, 1);
		shp->sh_iap = NULL;
	}

	return (0);
}

static int
sfdr_check_io_refs(dr_handle_t *hp, dr_devlist_t devlist[], int devnum)
{
	register int	i, reftotal = 0;
	static fn_t	f = "sfdr_check_io_refs";

	for (i = 0; i < devnum; i++) {
		dev_info_t	*dip;
		int		ref;

		dip = e_ddi_nodeid_to_dip(ddi_root_node(),
					devlist[i].dv_nodeid);
		if (dip != NULL) {
			ref = 0;
			sfdr_check_devices(dip, &ref, hp);
			if (ref) {
				if (SFDR_GET_ERR(HD2MACHERR(hp)) == 0) {
					char	errstr[80];

					sprintf(errstr,
						"%s: I/O devices active", f);
					SFDR_SET_ERR_STR(HD2MACHERR(hp),
							SFDR_ERR_BUSY,
							errstr);
				}
				DR_SET_ERRNO(&devlist[i].dv_error,
					SFDR_ERR2ERRNO(SFDR_ERR_BUSY));
			}
			PR_IO("%s: dip(%s) ref = %d\n",
				f, ddi_get_name(dip), ref);
			reftotal += ref;
		} else {
			PR_IO("%s: NO dip for nodeid (0x%x)\n",
				f, (uint_t)devlist[i].dv_nodeid);
		}
	}
	return (reftotal);
}

static int
sfdr_check_io_attached(dev_info_t *dip, void *arg)
{
	dnode_t	*nodeidp = (dnode_t *)arg;

	if (ddi_get_nodeid(dip) == (int)*nodeidp) {
		*nodeidp = 0;
		return (DDI_WALK_TERMINATE);
	} else {
		return (DDI_WALK_CONTINUE);
	}
}

/*
 * Called at driver load time to determine the state and condition
 * of an existing board in the system.
 */
static void
sfdr_board_discovery(sfdr_board_t *sbp)
{
	int		i, unit;
	dnode_t		nodeid;
	sfdr_devset_t	devs_lost, devs_attached = 0;
	static fn_t	f = "sfdr_board_discovery";

	if (SFDR_DEVS_PRESENT(sbp) == 0) {
		PR_ALL("%s: board %d has no devices present\n",
			f, sbp->sb_num);
		return;
	}

	/*
	 * Check for existence of cpus.
	 */
	for (i = 0; i < MAX_CPU_UNITS_PER_BOARD; i++) {
		if (!SFDR_DEV_IS_PRESENT(sbp, DR_NT_CPU, i))
			continue;

		nodeid = sbp->sb_devlist[NIX(DR_NT_CPU)][i];
		if (nodeid == (dnode_t)0)
			continue;

		unit = sfdr_check_unit_attached(nodeid, DR_NT_CPU);

		if (unit >= 0) {
			ASSERT(unit == i);
			SFDR_DEV_SET_ATTACHED(sbp, DR_NT_CPU, i);
			DEVSET_ADD(devs_attached, DR_NT_CPU, i);
			PR_ALL("%s: board %d, cpu-unit %d - attached\n",
				f, sbp->sb_num, i);
		}
		sfdr_init_cpu_unit(sbp, i);
	}

	/*
	 * Check for existence of memory.
	 */
	for (i = 0; i < MAX_MEM_UNITS_PER_BOARD; i++) {
		if (!SFDR_DEV_IS_PRESENT(sbp, DR_NT_MEM, i))
			continue;

		nodeid = sbp->sb_devlist[NIX(DR_NT_MEM)][i];
		if (nodeid == (dnode_t)0)
			continue;

		unit = sfdr_check_unit_attached(nodeid, DR_NT_MEM);

		if (unit >= 0) {
			ASSERT(unit == i);
			SFDR_DEV_SET_ATTACHED(sbp, DR_NT_MEM, i);
			DEVSET_ADD(devs_attached, DR_NT_MEM, i);
			PR_ALL("%s: board %d, mem-unit %d - attached\n",
				f, sbp->sb_num, i);
		}
		sfdr_init_mem_unit(sbp, i);
	}

	/*
	 * Check for i/o state.
	 */
	for (i = 0; i < MAX_IO_UNITS_PER_BOARD; i++) {
		if (!SFDR_DEV_IS_PRESENT(sbp, DR_NT_IO, i))
			continue;

		nodeid = sbp->sb_devlist[NIX(DR_NT_IO)][i];
		if (nodeid == (dnode_t)0)
			continue;

		unit = sfdr_check_unit_attached(nodeid, DR_NT_IO);

		if (unit >= 0) {
			/*
			 * Found it!
			 */
			ASSERT(unit == i);
			SFDR_DEV_SET_ATTACHED(sbp, DR_NT_IO, i);
			DEVSET_ADD(devs_attached, DR_NT_IO, i);
			PR_ALL("%s: board %d, io-unit %d - attached\n",
				f, sbp->sb_num, i);
		}
		sfdr_init_io_unit(sbp, i);
	}

	SFDR_DEVS_CONFIGURE(sbp, devs_attached);
	if (devs_attached && ((devs_lost = SFDR_DEVS_UNATTACHED(sbp)) != 0)) {
		int		ut;
		/*
		 * It is not legal on board discovery to have a
		 * board that is only partially attached.  A board
		 * is either all attached or all connected.  If a
		 * board has at least one attached device, then
		 * the the remaining devices, if any, must have
		 * been lost or disconnected.  These devices can
		 * only be recovered by a full attach from scratch.
		 * Note that devices previously in the unreferenced
		 * state are subsequently lost until the next full
		 * attach.  This is necessary since the driver unload
		 * that must have occurred would have wiped out the
		 * information necessary to re-configure the device
		 * back online, e.g. memlist.
		 */
		PR_ALL("%s: some devices LOST (0x%x)...\n", f, devs_lost);

		for (ut = 0; ut < MAX_CPU_UNITS_PER_BOARD; ut++)
			if (DEVSET_IN_SET(devs_lost, DR_NT_CPU, ut)) {
				SFDR_DEVICE_TRANSITION(sbp, DR_NT_CPU,
							ut, SFDR_STATE_EMPTY);
			}
		for (ut = 0; ut < MAX_MEM_UNITS_PER_BOARD; ut++)
			if (DEVSET_IN_SET(devs_lost, DR_NT_MEM, ut)) {
				SFDR_DEVICE_TRANSITION(sbp, DR_NT_MEM,
							ut, SFDR_STATE_EMPTY);
			}
		for (ut = 0; ut < MAX_IO_UNITS_PER_BOARD; ut++)
			if (DEVSET_IN_SET(devs_lost, DR_NT_IO, ut)) {
				SFDR_DEVICE_TRANSITION(sbp, DR_NT_IO,
							ut, SFDR_STATE_EMPTY);
			}

		SFDR_DEVS_DISCONNECT(sbp, devs_lost);
	}
}

/*ARGSUSED*/
static void
sfdr_board_init(sfdr_board_t *sbp, dev_info_t *dip, int bd)
{
	board_init(MACHBD2BD(sbp));

	sbp->sb_ref = 0;
	sbp->sb_num = bd;
	sbp->sb_dip = dip;

	/*
	 * Allocate the devlist for cpus.
	 */
	sbp->sb_devlist[NIX(DR_NT_CPU)] = GETSTRUCT(dnode_t,
						MAX_CPU_UNITS_PER_BOARD);

	/*
	 * Allocate the devlist for mem.
	 */
	sbp->sb_devlist[NIX(DR_NT_MEM)] = GETSTRUCT(dnode_t,
						MAX_MEM_UNITS_PER_BOARD);

	/*
	 * Allocate the devlist for io.
	 */
	sbp->sb_devlist[NIX(DR_NT_IO)] = GETSTRUCT(dnode_t,
						MAX_IO_UNITS_PER_BOARD);

	sbp->sb_dev[NIX(DR_NT_CPU)] = GETSTRUCT(sfdr_dev_unit_t,
						MAX_CPU_UNITS_PER_BOARD);

	sbp->sb_dev[NIX(DR_NT_MEM)] = GETSTRUCT(sfdr_dev_unit_t,
						MAX_MEM_UNITS_PER_BOARD);

	sbp->sb_dev[NIX(DR_NT_IO)] = GETSTRUCT(sfdr_dev_unit_t,
						MAX_IO_UNITS_PER_BOARD);

	/*
	 * Initialize the devlists
	 */
	if (!(sfdr_init_devlists(sbp))) {
		SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_EMPTY);
	} else {
		/*
		 * Couldn't have made it down here without
		 * having found at least one device.
		 */
		ASSERT(SFDR_DEVS_PRESENT(sbp) != 0);
		/*
		 * Check the state of any possible devices on the
		 * board.
		 */
		sfdr_board_discovery(sbp);
		(void) sfdr_make_dev_nodes(sbp, SFDR_DEVS_PRESENT(sbp));

		if (SFDR_DEVS_UNATTACHED(sbp) == 0) {
			/*
			 * The board has no unattached devices, therefore
			 * by reason of insanity it must be configured!
			 */
			SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_CONFIGURED);
		} else if (SFDR_DEVS_ATTACHED(sbp)) {
			SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_PARTIAL);
		} else {
			SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_CONNECTED);
		}
	}
}

/*ARGSUSED*/
static void
sfdr_board_destroy(sfdr_board_t *sbp)
{
	SFDR_BOARD_TRANSITION(sbp, SFDR_STATE_EMPTY);
	/*
	 * Free up MEM unit structs.
	 */
	FREESTRUCT(sbp->sb_dev[NIX(DR_NT_MEM)],
			sfdr_dev_unit_t, MAX_MEM_UNITS_PER_BOARD);
	sbp->sb_dev[NIX(DR_NT_MEM)] = NULL;
	/*
	 * Free up CPU unit structs.
	 */
	FREESTRUCT(sbp->sb_dev[NIX(DR_NT_CPU)],
			sfdr_dev_unit_t, MAX_CPU_UNITS_PER_BOARD);
	sbp->sb_dev[NIX(DR_NT_CPU)] = NULL;
	/*
	 * Free up IO unit structs.
	 */
	FREESTRUCT(sbp->sb_dev[NIX(DR_NT_IO)],
			sfdr_dev_unit_t, MAX_IO_UNITS_PER_BOARD);
	sbp->sb_dev[NIX(DR_NT_IO)] = NULL;

	/*
	 * free up CPU devlists.
	 */
	FREESTRUCT(sbp->sb_devlist[NIX(DR_NT_CPU)], dnode_t,
		MAX_CPU_UNITS_PER_BOARD);
	sbp->sb_devlist[NIX(DR_NT_CPU)] = NULL;
	/*
	 * free up MEM devlists.
	 */
	FREESTRUCT(sbp->sb_devlist[NIX(DR_NT_MEM)], dnode_t,
		MAX_MEM_UNITS_PER_BOARD);
	sbp->sb_devlist[NIX(DR_NT_MEM)] = NULL;
	/*
	 * free up IO devlists.
	 */
	FREESTRUCT(sbp->sb_devlist[NIX(DR_NT_IO)], dnode_t,
		MAX_IO_UNITS_PER_BOARD);
	sbp->sb_devlist[NIX(DR_NT_IO)] = NULL;

	board_destroy(MACHBD2BD(sbp));
}

/*ARGSUSED*/
static void
board_init(board_t *bp)
{
	mutex_init(&bp->b_lock, NULL, MUTEX_DRIVER, NULL);
}

/*ARGSUSED*/
static void
board_destroy(board_t *bp)
{
	mutex_destroy(&bp->b_lock);
}
