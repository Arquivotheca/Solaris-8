/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_ks.c	1.2	99/09/27 SMI"

/*
 * Mdb kernel support module.  This module is loaded automatically when the
 * kvm target is initialized.  Any global functions declared here are exported
 * for the resolution of symbols in subsequently loaded modules.
 */

#include <mdb/mdb_target.h>
#include <mdb/mdb_param.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_ks.h>

#include <sys/types.h>
#include <sys/procfs.h>
#include <sys/proc.h>
#include <sys/dnlc.h>
#include <sys/autoconf.h>
#include <sys/machelf.h>
#include <sys/modctl.h>
#include <sys/kobj.h>
#include <sys/fs/autofs.h>
#include <sys/ddi_impldefs.h>

#include <vm/seg_vn.h>
#include <vm/page.h>

/*
 * Kernel parameters from <sys/param.h> which we keep in-core:
 */
int _mdb_ks_hz;
int _mdb_ks_cpu_decay_factor;
pid_t _mdb_ks_maxpid;
unsigned long _mdb_ks_pagesize;
unsigned int _mdb_ks_pageshift;
unsigned long _mdb_ks_pageoffset;
unsigned long long _mdb_ks_pagemask;
unsigned long _mdb_ks_mmu_pagesize;
unsigned int _mdb_ks_mmu_pageshift;
unsigned long _mdb_ks_mmu_pageoffset;
unsigned long _mdb_ks_mmu_pagemask;
uintptr_t _mdb_ks_kernelbase;
uintptr_t _mdb_ks_userlimit;
uintptr_t _mdb_ks_userlimit32;
uintptr_t _mdb_ks_argsbase;
unsigned long _mdb_ks_msg_bsize;
unsigned long _mdb_ks_defaultstksz;
unsigned int _mdb_ks_nbpg;
int _mdb_ks_ncpu;
int _mdb_ks_clsize;

/*
 * In-core copy of DNLC information:
 */
#define	DNLC_HSIZE	1024
#define	DNLC_HASH(vp)	(((uintptr_t)(vp) >> 3) & (DNLC_HSIZE - 1))

typedef struct ncache dnlc_ent_t;

static dnlc_ent_t *dnlc;		/* Array of cached entries */
static dnlc_ent_t **dnlc_hash;		/* Array of hash buckets */
static uintptr_t dnlc_addr;		/* Kernel va of ncache array */
static volatile int dnlc_nalloc;	/* Number of allocated strings */
static int dnlc_nent;			/* Number of entries */

static struct vnodeops *autofs_vnops = NULL;

/*
 * STREAMS queue registrations:
 */
typedef struct mdb_qinfo {
	const mdb_qops_t *qi_ops;	/* Address of ops vector */
	uintptr_t qi_addr;		/* Address of qinit structure (key) */
	struct mdb_qinfo *qi_next;	/* Next qinfo in list */
} mdb_qinfo_t;

static mdb_qinfo_t *qi_head;		/* Head of qinfo chain */

/*
 * Device naming callback structure:
 */
typedef struct nm_query {
	const char *nm_name;		/* Device driver name [in/out] */
	major_t nm_major;		/* Device major number [in/out] */
	ushort_t nm_found;		/* Did we find a match? [out] */
} nm_query_t;

/*
 * Address-to-modctl callback structure:
 */
typedef struct a2m_query {
	uintptr_t a2m_addr;		/* Virtual address [in] */
	uintptr_t a2m_where;		/* Modctl address [out] */
} a2m_query_t;

/*
 * Segment-to-mdb_map callback structure:
 */
typedef struct {
	struct seg_ops *asm_segvn_ops;	/* Address of segvn ops [in] */
	void (*asm_callback)(const struct mdb_map *, void *); /* Callb [in] */
	void *asm_cbdata;		/* Callback data [in] */
} asmap_arg_t;

