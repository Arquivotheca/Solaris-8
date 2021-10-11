/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)version.c	1.14	98/10/23 SMI"

#include	<link.h>
#include	<stdio.h>
#include	"msg.h"
#include	"_debug.h"

#if	!defined(_ELF64)
/*
 * Print out the version section entries.
 */
void
Gelf_ver_def_print(GElf_Verdef * vdf, GElf_Xword num, const char * names)
{
	GElf_Xword	_num;
	char		index[10];

	dbg_print(MSG_ORIG(MSG_VER_DEF_2));

	for (_num = 1; _num <= num; _num++,
	    vdf = (GElf_Verdef *)((GElf_Xword)vdf + vdf->vd_next)) {

		GElf_Half	cnt = vdf->vd_cnt - 1;
		GElf_Half	ndx = vdf->vd_ndx;
		GElf_Verdaux *	vdap = (GElf_Verdaux *)((GElf_Xword)vdf +
			vdf->vd_aux);
		const char *	name, * dep;

		/*
		 * Obtain the name and first dependency (if any).
		 */
		name = (char *)(names + vdap->vda_name);
		vdap = (GElf_Verdaux *)((GElf_Xword)vdap + vdap->vda_next);
		if (cnt)
			dep = (char *)(names + vdap->vda_name);
		else
			dep = MSG_ORIG(MSG_STR_EMPTY);

		(void) sprintf(index, MSG_ORIG(MSG_FMT_INDEX), ndx);
		dbg_print(MSG_ORIG(MSG_VER_LINE_1), index, name, dep,
		    conv_verflg_str(vdf->vd_flags));

		/*
		 * Print any additional dependencies.
		 */
		if (cnt) {
			vdap = (GElf_Verdaux *)((GElf_Xword)vdap +
				vdap->vda_next);
			for (cnt--; cnt; cnt--,
			    vdap = (GElf_Verdaux *)((GElf_Xword)vdap +
			    vdap->vda_next)) {
				dep = (char *)(names + vdap->vda_name);
				dbg_print(MSG_ORIG(MSG_VER_LINE_2),
				    MSG_ORIG(MSG_STR_EMPTY), dep);
			}
		}
	}
}

void
Gelf_ver_need_print(GElf_Verneed * vnd, GElf_Xword num, const char * names)
{
	GElf_Xword	_num;

	dbg_print(MSG_ORIG(MSG_VER_NEED_2));

	for (_num = 1; _num <= num; _num++,
	    vnd = (GElf_Verneed *)((GElf_Xword)vnd + vnd->vn_next)) {

		GElf_Half	cnt = vnd->vn_cnt;
		GElf_Vernaux *	vnap = (GElf_Vernaux *)((GElf_Xword)vnd +
			vnd->vn_aux);
		const char *	name, * dep;

		/*
		 * Obtain the name of the needed file and the version name
		 * within it that we're dependent on.  Note that the count
		 * should be at least one, otherwise this is a pretty bogus
		 * entry.
		 */
		name = (char *)(names + vnd->vn_file);
		if (cnt)
			dep = (char *)(names + vnap->vna_name);
		else
			dep = MSG_INTL(MSG_STR_NULL);

		dbg_print(MSG_ORIG(MSG_VER_LINE_1), MSG_ORIG(MSG_STR_EMPTY),
		    name, dep, conv_verflg_str(vnap->vna_flags));

		/*
		 * Print any additional version dependencies.
		 */
		if (cnt) {
			vnap = (GElf_Vernaux *)((GElf_Xword)vnap +
				vnap->vna_next);
			for (cnt--; cnt; cnt--,
			    vnap = (GElf_Vernaux *)((GElf_Xword)vnap +
			    vnap->vna_next)) {
				dep = (char *)(names + vnap->vna_name);
				dbg_print(MSG_ORIG(MSG_VER_LINE_3),
				    MSG_ORIG(MSG_STR_EMPTY), dep,
				    conv_verflg_str(vnap->vna_flags));
			}
		}
	}
}

void
Dbg_ver_avail_title(const char * file)
{
	if (DBG_NOTCLASS(DBG_VERSIONS))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_VER_AVAIL_1), file);
	dbg_print(MSG_INTL(MSG_VER_AVAIL_2));
}

void
Dbg_ver_def_title(const char * file)
{
	if (DBG_NOTCLASS(DBG_VERSIONS))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_VER_DEF_1), file);
	dbg_print(MSG_ORIG(MSG_VER_DEF_2));
}

void
Dbg_ver_need_title(const char * file)
{
	if (DBG_NOTCLASS(DBG_VERSIONS))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_VER_NEED_1), file);
	dbg_print(MSG_ORIG(MSG_VER_NEED_2));
}

void
Dbg_ver_need_entry(Half cnt, const char * file, const char * version)
{
	if (DBG_NOTCLASS(DBG_VERSIONS))
		return;

	if (cnt == 0)
		dbg_print(MSG_ORIG(MSG_VER_LINE_5), file, version);
	else
		dbg_print(MSG_ORIG(MSG_VER_LINE_4), MSG_ORIG(MSG_STR_EMPTY),
		    version);
}

void
Dbg_ver_symbol(const char * name)
{
	static Boolean	ver_symbol_title = TRUE;

	if (DBG_NOTCLASS(DBG_VERSIONS | DBG_SYMBOLS))
		return;

	if (DBG_NOTCLASS(DBG_VERSIONS))
		if (ver_symbol_title) {
			ver_symbol_title = FALSE;
			dbg_print(MSG_ORIG(MSG_STR_EMPTY));
			dbg_print(MSG_INTL(MSG_SYM_VERSION));
		}

	Dbg_syms_created(name);
}

/*
 * This function doesn't test for any specific debugging category, thus it will
 * be generated for any debugging family.
 */
void
Dbg_ver_nointerface(const char * name)
{
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_VER_NOINTERFACE), name);
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}

#endif	/* !defined(_ELF64) */

/*
 * Print a version descriptor.
 */
void
Dbg_ver_desc_entry(Ver_desc * vdp)
{
	const char *	dep;
	Ver_desc *	_vdp, * __vdp;
	Listnode *	lnp;
	char		index[10];

	if (DBG_NOTCLASS(DBG_VERSIONS))
		return;

	if (vdp->vd_deps.head) {
		_vdp = (Ver_desc *)vdp->vd_deps.head->data;
		dep = _vdp->vd_name;
	} else {
		_vdp = 0;
		dep = MSG_ORIG(MSG_STR_EMPTY);
	}
	(void) sprintf(index, MSG_ORIG(MSG_FMT_INDEX), vdp->vd_ndx);
	dbg_print(MSG_ORIG(MSG_VER_LINE_1), index, vdp->vd_name, dep,
	    conv_verflg_str(vdp->vd_flags));

	/*
	 * Loop through the dependency list in case there are more that one
	 * dependency.
	 */
	for (LIST_TRAVERSE(&vdp->vd_deps, lnp, __vdp)) {
		if (_vdp == __vdp)
			continue;
		dbg_print(MSG_ORIG(MSG_VER_LINE_4), MSG_ORIG(MSG_STR_EMPTY),
		    __vdp->vd_name);
	}
}

void
Dbg_ver_avail_entry(Ver_index * vip, const char * select)
{
	if (DBG_NOTCLASS(DBG_VERSIONS))
		return;

	if (select)
		dbg_print(MSG_ORIG(MSG_VER_SELECTED), vip->vi_name, select);
	else
		dbg_print(MSG_ORIG(MSG_VER_ALL), vip->vi_name);
}
