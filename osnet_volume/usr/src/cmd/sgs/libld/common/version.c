/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)version.c	1.43	98/10/23 SMI"

#include	<string.h>
#include	<stdio.h>
#include	"debug.h"
#include	"msg.h"
#include	"_libld.h"


/*
 * Locate a version descriptor.
 */
Ver_desc *
vers_find(const char * name, Word hash, List * lst)
{
	Listnode *	lnp;
	Ver_desc *	vdp;

	for (LIST_TRAVERSE(lst, lnp, vdp)) {
		if (vdp->vd_hash != hash)
			continue;
		if (strcmp(vdp->vd_name, name) == 0)
			return (vdp);
	}
	return (0);
}

/*
 * Add a new version descriptor to a version descriptor list.  Note, users of
 * this are responsible for determining if the version descriptor already
 * exists (this can reduce the need to allocate storage for descriptor names
 * until it is determined a descriptor need be created (see map_symbol())).
 */
Ver_desc *
vers_desc(const char * name, Word hash, List * lst)
{
	Ver_desc *	vdp;

	if ((vdp = (Ver_desc *)libld_calloc(sizeof (Ver_desc), 1)) == 0)
		return ((Ver_desc *)S_ERROR);

	vdp->vd_name = name;
	vdp->vd_hash = hash;

	if (list_appendc(lst, vdp) == 0)
		return ((Ver_desc *)S_ERROR);
	else
		return (vdp);
}

/*
 * Now that all explict files have been processed validate any version
 * definitions.  Insure that any version references are available (a version
 * has been defined when it's been assigned an index).  Also calculate the
 * number of .version section entries that will be required to hold this
 * information.
 */
#define	_NUM_OF_VERS_	40	/* twice as big as the depth for libc version */
typedef struct {
	Ver_desc **	ver_stk;
	int 		ver_sp;
	int 		ver_lmt;
} Ver_Stack;

static uintptr_t
vers_visit_children(Ver_desc *vp, int flag)
{
	Listnode *		lnp1;
	Ver_desc *		vdp;
	static int		err = 0;
	static Ver_Stack	ver_stk = {0, 0, 0};
	int			tmp_sp;

	/*
	 * If there was any fatal error,
	 * just return.
	 */
	if (err == S_ERROR)
		return (err);

	/*
	 * if this is called from, ver_check_defs(), initialize sp.
	 */
	if (flag == 0)
		ver_stk.ver_sp = 0;

	/*
	 * Check if passed version pointer vp is already in the stack.
	 */
	for (tmp_sp = 0; tmp_sp < ver_stk.ver_sp; tmp_sp++) {
		Ver_desc *v;

		v = ver_stk.ver_stk[tmp_sp];
		if (v == vp) {
			/*
			 * cyclic dependency.
			 */
			if (err == 0) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_VER_CYCLIC));
				err = 1;
			}
			for (tmp_sp = 0; tmp_sp < ver_stk.ver_sp; tmp_sp++) {
				v = ver_stk.ver_stk[tmp_sp];
				if ((v->vd_flags & FLG_VER_CYCLIC) == 0) {
					v->vd_flags |= FLG_VER_CYCLIC;
					eprintf(ERR_NONE,
						MSG_INTL(MSG_VER_ADDVER),
						v->vd_name);
				}
			}
			if ((vp->vd_flags & FLG_VER_CYCLIC) == 0) {
				vp->vd_flags |= FLG_VER_CYCLIC;
				eprintf(ERR_NONE,
					MSG_INTL(MSG_VER_ADDVER),
					vp->vd_name);
			}
			return (err);
		}
	}

	/*
	 * Push version on the stack.
	 */
	if (ver_stk.ver_sp >= ver_stk.ver_lmt) {
		ver_stk.ver_lmt += _NUM_OF_VERS_;
		if ((ver_stk.ver_stk = (Ver_desc **)
		    realloc((void *)ver_stk.ver_stk,
		    ver_stk.ver_lmt * sizeof (Ver_desc *))) == NULL)
			return (S_ERROR);
	}
	ver_stk.ver_stk[(ver_stk.ver_sp)++] = vp;

	/*
	 * Now visit children.
	 */
	for (LIST_TRAVERSE(&vp->vd_deps, lnp1, vdp))
		if (vers_visit_children(vdp, 1) == S_ERROR)
			return (S_ERROR);

	/*
	 * Pop version from the stack.
	 */
	(ver_stk.ver_sp)--;

	return (err);
}

