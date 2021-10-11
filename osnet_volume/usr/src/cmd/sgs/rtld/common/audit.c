/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 * Audit interfaces.  Auditing can be enabled in two ways:
 *
 *	o	Using the LD_AUDIT environment variable
 *
 *	o	From individual objects containing a DT_DEPAUDIT entry
 *		(see ld(1) -P/-p options).
 *
 * The former establishes a global set of audit libraries which can inspect all
 * objects from a given process.  The latter establishes a local set of audit
 * libraries which can inspect the immediate dependencies of the caller.
 *
 * Audit library capabilities are indicated by flags within the link-map list
 * header (for global auditing), see LML_AUD_* flags, or by flags within the
 * individual link-map (for local auditing), see FL1_AU_* flags.  Both sets of
 * flags are defined in include/rtld.h and are made equivalent to simplify for
 * audit interface requirements.  The basic test for all audit interfaces is:
 *
 *    if (((lml->lm_flags | FLAGS1(lmp)) & FL1_AU_intf) && (lml == LIST(lmp)))
 *
 * The latter link-map list equivalence test insures that auditors themselves
 * (invoked through DT_DEPAUDIT) are not audited.
 */
#pragma ident	"@(#)audit.c	1.30	99/09/14 SMI"

#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/lwp.h>
#include	<stdio.h>
#include	<stdarg.h>
#include	<dlfcn.h>
#include	<string.h>
#include	"debug.h"
#include	"_rtld.h"
#include	"_audit.h"
#include	"_elf.h"
#include	"msg.h"

uint_t	audit_flags = 0;		/* Copy of specific audit flags to */
					/* simplify boot_elf.s access. */

static Audit_client *
_audit_client(Audit_info * aip, Rt_map * almp)
{
	int	ndx;

	if (aip == 0)
		return (0);

	for (ndx = 0; ndx < aip->ai_cnt; ndx++) {
		if (aip->ai_clients[ndx].ac_lmp == almp)
			return (&(aip->ai_clients[ndx]));
	}
	return (0);
}

/*
 * la_objsearch() caller.  Traverses through all audit library and calls any
 * la_objsearch() entry points found.
 *
 * Effectively any audit library can change the name we're working with, so we
 * continue to propagate the new name to each audit library.  Any 0 return
 * terminates the search.
 */
static char *
_audit_objsearch(List * list, char * name, Rt_map * clmp, uint_t flags)
{
	Audit_list *	alp;
	Listnode *	lnp;
	char *		nname = (char *)name;

	for (LIST_TRAVERSE(list, lnp, alp)) {
		Audit_client *	acp;

		if (alp->al_objsearch == 0)
			continue;

		if ((acp = _audit_client(AUDINFO(clmp), alp->al_lmp)) == 0)
			continue;
		if ((nname = (*alp->al_objsearch)(nname, &(acp->ac_cookie),
		    flags)) == 0)
			break;
	}
	return (nname);
}

char *
audit_objsearch(Rt_map * clmp, const char * name, uint_t flags)
{
	char *	nname = (char *)name;
	int	appl = 0;

	if ((rtld_flags & RT_FL_APPLIC) == 0)
		appl = rtld_flags |= RT_FL_APPLIC;

	if (auditors && (auditors->ad_flags & LML_AUD_SEARCH))
		nname = _audit_objsearch(&(auditors->ad_list), nname,
			clmp, flags);
	if (nname && AUDITORS(clmp) &&
	    (AUDITORS(clmp)->ad_flags & FL1_AU_SEARCH))
		nname = _audit_objsearch(&(AUDITORS(clmp)->ad_list), nname,
			clmp, flags);

	if (appl)
		rtld_flags &= ~RT_FL_APPLIC;

	return (nname);
}

/*
 * la_activity() caller.  Traverses through all audit library and calls any
 * la_activity() entry points found.
 */
