/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_cmds.c	1.1	99/08/11 SMI"

#include <sys/elf.h>
#include <sys/elf_SPARC.h>

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <mdb/mdb_string.h>
#include <mdb/mdb_argvec.h>
#include <mdb/mdb_nv.h>
#include <mdb/mdb_fmt.h>
#include <mdb/mdb_target.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb_conf.h>
#include <mdb/mdb_module.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_stdlib.h>
#include <mdb/mdb_lex.h>
#include <mdb/mdb_io_impl.h>
#include <mdb/mdb_help.h>
#include <mdb/mdb_disasm.h>
#include <mdb/mdb_frame.h>
#include <mdb/mdb_set.h>
#include <mdb/mdb.h>

static mdb_tgt_addr_t
write_uint16(mdb_tgt_as_t as, mdb_tgt_addr_t addr, uint64_t ull)
{
	uint16_t o, n = (uint16_t)ull;

	if (mdb_tgt_aread(mdb.m_target, as, &o, sizeof (o), addr) == -1)
		return (addr);

	if (mdb_tgt_awrite(mdb.m_target, as, &n, sizeof (n), addr) == -1)
		return (addr);

	if (mdb_tgt_aread(mdb.m_target, as, &n, sizeof (n), addr) == -1)
		return (addr);

	mdb_iob_printf(mdb.m_out, "%-#*lla%16T%-#8hx=%8T0x%hx\n",
	    mdb_iob_getmargin(mdb.m_out), addr, o, n);

	return (addr + sizeof (n));
}

static mdb_tgt_addr_t
write_uint32(mdb_tgt_as_t as, mdb_tgt_addr_t addr, uint64_t ull)
{
	uint32_t o, n = (uint32_t)ull;

	if (mdb_tgt_aread(mdb.m_target, as, &o, sizeof (o), addr) == -1)
		return (addr);

	if (mdb_tgt_awrite(mdb.m_target, as, &n, sizeof (n), addr) == -1)
		return (addr);

	if (mdb_tgt_aread(mdb.m_target, as, &n, sizeof (n), addr) == -1)
		return (addr);

	mdb_iob_printf(mdb.m_out, "%-#*lla%16T%-#16x=%8T0x%x\n",
	    mdb_iob_getmargin(mdb.m_out), addr, o, n);

	return (addr + sizeof (n));
}

static mdb_tgt_addr_t
write_uint64(mdb_tgt_as_t as, mdb_tgt_addr_t addr, uint64_t n)
{
	uint64_t o;

	if (mdb_tgt_aread(mdb.m_target, as, &o, sizeof (o), addr) == -1)
		return (addr);

	if (mdb_tgt_awrite(mdb.m_target, as, &n, sizeof (n), addr) == -1)
		return (addr);

	if (mdb_tgt_aread(mdb.m_target, as, &n, sizeof (n), addr) == -1)
		return (addr);

	mdb_iob_printf(mdb.m_out, "%-#*lla%16T%-#24llx=%8T0x%llx\n",
	    mdb_iob_getmargin(mdb.m_out), addr, o, n);

	return (addr + sizeof (n));
}

static int
write_arglist(mdb_tgt_as_t as, mdb_tgt_addr_t addr,
    int argc, const mdb_arg_t *argv)
{
	mdb_tgt_addr_t (*write_value)(mdb_tgt_as_t, mdb_tgt_addr_t, uint64_t);
	mdb_tgt_addr_t naddr;
	uintmax_t value;
	size_t i;

	if (argc == 1) {
		warn("expected address following %c\n", argv->a_un.a_char);
		return (DCMD_ERR);
	}

	switch (argv->a_un.a_char) {
	case 'v':
	case 'w':
		write_value = write_uint16;
		break;
	case 'W':
		write_value = write_uint32;
		break;
	case 'Z':
		write_value = write_uint64;
		break;
	}

	for (argv++, i = 1; i < argc; i++, argv++) {
		if (argv->a_type == MDB_TYPE_CHAR) {
			warn("expected immediate value instead of '%c'\n",
			    argv->a_un.a_char);
			return (DCMD_ERR);
		}

		if (argv->a_type == MDB_TYPE_STRING) {
			if (mdb_eval(argv->a_un.a_str) == -1) {
				mdb_warn("failed to write \"%s\"",
				    argv->a_un.a_str);
				return (DCMD_ERR);
			}
			value = mdb_nv_get_value(mdb.m_dot);
		} else
			value = argv->a_un.a_val;

		if ((naddr = write_value(as, addr, value)) == addr) {
			warn("failed to write %llr at address 0x%llx",
			    value, addr);
			break;
		}

		addr = naddr;
	}

	mdb_nv_set_value(mdb.m_dot, addr);
	return (DCMD_OK);
}

static mdb_tgt_addr_t
match_uint16(mdb_tgt_as_t as, mdb_tgt_addr_t addr, uint64_t v64, uint64_t m64)
{
	uint16_t x, val = (uint16_t)v64, mask = (uint16_t)m64;

	for (; mdb_tgt_aread(mdb.m_target, as, &x,
	    sizeof (x), addr) == sizeof (x); addr += sizeof (x)) {

		if ((x & mask) == val) {
			mdb_iob_printf(mdb.m_out, "%lla\n", addr);
			break;
		}
	}
	return (addr);
}

static mdb_tgt_addr_t
match_uint32(mdb_tgt_as_t as, mdb_tgt_addr_t addr, uint64_t v64, uint64_t m64)
{
	uint32_t x, val = (uint32_t)v64, mask = (uint32_t)m64;

	for (; mdb_tgt_aread(mdb.m_target, as, &x,
	    sizeof (x), addr) == sizeof (x); addr += sizeof (x)) {

		if ((x & mask) == val) {
			mdb_iob_printf(mdb.m_out, "%lla\n", addr);
			break;
		}
	}
	return (addr);
}

static mdb_tgt_addr_t
match_uint64(mdb_tgt_as_t as, mdb_tgt_addr_t addr, uint64_t val, uint64_t mask)
{
	uint64_t x;

	for (; mdb_tgt_aread(mdb.m_target, as, &x,
	    sizeof (x), addr) == sizeof (x); addr += sizeof (x)) {

		if ((x & mask) == val) {
			mdb_iob_printf(mdb.m_out, "%lla\n", addr);
			break;
		}
	}
	return (addr);
}

static int
match_arglist(mdb_tgt_as_t as, uint_t flags, mdb_tgt_addr_t addr,
    int argc, const mdb_arg_t *argv)
{
	mdb_tgt_addr_t (*match_value)(mdb_tgt_as_t, mdb_tgt_addr_t,
	    uint64_t, uint64_t);

	uint64_t mask;
	size_t i;

	if (argc < 2) {
		warn("expected value following %c\n", argv->a_un.a_char);
		return (DCMD_ERR);
	}

	if (argc > 3) {
		warn("only value and mask may follow %c\n", argv->a_un.a_char);
		return (DCMD_ERR);
	}

	for (i = 1; i < argc; i++) {
		if (argv[i].a_type == MDB_TYPE_STRING) {
			warn("expected immediate value instead of \"%s\"\n",
			    argv[i].a_un.a_str);
			return (DCMD_ERR);
		}
	}

	switch (argv->a_un.a_char) {
	case 'l':
		match_value = match_uint16;
		break;
	case 'L':
		match_value = match_uint32;
		break;
	case 'M':
		match_value = match_uint64;
		break;
	}

	mask = argc == 3 ? argv[2].a_un.a_val : -1ULL;
	addr = match_value(as, addr, argv[1].a_un.a_val, mask);
	mdb_nv_set_value(mdb.m_dot, addr);

	/*
	 * In adb(1), the match operators ignore any repeat count that has
	 * been applied to them.  We emulate this undocumented property
	 * by returning DCMD_ABORT if our input is not a pipeline.
	 */
	return ((flags & DCMD_PIPE) ? DCMD_OK : DCMD_ABORT);
}

static int
argncmp(int argc, const mdb_arg_t *argv, const char *s)
{
	for (; *s != '\0'; s++, argc--, argv++) {
		if (argc == 0 || argv->a_type != MDB_TYPE_CHAR)
			return (FALSE);
		if (argv->a_un.a_char != *s)
			return (FALSE);
	}
	return (TRUE);
}