uintptr_t
vers_check_defs(Ofl_desc * ofl)
{
	Listnode *	lnp1, * lnp2;
	Ver_desc *	vdp;
	uintptr_t 	is_cyclic = 0;


	DBG_CALL(Dbg_ver_def_title(ofl->ofl_name));

	/*
	 * First check if there are any cyclic dependency
	 */
	for (LIST_TRAVERSE(&ofl->ofl_verdesc, lnp1, vdp))
		if ((is_cyclic = vers_visit_children(vdp, 0)) == S_ERROR)
			return (S_ERROR);
	if (is_cyclic)
		ofl->ofl_flags |= FLG_OF_FATAL;

	for (LIST_TRAVERSE(&ofl->ofl_verdesc, lnp1, vdp)) {
		Byte		cnt;
		Sym *		sym;
		Sym_desc *	sdp;
		const char *	name = vdp->vd_name;
		unsigned char	bind;
		Ver_desc *	_vdp;

		if (vdp->vd_ndx == 0) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_VER_UNDEF), name,
			    vdp->vd_ref->vd_name,
			    vdp->vd_ref->vd_file->ifl_name);
			ofl->ofl_flags |= FLG_OF_FATAL;
			continue;
		}

		DBG_CALL(Dbg_ver_desc_entry(vdp));

		/*
		 * If a version definition contains no symbols this is possibly
		 * a mapfile error.
		 */
		if ((vdp->vd_flags &
		    (VER_FLG_BASE | VER_FLG_WEAK | FLG_VER_REFER)) == 0)
			DBG_CALL(Dbg_ver_nointerface(vdp->vd_name));

		/*
		 * Update the version entry count to account for this new
		 * version descriptor (the count is the size in bytes).
		 */
		ofl->ofl_verdefsz += (Word)sizeof (Verdef);

		/*
		 * Traverse this versions dependency list to determine what
		 * additional version dependencies we must account for against
		 * this descriptor.
		 */
		cnt = 1;
		for (LIST_TRAVERSE(&vdp->vd_deps, lnp2, _vdp)) {
#if	defined(__lint)
			/* get lint to think `_vdp' is used... */
			lnp2 = (Listnode *)_vdp;
#endif
			cnt++;
		}
		ofl->ofl_verdefsz += (Word)(cnt * sizeof (Verdaux));

		/*
		 * Except for the base version descriptor, generate an absolute
		 * symbol to reflect this version.
		 */
		if (vdp->vd_flags & VER_FLG_BASE)
			continue;

		if (vdp->vd_flags & VER_FLG_WEAK)
			bind = STB_WEAK;
		else
			bind = STB_GLOBAL;
		if (sdp = sym_find(name, vdp->vd_hash, ofl)) {
			/*
			 * If the symbol already exists and is undefined or was
			 * defined in a shared library, convert it to an
			 * absolute.
			 */
			if ((sdp->sd_sym->st_shndx == SHN_UNDEF) ||
			    (sdp->sd_ref != REF_REL_NEED)) {
				sdp->sd_sym->st_shndx = SHN_ABS;
				sdp->sd_sym->st_info =
					ELF_ST_INFO(bind, STT_OBJECT);
				sdp->sd_ref = REF_REL_NEED;
				sdp->sd_flags |= FLG_SY_GLOBAL;
				sdp->sd_aux->sa_verndx = vdp->vd_ndx;
			} else if ((sdp->sd_sym->st_shndx != SHN_ABS) &&
			    (sdp->sd_ref == REF_REL_NEED)) {
				eprintf(ERR_WARNING, MSG_INTL(MSG_VER_DEFINED),
				    name, sdp->sd_file->ifl_name);
			}
		} else {
			/*
			 * If the symbol does not exist create it.
			 */
			if ((sym = (Sym *)libld_calloc(sizeof (Sym), 1)) == 0)
				return (S_ERROR);
			sym->st_shndx = SHN_ABS;
			sym->st_info = ELF_ST_INFO(bind, STT_OBJECT);
			DBG_CALL(Dbg_ver_symbol(name));
			if ((sdp = sym_enter(name, sym, vdp->vd_hash,
			    vdp->vd_file, ofl, 0)) == (Sym_desc *)S_ERROR)
				return (S_ERROR);
			sdp->sd_ref = REF_REL_NEED;
			sdp->sd_flags |= FLG_SY_GLOBAL;
			sdp->sd_aux->sa_verndx = vdp->vd_ndx;
		}
	}
	return (1);
}