static void
_audit_activity(List * list, Rt_map * clmp, uint_t flags)
{
	Audit_list *	alp;
	Listnode *	lnp;

	for (LIST_TRAVERSE(list, lnp, alp)) {
		Audit_client *	acp;

		if (alp->al_activity == 0)
			continue;

		if ((acp = _audit_client(AUDINFO(clmp), alp->al_lmp)) == 0)
			continue;
		(*alp->al_activity)(&(acp->ac_cookie), flags);
	}
}

void
audit_activity(Rt_map * clmp, uint_t flags)
{
	int	appl = 0;

	/*
	 * We want to trigger the first addition or deletion only.  Ignore any
	 * consistent calls unless a previous addition or deletion occurred.
	 */
	if ((flags == LA_ACT_ADD) || (flags == LA_ACT_DELETE)) {
		if (rtld_flags & RT_FL_AUNOTIF)
			return;
		rtld_flags |= RT_FL_AUNOTIF;
	} else {
		if ((rtld_flags & RT_FL_AUNOTIF) == 0)
			return;
		rtld_flags &= ~RT_FL_AUNOTIF;
	}


	if ((rtld_flags & RT_FL_APPLIC) == 0)
		appl = rtld_flags |= RT_FL_APPLIC;

	if (auditors && (auditors->ad_flags & LML_AUD_ACTIVITY))
		_audit_activity(&(auditors->ad_list), clmp, flags);
	if (AUDITORS(clmp) && (AUDITORS(clmp)->ad_flags & FL1_AU_ACTIVITY))
		_audit_activity(&(AUDITORS(clmp)->ad_list), clmp, flags);

	if (appl)
		rtld_flags &= ~RT_FL_APPLIC;
}


/*
 * la_objopen() caller.  Create an audit information structure for the indicated
 * link-map, regardless of an la_objopen() entry point.  This structure is used
 * to supply information to various audit interfaces (see LML_MSK_AUDINFO).
 * Traverses through all audit library and calls any la_objopen() entry points
 * found.
 */
static int
_audit_objopen(List * list, Rt_map * nlmp, Lmid_t lmid, Audit_info * aip,
    int * ndx)
{
	Audit_list *	alp;
	Listnode *	lnp;

	for (LIST_TRAVERSE(list, lnp, alp)) {
		uint_t		flags;
		Audit_client *	acp;

		/*
		 * Associate a cookie with the audit library, and assign the
		 * initial cookie as the present link-map.
		 */
		acp = &aip->ai_clients[(*ndx)++];
		acp->ac_lmp = alp->al_lmp;
		acp->ac_cookie = (uintptr_t)nlmp;

		DBG_CALL(Dbg_audit_object(alp->al_libname, NAME(nlmp)));

		if (alp->al_objopen == 0)
			continue;

		flags = (*alp->al_objopen)((Link_map *)nlmp, lmid,
			&(acp->ac_cookie));
		if (flags & LA_FLG_BINDTO)
			acp->ac_flags |= FLG_AC_BINDTO;

		if (flags & LA_FLG_BINDFROM) {
			ulong_t		pltcnt;

			acp->ac_flags |= FLG_AC_BINDFROM;
			/*
			 * We only need dynamic plt's if a pltenter and/or a
			 * pltexit() entry point exist in one of our auditing
			 * libraries.
			 */
			if (aip->ai_dynplts || (JMPREL(nlmp) == 0) ||
			    ((audit_flags & (AF_PLTENTER | AF_PLTEXIT)) == 0))
				continue;

			/*
			 * Create one dynplt for every 'PLT' that exists in the
			 * object.
			 */
			pltcnt = PLTRELSZ(nlmp) / RELENT(nlmp);
			if ((aip->ai_dynplts = calloc(pltcnt,
			    dyn_plt_ent_size)) == 0)
				return (0);
		}
	}
	return (1);
}