static int
dnlc_load(void)
{
	int i;

	/*
	 * If we've already cached the DNLC and we're looking at a dump,
	 * our cache is good forever, so don't bother re-loading.
	 */
	if (dnlc_addr && mdb_prop_postmortem)
		return (0);

	/*
	 * Even for live kernels, the ncsize tunable and ncache array is
	 * only read and modified during dnlc_init(), so we only need to
	 * allocate this stuff once.
	 */
	if (dnlc_addr == NULL) {
		if (mdb_readvar(&dnlc_nent, "ncsize") == -1) {
			mdb_warn("failed to read ncsize");
			return (-1);
		}

		if (mdb_readvar(&dnlc_addr, "ncache") == -1) {
			mdb_warn("failed to read ncache");
			return (-1);
		}

		dnlc_hash = mdb_alloc(DNLC_HSIZE *
		    sizeof (dnlc_ent_t *), UM_SLEEP);

		dnlc = mdb_alloc(dnlc_nent * sizeof (dnlc_ent_t), UM_SLEEP);

	} else {
		dnlc_ent_t *ncp;
		int i, nalloc = dnlc_nalloc;

		for (ncp = dnlc, i = 0; i < nalloc; i++, ncp++) {
			if (ncp->name != NULL) {
				mdb_free(ncp->name, ncp->namlen + 1);
				ncp->name = NULL;
			}
		}
	}

	/*
	 * No strings have been allocated at this point.
	 */
	dnlc_nalloc = 0;

	/*
	 * Zero the hash buckets either way so we start fresh.
	 */
	(void) memset(dnlc_hash, 0, DNLC_HSIZE * sizeof (dnlc_ent_t *));

	/*
	 * Slurp the DNLC and hash it in-core.
	 */
	if (mdb_vread(dnlc, dnlc_nent * sizeof (dnlc_ent_t), dnlc_addr) == -1) {
		mdb_warn("failed to read DNLC contents");
		mdb_free(dnlc, dnlc_nent * sizeof (dnlc_ent_t));
		mdb_free(dnlc_hash, DNLC_HSIZE * sizeof (dnlc_ent_t *));
		dnlc_addr = NULL;
		return (-1);
	}

	for (i = 0; i < dnlc_nent; i++) {
		int hash = DNLC_HASH(dnlc[i].vp);
		uintptr_t namep = (uintptr_t)dnlc[i].name;
		dnlc_ent_t *last = dnlc_hash[hash];

		if (dnlc[i].vp == NULL || dnlc[i].namlen == 0) {
			dnlc[i].name = NULL;
			dnlc_nalloc++;
			dnlc[i].namlen = 0;
			continue;
		}

		dnlc[i].name = mdb_zalloc(dnlc[i].namlen + 1, UM_SLEEP);
		dnlc_nalloc++;	/* Increment count of allocated names */

		(void) mdb_vread(dnlc[i].name, dnlc[i].namlen, namep);

		while (last != NULL && last->hash_next != NULL)
			last = last->hash_next;

		if (last == NULL)
			dnlc_hash[hash] = &dnlc[i];
		else
			last->hash_next = &dnlc[i];

		dnlc[i].hash_next = NULL;
	}

	return (0);
}

/*ARGSUSED*/
int
dnlcdump(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	dnlc_ent_t *ent;
	int i;

	if (dnlc_load() == -1)
		return (DCMD_ERR);

	mdb_printf("%<u>%-?s %-?s %-32s%</u>\n", "VP", "DVP", "NAME");

	for (i = 0; i < DNLC_HSIZE; i++) {
		for (ent = dnlc_hash[i]; ent != NULL; ent = ent->hash_next) {
			mdb_printf("%0?p %0?p %s\n",
			    ent->vp, ent->dp, ent->name);
		}
	}

	return (DCMD_OK);
}

int
mdb_sprintpath(char *buf, size_t len, mdb_path_t *path)
{
	char *s = buf;
	int i;

	if (len < sizeof ("/..."))
		return (-1);

	if (!path->mdp_complete) {
		(void) strcpy(s, "??");
		s += 2;

		if (path->mdp_nelem == 0)
			return (-1);
	}

	if (path->mdp_nelem == 0) {
		(void) strcpy(s, "/");
		return (0);
	}

	for (i = path->mdp_nelem - 1; i >= 0; i--) {
		/*
		 * Number of bytes left is the distance from where we
		 * are to the end, minus 2 for '/' and '\0'
		 */
		ssize_t left = (ssize_t)(&buf[len] - s) - 2;

		if (left <= 0)
			break;

		*s++ = '/';
		(void) strncpy(s, path->mdp_name[i], left);
		s[left - 1] = '\0';
		s += strlen(s);

		if (left < strlen(path->mdp_name[i]))
			break;
	}

	if (i >= 0)
		(void) strcpy(&buf[len - 4], "...");

	return (0);
}

