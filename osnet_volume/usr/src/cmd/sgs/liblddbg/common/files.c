/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)files.c	1.46	99/09/14 SMI"

#include	"_synonyms.h"

#include	<string.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<limits.h>
#include	<stdio.h>
#include	"msg.h"
#include	"_debug.h"
#include	"libld.h"

static int	bind_title = 0;

void
Dbg_file_generic(Ifl_desc * ifl)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_INTL(MSG_FIL_BASIC), ifl->ifl_name,
		conv_etype_str(ifl->ifl_ehdr->e_type));
}

void
Dbg_file_skip(const char * nname, const char * oname)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	if (oname)
		dbg_print(MSG_INTL(MSG_FIL_SKIP_1), nname, oname);
	else
		dbg_print(MSG_INTL(MSG_FIL_SKIP_2), nname);
}

void
Dbg_file_reuse(const char * nname, const char * oname)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_INTL(MSG_FIL_REUSE), nname, oname);
}

/*
 * This function doesn't test for any specific debugging category, thus it will
 * be generated for any debugging family.
 */
void
Dbg_file_unused(const char * name)
{
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_UNUSED), name);
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}

void
Dbg_file_archive(const char * name, int again)
{
	const char *	str;

	if (DBG_NOTCLASS(DBG_FILES))
		return;

	if (again)
		str = MSG_INTL(MSG_STR_AGAIN);
	else
		str = MSG_ORIG(MSG_STR_EMPTY);

	dbg_print(MSG_INTL(MSG_FIL_ARCHIVE), name, str);
}

void
Dbg_file_analyze(const char * name, int mode)
{
	int	_mode = mode;

	if (DBG_NOTCLASS(DBG_FILES))
		return;

	if (DBG_NOTDETAIL())
		_mode &= RTLD_GLOBAL;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_ANALYZE), name, conv_dlmode_str(_mode));

	bind_title = 0;
}

void
Dbg_file_aout(const char * name, unsigned long dynamic, unsigned long base,
	unsigned long size)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_INTL(MSG_FIL_AOUT), name);
	dbg_print(MSG_INTL(MSG_FIL_DATA_1), EC_XWORD(dynamic),
	    EC_ADDR(base), EC_XWORD(size));
}

void
Dbg_file_elf(const char * name, unsigned long dynamic, unsigned long base,
	unsigned long size, unsigned long entry, unsigned long phdr,
	unsigned int phnum, Lmid_t lmid)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_INTL(MSG_FIL_ELF), name);
	dbg_print(MSG_INTL(MSG_FIL_DATA_1), EC_XWORD(dynamic),
	    EC_ADDR(base), EC_XWORD(size));
	dbg_print(MSG_INTL(MSG_FIL_DATA_2), EC_XWORD(entry),
	    EC_ADDR(phdr), EC_WORD(phnum));
	dbg_print(MSG_INTL(MSG_FIL_DATA_3), EC_XWORD(lmid));
}

void
Dbg_file_ldso(const char * name, unsigned long dynamic, unsigned long base,
	unsigned long envp, unsigned long auxv)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_LDSO), name);
	dbg_print(MSG_INTL(MSG_FIL_DATA_4), EC_XWORD(dynamic),
	    EC_ADDR(base));
	dbg_print(MSG_INTL(MSG_FIL_DATA_5), EC_ADDR(envp), EC_ADDR(auxv));
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}

void
Dbg_file_prot(const char * name, int prot)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_PROT), name, (prot ? '+' : '-'));
}

void
Dbg_file_delete(const char * name)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_DELETE), name);
}

static Msg	bind_str = MSG_STR_EMPTY;

void
Dbg_file_bind_title(int type)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;
	if (DBG_NOTDETAIL())
		return;

	bind_title = 1;

	/*
	 * Establish a binding title for later use in Dbg_file_bind_entry.
	 */
	if (type == REF_NEEDED)
	    bind_str = MSG_FIL_BND_NEED;    /* MSG_INTL(MSG_FIL_BND_NEED) */
	else if (type == REF_SYMBOL)
	    bind_str = MSG_FIL_BND_SYM;	    /* MSG_INTL(MSG_FIL_BND_SYM) */
	else if (type == REF_DLCLOSE)
	    bind_str = MSG_FIL_BND_DLCLOSE; /* MSG_INTL(MSG_FIL_BND_DLCLOSE) */
	else if (type == REF_DELETE)
	    bind_str = MSG_FIL_BND_DELETE;  /* MSG_INTL(MSG_FIL_BND_DELETE) */
	else if (type == REF_ORPHAN)
	    bind_str = MSG_FIL_BND_ORPHAN;  /* MSG_INTL(MSG_FIL_BND_ORPHAN */
	else
	    bind_title = 0;
}