int
audit_objopen(Rt_map * clmp, Rt_map * nlmp)
{
	Lmid_t		lmid = get_linkmap_id(LIST(nlmp));
	int		appl = 0, clients = 0, respond = 1, ndx = 0;
	Audit_info *	aip;

	/*
	 * Determine the total number of audit libraries in use.  This provides
	 * the number of client structures required for this object.
	 */
	if (auditors)
		clients = auditors->ad_cnt;
	if (AUDITORS(clmp))
		clients += AUDITORS(clmp)->ad_cnt;
	if ((nlmp != clmp) && AUDITORS(nlmp))
		clients += AUDITORS(nlmp)->ad_cnt;

	/*
	 * The initial allocation of the audit information structure includes
	 * an array of audit clients, 1 per audit library presently available.
	 *
	 *			 ---------------
	 *			| ai_cnt	|
	 * 	Audit_info	| ai_clients	|-------
	 *			| ai_dynplts	|	|
	 *			|---------------|	|
	 * 	Audit_client    |	1	|<------
	 *			|---------------|
	 *			|	2	|
	 *			    .........
	 */
	if ((AUDINFO(nlmp) = aip = calloc(1, sizeof (Audit_info) +
	    (sizeof (Audit_client) * clients))) == 0)
		return (0);

	aip->ai_cnt = clients;
	aip->ai_clients = (Audit_client *)((uintptr_t)aip +
		sizeof (Audit_info));


	if ((rtld_flags & RT_FL_APPLIC) == 0)
		appl = rtld_flags |= RT_FL_APPLIC;

	if (auditors)
		respond = _audit_objopen(&(auditors->ad_list), nlmp,
		    lmid, aip, &ndx);
	if (respond && AUDITORS(clmp))
		respond = _audit_objopen(&(AUDITORS(clmp)->ad_list), nlmp,
		    lmid, aip, &ndx);
	if (respond && (nlmp != clmp) && AUDITORS(nlmp))
		respond = _audit_objopen(&(AUDITORS(nlmp)->ad_list), nlmp,
		    lmid, aip, &ndx);

	if (appl)
		rtld_flags &= ~RT_FL_APPLIC;

	return (respond);
}

/*
 * la_objclose() caller.  Traverses through all audit library and calls any
 * la_objclose() entry points found.
 */
void
_audit_objclose(List * list, Rt_map * lmp)
{
	Audit_list *	alp;
	Listnode *	lnp;

	for (LIST_TRAVERSE(list, lnp, alp)) {
		Audit_client *	acp;

		if (alp->al_objclose == 0)
			continue;

		if ((acp = _audit_client(AUDINFO(lmp), alp->al_lmp)) == 0)
			continue;
		(*alp->al_objclose)(&(acp->ac_cookie));
	}
}

void
audit_objclose(Rt_map * clmp, Rt_map * lmp)
{
	int	appl = 0;

	if ((rtld_flags & RT_FL_APPLIC) == 0)
		appl = rtld_flags |= RT_FL_APPLIC;

	if (auditors && (auditors->ad_flags & LML_AUD_CLOSE))
		_audit_objclose(&(auditors->ad_list), lmp);
	if (AUDITORS(clmp) && (AUDITORS(clmp)->ad_flags & FL1_AU_CLOSE))
		_audit_objclose(&(AUDITORS(clmp)->ad_list), lmp);

	if (appl)
		rtld_flags &= ~RT_FL_APPLIC;
}

/*
 * la_pltenter() caller.  Traverses through all audit library and calls any
 * la_pltenter() entry points found.  NOTE: this routine is called via the
 * glue code established in elf_plt_trace_write(), the symbol descripter is
 * created as part of the glue and for 32bit environments the st_name is a
 * pointer to the real symbol name (ie. it's already been adjusted with the
 * objects base offset).  For 64bit environments the st_name remains the
 * original symbol offset and in this case it is used to compute the real name
 * pointer and pass as a separate argument to the auditor.
 */