int
mdb_autonode2path(uintptr_t addr, mdb_path_t *path)
{
	fninfo_t fni;
	fnnode_t fn;

	vnode_t vn;
	vfs_t vfs;

	if (mdb_vread(&vn, sizeof (vn), addr) == -1)
		return (-1);

	if (autofs_vnops == NULL || vn.v_op != autofs_vnops)
		return (-1);

	addr = (uintptr_t)vn.v_data;

	if (mdb_vread(&vfs, sizeof (vfs), (uintptr_t)vn.v_vfsp) == -1 ||
	    mdb_vread(&fni, sizeof (fni), (uintptr_t)vfs.vfs_data) == -1 ||
	    mdb_vread(&vn, sizeof (vn), (uintptr_t)fni.fi_rootvp) == -1)
		return (-1);

	for (;;) {
		size_t elem = path->mdp_nelem++;
		char elemstr[MAXNAMELEN];
		char *c, *p;

		if (elem == MDB_PATH_NELEM) {
			path->mdp_nelem--;
			return (-1);
		}

		if (mdb_vread(&fn, sizeof (fn), addr) != sizeof (fn)) {
			path->mdp_nelem--;
			return (-1);
		}

		if (mdb_readstr(elemstr, sizeof (elemstr),
		    (uintptr_t)fn.fn_name) <= 0) {
			(void) strcpy(elemstr, "?");
		}

		c = mdb_alloc(strlen(elemstr) + 1, UM_SLEEP | UM_GC);
		(void) strcpy(c, elemstr);

		path->mdp_vnode[elem] = (uintptr_t)&fn.fn_vnode -
		    (uintptr_t)&fn + addr;

		if (addr == (uintptr_t)fn.fn_parent) {
			path->mdp_name[elem] = &c[1];
			path->mdp_complete = TRUE;
			break;
		}

		if ((p = strrchr(c, '/')) != NULL)
			path->mdp_name[elem] = p + 1;
		else
			path->mdp_name[elem] = c;

		addr = (uintptr_t)fn.fn_parent;
	}

	return (0);
}

int
mdb_vnode2path(uintptr_t addr, mdb_path_t *path)
{
	uintptr_t rootdir;
	dnlc_ent_t *ent;

	if (dnlc_load() == -1)
		return (DCMD_ERR);

	if (mdb_readvar(&rootdir, "rootdir") == -1) {
		mdb_warn("failed to read 'rootdir'");
		return (DCMD_ERR);
	}

	bzero(path, sizeof (mdb_path_t));
again:
	if ((addr == NULL) && (path->mdp_nelem == 0)) {
		/*
		 * 0 elems && complete tells sprintpath to just print "/"
		 */
		path->mdp_complete = TRUE;
		return (DCMD_OK);
	}

	if (addr == rootdir) {
		path->mdp_complete = TRUE;
		return (DCMD_OK);
	}

	for (ent = dnlc_hash[DNLC_HASH(addr)]; ent; ent = ent->hash_next) {
		if ((uintptr_t)ent->vp == addr) {
			if (strcmp(ent->name, "..") == 0 ||
			    strcmp(ent->name, ".") == 0)
				continue;

			path->mdp_vnode[path->mdp_nelem] = (uintptr_t)ent->vp;
			path->mdp_name[path->mdp_nelem] = ent->name;
			path->mdp_nelem++;

			if (path->mdp_nelem == MDB_PATH_NELEM) {
				path->mdp_nelem--;
				mdb_warn("path exceeded maximum expected "
				    "elements\n");
				return (DCMD_ERR);
			}

			addr = (uintptr_t)ent->dp;
			goto again;
		}
	}

	(void) mdb_autonode2path(addr, path);
	return (0);
}

char *
mdb_vnode2buf(uintptr_t vp, char *buf, size_t len)
{
	mdb_path_t path;

	(void) mdb_vnode2path(vp, &path);
	(void) mdb_sprintpath(buf, len, &path);

	return (buf);
}

