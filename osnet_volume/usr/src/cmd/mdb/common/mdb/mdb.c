/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb.c	1.2	99/11/19 SMI"

/*
 * Modular Debugger (MDB)
 *
 * Refer to the white paper "A Modular Debugger for Solaris" for information
 * on the design, features, and goals of MDB.  See /shared/sac/PSARC/1999/169
 * for copies of the paper and related documentation.
 *
 * This file provides the basic construction and destruction of the debugger's
 * global state, as well as the main execution loop, mdb_run().  MDB maintains
 * a stack of execution frames (mdb_frame_t's) that keep track of its current
 * state, including a stack of input and output buffers, walk and memory
 * garbage collect lists, and a list of commands (mdb_cmd_t's).  As the
 * parser consumes input, it fills in a list of commands to execute, and then
 * invokes mdb_call(), below.  A command consists of a dcmd, telling us
 * what function to execute, and a list of arguments and other invocation-
 * specific data.  Each frame may have more than one command, kept on a list,
 * when multiple commands are separated by | operators.  New frames may be
 * stacked on old ones by nested calls to mdb_run: this occurs when, for
 * example, in the middle of processing one input source (such as a file
 * or the terminal), we invoke a dcmd that in turn calls mdb_eval().  mdb_eval
 * will construct a new frame whose input source is the string passed to
 * the eval function, and then execute this frame to completion.
 */

#include <sys/param.h>
#include <stropts.h>

#define	_MDB_PRIVATE
#include <mdb/mdb.h>

#include <mdb/mdb_context.h>
#include <mdb/mdb_argvec.h>
#include <mdb/mdb_signal.h>
#include <mdb/mdb_module.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_string.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb_frame.h>
#include <mdb/mdb_conf.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_lex.h>
#include <mdb/mdb_io.h>

/*
 * Macro for testing if a dcmd's return status (x) indicates that we should
 * abort the current loop or pipeline.
 */
#define	DCMD_ABORTED(x)	((x) == DCMD_USAGE || (x) == DCMD_ABORT)

extern const mdb_dcmd_t mdb_dcmd_builtins[];
extern mdb_dis_ctor_f *mdb_dis_builtins[];

/*
 * Variable discipline for toggling MDB_FL_PSYM based on the value of the
 * undocumented '_' variable.  Once adb(1) has been removed from the system,
 * we should just remove this functionality and always disable PSYM for macros.
 */
static uintmax_t
psym_disc_get(const mdb_var_t *v)
{
	int i = (mdb.m_flags & MDB_FL_PSYM) ? 1 : 0;
	int j = (MDB_NV_VALUE(v) != 0) ? 1 : 0;

	if ((i ^ j) == 0)
		MDB_NV_VALUE((mdb_var_t *)v) = j ^ 1;

	return (MDB_NV_VALUE(v));
}

static void
psym_disc_set(mdb_var_t *v, uintmax_t value)
{
	if (value == 0)
		mdb.m_flags |= MDB_FL_PSYM;
	else
		mdb.m_flags &= ~MDB_FL_PSYM;

	MDB_NV_VALUE(v) = value;
}

/*
 * Variable discipline for making <1 (most recent offset) behave properly.
 */
static uintmax_t
roff_disc_get(const mdb_var_t *v)
{
	return (MDB_NV_VALUE(v));
}

static void
roff_disc_set(mdb_var_t *v, uintmax_t value)
{
	mdb_nv_set_value(mdb.m_proffset, MDB_NV_VALUE(v));
	MDB_NV_VALUE(v) = value;
}

static char *
path_to_str(char *buf, const char **path)
{
	int i;

	buf[0] = '\0';

	if (path != NULL) {
		for (i = 0; path[i] != NULL; i++) {
			(void) strcat(buf, path[i]);
			(void) strcat(buf, ":");
		}
	}

	return (buf);
}