static void
_audit_pltenter(List * list, Rt_map * rlmp, Rt_map * dlmp, Sym * sym,
    uint_t ndx, void * regs, uint_t * flags)
{
	Audit_list *	alp;
	Listnode *	lnp;
#if	defined(_LP64)
	const char *	name = (const char *)(sym->st_name + STRTAB(dlmp));
#else
	const char *	name = (const char *)(sym->st_name);
#endif

	for (LIST_TRAVERSE(list, lnp, alp)) {
		Audit_client *	racp;
		Audit_client *	dacp;
		Addr		prev = sym->st_value;

		if ((racp = _audit_client(AUDINFO(rlmp), alp->al_lmp)) == 0)
			continue;
		if ((dacp = _audit_client(AUDINFO(dlmp), alp->al_lmp)) == 0)
			continue;
		if ((alp->al_pltenter == 0) ||
		    ((racp->ac_flags & FLG_AC_BINDFROM) == 0) ||
		    ((dacp->ac_flags & FLG_AC_BINDTO) == 0))
			continue;

		sym->st_value = (Addr)(*alp->al_pltenter)(sym, ndx,
		    &(racp->ac_cookie), &(dacp->ac_cookie), regs,
#if	defined(_LP64)
		    flags, name);
#else
		    flags);
#endif
		DBG_CALL(Dbg_audit_symval(alp->al_libname,
		    MSG_ORIG(MSG_AUD_PLTENTER), name, prev, sym->st_name));
	}
}

Addr
audit_pltenter(Rt_map * rlmp, Rt_map * dlmp, Sym * sym, uint_t ndx,
    void * regs, uint_t * flags)
{
	Sym	_sym = *sym;
	int	_appl = 0;

	if ((rtld_flags & RT_FL_APPLIC) == 0)
		_appl = rtld_flags |= RT_FL_APPLIC;

	if (auditors && (auditors->ad_flags & LML_AUD_PLTENTER))
		_audit_pltenter(&(auditors->ad_list), rlmp, dlmp, &_sym,
		    ndx, regs, flags);
	if (AUDITORS(rlmp) && (AUDITORS(rlmp)->ad_flags & FL1_AU_PLTENTER))
		_audit_pltenter(&(AUDITORS(rlmp)->ad_list), rlmp, dlmp, &_sym,
		    ndx, regs, flags);

	if (_appl)
		rtld_flags &= ~RT_FL_APPLIC;

	return (_sym.st_value);
}


/*
 * la_pltexit() caller.  Traverses through all audit library and calls any
 * la_pltexit() entry points found.  See notes above (_audit_pltenter) for
 * discussion on st_name.
 */
static Addr
_audit_pltexit(List * list, uintptr_t retval, Rt_map * rlmp, Rt_map * dlmp,
    Sym * sym, uint_t ndx)
{
	Audit_list *	alp;
	Listnode *	lnp;
#if	defined(_LP64)
	const char *	name = (const char *)(sym->st_name + STRTAB(dlmp));
#endif

	for (LIST_TRAVERSE(list, lnp, alp)) {
		Audit_client *	racp;
		Audit_client *	dacp;

		if ((racp = _audit_client(AUDINFO(rlmp), alp->al_lmp)) == 0)
			continue;
		if ((dacp = _audit_client(AUDINFO(dlmp), alp->al_lmp)) == 0)
			continue;
		if ((alp->al_pltexit == 0) ||
		    ((racp->ac_flags & FLG_AC_BINDFROM) == 0) ||
		    ((dacp->ac_flags & FLG_AC_BINDTO) == 0))
			continue;

		retval = (*alp->al_pltexit)(sym, ndx,
		    &(racp->ac_cookie), &(dacp->ac_cookie),
#if	defined(_LP64)
		    retval, name);
#else
		    retval);
#endif
	}
	return (retval);
}

