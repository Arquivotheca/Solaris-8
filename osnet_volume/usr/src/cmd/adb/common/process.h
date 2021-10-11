/*
 * Copyright (c) 1995, 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PROCESS_H
#define	_PROCESS_H

#pragma ident	"@(#)process.h	1.34	99/05/04 SMI"

#if defined(_KERNEL)
#include <sys/sysmacros.h>
#else
#define	_KERNEL
#include <sys/sysmacros.h>
#undef _KERNEL
#endif

#include <sys/elf.h>

#include <sys/thread.h>

#ifdef	__cplusplus
extern "C" {
#endif

addr_t	userpc;
#if !defined(KADB)
enum	{NOT_KERNEL,	/* -k option not used.  Not kernel debugging */
	LIVE,		/* adb -k on a live kernel */
	TRAPPED_PANIC,	/* adb -k on a dump got thru trap() (valid panic_reg) */
	CMN_ERR_PANIC	/* adb -k on a dump which has bypassed trap() */
	} kernel;
#else
int	kernel;
#endif
int	kcore;
int	slr;
/* leading 'C' in these names avoids similarly named system macros. */
proc_t	*Curproc;
kthread_id_t Curthread;
unsigned upage;
unsigned long physmem;
#ifdef _LP64
addr_t	dot;
#else
int	dot;
#endif
int	dotinc;
pid_t	pid;
int	executing;
int	fcor;
int	fsym;
int	signo;
int	sigcode;
extern int	trapafter;

Elf32_Ehdr	filhdr;
Elf64_Ehdr	filhdr64;
Elf32_Phdr	*proghdr;
Elf64_Phdr	*proghdr64;

#ifndef KADB
#ifdef u
#undef u
#endif
struct	user u;
#endif	/* !KADB */

char	*corfil, *symfil;

/*
 * file address maps : there are two maps, ? and /. Each consists of a sequence
 * of ranges. If mpr_b <= address <= mpr_e the f(address) = file_offset
 * where f(x) = x + (mpr_f-mpr_b). mpr_fn and mpr_fd identify the file.
 * the first 2 ranges are always present - additional ranges are added
 * if inspection of a core file indicates that some of the program text
 * is in shared libraries - one range per lib.
 */
struct map_range {
#ifdef _LP64
	unsigned long		mpr_b, mpr_e, mpr_f;	/* Now a long - LP64 */
#else
	int			mpr_b, mpr_e, mpr_f;
#endif
	char			*mpr_fn;
	int			mpr_fd;
	struct map_range	*mpr_next;
};

struct map {
	struct map_range	*map_head, *map_tail;
} txtmap, datmap;

/* bkpt flags */
#define	BKPTSET		0x01
#define	BKPT_TEMP	0x02		/* volatile bkpt flag */
#define	BKPT_EXACTIVE	0x10		/* watchpoint active for exec */
#define	BKPT_RDACTIVE	0x20		/* watchpoint active for read */
#define	BKPT_WRACTIVE	0x40		/* watchpoint active for write */
#define	BKPT_ERR	0x8000		/* breakpoint not successfully set */

/* bkpt types */
#define	BPINST	 0	/* :b - instruction breakpoint */
#define	BPWRITE	 1	/* :w - watchpoint on write access */
#define	BPACCESS 3	/* :a - watchpoint on read/write access */
#define	BPDBINS	 4	/* :p - watchpoint on execute access */
#define	BPPHYS	 5	/* :f - watchpoint on physical addr (r/w) (fusion) */
#define	BPDEFR	 6	/* :b - deferred breakpoint */
#define	BPX86IO	 7	/* :P - x86 kadb I/O breakpoint */

#define	NDEBUG	4		/* number of watchpoint regs on x86 */

#define	PTRACE_CLRBKPT	PTRACE_CLRDR7	/* clear all watchpoints */
#define	PTRACE_SETWR	PTRACE_SETWRBKPT
#define	PTRACE_SETAC	PTRACE_SETACBKPT

#define	MAXCOM	64

struct	bkpt {
	addr_t	loc;
	addr_t	ins;
	addr_t	ins2;	/* only needed for ia64 "movl" */
	int	count;
	int	initcnt;
	int	flag;
	int	type;
	int	len;
	char	comm[MAXCOM];
#if defined(KADB)
	char	mod_name[MAXCOM];	/* deferred bkpt module name */
	char	sym_name[MAXCOM];	/* deferred bkpt symbol name */
#endif
	struct	bkpt *nxtbkpt;
};

struct	bkpt *bkptlookup(addr_t);
struct  bkpt *wptlookup(addr_t);
struct	bkpt *wplookup(int);
struct	bkpt *tookwp(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _PROCESS_H */