const char **
mdb_path_alloc(const char *s, const char **opath,
    size_t opathlen, size_t *newlen)
{
	char *opathstr = mdb_alloc(opathlen + 1, UM_NOSLEEP);
	char *format = mdb_alloc(strlen(s) * 2 + 1, UM_NOSLEEP);

	const char **path;
	char *p, *q;

	struct utsname uts;
	size_t len;
	int i;

	mdb_arg_t arg_i, arg_o, arg_p, arg_r, arg_t, arg_R, arg_V;
	mdb_argvec_t argv;

	static const char *empty_path[] = { NULL };

	if (opathstr == NULL || format == NULL)
		goto nomem;

	while (*s == ':')
		s++; /* strip leading delimiters */

	if (*s == '\0') {
		*newlen = 0;
		return (empty_path);
	}

	(void) strcpy(format, s);
	mdb_argvec_create(&argv);

	/*
	 * %i embedded in path string expands to ISA.
	 */
	arg_i.a_type = MDB_TYPE_STRING;
	if (mdb.m_target != NULL)
		arg_i.a_un.a_str = mdb_tgt_isa(mdb.m_target);
	else
		arg_i.a_un.a_str = mdb_conf_isa();

	/*
	 * %o embedded in path string expands to the previous path setting.
	 */
	arg_o.a_type = MDB_TYPE_STRING;
	arg_o.a_un.a_str = path_to_str(opathstr, opath);

	/*
	 * %p embedded in path string expands to the platform name.
	 */
	arg_p.a_type = MDB_TYPE_STRING;
	if (mdb.m_target != NULL)
		arg_p.a_un.a_str = mdb_tgt_platform(mdb.m_target);
	else
		arg_p.a_un.a_str = mdb_conf_platform();

	/*
	 * %r embedded in path string expands to root directory, or
	 * to the empty string if root is "/" (to avoid // in paths).
	 */
	arg_r.a_type = MDB_TYPE_STRING;
	arg_r.a_un.a_str = strcmp(mdb.m_root, "/") ? mdb.m_root : "";

	/*
	 * %t embedded in path string expands to the target name.
	 */
	arg_t.a_type = MDB_TYPE_STRING;
	arg_t.a_un.a_str = mdb.m_target ? mdb_tgt_name(mdb.m_target) : "none";

	/*
	 * %R and %V expand to uname -r (release) and uname -v (version).
	 */
	if (mdb.m_target == NULL || mdb_tgt_uname(mdb.m_target, &uts) < 0)
		mdb_conf_uname(&uts);

	arg_R.a_type = MDB_TYPE_STRING;
	arg_R.a_un.a_str = uts.release;

	arg_V.a_type = MDB_TYPE_STRING;
	if (mdb.m_flags & MDB_FL_LATEST)
		arg_V.a_un.a_str = "latest";
	else
		arg_V.a_un.a_str = uts.version;

	/*
	 * In order to expand the buffer, we examine the format string for
	 * our % tokens and construct an argvec, replacing each % token
	 * with %s along the way.  If we encounter an unknown token, we
	 * shift over the remaining format buffer and stick in %%.
	 */
	for (q = format; (q = strchr(q, '%')) != NULL; q++) {
		switch (q[1]) {
		case 'i':
			mdb_argvec_append(&argv, &arg_i);
			*++q = 's';
			break;
		case 'o':
			mdb_argvec_append(&argv, &arg_o);
			*++q = 's';
			break;
		case 'p':
			mdb_argvec_append(&argv, &arg_p);
			*++q = 's';
			break;
		case 'r':
			mdb_argvec_append(&argv, &arg_r);
			*++q = 's';
			break;
		case 't':
			mdb_argvec_append(&argv, &arg_t);
			*++q = 's';
			break;
		case 'R':
			mdb_argvec_append(&argv, &arg_R);
			*++q = 's';
			break;
		case 'V':
			mdb_argvec_append(&argv, &arg_V);
			*++q = 's';
			break;
		default:
			bcopy(q + 1, q + 2, strlen(q));
			*++q = '%';
		}
	}

	/*
	 * We're now ready to use our printf engine to format the final string.
	 * Take one lap with a NULL buffer to determine how long the final
	 * string will be, allocate it, and format it.
	 */
	len = mdb_iob_asnprintf(NULL, 0, format, argv.a_data);
	if ((p = mdb_alloc(len + 1, UM_NOSLEEP)) != NULL)
		(void) mdb_iob_asnprintf(p, len + 1, format, argv.a_data);
	else
		goto nomem;

	mdb_argvec_zero(&argv);
	mdb_argvec_destroy(&argv);

	mdb_free(format, strlen(s) * 2 + 1);
	mdb_free(opathstr, opathlen + 1);
	opathstr = format = NULL;

	/*
	 * Compress the string to exclude any leading delimiters.
	 */
	for (q = p; *q == ':'; q++)
		continue;
	if (q != p)
		bcopy(q, p, strlen(q) + 1);

	/*
	 * Count up the number of delimited elements.  A sequence of
	 * consecutive delimiters is only counted once.
	 */
	for (i = 1, q = p; (q = strchr(q, ':')) != NULL; i++) {
		while (*q == ':')
			q++;
	}

	if ((path = mdb_alloc(sizeof (char *) * (i + 1), UM_NOSLEEP)) == NULL) {
		mdb_free(p, len + 1);
		goto nomem;
	}

	for (i = 0, q = strtok(p, ":"); q != NULL; q = strtok(NULL, ":"))
		path[i++] = q;

	path[i] = NULL;
	*newlen = len + 1;
	return (path);

nomem:
	warn("failed to allocate memory for path");
	if (opathstr != NULL)
		mdb_free(opathstr, opathlen + 1);
	if (format != NULL)
		mdb_free(format, strlen(s) * 2 + 1);
	*newlen = 0;
	return (empty_path);
}

void
mdb_path_free(const char *path[], size_t pathlen)
{
	int i;

	for (i = 0; path[i] != NULL; i++)
		continue; /* count up the number of path elements */

	if (i > 0) {
		mdb_free((void *)path[0], pathlen);
		mdb_free(path, sizeof (char *) * (i + 1));
	}
}

