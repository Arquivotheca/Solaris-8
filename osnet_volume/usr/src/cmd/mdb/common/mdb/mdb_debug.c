/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_debug.c	1.1	99/08/11 SMI"

#include <mdb/mdb_debug.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_io.h>
#include <mdb/mdb_lex.h>
#include <mdb/mdb.h>

#include <libproc.h>
#include <rtld_db.h>
#include <strings.h>
#include <stdarg.h>

typedef struct dbg_mode {
	const char *m_name;
	const char *m_desc;
	uint_t m_bits;
} dbg_mode_t;

static const dbg_mode_t dbg_modetab[] = {
	{ "cmdbuf", "debug command editing buffer", MDB_DBG_CMDBUF },
#ifdef YYDEBUG
	{ "parser", "debug parser internals", MDB_DBG_PARSER },
#endif
	{ "help", "display this listing", MDB_DBG_HELP },
	{ "module", "debug module processing", MDB_DBG_MODULE },
	{ "dcmd", "debug dcmd processing", MDB_DBG_DCMD },
	{ "elf", "debug ELF file processing", MDB_DBG_ELF },
	{ "mach", "debug machine-dependent code", MDB_DBG_MACH },
	{ "shell", "debug shell escapes", MDB_DBG_SHELL },
	{ "kmod", "debug kernel module processing", MDB_DBG_KMOD },
	{ "walk", "debug walk callback processing", MDB_DBG_WALK },
	{ "umem", "debug memory management", MDB_DBG_UMEM },
	{ "dstk", "debug execution stack", MDB_DBG_DSTK },
	{ "tgt", "debug target backends", MDB_DBG_TGT },
	{ "psvc", "debug proc_service clients", MDB_DBG_PSVC },
	{ "proc", "debug libproc internals", MDB_DBG_PROC },
	{ "all", "set all debug modes", (uint_t)-1L },
	{ "none", "unset all debug modes", 0 },
	{ NULL, 0 }
};

static const char dbg_prefix[] = "mdb DEBUG: ";

/*PRINTFLIKE2*/
void
mdb_dprintf(uint_t mode, const char *format, ...)
{
	if ((mdb.m_debug & mode) == mode) {
		va_list alist;

		mdb_iob_puts(mdb.m_err, dbg_prefix);
		va_start(alist, format);
		mdb_iob_vprintf(mdb.m_err, format, alist);
		va_end(alist);
	}
}

void
mdb_dvprintf(uint_t mode, const char *format, va_list alist)
{
	if ((mdb.m_debug & mode) == mode) {
		mdb_iob_puts(mdb.m_err, dbg_prefix);
		mdb_iob_vprintf(mdb.m_err, format, alist);
	}
}

uint_t
mdb_dstr2mode(const char *s)
{
	const dbg_mode_t *mp;
	const char *name;
	char dstr[256];

	uint_t bits = 0;

	if (s == NULL)
		return (0);

	(void) strncpy(dstr, s, sizeof (dstr));
	dstr[sizeof (dstr) - 1] = '\0';

	for (name = strtok(dstr, ","); name; name = strtok(NULL, ",")) {
		for (mp = dbg_modetab; mp->m_name != NULL; mp++) {
			if (strcmp(name, mp->m_name) == 0) {
				if (mp->m_bits != 0)
					bits |= mp->m_bits;
				else
					bits = 0;
				break;
			}
		}

		if (mp->m_name == NULL)
			warn("unknown debug option \"%s\"\n", name);
	}

	if (bits & MDB_DBG_HELP) {
		warn("Debugging tokens:\n");
		for (mp = dbg_modetab; mp->m_name != NULL; mp++)
			warn("\t%s: %s\n", mp->m_name, mp->m_desc);
	}

	return (bits);
}

void
mdb_dmode(uint_t bits)
{
	_libproc_debug = (bits & MDB_DBG_PROC) != 0;
	rd_log((bits & MDB_DBG_PSVC) != 0);
	mdb_lex_debug(bits & MDB_DBG_PARSER);
	mdb.m_debug = bits;
}

#ifdef DEBUG
int
mdb_dassert(const char *expr, const char *file, int line)
{
	fail("\"%s\", line %d: assertion failed: %s\n", file, line, expr);
	/*NOTREACHED*/
	return (0);
}
#endif

/*
 * Function to convert mdb longjmp codes (see <mdb/mdb.h>) into a string for
 * debugging routines.
 */
const char *
mdb_err2str(int err)
{
	static const char *const errtab[] = {
		"0", "PARSE", "NOMEM", "PAGER", "SIGINT",
		"QUIT", "ASSERT", "API", "ABORT"
	};

	static char buf[32];

	if (err >= 0 && err < sizeof (errtab) / sizeof (errtab[0]))
		return (errtab[err]);

	(void) mdb_iob_snprintf(buf, sizeof (buf), "ERR#%d", err);
	return (buf);
}
