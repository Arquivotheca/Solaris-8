/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ipc.c	1.1	99/09/23 SMI"

#include <mdb/mdb_modapi.h>
#include <mdb/mdb_ks.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>

#include <vm/anon.h>

#define	CMN_HDR_START	"%<u>"
#define	CMN_HDR_END	"%</u>\n"
#define	CMN_INDENT	(4)
#define	CMN_INACTIVE	"%s facility inactive.\n"


/*
 * Bitmap data for page protection flags suitable for use with %b.
 */
const mdb_bitmask_t prot_flag_bits[] = {
	{ "PROT_READ", PROT_READ, PROT_READ },
	{ "PROT_WRITE", PROT_WRITE, PROT_WRITE },
	{ "PROT_EXEC", PROT_EXEC, PROT_EXEC },
	{ "PROT_USER", PROT_USER, PROT_USER },
	{ NULL, 0, 0 }
};

static void
printtime_nice(const char *str, time_t time)
{
	if (time) {
		mdb_printf("%s%Y\n", str, time);
	} else {
		mdb_printf("%sn/a\n", str);
	}
}


/*
 * Print header common to all IPC types.
 */
static void
ipcperm_header()
{
	mdb_printf(CMN_HDR_START "%1s %5s %8s %5s %5s %5s %5s %5s %?s"
	    CMN_HDR_END, "T", "ID", "KEY", "MODE", "OWNER", "GROUP",
	    "CREAT", "CGRP", "ADDR");
}

/*
 * Print data common to all IPC types.
 */
static void
ipcperm_print(struct ipc_perm *perm, char type, int id, uintptr_t addr)
{
	mdb_printf("%1c ", type);
	if (id >= 0) {
		mdb_printf("%5d ", id);
	} else {
		mdb_printf("%5s ", "inval");
	}
	mdb_printf("%08x %5#o %5d %5d %5d %5d %0?p\n",
	    perm->key, perm->mode & 07777,
	    perm->uid, perm->gid, perm->cuid, perm->cgid,
	    addr);
}


static void
msq_print(struct msqid_ds *msqid, uintptr_t addr)
{
	mdb_printf("first: %0?p    last: %0?p\n",
	    msqid->msg_first, msqid->msg_last);
	mdb_printf("cbytes: %lu    qnum: %lu    qbytes: %lu\n",
	    msqid->msg_cbytes, msqid->msg_qnum, msqid->msg_qbytes);
	mdb_printf("lspid: %d    lrpid: %d\n",
	    msqid->msg_lspid, msqid->msg_lrpid);
	printtime_nice("stime: ", msqid->msg_stime);
	printtime_nice("rtime: ", msqid->msg_rtime);
	printtime_nice("ctime: ", msqid->msg_ctime);
	mdb_printf("cv: %0?p    qnum_cv: %0?p\n",
	    addr + (uintptr_t)OFFSETOF(struct msqid_ds, msg_cv),
	    addr + (uintptr_t)OFFSETOF(struct msqid_ds, msg_qnum_cv));
}


static void
shm_print(struct shmid_ds *shmid, uintptr_t addr)
{
	struct anon_map	am;
	shmatt_t	nattch;

	/*
	 * The current attach count isn't maintained in shm_nattch; it
	 * is actually computed from the reference count on the anon
	 * map.
	 */
	if (mdb_vread(&am, sizeof (struct anon_map), (uintptr_t)shmid->shm_amp)
	    == -1) {
		mdb_warn("failed to read anon_map at %#p", shmid->shm_amp);
		/*
		 * Use existing value at shmid->shm_nattch, with
		 * hopes that it might be valid.
		 */
		nattch = shmid->shm_nattch;
	} else {
		nattch = (am.refcnt >> 1) - 1;
	}

	mdb_printf(CMN_HDR_START "%10s %?s %5s %7s %7s %7s %7s" CMN_HDR_END,
	    "SEGSZ", "AMP", "LKCNT", "LPID", "CPID", "NATTCH", "CNATTCH");
	mdb_printf("%10#x %0?p %5u %7d %7d %7lu %7lu\n",
	    shmid->shm_segsz, shmid->shm_amp, shmid->shm_lkcnt,
	    shmid->shm_lpid, shmid->shm_cpid, nattch,
	    shmid->shm_cnattch);

	mdb_inc_indent(CMN_INDENT);
	printtime_nice("atime: ", shmid->shm_atime);
	printtime_nice("dtime: ", shmid->shm_dtime);
	printtime_nice("ctime: ", shmid->shm_ctime);
	mdb_printf("cv: %0?p\n",
	    addr + (uintptr_t)OFFSETOF(struct shmid_ds, shm_cv));
	mdb_printf("sptinfo: %0?p    sptseg: %0?p\n",
	    shmid->shm_sptinfo, shmid->shm_sptseg);
	mdb_printf("sptprot: <%lb>\n", shmid->shm_sptprot, prot_flag_bits);
	mdb_dec_indent(CMN_INDENT);
}


