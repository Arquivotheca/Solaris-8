/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_disasm.c	1.1	99/08/11 SMI"

#include <mdb/mdb_disasm_impl.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_string.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_nv.h>
#include <mdb/mdb.h>

int
mdb_dis_select(const char *name)
{
	mdb_var_t *v = mdb_nv_lookup(&mdb.m_disasms, name);

	if (v != NULL) {
		mdb.m_disasm = mdb_nv_get_cookie(v);
		return (0);
	}

	return (set_errno(EMDB_NODIS));
}

mdb_disasm_t *
mdb_dis_create(mdb_dis_ctor_f *ctor)
{
	mdb_disasm_t *dp = mdb_zalloc(sizeof (mdb_disasm_t), UM_SLEEP);
	mdb_module_t *mp;

	dp->dis_module = &mdb.m_rmod;

	for (mp = mdb.m_mhead; mp != NULL; mp = mp->mod_next) {
		if (ctor == mp->mod_dis_ctor) {
			dp->dis_module = mp;
			break;
		}
	}

	if (ctor(dp) == 0) {
		mdb_var_t *v = mdb_nv_lookup(&mdb.m_disasms, dp->dis_name);

		if (v == NULL) {
			(void) mdb_nv_insert(&mdb.m_disasms, dp->dis_name, NULL,
			    (uintptr_t)dp, MDB_NV_RDONLY | MDB_NV_SILENT);
		} else {
			dp->dis_ops->dis_destroy(dp);
			(void) set_errno(EMDB_DISEXISTS);
		}

		if (mdb.m_disasm == NULL)
			mdb.m_disasm = dp;

		return (dp);
	}

	mdb_free(dp, sizeof (mdb_disasm_t));
	return (NULL);
}

void
mdb_dis_destroy(mdb_disasm_t *dp)
{
	mdb_var_t *v = mdb_nv_lookup(&mdb.m_disasms, dp->dis_name);

	ASSERT(v != NULL);
	mdb_nv_remove(&mdb.m_disasms, v);
	dp->dis_ops->dis_destroy(dp);
	mdb_free(dp, sizeof (mdb_disasm_t));

	if (mdb.m_disasm == dp)
		(void) mdb_dis_select("default");
}

mdb_tgt_addr_t
mdb_dis_ins2str(mdb_disasm_t *dp, mdb_tgt_t *t, mdb_tgt_as_t as,
    char *buf, mdb_tgt_addr_t addr)
{
	return (dp->dis_ops->dis_ins2str(dp, t, as, buf, addr));
}

/*ARGSUSED*/
int
cmd_dismode(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc > 1)
		return (DCMD_USAGE);

	if (argc != 0) {
		const char *name;

		if (argv->a_type == MDB_TYPE_STRING)
			name = argv->a_un.a_str;
		else
			name = numtostr(argv->a_un.a_val, 10, NTOS_UNSIGNED);

		if (mdb_dis_select(name) == -1) {
			warn("failed to set disassembly mode");
			return (DCMD_ERR);
		}
	}

	mdb_printf("disassembly mode is %s (%s)\n",
	    mdb.m_disasm->dis_name, mdb.m_disasm->dis_desc);

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
print_dis(mdb_var_t *v, void *ignore)
{
	mdb_disasm_t *dp = mdb_nv_get_cookie(v);

	mdb_printf("%-24s - %s\n", dp->dis_name, dp->dis_desc);
	return (0);
}

/*ARGSUSED*/
int
cmd_disasms(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if ((flags & DCMD_ADDRSPEC) || argc != 0)
		return (DCMD_USAGE);

	mdb_nv_sort_iter(&mdb.m_disasms, print_dis, NULL, UM_SLEEP | UM_GC);
	return (DCMD_OK);
}

/*ARGSUSED*/
static void
defdis_destroy(mdb_disasm_t *dp)
{
	/* Nothing to do here */
}

/*ARGSUSED*/
static mdb_tgt_addr_t
defdis_ins2str(mdb_disasm_t *dp, mdb_tgt_t *t, mdb_tgt_as_t as,
    char *buf, mdb_tgt_addr_t addr)
{
	return (addr);
}

static const mdb_dis_ops_t defdis_ops = {
	defdis_destroy, defdis_ins2str
};

static int
defdis_create(mdb_disasm_t *dp)
{
	dp->dis_name = "default";
	dp->dis_desc = "default no-op disassembler";
	dp->dis_ops = &defdis_ops;

	return (0);
}

mdb_dis_ctor_f *mdb_dis_builtins[] = {
#ifdef __sparc
	sparc1_create,
	sparc2_create,
	sparc4_create,
	sparcv8_create,
	sparcv9_create,
	sparcv9plus_create,
#endif	/* __sparc */
#ifdef __i386
	ia32_create,
#endif	/* __i386 */
	defdis_create,
	NULL
};
