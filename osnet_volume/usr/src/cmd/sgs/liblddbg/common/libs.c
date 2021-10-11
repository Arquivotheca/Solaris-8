/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)libs.c	1.18	99/05/27 SMI"

#include	"paths.h"
#include	"msg.h"
#include	"_debug.h"
#include	"libld.h"

static void
Dbg_lib_dir_print(List * libdir)
{
	Listnode *	lnp;
	char *   	cp;

	for (LIST_TRAVERSE(libdir, lnp, cp)) {
		dbg_print(MSG_ORIG(MSG_LIB_FILE), cp);
	}
}

void
Dbg_libs_init(List * ulibdir, List * dlibdir)
{
	if (DBG_NOTCLASS(DBG_LIBS))
		return;

	dbg_print(MSG_INTL(MSG_LIB_INITPATH));
	Dbg_lib_dir_print(ulibdir);
	Dbg_lib_dir_print(dlibdir);
}

void
Dbg_libs_l(const char * name, const char * path)
{
	if (DBG_NOTCLASS(DBG_LIBS))
		return;
	if (DBG_NOTDETAIL())
		return;

	/*
	 * The file name is passed to us as "/libfoo".
	 */
	dbg_print(MSG_INTL(MSG_LIB_LOPT), &name[4], path);
}

void
Dbg_libs_path(const char * path)
{
	if (path == (const char *)0)
		return;
	if (DBG_NOTCLASS(DBG_LIBS))
		return;

	dbg_print(MSG_INTL(MSG_LIB_ENVPATH), path);
}

void
Dbg_libs_req(const char * so_name, const char * ref_file, const char * name)
{
	if (DBG_NOTCLASS(DBG_LIBS))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_INTL(MSG_LIB_REQUIRED), so_name, name,
	    ref_file);
}

void
Dbg_libs_update(List * ulibdir, List * dlibdir)
{
	if (DBG_NOTCLASS(DBG_LIBS))
		return;

	dbg_print(MSG_INTL(MSG_LIB_UPPATH));
	Dbg_lib_dir_print(ulibdir);
	Dbg_lib_dir_print(dlibdir);
}

void
Dbg_libs_yp(const char * path)
{
	if (DBG_NOTCLASS(DBG_LIBS))
		return;

	dbg_print(MSG_INTL(MSG_LIB_LIBPATH), path);
}

void
Dbg_libs_ylu(const char * path, const char * orig, int index)
{
	if (DBG_NOTCLASS(DBG_LIBS))
		return;

	dbg_print(MSG_INTL(MSG_LIB_YPATH), path, orig,
		(index == YLDIR) ? 'L' : 'U');
}

void
Dbg_libs_rpath(const char * name, const char * path)
{
	if (DBG_NOTCLASS(DBG_LIBS))
		return;

	dbg_print(MSG_INTL(MSG_LIB_RPATH), path, name);
}

void
Dbg_libs_dpath(const char * path)
{
	if (DBG_NOTCLASS(DBG_LIBS))
		return;

	dbg_print(MSG_INTL(MSG_LIB_DEFAULT), path);
}

void
Dbg_libs_find(const char * name)
{
	if (DBG_NOTCLASS(DBG_LIBS))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_LIB_FIND), name);
}

void
Dbg_libs_found(const char * path)
{
	if (DBG_NOTCLASS(DBG_LIBS))
		return;

	dbg_print(MSG_INTL(MSG_LIB_TRYING), path);
}

void
Dbg_libs_ignore(const char * path)
{
	if (DBG_NOTCLASS(DBG_LIBS))
		return;

	dbg_print(MSG_INTL(MSG_LIB_IGNORE), path);
}

void
Dbg_libs_audit(const char * opath, const char * npath)
{
	if (DBG_NOTCLASS(DBG_LIBS))
		return;

	if (npath == opath)
		return;
	else if (npath == 0)
		dbg_print(MSG_INTL(MSG_LIB_SKIP), opath);
	else
		dbg_print(MSG_INTL(MSG_LIB_ALTER), npath);
}