/*
 * Dereference dependencies as a part of normalizing (allows recursion).
 */
void
vers_derefer(Ifl_desc * ifl, Ver_desc * vdp, int weak)
{
	Listnode *	lnp;
	Ver_desc *	_vdp;
	Ver_index *	vip = &ifl->ifl_verndx[vdp->vd_ndx];

	/*
	 * If the head of the list was a weak then we only clear out
	 * weak dependencies, but if the head of the list was 'strong'
	 * we clear the REFER bit on all dependencies.
	 */
	if ((weak && (vdp->vd_flags & VER_FLG_WEAK)) || (!weak))
		vip->vi_flags &= ~FLG_VER_REFER;

	for (LIST_TRAVERSE(&vdp->vd_deps, lnp, _vdp))
		vers_derefer(ifl, _vdp, weak);
}

/*
 * If we need to record the versions of any needed dependencies traverse the
 * shared object dependency list and calculate what version needed entries are
 * required.
 */
uintptr_t
vers_check_need(Ofl_desc * ofl)
{
	Listnode *	lnp1;
	Ifl_desc *	ifl;

	/*
	 * Traverse the shared object list looking for dependencies.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_sos, lnp1, ifl)) {
		Listnode *	lnp2;
		Ver_index *	vip;
		Ver_desc *	vdp;
		Sdf_desc *	sdf = ifl->ifl_sdfdesc;
		Byte		cnt, need;

		if (!(ifl->ifl_flags & FLG_IF_NEEDED))
			continue;

		if (ifl->ifl_vercnt <= VER_NDX_GLOBAL)
			continue;

		/*
		 * If version needed definitions were specified in
		 * a mapfile ($SPECVERS=) then record those definitions
		 */
		if (sdf && (sdf->sdf_flags & FLG_SDF_SPECVER)) {
			Sdv_desc *	sdv;
			for (LIST_TRAVERSE(&sdf->sdf_verneed, lnp2,
			    sdv)) {
				ofl->ofl_verneedsz += (Word)sizeof (Vernaux);
				ofl->ofl_dynstrsz +=
					(Word)(strlen(sdv->sdv_name) + 1);
			}
			ifl->ifl_flags |= FLG_IF_VERNEED;
			ofl->ofl_verneedsz += (Word)sizeof (Verneed);
			ofl->ofl_dynstrsz += (Word)(strlen(ifl->ifl_soname)
			    + 1);
			continue;
		}

		/*
		 * Scan the version index list and if any weak version
		 * definition has been referenced by the user promote the
		 * dependency to be non-weak.  Weak version dependencies do not
		 * cause fatal errors from the runtime linker, non-weak
		 * dependencies do.
		 */
		for (need = 0, cnt = 0; cnt <= ifl->ifl_vercnt; cnt++) {
			vip = &ifl->ifl_verndx[cnt];
			vdp = vip->vi_desc;

			if ((vip->vi_flags & (FLG_VER_REFER | VER_FLG_WEAK)) ==
			    (FLG_VER_REFER | VER_FLG_WEAK))
				vdp->vd_flags &= ~VER_FLG_WEAK;

			/*
			 * Mark any weak reference as referred to so as to
			 * simplify normalization and later version dependency
			 * manipulation.
			 */
			if (vip->vi_flags & VER_FLG_WEAK)
				vip->vi_flags |= FLG_VER_REFER;
		}

		/*
		 * Scan the version dependency list to normalize the referenced
		 * dependencies.  Any needed version that is inherited by
		 * another like version is derefereced as it is not necessary
		 * to make this part of the version dependencies.
		 */
		for (LIST_TRAVERSE(&ifl->ifl_verdesc, lnp2, vdp)) {
			Listnode *	lnp3;
			Ver_desc *	_vdp;
			int		type;

			vip = &ifl->ifl_verndx[vdp->vd_ndx];

			if (!(vip->vi_flags & FLG_VER_REFER))
				continue;

			type = vdp->vd_flags & VER_FLG_WEAK;
			for (LIST_TRAVERSE(&vdp->vd_deps, lnp3, _vdp))
				vers_derefer(ifl, _vdp, type);
		}

		/*
		 * Finally, determine how many of the version dependencies need
		 * to be recorded.
		 */
		for (need = 0, cnt = 0; cnt <= ifl->ifl_vercnt; cnt++) {
			vip = &ifl->ifl_verndx[cnt];

			/*
			 * If a version has been referenced then record it as a
			 * version dependency.
			 */
			if (vip->vi_flags & FLG_VER_REFER) {
				ofl->ofl_verneedsz += (Word)sizeof (Vernaux);
				ofl->ofl_dynstrsz += (Word)(strlen(vip->vi_name)
				    + 1);
				need++;
			}
		}

		if (need) {
			ifl->ifl_flags |= FLG_IF_VERNEED;
			ofl->ofl_verneedsz += (Word)sizeof (Verneed);
			ofl->ofl_dynstrsz += (Word)(strlen(ifl->ifl_soname)
			    + 1);
		}
	}

	/*
	 * If no version needed information is required unset the output file
	 * flag.
	 */
	if (ofl->ofl_verneedsz == 0)
		ofl->ofl_flags &= ~FLG_OF_VERNEED;

	return (1);
}