uintptr_t
mdb_pid2proc(pid_t pid, proc_t *proc)
{
	int pid_hashsz, hash;
	uintptr_t paddr, pidhash, procdir;
	struct pid pidp;

	if (mdb_readvar(&pidhash, "pidhash") == -1)
		return (NULL);

	if (mdb_readvar(&pid_hashsz, "pid_hashsz") == -1)
		return (NULL);

	if (mdb_readvar(&procdir, "procdir") == -1)
		return (NULL);

	hash = pid & (pid_hashsz - 1);

	if (mdb_vread(&paddr, sizeof (paddr),
	    pidhash + (hash * sizeof (paddr))) == -1)
		return (NULL);

	while (paddr != 0) {
		if (mdb_vread(&pidp, sizeof (pidp), paddr) == -1)
			return (NULL);

		if (pidp.pid_id == pid) {
			uintptr_t procp;

			if (mdb_vread(&procp, sizeof (procp), procdir +
			    (pidp.pid_prslot * sizeof (procp))) == -1)
				return (NULL);

			if (proc != NULL)
				(void) mdb_vread(proc, sizeof (proc_t), procp);

			return (procp);
		}
		paddr = (uintptr_t)pidp.pid_link;
	}
	return (NULL);
}

uintptr_t
mdb_vnode2page(uintptr_t vp, uintptr_t offset)
{
	long page_hashsz, ndx;
	uintptr_t page_hash, pp;

	if (mdb_readvar(&page_hashsz, "page_hashsz") == -1 ||
	    mdb_readvar(&page_hash, "page_hash") == -1)
		return (NULL);

	ndx = PAGE_HASH_FUNC(vp, offset);
	page_hash += ndx * sizeof (uintptr_t);

	mdb_vread(&pp, sizeof (pp), page_hash);

	while (pp != NULL) {
		page_t page;

		mdb_vread(&page, sizeof (page), pp);

		if ((uintptr_t)page.p_vnode == vp &&
		    (uintptr_t)page.p_offset == offset)
			return (pp);

		pp = (uintptr_t)page.p_hash;
	}

	return (NULL);
}

char
mdb_vtype2chr(vtype_t type, mode_t mode)
{
	static const char vttab[] = {
		' ',	/* VNON */
		' ',	/* VREG */
		'/',	/* VDIR */
		' ',	/* VBLK */
		' ',	/* VCHR */
		'@',	/* VLNK */
		'|',	/* VFIFO */
		'>',	/* VDOOR */
		' ',	/* VPROC */
		'=',	/* VSOCK */
		' ',	/* VBAD */
        };

	if (type < 0 || type >= sizeof (vttab) / sizeof (vttab[0]))
		return ('?');

	if (type == VREG && (mode & 0111) != 0)
		return ('*');

	return (vttab[type]);
}

static int
a2m_walk_modctl(uintptr_t addr, const struct modctl *m, a2m_query_t *a2m)
{
	struct module mod;

	if (m->mod_mp == NULL)
		return (0);

	if (mdb_vread(&mod, sizeof (mod), (uintptr_t)m->mod_mp) == -1) {
		mdb_warn("couldn't read modctl %p's module", addr);
		return (0);
	}

	if (a2m->a2m_addr >= (uintptr_t)mod.text &&
	    a2m->a2m_addr < (uintptr_t)mod.text + mod.text_size)
		goto found;

	if (a2m->a2m_addr >= (uintptr_t)mod.data &&
	    a2m->a2m_addr < (uintptr_t)mod.data + mod.data_size)
		goto found;

	return (0);

found:
	a2m->a2m_where = addr;
	return (-1);
}

uintptr_t
mdb_addr2modctl(uintptr_t addr)
{
	a2m_query_t a2m;

	a2m.a2m_addr = addr;
	a2m.a2m_where = NULL;

	(void) mdb_walk("modctl", (mdb_walk_cb_t)a2m_walk_modctl, &a2m);
	return (a2m.a2m_where);
}

static mdb_qinfo_t *
qi_lookup(uintptr_t qinit_addr)
{
	mdb_qinfo_t *qip;

	for (qip = qi_head; qip != NULL; qip = qip->qi_next) {
		if (qip->qi_addr == qinit_addr)
			return (qip);
	}

	return (NULL);
}

void
mdb_qops_install(const mdb_qops_t *qops, uintptr_t qinit_addr)
{
	mdb_qinfo_t *qip = qi_lookup(qinit_addr);

	if (qip != NULL) {
		qip->qi_ops = qops;
		return;
	}

	qip = mdb_alloc(sizeof (mdb_qinfo_t), UM_SLEEP);

	qip->qi_ops = qops;
	qip->qi_addr = qinit_addr;
	qip->qi_next = qi_head;

	qi_head = qip;
}