Addr
audit_pltexit(uintptr_t retval, Rt_map * rlmp, Rt_map * dlmp, Sym * sym,
    uint_t ndx)
{
	uintptr_t	_retval = retval;
	int		_appl = 0;

	if ((rtld_flags & RT_FL_APPLIC) == 0)
		_appl = rtld_flags |= RT_FL_APPLIC;

	if (auditors && (auditors->ad_flags & LML_AUD_PLTEXIT))
		_retval = _audit_pltexit(&(auditors->ad_list), _retval,
			rlmp, dlmp, sym, ndx);
	if (AUDITORS(rlmp) && (AUDITORS(rlmp)->ad_flags & FL1_AU_PLTEXIT))
		_retval = _audit_pltexit(&(AUDITORS(rlmp)->ad_list), _retval,
			rlmp, dlmp, sym, ndx);

	if (_appl)
		rtld_flags &= ~RT_FL_APPLIC;

	return (_retval);
}


/*
 * la_symbind() caller.  Traverses through all audit library and calls any
 * la_symbind() entry points found.
 */
Addr
_audit_symbind(List * list, Rt_map * rlmp, Rt_map * dlmp, Sym * sym, uint_t ndx,
    uint_t * flags)
{
	Audit_list *	alp;
	Listnode *	lnp;
#if	defined(_LP64)
	const char *	name = (const char *)(sym->st_name + STRTAB(dlmp));
#else
	const char *	name = (const char *)(sym->st_name);
#endif

	for (LIST_TRAVERSE(list, lnp, alp)) {
		Audit_client *	racp;
		Audit_client *	dacp;
		Addr		prev = sym->st_value;

		if ((racp = _audit_client(AUDINFO(rlmp), alp->al_lmp)) == 0)
			continue;
		if ((dacp = _audit_client(AUDINFO(dlmp), alp->al_lmp)) == 0)
			continue;
		if (((racp->ac_flags & FLG_AC_BINDFROM) == 0) ||
		    ((dacp->ac_flags & FLG_AC_BINDTO) == 0))
			continue;

		/*
		 * BINDTO & BINDFROM are set for this object, thus we must
		 * enable all tracing unless disabled by la_symbind().
		 */
		*flags &= ~(LA_SYMB_NOPLTENTER | LA_SYMB_NOPLTEXIT);

		sym->st_value = (*alp->al_symbind)(sym, ndx,
		    &(racp->ac_cookie), &(dacp->ac_cookie),
#if	defined(_LP64)
		    flags, name);
#else
		    flags);
#endif
		if ((prev != sym->st_value) && (alp->al_vernum >= LAV_VERSION2))
			*flags |= LA_SYMB_ALTVALUE;

		DBG_CALL(Dbg_audit_symval(alp->al_libname,
		    MSG_ORIG(MSG_AUD_SYMBIND), name, prev, sym->st_value));
	}
	return (sym->st_value);
}

Addr
audit_symbind(Rt_map * rlmp, Rt_map * dlmp, Sym * sym, uint_t ndx, Addr value,
    uint_t * flags)
{
	Sym	_sym;
	int	_appl = 0;

	/*
	 * Construct a new symbol from that supplied but with the real address.
	 * In the 64-bit world the st_name field is only 32-bits which isn't
	 * big enough to hold a character pointer. We pass this pointer as a
	 * separate parameter for 64-bit audit libraries.
	 */
	_sym = *sym;
	_sym.st_value = value;

#if	!defined(_LP64)
	_sym.st_name += (Word)STRTAB(dlmp);
#endif

	/*
	 * Initialize the flags value passed to us.  This value is later used
	 * by la_plt_enter().  Its initial value may be altered by la_symbind()
	 * in preparation for these next calls to la_plt_enter().
	 */
	*flags = LA_SYMB_NOPLTENTER | LA_SYMB_NOPLTEXIT;

	if ((rtld_flags & RT_FL_APPLIC) == 0)
		_appl = rtld_flags |= RT_FL_APPLIC;

	if (auditors && (auditors->ad_flags & LML_AUD_SYMBIND))
		_sym.st_value = _audit_symbind(&(auditors->ad_list),
		    rlmp, dlmp, &_sym, ndx, flags);
	if (AUDITORS(rlmp) && (AUDITORS(rlmp)->ad_flags & FL1_AU_SYMBIND))
		_sym.st_value = _audit_symbind(&(AUDITORS(rlmp)->ad_list),
		    rlmp, dlmp, &_sym, ndx, flags);

	if (_appl)
		rtld_flags &= ~RT_FL_APPLIC;

	return (_sym.st_value);
}