static int
print_arglist(mdb_tgt_as_t as, mdb_tgt_addr_t addr, uint_t flags,
    int argc, const mdb_arg_t *argv)
{
	mdb_tgt_addr_t naddr = addr;
	char buf[MDB_TGT_SYM_NAMLEN];
	GElf_Sym sym;
	size_t i, n;

	if (DCMD_HDRSPEC(flags)) {
		const char *fmt;
		int is_dis;
		/*
		 * This is nasty, but necessary for precise adb compatibility.
		 * Detect disassembly format by looking for "ai" or "ia":
		 */
		if (argncmp(argc, argv, "ai")) {
			fmt = "%-#*lla\n";
			is_dis = TRUE;
		} else if (argncmp(argc, argv, "ia")) {
			fmt = "%-#*lla";
			is_dis = TRUE;
		} else if (flags & DCMD_PIPE_OUT) {
			fmt = "%*T%16T";
			is_dis = FALSE;
		} else {
			fmt = "%-#*lla%16T";
			is_dis = FALSE;
		}

		/*
		 * If symbolic decoding is on, disassembly is off, and the
		 * address exactly matches a symbol, print the symbol name:
		 */
		if ((mdb.m_flags & MDB_FL_PSYM) && !is_dis &&
		    (flags & DCMD_PIPE_OUT) == 0 &&
		    (as == MDB_TGT_AS_VIRT || as == MDB_TGT_AS_FILE) &&
		    mdb_tgt_lookup_by_addr(mdb.m_target, (uintptr_t)addr,
		    MDB_TGT_SYM_EXACT, buf, sizeof (buf), &sym) == 0)
			mdb_iob_printf(mdb.m_out, "%s:\n", buf);

		mdb_iob_printf(mdb.m_out, fmt,
		    (uint_t)mdb_iob_getmargin(mdb.m_out), addr);
	}

	if (argc == 0) {
		/*
		 * Yes, for you trivia buffs: if you use a format verb and give
		 * no format string, you get: X^"= "i ... note that in adb the
		 * the '=' verb once had 'z' as its default, but then 'z' was
		 * deleted (it was once an alias for 'i') and so =\n now calls
		 * scanform("z") and produces a 'bad modifier' message.
		 */
		static const mdb_arg_t def_argv[] = {
			{ MDB_TYPE_CHAR, MDB_INIT_CHAR('X') },
			{ MDB_TYPE_CHAR, MDB_INIT_CHAR('^') },
			{ MDB_TYPE_STRING, MDB_INIT_STRING("= ") },
			{ MDB_TYPE_CHAR, MDB_INIT_CHAR('i') }
		};

		argc = sizeof (def_argv) / sizeof (mdb_arg_t);
		argv = def_argv;
	}

	mdb_iob_setflags(mdb.m_out, MDB_IOB_INDENT);

	for (i = 0, n = 1; i < argc; i++, argv++) {
		switch (argv->a_type) {
		case MDB_TYPE_CHAR:
			naddr = mdb_fmt_print(mdb.m_target, as, naddr, n,
			    argv->a_un.a_char);
			n = 1;
			break;

		case MDB_TYPE_IMMEDIATE:
			n = argv->a_un.a_val;
			break;

		case MDB_TYPE_STRING:
			mdb_iob_puts(mdb.m_out, argv->a_un.a_str);
			n = 1;
			break;
		}
	}

	mdb.m_incr = naddr - addr;
	mdb_iob_clrflags(mdb.m_out, MDB_IOB_INDENT);

	return (DCMD_OK);
}

static int
print_common(mdb_tgt_as_t as, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_tgt_addr_t addr = mdb_nv_get_value(mdb.m_dot);

	if (argc != 0 && argv->a_type == MDB_TYPE_CHAR) {
		if (strchr("vwWZ", argv->a_un.a_char))
			return (write_arglist(as, addr, argc, argv));
		if (strchr("lLM", argv->a_un.a_char))
			return (match_arglist(as, flags, addr, argc, argv));
	}

	return (print_arglist(as, addr, flags, argc, argv));
}

/*ARGSUSED*/
static int
cmd_print_core(uintptr_t x, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (print_common(MDB_TGT_AS_VIRT, flags, argc, argv));
}

/*ARGSUSED*/
static int
cmd_print_object(uintptr_t x, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (print_common(MDB_TGT_AS_FILE, flags, argc, argv));
}

/*ARGSUSED*/
static int
cmd_print_phys(uintptr_t x, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (print_common(MDB_TGT_AS_PHYS, flags, argc, argv));
}