/*
 * Indicate dependency selection (allows recursion).
 */
void
vers_select(Ifl_desc * ifl, Ver_desc * vdp, const char * ref)
{
	Listnode *	lnp;
	Ver_desc *	_vdp;
	Ver_index *	vip = &ifl->ifl_verndx[vdp->vd_ndx];

	vip->vi_flags |= FLG_VER_AVAIL;
	DBG_CALL(Dbg_ver_avail_entry(vip, ref));

	for (LIST_TRAVERSE(&vdp->vd_deps, lnp, _vdp))
		vers_select(ifl, _vdp, ref);
}

Ver_index *
vers_index(Ifl_desc * ifl, int avail)
{
	Listnode *	lnp;
	Ver_desc *	vdp;
	Ver_index *	vip;
	Sdf_desc *	sdf = ifl->ifl_sdfdesc;
	Word		count = ifl->ifl_vercnt;
	Sdv_desc *	sdv;

	/*
	 * Allocate an index array large enough to hold all of the files
	 * version descriptors.
	 */
	if ((vip = (Ver_index *)libld_calloc(sizeof (Ver_index),
	    (count + 1))) == 0)
		return ((Ver_index *)S_ERROR);

	for (LIST_TRAVERSE(&ifl->ifl_verdesc, lnp, vdp)) {
		int		ndx = vdp->vd_ndx;

		vip[ndx].vi_name = vdp->vd_name;
		vip[ndx].vi_desc = vdp;

		/*
		 * Any relocatable object versions, and the `base' version are
		 * always available.
		 */
		if (avail || (vdp->vd_flags & VER_FLG_BASE))
			vip[ndx].vi_flags |= FLG_VER_AVAIL;

		/*
		 * If this is a weak version mark it as such.  Weak versions
		 * are always dragged into any version dependencies created,
		 * and if a weak version is referenced it will be promoted to
		 * a non-weak version dependency.
		 */
		if (vdp->vd_flags & VER_FLG_WEAK)
			vip[ndx].vi_flags |= VER_FLG_WEAK;
		/*
		 * If this version is mentioned in a mapfile
		 * $ADDVERS syntax then add a FLG_IF_NEEDED flag now
		 */
		if (sdf && (sdf->sdf_flags & FLG_SDF_ADDVER)) {
			Listnode *	lnp2;
			for (LIST_TRAVERSE(&sdf->sdf_verneed, lnp2, sdv)) {
				if (strcmp(vip[ndx].vi_name,
				    sdv->sdv_name) == 0) {
					vip[ndx].vi_flags |= FLG_VER_REFER;
					sdv->sdv_flags |= FLG_SDV_MATCHED;
					break;
				}
			}
		}
	}

	/*
	 * if $ADDVER was specified for this object verify that
	 * all of it's dependent upon versions were refered to.
	 */
	if (sdf && (sdf->sdf_flags & FLG_SDF_ADDVER)) {
		int	fail = 0;
		for (LIST_TRAVERSE(&sdf->sdf_verneed, lnp, sdv)) {
			if (!(sdv->sdv_flags & FLG_SDV_MATCHED)) {
				if (fail == 0) {
					fail++;
					eprintf(ERR_NONE,
					    MSG_INTL(MSG_VER_ADDVERS),
					    sdf->sdf_rfile, sdf->sdf_name);
				}
				eprintf(ERR_NONE, MSG_INTL(MSG_VER_ADDVER),
				    sdv->sdv_name);
			}
		}
		if (fail)
			return ((Ver_index *)S_ERROR);
	}

	return (vip);
}

/*
 * Process a version symbol index section.
 */
