/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_H
#define	_MDB_H

#pragma ident	"@(#)mdb.h	1.2	99/11/19 SMI"

#include <mdb/mdb_nv.h>
#include <mdb/mdb_io.h>
#include <mdb/mdb_gelf.h>
#include <mdb/mdb_addrvec.h>
#include <mdb/mdb_argvec.h>
#include <mdb/mdb_target.h>
#include <mdb/mdb_disasm.h>
#include <mdb/mdb_module.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_list.h>
#include <mdb/mdb_vcb.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	MDB_ERR_PARSE	1	/* Error occurred in lexer or parser */
#define	MDB_ERR_NOMEM	2	/* Failed to allocate needed memory */
#define	MDB_ERR_PAGER	3	/* User quit current command from pager */
#define	MDB_ERR_SIGINT	4	/* User interrupt: abort current command */
#define	MDB_ERR_QUIT	5	/* User request: quit debugger */
#define	MDB_ERR_ASSERT	6	/* Assertion failure: abort current command */
#define	MDB_ERR_API	7	/* API function error: abort current command */
#define	MDB_ERR_ABORT	8	/* User abort or resume: abort to top level */

#define	MDB_DEF_RADIX	16	/* Default output radix */
#define	MDB_DEF_NARGS	6	/* Default # of arguments in stack trace */
#define	MDB_DEF_HISTLEN	128	/* Default length of command history */
#define	MDB_DEF_SYMDIST	0x8000	/* Default symbol distance for addresses */

#define	MDB_FL_PSYM	0x0001	/* Print dot as symbol + offset when possible */
#define	MDB_FL_LOG	0x0002	/* Logging is enabled */
#define	MDB_FL_NOMODS	0x0004	/* Skip automatic mdb module loading */
#define	MDB_FL_USECUP	0x0008	/* Use terminal cup initialization sequences */
#define	MDB_FL_ADB	0x0010	/* Enable stricter adb(1) compatibility */
#define	MDB_FL_FCHILD	0x0020	/* Follow child on fork(2) return */
#define	MDB_FL_IGNEOF	0x0040	/* Ignore EOF as a synonym for ::quit */
#define	MDB_FL_REPLAST	0x0080	/* Naked newline repeats previous command */
#define	MDB_FL_PAGER	0x0100	/* Enable pager by default */
#define	MDB_FL_LATEST	0x0200	/* Replace version string with "latest" */

#define	MDB_FL_VOLATILE	0x0001	/* Mask of all volatile flags to save/restore */

#define	MDB_PROMPTLEN	35	/* Maximum prompt length */

typedef struct mdb {
	uint_t m_tgtflags;	/* Target open flags (see mdb_target.h) */
	uint_t m_flags;		/* Miscellaneous flags (see above) */
	uint_t m_debug;		/* Debugging flags (see mdb_debug.h) */
	int m_radix;		/* Default radix for output formatting */
	int m_nargs;		/* Default number of arguments in stack trace */
	int m_histlen;		/* Length of command history */
	size_t m_symdist;	/* Distance from sym for addr match (0=smart) */
	const char *m_pname;	/* Program basename from argv[0] */
	char m_prompt[MDB_PROMPTLEN + 1]; /* Prompt for interactive mode */
	size_t m_promptlen;	/* Length of prompt in bytes */
	const char *m_shell;	/* Shell for ! commands and pipelines */
	char *m_root;		/* Root for path construction */
	char *m_ipathstr;	/* Path string for include path */
	char *m_lpathstr;	/* Path string for library path */
	const char **m_ipath;	/* Path for $< and $<< macro files */
	size_t m_ipathlen;	/* Length of underlying ipath buffer */
	const char **m_lpath;	/* Path for :: loadable modules */
	size_t m_lpathlen;	/* Length of underlying lpath buffer */
	mdb_modinfo_t m_rminfo;	/* Root debugger module information */
	mdb_module_t m_rmod;	/* Root debugger module (builtins) */
	mdb_module_t *m_mhead;	/* Head of module list (in load order) */
	mdb_module_t *m_mtail;	/* Tail of module list (in load order) */
	mdb_list_t m_tgtlist;	/* List of active target backends */
	mdb_tgt_t *m_target;	/* Current debugger target backend */
	mdb_nv_t m_disasms;	/* Hash of available disassemblers */
	mdb_disasm_t *m_disasm;	/* Current disassembler backend */
	mdb_nv_t m_modules;	/* Name/value hash for loadable modules */
	mdb_nv_t m_dcmds;	/* Name/value hash for extended commands */
	mdb_nv_t m_walkers;	/* Name/value hash for walk operations */
	mdb_nv_t m_nv;		/* Name/value hash for named variables */
	mdb_var_t *m_dot;	/* Variable reference for '.' */
	uintmax_t m_incr;	/* Current increment */
	uintmax_t m_raddr;	/* Most recent address specified to a dcmd */
	uintmax_t m_dcount;	/* Most recent count specified to a dcmd */
	mdb_var_t *m_rvalue;	/* Most recent value printed */
	mdb_var_t *m_roffset;	/* Most recent offset from an instruction */
	mdb_var_t *m_proffset;	/* Previous value of m_roffset */
	mdb_var_t *m_rcount;	/* Most recent count on $< dcmd */
	mdb_iob_t *m_in;	/* Input stream */
	mdb_iob_t *m_out;	/* Output stream */
	mdb_iob_t *m_err;	/* Error stream */
	mdb_io_t *m_term;	/* Terminal for interactive mode */
	mdb_io_t *m_log;	/* Log file i/o backend (NULL if not logging) */
	mdb_module_t *m_lmod;	/* Pointer to loading module, if in load */
	mdb_list_t m_lastc;	/* Last executed command list */
	mdb_gelf_symtab_t *m_prsym;		/* Private symbol table */
	struct mdb_frame *volatile m_frame;	/* Stack of execution frames */
	struct mdb_frame *volatile m_fmark;	/* Stack marker for pager */
	uint_t m_depth;		/* Depth of m_frame stack */
	volatile uint_t m_intr;	/* Don't allow SIGINT if set */
	volatile uint_t m_pend;	/* Pending SIGINT count */
} mdb_t;

#ifdef _MDB_PRIVATE
mdb_t mdb;
#else
extern mdb_t mdb;
#endif

#ifdef _MDB

extern void mdb_create(const char *, const char *);
extern void mdb_destroy(void);

extern int mdb_call_idcmd(mdb_idcmd_t *, uintmax_t, uintmax_t, uint_t,
    mdb_argvec_t *, mdb_addrvec_t *, mdb_vcb_t *);

extern int mdb_call(uintmax_t, uintmax_t, uint_t);
extern int mdb_run(void);

extern int mdb_set_prompt(const char *);
extern void mdb_set_ipath(const char *);
extern void mdb_set_lpath(const char *);

const char **mdb_path_alloc(const char *, const char **, size_t, size_t *);
extern void mdb_path_free(const char *[], size_t);

extern uintmax_t mdb_dot_incr(const char *);
extern uintmax_t mdb_dot_decr(const char *);

extern mdb_iwalker_t *mdb_walker_lookup(const char *);
extern mdb_idcmd_t *mdb_dcmd_lookup(const char *);
extern void mdb_dcmd_usage(const mdb_idcmd_t *, mdb_iob_t *);

extern void mdb_pservice_init(void);

extern void mdb_intr_enable(void);
extern void mdb_intr_disable(void);

#endif /* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_H */