void
mdb_set_ipath(const char *path)
{
	const char **opath = mdb.m_ipath;
	size_t opathlen = mdb.m_ipathlen;

	(void) strncpy(mdb.m_ipathstr, path, MAXPATHLEN);
	mdb.m_ipathstr[MAXPATHLEN - 1] = 0;
	mdb.m_ipath = mdb_path_alloc(path, opath, opathlen, &mdb.m_ipathlen);

	if (opath != NULL)
		mdb_path_free(opath, opathlen);
}

void
mdb_set_lpath(const char *path)
{
	const char **opath = mdb.m_lpath;
	size_t opathlen = mdb.m_lpathlen;

	(void) strncpy(mdb.m_lpathstr, path, MAXPATHLEN);
	mdb.m_lpathstr[MAXPATHLEN - 1] = 0;
	mdb.m_lpath = mdb_path_alloc(path, opath, opathlen, &mdb.m_lpathlen);

	if (opath != NULL)
		mdb_path_free(opath, opathlen);
}

int
mdb_set_prompt(const char *p)
{
	size_t len = strlen(p);

	if (len > MDB_PROMPTLEN) {
		warn("prompt may not exceed %d characters\n", MDB_PROMPTLEN);
		return (0);
	}

	(void) strcpy(mdb.m_prompt, p);
	mdb.m_promptlen = len;
	return (1);
}

void
mdb_create(const char *execname, const char *arg0)
{
	static const mdb_nv_disc_t psym_disc = { psym_disc_set, psym_disc_get };
	static const mdb_nv_disc_t roff_disc = { roff_disc_set, roff_disc_get };

	static char rootdir[MAXPATHLEN];
	static mdb_frame_t frame0;

	const mdb_dcmd_t *dcp;
	mdb_module_t *mp;
	int i;

	bzero(&mdb, sizeof (mdb_t));

	mdb.m_flags = MDB_FL_PSYM | MDB_FL_PAGER;
	mdb.m_radix = MDB_DEF_RADIX;
	mdb.m_nargs = MDB_DEF_NARGS;
	mdb.m_histlen = MDB_DEF_HISTLEN;

	mdb.m_pname = strbasename(arg0);
	if (strcmp(mdb.m_pname, "adb") == 0) {
		mdb.m_flags |= MDB_FL_NOMODS | MDB_FL_ADB | MDB_FL_REPLAST;
		mdb.m_flags &= ~MDB_FL_PAGER;
	} else
		(void) mdb_set_prompt("> ");

	mdb.m_ipathstr = mdb_zalloc(MAXPATHLEN, UM_SLEEP);
	mdb.m_lpathstr = mdb_zalloc(MAXPATHLEN, UM_SLEEP);

#ifdef _LP64
	(void) strcpy(mdb.m_ipathstr,
	    "%r/usr/platform/%p/lib/adb/%i:%r/usr/lib/adb/%i");

	(void) strcpy(mdb.m_lpathstr,
	    "%r/usr/platform/%p/lib/mdb/%t/%i:%r/usr/lib/mdb/%t/%i");
#else /* _LP64 */
	(void) strcpy(mdb.m_ipathstr,
	    "%r/usr/platform/%p/lib/adb:%r/usr/lib/adb");

	(void) strcpy(mdb.m_lpathstr,
	    "%r/usr/platform/%p/lib/mdb/%t:%r/usr/lib/mdb/%t");
#endif /* _LP64 */

	(void) strncpy(rootdir, execname, sizeof (rootdir));
	rootdir[sizeof (rootdir) - 1] = '\0';
	(void) strdirname(rootdir);

	if (strcmp(strbasename(rootdir), "sparcv9") == 0 ||
	    strcmp(strbasename(rootdir), "sparcv7") == 0 ||
	    strcmp(strbasename(rootdir), "ia64") == 0 ||
	    strcmp(strbasename(rootdir), "i86") == 0)
		(void) strdirname(rootdir);

	if (strcmp(strbasename(rootdir), "bin") == 0) {
		(void) strdirname(rootdir);
		if (strcmp(strbasename(rootdir), "usr") == 0)
			(void) strdirname(rootdir);
	} else
		(void) strcpy(rootdir, "/");

	mdb.m_root = rootdir;

	mdb.m_rminfo.mi_dvers = MDB_API_VERSION;
	mdb.m_rminfo.mi_dcmds = mdb_dcmd_builtins;
	mdb.m_rminfo.mi_walkers = NULL;

	mdb_nv_create(&mdb.m_rmod.mod_walkers);
	mdb_nv_create(&mdb.m_rmod.mod_dcmds);

	mdb.m_rmod.mod_name = mdb.m_pname;
	mdb.m_rmod.mod_info = &mdb.m_rminfo;

	mdb_nv_create(&mdb.m_disasms);
	mdb_nv_create(&mdb.m_modules);
	mdb_nv_create(&mdb.m_dcmds);
	mdb_nv_create(&mdb.m_walkers);
	mdb_nv_create(&mdb.m_nv);

	mdb.m_dot = mdb_nv_insert(&mdb.m_nv, ".", NULL, 0, MDB_NV_PERSIST);
	mdb.m_rvalue = mdb_nv_insert(&mdb.m_nv, "0", NULL, 0, MDB_NV_PERSIST);

	mdb.m_roffset =
	    mdb_nv_insert(&mdb.m_nv, "1", &roff_disc, 0, MDB_NV_PERSIST);

	mdb.m_proffset = mdb_nv_insert(&mdb.m_nv, "2", NULL, 0, MDB_NV_PERSIST);
	mdb.m_rcount = mdb_nv_insert(&mdb.m_nv, "9", NULL, 0, MDB_NV_PERSIST);

	(void) mdb_nv_insert(&mdb.m_nv, "b", NULL, 0, MDB_NV_PERSIST);
	(void) mdb_nv_insert(&mdb.m_nv, "d", NULL, 0, MDB_NV_PERSIST);
	(void) mdb_nv_insert(&mdb.m_nv, "e", NULL, 0, MDB_NV_PERSIST);
	(void) mdb_nv_insert(&mdb.m_nv, "m", NULL, 0, MDB_NV_PERSIST);
	(void) mdb_nv_insert(&mdb.m_nv, "t", NULL, 0, MDB_NV_PERSIST);
	(void) mdb_nv_insert(&mdb.m_nv, "_", &psym_disc, 0, MDB_NV_PERSIST);

	mdb.m_prsym = mdb_gelf_symtab_create_mutable();

	(void) mdb_nv_insert(&mdb.m_modules, mdb.m_pname, NULL,
	    (uintptr_t)&mdb.m_rmod, MDB_NV_RDONLY);

	for (dcp = &mdb_dcmd_builtins[0]; dcp->dc_name != NULL; dcp++)
		(void) mdb_module_add_dcmd(&mdb.m_rmod, dcp, 0);

	for (i = 0; mdb_dis_builtins[i] != NULL; i++)
		(void) mdb_dis_create(mdb_dis_builtins[i]);

	if ((mp = mdb_module_load("mdb_kvm", MDB_MOD_BUILTIN)) != NULL)
		mp->mod_tgt_ctor = mdb_kvm_tgt_create;

	if ((mp = mdb_module_load("mdb_proc", MDB_MOD_BUILTIN)) != NULL)
		mp->mod_tgt_ctor = mdb_proc_tgt_create;

	if ((mp = mdb_module_load("mdb_kproc", MDB_MOD_BUILTIN)) != NULL)
		mp->mod_tgt_ctor = mdb_kproc_tgt_create;

	mdb.m_frame = &frame0;
}