int
vers_sym_process(Is_desc * isp, Ifl_desc * ifl)
{
	Shdr *	symshdr;
	Shdr *	vershdr = isp->is_shdr;
	/*
	 * Verify that the versym is the same size as the
	 * linked symbol table.  If these two get out of sync
	 * the file is considered corrupted.
	 */
	symshdr = ifl->ifl_isdesc[vershdr->sh_link]->is_shdr;
	if ((symshdr->sh_size / symshdr->sh_entsize) != (vershdr->sh_size /
	    vershdr->sh_entsize)) {
		eprintf(ERR_WARNING, MSG_INTL(MSG_ELF_VERSYM), ifl->ifl_name,
			isp->is_name,
			EC_WORD(vershdr->sh_size / vershdr->sh_entsize),
			ifl->ifl_isdesc[vershdr->sh_link]->is_name,
			EC_WORD(symshdr->sh_size / symshdr->sh_entsize));
		return (1);
	}
	ifl->ifl_versym = (Versym *)isp->is_indata->d_buf;
	return (1);
}

/*
 * Process a version definition section from an input file.  A list of version
 * descriptors is created and associated with the input files descriptor.  If
 * this is a shared object these descriptors will be used to indicate the
 * availability of each version.  If this is a relocatable object then these
 * descriptors will be promoted (concatenated) to the output files image.
 */
