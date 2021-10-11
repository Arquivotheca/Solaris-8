/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PCONTROL_H
#define	_PCONTROL_H

#pragma ident	"@(#)Pcontrol.h	1.2	99/03/23 SMI"

/*
 * Implemention-specific include file for libproc process management.
 * This is not to be seen by the clients of libproc.
 */

#include <stdio.h>
#include <gelf.h>
#include <procfs.h>
#include <rtld_db.h>
#include <libproc.h>

#ifdef	__cplusplus
extern "C" {
#endif

#include "Putil.h"

/*
 * Definitions of the process control structures, internal to libproc.
 * These may change without affecting clients of libproc.
 */

typedef struct {		/* symbol table */
	Elf_Data *sym_data;	/* start of table */
	size_t	sym_symn;	/* number of entries */
	char	*sym_strs;	/* ptr to strings */
} sym_tbl_t;

typedef struct file_info {	/* symbol information for a mapped file */
	list_t	file_list;	/* linked list */
	char	file_pname[PRMAPSZ];	/* name from prmap_t */
	struct map_info *file_map;	/* primary (text) mapping */
	int	file_ref;	/* references from map_info_t structures */
	int	file_fd;	/* file descriptor for the mapped file */
	int	file_init;	/* 0: initialization yet to be performed */
	GElf_Half file_etype;	/* ELF e_type from ehdr */
	GElf_Half file_class;	/* ELF e_ident[EI_CLASS] from ehdr */
	rd_loadobj_t *file_lo;	/* load object structure from rtld_db */
	char	*file_lname;	/* load object name from rtld_db */
	char	*file_lbase;	/* pointer to basename of file_lname */
	Elf	*file_elf;	/* elf handle so we can close */
	sym_tbl_t file_symtab;	/* symbol table */
	sym_tbl_t file_dynsym;	/* dynamic symbol table */
	uintptr_t file_dyn_base;	/* load address for ET_DYN files */
	uintptr_t file_plt_base;	/* base address for PLT */
	size_t	file_plt_size;	/* size of PLT region */
	uintptr_t file_jmp_rel;	/* base address of PLT relocations */
} file_info_t;

typedef struct map_info {	/* description of an address space mapping */
	list_t	map_list;	/* linked list */
	prmap_t	map_pmap;	/* /proc description of this mapping */
	file_info_t *map_file;	/* pointer into list of mapped files */
	off64_t map_offset;	/* offset into core file (if core) */
	int map_external;	/* data is external to core file (if core) */
} map_info_t;

typedef struct lwp_info {	/* per-lwp information from core file */
	list_t	lwp_list;	/* linked list */
	lwpid_t	lwp_id;		/* lwp identifier */
	lwpsinfo_t lwp_psinfo;	/* /proc/<pid>/lwp/<lwpid>/lwpsinfo data */
	lwpstatus_t lwp_status;	/* /proc/<pid>/lwp/<lwpid>/lwpstatus data */
#if defined(sparc) || defined(__sparc)
	gwindows_t *lwp_gwins;	/* /proc/<pid>/lwp/<lwpid>/gwindows data */
	prxregset_t *lwp_xregs;	/* /proc/<pid>/lwp/<lwpid>/xregs data */
	int64_t *lwp_asrs;	/* /proc/<pid>/lwp/<lwpid>/asrs data */
#endif
} lwp_info_t;

typedef struct core_info {	/* information specific to core files */
	char core_dmodel;	/* data model for core file */
	int core_errno;		/* error during initialization if != 0 */
	map_info_t **core_map;	/* sorted array of map_info_t pointers */
	list_t core_lwp_head;	/* head of list of lwp info */
	lwp_info_t *core_lwp;	/* current lwp information */
	uint_t core_nlwp;	/* number of lwp's in list */
	off64_t core_size;	/* size of core file in bytes */
	char *core_platform;	/* platform string from core file */
	struct utsname *core_uts;	/* uname(2) data from core file */
	prcred_t *core_cred;	/* process credential from core file */
} core_info_t;

typedef struct elf_file {	/* convenience for managing ELF files */
	GElf_Ehdr e_hdr;	/* ELF file header information */
	Elf *e_elf;		/* ELF library handle */
	int e_fd;		/* file descriptor */
} elf_file_t;

typedef struct ps_rwops {	/* ops vector for Pread() and Pwrite() */
	ssize_t (*p_pread)(struct ps_prochandle *,
	    void *, size_t, uintptr_t);
	ssize_t (*p_pwrite)(struct ps_prochandle *,
	    const void *, size_t, uintptr_t);
} ps_rwops_t;

struct ps_prochandle {
	pstatus_t orig_status;	/* remembered status on Pgrab() */
	pstatus_t status;	/* status when stopped */
	psinfo_t psinfo;	/* psinfo_t from last Ppsinfo() request */
	uintptr_t sysaddr;	/* address of most recent syscall instruction */
	pid_t	pid;		/* process-ID */
	int	state;		/* state of the process, see "libproc.h" */
	uint_t	flags;		/* see defines below */
	uint_t	agentcnt;	/* Pcreate_agent()/Pdestroy_agent() ref count */
	int	asfd;		/* /proc/<pid>/as filedescriptor */
	int	ctlfd;		/* /proc/<pid>/ctl filedescriptor */
	int	statfd;		/* /proc/<pid>/status filedescriptor */
	int	agentctlfd;	/* /proc/<pid>/lwp/agent/ctl */
	int	agentstatfd;	/* /proc/<pid>/lwp/agent/status */
	int	info_valid;	/* if zero, map and file info need updating */
	uint_t	num_mappings;	/* number of map elements in map_info */
	uint_t	num_files;	/* number of file elements in file_info */
	list_t	map_head;	/* head of address space mappings */
	list_t	file_head;	/* head of mapped files w/ symbol table info */
	char	*execname;	/* name of the executable file */
	auxv_t	*auxv;		/* the process's aux vector */
	rd_agent_t *rap;	/* cookie for rtld_db */
	map_info_t *map_exec;	/* the mapping for the executable file */
	map_info_t *map_ldso;	/* the mapping for ld.so.1 */
	const ps_rwops_t *ops;	/* pointer to ops-vector for read and write */
	core_info_t *core;	/* information specific to core (if PS_DEAD) */
};

/* flags */
#define	CREATED		0x01	/* process was created by Pcreate() */
#define	SETSIG		0x02	/* set signal trace mask before continuing */
#define	SETFAULT	0x04	/* set fault trace mask before continuing */
#define	SETENTRY	0x08	/* set sysentry trace mask before continuing */
#define	SETEXIT		0x10	/* set sysexit trace mask before continuing */
#define	SETHOLD		0x20	/* set signal hold mask before continuing */
#define	SETREGS		0x40	/* set registers before continuing */

/* shorthand for register array */
#define	REG	status.pr_lwp.pr_reg

/*
 * Implementation functions in the process control library.
 * These are not exported to clients of the library.
 */
extern	int	Pscantext(struct ps_prochandle *);
extern	void	Pinitsym(struct ps_prochandle *);
extern	void	Preadauxvec(struct ps_prochandle *);
extern	void	Pbuild_file_symtab(struct ps_prochandle *, file_info_t *);
extern	map_info_t *Paddr2mptr(struct ps_prochandle *, uintptr_t);

extern	char 	*Pfindexec(struct ps_prochandle *, const char *,
	int (*)(const char *, void *), void *);

/*
 * Simple convenience.
 */
#define	TRUE	1
#define	FALSE	0

#ifdef	__cplusplus
}
#endif

#endif	/* _PCONTROL_H */