void
mdb_destroy(void)
{
	const mdb_dcmd_t *dcp;
	mdb_var_t *v;

	mdb_module_unload_all();

	if (mdb.m_target != NULL)
		(void) mdb_tgt_destroy(mdb.m_target);

	if (mdb.m_prsym != NULL)
		mdb_gelf_symtab_destroy(mdb.m_prsym);

	mdb_nv_rewind(&mdb.m_disasms);
	while ((v = mdb_nv_advance(&mdb.m_disasms)) != NULL)
		mdb_dis_destroy(mdb_nv_get_cookie(v));

	for (dcp = &mdb_dcmd_builtins[0]; dcp->dc_name != NULL; dcp++)
		(void) mdb_module_remove_dcmd(&mdb.m_rmod, dcp->dc_name);

	mdb_nv_destroy(&mdb.m_nv);
	mdb_nv_destroy(&mdb.m_walkers);
	mdb_nv_destroy(&mdb.m_dcmds);
	mdb_nv_destroy(&mdb.m_modules);
	mdb_nv_destroy(&mdb.m_disasms);

	mdb_free(mdb.m_ipathstr, MAXPATHLEN);
	mdb_free(mdb.m_lpathstr, MAXPATHLEN);

	if (mdb.m_ipath != NULL)
		mdb_path_free(mdb.m_ipath, mdb.m_ipathlen);

	if (mdb.m_lpath != NULL)
		mdb_path_free(mdb.m_lpath, mdb.m_lpathlen);

	if (mdb.m_in != NULL)
		mdb_iob_destroy(mdb.m_in);

	mdb_iob_destroy(mdb.m_out);
	mdb_iob_destroy(mdb.m_err);

	if (mdb.m_log != NULL)
		mdb_io_rele(mdb.m_log);
}

/*
 * The real main loop of the debugger: create a new execution frame on the
 * debugger stack, and while we have input available, call into the parser.
 */