uintptr_t
vers_def_process(Is_desc * isp, Ifl_desc * ifl, Ofl_desc * ofl)
{
	const char *	str, * file = ifl->ifl_name;
	Sdf_desc *	sdf = ifl->ifl_sdfdesc;
	Sdv_desc *	sdv;
	Word		num, _num;
	Verdef *	vdf;
	int		relobj;

	/*
	 * If there is no version section then simply indicate that all version
	 * definitions asked for do not exist.
	 */
	if (isp == 0) {
		Listnode *	lnp;

		for (LIST_TRAVERSE(&sdf->sdf_vers, lnp, sdv)) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_VER_NOEXIST),
			    ifl->ifl_name, sdv->sdv_name, sdv->sdv_ref);
			ofl->ofl_flags |= FLG_OF_FATAL;
		}
		return (0);
	}

	vdf = (Verdef *)isp->is_indata->d_buf;

	/*
	 * Verify the version revision.  We only check the first version
	 * structure as it is assumed all other version structures in this
	 * data section will be of the same revision.
	 */
	if (vdf->vd_version > VER_DEF_CURRENT)
		(void) eprintf(ERR_WARNING, MSG_INTL(MSG_VER_HIGHER),
		    ifl->ifl_name, vdf->vd_version, VER_DEF_CURRENT);


	num = isp->is_shdr->sh_info;
	str = (char *)ifl->ifl_isdesc[isp->is_shdr->sh_link]->is_indata->d_buf;

	if (ifl->ifl_ehdr->e_type == ET_REL)
		relobj = 1;
	else
		relobj = 0;

	DBG_CALL(Dbg_ver_def_title(file));

	/*
	 * Loop through the version information setting up a version descriptor
	 * for each version definition.
	 */
	for (_num = 1; _num <= num; _num++,
	    vdf = (Verdef *)((uintptr_t)vdf + vdf->vd_next)) {
		const char *	name;
		Ver_desc *	ivdp, * ovdp = 0;
		Word		hash;
		Half 		cnt = vdf->vd_cnt;
		Half		ndx = vdf->vd_ndx;
		Verdaux *	vdap = (Verdaux *)((uintptr_t)vdf +
				    vdf->vd_aux);

		/*
		 * Keep track of the largest index for use in creating a
		 * version index array later, and create a version descriptor.
		 */
		if (ndx > ifl->ifl_vercnt)
			ifl->ifl_vercnt = ndx;

		name = (char *)(str + vdap->vda_name);
		/* LINTED */
		hash = (Word)elf_hash(name);
		if ((ivdp = vers_find(name, hash, &ifl->ifl_verdesc)) == 0) {
			if ((ivdp = vers_desc(name, hash,
			    &ifl->ifl_verdesc)) == (Ver_desc *)S_ERROR)
				return (S_ERROR);
		}
		ivdp->vd_ndx = ndx;
		ivdp->vd_file = ifl;
		ivdp->vd_flags = vdf->vd_flags;

		/*
		 * If we're processing a relocatable object then this version
		 * definition needs to be propagated to the output file.
		 * Generate a new output file version and associated this input
		 * version to it.  During symbol processing the version index of
		 * the symbol will be promoted from the input file to the output
		 * files version definition.
		 */
		if (relobj) {
			if (!(ofl->ofl_flags & FLG_OF_RELOBJ))
				ofl->ofl_flags |= FLG_OF_PROCRED;

			if ((ivdp->vd_flags & VER_FLG_BASE) == 0) {
				/*
				 * If no version descriptors have yet been set
				 * up, initialize a base version to represent
				 * the output file itself.  This `base' version
				 * catches any internally generated symbols
				 * (_end, _etext, etc.) and
				 * serves to initialize the output version
				 * descriptor count.
				 */
				if (ofl->ofl_vercnt == 0) {
					if (vers_base(ofl) ==
					    (Ver_desc *)S_ERROR)
						return (S_ERROR);
				}
				ofl->ofl_flags |= FLG_OF_VERDEF;
				if ((ovdp = vers_find(name, hash,
				    &ofl->ofl_verdesc)) == 0) {
					if ((ovdp = vers_desc(name, hash,
					    &ofl->ofl_verdesc)) ==
					    (Ver_desc *)S_ERROR)
						return (S_ERROR);

					/* LINTED */
					ovdp->vd_ndx = (Half)++ofl->ofl_vercnt;
					ovdp->vd_file = ifl;
					ovdp->vd_flags = vdf->vd_flags;
				}
			}

			/*
			 * Maintain the association between the input version
			 * descriptor and the output version descriptor so that
			 * an associated symbols will be assigned to the
			 * correct version.
			 */
			ivdp->vd_ref = ovdp;
		}

		/*
		 * Process any dependencies this version may have.
		 */
		vdap = (Verdaux *)((uintptr_t)vdap + vdap->vda_next);
		for (cnt--; cnt; cnt--,
		    vdap = (Verdaux *)((uintptr_t)vdap + vdap->vda_next)) {
			Ver_desc *	_ivdp;

			name = (char *)(str + vdap->vda_name);
			/* LINTED */
			hash = (Word)elf_hash(name);

			if ((_ivdp = vers_find(name, hash,
			    &ifl->ifl_verdesc)) == 0) {
				if ((_ivdp = vers_desc(name, hash,
				    &ifl->ifl_verdesc)) ==
				    (Ver_desc *)S_ERROR)
					return (S_ERROR);
			}
			if (list_appendc(&ivdp->vd_deps, _ivdp) == 0)
				return (S_ERROR);
		}
		DBG_CALL(Dbg_ver_desc_entry(ivdp));
	}

	/*
	 * Now that we know the total number of version definitions for this
	 * file, build an index array for fast access when processing symbols.
	 */
	if ((ifl->ifl_verndx = vers_index(ifl, relobj)) == (Ver_index *)S_ERROR)
		return (S_ERROR);

	if (relobj)
		return (1);

	/*
	 * If this object has version control definitions against it then these
	 * must be processed so as to select those version definitions to which
	 * symbol bindings can occur.  Otherwise simply mark all versions as
	 * available.
	 */
	DBG_CALL(Dbg_ver_avail_title(file));

	if (sdf && (sdf->sdf_flags & FLG_SDF_SELECT)) {
		Listnode *	lnp1;

		for (LIST_TRAVERSE(&sdf->sdf_vers, lnp1, sdv)) {
			Listnode *	lnp2;
			Ver_desc *	vdp;
			int		found = 0;

			for (LIST_TRAVERSE(&ifl->ifl_verdesc, lnp2, vdp)) {
				if (strcmp(sdv->sdv_name, vdp->vd_name) == 0) {
					found++;
					break;
				}
			}
			if (found)
				vers_select(ifl, vdp, sdv->sdv_ref);
			else {
				eprintf(ERR_FATAL, MSG_INTL(MSG_VER_NOEXIST),
				    ifl->ifl_name, sdv->sdv_name, sdv->sdv_ref);
				ofl->ofl_flags |= FLG_OF_FATAL;
			}
		}
	} else {
		Ver_index *	vip;
		int		cnt;

		for (cnt = VER_NDX_GLOBAL; cnt <= ifl->ifl_vercnt; cnt++) {
			vip = &ifl->ifl_verndx[cnt];
			vip->vi_flags |= FLG_VER_AVAIL;
			DBG_CALL(Dbg_ver_avail_entry(vip, 0));
		}
	}

	/*
	 * If this is an explict dependency indicate that this file is a
	 * candidate for requiring version needed information to be recorded in
	 * the image we're creating.
	 */
	if (ifl->ifl_flags & FLG_IF_NEEDED)
		ofl->ofl_flags |= FLG_OF_VERNEED;

	return (1);
}

