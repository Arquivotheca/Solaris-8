/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)genunix.c	1.10	99/12/12 SMI"

#include <mdb/mdb_param.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_ks.h>

#include <sys/types.h>
#include <sys/mutex.h>
#include <sys/thread.h>
#include <sys/condvar.h>
#include <sys/session.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/var.h>
#include <sys/t_lock.h>
#include <sys/callo.h>
#include <sys/priocntl.h>
#include <sys/class.h>
#include <sys/regset.h>
#include <sys/stack.h>
#include <sys/cpuvar.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/fs/autofs.h>
#include <sys/flock_impl.h>
#include <sys/kmem_impl.h>
#include <sys/vmem_impl.h>
#include <sys/kstat.h>
#include <sys/sleepq.h>
#include <sys/sobject.h>
#include <vm/seg_vn.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/seg_map.h>
#include <sys/dditypes.h>
#include <sys/ddi_impldefs.h>
#include <alloca.h>
#include <sys/rwlock_impl.h>
#include <sys/turnstile.h>
#include <sys/sysmacros.h>
#include <sys/sysconf.h>

#include "devinfo.h"
#include "leaky.h"
#include "kmem.h"
#include "bio.h"
#include "streams.h"
#include "cyclic.h"
#include "findstack.h"

/*
 * Surely this is defined somewhere...
 */
#define	NINTR		16

#ifndef STACK_BIAS
#define	STACK_BIAS	0
#endif

static char
pstat2ch(uchar_t state)
{
	switch (state) {
		case SSLEEP: return ('S');
		case SRUN: return ('R');
		case SZOMB: return ('Z');
		case SIDL: return ('I');
		case SONPROC: return ('O');
		case SSTOP: return ('T');
		default: return ('?');
	}
}

#define	PS_PRTTHREADS	0x1
#define	PS_PRTLWPS	0x2
#define	PS_PSARGS	0x4

static int
ps_threadprint(uintptr_t addr, const void *data, void *private)
{
	const kthread_t *t = (const kthread_t *)data;
	uint_t prt_flags = *((uint_t *)private);

	static const mdb_bitmask_t t_state_bits[] = {
		{ "TS_FREE",	TS_FREE,	TS_FREE		},
		{ "TS_SLEEP",	TS_SLEEP,	TS_SLEEP	},
		{ "TS_RUN",	TS_RUN,		TS_RUN		},
		{ "TS_ONPROC",	TS_ONPROC,	TS_ONPROC	},
		{ "TS_ZOMB",	TS_ZOMB,	TS_ZOMB		},
		{ "TS_STOPPED",	TS_STOPPED,	TS_STOPPED	},
		{ NULL,		0,		0		}
	};

	if (prt_flags & PS_PRTTHREADS)
		mdb_printf("\tT  %?a <%b>\n", addr, t->t_state, t_state_bits);

	if (prt_flags & PS_PRTLWPS)
		mdb_printf("\tL  %?a ID: %u\n", t->t_lwp, t->t_tid);

	return (WALK_NEXT);
}

int
ps(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uint_t prt_flags = 0;
	proc_t pr;
	struct pid pid, pgid, sid;
	sess_t session;
	cred_t cred;

	if (!(flags & DCMD_ADDRSPEC)) {
		if (mdb_walk_dcmd("proc", "ps", argc, argv) == -1) {
			mdb_warn("can't walk 'proc'");
			return (DCMD_ERR);
		}
		return (DCMD_OK);
	}

	if (mdb_getopts(argc, argv,
	    'f', MDB_OPT_SETBITS, PS_PSARGS, &prt_flags,
	    'l', MDB_OPT_SETBITS, PS_PRTLWPS, &prt_flags,
	    't', MDB_OPT_SETBITS, PS_PRTTHREADS, &prt_flags, NULL) != argc)
		return (DCMD_USAGE);

	if (DCMD_HDRSPEC(flags)) {
		mdb_printf("%<u>%1s %5s %5s %5s %5s %5s %10s %?s %s%</u>\n",
		    "S", "PID", "PPID", "PGID", "SID",
		    "UID", "FLAGS", "ADDR", "NAME");
	}

	mdb_vread(&pr, sizeof (pr), addr);
	mdb_vread(&pid, sizeof (pid), (uintptr_t)pr.p_pidp);
	mdb_vread(&pgid, sizeof (pgid), (uintptr_t)pr.p_pgidp);
	mdb_vread(&cred, sizeof (cred), (uintptr_t)pr.p_cred);
	mdb_vread(&session, sizeof (session), (uintptr_t)pr.p_sessp);
	mdb_vread(&sid, sizeof (sid), (uintptr_t)session.s_sidp);

	mdb_printf("%c %5d %5d %5d %5d %5d 0x%08x %0?p %s\n",
	    pstat2ch(pr.p_stat), pid.pid_id, pr.p_ppid, pgid.pid_id,
	    sid.pid_id, cred.cr_uid, pr.p_flag, addr,
	    (prt_flags & PS_PSARGS) ? pr.p_user.u_psargs :
	    pr.p_user.u_comm);

	if (prt_flags & ~PS_PSARGS)
		(void) mdb_pwalk("thread", ps_threadprint, &prt_flags, addr);

	return (DCMD_OK);
}