int
mdb_run(void)
{
	volatile int err;
	mdb_frame_t f;

	mdb_intr_disable();
	mdb_frame_push(&f);

	if ((err = setjmp(f.f_pcb)) != 0) {
		int pop = (mdb.m_in != NULL &&
		    (mdb_iob_isapipe(mdb.m_in) || mdb_iob_isastr(mdb.m_in)));

		mdb_dprintf(MDB_DBG_DSTK, "[%u] caught event %s\n",
		    mdb.m_depth, mdb_err2str(err));

		/*
		 * If an interrupt or quit signal is reported, we may have been
		 * in the middle of typing or processing the command line:
		 * print a newline and discard everything in the parser's iob.
		 */
		if (err == MDB_ERR_SIGINT || err == MDB_ERR_QUIT) {
			mdb_iob_nl(mdb.m_out);
			yydiscard();
		}

		/*
		 * If a syntax error or other failure has occurred, pop all
		 * input buffers pushed by commands executed in this frame.
		 */
		while (mdb_iob_stack_size(&f.f_istk) != 0) {
			if (mdb.m_in != NULL)
				mdb_iob_destroy(mdb.m_in);
			mdb.m_in = mdb_iob_stack_pop(&f.f_istk);
			yylineno = mdb_iob_lineno(mdb.m_in);
		}

		/*
		 * Reset standard output and the current frame to a known,
		 * clean state, so we can continue execution.
		 */
		mdb_iob_margin(mdb.m_out, MDB_IOB_DEFMARGIN);
		mdb_iob_clrflags(mdb.m_out, MDB_IOB_INDENT);
		mdb_iob_discard(mdb.m_out);
		mdb_frame_reset(&f);

		/*
		 * If we quit or abort using the output pager, reset the
		 * line count on standard output back to zero.
		 */
		if (err == MDB_ERR_PAGER || err == MDB_ERR_ABORT)
			mdb_iob_clearlines(mdb.m_out);

		/*
		 * If the user requested the debugger quit or abort back to
		 * the top, or if standard input is a pipe or mdb_eval("..."),
		 * then propagate the error up the debugger stack.
		 */
		if (err == MDB_ERR_QUIT || err == MDB_ERR_ABORT || pop != 0 ||
		    (err == MDB_ERR_PAGER && mdb.m_fmark != &f)) {
			mdb_frame_pop(&f, err);
			return (err);
		}

		/*
		 * If we've returned here from a context where signals were
		 * blocked (e.g. a signal handler), we can now unblock them.
		 */
		if (err == MDB_ERR_SIGINT)
			(void) mdb_signal_unblock(SIGINT);
	} else
		mdb_intr_enable();

	for (;;) {
		while (mdb.m_in != NULL && (mdb_iob_getflags(mdb.m_in) &
		    (MDB_IOB_ERR | MDB_IOB_EOF)) == 0) {
			if (mdb.m_depth == 1 &&
			    mdb_iob_stack_size(&f.f_istk) == 0)
				mdb_iob_clearlines(mdb.m_out);
			(void) yyparse();
		}

		if (mdb.m_in != NULL) {
			if (mdb_iob_err(mdb.m_in)) {
				warn("error reading input stream %s\n",
				    mdb_iob_name(mdb.m_in));
			}
			mdb_iob_destroy(mdb.m_in);
			mdb.m_in = NULL;
		}

		if (mdb_iob_stack_size(&f.f_istk) == 0)
			break; /* return when we're out of input */

		mdb.m_in = mdb_iob_stack_pop(&f.f_istk);
		yylineno = mdb_iob_lineno(mdb.m_in);
	}

	mdb_frame_pop(&f, 0);
	return (0);
}

/*
 * The read-side of the pipe executes this service routine.  We simply call
 * mdb_run to create a new frame on the execution stack and run the MDB parser,
 * and then propagate any error code back to the previous frame.
 */
static int
runsvc(void)
{
	int err = mdb_run();

	if (err != 0) {
		mdb_dprintf(MDB_DBG_DSTK, "forwarding error %s from pipeline",
		    mdb_err2str(err));
		longjmp(mdb.m_frame->f_pcb, err);
	}

	return (err);
}

/*
 * Read-side pipe service routine: if we longjmp here, just return to the read
 * routine because now we have more data to consume.  Otherwise:
 * (1) if ctx_data is non-NULL, longjmp to the write-side to produce more data;
 * (2) if wriob is NULL, there is no writer but this is the first read, so we
 *     can just execute mdb_run() to completion on the current stack;
 * (3) if (1) and (2) are false, then there is a writer and this is the first
 *     read, so create a co-routine context to execute mdb_run().
 */
/*ARGSUSED*/
static void
rdsvc(mdb_iob_t *rdiob, mdb_iob_t *wriob, mdb_iob_ctx_t *ctx)
{
	if (setjmp(ctx->ctx_rpcb) == 0) {
		/*
		 * Save the current standard input into the pipe context, and
		 * reset m_in to point to the pipe.  We will restore it on
		 * the way back in wrsvc() below.
		 */
		ctx->ctx_iob = mdb.m_in;
		mdb.m_in = rdiob;

		if (ctx->ctx_data != NULL)
			longjmp(ctx->ctx_wpcb, 1);
		else if (wriob == NULL)
			(void) runsvc();
		else if ((ctx->ctx_data = mdb_context_create(runsvc)) != NULL)
			mdb_context_switch(ctx->ctx_data);
		else
			mdb_warn("failed to create pipe context");
	}
}