void
mdb_qops_remove(const mdb_qops_t *qops, uintptr_t qinit_addr)
{
	mdb_qinfo_t *qip, *p = NULL;

	for (qip = qi_head; qip != NULL; p = qip, qip = qip->qi_next) {
		if (qip->qi_addr == qinit_addr && qip->qi_ops == qops) {
			if (qi_head == qip)
				qi_head = qip->qi_next;
			else
				p->qi_next = qip->qi_next;
			mdb_free(qip, sizeof (mdb_qinfo_t));
			return;
		}
	}
}

char *
mdb_qname(const queue_t *q, char *buf, size_t nbytes)
{
	struct module_info mi;
	struct qinit qi;

	if (mdb_vread(&qi, sizeof (qi), (uintptr_t)q->q_qinfo) == -1) {
		mdb_warn("failed to read qinit at %p", q->q_qinfo);
		goto err;
	}

	if (mdb_vread(&mi, sizeof (mi), (uintptr_t)qi.qi_minfo) == -1) {
		mdb_warn("failed to read module_info at %p", qi.qi_minfo);
		goto err;
	}

	if (mdb_readstr(buf, nbytes, (uintptr_t)mi.mi_idname) <= 0) {
		mdb_warn("failed to read mi_idname at %p", mi.mi_idname);
		goto err;
	}

	return (buf);

err:
	(void) mdb_snprintf(buf, nbytes, "???");
	return (buf);
}

void
mdb_qinfo(const queue_t *q, char *buf, size_t nbytes)
{
	mdb_qinfo_t *qip = qi_lookup((uintptr_t)q->q_qinfo);
	buf[0] = '\0';

	if (qip != NULL)
		qip->qi_ops->q_info(q, buf, nbytes);
}

uintptr_t
mdb_qrnext(const queue_t *q)
{
	mdb_qinfo_t *qip = qi_lookup((uintptr_t)q->q_qinfo);

	if (qip != NULL)
		return (qip->qi_ops->q_rnext(q));

	return (NULL);
}

uintptr_t
mdb_qwnext(const queue_t *q)
{
	mdb_qinfo_t *qip = qi_lookup((uintptr_t)q->q_qinfo);

	if (qip != NULL)
		return (qip->qi_ops->q_wnext(q));

	return (NULL);
}

uintptr_t
mdb_qrnext_default(const queue_t *q)
{
	return ((uintptr_t)q->q_next);
}

uintptr_t
mdb_qwnext_default(const queue_t *q)
{
	return ((uintptr_t)q->q_next);
}

/*ARGSUSED*/
static int
nm_find(uintptr_t addr, struct devnames *din, nm_query_t *nmp)
{
	char name[MODMAXNAMELEN + 1];

	if (mdb_readstr(name, sizeof (name), (uintptr_t)din->dn_name) > 0 &&
	    strcmp(name, nmp->nm_name) == 0) {
		nmp->nm_found = TRUE;
		return (-1);
	}

	nmp->nm_major++;
	return (0);
}

int
mdb_name_to_major(const char *name, major_t *major)
{
	nm_query_t nm = { NULL, 0, FALSE };
	int status = -1;

	nm.nm_name = name;

	if (mdb_walk("devnames", (mdb_walk_cb_t)nm_find, &nm) == 0 &&
	    nm.nm_found == TRUE) {
		*major = nm.nm_major;
		status = 0;
	}

	return (status);
}

const char *
mdb_major_to_name(major_t major)
{
	static char name[MODMAXNAMELEN + 1];

	uintptr_t devnamesp;
	struct devnames dn;
	uint_t devcnt;

	if (mdb_readvar(&devcnt, "devcnt") == -1 || major >= devcnt ||
	    mdb_readvar(&devnamesp, "devnamesp") == -1)
		return (NULL);

	if (mdb_vread(&dn, sizeof (struct devnames), devnamesp +
	    major * sizeof (struct devnames)) != sizeof (struct devnames))
		return (NULL);

	if (mdb_readstr(name, MODMAXNAMELEN + 1, (uintptr_t)dn.dn_name) == -1)
		return (NULL);

	return ((const char *)name);
}

