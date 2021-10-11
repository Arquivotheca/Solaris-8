/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_set.c	1.1	99/08/11 SMI"

/*
 * Support for ::set dcmd.  The +/-o option processing code is provided in a
 * stand-alone function so it can be used by the command-line option processing
 * code in mdb_main.c.  This facility provides an easy way for us to add more
 * configurable options without having to add a new dcmd each time.
 */

#include <mdb/mdb_target.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_string.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb.h>

static void
opt_follow_child(int enable)
{
	if (enable)
		mdb.m_flags |= MDB_FL_FCHILD;
	else
		mdb.m_flags &= ~MDB_FL_FCHILD;
}

static void
opt_ignoreeof(int enable)
{
	if (enable)
		mdb.m_flags |= MDB_FL_IGNEOF;
	else
		mdb.m_flags &= ~MDB_FL_IGNEOF;
}

static void
opt_pager(int enable)
{
	if (enable) {
		mdb_iob_setflags(mdb.m_out, MDB_IOB_PGENABLE);
		mdb.m_flags |= MDB_FL_PAGER;
	} else  {
		mdb_iob_clrflags(mdb.m_out, MDB_IOB_PGENABLE);
		mdb.m_flags &= ~MDB_FL_PAGER;
	}
}

static void
opt_repeatlast(int enable)
{
	if (enable)
		mdb.m_flags |= MDB_FL_REPLAST;
	else
		mdb.m_flags &= ~MDB_FL_REPLAST;
}

static void
opt_adb(int enable)
{
	if (enable) {
		mdb.m_flags |= MDB_FL_NOMODS | MDB_FL_ADB;
		(void) mdb_set_prompt("");
	} else {
		mdb.m_flags &= ~(MDB_FL_NOMODS | MDB_FL_ADB);
		if (mdb.m_promptlen == 0)
			(void) mdb_set_prompt("> ");
	}

	opt_repeatlast(enable);
	opt_pager(1 - enable);
}

static void
opt_latest(int enable)
{
	if (enable)
		mdb.m_flags |= MDB_FL_LATEST;
	else
		mdb.m_flags &= ~MDB_FL_LATEST;
}

int
mdb_set_options(const char *s, int enable)
{
	static const struct opdesc {
		const char *opt_name;
		void (*opt_func)(int);
	} opdtab[] = {
		{ "adb", opt_adb },
		{ "follow_child", opt_follow_child },
		{ "ignoreeof", opt_ignoreeof },
		{ "pager", opt_pager },
		{ "repeatlast", opt_repeatlast },
		{ "latest", opt_latest },
		{ NULL, NULL }
	};

	const struct opdesc *opp;
	const char *opt;
	char *buf = strdup(s);
	int status = 1;

	for (opt = strtok(buf, ","); opt != NULL; opt = strtok(NULL, ",")) {
		for (opp = opdtab; opp->opt_name != NULL; opp++) {
			if (strcmp(opt, opp->opt_name) == 0) {
				opp->opt_func(enable ? 1 : 0);
				break;
			}
		}
		if (opp->opt_name == NULL) {
			mdb_warn("invalid debugger option -- %s\n", opt);
			status = 0;
		}
	}

	mdb_free(buf, strlen(s) + 1);
	return (status);
}

static void
print_path(const char **path)
{
	if (path != NULL) {
		for (mdb_printf("%s", *path++); *path != NULL; path++)
			mdb_printf(":%s", *path);
	}
	mdb_printf("\n\n");
}