/*
 * Write-side pipe service routine: if we longjmp here, just return to the
 * write routine because now we have free space in the pipe buffer for writing;
 * otherwise longjmp to the read-side to consume data and create space for us.
 */
/*ARGSUSED*/
static void
wrsvc(mdb_iob_t *rdiob, mdb_iob_t *wriob, mdb_iob_ctx_t *ctx)
{
	if (setjmp(ctx->ctx_wpcb) == 0) {
		mdb.m_in = ctx->ctx_iob;
		longjmp(ctx->ctx_rpcb, 1);
	}
}

/*
 * Call the current frame's mdb command.  This entry point is used by the
 * MDB parser to actually execute a command once it has successfully parsed
 * a line of input.  The command is waiting for us in the current frame.
 * We loop through each command on the list, executing its dcmd with the
 * appropriate argument.  If the command has a successor, we know it had
 * a | operator after it, and so we need to create a pipe and replace
 * stdout with the pipe's output buffer.
 */
int
mdb_call(uintmax_t addr, uintmax_t count, uint_t flags)
{
	mdb_frame_t *fp = mdb.m_frame;
	mdb_cmd_t *cp, *ncp;
	mdb_iob_t *iobs[2];
	int status, err = 0;
	jmp_buf pcb;

	if (mdb_iob_isapipe(mdb.m_in))
		yyerror("syntax error");

	mdb_intr_disable();
	fp->f_cp = mdb_list_next(&fp->f_cmds);

	if (flags & DCMD_LOOP)
		flags |= DCMD_LOOPFIRST; /* set LOOPFIRST if this is a loop */

	for (cp = mdb_list_next(&fp->f_cmds); cp; cp = mdb_list_next(cp)) {
		if (mdb_list_next(cp) != NULL) {
			mdb_iob_pipe(iobs, rdsvc, wrsvc);

			mdb_iob_stack_push(&fp->f_istk, mdb.m_in, yylineno);
			mdb.m_in = iobs[MDB_IOB_RDIOB];

			mdb_iob_stack_push(&fp->f_ostk, mdb.m_out, 0);
			mdb.m_out = iobs[MDB_IOB_WRIOB];

			ncp = mdb_list_next(cp);
			mdb_vcb_inherit(cp, ncp);

			bcopy(fp->f_pcb, pcb, sizeof (jmp_buf));
			ASSERT(fp->f_pcmd == NULL);
			fp->f_pcmd = ncp;

			if ((err = setjmp(fp->f_pcb)) == 0) {
				status = mdb_call_idcmd(cp->c_dcmd, addr, count,
				    flags | DCMD_PIPE_OUT, &cp->c_argv,
				    &cp->c_addrv, cp->c_vcbs);

				ASSERT(mdb.m_in == iobs[MDB_IOB_RDIOB]);
				ASSERT(mdb.m_out == iobs[MDB_IOB_WRIOB]);
			} else {
				mdb_dprintf(MDB_DBG_DSTK, "caught error %s "
				    "from pipeline", mdb_err2str(err));
			}

			if (err != 0 || DCMD_ABORTED(status)) {
				mdb_iob_setflags(mdb.m_in, MDB_IOB_ERR);
				mdb_iob_setflags(mdb.m_out, MDB_IOB_ERR);
			} else {
				mdb_iob_flush(mdb.m_out);
				(void) mdb_iob_ctl(mdb.m_out, I_FLUSH,
				    (void *)FLUSHW);
			}

			mdb_iob_destroy(mdb.m_out);
			mdb.m_out = mdb_iob_stack_pop(&fp->f_ostk);

			if (mdb.m_in != NULL)
				mdb_iob_destroy(mdb.m_in);

			mdb.m_in = mdb_iob_stack_pop(&fp->f_istk);
			yylineno = mdb_iob_lineno(mdb.m_in);

			fp->f_pcmd = NULL;
			bcopy(pcb, fp->f_pcb, sizeof (jmp_buf));

			if (err == MDB_ERR_ABORT || err == MDB_ERR_QUIT)
				longjmp(fp->f_pcb, err);

			if (err != 0 || DCMD_ABORTED(status) ||
			    mdb_addrvec_length(&ncp->c_addrv) == 0)
				break;

			addr = mdb_nv_get_value(mdb.m_dot);
			count = 1;
			flags = 0;

		} else {
			mdb_intr_enable();
			(void) mdb_call_idcmd(cp->c_dcmd, addr, count, flags,
			    &cp->c_argv, &cp->c_addrv, cp->c_vcbs);
			mdb_intr_disable();
		}

		fp->f_cp = mdb_list_next(cp);
		mdb_cmd_reset(cp);
	}

	/*
	 * If our last-command list is non-empty, destroy it.  Then copy the
	 * current frame's cmd list to the m_lastc list and reset the frame.
	 */
	while ((cp = mdb_list_next(&mdb.m_lastc)) != NULL) {
		mdb_list_delete(&mdb.m_lastc, cp);
		mdb_cmd_destroy(cp);
	}

	mdb_list_move(&fp->f_cmds, &mdb.m_lastc);
	mdb_frame_reset(fp);
	mdb_intr_enable();
	return (err == 0);
}