/*
 * Process a version needed section.
 */
uintptr_t
vers_need_process(Is_desc * isp, Ifl_desc * ifl, Ofl_desc * ofl)
{
	const char *	str, * file = ifl->ifl_name;
	Word		num, _num;
	Verneed *	vnd;

	vnd = (Verneed *)isp->is_indata->d_buf;

	/*
	 * Verify the version revision.  We only check the first version
	 * structure as it is assumed all other version structures in this
	 * data section will be of the same revision.
	 */
	if (vnd->vn_version > VER_DEF_CURRENT)
		(void) eprintf(ERR_WARNING, MSG_INTL(MSG_VER_HIGHER),
		    ifl->ifl_name, vnd->vn_version, VER_DEF_CURRENT);


	num = isp->is_shdr->sh_info;
	str = (char *)ifl->ifl_isdesc[isp->is_shdr->sh_link]->is_indata->d_buf;

	DBG_CALL(Dbg_ver_need_title(file));

	/*
	 * Loop through the version information setting up a version descriptor
	 * for each version definition.
	 */
	for (_num = 1; _num <= num; _num++,
	    vnd = (Verneed *)((uintptr_t)vnd + vnd->vn_next)) {
		Sdf_desc *	sdf;
		Sdv_desc *	sdv;
		const char *	name;
		Half		cnt = vnd->vn_cnt;
		Vernaux *	vnap = (Vernaux *)((uintptr_t)vnd +
				    vnd->vn_aux);
		Half		_cnt;

		name = (char *)(str + vnd->vn_file);

		/*
		 * Set up a shared object descriptor and add to it the necessary
		 * needed versions.  This information may also have been added
		 * by a mapfile (see map_dash()).
		 */
		if ((sdf = sdf_find(name, &ofl->ofl_soneed)) == 0) {
			if ((sdf = sdf_add(name, &ofl->ofl_soneed)) ==
			    (Sdf_desc *)S_ERROR)
				return (S_ERROR);
			sdf->sdf_rfile = file;
			sdf->sdf_flags |= FLG_SDF_VERIFY;
		}

		for (_cnt = 0; cnt; _cnt++, cnt--,
		    vnap = (Vernaux *)((uintptr_t)vnap + vnap->vna_next)) {
			if (!(sdv =
			    (Sdv_desc *)libld_calloc(sizeof (Sdv_desc), 1)))
				return (S_ERROR);
			sdv->sdv_name = str + vnap->vna_name;
			sdv->sdv_ref = file;
			if (list_appendc(&sdf->sdf_vers, sdv) == 0)
				return (S_ERROR);
			DBG_CALL(Dbg_ver_need_entry(_cnt, name, sdv->sdv_name));
		}
	}

	return (1);
}

/*
 * If a symbol is obtained from a versioned relocatable object then the symbols
 * version association must be promoted to the version definition as it will be
 * represented in the output file.
 */
void
vers_promote(Sym_desc * sdp, Word ndx, Ifl_desc * ifl, Ofl_desc * ofl)
{
	Half 	vndx;

	/*
	 * A version symbol index of 0 implies the symbol is local.  A value of
	 * VER_NDX_GLOBAL implies the symbol is global but has not been
	 * assigned to a specfic version definition.
	 */
	vndx = ifl->ifl_versym[ndx];
	if (vndx == 0) {
		sdp->sd_flags |= (FLG_SY_LOCAL | FLG_SY_REDUCED);
		return;
	}

	if (vndx == VER_NDX_ELIMINATE) {
		sdp->sd_flags |= (FLG_SY_LOCAL | FLG_SY_ELIM | FLG_SY_REDUCED);
		return;
	}

	if (vndx == VER_NDX_GLOBAL) {
		if (!(sdp->sd_flags & FLG_SY_LOCAL)) {
			sdp->sd_flags |= FLG_SY_GLOBAL;
			sdp->sd_aux->sa_verndx = VER_NDX_GLOBAL;
		}
		return;
	}

	/*
	 * Any other version index requires association to the appropriate
	 * version definition.
	 */
	if ((ifl->ifl_verndx == 0) || (vndx > ifl->ifl_vercnt)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_VER_INVALNDX), sdp->sd_name,
		    ifl->ifl_name, vndx);
		ofl->ofl_flags |= FLG_OF_FATAL;
		return;
	}

	if (!(sdp->sd_flags & FLG_SY_LOCAL))
		sdp->sd_flags |= FLG_SY_GLOBAL;

	/*
	 * Promote the symbols version index to the appropriate output version
	 * definition.
	 */
	if (!(sdp->sd_flags & FLG_SY_VERSPROM)) {
		Ver_index *	vip;

		vip = &ifl->ifl_verndx[vndx];
		sdp->sd_aux->sa_verndx = vip->vi_desc->vd_ref->vd_ndx;
		sdp->sd_flags |= FLG_SY_VERSPROM;
	}
}