/*
 * la_preinit() caller.  Traverses through all audit library and calls any
 * la_preinit() entry points found.
 */
static void
_audit_preinit(List * list, Rt_map * clmp)
{
	Audit_list *	alp;
	Listnode *	lnp;

	for (LIST_TRAVERSE(list, lnp, alp)) {
		Audit_client *	acp;

		if (alp->al_preinit == 0)
			continue;

		if ((acp = _audit_client(AUDINFO(clmp), alp->al_lmp)) == 0)
			continue;
		(*alp->al_preinit)(&(acp->ac_cookie));
	}
}

void
audit_preinit(Rt_map * clmp)
{
	int	appl = 0;

	if ((rtld_flags & RT_FL_APPLIC) == 0)
		appl = rtld_flags |= RT_FL_APPLIC;

	if (auditors && (auditors->ad_flags & LML_AUD_PREINIT))
		_audit_preinit(&(auditors->ad_list), clmp);
	if (AUDITORS(clmp) && (AUDITORS(clmp)->ad_flags & FL1_AU_PREINIT))
		_audit_preinit(&(AUDITORS(clmp)->ad_list), clmp);

	if (appl)
		rtld_flags &= ~RT_FL_APPLIC;
}


/*
 * Clean up (free) an audit descriptor.
 */
void
audit_desc_cleanup(Audit_desc * adp)
{
	Audit_list *	alp;
	Listnode *	lnp, * olnp;

	if (adp == 0)
		return;
	if (adp->ad_name)
		free(adp->ad_name);

	olnp = 0;
	for (LIST_TRAVERSE(&(adp->ad_list), lnp, alp)) {
		(void) dlclose_core(HANDLE(alp->al_lmp), alp->al_lmp, 0);
		free(alp);
		if (olnp)
			free(olnp);
		olnp = lnp;
	}
	if (olnp)
		free(olnp);
	free(adp);
}

/*
 * Clean up (free) an audit information structure.
 */
void
audit_info_cleanup(Audit_info * aip)
{
	if (aip == 0)
		return;

	if (aip->ai_dynplts)
		free(aip->ai_dynplts);
	free(aip);
}

static Addr
audit_symget(Rt_map * lmp, const char * sname, const char * lname)
{
	Rt_map *	_lmp;
	Sym *		sym;
	Addr		addr;
	Slookup		sl;

	sl.sl_name = sname;
	sl.sl_permit = 0;
	sl.sl_cmap = lml_rtld.lm_head;
	sl.sl_imap = lmp;
	sl.sl_rsymndx = 0;
	if ((sym = LM_LOOKUP_SYM(lmp)(&sl, &_lmp,
	    (LKUP_DEFT | LKUP_FIRST))) == (Sym *) 0) {
		return (0);
	}
	addr = sym->st_value;
	if (!(FLAGS(lmp) & FLG_RT_FIXED))
		addr += ADDR(lmp);

	DBG_CALL(Dbg_audit_interface(lname, sname));
	return (addr);
}

/*
 * Given a list of one or more audit libraries, open each one and establish a
 * a descriptor representing the entry points it provides.
 */
