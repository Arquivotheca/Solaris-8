/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_MODAPI_H
#define	_MDB_MODAPI_H

#pragma ident	"@(#)mdb_modapi.h	1.2	99/11/19 SMI"

/*
 * MDB Module API
 *
 * The debugger provides a set of interfaces for use in writing loadable
 * debugger modules.  Modules that call functions not listed in this header
 * file may not be compatible with future versions of the debugger.
 */

#include <sys/types.h>
#include <gelf.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Make sure that NULL, TRUE, FALSE, MIN, and MAX have the usual definitions
 * so module writers can depend on these macros and defines.
 */
#ifndef NULL
#if defined(_LP64) && !defined(__cplusplus)
#define	NULL	0L
#else
#define	NULL	0
#endif
#endif

#ifndef TRUE
#define	TRUE	1
#endif

#ifndef FALSE
#define	FALSE	0
#endif

#ifndef MIN
#define	MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define	MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define	MDB_API_VERSION	1	/* Current API version number */

/*
 * Debugger command function flags:
 */
#define	DCMD_ADDRSPEC	0x01	/* Dcmd invoked with explicit address */
#define	DCMD_LOOP	0x02	/* Dcmd invoked in loop with ,cnt syntax */
#define	DCMD_LOOPFIRST	0x04	/* Dcmd invoked as first iteration of LOOP */
#define	DCMD_PIPE	0x08	/* Dcmd invoked with input from pipe */
#define	DCMD_PIPE_OUT	0x10	/* Dcmd invoked with output set to pipe */

#define	DCMD_HDRSPEC(fl)	(((fl) & DCMD_LOOPFIRST) || !((fl) & DCMD_LOOP))

/*
 * Debugger command function return values:
 */
#define	DCMD_OK		0	/* Dcmd completed successfully */
#define	DCMD_ERR	1	/* Dcmd failed due to an error */
#define	DCMD_USAGE	2	/* Dcmd usage error; abort and print usage */
#define	DCMD_NEXT	3	/* Invoke next dcmd in precedence list */
#define	DCMD_ABORT	4	/* Dcmd failed; abort current loop or pipe */

#define	OFFSETOF(s, m)		(size_t)(&(((s *)0)->m))

extern int mdb_prop_postmortem;	/* Are we looking at a static dump? */
extern int mdb_prop_kernel;	/* Are we looking at a kernel? */

typedef enum {
	MDB_TYPE_STRING,	/* a_un.a_str is valid */
	MDB_TYPE_IMMEDIATE,	/* a_un.a_val is valid */
	MDB_TYPE_CHAR		/* a_un.a_char is valid */
} mdb_type_t;

typedef struct mdb_arg {
	mdb_type_t a_type;
	union {
		const char *a_str;
		uintmax_t a_val;
		char a_char;
	} a_un;
} mdb_arg_t;

typedef int mdb_dcmd_f(uintptr_t, uint_t, int, const mdb_arg_t *);

typedef struct mdb_dcmd {
	const char *dc_name;		/* Command name */
	const char *dc_usage;		/* Usage message (optional) */
	const char *dc_descr;		/* Description */
	mdb_dcmd_f *dc_funcp;		/* Command function */
	void (*dc_help)(void);		/* Command help function (or NULL) */
} mdb_dcmd_t;

#define	WALK_ERR	-1		/* Walk fatal error (terminate walk) */
#define	WALK_NEXT	0		/* Walk should continue to next step */
#define	WALK_DONE	1		/* Walk is complete (no errors) */

typedef int (*mdb_walk_cb_t)(uintptr_t, const void *, void *);

typedef struct mdb_walk_state {
	mdb_walk_cb_t walk_callback;	/* Callback to issue */
	void *walk_cbdata;		/* Callback private data */
	uintptr_t walk_addr;		/* Current address */
	void *walk_data;		/* Walk private data */
	void *walk_arg;			/* Walk private argument */
	const void *walk_layer;		/* Data from underlying layer */
} mdb_walk_state_t;

typedef struct mdb_walker {
	const char *walk_name;		/* Walk type name */
	const char *walk_descr;		/* Walk description */
	int (*walk_init)(mdb_walk_state_t *);	/* Walk constructor */
	int (*walk_step)(mdb_walk_state_t *);	/* Walk iterator */
	void (*walk_fini)(mdb_walk_state_t *);	/* Walk destructor */
	void *walk_init_arg;		/* Walk constructor argument */
} mdb_walker_t;

typedef struct mdb_modinfo {
	ushort_t mi_dvers;		/* Debugger version number */
	const mdb_dcmd_t *mi_dcmds;	/* NULL-terminated list of dcmds */
	const mdb_walker_t *mi_walkers;	/* NULL-terminated list of walks */
} mdb_modinfo_t;

typedef struct mdb_bitmask {
	const char *bm_name;		/* String name to print */
	u_longlong_t bm_mask;		/* Mask for bits */
	u_longlong_t bm_bits;		/* Result required for value & mask */
} mdb_bitmask_t;

