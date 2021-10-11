
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */
#pragma ident	"@(#)rdb.h	1.10	98/08/28 SMI"

#ifndef _RDB_H
#define	_RDB_H

#pragma	ident	"@(#)rdb.h	1.10	98/08/28 SMI"

#include <rtld_db.h>
#include <sys/types.h>
#include <sys/procfs.h>
#include <proc_service.h>
#include <libelf.h>
#include <gelf.h>

#include "rdb_mach.h"


/*
 * Definitions from 2.7 sys/procfs_isa.h.
 */
#ifndef	PR_MODEL_LP64
#define	PR_MODEL_UNKNOWN 0
#define	PR_MODEL_ILP32	1	/* process data model is ILP32 */
#define	PR_MODEL_LP64	2	/* process data model is LP64 */
#endif

#define	INTERPSECT	".interp"
#define	PLTSECT		".plt"


/*
 * Flags for step_n routine
 */
typedef enum {
	FLG_SN_NONE = 0,
	FLG_SN_VERBOSE = (1 << 0),	/* dissamble instructions */
	FLG_SN_PLTSKIP = (1 << 1)	/* step *over* PLTS */
} sn_flags_e;


typedef	enum {
	RET_FAILED = -1,
	RET_OK = 0
} retc_t;

typedef struct sym_tbl {
	Elf_Data *	st_syms;	/* start of table */
	char *		st_strs;	/* ptr to strings */
	size_t		st_symn;	/* number of entries */
} sym_tbl_t;


typedef struct	map_info {
	char *			mi_name;	/* file info */
	char *			mi_refname;	/* filter reference name */
	ulong_t			mi_addr;	/* start address */
	ulong_t			mi_end;		/* end address */
	int			mi_mapfd;	/* file desc. for mapping */
	unsigned		mi_pltentsz;	/* size of PLT entries */
	Elf *			mi_elf;		/* elf handle so we can close */
	GElf_Ehdr		mi_ehdr;
	sym_tbl_t		mi_symtab;	/* symbol table */
	sym_tbl_t		mi_dynsym;	/* dynamic symbol table */
	Lmid_t			mi_lmident;	/* Link Map Ident */
	ulong_t			mi_pltbase;	/* PLT base address */
	ulong_t			mi_pltsize;	/* size of PLT table */
	struct map_info *	mi_next;
	ulong_t			mi_flags;	/* misc flags */
	rd_loadobj_t		mi_loadobj;	/* keep the old loadobj for */
						/* 	good luck */
} map_info_t;

#define	FLG_MI_EXEC		0x0001		/* is object an EXEC */

#define	FLG_PAP_SONAME		0x0001		/* embed SONAME in sym name */
#define	FLG_PAP_NOHEXNAME	0x0002		/* if no symbol return */
						/* null string */


typedef struct map_list {
	map_info_t *		ml_head;
	map_info_t *		ml_tail;
} map_list_t;


/*
 * Break point information
 */
typedef struct bpt_struct {
	ulong_t			bl_addr;	/* address of breakpoint */
	bptinstr_t 		bl_instr;	/* original instruction */
	unsigned		bl_flags;	/* break point flags */
	struct bpt_struct *	bl_next;
} bptlist_t;

#define	FLG_BP_USERDEF		0x0001		/* user defined BP */
#define	FLG_BP_RDPREINIT	0x0002		/* PREINIT BreakPoint */
#define	FLG_BP_RDPOSTINIT	0x0004		/* POSTINIT BreakPoint */
#define	FLG_BP_RDDLACT		0x0008		/* DLACT BreakPoint */
#define	FLG_BP_PLTRES		0x0010		/* PLT Resolve BP */

#define	MASK_BP_SPECIAL \
		(FLG_BP_RDPREINIT | FLG_BP_RDPOSTINIT | FLG_BP_RDDLACT)
#define	MASK_BP_STOP \
		(FLG_BP_USERDEF | FLG_BP_PLTRES)
#define	MASK_BP_ALL \
		(MASK_BP_SPECIAL | FLG_BP_USERDEF)

/*
 * Proc Services Structure
 */