int
audit_setup(Rt_map * clmp, Audit_desc * adp)
{
	char *		ptr, * next;
#if	defined(__sparcv9)
	const char *	pltenterstr = MSG_ORIG(MSG_SYM_LAV9PLTENTER);
#elif	defined(__sparc)
	const char *	pltenterstr = MSG_ORIG(MSG_SYM_LAV8PLTENTER);
#else
	const char *	pltenterstr = MSG_ORIG(MSG_SYM_LAX86PLTENTER);
#endif
	int		error = 1;

	DBG_CALL(Dbg_audit_lib(adp->ad_name));

	/*
	 * The audit definitions may be a list (which will already have been
	 * dupped) so split it into individual tokens.
	 */
	ptr = strtok_r(adp->ad_name, MSG_ORIG(MSG_STR_DELIMIT), &next);
	do {
		Dl_handle *	dlp;
		Rt_map *	lmp;
		Rt_map **	tobj;
		Audit_list *	alp;

		/*
		 * If this is a secure application only allow simple filenames
		 * to be specified.  The lookup for these files will be
		 * restricted, but is allowed by placing auditing objects in
		 * secure directories.
		 */
		if (rtld_flags & RT_FL_SECURE) {
			if (strchr(ptr, '/')) {
				DBG_CALL(Dbg_libs_ignore(ptr));
				continue;
			}
		}

		/*
		 * Open the audit library on its own link-map.
		 */
		if ((dlp = dlmopen_core((Lm_list *)LM_ID_NEWLM, ptr,
		    (RTLD_GLOBAL | RTLD_WORLD), clmp, FLG_RT_AUDIT)) == 0) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_AUD_DISABLED), ptr);
			error = 0;
			continue;
		}
		lmp = HDLHEAD(dlp);

		/*
		 * Allocate an audit list descriptor for this object and
		 * search for all known entry points.
		 */
		if ((alp = calloc(1, sizeof (Audit_list))) == 0) {
			(void) dlclose_core(dlp, lmp, 0);
			error = 0;
			continue;
		}
		alp->al_libname = ptr;
		alp->al_lmp = lmp;

		/*
		 * All audit library must handshake through la_version().
		 * Determine that the symbol exists, finish initializing the
		 * object, and then call the function.
		 */
		if ((alp->al_version = (uint_t(*)(uint_t))audit_symget(lmp,
		    MSG_ORIG(MSG_SYM_LAVERSION), alp->al_libname)) == 0) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_NOSYM),
				MSG_ORIG(MSG_SYM_LAVERSION));
			eprintf(ERR_FATAL, MSG_INTL(MSG_AUD_DISABLED), ptr);
			(void) dlclose_core(dlp, lmp, 0);
			free(alp);
			error = 0;
			continue;
		}

		if ((tobj = tsort(lmp, LIST(lmp)->lm_init, RT_SORT_REV)) ==
		    (Rt_map **)S_ERROR) {
			(void) dlclose_core(dlp, lmp, 0);
			free(alp);
			error = 0;
			continue;
		}

		rtld_flags |= RT_FL_APPLIC;
		if (tobj != (Rt_map **)NULL)
			call_init(tobj);

		alp->al_vernum = alp->al_version(LAV_CURRENT);
		rtld_flags &= ~RT_FL_APPLIC;

		if ((alp->al_vernum < LAV_VERSION1) ||
		    (alp->al_vernum > LAV_CURRENT)) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_AUD_BADVERS),
				LAV_CURRENT, alp->al_vernum);
			eprintf(ERR_FATAL, MSG_INTL(MSG_AUD_DISABLED), ptr);
			(void) dlclose_core(dlp, lmp, 0);
			free(alp);
			error = 0;
			continue;
		}

		if (list_append(&(adp->ad_list), alp) == 0) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_AUD_DISABLED), ptr);
			(void) dlclose_core(dlp, lmp, 0);
			free(alp);
			error = 0;
			continue;
		}
		adp->ad_cnt++;
		DBG_CALL(Dbg_audit_version(ptr, alp->al_vernum));

		/*
		 * Collect any remaining entry points.
		 */
		if (alp->al_preinit = (void(*)(uintptr_t *))audit_symget(lmp,
		    MSG_ORIG(MSG_SYM_LAPREINIT), alp->al_libname))
			adp->ad_flags |= LML_AUD_PREINIT;

		if (alp->al_objsearch = (char * (*)(const char *, uintptr_t *,
		    uint_t)) audit_symget(lmp, MSG_ORIG(MSG_SYM_LAOBJSEARCH),
		    alp->al_libname))
			adp->ad_flags |= LML_AUD_SEARCH;

		if (alp->al_objopen = (uint_t(*)(Link_map *, Lmid_t,
		    uintptr_t *)) audit_symget(lmp, MSG_ORIG(MSG_SYM_LAOBJOPEN),
		    alp->al_libname))
			adp->ad_flags |= LML_AUD_OPEN;

		if (alp->al_objclose = (uint_t(*)(uintptr_t *))
		    audit_symget(lmp, MSG_ORIG(MSG_SYM_LAOBJCLOSE),
		    alp->al_libname))
			adp->ad_flags |= LML_AUD_CLOSE;

		if (alp->al_activity = (void (*)(uintptr_t *, uint_t))
		    audit_symget(lmp, MSG_ORIG(MSG_SYM_LAACTIVITY),
		    alp->al_libname))
			adp->ad_flags |= LML_AUD_ACTIVITY;