/*ARGSUSED*/
int
callout(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	callout_table_t	*co_ktable[CALLOUT_TABLES];
	int co_kfanout;
	callout_table_t co_table;
	callout_t co_callout;
	callout_t *co_ptr;
	int co_id;
	clock_t lbolt;
	int i, j, k;
	const char *lbolt_sym;

	if (mdb_prop_postmortem)
		lbolt_sym = "panic_lbolt";
	else
		lbolt_sym = "lbolt";

	if (mdb_readvar(&lbolt, lbolt_sym) == -1) {
		mdb_warn("failed to read '%s'", lbolt_sym);
		return (DCMD_ERR);
	}

	if (mdb_readvar(&co_kfanout, "callout_fanout") == -1) {
		mdb_warn("failed to read callout_fanout");
		return (DCMD_ERR);
	}

	if (mdb_readvar(&co_ktable, "callout_table") == -1) {
		mdb_warn("failed to read callout_table");
		return (DCMD_ERR);
	}

	mdb_printf("%<u>%-24s %-?s %-?s %-?s%</u>\n",
	    "FUNCTION", "ARGUMENT", "ID", "TIME");

	for (i = 0; i < CALLOUT_NTYPES; i++) {
		for (j = 0; j < co_kfanout; j++) {

			co_id = CALLOUT_TABLE(i, j);

			if (mdb_vread(&co_table, sizeof (co_table),
			    (uintptr_t)co_ktable[co_id]) == -1) {
				mdb_warn("failed to read table at %p",
				    (uintptr_t)co_ktable[co_id]);
				continue;
			}

			for (k = 0; k < CALLOUT_BUCKETS; k++) {
				co_ptr = co_table.ct_idhash[k];

				while (co_ptr != NULL) {
					mdb_vread(&co_callout,
					    sizeof (co_callout),
					    (uintptr_t)co_ptr);

					mdb_printf("%-24a %0?p %0?lx %?lx "
					    "(T%+ld)\n", co_callout.c_func,
					    co_callout.c_arg, co_callout.c_xid,
					    co_callout.c_runtime,
					    co_callout.c_runtime - lbolt);

					co_ptr = co_callout.c_idnext;
				}
			}
		}
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
int
class(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	long num_classes, i;
	sclass_t *class_tbl;
	GElf_Sym g_sclass;
	char class_name[PC_CLNMSZ];
	size_t tbl_size;

	if (mdb_lookup_by_name("sclass", &g_sclass) == -1) {
		mdb_warn("failed to find symbol sclass\n");
		return (DCMD_ERR);
	}

	tbl_size = g_sclass.st_size;
	num_classes = tbl_size / (sizeof (sclass_t));
	class_tbl = mdb_alloc(tbl_size, UM_SLEEP | UM_GC);

	if (mdb_readsym(class_tbl, tbl_size, "sclass") == -1) {
		mdb_warn("failed to read sclass");
		return (DCMD_ERR);
	}

	mdb_printf("%<u>%4s %-10s %-24s %-24s%</u>\n", "SLOT", "NAME",
	    "INIT FCN", "CLASS FCN");

	for (i = 0; i < num_classes; i++) {
		if (mdb_vread(class_name, sizeof (class_name),
		    (uintptr_t)class_tbl[i].cl_name) == -1)
			(void) strcpy(class_name, "???");

		mdb_printf("%4ld %-10s %-24a %-24a\n", i, class_name,
		    class_tbl[i].cl_init, class_tbl[i].cl_funcs);
	}

	return (DCMD_OK);
}

static void
print_vop(uintptr_t addr)
{
	vnode_t vn;

	if (mdb_vread(&vn, sizeof (vn), addr) == -1)
		mdb_printf("<bad vp=%p>\n", addr);
	else
		mdb_printf("%p (%a)\n", addr, vn.v_op);
}

int
vnode2path(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	int offs[MDB_PATH_NELEM];
	uintptr_t rootdir;
	mdb_path_t path;
	vnode_t vn;

	uint_t opt_v = FALSE;
	uint_t opt_F = FALSE;
	int i, pos = 1;

	if (mdb_getopts(argc, argv,
	    'v', MDB_OPT_SETBITS, TRUE, &opt_v,
	    'F', MDB_OPT_SETBITS, TRUE, &opt_F, NULL) != argc)
		return (DCMD_USAGE);

	if (!(flags & DCMD_ADDRSPEC)) {
		mdb_warn("expected explicit vnode_t address before ::\n");
		return (DCMD_USAGE);
	}

	if (mdb_readvar(&rootdir, "rootdir") == -1) {
		mdb_warn("failed to read rootdir");
		return (DCMD_ERR);
	}

	if (mdb_vnode2path(addr, &path) == -1)
		return (DCMD_ERR);

	if (!path.mdp_complete && path.mdp_nelem == 0)
		return (DCMD_OK);

	if (path.mdp_nelem == 0)
		mdb_printf("/\n");
	else {
		if (!path.mdp_complete) {
			mdb_printf("??");
			pos += 2;
		}

		for (i = path.mdp_nelem - 1; i >= 0; i--) {
			int l = strlen(path.mdp_name[i]);

			mdb_printf("/%s", path.mdp_name[i]);
			offs[i] = pos + (l + 1) / 2 - path.mdp_complete;
			pos += l + 1;
		}

		if (opt_F && mdb_vread(&vn, sizeof (vn), addr) == sizeof (vn))
			mdb_printf("%c", mdb_vtype2chr(vn.v_type, 0));

		mdb_printf("\n");
	}

	if (!opt_v)
		return (DCMD_OK);

	for (i = 0; i < path.mdp_nelem; i++) {
		int last, j, header = 1;
again:
		last = 0;

		if (path.mdp_complete)
			mdb_printf("|");

		for (j = path.mdp_nelem - 1; j >= i; j--) {
			mdb_printf("%*s%c", offs[j] - last - 1, "",
			    (j > i || header) ? '|' : '+');
			last = offs[j];
		}

		if (header) {
			header--;
			mdb_printf("\n");
			goto again;
		}

		mdb_printf("--> ");
		print_vop(path.mdp_vnode[i]);
	}

	if (path.mdp_complete) {
		mdb_printf("|\n+--> ");
		print_vop(rootdir);
	}

	return (DCMD_OK);
}

#ifdef _LP64
#define	FSINFO_MNTLEN	24
#else
#define	FSINFO_MNTLEN	32
#endif

/*ARGSUSED*/
int
fsinfo(uintptr_t fsid, uint_t flags, int argc, const mdb_arg_t *argv)
{
	struct vfs vfs;
	uintptr_t vfsp;

	int autofs_fstype;
	char buf[FSINFO_MNTLEN];
	int oprop = mdb_prop_postmortem;

	if (argc != 0)
		return (DCMD_USAGE);

	if (mdb_readvar(&vfsp, "rootvfs") == -1) {
		mdb_warn("failed to read 'rootvfs'");
		return (DCMD_ERR);
	}

	mdb_prop_postmortem = TRUE;

	if (mdb_readvar(&autofs_fstype, "autofs_fstype") == -1)
		autofs_fstype = -1;

	mdb_printf("%<u>%?s %3s %?s %-16s %-s%</u>\n",
	    "VFSP", "TYP", "DATA", "OPS", "MOUNT");

	while (vfsp != NULL) {
		mdb_vread(&vfs, sizeof (vfs), vfsp);

		mdb_printf("%?p %3d %?p %-16a ", vfsp, vfs.vfs_fstype,
		    vfs.vfs_data, vfs.vfs_op);

		if (vfs.vfs_fstype == autofs_fstype) {
			char mp[FSINFO_MNTLEN];
			fninfo_t fn;
			int len;

			mdb_vread(&fn, sizeof (fn), (uintptr_t)vfs.vfs_data);

			len = fn.fi_pathlen > FSINFO_MNTLEN - 1 ?
			    FSINFO_MNTLEN - 1: fn.fi_pathlen;

			mdb_vread(mp, len, (uintptr_t)fn.fi_path);
			mp[len] = '\0';

			if (len < fn.fi_pathlen)
				strcpy(&mp[len - 3], "...");

			mdb_printf("%s", mp);

		} else {
			mdb_path_t path;

			mdb_vnode2path((uintptr_t)vfs.vfs_vnodecovered, &path);
			mdb_sprintpath(buf, FSINFO_MNTLEN, &path);
			mdb_printf("%s", buf);
		}

		mdb_printf("\n");
		vfsp = (uintptr_t)vfs.vfs_next;
	}

	mdb_prop_postmortem = oprop;

	return (DCMD_OK);
}

/*ARGSUSED*/
int
lminfo(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	GElf_Sym sym;
	graph_t **ghash;
	int nent, i;
	uintptr_t ptr;

	int oprop = mdb_prop_postmortem;

	if (argc != 0)
		return (DCMD_USAGE);

	if (mdb_lookup_by_name("lock_graph", &sym) == -1) {
		mdb_warn("failed to find 'lock_graph'\n");
		return (DCMD_ERR);
	}

	nent = sym.st_size / sizeof (graph_t *);
	ptr = (uintptr_t)sym.st_value;

	ghash = mdb_alloc(nent * sizeof (graph_t *), UM_SLEEP | UM_GC);
	mdb_vread(ghash, sym.st_size, ptr);

	mdb_printf("%<u>%-?s %2s %4s %5s %-16s %-?s %s%</u>\n",
	    "ADDR", "TP", "FLAG", "PID", "COMM", "VNODE", "PATH");

	mdb_prop_postmortem = TRUE;

	for (i = 0; i < nent; i++) {
		graph_t gr;
		uintptr_t lp, end;
		lock_descriptor_t ld;
		int active = 1;

		if (ghash[i] == NULL)
			continue;

		mdb_vread(&gr, sizeof (gr), (uintptr_t)ghash[i]);

		lp = (uintptr_t)gr.active_locks.l_next;
		end = (uintptr_t)ghash[i] +
		    OFFSETOF(graph_t, active_locks.l_next);

again:
		while (lp != end) {
			proc_t p;
			mdb_path_t path;
			char buf[14];

			mdb_vread(&ld, sizeof (ld), lp);

			mdb_printf("%-?p %2s %04x %5d %-16s %-?p ",
			    lp, ld.l_type == F_RDLCK ? "RD" :
			    ld.l_type == F_WRLCK ? "WR" : "??",
			    ld.l_state, ld.l_flock.l_pid,
			    ld.l_flock.l_pid == 0 ? "<kernel>" :
			    mdb_pid2proc(ld.l_flock.l_pid, &p) == NULL ?
			    "<defunct>" : p.p_user.u_comm,
			    ld.l_vnode);

			mdb_vnode2path((uintptr_t)ld.l_vnode, &path);
#ifdef _LP64
			mdb_sprintpath(buf, 14, &path);
#else
			mdb_sprintpath(buf, 24, &path);
#endif
			mdb_printf("%s\n", buf);

			lp = (uintptr_t)ld.l_next;
		}

		if (active) {
			active = 0;
			lp = (uintptr_t)gr.sleeping_locks.l_next;
			end = (uintptr_t)ghash[i] +
			    OFFSETOF(graph_t, sleeping_locks.l_next);
			goto again;
		}
	}

	mdb_prop_postmortem = oprop;
	return (DCMD_OK);
}

/*ARGSUSED*/
int
seg(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	struct seg s;

	if (argc != 0)
		return (DCMD_USAGE);

	if ((flags & DCMD_LOOPFIRST) || !(flags & DCMD_LOOP)) {
		mdb_printf("%<u>%?s %?s %?s %?s %s%</u>\n",
		    "SEG", "BASE", "SIZE", "DATA", "OPS");
	}

	if (mdb_vread(&s, sizeof (s), addr) == -1) {
		mdb_warn("failed to read seg at %p", addr);
		return (DCMD_ERR);
	}

	mdb_printf("%?p %?p %?lx %?p %a\n",
	    addr, s.s_base, s.s_size, s.s_data, s.s_ops);

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
pmap_walk_anon(uintptr_t addr, const struct anon *anon, int *nres)
{
	uintptr_t pp =
	    mdb_vnode2page((uintptr_t)anon->an_vp, (uintptr_t)anon->an_off);

	if (pp != NULL)
		(*nres)++;

	return (WALK_NEXT);
}

static int
pmap_walk_seg(uintptr_t addr, const struct seg *seg, uintptr_t segvn)
{

	mdb_printf("%0?p %0?p %7dk", addr, seg->s_base, seg->s_size / 1024);

	if (segvn == (uintptr_t)seg->s_ops) {
		struct segvn_data svn;
		int nres = 0;

		(void) mdb_vread(&svn, sizeof (svn), (uintptr_t)seg->s_data);

		if (svn.amp == NULL) {
			mdb_printf(" %8s", "");
			goto drive_on;
		}

		/*
		 * We've got an amp for this segment; walk through
		 * the amp, and determine mappings.
		 */
		if (mdb_pwalk("anon", (mdb_walk_cb_t)pmap_walk_anon,
		    &nres, (uintptr_t)svn.amp) == -1)
			mdb_printf("failed to walk anon: amp=%p\n", svn.amp);

		mdb_printf(" %7dk", (nres * PAGESIZE) / 1024);
drive_on:

		if (svn.vp != NULL) {
			mdb_path_t path;
			char buf[29];

			mdb_vnode2path((uintptr_t)svn.vp, &path);
			mdb_sprintpath(buf, sizeof (buf), &path);
			mdb_printf(" %s", buf);
		} else
			mdb_printf(" [ anon ]");
	}

	mdb_printf("\n");
	return (WALK_NEXT);
}

/*ARGSUSED*/
int
pmap(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uintptr_t segvn;
	proc_t proc;

	GElf_Sym sym;

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb_vread(&proc, sizeof (proc), addr) == -1) {
		mdb_warn("failed to read proc at %p", addr);
		return (DCMD_ERR);
	}

	if (mdb_lookup_by_name("segvn_ops", &sym) == 0)
		segvn = (uintptr_t)sym.st_value;
	else
		segvn = NULL;

	mdb_printf("%?s %?s %8s %8s %s\n",
	    "SEG", "BASE", "SIZE", "RES", "PATH");

	if (mdb_pwalk("seg", (mdb_walk_cb_t)pmap_walk_seg,
	    (void *)segvn, (uintptr_t)proc.p_as) == -1) {
		mdb_warn("failed to walk segments of as %p", proc.p_as);
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

typedef struct anon_walk_data {
	uintptr_t *aw_levone;
	uintptr_t *aw_levtwo;
	int aw_nlevone;
	int aw_levone_ndx;
	int aw_levtwo_ndx;
	struct anon_map aw_amp;
	struct anon_hdr aw_ahp;
} anon_walk_data_t;

int
anon_walk_init(mdb_walk_state_t *wsp)
{
	anon_walk_data_t *aw;

	if (wsp->walk_addr == NULL) {
		mdb_printf("anon walk doesn't support global walks\n");
		return (WALK_ERR);
	}

	aw = mdb_alloc(sizeof (anon_walk_data_t), UM_SLEEP);

	if (mdb_vread(&aw->aw_amp, sizeof (aw->aw_amp), wsp->walk_addr) == -1) {
		mdb_warn("failed to read anon map at %p", wsp->walk_addr);
		mdb_free(aw, sizeof (anon_walk_data_t));
		return (WALK_ERR);
	}

	if (mdb_vread(&aw->aw_ahp, sizeof (aw->aw_ahp),
	    (uintptr_t)(aw->aw_amp.ahp)) == -1) {
		mdb_warn("failed to read anon hdr ptr at %p", aw->aw_amp.ahp);
		mdb_free(aw, sizeof (anon_walk_data_t));
		return (WALK_ERR);
	}

	if (aw->aw_ahp.size <= ANON_CHUNK_SIZE ||
	    (aw->aw_ahp.flags & ANON_ALLOC_FORCE)) {
		aw->aw_nlevone = aw->aw_ahp.size;
		aw->aw_levtwo = NULL;
	} else {
		aw->aw_nlevone =
		    (aw->aw_ahp.size + ANON_CHUNK_OFF) >> ANON_CHUNK_SHIFT;
		aw->aw_levtwo =
		    mdb_zalloc(ANON_CHUNK_SIZE * sizeof (uintptr_t), UM_SLEEP);
	}

	aw->aw_levone =
	    mdb_alloc(aw->aw_nlevone * sizeof (uintptr_t), UM_SLEEP);

	aw->aw_levone_ndx = 0;
	aw->aw_levtwo_ndx = 0;

	mdb_vread(aw->aw_levone, aw->aw_nlevone * sizeof (uintptr_t),
	    (uintptr_t)aw->aw_ahp.array_chunk);

	if (aw->aw_levtwo != NULL) {
		while (aw->aw_levone[aw->aw_levone_ndx] == NULL) {
			aw->aw_levone_ndx++;
			if (aw->aw_levone_ndx == aw->aw_nlevone) {
				mdb_warn("corrupt anon; couldn't"
				    "find ptr to lev two map");
				goto out;
			}
		}

		mdb_vread(aw->aw_levtwo, ANON_CHUNK_SIZE * sizeof (uintptr_t),
		    aw->aw_levone[aw->aw_levone_ndx]);
	}

out:
	wsp->walk_data = aw;
	return (0);
}

int
anon_walk_step(mdb_walk_state_t *wsp)
{
	int status;
	anon_walk_data_t *aw = (anon_walk_data_t *)wsp->walk_data;
	struct anon anon;
	uintptr_t anonptr;

again:
	/*
	 * Once we've walked through level one, we're done.
	 */
	if (aw->aw_levone_ndx == aw->aw_nlevone)
		return (WALK_DONE);

	if (aw->aw_levtwo == NULL) {
		anonptr = aw->aw_levone[aw->aw_levone_ndx];
		aw->aw_levone_ndx++;
	} else {
		anonptr = aw->aw_levtwo[aw->aw_levtwo_ndx];
		aw->aw_levtwo_ndx++;

		if (aw->aw_levtwo_ndx == ANON_CHUNK_SIZE) {
			aw->aw_levtwo_ndx = 0;

			do {
				aw->aw_levone_ndx++;

				if (aw->aw_levone_ndx == aw->aw_nlevone)
					return (WALK_DONE);
			} while (aw->aw_levone[aw->aw_levone_ndx] == NULL);

			mdb_vread(aw->aw_levtwo, ANON_CHUNK_SIZE *
			    sizeof (uintptr_t),
			    aw->aw_levone[aw->aw_levone_ndx]);
		}
	}

	if (anonptr != NULL) {
		mdb_vread(&anon, sizeof (anon), anonptr);
		status = wsp->walk_callback(anonptr, &anon, wsp->walk_cbdata);
	} else
		goto again;

	return (status);
}

void
anon_walk_fini(mdb_walk_state_t *wsp)
{
	anon_walk_data_t *aw = (anon_walk_data_t *)wsp->walk_data;

	if (aw->aw_levtwo != NULL)
		mdb_free(aw->aw_levtwo, ANON_CHUNK_SIZE * sizeof (uintptr_t));

	mdb_free(aw->aw_levone, aw->aw_nlevone * sizeof (uintptr_t));
	mdb_free(aw, sizeof (anon_walk_data_t));
}

typedef struct wchan_walk_data {
	caddr_t *ww_seen;
	int ww_seen_size;
	int ww_seen_ndx;
	uintptr_t ww_thr;
	sleepq_head_t ww_sleepq[NSLEEPQ];
	int ww_sleepq_ndx;
	uintptr_t ww_compare;
} wchan_walk_data_t;

int
wchan_walk_init(mdb_walk_state_t *wsp)
{
	wchan_walk_data_t *ww =
	    mdb_zalloc(sizeof (wchan_walk_data_t), UM_SLEEP);

	if (mdb_readvar(&ww->ww_sleepq[0], "sleepq_head") == -1) {
		mdb_warn("failed to read sleepq");
		mdb_free(ww, sizeof (wchan_walk_data_t));
		return (WALK_ERR);
	}

	if ((ww->ww_compare = wsp->walk_addr) == NULL) {
		if (mdb_readvar(&ww->ww_seen_size, "nthread") == -1) {
			mdb_warn("failed to read nthread");
			mdb_free(ww, sizeof (wchan_walk_data_t));
			return (WALK_ERR);
		}

		ww->ww_seen = mdb_alloc(ww->ww_seen_size *
		    sizeof (caddr_t), UM_SLEEP);
	} else {
		ww->ww_sleepq_ndx = SQHASHINDEX(wsp->walk_addr);
	}

	wsp->walk_data = ww;
	return (WALK_NEXT);
}

int
wchan_walk_step(mdb_walk_state_t *wsp)
{
	wchan_walk_data_t *ww = wsp->walk_data;
	sleepq_head_t *sq;
	kthread_t thr;
	uintptr_t t;
	int i;

again:
	/*
	 * Get the address of the first thread on the next sleepq in the
	 * sleepq hash.  If ww_compare is set, ww_sleepq_ndx is already
	 * set to the appropriate sleepq index for the desired cv.
	 */
	for (t = ww->ww_thr; t == NULL; ) {
		if (ww->ww_sleepq_ndx == NSLEEPQ)
			return (WALK_DONE);

		sq = &ww->ww_sleepq[ww->ww_sleepq_ndx++];
		t = (uintptr_t)sq->sq_queue.sq_first;

		/*
		 * If we were looking for a specific cv and we're at the end
		 * of its sleepq, we're done walking.
		 */
		if (t == NULL && ww->ww_compare != NULL)
			return (WALK_DONE);
	}

	/*
	 * Read in the thread.  If it's t_wchan pointer is NULL, the thread has
	 * woken up since we took a snapshot of the sleepq (i.e. we are probably
	 * being applied to a live system); we can't believe the t_link pointer
	 * anymore either, so just skip to the next sleepq index.
	 */
	if (mdb_vread(&thr, sizeof (thr), t) != sizeof (thr)) {
		mdb_warn("failed to read thread at %p", t);
		return (WALK_ERR);
	}

	if (thr.t_wchan == NULL) {
		ww->ww_thr = NULL;
		goto again;
	}

	/*
	 * Set ww_thr to the address of the next thread in the sleepq list.
	 */
	ww->ww_thr = (uintptr_t)thr.t_link;

	/*
	 * If we're walking a specific cv, invoke the callback if we've
	 * found a match, or loop back to the top and read the next thread.
	 */
	if (ww->ww_compare != NULL) {
		if (ww->ww_compare == (uintptr_t)thr.t_wchan)
			return (wsp->walk_callback(t, &thr, wsp->walk_cbdata));

		if (ww->ww_thr == NULL)
			return (WALK_DONE);

		goto again;
	}

	/*
	 * If we're walking all cvs, seen if we've already encountered this one
	 * on the current sleepq.  If we have, skip to the next thread.
	 */
	for (i = 0; i < ww->ww_seen_ndx; i++) {
		if (ww->ww_seen[i] == thr.t_wchan)
			goto again;
	}

	/*
	 * If we're not at the end of a sleepq, save t_wchan; otherwise reset
	 * the seen index so our array is empty at the start of the next sleepq.
	 * If we hit seen_size this is a live kernel and nthread is now larger,
	 * cope by replacing the final element in our memory.
	 */
	if (ww->ww_thr != NULL) {
		if (ww->ww_seen_ndx < ww->ww_seen_size)
			ww->ww_seen[ww->ww_seen_ndx++] = thr.t_wchan;
		else
			ww->ww_seen[ww->ww_seen_size - 1] = thr.t_wchan;
	} else
		ww->ww_seen_ndx = 0;

	return (wsp->walk_callback((uintptr_t)thr.t_wchan,
	    NULL, wsp->walk_cbdata));
}

void
wchan_walk_fini(mdb_walk_state_t *wsp)
{
	wchan_walk_data_t *ww = wsp->walk_data;

	mdb_free(ww->ww_seen, ww->ww_seen_size * sizeof (uintptr_t));
	mdb_free(ww, sizeof (wchan_walk_data_t));
}

struct wcdata {
	sobj_ops_t sobj;
	int nwaiters;
};

/*ARGSUSED*/
static int
wchaninfo_twalk(uintptr_t addr, const kthread_t *t, struct wcdata *wc)
{
	if (wc->sobj.sobj_type == SOBJ_NONE) {
		(void) mdb_vread(&wc->sobj, sizeof (sobj_ops_t),
		    (uintptr_t)t->t_sobj_ops);
	}

	wc->nwaiters++;
	return (WALK_NEXT);
}

static int
wchaninfo_vtwalk(uintptr_t addr, const kthread_t *t, int *first)
{
	proc_t p;

	(void) mdb_vread(&p, sizeof (p), (uintptr_t)t->t_procp);

	if (*first) {
		*first = 0;
		mdb_printf(":  %0?p %s\n", addr, p.p_user.u_comm);
	} else {
		mdb_printf("%*s%0?p %s\n", (int)(sizeof (uintptr_t) * 2 + 17),
		    "", addr, p.p_user.u_comm);
	}

	return (WALK_NEXT);
}

/*ARGSUSED*/
static int
wchaninfo_walk(uintptr_t addr, void *ignored, uint_t *verbose)
{
	struct wcdata wc;
	int first = 1;

	bzero(&wc, sizeof (wc));
	wc.sobj.sobj_type = SOBJ_NONE;

	if (mdb_pwalk("wchan", (mdb_walk_cb_t)wchaninfo_twalk, &wc, addr) < 0) {
		mdb_warn("failed to walk wchan %p", addr);
		return (WALK_NEXT);
	}

	mdb_printf("%0?p %4s %8d%s", addr,
	    wc.sobj.sobj_type == SOBJ_CV ? "cond" :
	    wc.sobj.sobj_type == SOBJ_SEMA ? "sema" : "??",
	    wc.nwaiters, (*verbose) ? "" : "\n");

	if (*verbose != 0 && wc.nwaiters != 0 && mdb_pwalk("wchan",
	    (mdb_walk_cb_t)wchaninfo_vtwalk, &first, addr) == -1) {
		mdb_warn("failed to walk waiters for wchan %p", addr);
		mdb_printf("\n");
	}

	return (WALK_NEXT);
}

int
wchaninfo(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uint_t v = FALSE;

	if (mdb_getopts(argc, argv,
	    'v', MDB_OPT_SETBITS, TRUE, &v, NULL) != argc)
		return (DCMD_USAGE);

	if (v == TRUE) {
		mdb_printf("%-?s %-4s %8s   %-?s %s\n",
		    "ADDR", "TYPE", "NWAITERS", "THREAD", "PROC");
	} else
		mdb_printf("%-?s %-4s %8s\n", "ADDR", "TYPE", "NWAITERS");

	if (flags & DCMD_ADDRSPEC) {
		if (wchaninfo_walk(addr, NULL, &v) == WALK_ERR)
			return (DCMD_ERR);
	} else if (mdb_walk("wchan", (mdb_walk_cb_t)wchaninfo_walk, &v) == -1) {
		mdb_warn("failed to walk wchans");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

int
blocked_walk_init(mdb_walk_state_t *wsp)
{
	if ((wsp->walk_data = (void *)wsp->walk_addr) == NULL) {
		mdb_warn("must specify a sobj * for blocked walk");
		return (WALK_ERR);
	}

	wsp->walk_addr = NULL;

	if (mdb_layered_walk("thread", wsp) == -1) {
		mdb_warn("couldn't walk 'thread'");
		return (WALK_ERR);
	}

	return (WALK_NEXT);
}

int
blocked_walk_step(mdb_walk_state_t *wsp)
{
	uintptr_t addr = (uintptr_t)((const kthread_t *)wsp->walk_layer)->t_ts;
	uintptr_t taddr = wsp->walk_addr;
	turnstile_t ts;

	if (mdb_vread(&ts, sizeof (ts), addr) == -1) {
		mdb_warn("couldn't read %p's turnstile at %p", taddr, addr);
		return (WALK_ERR);
	}

	if (ts.ts_waiters == 0 || ts.ts_sobj != wsp->walk_data)
		return (WALK_NEXT);

	return (wsp->walk_callback(taddr, wsp->walk_layer, wsp->walk_cbdata));
}

typedef struct rwlock_block {
	struct rwlock_block *rw_next;
	int rw_qnum;
	uintptr_t rw_thread;
} rwlock_block_t;

int
rwlock_walk(uintptr_t taddr, const kthread_t *t, rwlock_block_t **rwp)
{
	turnstile_t ts;
	uintptr_t addr = (uintptr_t)t->t_ts;
	rwlock_block_t *rw;
	int i;

	if (mdb_vread(&ts, sizeof (ts), addr) == -1) {
		mdb_warn("couldn't read %p's turnstile at %p", taddr, addr);
		return (WALK_ERR);
	}

	for (i = 0; i < TS_NUM_Q; i++) {
		if ((uintptr_t)t->t_sleepq ==
		    (uintptr_t)&ts.ts_sleepq[i] - (uintptr_t)&ts + addr)
			break;
	}

	if (i == TS_NUM_Q) {
		if (mdb_prop_postmortem) {
			/*
			 * This shouldn't happen post-mortem; the blocked walk
			 * returned a thread which wasn't actually blocked on
			 * its turnstile.  This may happen in-situ if the
			 * thread wakes up during the ::rwlock.
			 */
			mdb_warn("thread %p isn't blocked on ts %p\n",
			    taddr, addr);
			return (WALK_ERR);
		}

		return (WALK_NEXT);
	}

	rw = mdb_alloc(sizeof (rwlock_block_t), UM_SLEEP | UM_GC);

	rw->rw_next = *rwp;
	rw->rw_qnum = i;
	rw->rw_thread = taddr;
	*rwp = rw;

	return (WALK_NEXT);
}

#define	RW_BIT(n, offon) (wwwh & (1 << (n)) ? offon[1] : offon[0])
#define	RW_BIT_SET(n) (wwwh & (1 << (n)))

#ifdef _LP64
#define	RW_OWNR_WIDTH	16
#define	RW_ADDR_WIDTH	16
#else
#define	RW_OWNR_WIDTH	11
#define	RW_ADDR_WIDTH	8
#endif

#define	RW_LONG (RW_ADDR_WIDTH + RW_OWNR_WIDTH + 4)
#define	RW_ELBOW 18
#define	RW_SHORT (RW_ADDR_WIDTH + RW_OWNR_WIDTH - 1 - RW_ELBOW)
#define	RW_LONGLONG (RW_LONG + 4)

#define	RW_NEWLINE \
	if (rw != NULL) { \
		int q = rw->rw_qnum; \
		mdb_printf(" %?p (%s)", rw->rw_thread, \
		    q == TS_READER_Q ? "R" : q == TS_WRITER_Q ? "W" : "?"); \
		rw = rw->rw_next; \
	} \
	mdb_printf("\n");

/*ARGSUSED*/
int
rwlock(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	rwlock_impl_t lock;
	rwlock_block_t *rw = NULL;
	uintptr_t wwwh;

	if (!(flags & DCMD_ADDRSPEC) || addr == NULL)
		return (DCMD_USAGE);

	if (mdb_vread(&lock, sizeof (lock), addr) == -1) {
		mdb_warn("failed to read rwlock at 0x%p", addr);
		return (DCMD_ERR);
	}

	if (mdb_pwalk("blocked", (mdb_walk_cb_t)rwlock_walk, &rw, addr) == -1) {
		mdb_warn("couldn't walk 'blocked' for sobj %p", addr);
		return (WALK_ERR);
	}

	mdb_printf("%?s %*s %5s %?s\n", "ADDR",
	    RW_OWNR_WIDTH, "OWNER/COUNT", "FLAGS", "WAITERS");

	mdb_printf("%?p ", addr);

	if ((wwwh = lock.rw_wwwh) & RW_WRITE_LOCKED) {
		mdb_printf("%?p", wwwh & RW_OWNER);
	} else {
		char c[20];

		sprintf(c, "READERS=%ld",
		    (wwwh & RW_HOLD_COUNT) >> RW_HOLD_COUNT_SHIFT);
		mdb_printf("%*s", RW_OWNR_WIDTH, wwwh & RW_READ_LOCK ? c : "-");
	}

	mdb_printf("  B%c%c%c",
	    RW_BIT(2, "01"), RW_BIT(1, "01"), RW_BIT(0, "01"));
	RW_NEWLINE;

	mdb_printf("%*s%c%c%c", RW_LONG, "",
	    RW_BIT(2, " |"), RW_BIT(1, " |"), RW_BIT(0, " |"));
	RW_NEWLINE;

	if (!RW_BIT_SET(2))
		goto no_two;

	mdb_printf("%*s%*s ----+%c%c", RW_SHORT, "", RW_ELBOW,
	    "WRITE_LOCKED", RW_BIT(1, " |"), RW_BIT(0, " |"));
	RW_NEWLINE;

no_two:
	if (!RW_BIT_SET(1))
		goto no_one;

	mdb_printf("%*s%*s -----+%c", RW_SHORT, "", RW_ELBOW,
	    "WRITE_WANTED", RW_BIT(0, " |"));
	RW_NEWLINE;

no_one:
	if (!RW_BIT_SET(0))
		goto no_zero;

	mdb_printf("%*s%*s ------+", RW_SHORT, "", RW_ELBOW, "HAS_WAITERS");
	RW_NEWLINE;

no_zero:
	while (rw != NULL) {
		mdb_printf("%*s", RW_LONGLONG, "");
		RW_NEWLINE;
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
int
whereopen_fwalk(uintptr_t addr, struct file *f, uintptr_t *target)
{
	if ((uintptr_t)f->f_vnode == *target) {
		mdb_printf("file %p\n", addr);
		*target = NULL;
	}

	return (WALK_NEXT);
}

/*ARGSUSED*/
int
whereopen_pwalk(uintptr_t addr, void *ignored, uintptr_t *target)
{
	uintptr_t t = *target;

	if (mdb_pwalk("file", (mdb_walk_cb_t)whereopen_fwalk, &t, addr) == -1) {
		mdb_warn("couldn't file walk proc %p", addr);
		return (WALK_ERR);
	}

	if (t == NULL)
		mdb_printf("%p\n", addr);

	return (WALK_NEXT);
}

/*ARGSUSED*/
int
whereopen(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uintptr_t target = addr;

	if (!(flags & DCMD_ADDRSPEC) || addr == NULL)
		return (DCMD_USAGE);

	if (mdb_walk("proc", (mdb_walk_cb_t)whereopen_pwalk, &target) == -1) {
		mdb_warn("can't proc walk");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

typedef struct datafmt {
	char	*hdr1;
	char	*hdr2;
	char	*dashes;
	char	*fmt;
} datafmt_t;

static datafmt_t kmemfmt[] = {
	{ "cache                    ", "name                     ",
	"-------------------------", "%-25s "				},
	{ "   buf",	"  size",	"------",	"%6u "		},
	{ "   buf",	"in use",	"------",	"%6u "		},
	{ "   buf",	" total",	"------",	"%6u "		},
	{ "   memory",	"   in use",	"---------",	"%9u "		},
	{ "    alloc",	"  succeed",	"---------",	"%9u "		},
	{ "alloc",	" fail",	"-----",	"%5u "		},
	{ NULL,		NULL,		NULL,		NULL		}
};

static datafmt_t vmemfmt[] = {
	{ "vmem                     ", "name                     ",
	"-------------------------", "%-*s "				},
	{ "   memory",	"   in use",	"---------",	"%9llu "	},
	{ "    memory",	"     total",	"----------",	"%10llu "	},
	{ "   memory",	"   import",	"---------",	"%9llu "	},
	{ "    alloc",	"  succeed",	"---------",	"%9llu "	},
	{ "alloc",	" fail",	"-----",	"%5llu "	},
	{ NULL,		NULL,		NULL,		NULL		}
};

/*ARGSUSED*/
static int
kmastat_cpu_avail(uintptr_t addr, const kmem_cpu_cache_t *ccp, int *avail)
{
	if (ccp->cc_rounds > 0)
		*avail += ccp->cc_rounds;
	if (ccp->cc_full_mag)
		*avail += ccp->cc_magsize;

	return (WALK_NEXT);
}

/*ARGSUSED*/
static int
kmastat_cpu_alloc(uintptr_t addr, const kmem_cpu_cache_t *ccp, int *alloc)
{
	*alloc += ccp->cc_alloc;

	return (WALK_NEXT);
}

/*ARGSUSED*/
static int
kmastat_slab_avail(uintptr_t addr, const kmem_slab_t *sp, int *avail)
{
	*avail += sp->slab_chunks - sp->slab_refcnt;

	return (WALK_NEXT);
}

typedef struct kmastat_vmem {
	uintptr_t kv_addr;
	struct kmastat_vmem *kv_next;
	int kv_meminuse;
	int kv_alloc;
	int kv_fail;
} kmastat_vmem_t;

static int
kmastat_cache(uintptr_t addr, const kmem_cache_t *cp, kmastat_vmem_t **kvp)
{
	kmastat_vmem_t *kv;
	datafmt_t *dfp = kmemfmt;
	int avail, alloc, total;
	size_t meminuse = (cp->cache_slab_create - cp->cache_slab_destroy) *
	    cp->cache_slabsize;

	mdb_walk_cb_t cpu_avail = (mdb_walk_cb_t)kmastat_cpu_avail;
	mdb_walk_cb_t cpu_alloc = (mdb_walk_cb_t)kmastat_cpu_alloc;
	mdb_walk_cb_t slab_avail = (mdb_walk_cb_t)kmastat_slab_avail;

	alloc = cp->cache_global_alloc + cp->cache_depot_alloc;
	avail = cp->cache_fmag_total * cp->cache_magazine_size;
	total = cp->cache_buftotal;

	(void) mdb_pwalk("kmem_cpu_cache", cpu_alloc, &alloc, addr);
	(void) mdb_pwalk("kmem_cpu_cache", cpu_avail, &avail, addr);
	(void) mdb_pwalk("kmem_slab", slab_avail, &avail, addr);

	for (kv = *kvp; kv != NULL; kv = kv->kv_next) {
		if (kv->kv_addr == (uintptr_t)cp->cache_arena)
			goto out;
	}

	kv = mdb_zalloc(sizeof (kmastat_vmem_t), UM_SLEEP | UM_GC);
	kv->kv_next = *kvp;
	kv->kv_addr = (uintptr_t)cp->cache_arena;
	*kvp = kv;
out:
	kv->kv_meminuse += meminuse;
	kv->kv_alloc += alloc;
	kv->kv_fail += cp->cache_alloc_fail;

	mdb_printf((dfp++)->fmt, cp->cache_name);
	mdb_printf((dfp++)->fmt, cp->cache_bufsize);
	mdb_printf((dfp++)->fmt, total - avail);
	mdb_printf((dfp++)->fmt, total);
	mdb_printf((dfp++)->fmt, meminuse);
	mdb_printf((dfp++)->fmt, alloc);
	mdb_printf((dfp++)->fmt, cp->cache_alloc_fail);
	mdb_printf("\n");

	return (WALK_NEXT);
}

static int
kmastat_vmem_totals(uintptr_t addr, const vmem_t *v, kmastat_vmem_t *kv)
{
	while (kv != NULL && kv->kv_addr != addr)
		kv = kv->kv_next;

	if (kv == NULL || kv->kv_alloc == 0)
		return (WALK_NEXT);

	mdb_printf("Total [%s]%*s %6s %6s %6s %9u %9u %5u\n", v->vm_name,
	    17 - strlen(v->vm_name), "", "", "", "",
	    kv->kv_meminuse, kv->kv_alloc, kv->kv_fail);

	return (WALK_NEXT);
}

/*ARGSUSED*/
static int
kmastat_vmem(uintptr_t addr, const vmem_t *v, void *ignored)
{
	datafmt_t *dfp = vmemfmt;
	const vmem_kstat_t *vkp = &v->vm_kstat;
	uintptr_t paddr;
	vmem_t parent;
	int ident = 0;

	for (paddr = (uintptr_t)v->vm_source; paddr != NULL; ident += 4) {
		if (mdb_vread(&parent, sizeof (parent), paddr) == -1) {
			mdb_warn("couldn't trace %p's ancestry", addr);
			ident = 0;
			break;
		}
		paddr = (uintptr_t)parent.vm_source;
	}

	mdb_printf("%*s", ident, "");
	mdb_printf((dfp++)->fmt, 25 - ident, v->vm_name);
	mdb_printf((dfp++)->fmt, vkp->vk_mem_inuse.value.ui64);
	mdb_printf((dfp++)->fmt, vkp->vk_mem_total.value.ui64);
	mdb_printf((dfp++)->fmt, vkp->vk_mem_import.value.ui64);
	mdb_printf((dfp++)->fmt, vkp->vk_alloc.value.ui64);
	mdb_printf((dfp++)->fmt, vkp->vk_fail.value.ui64);

	mdb_printf("\n");

	return (WALK_NEXT);
}

/*ARGSUSED*/
int
kmastat(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	kmastat_vmem_t *kv = NULL;
	datafmt_t *dfp;

	if (argc != 0)
		return (DCMD_USAGE);

	for (dfp = kmemfmt; dfp->hdr1 != NULL; dfp++)
		mdb_printf("%s ", dfp->hdr1);
	mdb_printf("\n");

	for (dfp = kmemfmt; dfp->hdr1 != NULL; dfp++)
		mdb_printf("%s ", dfp->hdr2);
	mdb_printf("\n");

	for (dfp = kmemfmt; dfp->hdr1 != NULL; dfp++)
		mdb_printf("%s ", dfp->dashes);
	mdb_printf("\n");

	if (mdb_walk("kmem_cache", (mdb_walk_cb_t)kmastat_cache, &kv) == -1) {
		mdb_warn("can't walk 'kmem_cache'");
		return (DCMD_ERR);
	}

	for (dfp = kmemfmt; dfp->hdr1 != NULL; dfp++)
		mdb_printf("%s ", dfp->dashes);
	mdb_printf("\n");

	if (mdb_walk("vmem", (mdb_walk_cb_t)kmastat_vmem_totals, kv) == -1) {
		mdb_warn("can't walk 'vmem'");
		return (DCMD_ERR);
	}

	for (dfp = kmemfmt; dfp->hdr1 != NULL; dfp++)
		mdb_printf("%s ", dfp->dashes);
	mdb_printf("\n");

	mdb_printf("\n");

	for (dfp = vmemfmt; dfp->hdr1 != NULL; dfp++)
		mdb_printf("%s ", dfp->hdr1);
	mdb_printf("\n");

	for (dfp = vmemfmt; dfp->hdr1 != NULL; dfp++)
		mdb_printf("%s ", dfp->hdr2);
	mdb_printf("\n");

	for (dfp = vmemfmt; dfp->hdr1 != NULL; dfp++)
		mdb_printf("%s ", dfp->dashes);
	mdb_printf("\n");

	if (mdb_walk("vmem", (mdb_walk_cb_t)kmastat_vmem, NULL) == -1) {
		mdb_warn("can't walk 'vmem'");
		return (DCMD_ERR);
	}

	for (dfp = vmemfmt; dfp->hdr1 != NULL; dfp++)
		mdb_printf("%s ", dfp->dashes);
	mdb_printf("\n");
	return (DCMD_OK);
}

typedef struct kgrep_walk_data {
	uintptr_t *kg_where;
	size_t kg_ndx;
	size_t kg_size;
	uintptr_t *kg_page;
	uintptr_t kg_pattern;
} kgrep_walk_data_t;

/*ARGSUSED*/
static int
kgrep_walk_seg(uintptr_t segaddr, const struct seg *seg, kgrep_walk_data_t *kg)
{
	size_t addr_per = PAGESIZE / sizeof (uintptr_t);
	uintptr_t lim = (uintptr_t)seg->s_base + seg->s_size;
	uintptr_t addr, *new;
	size_t i;

	for (addr = (uintptr_t)seg->s_base; addr < lim; addr += PAGESIZE) {
		if (mdb_vread(kg->kg_page, PAGESIZE, addr) == -1)
			continue;

		for (i = 0; i < addr_per; i++) {
			size_t sz = kg->kg_size;

			if (kg->kg_page[i] != kg->kg_pattern)
				continue;

			kg->kg_where[kg->kg_ndx++] =
			    addr + i * sizeof (uintptr_t);

			if (kg->kg_ndx < sz)
				continue;

			new = mdb_zalloc(sizeof (uintptr_t) *
			    (sz << 1), UM_SLEEP);

			bcopy(kg->kg_where, new, sizeof (uintptr_t) * sz);
			mdb_free(kg->kg_where, sz);

			kg->kg_where = new;
			kg->kg_size <<= 1;
		}
	}

	return (WALK_NEXT);
}

int
kgrep_walk_init(mdb_walk_state_t *wsp)
{
	GElf_Sym kas;
	kgrep_walk_data_t *kg;

	if (!mdb_prop_postmortem) {
		mdb_printf("kgrep can only be run on a system "
		    "dump; see dumpadm(1M)\n");
		return (WALK_ERR);
	}

	if (wsp->walk_addr == NULL) {
		mdb_warn("kgrep needs an address\n");
		return (WALK_ERR);
	}

	if (mdb_lookup_by_name("kas", &kas) == -1) {
		mdb_warn("failed to locate 'kas' symbol\n");
		return (WALK_ERR);
	}

	kg = mdb_zalloc(sizeof (kgrep_walk_data_t), UM_SLEEP);
	kg->kg_page = mdb_alloc(PAGESIZE, UM_SLEEP | UM_GC);
	kg->kg_where = mdb_zalloc(sizeof (uintptr_t), UM_SLEEP);
	kg->kg_size = 1;
	kg->kg_pattern = wsp->walk_addr;

	if (mdb_pwalk("seg", (mdb_walk_cb_t)kgrep_walk_seg,
	    kg, kas.st_value) == -1) {
		mdb_warn("failed to walk kas segments");
		return (WALK_ERR);
	}

	kg->kg_ndx = 0;
	wsp->walk_data = kg;

	return (WALK_NEXT);
}

int
kgrep_walk_step(mdb_walk_state_t *wsp)
{
	kgrep_walk_data_t *kg = wsp->walk_data;

	if (kg->kg_ndx == kg->kg_size || kg->kg_where[kg->kg_ndx] == NULL)
		return (WALK_DONE);

	return (wsp->walk_callback(kg->kg_where[kg->kg_ndx++],
	    NULL, wsp->walk_cbdata));
}

void
kgrep_walk_fini(mdb_walk_state_t *wsp)
{
	kgrep_walk_data_t *kg = wsp->walk_data;

	mdb_free(kg->kg_where, kg->kg_size);
	mdb_free(kg, sizeof (kgrep_walk_data_t));
}

/*ARGSUSED*/
int
kgrep_walk(uintptr_t addr, void *buf, void *ignored)
{
	mdb_printf("%p\n", addr);
	return (WALK_NEXT);
}

/*ARGSUSED*/
int
kgrep(uintptr_t pat, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb_pwalk("kgrep", (mdb_walk_cb_t)kgrep_walk, NULL, pat) == -1) {
		mdb_warn("couldn't walk kgrep");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

typedef struct file_walk_data {
	struct uf_entry *fw_flist;
	int fw_flistsz;
	int fw_ndx;
	int fw_nofiles;
} file_walk_data_t;

int
file_walk_init(mdb_walk_state_t *wsp)
{
	file_walk_data_t *fw;
	proc_t p;

	if (wsp->walk_addr == NULL) {
		mdb_printf("file walk doesn't support global walks\n");
		return (WALK_ERR);
	}

	fw = mdb_alloc(sizeof (file_walk_data_t), UM_SLEEP);

	if (mdb_vread(&p, sizeof (p), wsp->walk_addr) == -1) {
		mdb_warn("failed to read proc structure at %p", wsp->walk_addr);
		return (WALK_ERR);
	}

	fw->fw_nofiles = p.p_user.u_finfo.fi_nfiles;
	fw->fw_flistsz = sizeof (struct uf_entry) * fw->fw_nofiles;
	fw->fw_flist = mdb_alloc(fw->fw_flistsz, UM_SLEEP);

	if (mdb_vread(fw->fw_flist, fw->fw_flistsz,
	    (uintptr_t)p.p_user.u_finfo.fi_list) == -1) {
		mdb_warn("failed to read file array at %p",
		    p.p_user.u_finfo.fi_list);
		mdb_free(fw->fw_flist, fw->fw_flistsz);
		return (WALK_ERR);
	}

	fw->fw_ndx = 0;
	wsp->walk_data = fw;

	return (WALK_NEXT);
}

int
file_walk_step(mdb_walk_state_t *wsp)
{
	file_walk_data_t *fw = (file_walk_data_t *)wsp->walk_data;
	struct file file;
	uintptr_t fp;

again:
	if (fw->fw_ndx == fw->fw_nofiles)
		return (WALK_DONE);

	if ((fp = (uintptr_t)fw->fw_flist[fw->fw_ndx++].uf_file) == NULL)
		goto again;

	(void) mdb_vread(&file, sizeof (file), (uintptr_t)fp);
	return (wsp->walk_callback(fp, &file, wsp->walk_cbdata));
}

void
file_walk_fini(mdb_walk_state_t *wsp)
{
	file_walk_data_t *fw = (file_walk_data_t *)wsp->walk_data;

	mdb_free(fw->fw_flist, fw->fw_flistsz);
	mdb_free(fw, sizeof (file_walk_data_t));
}

typedef struct proc_walk_data {
	uintptr_t *pw_stack;
	int pw_depth;
	int pw_max;
} proc_walk_data_t;

int
proc_walk_init(mdb_walk_state_t *wsp)
{
	GElf_Sym sym;
	proc_walk_data_t *pw;

	if (wsp->walk_addr == NULL) {
		if (mdb_lookup_by_name("p0", &sym) == -1) {
			mdb_warn("failed to read 'practive'");
			return (WALK_ERR);
		}
		wsp->walk_addr = (uintptr_t)sym.st_value;
	}

	pw = mdb_zalloc(sizeof (proc_walk_data_t), UM_SLEEP);

	if (mdb_readvar(&pw->pw_max, "nproc") == -1) {
		mdb_warn("failed to read 'nproc'");
		mdb_free(pw, sizeof (pw));
		return (WALK_ERR);
	}

	pw->pw_stack = mdb_alloc(pw->pw_max * sizeof (uintptr_t), UM_SLEEP);
	wsp->walk_data = pw;

	return (WALK_NEXT);
}

int
proc_walk_step(mdb_walk_state_t *wsp)
{
	proc_walk_data_t *pw = wsp->walk_data;
	uintptr_t addr = wsp->walk_addr;

	int status;
	proc_t pr;

	if (mdb_vread(&pr, sizeof (proc_t), addr) == -1) {
		mdb_warn("failed to read proc at %p", addr);
		return (WALK_DONE);
	}

	if (pw->pw_depth > 0 && addr == pw->pw_stack[pw->pw_depth - 1]) {
		pw->pw_depth--;
		goto sib;
	}

	status = wsp->walk_callback(addr, &pr, wsp->walk_cbdata);

	if (status != WALK_NEXT)
		return (status);

	if ((wsp->walk_addr = (uintptr_t)pr.p_child) != NULL) {
		pw->pw_stack[pw->pw_depth++] = addr;

		if (pw->pw_depth == pw->pw_max) {
			mdb_warn("depth %d exceeds max depth; try again\n",
			    pw->pw_depth);
			return (WALK_DONE);
		}
		return (WALK_NEXT);
	}

sib:
	/*
	 * We know that p0 has no siblings, and if another starting proc
	 * was given, we don't want to walk its siblings anyway.
	 */
	if (pw->pw_depth == 0)
		return (WALK_DONE);

	if ((wsp->walk_addr = (uintptr_t)pr.p_sibling) == NULL) {
		if (pw->pw_depth > 0) {
			wsp->walk_addr = pw->pw_stack[pw->pw_depth - 1];
			return (WALK_NEXT);
		}
		return (WALK_DONE);
	}

	return (WALK_NEXT);
}

void
proc_walk_fini(mdb_walk_state_t *wsp)
{
	proc_walk_data_t *pw = wsp->walk_data;

	mdb_free(pw->pw_stack, pw->pw_max * sizeof (uintptr_t));
	mdb_free(pw, sizeof (proc_walk_data_t));
}

typedef struct thread_walk {
	kthread_t *tw_thread;
	uintptr_t tw_last;
	uint_t tw_inproc;
	uint_t tw_step;
} thread_walk_t;

int
thread_walk_init(mdb_walk_state_t *wsp)
{
	thread_walk_t *twp = mdb_alloc(sizeof (thread_walk_t), UM_SLEEP);

	if (wsp->walk_addr == NULL) {
		if (mdb_readvar(&wsp->walk_addr, "allthreads") == -1) {
			mdb_warn("failed to read 'allthreads'");
			mdb_free(twp, sizeof (thread_walk_t));
			return (WALK_ERR);
		}

		twp->tw_inproc = FALSE;

	} else {
		proc_t pr;

		if (mdb_vread(&pr, sizeof (proc_t), wsp->walk_addr) == -1) {
			mdb_warn("failed to read proc at %p", wsp->walk_addr);
			mdb_free(twp, sizeof (thread_walk_t));
			return (WALK_ERR);
		}

		wsp->walk_addr = (uintptr_t)pr.p_tlist;
		twp->tw_inproc = TRUE;
	}

	twp->tw_thread = mdb_alloc(sizeof (kthread_t), UM_SLEEP);
	twp->tw_last = wsp->walk_addr;
	twp->tw_step = FALSE;

	wsp->walk_data = twp;
	return (WALK_NEXT);
}

int
thread_walk_step(mdb_walk_state_t *wsp)
{
	thread_walk_t *twp = (thread_walk_t *)wsp->walk_data;
	int status;

	if (wsp->walk_addr == NULL)
		return (WALK_DONE); /* Proc has 0 threads or allthreads = 0 */

	if (twp->tw_step && wsp->walk_addr == twp->tw_last)
		return (WALK_DONE); /* We've wrapped around */

	if (mdb_vread(twp->tw_thread, sizeof (kthread_t),
	    wsp->walk_addr) == -1) {
		mdb_warn("failed to read thread at %p", wsp->walk_addr);
		return (WALK_DONE);
	}

	status = wsp->walk_callback(wsp->walk_addr, twp->tw_thread,
	    wsp->walk_cbdata);

	if (twp->tw_inproc)
		wsp->walk_addr = (uintptr_t)twp->tw_thread->t_forw;
	else
		wsp->walk_addr = (uintptr_t)twp->tw_thread->t_next;

	twp->tw_step = TRUE;
	return (status);
}

void
thread_walk_fini(mdb_walk_state_t *wsp)
{
	thread_walk_t *twp = (thread_walk_t *)wsp->walk_data;

	mdb_free(twp->tw_thread, sizeof (thread_walk_t));
	mdb_free(twp, sizeof (thread_walk_t));
}

int
deathrow_walk_init(mdb_walk_state_t *wsp)
{
	if (mdb_layered_walk("thread_deathrow", wsp) == -1) {
		mdb_warn("couldn't walk 'thread_deathrow'");
		return (WALK_ERR);
	}

	if (mdb_layered_walk("lwp_deathrow", wsp) == -1) {
		mdb_warn("couldn't walk 'lwp_deathrow'");
		return (WALK_ERR);
	}

	return (WALK_NEXT);
}

int
deathrow_walk_step(mdb_walk_state_t *wsp)
{
	kthread_t t;
	uintptr_t addr = wsp->walk_addr;

	if (addr == NULL)
		return (WALK_DONE);

	if (mdb_vread(&t, sizeof (t), addr) == -1) {
		mdb_warn("couldn't read deathrow thread at %p", addr);
		return (WALK_ERR);
	}

	wsp->walk_addr = (uintptr_t)t.t_forw;

	return (wsp->walk_callback(addr, &t, wsp->walk_cbdata));
}

int
thread_deathrow_walk_init(mdb_walk_state_t *wsp)
{
	if (mdb_readvar(&wsp->walk_addr, "thread_deathrow") == -1) {
		mdb_warn("couldn't read symbol 'thread_deathrow'");
		return (WALK_ERR);
	}

	return (WALK_NEXT);
}

int
lwp_deathrow_walk_init(mdb_walk_state_t *wsp)
{
	if (mdb_readvar(&wsp->walk_addr, "lwp_deathrow") == -1) {
		mdb_warn("couldn't read symbol 'lwp_deathrow'");
		return (WALK_ERR);
	}

	return (WALK_NEXT);
}

typedef struct seg_walk {
	struct seg *sw_seg;
	uintptr_t (*sw_next)(const struct seg *);
	uintptr_t sw_last;
} seg_walk_t;

uintptr_t
seg_walk_linklist(const struct seg *segp)
{
	return ((uintptr_t)segp->s_next.list);
}

uintptr_t
seg_walk_skiplist(const struct seg *segp)
{
	seg_skiplist ssl;

	if (mdb_vread(&ssl, sizeof (ssl),
	    (uintptr_t)segp->s_next.skiplist) == -1)
		return (NULL);

	return ((uintptr_t)ssl.segs[0]);
}

int
seg_walk_init(mdb_walk_state_t *wsp)
{
	seg_walk_t *swp;
	seg_skiplist ssl;
	struct as as;

	if (wsp->walk_addr == NULL) {
		mdb_warn("seg walk must begin at struct as *\n");
		return (WALK_ERR);
	}

	if (mdb_vread(&as, sizeof (as), wsp->walk_addr) == -1) {
		mdb_warn("failed to read as at %p", wsp->walk_addr);
		return (WALK_ERR);
	}

	if (as.a_lrep != AS_LREP_LINKEDLIST && as.a_lrep != AS_LREP_SKIPLIST) {
		mdb_warn("invalid as.a_lrep 0x%x\n", as.a_lrep);
		return (WALK_ERR);
	}

	if (as.a_lrep == AS_LREP_SKIPLIST) {
		if (mdb_vread(&ssl, sizeof (ssl),
		    (uintptr_t)as.a_segs.skiplist) == -1) {
			mdb_warn("failed to read skiplist at %p",
			    as.a_segs.skiplist);
			return (WALK_ERR);
		}
	}

	swp = mdb_alloc(sizeof (seg_walk_t), UM_SLEEP);
	swp->sw_seg = mdb_alloc(sizeof (struct seg), UM_SLEEP);

	if (as.a_lrep == AS_LREP_LINKEDLIST) {
		swp->sw_next = seg_walk_linklist;
		wsp->walk_addr = (uintptr_t)as.a_segs.list;
	} else {
		swp->sw_next = seg_walk_skiplist;
		wsp->walk_addr = (uintptr_t)ssl.segs[0];
	}

	swp->sw_last = (uintptr_t)as.a_tail;
	wsp->walk_data = swp;

	return (WALK_NEXT);
}

int
seg_walk_step(mdb_walk_state_t *wsp)
{
	seg_walk_t *swp = (seg_walk_t *)wsp->walk_data;
	int status;

	if (wsp->walk_addr == NULL)
		return (WALK_DONE);

	if (mdb_vread(swp->sw_seg, sizeof (struct seg), wsp->walk_addr) == -1) {
		mdb_warn("failed to read seg at %p", wsp->walk_addr);
		return (WALK_DONE);
	}

	status = wsp->walk_callback(wsp->walk_addr, swp->sw_seg,
	    wsp->walk_cbdata);

	if (wsp->walk_addr != swp->sw_last)
		wsp->walk_addr = swp->sw_next(swp->sw_seg);
	else
		wsp->walk_addr = NULL;

	return (status);
}

void
seg_walk_fini(mdb_walk_state_t *wsp)
{
	seg_walk_t *swp = (seg_walk_t *)wsp->walk_data;

	mdb_free(swp->sw_seg, sizeof (struct seg));
	mdb_free(swp, sizeof (seg_walk_t));
}

typedef struct cpu_walk {
	uintptr_t cw_end;
	uintptr_t cw_current;
} cpu_walk_t;

int
cpu_walk_init(mdb_walk_state_t *wsp)
{
	GElf_Sym sym;
	const char *name[] = { "stod_cpu0", "cpu0", "cpus", NULL };
	cpu_walk_t *cw;
	int i;

	for (i = 0; name[i] != NULL; i++)
		if (mdb_lookup_by_name(name[i], &sym) != -1)
			break;

	if (name[i] == NULL) {
		mdb_warn("failed to locate first CPU\n");
		return (WALK_ERR);
	}

	cw = mdb_alloc(sizeof (cpu_walk_t), UM_SLEEP);
	cw->cw_end = cw->cw_current = (uintptr_t)sym.st_value;

	wsp->walk_data = cw;
	return (WALK_NEXT);
}

int
cpu_walk_step(mdb_walk_state_t *wsp)
{
	cpu_walk_t *cw = wsp->walk_data;
	cpu_t cpu;
	int status;

	if (mdb_vread(&cpu, sizeof (cpu), cw->cw_current) == -1) {
		mdb_warn("failed to read cpu at %p", cw->cw_current);
		return (WALK_DONE);
	}

	status = wsp->walk_callback(cw->cw_current, &cpu, wsp->walk_cbdata);
	cw->cw_current = (uintptr_t)cpu.cpu_next;

	if (cw->cw_current == cw->cw_end)
		return (WALK_DONE);

	return (status);
}

void
cpu_walk_fini(mdb_walk_state_t *wsp)
{
	mdb_free(wsp->walk_data, sizeof (cpu_walk_t));
}

typedef struct cpuinfo_data {
	int	cid_flags;
	int	cid_cpu;
	int	cid_ncpus;
	clock_t	cid_lbolt;
	uintptr_t **cid_ithr;
	char	cid_print_head;
	char	cid_print_thr;
	char	cid_print_ithr;
	char	cid_print_orphans;
	char	cid_found_orphans;
} cpuinfo_data_t;

int
cpuinfo_walk_ithread(uintptr_t addr, const kthread_t *thr, cpuinfo_data_t *cid)
{
	cpu_t c;
	int id;
	uint8_t pil;

	if (!(thr->t_flag & T_INTR_THREAD) || thr->t_state == TS_FREE)
		return (WALK_NEXT);

	if (thr->t_bound_cpu == NULL) {
		mdb_warn("thr %p is intr thread w/out a CPU\n", addr);
		return (WALK_NEXT);
	}

	mdb_vread(&c, sizeof (c), (uintptr_t)thr->t_bound_cpu);

	if ((id = c.cpu_id) >= NCPU) {
		mdb_warn("CPU %p has id (%d) greater than NCPU (%d)\n",
		    thr->t_bound_cpu, id, NCPU);
		return (WALK_NEXT);
	}

	if ((pil = thr->t_pil) >= NINTR) {
		mdb_warn("thread %p has pil (%d) greater than %d\n",
		    addr, pil, NINTR);
		return (WALK_NEXT);
	}

	if (cid->cid_ithr[id][pil] != NULL) {
		mdb_warn("CPU %d has multiple threads at pil %d (at least "
		    "%p and %p)\n", id, pil, addr, cid->cid_ithr[id][pil]);
		return (WALK_NEXT);
	}

	cid->cid_ithr[id][pil] = addr;

	return (WALK_NEXT);
}

int
cpuinfo_walk_cpu(uintptr_t addr, const cpu_t *cpu, cpuinfo_data_t *cid)
{
	const int CPUINFO_THRDELT = 19;
	const int CPUINFO_ITHRDELT = 4;

	kthread_t t;
	proc_t p;

	if (cid->cid_print_head) {
		mdb_printf("%2s %-8s %3s %4s %4s %3s %4s %5s %-6s %-?s %s\n",
		    "ID", "ADDR", "FLG", "NRUN", "BSPL", "PRI", "RNRN",
		    "KRNRN", "SWITCH", "THREAD", "PROC");
		cid->cid_print_head = FALSE;
	}

	mdb_vread(&t, sizeof (t), (uintptr_t)cpu->cpu_thread);

	mdb_printf("%2d %08x %3x %4d %4d %3d %4s %5s t-%-4d %0?p",
	    cpu->cpu_id, addr, cpu->cpu_flags, cpu->cpu_disp.disp_nrunnable,
	    cpu->cpu_base_spl, t.t_pri, cpu->cpu_runrun ? "yes" : "no",
	    cpu->cpu_kprunrun ? "yes" : "no",
	    cid->cid_lbolt - cpu->cpu_last_swtch, cpu->cpu_thread);

	if (cpu->cpu_thread == cpu->cpu_idle_thread)
		mdb_printf(" (idle)\n");
	else {
		mdb_vread(&p, sizeof (p), (uintptr_t)t.t_procp);
		mdb_printf(" %s\n", p.p_user.u_comm);
	}

	if (cid->cid_print_ithr) {
		int i, found_one = FALSE;
		int print_thr = cpu->cpu_disp.disp_nrunnable &&
		    cid->cid_print_thr;

		for (i = 0; i < NINTR; i++) {
			uintptr_t iaddr = cid->cid_ithr[cpu->cpu_id][i];

			if (iaddr == NULL)
				continue;

			if (!found_one) {
				found_one = TRUE;
				mdb_printf("%*s%c%*s|\n", CPUINFO_THRDELT, "",
				    print_thr ? '|' : ' ',
				    CPUINFO_ITHRDELT, "");
				mdb_printf("%*s%c%*s+--> %3s %-?s\n",
				    CPUINFO_THRDELT, "", print_thr ? '|' : ' ',
				    CPUINFO_ITHRDELT, "", "PIL", "THREAD");
			}

			mdb_vread(&t, sizeof (t), iaddr);

			mdb_printf("%*s%c%*s     %3d %0?p\n", CPUINFO_THRDELT,
			    "", print_thr ? '|' : ' ', CPUINFO_ITHRDELT, "",
			    t.t_pil, iaddr);
		}
		cid->cid_print_head = TRUE;

		if (!print_thr)
			mdb_printf("\n");
	}

	if (cpu->cpu_disp.disp_nrunnable && cid->cid_print_thr) {
		dispq_t *dq;

		int i, npri = cpu->cpu_disp.disp_npri;

		dq = mdb_alloc(sizeof (dispq_t) * npri, UM_SLEEP | UM_GC);

		mdb_vread(dq, sizeof (dispq_t) * npri,
		    (uintptr_t)cpu->cpu_disp.disp_q);

		mdb_printf("%*s|\n%*s+-->  %3s %-?s %s\n", CPUINFO_THRDELT,
		    "", CPUINFO_THRDELT, "", "PRI", "THREAD", "PROC");

		for (i = npri - 1; i >= 0; i--) {
			uintptr_t taddr = (uintptr_t)dq[i].dq_first;

			while (taddr != NULL) {
				mdb_vread(&t, sizeof (t), taddr);
				mdb_vread(&p, sizeof (p), (uintptr_t)t.t_procp);

				mdb_printf("%*s      %3d %0?p %s\n",
				    CPUINFO_THRDELT, "", t.t_pri, taddr,
				    p.p_user.u_comm);

				taddr = (uintptr_t)t.t_link;
			}
		}
		mdb_printf("\n");
		cid->cid_print_head = TRUE;
	}
	return (WALK_NEXT);
}

int
cpuinfo(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uint_t verbose = FALSE, ithr = FALSE;
	cpuinfo_data_t cid;

	if (!(flags & DCMD_ADDRSPEC))
		cid.cid_cpu = (int)addr;

	if (mdb_getopts(argc, argv,
	    'v', MDB_OPT_SETBITS, TRUE, &verbose, NULL) != argc)
		return (DCMD_USAGE);

	if (verbose)
		ithr = TRUE;

	cid.cid_print_ithr = FALSE;
	cid.cid_print_thr = FALSE;

	if (ithr) {
		int i;

		cid.cid_ithr = mdb_alloc(sizeof (uintptr_t **)
		    * NCPU, UM_SLEEP | UM_GC);

		for (i = 0; i < NCPU; i++)
			cid.cid_ithr[i] = mdb_zalloc(sizeof (uintptr_t *) *
			    NINTR, UM_SLEEP | UM_GC);

		if (mdb_walk("thread", (mdb_walk_cb_t)cpuinfo_walk_ithread,
		    &cid) == -1) {
			mdb_printf("couldn't walk thread");
			return (DCMD_ERR);
		}

		cid.cid_print_ithr = TRUE;
	}

	if (verbose)
		cid.cid_print_thr = TRUE;

	cid.cid_print_head = TRUE;

	if (mdb_readvar(&cid.cid_lbolt, "panic_lbolt") == -1) {
		/*
		 * This is weak.  If we couldn't read panic_lbolt, then
		 * this is probably 2.6; exploit the bug in 2.6 that lbolt64
		 * isn't bumped after panicstr is set.
		 */
		uint64_t lbolt64;
		if (mdb_readvar(&lbolt64, "lbolt64") == -1) {
			mdb_warn("failed to read panic_lbolt");
			return (DCMD_ERR);
		}
		cid.cid_lbolt = (clock_t)lbolt64;
	}

	if (cid.cid_lbolt == 0) {
		if (mdb_readvar(&cid.cid_lbolt, "lbolt") == -1) {
			mdb_warn("failed to read lbolt");
			return (DCMD_ERR);
		}
	}

	if (mdb_walk("cpu", (mdb_walk_cb_t)cpuinfo_walk_cpu, &cid) == -1) {
		mdb_warn("can't walk cpus");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
int
flipone(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	int i;

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	for (i = 0; i < sizeof (addr) * NBBY; i++)
		mdb_printf("%p\n", (addr & ~(1 << i)) | (addr ^ (1 << i)));

	return (DCMD_OK);
}

/*
 * Grumble, grumble.
 */
#define	SMAP_HASHFUNC(vp, off)	\
	((((uintptr_t)(vp) >> 6) + ((uintptr_t)(vp) >> 3) + \
	((off) >> MAXBSHIFT)) & smd_hashmsk)

int
vnode2smap(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	long smd_hashmsk;
	int hash;
	uintptr_t offset = 0;
	struct smap smp;
	uintptr_t saddr, kaddr;
	uintptr_t smd_hash, smd_smap;
	struct seg seg;

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb_readvar(&smd_hashmsk, "smd_hashmsk") == -1) {
		mdb_warn("failed to read smd_hashmsk");
		return (DCMD_ERR);
	}

	if (mdb_readvar(&smd_hash, "smd_hash") == -1) {
		mdb_warn("failed to read smd_hash");
		return (DCMD_ERR);
	}

	if (mdb_readvar(&smd_smap, "smd_smap") == -1) {
		mdb_warn("failed to read smd_hash");
		return (DCMD_ERR);
	}

	if (mdb_readvar(&kaddr, "segkmap") == -1) {
		mdb_warn("failed to read segkmap");
		return (DCMD_ERR);
	}

	if (mdb_vread(&seg, sizeof (seg), kaddr) == -1) {
		mdb_warn("failed to read segkmap at %p", kaddr);
		return (DCMD_ERR);
	}

	if (argc != 0) {
		const mdb_arg_t *arg = &argv[0];

		if (arg->a_type == MDB_TYPE_IMMEDIATE)
			offset = arg->a_un.a_val;
		else
			offset = (uintptr_t)mdb_strtoull(arg->a_un.a_str);
	}

	hash = SMAP_HASHFUNC(addr, offset);

	if (mdb_vread(&saddr, sizeof (saddr),
	    smd_hash + hash * sizeof (uintptr_t)) == -1) {
		mdb_printf("couldn't read smap at %p",
		    smd_hash + hash * sizeof (uintptr_t));
		return (DCMD_ERR);
	}

	do {
		if (mdb_vread(&smp, sizeof (smp), saddr) == -1) {
			mdb_warn("couldn't read smap at %p", saddr);
			return (DCMD_ERR);
		}

		if ((uintptr_t)smp.sm_vp == addr && smp.sm_off == offset) {
			mdb_printf("vnode %p, offs %p is smap %p, vaddr %p\n",
			    addr, offset, saddr, ((saddr - smd_smap) /
			    sizeof (smp)) * MAXBSIZE + seg.s_base);
			return (DCMD_OK);
		}

		saddr = (uintptr_t)smp.sm_hash;
	} while (saddr != NULL);

	mdb_printf("no smap for vnode %p, offs %p\n", addr, offset);
	return (DCMD_OK);
}

/*ARGSUSED*/
int
addr2smap(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uintptr_t kaddr;
	struct seg seg;
	struct segmap_data sd;

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb_readvar(&kaddr, "segkmap") == -1) {
		mdb_warn("failed to read segkmap");
		return (DCMD_ERR);
	}

	if (mdb_vread(&seg, sizeof (seg), kaddr) == -1) {
		mdb_warn("failed to read segkmap at %p", kaddr);
		return (DCMD_ERR);
	}

	if (mdb_vread(&sd, sizeof (sd), (uintptr_t)seg.s_data) == -1) {
		mdb_warn("failed to read segmap_data at %p", seg.s_data);
		return (DCMD_ERR);
	}

	mdb_printf("%p is smap %p\n", addr,
	    ((addr - (uintptr_t)seg.s_base) >> MAXBSHIFT) *
	    sizeof (struct smap) + (uintptr_t)sd.smd_sm);

	return (DCMD_OK);
}

int
as2proc_walk(uintptr_t addr, const proc_t *p, struct as **asp)
{
	if (p->p_as == *asp)
		mdb_printf("%p\n", addr);
	return (WALK_NEXT);
}

/*ARGSUSED*/
int
as2proc(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb_walk("proc", (mdb_walk_cb_t)as2proc_walk, &addr) == -1) {
		mdb_warn("failed to walk proc");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
int
ptree_walk(uintptr_t addr, const proc_t *p, void *ignored)
{
	proc_t parent;
	int ident = 0;
	uintptr_t paddr;

	for (paddr = (uintptr_t)p->p_parent; paddr != NULL; ident += 5) {
		mdb_vread(&parent, sizeof (parent), paddr);
		paddr = (uintptr_t)parent.p_parent;
	}

	mdb_inc_indent(ident);
	mdb_printf("%0?p  %s\n", addr, p->p_user.u_comm);
	mdb_dec_indent(ident);

	return (WALK_NEXT);
}

void
ptree_ancestors(uintptr_t addr, uintptr_t start)
{
	proc_t p;

	if (mdb_vread(&p, sizeof (p), addr) == -1) {
		mdb_warn("couldn't read ancestor at %p", addr);
		return;
	}

	if (p.p_parent != NULL)
		ptree_ancestors((uintptr_t)p.p_parent, start);

	if (addr != start)
		(void) ptree_walk(addr, &p, NULL);
}

/*ARGSUSED*/
int
ptree(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (!(flags & DCMD_ADDRSPEC))
		addr = NULL;
	else
		ptree_ancestors(addr, addr);

	if (mdb_pwalk("proc", (mdb_walk_cb_t)ptree_walk, NULL, addr) == -1) {
		mdb_warn("couldn't walk 'proc'");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
fd(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	int fdnum;
	const mdb_arg_t *argp = &argv[0];
	proc_t p;
	uf_entry_t uf;

	if ((flags & DCMD_ADDRSPEC) == 0) {
		mdb_warn("fd doesn't give global information\n");
		return (DCMD_ERR);
	}
	if (argc != 1)
		return (DCMD_USAGE);

	if (argp->a_type == MDB_TYPE_IMMEDIATE)
		fdnum = argp->a_un.a_val;
	else
		fdnum = mdb_strtoull(argp->a_un.a_str);

	if (mdb_vread(&p, sizeof (struct proc), addr) == -1) {
		mdb_warn("couldn't read proc_t at %p", addr);
		return (DCMD_ERR);
	}
	if (fdnum > p.p_user.u_finfo.fi_nfiles) {
		mdb_warn("process %p only has %d files open.\n",
		    addr, p.p_user.u_finfo.fi_nfiles);
		return (DCMD_ERR);
	}
	if (mdb_vread(&uf, sizeof (uf_entry_t),
	    (uintptr_t)&p.p_user.u_finfo.fi_list[fdnum]) == -1) {
		mdb_warn("couldn't read uf_entry_t at %p",
		    &p.p_user.u_finfo.fi_list[fdnum]);
		return (DCMD_ERR);
	}

	mdb_printf("%p\n", uf.uf_file);
	return (DCMD_OK);
}

/*ARGSUSED*/
static int
pid2proc(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	pid_t pid = (pid_t)addr;

	if (argc != 0)
		return (DCMD_USAGE);

	if ((addr = mdb_pid2proc(pid, NULL)) == NULL) {
		mdb_warn("PID 0t%d not found\n", pid);
		return (DCMD_ERR);
	}

	mdb_printf("%p\n", addr);
	return (DCMD_OK);
}

static char *sysfile_cmd[] = {
	"exclude:",
	"include:",
	"forceload:",
	"rootdev:",
	"rootfs:",
	"swapdev:",
	"swapfs:",
	"moddir:",
	"set",
	"unknown",
};

static char *sysfile_ops[] = { "", "=", "&", "|" };

static int
sysfile_vmem_seg(uintptr_t addr, const vmem_seg_t *vsp, void **target)
{
	if (vsp->vs_type == VMEM_ALLOC && (void *)vsp->vs_start == *target) {
		*target = NULL;
		return (WALK_DONE);
	}
	return (WALK_NEXT);
}

/*ARGSUSED*/
static int
sysfile(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	struct sysparam *sysp, sys;
	char var[256];
	char modname[256];
	char val[256];
	char strval[256];
	vmem_t *mod_sysfile_arena;
	void *straddr;

	if (mdb_readvar(&sysp, "sysparam_hd") == -1) {
		mdb_warn("failed to read sysparam_hd");
		return (DCMD_ERR);
	}

	if (mdb_readvar(&mod_sysfile_arena, "mod_sysfile_arena") == -1) {
		mdb_warn("failed to read mod_sysfile_arena");
		return (DCMD_ERR);
	}

	while (sysp != NULL) {
		var[0] = '\0';
		val[0] = '\0';
		modname[0] = '\0';
		if (mdb_vread(&sys, sizeof (sys), (uintptr_t)sysp) == -1) {
			mdb_warn("couldn't read sysparam %p", sysp);
			return (DCMD_ERR);
		}
		if (sys.sys_modnam != NULL &&
		    mdb_readstr(modname, 256,
		    (uintptr_t)sys.sys_modnam) == -1) {
			mdb_warn("couldn't read modname in %p", sysp);
			return (DCMD_ERR);
		}
		if (sys.sys_ptr != NULL &&
		    mdb_readstr(var, 256, (uintptr_t)sys.sys_ptr) == -1) {
			mdb_warn("couldn't read ptr in %p", sysp);
			return (DCMD_ERR);
		}
		if (sys.sys_op != SETOP_NONE) {
			/*
			 * Is this an int or a string?  We determine this
			 * by checking whether straddr is contained in
			 * mod_sysfile_arena.  If so, the walker will set
			 * straddr to NULL.
			 */
			straddr = (void *)sys.sys_info;
			if (sys.sys_op == SETOP_ASSIGN &&
			    sys.sys_info != 0 &&
			    mdb_pwalk("vmem_seg",
			    (mdb_walk_cb_t)sysfile_vmem_seg, &straddr,
			    (uintptr_t)mod_sysfile_arena) == 0 &&
			    straddr == NULL &&
			    mdb_readstr(strval, 256,
			    (uintptr_t)sys.sys_info) != -1) {
				sprintf(val, "\"%s\"", strval);
			} else {
				sprintf(val, "0x%llx [0t%llu]", sys.sys_info,
				    sys.sys_info);
			}
		}
		mdb_printf("%s %s%s%s%s%s\n", sysfile_cmd[sys.sys_type],
		    modname, modname[0] == '\0' ? "" : ":",
		    var, sysfile_ops[sys.sys_op], val);

		sysp = sys.sys_next;
	}

	return (DCMD_OK);
}

static const mdb_dcmd_t dcmds[] = {
	{ "addr2smap", ":[offset]", "translate address to smap", addr2smap },
	{ "allocdby", ":", "given a thread, print its allocated buffers",
		allocdby },
	{ "as2proc", ":", "convert as to proc_t address", as2proc },
	{ "bufctl", ":[-a addr] [-c caller] [-e earliest] [-l latest] "
		"[-t thread]", "print or filter a bufctl", bufctl },
	{ "bufpagefind", ":addr", "find page_t on buf_t list", bufpagefind },
	{ "callout", NULL, "print callout table", callout },
	{ "cpuinfo", "[-v]", "print CPUs and runnable threads", cpuinfo },
	{ "cycinfo", NULL, "dump cyc_cpu info", cycinfo },
	{ "cyclic", ":", "developer information", cyclic },
	{ "cyccover", NULL, "dump cyclic coverage information", cyccover },
	{ "cyctrace", NULL, "dump cyclic trace buffer", cyctrace },
	{ "class", NULL, "print process scheduler classes", class },
	{ "devbindings", "device-name", "print nodes bound to <device-name>",
		devbindings },
	{ "devinfo", ":[-q]", "detailed devinfo of one node", devinfo },
	{ "devnames", "?[-v] [num]", "print devnames array", devnames },
	{ "fd", ":[fd num]", "get a file pointer from an fd", fd },
	{ "findleaks", "?[-v]", "search for potential kernel memory leaks",
		findleaks },
	{ "findstack", ":", "find kernel thread stack", findstack },
	{ "findstack_debug", NULL, "toggle findstack debugging",
		findstack_debug },
	{ "flipone", ":", "the vik_rev_level 2 special", flipone },
	{ "freedby", ":", "given a thread, print its freed buffers", freedby },
	{ "fsinfo", NULL, "print list of mounted filesystems", fsinfo },
	{ "kgrep", ":", "search kernel as for a pointer", kgrep },
	{ "rwlock", ":", "dump out a readers/writer lock", rwlock },
	{ "seg", ":", "print address space segment", seg },
	{ "stream", ":", "display STREAM", stream },
	{ "vnode2path", ":[-v]", "vnode address to pathname", vnode2path },
	{ "vnode2smap", ":[offset]", "translate vnode to smap", vnode2smap },
	{ "wchaninfo", "?[-v]", "dump condition variable", wchaninfo },
	{ "whatis", ":[-abv]", "given an address, return information", whatis },
	{ "whereopen", ":", "given a vnode, dumps procs which have it open",
	    whereopen },
	{ "kmalog", "[ fail | slab ]",
	    "display kmem transaction log and stack traces", kmalog },
	{ "kmastat", NULL, "kernel memory allocator stats", kmastat },
	{ "kmausers", "[-ef] [cache ...]", "display current medium and large "
		"users of the kmem allocator", kmausers },
	{ "kmem_cache", NULL, "print kernel memory caches", kmem_cache },
	{ "kmem_debug", NULL, "toggle kmem dcmd/walk debugging", kmem_debug },
	{ "kmem_log", NULL, "dump kmem transaction log", kmem_log },
	{ "kmem_verify", "?", "check integrity of kmem-managed memory",
		kmem_verify },
	{ "lminfo", NULL, "print lock manager information", lminfo },
	{ "pid2proc", "?", "convert PID to proc_t address", pid2proc },
	{ "pmap", ":", "print process memory map", pmap },
	{ "prtconf", "?[-vpc]", "print devinfo tree", prtconf, prtconf_help },
	{ "ps", "[-flt]", "list processes (and associated thr,lwp)", ps },
	{ "ptree", NULL, "print process tree", ptree },
	{ "major2name", "?<major-num>", "convert major number to dev name",
		major2name },
	{ "modctl2devinfo", ":", "given a modctl, list its devinfos",
		modctl2devinfo },
	{ "name2major", "<dev-name>", "convert dev name to major number",
		name2major },
	{ "softstate", ":<instance>", "retrieve soft-state pointer",
		softstate },
	{ "system", NULL, "print contents of /etc/system file", sysfile },
	{ "queue", ":[-v] [-m mod] [-f flag] [-F flag]",
		"filter and display STREAM queue", queue },
	{ "q2syncq", ":", "print syncq for a given queue", q2syncq },
	{ "q2otherq", ":", "print peer queue for a given queue", q2otherq },
	{ "q2rdq", ":", "print read queue for a given queue", q2rdq },
	{ "q2wrq", ":", "print write queue for a given queue", q2wrq },
	{ "syncq", ":[-v] [-f flag] [-F flag] [-t type] [-T type]",
		"filter and display STREAM sync queue", syncq },
	{ "syncq2q", ":", "print queue for a given syncq", syncq2q },
	{ "vmem", NULL, "print a vmem_t", vmem },
	{ "vmem_seg", NULL, "print a vmem seg", vmem_seg },
	{ NULL }
};

static const mdb_walker_t walkers[] = {
	{ "proc", "list of active proc_t structures",
		proc_walk_init, proc_walk_step, proc_walk_fini },
	{ "thread", "global or per-process kthread_t structures",
		thread_walk_init, thread_walk_step, thread_walk_fini },
	{ "deathrow", "walk threads on both lwp_ and thread_deathrow",
		deathrow_walk_init, deathrow_walk_step, NULL },
	{ "thread_deathrow", "walk threads on thread_deathrow",
		thread_deathrow_walk_init, deathrow_walk_step, NULL },
	{ "lwp_deathrow", "walk lwp_deathrow",
		lwp_deathrow_walk_init, deathrow_walk_step, NULL },
	{ "seg", "given an as, list of segments",
		seg_walk_init, seg_walk_step, seg_walk_fini },
	{ "anon", "given an amp, list of anon structures",
		anon_walk_init, anon_walk_step, anon_walk_fini },
	{ "wchan", "given a wchan, list of blocked threads",
		wchan_walk_init, wchan_walk_step, wchan_walk_fini },
	{ "file", "given a proc pointer, list of open file pointers",
		file_walk_init, file_walk_step, file_walk_fini },
	{ "blocked", "walk threads blocked on a given sobj",
		blocked_walk_init, blocked_walk_step, NULL },
	{ "cpu", "walk cpu structures",
		cpu_walk_init, cpu_walk_step, cpu_walk_fini },
	{ "cyccpu", "walk per-CPU cyc_cpu structures",
		cyccpu_walk_init, cyccpu_walk_step, NULL },
	{ "cyctrace", "walk cyclic trace buffer",
		cyctrace_walk_init, cyctrace_walk_step, cyctrace_walk_fini },
	{ "devinfo", "walk devinfo tree or subtree",
		devinfo_walk_init, devinfo_walk_step, devinfo_walk_fini },
	{ "devinfo_children", "walk children of devinfo node",
		devinfo_children_walk_init, devinfo_children_walk_step,
		devinfo_children_walk_fini },
	{ "devinfo_parents", "walk ancestors of devinfo node",
		devinfo_parents_walk_init, devinfo_parents_walk_step,
		devinfo_parents_walk_fini },
	{ "devi_next", "walk devinfo list",
		NULL, devi_next_walk_step, NULL },
	{ "devnames", "walk devnames array",
		devnames_walk_init, devnames_walk_step, devnames_walk_fini },
	{ "kmem", "walk a kmem cache",
		kmem_walk_init, kmem_walk_step, kmem_walk_fini },
	{ "freemem", "walk a kmem cache's free memory",
		freemem_walk_init, kmem_walk_step, kmem_walk_fini },
	{ "bufctl", "walk a kmem cache's bufctls",
		bufctl_walk_init, bufctl_walk_step, kmem_walk_fini },
	{ "freectl", "walk a kmem cache's free bufctls",
		freectl_walk_init, bufctl_walk_step, kmem_walk_fini },
	{ "kmem_log", "walk the kmem transaction log",
		kmem_log_walk_init, kmem_log_walk_step, kmem_log_walk_fini },
	{ "kmem_cpu_cache", "given a kmem cache, walk its per-CPU caches",
		kmem_cpu_cache_walk_init, kmem_cpu_cache_walk_step, NULL },
	{ "kmem_slab", "given a kmem cache, walk its slabs",
		kmem_slab_walk_init, kmem_slab_walk_step, NULL },
	{ "allocdby", "given a thread, walk its allocated bufctls",
		allocdby_walk_init, allocdby_walk_step, allocdby_walk_fini },
	{ "freedby", "given a thread, walk its freed bufctls",
		freedby_walk_init, allocdby_walk_step, allocdby_walk_fini },
	{ "buf", "walk the bio buf hash",
		buf_walk_init, buf_walk_step, buf_walk_fini },
	{ "qlink", "walk queue_t list using q_link",
		queue_walk_init, queue_link_step, queue_walk_fini },
	{ "qnext", "walk queue_t list using q_next",
		queue_walk_init, queue_next_step, queue_walk_fini },
	{ "readq", "walk read queue side of stdata",
		str_walk_init, strr_walk_step, str_walk_fini },
	{ "writeq", "walk write queue side of stdata",
		str_walk_init, strw_walk_step, str_walk_fini },
	{ "kgrep", "search kernel as for a pointer",
		kgrep_walk_init, kgrep_walk_step, kgrep_walk_fini },
	{ "vmem", "walk vmem strutures in pre-fix, depth-first order",
		vmem_walk_init, vmem_walk_step, vmem_walk_fini },
	{ "vmem_postfix", "walk vmem strutures in post-fix, depth-first order",
		vmem_walk_init, vmem_postfix_walk_step, vmem_walk_fini },
	{ "vmem_seg", "given a vmem_t, walk all of its vmem_segs",
		vmem_seg_walk_init, vmem_seg_walk_step, vmem_seg_walk_fini },
	{ "vmem_alloc", "given a vmem_t, walk its allocated vmem_segs",
		vmem_alloc_walk_init, vmem_seg_walk_step, vmem_seg_walk_fini },
	{ "vmem_free", "given a vmem_t, walk its free vmem_segs",
		vmem_free_walk_init, vmem_seg_walk_step, vmem_seg_walk_fini },
	{ "vmem_span", "given a vmem_t, walk its spanning vmem_segs",
		vmem_span_walk_init, vmem_seg_walk_step, vmem_seg_walk_fini },
	{ "leak", "given a bufctl, walk bufctls for similar leaks",
		leaky_walk_init, leaky_walk_step, leaky_walk_fini },
	{ "leakbuf", "given a bufctl, walk buffers for similar leaks",
		leaky_buf_walk_init, leaky_buf_walk_step, NULL },
	{ NULL }
};

static const mdb_modinfo_t modinfo = { MDB_API_VERSION, dcmds, walkers };

const mdb_modinfo_t *
_mdb_init(void)
{
	mdb_walker_t w = {
		"kmem_cache", "walk list of kmem caches", kmem_cache_walk_init,
		kmem_cache_walk_step, kmem_cache_walk_fini
	};

	if (mdb_readvar(&devinfo_root, "top_devinfo") == -1) {
		mdb_warn("failed to read 'top_devinfo'");
		return (NULL);
	}

	if (mdb_readvar(&kmem_content_maxsave, "kmem_content_maxsave") == -1) {
		mdb_warn("failed to read 'kmem_content_maxsave'\n");
		return (NULL);
	}

	if (findstack_init() != DCMD_OK)
		return (NULL);

	/*
	 * Load the kmem_cache walker manually, and then invoke it to add
	 * named walks for each cache.  This walker needs to be added by
	 * hand since walkers in the linkage structure cannot be loaded
	 * until _mdb_init returns the pointer to the linkage structure.
	 */
	if (mdb_add_walker(&w) == 0) {
		(void) mdb_walk("kmem_cache", (mdb_walk_cb_t)
		    kmem_init_walkers, NULL);
	} else
		mdb_warn("failed to add kmem_cache walker");

	return (&modinfo);
}