uintmax_t
mdb_dot_incr(const char *op)
{
	uintmax_t odot, ndot;

	odot = mdb_nv_get_value(mdb.m_dot);
	ndot = odot + mdb.m_incr;

	if ((odot ^ ndot) & 0x8000000000000000ull)
		yyerror("'%s' would cause '.' to overflow\n", op);

	return (ndot);
}

uintmax_t
mdb_dot_decr(const char *op)
{
	uintmax_t odot, ndot;

	odot = mdb_nv_get_value(mdb.m_dot);
	ndot = odot - mdb.m_incr;

	if (ndot > odot)
		yyerror("'%s' would cause '.' to underflow\n", op);

	return (ndot);
}

mdb_iwalker_t *
mdb_walker_lookup(const char *s)
{
	const char *p = strchr(s, '`');
	mdb_var_t *v;

	if (p != NULL) {
		size_t nbytes = MIN((size_t)(p - s), MDB_NV_NAMELEN - 1);
		char mname[MDB_NV_NAMELEN];
		mdb_module_t *mod;

		(void) strncpy(mname, s, nbytes);
		mname[nbytes] = '\0';

		if ((v = mdb_nv_lookup(&mdb.m_modules, mname)) == NULL) {
			(void) set_errno(EMDB_NOMOD);
			return (NULL);
		}

		mod = mdb_nv_get_cookie(v);

		if ((v = mdb_nv_lookup(&mod->mod_walkers, ++p)) != NULL)
			return (mdb_nv_get_cookie(v));

	} else if ((v = mdb_nv_lookup(&mdb.m_walkers, s)) != NULL)
		return (mdb_nv_get_cookie(mdb_nv_get_cookie(v)));

	(void) set_errno(EMDB_NOWALK);
	return (NULL);
}

mdb_idcmd_t *
mdb_dcmd_lookup(const char *s)
{
	const char *p = strchr(s, '`');
	mdb_var_t *v;

	if (p != NULL) {
		size_t nbytes = MIN((size_t)(p - s), MDB_NV_NAMELEN - 1);
		char mname[MDB_NV_NAMELEN];
		mdb_module_t *mod;

		(void) strncpy(mname, s, nbytes);
		mname[nbytes] = '\0';

		if ((v = mdb_nv_lookup(&mdb.m_modules, mname)) == NULL) {
			(void) set_errno(EMDB_NOMOD);
			return (NULL);
		}

		mod = mdb_nv_get_cookie(v);

		if ((v = mdb_nv_lookup(&mod->mod_dcmds, ++p)) != NULL)
			return (mdb_nv_get_cookie(v));

	} else if ((v = mdb_nv_lookup(&mdb.m_dcmds, s)) != NULL)
		return (mdb_nv_get_cookie(mdb_nv_get_cookie(v)));

	(void) set_errno(EMDB_NODCMD);
	return (NULL);
}

void
mdb_dcmd_usage(const mdb_idcmd_t *idcp, mdb_iob_t *iob)
{
	const char *prefix = "", *usage = "";
	char name0 = idcp->idc_name[0];

	if (idcp->idc_usage != NULL) {
		if (idcp->idc_usage[0] == ':') {
			if (name0 != ':' && name0 != '$')
				prefix = "address::";
			else
				prefix = "address";
			usage = &idcp->idc_usage[1];

		} else if (idcp->idc_usage[0] == '?') {
			if (name0 != ':' && name0 != '$')
				prefix = "[address]::";
			else
				prefix = "[address]";
			usage = &idcp->idc_usage[1];

		} else
			usage = idcp->idc_usage;
	}

	mdb_iob_printf(iob, "Usage: %s%s %s\n", prefix, idcp->idc_name, usage);

	if (idcp->idc_help != NULL) {
		mdb_iob_printf(iob, "%s: try '::help %s' for more "
		    "information\n", mdb.m_pname, idcp->idc_name);
	}
}

static mdb_idcmd_t *
dcmd_ndef(const mdb_idcmd_t *idcp)
{
	mdb_var_t *v = mdb_nv_get_ndef(idcp->idc_var);

	if (v != NULL)
		return (mdb_nv_get_cookie(mdb_nv_get_cookie(v)));

	return (NULL);
}