void
Dbg_file_bind_entry(Rt_map * clmp, Rt_map * nlmp)
{
	Permit *	permit = PERMIT(nlmp);
	const char *	str;

	if (DBG_NOTCLASS(DBG_FILES))
		return;
	if (DBG_NOTDETAIL())
		return;

	/*
	 * If this is the first time here print out a binding title.
	 */
	if (bind_title) {
		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_INTL(bind_str), NAME(clmp));
		bind_title = 0;
	}

	/* LINTED */
	dbg_print(MSG_INTL(MSG_FIL_REFCNT), EC_WORD(COUNT(nlmp)), NAME(nlmp));

	if (MODE(nlmp) & RTLD_GLOBAL)
		str = MSG_INTL(MSG_FIL_GLOBAL);
	else
		str = MSG_ORIG(MSG_STR_EMPTY);

	if (permit) {
		unsigned long	_cnt, cnt = permit->p_cnt;
		unsigned long *	value = &permit->p_value[0];

		dbg_print(MSG_INTL(MSG_FIL_PERMIT_1), EC_XWORD(*value), str);
		for (_cnt = 1, value++; _cnt < cnt; _cnt++, value++)
			dbg_print(MSG_INTL(MSG_FIL_PERMIT_2), EC_XWORD(*value));
	} else
		dbg_print(MSG_INTL(MSG_FIL_PERMIT_3), str);
}

void
Dbg_file_bind_needed(Rt_map * clmp)
{
	Listnode *	lnp;
	Rt_map *	nlmp;

	if (DBG_NOTCLASS(DBG_FILES))
		return;
	if (DBG_NOTDETAIL())
		return;

	/*
	 * Traverse the callers dependency lists.
	 */
	Dbg_file_bind_title(REF_NEEDED);
	for (LIST_TRAVERSE(&EDEPENDS(clmp), lnp, nlmp))
		Dbg_file_bind_entry(clmp, nlmp);
	for (LIST_TRAVERSE(&IDEPENDS(clmp), lnp, nlmp))
		Dbg_file_bind_entry(clmp, nlmp);
}

void
Dbg_file_dlopen(const char * name, const char * from, int mode)
{
	int	_mode = mode;

	if (DBG_NOTCLASS(DBG_FILES))
		return;

	if (DBG_NOTDETAIL())
		_mode &= RTLD_GLOBAL;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_DLOPEN), name, from, conv_dlmode_str(_mode));

	bind_title = 0;
}

void
Dbg_file_dlclose(const char * name, int ign)
{
	const char *	str;

	if (DBG_NOTCLASS(DBG_FILES))
		return;

	if (ign)
		str = MSG_INTL(MSG_STR_IGNORE);
	else
		str = MSG_ORIG(MSG_STR_EMPTY);

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_DLCLOSE), name, str);
}

void
Dbg_file_dldump(const char * ipath, const char * opath, int flags)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_DLDUMP), ipath, opath,
		conv_dlflag_str(flags));
}

void
Dbg_file_lazyload(const char * file, const char * from, const char * symname)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_LAZYLOAD), file, from, symname);
}

void
Dbg_file_nl()
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}

void
Dbg_file_preload(const char * name)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_INTL(MSG_FIL_PRELOAD), name);
}

void
Dbg_file_needed(const char * name, const char * parent)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_NEEDED), name, parent);
}

void
Dbg_file_filter(const char * name, const char * filter)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	if (filter) {
		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_INTL(MSG_FIL_FILTER_1), name, filter);
	} else {
		dbg_print(MSG_INTL(MSG_FIL_FILTER_2), name);
		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	}
}