/*ARGSUSED1*/
static void
sem_print(struct semid_ds *semid, uintptr_t addr)
{
	mdb_printf("base: %0?p    nsems: %u\n",
	    semid->sem_base, semid->sem_nsems);
	printtime_nice("otime: ", semid->sem_otime);
	printtime_nice("ctime: ", semid->sem_ctime);
	mdb_printf("binary: %s\n", semid->sem_binary ? "yes" : "no");
}


/*
 * Ops vector definition.  By using this, and by summoning the unholy
 * powers of OFFSETOF, we can avoid having three nearly identical
 * copies of the same code.
 */
typedef struct ipc_ops_vec {
	char	*iv_desc;	/* description		*/
	char	*iv_wcmd;	/* walker name		*/
	char	*iv_ocmd;	/* output dcmd		*/
	char	*iv_array;	/* id array name	*/
	char	*iv_info;	/* info structure name	*/
	void	(*iv_print)(void *, uintptr_t); /* output callback */
	int	iv_lengthoff;	/* offset to length field in info struct */
	int	iv_idsize;	/* size of id struct	*/
	int	iv_insize;	/* size of info struct	*/
	char	iv_type;	/* one character type id */
} ipc_ops_vec_t;

ipc_ops_vec_t msg_ops_vec = {
	"Message Queue",
	"msg",
	"msqid_ds",
	"msgque",
	"msginfo",
	(void(*)(void *, uintptr_t))msq_print,
	OFFSETOF(struct msginfo, msgmni),
	sizeof (struct msqid_ds),
	sizeof (struct msginfo),
	'q'
};

ipc_ops_vec_t shm_ops_vec = {
	"Shared Memory",
	"shm",
	"shmid_ds",
	"shmem",
	"shminfo",
	(void(*)(void *, uintptr_t))shm_print,
	OFFSETOF(struct shminfo, shmmni),
	sizeof (struct shmid_ds),
	sizeof (struct shminfo),
	'm'
};

ipc_ops_vec_t sem_ops_vec = {
	"Semaphore",
	"sem",
	"semid_ds",
	"sema",
	"seminfo",
	(void(*)(void *, uintptr_t))sem_print,
	OFFSETOF(struct seminfo, semmni),
	sizeof (struct semid_ds),
	sizeof (struct seminfo),
	's'
};


/*
 * Generic IPC data structure display code
 */
static int
ds_print(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv,
    ipc_ops_vec_t *iv)
{
	uint_t		oflags = 0;
	uintptr_t	array;
	void		*iddata;
	void		*info;
	int		id, max;

	if (!(flags & DCMD_ADDRSPEC)) {
		if (mdb_walk_dcmd(iv->iv_wcmd, iv->iv_ocmd, argc, argv) == -1) {
			mdb_warn("can't walk '%s'", iv->iv_wcmd);
			return (DCMD_ERR);
		}
		return (DCMD_OK);
	}

	if (mdb_getopts(argc, argv, 'l', MDB_OPT_SETBITS, 1, &oflags, NULL)
	    != argc)
		return (DCMD_USAGE);

	if (mdb_readvar(&array, iv->iv_array) == -1) {
		mdb_warn("failed to read '%s'", iv->iv_array);
		return (DCMD_ERR);
	}

	iddata = mdb_alloc(iv->iv_idsize, UM_SLEEP | UM_GC);
	if (mdb_vread(iddata, iv->iv_idsize, addr) == -1) {
		mdb_warn("failed to read %s at %#p", iv->iv_ocmd, addr);
		return (DCMD_ERR);
	}

	info = mdb_alloc(iv->iv_insize, UM_SLEEP | UM_GC);
	if (mdb_readvar(info, iv->iv_info) == -1) {
		mdb_warn("failed to read '%s'", iv->iv_info);
		return (DCMD_ERR);
	}

	id = (addr - array) / iv->iv_idsize;
	max = *(int *)((uintptr_t)info + iv->iv_lengthoff);
	if ((id < 0) || (id >= max)) {
		id = -1;
	} else {
		id += max * ((struct ipc_perm *)iddata)->seq;
	}

	if (!DCMD_HDRSPEC(flags) && (oflags && iv->iv_print)) {
		mdb_printf("\n");
	}

	if (DCMD_HDRSPEC(flags) || (oflags && iv->iv_print)) {
		ipcperm_header();
	}

	ipcperm_print((struct ipc_perm *)iddata, iv->iv_type, id, addr);
	if (oflags && iv->iv_print) {
		mdb_inc_indent(CMN_INDENT);
		iv->iv_print(iddata, addr);
		mdb_dec_indent(CMN_INDENT);
	}

	return (DCMD_OK);
}


/*
 * Stubs to call ds_print with the appropriate ops vector
 */

static int
shmidds(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (ds_print(addr, flags, argc, argv, &shm_ops_vec));
}