/*ARGSUSED*/
int
cmd_set(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	const char *opt_I = NULL, *opt_L = NULL, *opt_P = NULL, *opt_o = NULL;
	const char *opt_plus_o = NULL, *opt_D = NULL;
	uint_t opt_w = FALSE, opt_F = FALSE;
	uintptr_t opt_s = (uintptr_t)(long)-1;

	int tflags = 0;
	int i;

	/*
	 * If no options are specified, print out the current set of target
	 * and debugger properties that can be modified with ::set.
	 */
	if (argc == 0) {
		int tflags = mdb_tgt_getflags(mdb.m_target);
		uint_t oflags = mdb_iob_getflags(mdb.m_out) & MDB_IOB_AUTOWRAP;
		char delim = ' ';

		mdb_iob_clrflags(mdb.m_out, MDB_IOB_AUTOWRAP);
		mdb_printf("\n  macro path: ");
		print_path(mdb.m_ipath);
		mdb_printf(" module path: ");
		print_path(mdb.m_lpath);
		mdb_iob_setflags(mdb.m_out, oflags);

		mdb_printf(" symbol matching distance: %lr (%s)\n",
		    mdb.m_symdist, mdb.m_symdist ?
		    "absolute mode" : "smart mode");

		mdb_printf("           command prompt: \"%s\"\n", mdb.m_prompt);

		mdb_printf("         debugger options:");
		if (mdb.m_flags & MDB_FL_ADB) {
			mdb_printf("%c%s", delim, "adb");
			delim = ',';
		}
		if (mdb.m_flags & MDB_FL_FCHILD) {
			mdb_printf("%c%s", delim, "follow_child");
			delim = ',';
		}
		if (mdb.m_flags & MDB_FL_IGNEOF) {
			mdb_printf("%c%s", delim, "ignoreeof");
			delim = ',';
		}
		if (mdb.m_flags & MDB_FL_PAGER) {
			mdb_printf("%c%s", delim, "pager");
			delim = ',';
		}
		if (mdb.m_flags & MDB_FL_REPLAST) {
			mdb_printf("%c%s", delim, "repeatlast");
			delim = ',';
		}
		mdb_printf("\n");

		mdb_printf("           target options: ");
		if (tflags & MDB_TGT_F_RDWR)
			mdb_printf("read-write");
		else
			mdb_printf("read-only");
		if (tflags & MDB_TGT_F_FORCE)
			mdb_printf(", force-attach");
		mdb_printf("\n");

		return (DCMD_OK);
	}

	while ((i = mdb_getopts(argc, argv,
	    'F', MDB_OPT_SETBITS, TRUE, &opt_F,
	    'I', MDB_OPT_STR, &opt_I,
	    'L', MDB_OPT_STR, &opt_L,
	    'P', MDB_OPT_STR, &opt_P,
	    'o', MDB_OPT_STR, &opt_o,
	    's', MDB_OPT_UINTPTR, &opt_s,
	    'w', MDB_OPT_SETBITS, TRUE, &opt_w,
	    'D', MDB_OPT_STR, &opt_D, NULL)) != argc) {

		argv += i; /* skip past args we processed */
		argc -= i; /* adjust argc */

		if (argv[0].a_type != MDB_TYPE_STRING || argc < 2 ||
		    argv[1].a_type != MDB_TYPE_STRING ||
		    strcmp(argv->a_un.a_str, "+o") != 0)
			return (DCMD_USAGE);

		opt_plus_o = argv[1].a_un.a_str;
		argv += 2; /* skip +o string */
		argc -= 2; /* adjust argc */
	}

	/*
	 * Handle -w and -F first: as these options modify the target, they
	 * are the only ::set changes that can potentially fail.
	 */
	if (opt_w == TRUE)
		tflags |= MDB_TGT_F_RDWR;
	if (opt_F == TRUE)
		tflags |= MDB_TGT_F_FORCE;
	if (tflags != 0 && mdb_tgt_setflags(mdb.m_target, tflags) == -1) {
		mdb_warn("failed to re-open target for writing");
		return (DCMD_ERR);
	}

	/*
	 * Now handle everything that either can't fail or we don't care if
	 * it does.  Note that we handle +/-o first in case another option
	 * overrides a change made implicity by a +/-o argument (e.g. -P).
	 */
	if (opt_o != NULL)
		(void) mdb_set_options(opt_o, TRUE);
	if (opt_plus_o != NULL)
		(void) mdb_set_options(opt_plus_o, FALSE);
	if (opt_I != NULL)
		mdb_set_ipath(opt_I);
	if (opt_L != NULL)
		mdb_set_lpath(opt_L);
	if (opt_P != NULL)
		(void) mdb_set_prompt(opt_P);
	if (opt_s != (uintptr_t)-1)
		mdb.m_symdist = (size_t)opt_s;
	if (opt_D != NULL && (i = mdb_dstr2mode(opt_D)) != MDB_DBG_HELP)
		mdb_dmode((uint_t)i);

	return (DCMD_OK);
}