typedef struct mdb_pipe {
	uintptr_t *pipe_data;		/* Array of pipe values */
	size_t pipe_len;		/* Array length */
} mdb_pipe_t;

extern int mdb_pwalk(const char *, mdb_walk_cb_t, void *, uintptr_t);
extern int mdb_walk(const char *, mdb_walk_cb_t, void *);

extern int mdb_pwalk_dcmd(const char *, const char *,
	int, const mdb_arg_t *, uintptr_t);

extern int mdb_walk_dcmd(const char *, const char *, int, const mdb_arg_t *);

extern int mdb_layered_walk(const char *, mdb_walk_state_t *);

extern int mdb_call_dcmd(const char *, uintptr_t,
	uint_t, int, const mdb_arg_t *);

extern int mdb_add_walker(const mdb_walker_t *);
extern int mdb_remove_walker(const char *);

extern ssize_t mdb_vread(void *, size_t, uintptr_t);
extern ssize_t mdb_vwrite(const void *, size_t, uintptr_t);

extern ssize_t mdb_pread(void *, size_t, uint64_t);
extern ssize_t mdb_pwrite(const void *, size_t, uint64_t);

extern ssize_t mdb_readstr(char *, size_t, uintptr_t);
extern ssize_t mdb_writestr(const char *, uintptr_t);

extern ssize_t mdb_readsym(void *, size_t, const char *);
extern ssize_t mdb_writesym(const void *, size_t, const char *);

extern ssize_t mdb_readvar(void *, const char *);
extern ssize_t mdb_writevar(const void *, const char *);

#define	MDB_SYM_NAMLEN	1024			/* Recommended max name len */

#define	MDB_SYM_FUZZY	0			/* Match closest address */
#define	MDB_SYM_EXACT	1			/* Match exact address only */

#define	MDB_OBJ_EXEC	((const char *)0L)	/* Primary executable file */
#define	MDB_OBJ_RTLD	((const char *)1L)	/* Run-time link-editor */
#define	MDB_OBJ_EVERY	((const char *)-1L)	/* All known symbols */

extern int mdb_lookup_by_name(const char *, GElf_Sym *);
extern int mdb_lookup_by_obj(const char *, const char *, GElf_Sym *);
extern int mdb_lookup_by_addr(uintptr_t, uint_t, char *, size_t, GElf_Sym *);

#define	MDB_OPT_SETBITS	1			/* Set specified flag bits */
#define	MDB_OPT_CLRBITS	2			/* Clear specified flag bits */
#define	MDB_OPT_STR	3			/* const char * argument */
#define	MDB_OPT_UINTPTR	4			/* uintptr_t argument */
#define	MDB_OPT_UINT64	5			/* uint64_t argument */

extern int mdb_getopts(int, const mdb_arg_t *, ...);

extern u_longlong_t mdb_strtoull(const char *);

#define	UM_NOSLEEP	0x0	/* Do not call failure handler; may fail */
#define	UM_SLEEP	0x1	/* Can block for memory; will always succeed */
#define	UM_GC		0x2	/* Garbage-collect this block automatically */

extern void *mdb_alloc(size_t, uint_t);
extern void *mdb_zalloc(size_t, uint_t);
extern void mdb_free(void *, size_t);

extern size_t mdb_snprintf(char *, size_t, const char *, ...);
extern void mdb_printf(const char *, ...);
extern void mdb_warn(const char *, ...);
extern void mdb_flush(void);

extern const char *mdb_one_bit(int, int, int);
extern const char *mdb_inval_bits(int, int, int);

extern ulong_t mdb_inc_indent(ulong_t);
extern ulong_t mdb_dec_indent(ulong_t);

extern int mdb_eval(const char *);
extern void mdb_set_dot(uintmax_t);
extern uintmax_t mdb_get_dot(void);

extern void mdb_get_pipe(mdb_pipe_t *);
extern void mdb_set_pipe(const mdb_pipe_t *);

extern ssize_t mdb_get_xdata(const char *, void *, size_t);

extern char *strcat(char *, const char *);
extern char *strcpy(char *, const char *);
extern char *strncpy(char *, const char *, size_t);
extern char *strchr(const char *, int);
extern char *strrchr(const char *, int);

extern int strcmp(const char *, const char *);
extern int strncmp(const char *, const char *, size_t);
extern int strcasecmp(const char *, const char *);
extern int strncasecmp(const char *, const char *, size_t);

extern size_t strlen(const char *);

extern int bcmp(const void *, const void *, size_t);
extern void bcopy(const void *, void *, size_t);
extern void bzero(void *, size_t);

extern void *memcpy(void *, const void *, size_t);
extern void *memmove(void *, const void *, size_t);
extern int memcmp(const void *, const void *, size_t);
extern void *memchr(const void *, int, size_t);
extern void *memset(void *, int, size_t);
extern void *memccpy(void *, const void *, int, size_t);

extern void *bsearch(const void *, const void *, size_t, size_t,
    int (*)(const void *, const void *));

extern void qsort(void *, size_t, size_t,
    int (*)(const void *, const void *));

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_MODAPI_H */