static int
msqidds(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (ds_print(addr, flags, argc, argv, &msg_ops_vec));
}

static int
semidds(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (ds_print(addr, flags, argc, argv, &sem_ops_vec));
}


/*
 * Generic IPC walker
 */

typedef struct ipc_walk_data {
	int	iw_index;
	int	iw_max;
	void	*iw_iddata;
} ipc_walk_data_t;


static int
ds_walk_init(mdb_walk_state_t *wsp)
{
	ipc_ops_vec_t	*iv = wsp->walk_arg;
	ipc_walk_data_t	*iwd;
	void		*info;
	uintptr_t	array;
	int		*max;

	info = mdb_alloc(iv->iv_insize, UM_SLEEP);
	max = (int *)((uintptr_t)info + iv->iv_lengthoff);
	if ((mdb_readvar(info, iv->iv_info) == -1) ||
	    (mdb_readvar(&array, iv->iv_array) == -1) ||
	    (array == NULL) || (*max == 0)) {
		mdb_free(info, iv->iv_insize);
		mdb_warn(CMN_INACTIVE, iv->iv_desc);
		return (WALK_DONE);
	}

	iwd = mdb_alloc(sizeof (ipc_walk_data_t), UM_SLEEP);
	iwd->iw_index = 0;
	iwd->iw_max = *max;
	iwd->iw_iddata = mdb_alloc(iv->iv_idsize, UM_SLEEP);

	wsp->walk_addr = array;
	wsp->walk_data = iwd;

	mdb_free(info, iv->iv_insize);

	return (WALK_NEXT);
}

static int
ds_walk_step(mdb_walk_state_t *wsp)
{
	ipc_ops_vec_t	*iv = wsp->walk_arg;
	ipc_walk_data_t	*iwd = wsp->walk_data;
	uintptr_t	addr = wsp->walk_addr;
	int		status;

	if (iwd->iw_index == iwd->iw_max) {
		return (WALK_DONE);
	}

	if (mdb_vread(iwd->iw_iddata, iv->iv_idsize, addr) == -1) {
		mdb_warn("failed to read %s at %#p", iv->iv_ocmd, addr);
		return (WALK_DONE);
	}

	if (((struct ipc_perm *)iwd->iw_iddata)->mode & IPC_ALLOC) {
		status = wsp->walk_callback(addr, iwd->iw_iddata,
		    wsp->walk_cbdata);
	}

	iwd->iw_index++;
	wsp->walk_addr = addr + iv->iv_idsize;

	return (status);
}


static void
ds_walk_fini(mdb_walk_state_t *wsp)
{
	mdb_free(((ipc_walk_data_t *)wsp->walk_data)->iw_iddata,
	    ((ipc_ops_vec_t *)wsp->walk_arg)->iv_idsize);
	mdb_free(wsp->walk_data, sizeof (ipc_walk_data_t));
}


/*
 * The "::ipcs" command itself.  Just walks each IPC type in turn.
 */

/*ARGSUSED*/
static int
ipcs(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uint_t	oflags = 0;

	if (mdb_getopts(argc, argv, 'l', MDB_OPT_SETBITS, 1, &oflags, NULL)
	    != argc)
		return (DCMD_USAGE);

	if (mdb_walk_dcmd("msg", "msqid_ds", argc, argv) == -1) {
		mdb_warn("can't walk 'msg'");
		return (DCMD_ERR);
	}

	if (oflags) {
		mdb_printf("\n");
	}

	if (mdb_walk_dcmd("shm", "shmid_ds", argc, argv) == -1) {
		mdb_warn("can't walk 'shm'");
		return (DCMD_ERR);
	}

	if (oflags) {
		mdb_printf("\n");
	}

	if (mdb_walk_dcmd("sem", "semid_ds", argc, argv) == -1) {
		mdb_warn("can't walk 'sem'");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}


/*
 * MDB module linkage
 */

static const mdb_dcmd_t dcmds[] = {
	{ "shmid_ds", "?[-l]", "display a shmid_ds", shmidds },
	{ "msqid_ds", "?[-l]", "display a msqid_ds", msqidds },
	{ "semid_ds", "?[-l]", "display s semid_ds", semidds },
	{ "ipcs", "[-l]", "display SysV IPC information", ipcs },
	{ NULL }
};

static const mdb_walker_t walkers[] = {
	{ "shm", "walk the active shmid_ds structures",
		ds_walk_init, ds_walk_step, ds_walk_fini, &shm_ops_vec },
	{ "msg", "walk the active msqid_ds structures",
		ds_walk_init, ds_walk_step, ds_walk_fini, &msg_ops_vec },
	{ "sem", "walk the active semid_ds structures",
		ds_walk_init, ds_walk_step, ds_walk_fini, &sem_ops_vec },
	{ NULL }
};

static const mdb_modinfo_t modinfo = { MDB_API_VERSION, dcmds, walkers };

const mdb_modinfo_t *
_mdb_init(void)
{
	return (&modinfo);
}