struct ps_prochandle {
	rd_agent_t *	pp_rap;		/* rtld_db handle */
	int		pp_fd;		/* open proc fd */
	ulong_t		pp_ldsobase;	/* ld.so.1 base address */
	map_info_t	pp_ldsomap;	/* ld.so.1 map info */
	map_info_t	pp_execmap;	/* exec map info */
	map_list_t	pp_lmaplist;	/* list of link map infos */
	bptlist_t *	pp_breakpoints;	/* break point list */
	auxv_t *	pp_auxvp;	/* pointer to AUX vectors */
	int		pp_flags;	/* misc flags */
	int		pp_dmodel;	/* data model */
};

#define	FLG_PP_PROMPT	0x0001		/* display debugger prompt */
#define	FLG_PP_LMAPS	0x0002		/* link maps available */
#define	FLG_PP_PACT	0x0004		/* active process being traced */
#define	FLG_PP_PLTSKIP	0x0008		/* PLT skipping is active */



/*
 * Debugging Structure
 */

typedef struct rtld_debug {
	int		rd_vers;
	caddr_t		rd_preinit;
	caddr_t		rd_postinit;
} rtld_debug_t;



#define	TRAPBREAK	0x91d02001	/* ta	ST_BREAKPOINT */


/*
 * values for rdb_flags
 */
#define	RDB_FL_EVENTS	0x0001		/* enable printing event information */

/*
 * Globals
 */

extern struct ps_prochandle	proch;
extern unsigned long		rdb_flags;

/*
 * Functions
 */
extern map_info_t *	addr_to_map(struct ps_prochandle *, ulong_t);
extern retc_t		addr_to_sym(struct ps_prochandle *, ulong_t,
				GElf_Sym *, char **);
extern void		CallStack(struct ps_prochandle * ph);
extern unsigned		continue_to_break(struct ps_prochandle *);
extern retc_t		delete_all_breakpoints(struct ps_prochandle *);
extern retc_t		delete_breakpoint(struct ps_prochandle *, ulong_t,
				unsigned);
extern void		disasm(struct ps_prochandle *, int);
extern retc_t		disasm_addr(struct ps_prochandle *, ulong_t, int);
extern retc_t		display_all_regs(struct ps_prochandle *);
extern retc_t		display_maps(struct ps_prochandle *);
extern retc_t		display_linkmaps(struct ps_prochandle *);
extern void		free_linkmaps(struct ps_prochandle *);
extern bptlist_t *	find_bp(struct ps_prochandle * ph, ulong_t addr);
extern ulong_t		get_ldbase(int pfd, int dmodel);
extern retc_t		get_linkmaps(struct ps_prochandle *);
extern ulong_t		hexstr_to_num(const char *);
extern void		list_breakpoints(struct ps_prochandle *);
extern retc_t		load_map(int, map_info_t * mp);
extern char *		print_address(unsigned long);
extern char *		print_address_ps(struct ps_prochandle *,
				unsigned long, unsigned);
extern void		print_mem(struct ps_prochandle *, ulong_t, int,
				char *);
extern void		print_varstring(struct ps_prochandle *, const char *);
extern void		print_mach_varstring(struct ps_prochandle *,
				const char *);
extern void		rdb_help(const char *);
extern void		rdb_prompt();
extern void		perr(char *);
extern retc_t		proc_string_read(struct ps_prochandle *,
				ulong_t, char *, int);
extern retc_t		ps_close(struct ps_prochandle *);
extern retc_t		ps_init(int, struct ps_prochandle *);
extern retc_t		set_breakpoint(struct ps_prochandle *, ulong_t,
				unsigned);
extern retc_t		set_objpad(struct ps_prochandle *, size_t);
extern retc_t		step_n(struct ps_prochandle *, size_t, sn_flags_e);
extern void		step_to_addr(struct ps_prochandle *, ulong_t);
extern retc_t		str_map_sym(const char *, map_info_t *, GElf_Sym *,
				char **);
extern map_info_t *	str_to_map(struct ps_prochandle *, const char *);
extern retc_t		str_to_sym(struct ps_prochandle *, const char *,
				GElf_Sym *);
extern int		yyparse(void);
extern void		yyerror(char *);
extern int		yylex(void);

#endif