/*
 * If any versioning is called for make sure an initial version descriptor is
 * assigned to represent the file itself.  Known as the base version.
 */
Ver_desc *
vers_base(Ofl_desc * ofl)
{
	Ver_desc *	vdp;
	const char *	name;

	/*
	 * Determine the filename to associate to the version descriptor.  This
	 * is either the SONAME (if one has been supplied) or the basename of
	 * the output file.
	 */
	if ((name = ofl->ofl_soname) == 0) {
		const char *	str = ofl->ofl_name;

		while (*str != '\0') {
			if (*str++ == '/')
				name = str;
		}
		if (name == 0)
			name = ofl->ofl_name;
	}

	/*
	 * Generate the version descriptor.
	 */
	/* LINTED */
	if ((vdp = vers_desc(name, (Word)elf_hash(name), &ofl->ofl_verdesc)) ==
	    (Ver_desc *)S_ERROR)
		return ((Ver_desc *)S_ERROR);

	/*
	 * Assign the base index to this version and initialize the output file
	 * descriptor with the number of version descriptors presently in use.
	 */
	vdp->vd_ndx = ofl->ofl_vercnt = VER_NDX_GLOBAL;
	vdp->vd_flags |= VER_FLG_BASE;

	return (vdp);
}

/*
 * Now that all input shared objects have been processed, verify that all
 * version requirements have been met.  Any version control requirements will
 * have been specified by the user (and placed on the ofl_oscntl list) and are
 * verified at the time the object was processed (see ver_def_process()).
 * Here we process all version requirements established from shared objects
 * themselves (ie,. NEEDED dependencies).
 */
int
vers_verify(Ofl_desc * ofl)
{
	Listnode *	lnp1;
	Sdf_desc *	sdf;

	/*
	 * As with the runtime environment, disable all version verification if
	 * requested.
	 */
	if (getenv(MSG_ORIG(MSG_LD_NOVERSION)) != NULL)
		return (1);

	for (LIST_TRAVERSE(&ofl->ofl_soneed, lnp1, sdf)) {
		Listnode *	lnp2;
		Sdv_desc *	sdv;
		Ifl_desc *	ifl = sdf->sdf_file;

		if (!(sdf->sdf_flags & FLG_SDF_VERIFY))
			continue;

		/*
		 * If this file contains no version definitions then ignore
		 * any versioning verification.  This is the same model as
		 * carried out by ld.so.1 and is intended to allow backward
		 * compatibility should a shared object with a version
		 * requirment be returned to an older system on which a
		 * non-versioned shared object exists.
		 */
		if ((ifl == 0) || (ifl->ifl_verdesc.head == 0))
			continue;

		/*
		 * If individual versions were specified for this file make
		 * sure that they actually exist in the appropriate file, and
		 * that they are available for binding.
		 */
		for (LIST_TRAVERSE(&sdf->sdf_vers, lnp2, sdv)) {
			Listnode *	lnp3;
			Ver_desc *	vdp;
			int		found = 0;

			for (LIST_TRAVERSE(&ifl->ifl_verdesc, lnp3, vdp)) {
				if (strcmp(sdv->sdv_name, vdp->vd_name) == 0) {
					found++;
					break;
				}
			}
			if (found) {
				Ver_index *	vip;

				vip = &ifl->ifl_verndx[vdp->vd_ndx];
				if (!(vip->vi_flags & FLG_VER_AVAIL)) {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_VER_UNAVAIL),
					    ifl->ifl_name, sdv->sdv_name,
					    sdv->sdv_ref);
					ofl->ofl_flags |= FLG_OF_FATAL;
				}
			} else {
				eprintf(ERR_FATAL, MSG_INTL(MSG_VER_NOEXIST),
				    ifl->ifl_name, sdv->sdv_name, sdv->sdv_ref);
				ofl->ofl_flags |= FLG_OF_FATAL;
			}
		}
	}
	return (1);
}