void
Dbg_file_fixname(const char * oname, const char * nname)
{
	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_FIXNAME), oname, nname);
}

void
Dbg_file_output(Ofl_desc * ofl)
{
	const char *	prefix = MSG_ORIG(MSG_PTH_OBJECT);
	char	*	oname, * nname, * ofile;
	int		fd;

	if (DBG_NOTCLASS(DBG_FILES))
		return;
	if (DBG_NOTDETAIL())
		return;

	/*
	 * Obtain the present input object filename for concatenation to the
	 * prefix name.
	 */
	oname = (char *)ofl->ofl_name;
	if ((ofile = strrchr(oname, '/')) == NULL)
		ofile = oname;
	else
		ofile++;

	/*
	 * Concatenate the prefix with the object filename, open the file and
	 * write out the present Elf memory image.  As this is debugging we
	 * ignore all errors.
	 */
	if ((nname = (char *)malloc(strlen(prefix) + strlen(ofile) + 1)) != 0) {
		(void) strcpy(nname, prefix);
		(void) strcat(nname, ofile);
		if ((fd = open(nname, O_RDWR | O_CREAT | O_TRUNC, 0666)) != -1)
			(void) write(fd, ofl->ofl_ehdr, ofl->ofl_size);
	}
}

void
Dbg_file_config_dis(const char * config, int features)
{
	const char *	str;
	int		error = features & ~DBG_CONF_FEATMSK;

	if (error == DBG_CONF_IGNORE)
		str = MSG_INTL(MSG_FIL_CONFIG_ERR_1);
	else if (error == DBG_CONF_VERSION)
		str = MSG_INTL(MSG_FIL_CONFIG_ERR_2);
	else if (error == DBG_CONF_PRCFAIL)
		str = MSG_INTL(MSG_FIL_CONFIG_ERR_3);
	else if (error == DBG_CONF_CORRUPT)
		str = MSG_INTL(MSG_FIL_CONFIG_ERR_4);
	else
		str = conv_config_str(features);

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_FIL_CONFIG_ERR), config, str);
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}

void
Dbg_file_config_obj(const char * dir, const char * file, const char * config)
{
	char *	name, _name[PATH_MAX];

	if (DBG_NOTCLASS(DBG_FILES))
		return;

	if (file) {
		(void) sprintf(_name, MSG_ORIG(MSG_FMT_PATH), dir, file);
		name = _name;
	} else
		name = (char *)dir;

	dbg_print(MSG_INTL(MSG_FIL_CONFIG), name, config);
}

#if	!defined(_ELF64)
void
Dbg_file_rejected(const char * name, int why, uint_t what)
{
	const char * msg = NULL;

	if (DBG_NOTCLASS(DBG_FILES))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));

	if (why == DBG_IFL_ERR_TYPE)
		dbg_print(MSG_INTL(MSG_FIL_ERR_TYPE), name,
		    conv_etype_str((Half)what));
	else if (why == DBG_IFL_ERR_MACH)
		dbg_print(MSG_INTL(MSG_FIL_ERR_MACH), name,
		    conv_emach_str((Half)what));
	else if (why == DBG_IFL_ERR_CLASS)
		msg = MSG_INTL(MSG_FIL_ERR_CLASS);
	else if (why == DBG_IFL_ERR_LIBVER)
		msg = MSG_INTL(MSG_FIL_ERR_LIBVER);
	else if (why == DBG_IFL_ERR_DATA)
		msg = MSG_INTL(MSG_FIL_ERR_DATA);
	else if (why == DBG_IFL_ERR_FLAGS)
		msg = MSG_INTL(MSG_FIL_ERR_FLAGS);
	else if (why == DBG_IFL_ERR_BADMATCH)
		msg = MSG_INTL(MSG_FIL_ERR_BADMATCH);
	else if (why == DBG_IFL_ERR_HALFLAG)
		msg = MSG_INTL(MSG_FIL_ERR_HALFLAG);
	else if (why == DBG_IFL_ERR_US3)
		msg = MSG_INTL(MSG_FIL_ERR_US3);
	else
		/* shouldn't happen */
		msg = MSG_INTL(MSG_FIL_ERR_REJECTED);

	if (msg != NULL)
		dbg_print(msg, name, what);
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}
#endif