int
mdb_get_soft_state(const char *name, size_t item, uintptr_t *sp)
{
	GElf_Sym sym;
	struct i_ddi_soft_state ss;
	uintptr_t addr;

	if (mdb_lookup_by_name(name, &sym) == -1) {
		mdb_warn("failed to find soft state %s", name);
		return (-1);
	}

	if (mdb_vread(&addr, sizeof (addr), (uintptr_t)sym.st_value) == -1) {
		mdb_warn("failed to read soft state pointer at %p",
		    (uintptr_t)sym.st_value);
		return (-1);
	}

	if (mdb_vread(&ss, sizeof (ss), addr) == -1) {
		mdb_warn("failed to read soft state at %p", addr);
		return (-1);
	}

	if (item >= ss.n_items) {
		mdb_warn("item %lu is not in range [0..%lu]\n",
		    (ulong_t)item, (ulong_t)ss.n_items);
		return (-1);
	}

	addr = (uintptr_t)ss.array + item * sizeof (uintptr_t);

	if (mdb_vread(sp, sizeof (uintptr_t), addr) == -1) {
		mdb_warn("failed to read soft state for item %lu at %p",
		    (ulong_t)item, addr);
		return (-1);
	}

	return (0);
}

static const mdb_dcmd_t dcmds[] = {
	{ "dnlc", NULL, "print DNLC contents", dnlcdump },
	{ NULL }
};

static const mdb_modinfo_t modinfo = { MDB_API_VERSION, dcmds };

const mdb_modinfo_t *
_mdb_init(void)
{
	GElf_Sym sym;

	if (mdb_lookup_by_name("auto_vnodeops", &sym) == 0)
		autofs_vnops = (struct vnodeops *)sym.st_value;

	(void) mdb_readvar(&_mdb_ks_hz, "_hz");
	(void) mdb_readvar(&_mdb_ks_cpu_decay_factor, "_cpu_decay_factor");
	(void) mdb_readvar(&_mdb_ks_maxpid, "_maxpid");
	(void) mdb_readvar(&_mdb_ks_pagesize, "_pagesize");
	(void) mdb_readvar(&_mdb_ks_pageshift, "_pageshift");
	(void) mdb_readvar(&_mdb_ks_pageoffset, "_pageoffset");
	(void) mdb_readvar(&_mdb_ks_pagemask, "_pagemask");
	(void) mdb_readvar(&_mdb_ks_mmu_pagesize, "_mmu_pagesize");
	(void) mdb_readvar(&_mdb_ks_mmu_pageshift, "_mmu_pageshift");
	(void) mdb_readvar(&_mdb_ks_mmu_pageoffset, "_mmu_pageoffset");
	(void) mdb_readvar(&_mdb_ks_mmu_pagemask, "_mmu_pagemask");
	(void) mdb_readvar(&_mdb_ks_kernelbase, "_kernelbase");
	(void) mdb_readvar(&_mdb_ks_userlimit, "_userlimit");
	(void) mdb_readvar(&_mdb_ks_userlimit32, "_userlimit32");
	(void) mdb_readvar(&_mdb_ks_argsbase, "_argsbase");
	(void) mdb_readvar(&_mdb_ks_msg_bsize, "_msg_bsize");
	(void) mdb_readvar(&_mdb_ks_defaultstksz, "_defaultstksz");
	(void) mdb_readvar(&_mdb_ks_nbpg, "_nbpg");
	(void) mdb_readvar(&_mdb_ks_ncpu, "_ncpu");
	(void) mdb_readvar(&_mdb_ks_clsize, "_clsize");

	return (&modinfo);
}

void
_mdb_fini(void)
{
	if (dnlc_addr != NULL) {
		dnlc_ent_t *ncp;
		int i;

		for (ncp = dnlc, i = 0; i < dnlc_nalloc; i++, ncp++) {
			if (ncp->name != NULL)
				mdb_free(ncp->name, ncp->namlen + 1);
		}

		mdb_free(dnlc_hash, DNLC_HSIZE * sizeof (dnlc_ent_t *));
		mdb_free(dnlc, dnlc_nent * sizeof (dnlc_ent_t));
	}

	while (qi_head != NULL) {
		mdb_qinfo_t *qip = qi_head;
		qi_head = qip->qi_next;
		mdb_free(qip, sizeof (mdb_qinfo_t));
	}
}

/*
 * Interface between MDB kproc target and mdb_ks.  The kproc target relies
 * on looking up and invoking these functions in mdb_ks so that dependencies
 * on the current kernel implementation are isolated in mdb_ks.
 */

