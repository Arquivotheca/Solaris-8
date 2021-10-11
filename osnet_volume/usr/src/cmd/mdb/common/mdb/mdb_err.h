/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_ERR_H
#define	_MDB_ERR_H

#pragma ident	"@(#)mdb_err.h	1.2	99/11/19 SMI"

#include <stdarg.h>
#include <errno.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _MDB

#define	EMDB_BASE	1000			/* Base value for mdb errnos */

enum {
	EMDB_NOSYM = EMDB_BASE,			/* Symbol not found */
	EMDB_NOOBJ,				/* Object file not found */
	EMDB_NOMAP,				/* No mapping for address */
	EMDB_NODCMD,				/* Dcmd not found */
	EMDB_NOWALK,				/* Walk not found */
	EMDB_DCMDEXISTS,			/* Dcmd already exists */
	EMDB_WALKEXISTS,			/* Walk already exists */
	EMDB_NOPLAT,				/* No platform support */
	EMDB_NOPROC,				/* No process created yet */
	EMDB_NAME2BIG,				/* Name is too long */
	EMDB_NAMEBAD,				/* Name is invalid */
	EMDB_ALLOC,				/* Failed to allocate memory */
	EMDB_NOMOD,				/* Module not found */
	EMDB_BUILTINMOD,			/* Cannot unload builtin mod */
	EMDB_NOWCB,				/* No walk is active */
	EMDB_BADWCB,				/* Invalid walk state */
	EMDB_NOWALKLOC,				/* Walker doesn't accept addr */
	EMDB_NOWALKGLOB,			/* Walker requires addr */
	EMDB_WALKINIT,				/* Walker init failed */
	EMDB_WALKLOOP,				/* Walker layering loop */
	EMDB_IORO,				/* I/O stream is read-only */
	EMDB_IOWO,				/* I/O stream is write-only */
	EMDB_NOSYMADDR,				/* No symbol for address */
	EMDB_NODIS,				/* Disassembler not found */
	EMDB_DISEXISTS,				/* Disassembler exists */
	EMDB_NOSESPEC,				/* No software event spec */
	EMDB_NOXD,				/* No such xdata */
	EMDB_XDEXISTS,				/* Xdata name already exists */
	EMDB_TGTNOTSUP,				/* Op not supported by tgt */
	EMDB_TGTRDONLY,				/* Tgt not open for writing */
	EMDB_BADREG,				/* Invalid register name */
	EMDB_NOREGS,				/* No registers for thread */
	EMDB_STKALIGN,				/* Bad stack pointer align */
	EMDB_NOEXEC,				/* No executable file open */
	EMDB_EVAL,				/* Failed to mdb_eval() */
	EMDB_CANCEL,				/* Command cancelled by user */
	EMDB_PARTIAL,				/* Partial read occurred */
	EMDB_DCFAIL,				/* Dcmd failed */
	EMDB_DCUSAGE				/* Dcmd usage error */
};

extern const char *mdb_strerror(int);

extern void vwarn(const char *, va_list);
extern void vdie(const char *, va_list);
extern void vfail(const char *, va_list);

extern void warn(const char *, ...);
extern void die(const char *, ...);
extern void fail(const char *, ...);

extern int set_errbytes(size_t, size_t);
extern int set_errno(int);

#endif /* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_ERR_H */
