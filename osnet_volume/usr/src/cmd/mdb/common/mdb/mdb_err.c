/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_err.c	1.2	99/11/19 SMI"

#include <mdb/mdb_signal.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb.h>

#include <strings.h>
#include <stdlib.h>

static const char *const _mdb_errlist[] = {
	"unknown symbol name",				/* EMDB_NOSYM */
	"unknown object file name",			/* EMDB_NOOBJ */
	"no mapping for address",			/* EMDB_NOMAP */
	"unknown dcmd name",				/* EMDB_NODCMD */
	"unknown walk name",				/* EMDB_NOWALK */
	"dcmd name already in use",			/* EMDB_DCMDEXISTS */
	"walk name already in use",			/* EMDB_WALKEXISTS */
	"no support for platform",			/* EMDB_NOPLAT */
	"no process active",				/* EMDB_NOPROC */
	"specified name is too long",			/* EMDB_NAME2BIG */
	"specified name contains illegal characters",	/* EMDB_NAMEBAD */
	"failed to allocate needed memory",		/* EMDB_ALLOC */
	"specified module is not loaded",		/* EMDB_NOMOD */
	"cannot unload built-in module",		/* EMDB_BUILTINMOD */
	"no walk is currently active",			/* EMDB_NOWCB */
	"invalid walk state argument",			/* EMDB_BADWCB */
	"walker does not accept starting address",	/* EMDB_NOWALKLOC */
	"walker requires starting address",		/* EMDB_NOWALKGLOB */
	"failed to initialize walk",			/* EMDB_WALKINIT */
	"walker cannot be layered on itself",		/* EMDB_WALKLOOP */
	"i/o stream is read-only",			/* EMDB_IORO */
	"i/o stream is write-only",			/* EMDB_IOWO */
	"no symbol corresponds to address",		/* EMDB_NOSYMADDR */
	"unknown disassembler name",			/* EMDB_NODIS */
	"disassembler name already in use",		/* EMDB_DISEXISTS */
	"no such software event specifier",		/* EMDB_NOSESPEC */
	"no such xdata available",			/* EMDB_NOXD */
	"xdata name already in use",			/* EMDB_XDEXISTS */
	"operation not supported by target",		/* EMDB_TGTNOTSUP */
	"target is not open for writing",		/* EMDB_TGTRDONLY */
	"invalid register name",			/* EMDB_BADREG */
	"no register set available for thread",		/* EMDB_NOREGS */
	"stack address is not properly aligned",	/* EMDB_STKALIGN */
	"no executable file is open",			/* EMDB_NOEXEC */
	"failed to evaluate command",			/* EMDB_EVAL */
	"command cancelled by user",			/* EMDB_CANCEL */
	"only %lu of %lu bytes could be read",		/* EMDB_PARTIAL */
	"dcmd failed",					/* EMDB_DCFAIL */
	"improper dcmd usage"				/* EMDB_DCUSAGE */
};

static const int _mdb_nerr = sizeof (_mdb_errlist) / sizeof (_mdb_errlist[0]);

static size_t errno_rbytes;	/* EMDB_PARTIAL actual bytes read */
static size_t errno_nbytes;	/* EMDB_PARTIAL total bytes requested */

const char *
mdb_strerror(int err)
{
	static char buf[256];
	const char *str;

	if (err >= EMDB_BASE && (err - EMDB_BASE) < _mdb_nerr)
		str = _mdb_errlist[err - EMDB_BASE];
	else
		str = strerror(err);

	if (err == EMDB_PARTIAL) {
		(void) mdb_iob_snprintf(buf, sizeof (buf), str,
		    errno_rbytes, errno_nbytes);
		str = buf;
	}

	return (str ? str : "unknown error");
}

void
vwarn(const char *format, va_list alist)
{
	int err = errno;

	mdb_iob_printf(mdb.m_err, "%s: ", mdb.m_pname);
	mdb_iob_vprintf(mdb.m_err, format, alist);

	if (strchr(format, '\n') == NULL)
		mdb_iob_printf(mdb.m_err, ": %s\n", mdb_strerror(err));
}

void
vdie(const char *format, va_list alist)
{
	vwarn(format, alist);
	exit(1);
}

void
vfail(const char *format, va_list alist)
{
	extern const char *volatile _mdb_abort_str;
	static char buf[256];

	if (_mdb_abort_str == NULL) {
		_mdb_abort_str = buf; /* Do this first so we don't recurse */
		(void) mdb_iob_vsnprintf(buf, sizeof (buf), format, alist);
	}

	mdb_iob_printf(mdb.m_err, "%s ABORT: ", mdb.m_pname);
	mdb_iob_vprintf(mdb.m_err, format, alist);
	mdb_iob_flush(mdb.m_err);

	(void) mdb_signal_blockall();
	(void) mdb_signal_raise(SIGABRT);
	(void) mdb_signal_unblock(SIGABRT);
}

/*PRINTFLIKE1*/
void
warn(const char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vwarn(format, alist);
	va_end(alist);
}

/*PRINTFLIKE1*/
void
die(const char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vdie(format, alist);
	va_end(alist);
}

/*PRINTFLIKE1*/
void
fail(const char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vfail(format, alist);
	va_end(alist);
}

int
set_errbytes(size_t rbytes, size_t nbytes)
{
	errno_rbytes = rbytes;
	errno_nbytes = nbytes;
	errno = EMDB_PARTIAL;
	return (-1);
}

int
set_errno(int err)
{
	errno = err;
	return (-1);
}