/*
 * Given the address of a proc_t, return the p.p_as pointer; return NULL
 * if we were unable to read a proc structure from the given address.
 */
uintptr_t
mdb_kproc_as(uintptr_t proc_addr)
{
	proc_t p;

	if (mdb_vread(&p, sizeof (p), proc_addr) == sizeof (p))
		return ((uintptr_t)p.p_as);

	return (NULL);
}

/*
 * Given the address of a proc_t, return the p.p_model value; return
 * PR_MODEL_UNKNOWN if we were unable to read a proc structure or if
 * the model value does not match one of the two known values.
 */
uint_t
mdb_kproc_model(uintptr_t proc_addr)
{
	proc_t p;

	if (mdb_vread(&p, sizeof (p), proc_addr) == sizeof (p)) {
		switch (p.p_model) {
		case DATAMODEL_ILP32:
			return (PR_MODEL_ILP32);
		case DATAMODEL_LP64:
			return (PR_MODEL_LP64);
		}
	}

	return (PR_MODEL_UNKNOWN);
}

/*
 * Callback function for walking process's segment list.  For each segment,
 * we fill in an mdb_map_t describing its properties, and then invoke
 * the callback function provided by the kproc target.
 */
static int
asmap_step(uintptr_t addr, const struct seg *seg, asmap_arg_t *asmp)
{
	struct segvn_data svd;
	mdb_map_t map;

	if (seg->s_ops == asmp->asm_segvn_ops && mdb_vread(&svd,
	    sizeof (svd), (uintptr_t)seg->s_data) == sizeof (svd)) {

		if (svd.vp != NULL) {
			mdb_path_t path;

			if (mdb_vnode2path((uintptr_t)svd.vp, &path) == 0) {
				(void) mdb_sprintpath(map.map_name,
				    MDB_TGT_MAPSZ, &path);
			} else {
				(void) mdb_snprintf(map.map_name,
				    MDB_TGT_MAPSZ, "[ vnode %p ]", svd.vp);
			}
		} else
			(void) strcpy(map.map_name, "[ anon ]");

	} else {
		(void) mdb_snprintf(map.map_name, MDB_TGT_MAPSZ,
		    "[ seg %p ]", addr);
	}

	map.map_base = (uintptr_t)seg->s_base;
	map.map_size = seg->s_size;
	map.map_flags = 0;

	asmp->asm_callback((const struct mdb_map *)&map, asmp->asm_cbdata);
	return (WALK_NEXT);
}

/*
 * Given a process address space, walk its segment list using the seg walker,
 * convert the segment data to an mdb_map_t, and pass this information
 * back to the kproc target via the given callback function.
 */
int
mdb_kproc_asiter(uintptr_t as,
    void (*func)(const struct mdb_map *, void *), void *p)
{
	asmap_arg_t arg;
	GElf_Sym sym;

	arg.asm_segvn_ops = NULL;
	arg.asm_callback = func;
	arg.asm_cbdata = p;

	if (mdb_lookup_by_name("segvn_ops", &sym) == 0)
		arg.asm_segvn_ops = (struct seg_ops *)sym.st_value;

	return (mdb_pwalk("seg", (mdb_walk_cb_t)asmap_step, &arg, as));
}

/*
 * Copy the auxv array from the given process's u-area into the provided
 * buffer.  If the buffer is NULL, only return the size of the auxv array
 * so the caller knows how much space will be required.
 */
int
mdb_kproc_auxv(uintptr_t proc, auxv_t *auxv)
{
	if (auxv != NULL) {
		proc_t p;

		if (mdb_vread(&p, sizeof (p), proc) != sizeof (p))
			return (-1);

		bcopy(p.p_user.u_auxv, auxv,
		    sizeof (auxv_t) * __KERN_NAUXV_IMPL);
	}

	return (__KERN_NAUXV_IMPL);
}

/*
 * Given a process address, return the PID.
 */
pid_t
mdb_kproc_pid(uintptr_t proc_addr)
{
	struct pid pid;
	proc_t p;

	if (mdb_vread(&p, sizeof (p), proc_addr) == sizeof (p) &&
	    mdb_vread(&pid, sizeof (pid), (uintptr_t)p.p_pidp) == sizeof (pid))
		return (pid.pid_id);

	return (-1);
}