#if	defined(_LP64)
		if (alp->al_symbind = (uintptr_t(*)(Sym *, uint_t,
		    uintptr_t *, uintptr_t *, uint_t *, const char *))
		    audit_symget(lmp, MSG_ORIG(MSG_SYM_LASYMBIND64),
		    alp->al_libname))
#else
		if (alp->al_symbind = (uintptr_t(*)(Sym *, uint_t,
		    uintptr_t *, uintptr_t *, uint_t *))
		    audit_symget(lmp, MSG_ORIG(MSG_SYM_LASYMBIND),
		    alp->al_libname))
#endif
			adp->ad_flags |= LML_AUD_SYMBIND;

#if	defined(_LP64)
		if (alp->al_pltenter = (uintptr_t(*)(Sym *, uint_t,
		    uintptr_t *, uintptr_t *, void *, uint_t *, const char *))
		    audit_symget(lmp, pltenterstr, alp->al_libname)) {
#else
		if (alp->al_pltenter = (uintptr_t(*)(Sym *, uint_t,
		    uintptr_t *, uintptr_t *, void *, uint_t *))
		    audit_symget(lmp, pltenterstr, alp->al_libname)) {
#endif
			adp->ad_flags |= LML_AUD_PLTENTER;
			audit_flags |= AF_PLTENTER;
		}
#if	defined(_LP64)
		if (alp->al_pltexit = (uintptr_t(*)(Sym *, uint_t,
		    uintptr_t *, uintptr_t *, uintptr_t, const char *))
		    audit_symget(lmp, MSG_ORIG(MSG_SYM_LAPLTEXIT64),
		    alp->al_libname)) {
#else
		if (alp->al_pltexit = (uintptr_t(*)(Sym *, uint_t,
		    uintptr_t *, uintptr_t *, uintptr_t))
		    audit_symget(lmp, MSG_ORIG(MSG_SYM_LAPLTEXIT),
		    alp->al_libname)) {
#endif
			adp->ad_flags |= LML_AUD_PLTEXIT;
			audit_flags |= AF_PLTEXIT;
		}
	} while ((ptr = strtok_r(NULL,
	    MSG_ORIG(MSG_STR_DELIMIT), &next)) != NULL);

	/*
	 * Free the original audit string, as this descriptor may be used again
	 * to add additional auditing.
	 */
	free(adp->ad_name);
	adp->ad_name = 0;

	return (error);
}