static int
dcmd_invoke(mdb_idcmd_t *idcp, uintptr_t addr, uint_t flags,
    int argc, const mdb_arg_t *argv, const mdb_vcb_t *vcbs)
{
	int status;

	mdb_dprintf(MDB_DBG_DCMD, "dcmd %s`%s dot = %lr incr = %llr\n",
	    idcp->idc_modp->mod_name, idcp->idc_name, addr, mdb.m_incr);

	if ((status = idcp->idc_funcp(addr, flags, argc, argv)) == DCMD_USAGE) {
		mdb_dcmd_usage(idcp, mdb.m_err);
		goto done;
	}

	while (status == DCMD_NEXT && (idcp = dcmd_ndef(idcp)) != NULL)
		status = idcp->idc_funcp(addr, flags, argc, argv);

	if (status == DCMD_USAGE)
		mdb_dcmd_usage(idcp, mdb.m_err);

	if (status == DCMD_NEXT)
		status = DCMD_OK;
done:
	/*
	 * If standard output is a pipe and there are vcbs active, we need to
	 * flush standard out and the write-side of the pipe.  The reasons for
	 * this are explained in more detail in mdb_vcb.c.
	 */
	if ((flags & DCMD_PIPE_OUT) && (vcbs != NULL)) {
		mdb_iob_flush(mdb.m_out);
		(void) mdb_iob_ctl(mdb.m_out, I_FLUSH, (void *)FLUSHW);
	}

	return (status);
}

/*
 * Call an internal dcmd directly: this code is used by module API functions
 * that need to execute dcmds, and by mdb_call() above.
 */
int
mdb_call_idcmd(mdb_idcmd_t *idcp, uintmax_t addr, uintmax_t count,
    uint_t flags, mdb_argvec_t *avp, mdb_addrvec_t *adp, mdb_vcb_t *vcbs)
{
	int is_exec = (strcmp(idcp->idc_name, "$<") == 0);
	mdb_arg_t *argv;
	int argc;
	uintmax_t i;
	int status;

	/*
	 * Update the values of dot and the most recent address and count
	 * to the values of our input parameters.
	 */
	mdb_nv_set_value(mdb.m_dot, addr);
	mdb.m_raddr = addr;
	mdb.m_dcount = count;

	/*
	 * Here the adb(1) man page lies: '9' is only set to count
	 * when the command is $<, not when it's $<<.
	 */
	if (is_exec)
		mdb_nv_set_value(mdb.m_rcount, count);

	/*
	 * We can now return if the repeat count is zero.
	 */
	if (count == 0)
		return (DCMD_OK);

	/*
	 * To guard against bad dcmds, we avoid passing the actual argv that
	 * we will use to free argument strings directly to the dcmd.  Instead,
	 * we pass a copy that will be garbage collected automatically.
	 */
	argc = avp->a_nelems;
	argv = mdb_alloc(sizeof (mdb_arg_t) * argc, UM_SLEEP | UM_GC);
	bcopy(avp->a_data, argv, sizeof (mdb_arg_t) * argc);

	if (mdb_addrvec_length(adp) != 0) {
		flags |= DCMD_PIPE | DCMD_LOOP | DCMD_LOOPFIRST | DCMD_ADDRSPEC;
		addr = mdb_addrvec_shift(adp);
		mdb_nv_set_value(mdb.m_dot, addr);
		mdb_vcb_propagate(vcbs);
		count = 1;
	}

	status = dcmd_invoke(idcp, addr, flags, argc, argv, vcbs);
	if (DCMD_ABORTED(status))
		goto done;

	/*
	 * If the command is $< and we're not receiving input from a pipe, we
	 * ignore the repeat count and just return since the macro file is now
	 * pushed on to the input stack.
	 */
	if (is_exec && mdb_addrvec_length(adp) == 0)
		goto done;

	/*
	 * If we're going to loop, we've already executed the dcmd once,
	 * so clear the LOOPFIRST flag before proceeding.
	 */
	if (flags & DCMD_LOOP)
		flags &= ~DCMD_LOOPFIRST;

	for (i = 1; i < count; i++) {
		addr = mdb_dot_incr(",");
		mdb_nv_set_value(mdb.m_dot, addr);
		status = dcmd_invoke(idcp, addr, flags, argc, argv, vcbs);
		if (DCMD_ABORTED(status))
			goto done;
	}

	while (mdb_addrvec_length(adp) != 0) {
		addr = mdb_addrvec_shift(adp);
		mdb_nv_set_value(mdb.m_dot, addr);
		mdb_vcb_propagate(vcbs);
		status = dcmd_invoke(idcp, addr, flags, argc, argv, vcbs);
		if (DCMD_ABORTED(status))
			goto done;
	}
done:
	mdb_iob_nlflush(mdb.m_out);
	return (status);
}

void
mdb_intr_enable(void)
{
	ASSERT(mdb.m_intr >= 1);
	if (mdb.m_intr == 1 && mdb.m_pend != 0) {
		(void) mdb_signal_block(SIGINT);
		mdb.m_intr = mdb.m_pend = 0;
		mdb_dprintf(MDB_DBG_DSTK, "delivering pending INT\n");
		longjmp(mdb.m_frame->f_pcb, MDB_ERR_SIGINT);
	} else
		mdb.m_intr--;
}

void
mdb_intr_disable(void)
{
	mdb.m_intr++;
	ASSERT(mdb.m_intr >= 1);
}