/*ARGSUSED*/
static int
cmd_print_value(uintptr_t addr, uint_t flags,
	int argc, const mdb_arg_t *argv)
{
	uintmax_t ndot, dot = mdb_nv_get_value(mdb.m_dot);
	const char *tgt_argv[1];
	mdb_tgt_t *t;
	size_t i, n;

	if (argc == 0) {
		warn("expected one or more format characters following '='\n");
		return (DCMD_ERR);
	}

	tgt_argv[0] = (const char *)&dot;
	t = mdb_tgt_create(mdb_value_tgt_create, 0, 1, tgt_argv);
	mdb_iob_setflags(mdb.m_out, MDB_IOB_INDENT);

	for (i = 0, n = 1; i < argc; i++, argv++) {
		switch (argv->a_type) {
		case MDB_TYPE_CHAR:
			ndot = mdb_fmt_print(t, MDB_TGT_AS_VIRT,
			    dot, n, argv->a_un.a_char);
			if (argv->a_un.a_char == '+' ||
			    argv->a_un.a_char == '-')
				dot = ndot;
			n = 1;
			break;

		case MDB_TYPE_IMMEDIATE:
			n = argv->a_un.a_val;
			break;

		case MDB_TYPE_STRING:
			mdb_iob_puts(mdb.m_out, argv->a_un.a_str);
			n = 1;
			break;
		}
	}

	mdb_iob_clrflags(mdb.m_out, MDB_IOB_INDENT);
	mdb_nv_set_value(mdb.m_dot, dot);
	mdb.m_incr = 0;

	mdb_tgt_destroy(t);
	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_assign_variable(uintptr_t addr, uint_t flags,
    int argc, const mdb_arg_t *argv)
{
	uintmax_t dot = mdb_nv_get_value(mdb.m_dot);
	const char *p;
	mdb_var_t *v;

	if (argc == 2) {
		if (argv->a_type != MDB_TYPE_CHAR) {
			warn("improper arguments following '>' operator\n");
			return (DCMD_ERR);
		}

		switch (argv->a_un.a_char) {
		case 'c':
			addr = *((uchar_t *)&addr);
			break;
		case 's':
			addr = *((ushort_t *)&addr);
			break;
		case 'i':
			addr = *((uint_t *)&addr);
			break;
		case 'l':
			addr = *((ulong_t *)&addr);
			break;
		default:
			mdb_warn("%c is not a valid // modifier\n",
			    argv->a_un.a_char);
			return (DCMD_ERR);
		}

		dot = addr;
		argv++;
		argc--;
	}

	if (argc != 1 || argv->a_type != MDB_TYPE_STRING) {
		mdb_warn("expected single variable name following '>'\n");
		return (DCMD_ERR);
	}

	if (strlen(argv->a_un.a_str) >= (size_t)MDB_NV_NAMELEN) {
		mdb_warn("variable names may not exceed %d characters\n",
		    MDB_NV_NAMELEN - 1);
		return (DCMD_ERR);
	}

	if ((p = strbadid(argv->a_un.a_str)) != NULL) {
		mdb_warn("'%c' may not be used in a variable name\n", *p);
		return (DCMD_ERR);
	}

	if ((v = mdb_nv_lookup(&mdb.m_nv, argv->a_un.a_str)) == NULL)
		(void) mdb_nv_insert(&mdb.m_nv, argv->a_un.a_str, NULL, dot, 0);
	else
		mdb_nv_set_value(v, dot);

	mdb.m_incr = 0;
	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_src_file(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_io_t *fio;

	if (argc != 1 || argv->a_type != MDB_TYPE_STRING)
		return (DCMD_USAGE);

	if (flags & DCMD_PIPE_OUT) {
		mdb_warn("macro files cannot be used as input to a pipeline\n");
		return (DCMD_ABORT);
	}

	if ((fio = mdb_fdio_create_path(mdb.m_ipath,
	    argv->a_un.a_str, O_RDONLY, 0)) != NULL) {
		mdb_frame_t *fp = mdb.m_frame;
		int err;

		mdb_iob_stack_push(&fp->f_istk, mdb.m_in, yylineno);
		mdb.m_in = mdb_iob_create(fio, MDB_IOB_RDONLY);
		err = mdb_run();

		ASSERT(fp == mdb.m_frame);
		mdb.m_in = mdb_iob_stack_pop(&fp->f_istk);
		yylineno = mdb_iob_lineno(mdb.m_in);

		if (err == MDB_ERR_PAGER && mdb.m_fmark != fp)
			longjmp(fp->f_pcb, err);

		if (err == MDB_ERR_QUIT || err == MDB_ERR_ABORT ||
		    err == MDB_ERR_SIGINT)
			longjmp(fp->f_pcb, err);

		return (DCMD_OK);
	}

	mdb_warn("failed to open %s", argv->a_un.a_str);
	return (DCMD_ABORT);
}

static int
cmd_exec_file(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_io_t *fio;

	/*
	 * The syntax [expr[,count]]$< with no trailing macro file name is
	 * magic in that if count is zero, this command won't be called and
	 * the expression is thus a no-op.  If count is non-zero, we get
	 * invoked with argc == 0, and this means abort the current macro.
	 * If our debugger stack depth is greater than one, we may be using
	 * $< from within a previous $<<, so in that case we set m_in to
	 * NULL to force this entire frame to be popped.
	 */
	if (argc == 0) {
		if (mdb_iob_stack_size(&mdb.m_frame->f_istk) != 0) {
			mdb_iob_destroy(mdb.m_in);
			mdb.m_in = mdb_iob_stack_pop(&mdb.m_frame->f_istk);
		} else if (mdb.m_depth > 1) {
			mdb_iob_destroy(mdb.m_in);
			mdb.m_in = NULL;
		} else
			mdb_warn("input stack is empty\n");
		return (DCMD_OK);
	}

	if ((flags & (DCMD_PIPE | DCMD_PIPE_OUT)) || mdb.m_depth == 1)
		return (cmd_src_file(addr, flags, argc, argv));

	if (argc != 1 || argv->a_type != MDB_TYPE_STRING)
		return (DCMD_USAGE);

	if ((fio = mdb_fdio_create_path(mdb.m_ipath,
	    argv->a_un.a_str, O_RDONLY, 0)) != NULL) {
		mdb_iob_destroy(mdb.m_in);
		mdb.m_in = mdb_iob_create(fio, MDB_IOB_RDONLY);
		return (DCMD_OK);
	}

	mdb_warn("failed to open %s", argv->a_un.a_str);
	return (DCMD_ABORT);
}

/*ARGSUSED*/
static int
cmd_cat(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	int status = DCMD_OK;
	char buf[BUFSIZ];
	mdb_iob_t *iob;
	mdb_io_t *fio;

	for (; argc-- != 0; argv++) {
		if (argv->a_type != MDB_TYPE_STRING) {
			mdb_warn("expected string argument\n");
			status = DCMD_ERR;
			continue;
		}

		if ((fio = mdb_fdio_create_path(NULL,
		    argv->a_un.a_str, O_RDONLY, 0)) == NULL) {
			mdb_warn("failed to open %s", argv->a_un.a_str);
			status = DCMD_ERR;
			continue;
		}

		iob = mdb_iob_create(fio, MDB_IOB_RDONLY);

		while (!(mdb_iob_getflags(iob) & (MDB_IOB_EOF | MDB_IOB_ERR))) {
			ssize_t len = mdb_iob_read(iob, buf, sizeof (buf));
			if (len > 0)
				(void) mdb_iob_write(mdb.m_out, buf, len);
		}

		if (mdb_iob_err(iob))
			mdb_warn("error while reading %s", mdb_iob_name(iob));

		mdb_iob_destroy(iob);
	}

	return (status);
}

/*ARGSUSED*/
static int
cmd_grep(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc != 1 || argv->a_type != MDB_TYPE_STRING)
		return (DCMD_USAGE);

	if (mdb_eval(argv->a_un.a_str) == -1)
		return (DCMD_ERR);

	if (mdb_get_dot() != 0)
		mdb_printf("%lr\n", addr);

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_map(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc != 1 || argv->a_type != MDB_TYPE_STRING)
		return (DCMD_USAGE);

	if (mdb_eval(argv->a_un.a_str) == -1)
		return (DCMD_ERR);

	mdb_printf("%llr\n", mdb_get_dot());
	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_notsup(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_warn("command is not supported by current target\n");
	return (DCMD_ERR);
}

/*ARGSUSED*/
static int
cmd_quit(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	longjmp(mdb.m_frame->f_pcb, MDB_ERR_QUIT);
	/*NOTREACHED*/
	return (DCMD_ERR);
}

/*ARGSUSED*/
static int
cmd_vars(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uint_t opt_nz = FALSE, opt_tag = FALSE, opt_prt = FALSE;
	mdb_var_t *v;

	if (mdb_getopts(argc, argv,
	    'n', MDB_OPT_SETBITS, TRUE, &opt_nz,
	    'p', MDB_OPT_SETBITS, TRUE, &opt_prt,
	    't', MDB_OPT_SETBITS, TRUE, &opt_tag, NULL) != argc)
		return (DCMD_USAGE);

	mdb_nv_rewind(&mdb.m_nv);

	while ((v = mdb_nv_advance(&mdb.m_nv)) != NULL) {
		if ((opt_tag == FALSE || (v->v_flags & MDB_NV_TAGGED)) &&
		    (opt_nz == FALSE || mdb_nv_get_value(v) != 0)) {
			if (opt_prt) {
				mdb_printf("%#llr>%s\n",
				    mdb_nv_get_value(v), mdb_nv_get_name(v));
			} else {
				mdb_printf("%s = %llr\n",
				    mdb_nv_get_name(v), mdb_nv_get_value(v));
			}
		}
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_nzvars(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uintmax_t value;
	mdb_var_t *v;

	if (argc != 0)
		return (DCMD_USAGE);

	mdb_nv_rewind(&mdb.m_nv);

	while ((v = mdb_nv_advance(&mdb.m_nv)) != NULL) {
		if ((value = mdb_nv_get_value(v)) != 0)
			mdb_printf("%s = %llr\n", mdb_nv_get_name(v), value);
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_radix(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc != 0)
		return (DCMD_USAGE);

	if (flags & DCMD_ADDRSPEC) {
		if (addr < 2 || addr > 16) {
			warn("expected radix from 2 to 16\n");
			return (DCMD_ERR);
		}
		mdb.m_radix = (int)addr;
	}

	mdb_iob_printf(mdb.m_out, "radix = %d base ten\n", mdb.m_radix);
	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_symdist(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc != 0)
		return (DCMD_USAGE);

	if (flags & DCMD_ADDRSPEC)
		mdb.m_symdist = addr;

	mdb_printf("symbol matching distance = %lr (%s)\n",
	    mdb.m_symdist, mdb.m_symdist ? "absolute mode" : "smart mode");

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_pgwidth(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc != 0)
		return (DCMD_USAGE);

	if (flags & DCMD_ADDRSPEC)
		mdb_iob_resize(mdb.m_out, mdb.m_out->iob_rows, addr);

	mdb_printf("output page width = %lu\n", mdb.m_out->iob_cols);
	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_reopen(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc != 0)
		return (DCMD_USAGE);

	if (mdb_tgt_setflags(mdb.m_target, MDB_TGT_F_RDWR) == -1) {
		warn("failed to re-open target for writing");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
print_xdata(void *ignored, const char *name, const char *desc, size_t nbytes)
{
	mdb_printf("%-24s - %s (%lu bytes)\n", name, desc, (ulong_t)nbytes);
	return (0);
}

/*ARGSUSED*/
static int
cmd_xdata(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc != 0 || (flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	(void) mdb_tgt_xdata_iter(mdb.m_target, print_xdata, NULL);
	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_unset(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_var_t *v;
	size_t i;

	for (i = 0; i < argc; i++) {
		if (argv[i].a_type != MDB_TYPE_STRING) {
			warn("bad option: arg %lu is not a string\n",
			    (ulong_t)i + 1);
			return (DCMD_USAGE);
		}
	}

	for (i = 0; i < argc; i++, argv++) {
		if ((v = mdb_nv_lookup(&mdb.m_nv, argv->a_un.a_str)) == NULL)
			warn("variable '%s' not defined\n", argv->a_un.a_str);
		else
			mdb_nv_remove(&mdb.m_nv, v);
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_log(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uint_t opt_e = FALSE, opt_d = FALSE;
	const char *filename = NULL;
	int i;

	i = mdb_getopts(argc, argv,
	    'd', MDB_OPT_SETBITS, TRUE, &opt_d,
	    'e', MDB_OPT_SETBITS, TRUE, &opt_e, NULL);

	if ((i != argc && i != argc - 1) || (opt_d && opt_e) ||
	    (i != argc && argv[i].a_type != MDB_TYPE_STRING) ||
	    (i != argc && opt_d == TRUE) || (flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb.m_depth != 1) {
		mdb_warn("log may not be manipulated in this context\n");
		return (DCMD_ABORT);
	}

	if (i != argc)
		filename = argv[i].a_un.a_str;

	/*
	 * If no arguments were specified, print the log file name (if any)
	 * and report whether the log is enabled or disabled.
	 */
	if (argc == 0) {
		if (mdb.m_log) {
			mdb_printf("%s: logging to \"%s\" is currently %s\n",
			    mdb.m_pname, IOP_NAME(mdb.m_log),
			    mdb.m_flags & MDB_FL_LOG ?  "enabled" : "disabled");
		} else
			mdb_printf("%s: no log is active\n", mdb.m_pname);
		return (DCMD_OK);
	}

	/*
	 * If the -d option was specified, pop the log i/o object off the
	 * i/o stack of stdin, stdout, and stderr.
	 */
	if (opt_d) {
		if (mdb.m_flags & MDB_FL_LOG) {
			(void) mdb_iob_pop_io(mdb.m_in);
			(void) mdb_iob_pop_io(mdb.m_out);
			(void) mdb_iob_pop_io(mdb.m_err);
			mdb.m_flags &= ~MDB_FL_LOG;
		} else
			mdb_warn("logging is already disabled\n");
		return (DCMD_OK);
	}

	/*
	 * The -e option is the default: (re-)enable logging by pushing
	 * the log i/o object on to stdin, stdout, and stderr.  If we have
	 * a previous log file, we need to pop it and close it.  If we have
	 * no new log file, push the previous one back on.
	 */
	if (filename != NULL) {
		if (mdb.m_log != NULL) {
			if (mdb.m_flags & MDB_FL_LOG) {
				(void) mdb_iob_pop_io(mdb.m_in);
				(void) mdb_iob_pop_io(mdb.m_out);
				(void) mdb_iob_pop_io(mdb.m_err);
				mdb.m_flags &= ~MDB_FL_LOG;
			}
			mdb_io_rele(mdb.m_log);
		}

		mdb.m_log = mdb_fdio_create_path(NULL, filename,
		    O_CREAT | O_APPEND | O_WRONLY, 0666);

		if (mdb.m_log == NULL) {
			mdb_warn("failed to open %s", filename);
			return (DCMD_ERR);
		}
	}

	if (mdb.m_log != NULL) {
		mdb_iob_push_io(mdb.m_in, mdb_logio_create(mdb.m_log));
		mdb_iob_push_io(mdb.m_out, mdb_logio_create(mdb.m_log));
		mdb_iob_push_io(mdb.m_err, mdb_logio_create(mdb.m_log));

		mdb_printf("%s: logging to \"%s\"\n", mdb.m_pname, filename);
		mdb.m_log = mdb_io_hold(mdb.m_log);
		mdb.m_flags |= MDB_FL_LOG;

		return (DCMD_OK);
	}

	mdb_warn("no log file has been selected\n");
	return (DCMD_ERR);
}

static int
cmd_old_log(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc == 0) {
		mdb_arg_t arg = { MDB_TYPE_STRING, MDB_INIT_STRING("-d") };
		return (cmd_log(addr, flags, 1, &arg));
	}

	return (cmd_log(addr, flags, argc, argv));
}

/*ARGSUSED*/
static int
cmd_load(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	int mode = MDB_MOD_LOCAL;

	if (argc > 1 && argv->a_type == MDB_TYPE_STRING &&
	    strcmp(argv->a_un.a_str, "-g") == 0) {
		mode = MDB_MOD_GLOBAL;
		argv++;
		argc--;
	}

	if (argc > 1 && argv->a_type == MDB_TYPE_STRING &&
	    strcmp(argv->a_un.a_str, "-f") == 0) {
		mode |= MDB_MOD_FORCE;
		argv++;
		argc--;
	}

	if (argc != 1 || argv->a_type != MDB_TYPE_STRING)
		return (DCMD_USAGE);

	if (mdb_module_load(argv->a_un.a_str, mode) != NULL)
		return (DCMD_OK);

	return (DCMD_ERR);
}

/*ARGSUSED*/
static int
cmd_unload(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc != 1 || argv->a_type != MDB_TYPE_STRING)
		return (DCMD_USAGE);

	if (mdb_module_unload(argv->a_un.a_str) == -1) {
		warn("failed to unload %s", argv->a_un.a_str);
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

static int
cmd_dbmode(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc > 1 || (argc != 0 && (flags & DCMD_ADDRSPEC)))
		return (DCMD_USAGE);

	if (argc != 0) {
		if (argv->a_type != MDB_TYPE_STRING)
			return (DCMD_USAGE);
		if ((addr = mdb_dstr2mode(argv->a_un.a_str)) != MDB_DBG_HELP)
			mdb_dmode(addr);
	} else if (flags & DCMD_ADDRSPEC)
		mdb_dmode(addr);

	mdb_printf("debugging mode = 0x%04x\n", mdb.m_debug);
	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_version(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
#ifdef DEBUG
	mdb_printf("\r%s (DEBUG)\n", mdb_conf_version());
#else
	mdb_printf("\r%s\n", mdb_conf_version());
#endif
	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_algol(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (mdb.m_flags & MDB_FL_ADB)
		mdb_printf("No algol 68 here\n");
	else
		mdb_printf("No adb here\n");
	return (DCMD_OK);
}

static int
print_global(void *data, const GElf_Sym *sym, const char *name)
{
	uintptr_t value;

	if (mdb_tgt_vread((mdb_tgt_t *)data, &value, sizeof (value),
	    (uintptr_t)sym->st_value) == sizeof (value))
		mdb_printf("%s(%llr):\t%lr\n", name, sym->st_value, value);
	else
		mdb_printf("%s(%llr):\t?\n", name, sym->st_value);

	return (0);
}

/*ARGSUSED*/
static int
cmd_globals(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc != 0)
		return (DCMD_USAGE);

	(void) mdb_tgt_symbol_iter(mdb.m_target, MDB_TGT_OBJ_EVERY,
	    MDB_TGT_SYMTAB, MDB_TGT_BIND_GLOBAL | MDB_TGT_TYPE_OBJECT |
	    MDB_TGT_TYPE_FUNC, print_global, mdb.m_target);

	return (0);
}

/*ARGSUSED*/
static int
cmd_eval(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc != 1 || argv->a_type != MDB_TYPE_STRING)
		return (DCMD_USAGE);

	if (mdb_eval(argv->a_un.a_str) == -1)
		return (DCMD_ERR);

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
print_file(void *data, const GElf_Sym *sym, const char *name)
{
	int i = *((int *)data);

	mdb_printf("%d\t%s\n", i++, name);
	*((int *)data) = i;
	return (0);
}

/*ARGSUSED*/
static int
cmd_files(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	int i = 1;

	if (argc != 0)
		return (DCMD_USAGE);

	(void) mdb_tgt_symbol_iter(mdb.m_target, MDB_TGT_OBJ_EVERY,
	    MDB_TGT_SYMTAB, MDB_TGT_BIND_ANY | MDB_TGT_TYPE_FILE,
	    print_file, &i);

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
print_map(void *ignored, const mdb_map_t *map, const char *name)
{
	if (name == NULL || *name == '\0') {
		if (map->map_flags & MDB_TGT_MAP_SHMEM)
			name = "[ shmem ]";
		else if (map->map_flags & MDB_TGT_MAP_STACK)
			name = "[ stack ]";
		else if (map->map_flags & MDB_TGT_MAP_HEAP)
			name = "[ heap ]";
		else if (map->map_flags & MDB_TGT_MAP_ANON)
			name = "[ anon ]";
	}

	mdb_printf("%?p %?p %?lx %s\n", map->map_base, map->map_base +
	    map->map_size, map->map_size, name ? name : map->map_name);
	return (0);
}

static int
cmd_mappings(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	const mdb_map_t *m;

	if (argc > 1 || (argc != 0 && (flags & DCMD_ADDRSPEC)))
		return (DCMD_USAGE);

	mdb_printf("%<u>%?s %?s %?s %s%</u>\n",
	    "BASE", "LIMIT", "SIZE", "NAME");

	if (flags & DCMD_ADDRSPEC) {
		if ((m = mdb_tgt_addr_to_map(mdb.m_target, addr)) == NULL)
			mdb_warn("failed to obtain mapping");
		else
			(void) print_map(NULL, m, NULL);

	} else if (argc != 0) {
		if (argv->a_type == MDB_TYPE_STRING)
			m = mdb_tgt_name_to_map(mdb.m_target, argv->a_un.a_str);
		else
			m = mdb_tgt_addr_to_map(mdb.m_target, argv->a_un.a_val);

		if (m == NULL)
			mdb_warn("failed to obtain mapping");
		else
			(void) print_map(NULL, m, NULL);

	} else if (mdb_tgt_mapping_iter(mdb.m_target, print_map, NULL) == -1)
		mdb_warn("failed to iterate over mappings");

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_objects(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc != 0)
		return (DCMD_USAGE);

	mdb_printf("%<u>%?s %?s %?s %s%</u>\n",
	    "BASE", "LIMIT", "SIZE", "NAME");

	if (mdb_tgt_object_iter(mdb.m_target, print_map, NULL) == -1) {
		warn("failed to iterate over objects");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

typedef struct {
	GElf_Sym nm_sym;
	const char *nm_name;
} nm_sym_t;

static const char *
nm_type2str(uchar_t info)
{
	switch (GELF_ST_TYPE(info)) {
	case STT_NOTYPE:
		return ("NOTY");
	case STT_OBJECT:
		return ("OBJT");
	case STT_FUNC:
		return ("FUNC");
	case STT_SECTION:
		return ("SECT");
	case STT_FILE:
		return ("FILE");
	case STT_SPARC_REGISTER:
		return ("REGI");
	default:
		return ("?");
	}
}

static const char *
nm_bind2str(uchar_t info)
{
	switch (GELF_ST_BIND(info)) {
	case STB_LOCAL:
		return ("LOCL");
	case STB_GLOBAL:
		return ("GLOB");
	case STB_WEAK:
		return ("WEAK");
	default:
		return ("?");
	}
}

static const char *
nm_sect2str(GElf_Half shndx)
{
	static char buf[16];

	switch (shndx) {
	case SHN_UNDEF:
		return ("UNDEF");
	case SHN_ABS:
		return ("ABS");
	case SHN_COMMON:
		return ("COMMON");
	default:
		(void) mdb_iob_snprintf(buf, sizeof (buf), "%hu", shndx);
		return ((const char *)buf);
	}
}

static int
nm_any(void *fmt, const GElf_Sym *sym, const char *name)
{
	mdb_iob_printf(mdb.m_out, (const char *)fmt,
	    sym->st_value, sym->st_size, nm_type2str(sym->st_info),
	    nm_bind2str(sym->st_info), sym->st_other,
	    nm_sect2str(sym->st_shndx), name);

	return (0);
}

static int
nm_undef(void *fmt, const GElf_Sym *sym, const char *name)
{
	if (sym->st_shndx == SHN_UNDEF) {
		mdb_iob_printf(mdb.m_out, (const char *)fmt,
		    sym->st_value, sym->st_size, nm_type2str(sym->st_info),
		    nm_bind2str(sym->st_info), sym->st_other,
		    nm_sect2str(sym->st_shndx), name);
	}

	return (0);
}

/*ARGSUSED*/
static int
nm_asgn(void *fmt, const GElf_Sym *sym, const char *name)
{
	const char *opts;

	switch (GELF_ST_TYPE(sym->st_info)) {
	case STT_FUNC:
		opts = "-f";
		break;
	case STT_OBJECT:
		opts = "-o";
		break;
	default:
		opts = "";
	}

	mdb_iob_printf(mdb.m_out, "%#llr::nmadd %s -s %#llr %s\n",
	    sym->st_value, opts, sym->st_size, name);

	return (0);
}

/*ARGSUSED*/
static int
nm_cnt_any(void *data, const GElf_Sym *sym, const char *name)
{
	size_t *cntp = (size_t *)data;
	(*cntp)++;
	return (0);
}

/*ARGSUSED*/
static int
nm_cnt_undef(void *data, const GElf_Sym *sym, const char *name)
{
	if (sym->st_shndx == SHN_UNDEF) {
		size_t *cntp = (size_t *)data;
		(*cntp)++;
	}
	return (0);
}

static int
nm_get_any(void *data, const GElf_Sym *sym, const char *name)
{
	nm_sym_t **sympp = (nm_sym_t **)data;

	(*sympp)->nm_sym = *sym;
	(*sympp)->nm_name = name;
	(*sympp)++;

	return (0);
}

static int
nm_get_undef(void *data, const GElf_Sym *sym, const char *name)
{
	if (sym->st_shndx == SHN_UNDEF) {
		nm_sym_t **sympp = (nm_sym_t **)data;

		(*sympp)->nm_sym = *sym;
		(*sympp)->nm_name = name;
		(*sympp)++;
	}
	return (0);
}

static int
nm_compare_name(const void *lp, const void *rp)
{
	const nm_sym_t *lhs = (nm_sym_t *)lp;
	const nm_sym_t *rhs = (nm_sym_t *)rp;

	return (strcmp(lhs->nm_name, rhs->nm_name));
}

static int
nm_compare_val(const void *lp, const void *rp)
{
	const nm_sym_t *lhs = (nm_sym_t *)lp;
	const nm_sym_t *rhs = (nm_sym_t *)rp;

	return (lhs->nm_sym.st_value < rhs->nm_sym.st_value ? -1 :
	    (lhs->nm_sym.st_value > rhs->nm_sym.st_value ? 1 : 0));
}

/*ARGSUSED*/
static int
cmd_nm(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	enum {
		NM_DYNSYM	= 0x0001,	/* -D (use dynsym) */
		NM_DEC		= 0x0002,	/* -d (decimal output) */
		NM_GLOBAL	= 0x0004,	/* -g (globals only) */
		NM_NOHDRS	= 0x0008,	/* -h (suppress header) */
		NM_OCT		= 0x0010,	/* -o (octal output) */
		NM_UNDEF	= 0x0020,	/* -u (undefs only) */
		NM_HEX		= 0x0040,	/* -x (hex output) */
		NM_SORT_NAME	= 0x0080,	/* -n (sort by name) */
		NM_SORT_VALUE	= 0x0100,	/* -v (sort by value) */
		NM_PRVSYM	= 0x0200,	/* -P (use private symtab) */
		NM_PRTASGN	= 0x0400	/* -p (print in asgn syntax) */
	};

	uint_t optf = 0;
	int i;

	mdb_tgt_sym_f *callback;
	uint_t which, type;

	const char *object = MDB_TGT_OBJ_EVERY;
	size_t hwidth, nsyms = 0;

	nm_sym_t *syms, *symp;
	const char *fmt;

	i = mdb_getopts(argc, argv,
	    'D', MDB_OPT_SETBITS, NM_DYNSYM, &optf,
	    'P', MDB_OPT_SETBITS, NM_PRVSYM, &optf,
	    'd', MDB_OPT_SETBITS, NM_DEC, &optf,
	    'g', MDB_OPT_SETBITS, NM_GLOBAL, &optf,
	    'h', MDB_OPT_SETBITS, NM_NOHDRS, &optf,
	    'n', MDB_OPT_SETBITS, NM_SORT_NAME, &optf,
	    'o', MDB_OPT_SETBITS, NM_OCT, &optf,
	    'p', MDB_OPT_SETBITS, NM_PRTASGN | NM_NOHDRS, &optf,
	    'u', MDB_OPT_SETBITS, NM_UNDEF, &optf,
	    'v', MDB_OPT_SETBITS, NM_SORT_VALUE, &optf,
	    'x', MDB_OPT_SETBITS, NM_HEX, &optf, NULL);

	if (i != argc) {
		if (argc != 0 && (argc - i) == 1) {
			if (argv[i].a_type != MDB_TYPE_STRING ||
			    argv[i].a_un.a_str[0] == '-')
				return (DCMD_USAGE);
			else
				object = argv[i].a_un.a_str;
		} else
			return (DCMD_USAGE);
	}

	if ((optf & (NM_DEC | NM_HEX | NM_OCT)) == 0) {
		switch (mdb.m_radix) {
		case 8:
			optf |= NM_OCT;
			break;
		case 10:
			optf |= NM_DEC;
			break;
		default:
			optf |= NM_HEX;
		}
	}

	switch (optf & (NM_DEC | NM_HEX | NM_OCT)) {
	case NM_DEC:
#ifdef _LP64
		fmt = "%-20llu|%-20llu|%-5s|%-5s|%-5u|%-8s|%s\n";
		hwidth = 20;
#else
		fmt = "%-10llu|%-10llu|%-5s|%-5s|%-5u|%-8s|%s\n";
		hwidth = 10;
#endif
		break;
	case NM_HEX:
#ifdef _LP64
		fmt = "0x%016llx|0x%016llx|%-5s|%-5s|0x%-3x|%-8s|%s\n";
		hwidth = 18;
#else
		fmt = "0x%08llx|0x%08llx|%-5s|%-5s|0x%-3x|%-8s|%s\n";
		hwidth = 10;
#endif
		break;
	case NM_OCT:
#ifdef _LP64
		fmt = "%-22llo|%-22llo|%-5s|%-5s|%-5o|%-8s|%s\n";
		hwidth = 22;
#else
		fmt = "%-11llo|%-11llo|%-5s|%-5s|%-5o|%-8s|%s\n";
		hwidth = 11;
#endif
		break;
	default:
		warn("-d/-o/-x options are mutually exclusive\n");
		return (DCMD_USAGE);
	}

	if (object != MDB_TGT_OBJ_EVERY && (optf & NM_PRVSYM)) {
		warn("-P/object options are mutually exclusive\n");
		return (DCMD_USAGE);
	}

	if (!(optf & NM_NOHDRS)) {
		mdb_iob_printf(mdb.m_out,
		    "%<u>%-*s %-*s %-5s %-5s %-5s %-8s %s%</u>\n",
		    hwidth, "Value", hwidth, "Size",
		    "Type", "Bind", "Other", "Shndx", "Name");
	}

	if (optf & NM_DYNSYM)
		which = MDB_TGT_DYNSYM;
	else
		which = MDB_TGT_SYMTAB;

	if (optf & NM_GLOBAL)
		type = MDB_TGT_BIND_GLOBAL | MDB_TGT_TYPE_ANY;
	else
		type = MDB_TGT_BIND_ANY | MDB_TGT_TYPE_ANY;

	if (optf & (NM_SORT_NAME | NM_SORT_VALUE)) {
		if (optf & NM_UNDEF)
			callback = nm_cnt_undef;
		else
			callback = nm_cnt_any;

		if (optf & NM_PRVSYM)
			nsyms = mdb_gelf_symtab_size(mdb.m_prsym);
		else {
			(void) mdb_tgt_symbol_iter(mdb.m_target, object,
			    which, type, callback, &nsyms);
		}

		if (nsyms == 0)
			return (DCMD_OK);

		syms = symp = mdb_alloc(sizeof (nm_sym_t) * nsyms,
		    UM_SLEEP | UM_GC);

		if (optf & NM_UNDEF)
			callback = nm_get_undef;
		else
			callback = nm_get_any;

		if (optf & NM_PRVSYM) {
			mdb_gelf_symtab_iter(mdb.m_prsym, callback, &symp);

		} else if (mdb_tgt_symbol_iter(mdb.m_target, object,
		    which, type, callback, &symp) == -1) {
			warn("failed to iterate over symbols");
			return (DCMD_ERR);
		}

		if (optf & NM_SORT_NAME)
			qsort(syms, nsyms, sizeof (nm_sym_t), nm_compare_name);
		else
			qsort(syms, nsyms, sizeof (nm_sym_t), nm_compare_val);
	}

	if ((optf & (NM_PRVSYM | NM_PRTASGN)) == (NM_PRVSYM | NM_PRTASGN))
		callback = nm_asgn;
	else if (optf & NM_UNDEF)
		callback = nm_undef;
	else
		callback = nm_any;

	if (optf & (NM_SORT_NAME | NM_SORT_VALUE)) {
		for (symp = syms; nsyms-- != 0; symp++)
			callback((void *)fmt, &symp->nm_sym, symp->nm_name);

	} else {
		if (optf & NM_PRVSYM) {
			mdb_gelf_symtab_iter(mdb.m_prsym,
			    callback, (void *)fmt);

		} else if (mdb_tgt_symbol_iter(mdb.m_target, object,
		    which, type, callback, (void *)fmt) == -1) {
			warn("failed to iterate over symbols");
			return (DCMD_ERR);
		}
	}

	return (DCMD_OK);
}

static int
cmd_nmadd(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uintptr_t opt_e = 0, opt_s = 0;
	uint_t opt_f = FALSE, opt_o = FALSE;

	GElf_Sym sym;
	int i;

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	i = mdb_getopts(argc, argv,
	    'f', MDB_OPT_SETBITS, TRUE, &opt_f,
	    'o', MDB_OPT_SETBITS, TRUE, &opt_o,
	    'e', MDB_OPT_UINTPTR, &opt_e,
	    's', MDB_OPT_UINTPTR, &opt_s, NULL);

	if (i != (argc - 1) || argv[i].a_type != MDB_TYPE_STRING ||
	    argv[i].a_un.a_str[0] == '-' || argv[i].a_un.a_str[0] == '+')
		return (DCMD_USAGE);

	if (opt_e && opt_e < addr) {
		warn("end (%p) is less than start address (%p)\n",
		    (void *)opt_e, (void *)addr);
		return (DCMD_USAGE);
	}

	if (mdb_gelf_symtab_lookup_by_name(mdb.m_prsym,
	    argv[i].a_un.a_str, &sym) == -1) {
		bzero(&sym, sizeof (sym));
		sym.st_info = GELF_ST_INFO(STB_GLOBAL, STT_NOTYPE);
	}

	if (opt_f)
		sym.st_info = GELF_ST_INFO(STB_GLOBAL, STT_FUNC);
	if (opt_o)
		sym.st_info = GELF_ST_INFO(STB_GLOBAL, STT_OBJECT);
	if (opt_e)
		sym.st_size = (GElf_Xword)(opt_e - addr);
	if (opt_s)
		sym.st_size = (GElf_Xword)(opt_s);
	sym.st_value = (GElf_Addr)addr;

	mdb_gelf_symtab_insert(mdb.m_prsym, argv[i].a_un.a_str, &sym);

	mdb_iob_printf(mdb.m_out, "added %s, value=%llr size=%llr\n",
	    argv[i].a_un.a_str, sym.st_value, sym.st_size);

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_nmdel(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	const char *name;
	GElf_Sym sym;

	if (argc != 1 || argv->a_type != MDB_TYPE_STRING ||
	    argv->a_un.a_str[0] == '-')
		return (DCMD_USAGE);

	name = argv->a_un.a_str;

	if (mdb_gelf_symtab_lookup_by_name(mdb.m_prsym, name, &sym) == 0) {
		mdb_gelf_symtab_delete(mdb.m_prsym, name, &sym);
		mdb_printf("deleted %s, value=%llr size=%llr\n",
		    name, sym.st_value, sym.st_size);
		return (DCMD_OK);
	}

	mdb_warn("symbol '%s' not found in private symbol table\n", name);
	return (DCMD_ERR);
}

static int
dis_str2addr(const char *s, uintptr_t *addr)
{
	GElf_Sym sym;

	if (s[0] >= '0' && s[0] <= '9') {
		*addr = (uintptr_t)mdb_strtoull(s);
		return (0);
	}

	if (mdb_tgt_lookup_by_name(mdb.m_target,
	    MDB_TGT_OBJ_EVERY, s, &sym) == -1) {
		mdb_warn("symbol '%s' not found\n", s);
		return (-1);
	}

	*addr = (uintptr_t)sym.st_value;
	return (0);
}

static int
cmd_dis(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_tgt_t *tgt = mdb.m_target;
	mdb_disasm_t *dis = mdb.m_disasm;

	uintptr_t naddr;
	mdb_tgt_as_t as;
	char buf[BUFSIZ];
	GElf_Sym sym;
	int i;

	uint_t opt_f = FALSE;		/* File-mode off by default */
	uint_t opt_w = FALSE;		/* Window mode off by default */
	uintptr_t n = 0;		/* Length of window in instructions */
	uintptr_t eaddr = 0;		/* Ending address; 0 if limited by n */

	i = mdb_getopts(argc, argv,
	    'f', MDB_OPT_SETBITS, TRUE, &opt_f,
	    'w', MDB_OPT_SETBITS, TRUE, &opt_w,
	    'n', MDB_OPT_UINTPTR, &n, NULL);

	/*
	 * Disgusting argument post-processing ... basically the idea is to get
	 * the target address into addr, which we do by using the specified
	 * expression value, looking up a string as a symbol name, or by
	 * using the address specified as dot.
	 */
	if (i != argc) {
		if (argc != 0 && (argc - i) == 1) {
			if (argv[i].a_type == MDB_TYPE_STRING) {
				if (argv[i].a_un.a_str[0] == '-')
					return (DCMD_USAGE);

				if (dis_str2addr(argv[i].a_un.a_str, &addr))
					return (DCMD_ERR);
			} else
				addr = argv[i].a_un.a_val;
		} else
			return (DCMD_USAGE);
	}

	/*
	 * If we're not in window mode yet, and some type of arguments were
	 * specified, see if the address corresponds nicely to a function.
	 * If not, turn on window mode; otherwise disassemble the function.
	 */
	if (opt_w == FALSE && (argc != i || (flags & DCMD_ADDRSPEC))) {
		if (mdb_tgt_lookup_by_addr(tgt, addr,
		    MDB_TGT_SYM_EXACT, buf, sizeof (buf), &sym) == 0 &&
		    GELF_ST_TYPE(sym.st_info) == STT_FUNC) {
			/*
			 * If the symbol has a size then set our end address to
			 * be the end of the function symbol we just located.
			 */
			if (sym.st_size != 0)
				eaddr = addr + (uintptr_t)sym.st_size;
		} else
			opt_w = TRUE;
	}

	/*
	 * Window-mode doesn't make sense in a loop.
	 */
	if (flags & DCMD_LOOP)
		opt_w = FALSE;

	/*
	 * If -n was explicit, limit output to n instructions;
	 * otherwise set n to some reasonable default
	 */
	if (n != 0)
		eaddr = 0;
	else
		n = 10;

	if (opt_f)
		as = MDB_TGT_AS_FILE;
	else
		as = MDB_TGT_AS_VIRT;

	if (opt_w == FALSE) {
		while ((eaddr == 0 && n-- != 0) || (addr < eaddr)) {
			naddr = mdb_dis_ins2str(dis, tgt, as, buf, addr);
			if (naddr == addr)
				return (DCMD_ERR);
			mdb_printf("%-#32a%8T%s\n", addr, buf);
			addr = naddr;
		}

	} else {
#ifdef __sparc
		uintptr_t oaddr;

		if (addr & 0x3) {
			warn("address is not properly aligned\n");
			return (DCMD_ERR);
		}

		for (oaddr = addr - (n * 4); oaddr != addr; oaddr = naddr) {
			naddr = mdb_dis_ins2str(dis, tgt, as, buf, oaddr);
			if (naddr == oaddr)
				return (DCMD_ERR);
			mdb_printf("%-#32a%8T%s\n", oaddr, buf);
		}
#endif

		if ((naddr = mdb_dis_ins2str(dis, tgt, as, buf, addr)) == addr)
			return (DCMD_ERR);

		mdb_printf("%<b>");
		mdb_flush();
		mdb_printf("%-#32a%8T%s%", addr, buf);
		mdb_printf("%</b>\n");

		for (addr = naddr; n-- != 0; addr = naddr) {
			naddr = mdb_dis_ins2str(dis, tgt, as, buf, addr);
			if (naddr == addr)
				return (DCMD_ERR);
			mdb_printf("%-#32a%8T%s\n", addr, buf);
		}
	}

	mdb_nv_set_value(mdb.m_dot, addr);
	return (DCMD_OK);
}

/*ARGSUSED*/
static int
walk_step(uintptr_t addr, const void *data, void *private)
{
	mdb_printf("%lr\n", addr);
	return (WALK_NEXT);
}

static int
cmd_walk(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	int status;

	if (argc < 1 || argc > 2 || argv[0].a_type != MDB_TYPE_STRING ||
	    argv[argc - 1].a_type != MDB_TYPE_STRING)
		return (DCMD_USAGE);

	if (argc > 1) {
		const char *name = argv[1].a_un.a_str;
		mdb_var_t *v = mdb_nv_lookup(&mdb.m_nv, name);
		const char *p;

		if (v != NULL && (v->v_flags & MDB_NV_RDONLY) != 0) {
			mdb_warn("variable %s is read-only\n", name);
			return (DCMD_ERR);
		}

		if (v == NULL && (p = strbadid(name)) != NULL) {
			mdb_warn("'%c' may not be used in a variable "
			    "name\n", *p);
			return (DCMD_ERR);
		}

		if (v == NULL && (v = mdb_nv_insert(&mdb.m_nv,
		    name, NULL, 0, 0)) == NULL)
			return (DCMD_ERR);

		mdb_vcb_insert(mdb_vcb_create(v), mdb.m_frame);
	}

	if (flags & DCMD_ADDRSPEC)
		status = mdb_pwalk(argv->a_un.a_str, walk_step, NULL, addr);
	else
		status = mdb_walk(argv->a_un.a_str, walk_step, NULL);

	if (status == -1) {
		mdb_warn("failed to perform walk");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_dump(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uintptr_t start, end;
	uchar_t buf[16];
	int i;

	if (argc != 0)
		return (DCMD_USAGE);

	/*
	 * Print a header line marking the location of the starting byte in
	 * the hex display (with \/) and the ASCII display (with v).
	 */
	for (mdb_printf("%11s", ""), i = 0; i < sizeof (buf); i++) {
		if ((addr & (sizeof (buf) - 1)) == i)
			mdb_iob_puts(mdb.m_out, "  \\/");
		else
			mdb_iob_printf(mdb.m_out, "  %x", i);
		if ((i & 7) == 7)
			mdb_iob_putc(mdb.m_out, ' ');
	}

	for (mdb_iob_putc(mdb.m_out, ' '), i = 0; i < sizeof (buf); i++) {
		if ((addr & (sizeof (buf) - 1)) == i)
			mdb_iob_putc(mdb.m_out, 'v');
		else
			mdb_iob_printf(mdb.m_out, "%x", i);
	}

	/*
	 * Compute the start and end of the surrounding 16-byte aligned region.
	 */
	mdb_iob_nlflush(mdb.m_out);
	start = addr & ~(sizeof (buf) - 1);
	end = (addr + mdb.m_dcount + (sizeof (buf) - 1)) & ~(sizeof (buf) - 1);

	/*
	 * Now read and display each 16-byte chunk in hex and ASCII.  We print
	 * the virtual address in 11 columns since this is enough space for
	 * most 64-bit kernel addresses, but allows us to fit in 80 columns.
	 */
	for (addr = start; addr < end; addr += sizeof (buf)) {
		if (mdb_tgt_vread(mdb.m_target, buf,
		    sizeof (buf), addr) != sizeof (buf)) {
			mdb_warn("failed to read data at %p", addr);
			goto done;
		}

		mdb_iob_printf(mdb.m_out, "%11p  ", addr);

		for (i = 0; i < sizeof (buf); i++) {
			mdb_iob_printf(mdb.m_out, "%02x ", (uint_t)buf[i]);
			if ((i & 7) == 7)
				mdb_iob_putc(mdb.m_out, ' ');
		}

		for (i = 0; i < sizeof (buf); i++) {
			if (buf[i] < '!' || buf[i] > '~')
				mdb_iob_putc(mdb.m_out, '.');
			else
				mdb_iob_putc(mdb.m_out, buf[i]);
		}

		mdb_iob_nlflush(mdb.m_out);
	}
done:
	mdb_nv_set_value(mdb.m_dot, addr);
	return ((flags & DCMD_LOOP) ? DCMD_ABORT : DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_echo(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	for (; argc-- != 0; argv++) {
		if (argv->a_type == MDB_TYPE_STRING)
			mdb_printf("%s ", argv->a_un.a_str);
		else
			mdb_printf("%llr ", argv->a_un.a_val);
	}

	mdb_printf("\n");
	return (DCMD_OK);
}

static int
cmd_typeset(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	int add_tag = 0, del_tag = 0;
	const char *p;
	mdb_var_t *v;

	if (argc == 0)
		return (cmd_vars(addr, flags, argc, argv));

	if (argv->a_type == MDB_TYPE_STRING && (argv->a_un.a_str[0] == '-' ||
	    argv->a_un.a_str[0] == '+')) {
		if (argv->a_un.a_str[1] != 't')
			return (DCMD_USAGE);
		if (argv->a_un.a_str[0] == '-')
			add_tag++;
		else
			del_tag++;
		argc--;
		argv++;
	}

	if (!(flags & DCMD_ADDRSPEC))
		addr = 0; /* set variables to zero unless explicit addr given */

	for (; argc-- != 0; argv++) {
		if (argv->a_type != MDB_TYPE_STRING)
			continue;

		if (argv->a_un.a_str[0] == '-' || argv->a_un.a_str[0] == '+') {
			warn("ignored bad option -- %s\n", argv->a_un.a_str);
			continue;
		}

		if ((p = strbadid(argv->a_un.a_str)) != NULL) {
			mdb_warn("'%c' may not be used in a variable "
			    "name\n", *p);
			return (DCMD_ERR);
		}

		if ((v = mdb_nv_lookup(&mdb.m_nv, argv->a_un.a_str)) == NULL) {
			v = mdb_nv_insert(&mdb.m_nv, argv->a_un.a_str,
			    NULL, addr, 0);
		} else if (flags & DCMD_ADDRSPEC)
			mdb_nv_set_value(v, addr);

		if (v != NULL) {
			if (add_tag)
				v->v_flags |= MDB_NV_TAGGED;
			if (del_tag)
				v->v_flags &= ~MDB_NV_TAGGED;
		}
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_context(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (argc != 0 || !(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb_tgt_setcontext(mdb.m_target, (void *)addr) == 0)
		return (DCMD_OK);

	return (DCMD_ERR);
}

/*ARGSUSED*/
static int
cmd_prompt(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	const char *p = "";

	if (argc != 0) {
		if (argc > 1 || argv->a_type != MDB_TYPE_STRING)
			return (DCMD_USAGE);
		p = argv->a_un.a_str;
	}

	(void) mdb_set_prompt(p);
	return (DCMD_OK);
}

/*ARGSUSED*/
static int
cmd_vtop(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	physaddr_t pa;

	if (argc != 0 || !(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb_tgt_vtop(mdb.m_target, MDB_TGT_AS_VIRT, addr, &pa) == -1) {
		mdb_warn("failed to get physical mapping");
		return (DCMD_ERR);
	}

	mdb_printf("virtual %lr mapped to physical %llr\n", addr, pa);
	return (DCMD_OK);
}

static void
nm_help(void)
{
	mdb_printf("-D   print .dynsym instead of .symtab\n"
	    "-P   print private symbol table instead of .symtab\n"
	    "-d   print value and size in decimal\n"
	    "-g   only print global symbols\n"
	    "-h   suppress header line\n"
	    "-n   sort symbols by name\n"
	    "-o   print value and size in octal\n"
	    "-p   print symbols as a series of ::nmadd commands\n"
	    "-u   only print undefined symbols\n"
	    "-v   sort symbols by value\n"
	    "-x   print value and size in hexadecimal\n"
	    "obj  specify object whose symbol table should be used\n");
}

static void
nmadd_help(void)
{
	mdb_printf("-f       set type of symbol to STT_FUNC\n"
	    "-o       set type of symbol to STT_OBJECT\n"
	    "-e end   set size of symbol to end - start address\n"
	    "-s size  set size of symbol to explicit value\n"
	    "name     specify symbol name to add\n");
}

/*
 * Table of built-in dcmds associated with the root 'mdb' module.  Future
 * expansion of this program should be done here, or through the external
 * loadable module interface.
 */
const mdb_dcmd_t mdb_dcmd_builtins[] = {
	{ ">", "variable-name", "assign variable", cmd_assign_variable },
	{ "$<", "macro-name", "replace input with macro file", cmd_exec_file },
	{ "$<<", "macro-name", "source macro file", cmd_src_file },
	{ "$>", "[file]", "log session to a file", cmd_old_log },
	{ "?", "fmt-list", "format data from object file", cmd_print_object },
	{ "/", "fmt-list", "format data from virtual as", cmd_print_core },
	{ "\\", "fmt-list", "format data from physical as", cmd_print_phys },
	{ "=", "fmt-list", "format immediate value", cmd_print_value },
	{ "$%", NULL, NULL, cmd_quit },
	{ "$?", NULL, "print status and registers", cmd_notsup },
	{ "$a", NULL, NULL, cmd_algol },
	{ "$c", "[cnt]", "print stack backtrace", cmd_notsup },
	{ "$C", "[cnt]", "print stack backtrace", cmd_notsup },
	{ "$d", NULL, "get/set default output radix", cmd_radix },
	{ "$D", "?[mode,...]", NULL, cmd_dbmode },
	{ "$e", NULL, "print listing of global symbols", cmd_globals },
	{ "$f", NULL, "print listing of source files", cmd_files },
	{ "$m", "?[name]", "print address space mappings", cmd_mappings },
	{ "$p", ":", "change debugger target context", cmd_context },
	{ "$P", "[prompt]", "set debugger prompt string", cmd_prompt },
	{ "$q", NULL, "quit debugger", cmd_quit },
	{ "$Q", NULL, "quit debugger", cmd_quit },
	{ "$r", NULL, "print general-purpose registers", cmd_notsup },
	{ "$s", NULL, "get/set symbol matching distance", cmd_symdist },
	{ "$v", NULL, "print non-zero variables", cmd_nzvars },
	{ "$V", "[mode]", "get/set disassembly mode", cmd_dismode },
	{ "$w", NULL, "get/set output page width", cmd_pgwidth },
	{ "$W", NULL, "re-open target in write mode", cmd_reopen },
	{ "$x", NULL, "print floating point registers", cmd_notsup },
	{ "$X", NULL, "print floating point registers", cmd_notsup },
	{ "$y", NULL, "print floating point registers", cmd_notsup },
	{ "$Y", NULL, "print floating point registers", cmd_notsup },
	{ ":A", "?[core|pid]", "attach to process or core file", cmd_notsup },
	{ ":R", NULL, "release the previously attached process", cmd_notsup },
	{ "attach", "?[core|pid]",
	    "attach to process or core file", cmd_notsup },
	{ "cat", "[file ...]", "concatenate and display files", cmd_cat },
	{ "context", ":", "change debugger target context", cmd_context },
	{ "dcmds", NULL, "list available debugger commands", cmd_dcmds },
	{ "dis", "?[-fw] [-n cnt] [addr]", "disassemble near addr", cmd_dis },
	{ "disasms", NULL, "list available disassemblers", cmd_disasms },
	{ "dismode", "[mode]", "get/set disassembly mode", cmd_dismode },
	{ "dmods", "[-l] [mod]", "list loaded debugger modules", cmd_dmods },
	{ "dump", "?", "dump memory from specified address", cmd_dump },
	{ "echo", "args ...", "echo arguments", cmd_echo },
	{ "eval", "command", "evaluate the specified command", cmd_eval },
	{ "files", NULL, "print listing of source files", cmd_files },
	{ "formats", NULL, "list format specifiers", cmd_formats },
	{ "fpregs", NULL, "print floating point registers", cmd_notsup },
	{ "grep", "expr", "print dot if expression is true", cmd_grep },
	{ "help", "[cmd]", "list commands/command help", cmd_help },
	{ "load", "module", "load debugger module", cmd_load },
	{ "log", "[-d | [-e] file]", "log session to a file", cmd_log },
	{ "map", "expr", "print dot after evaluating expression", cmd_map },
	{ "mappings", "?[name]", "print address space mappings", cmd_mappings },
	{ "nm", "[-DPdghnopuvx] [object]", "print symbols", cmd_nm, nm_help },
	{ "nmadd", ":[-fo] [-e end] [-s size] name",
	    "add name to private symbol table", cmd_nmadd, nmadd_help },
	{ "nmdel", "name", "remove name from private symbol table", cmd_nmdel },
	{ "objects", NULL, "print load objects map", cmd_objects },
	{ "quit", NULL, "quit debugger", cmd_quit },
	{ "regs", NULL, "print general purpose registers", cmd_notsup },
	{ "release", NULL,
	    "release the previously attached process", cmd_notsup },
	{ "set", "[-wF] [+/-o opt] [-s dist] [-I path] [-L path] [-P prompt]",
	    "get/set debugger properties", cmd_set },
	{ "stack", NULL, "print stack backtrace", cmd_notsup },
	{ "status", NULL, "print summary of current target", cmd_notsup },
	{ "typeset", "[+/-t] var ...", "set variable attributes", cmd_typeset },
	{ "unload", "module", "unload debugger module", cmd_unload },
	{ "unset", "[name ...]", "unset variables", cmd_unset },
	{ "vars", "[-npt]", "print listing of variables", cmd_vars },
	{ "version", NULL, "print debugger version string", cmd_version },
	{ "vtop", ":", "print physical mapping of virtual address", cmd_vtop },
	{ "walk", "?name [variable]", "walk data structure", cmd_walk },
	{ "walkers", NULL, "list available walkers", cmd_walkers },
	{ "whence", "[-v] name ...", "show source of walk or dcmd", cmd_which },
	{ "which", "[-v] name ...", "show source of walk or dcmd", cmd_which },
	{ "xdata", NULL, "print list of external data buffers", cmd_xdata },
	{ NULL }
};
