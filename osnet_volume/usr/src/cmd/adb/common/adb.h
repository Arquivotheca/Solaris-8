/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ADB_ADB_H
#define	_ADB_ADB_H

#pragma ident	"@(#)adb.h	1.61	99/05/04 SMI"

/*
 * adb - a debugger
 *
 * symbolic and kernel enhanced 4.2bsd version.
 *
 * this is a 32 bit machine version of this program.
 * it keeps large data structures in core, and expects
 * in several places that an int can represent 32 integers
 * and file indices.
 */

#include <sys/types.h>

#if ADB
#if !defined(_KERNEL) /* Egregious kludge */
#define	_KERNEL
#include <sys/siginfo.h>
#undef _KERNEL
#endif
#endif	/* ADB */

#include <sys/user.h>
#include <procfs.h>

#include <sys/elf.h>
#define	_a_out_h
/*
 * XXX - The above define should ultimately be taken out.  It's a
 * kluge until <stab.h> is corrected.
 *
 * <kvm.h> includes <nlist.h>, "adb.h" includes <stab.h>.
 * Both <nlist.h> and <stab.h> define 'nlist'.  One of them had to go,
 * chose the one in <stab.h>.  This problem is avoided in sparc by
 * having a local "stab.h" which is not desirable.
 */

#include <stab.h>
#include <sys/param.h>
#ifdef sparc
#include <vfork.h>
#endif sparc

#if	defined(__STDC__) && !defined(KADB)
#include <stdlib.h>
#endif	/* __STDC__ */

#include <ctype.h>

#undef NFILE    /* from sys/param.h */

#if defined(sparc)
#if defined(__sparcv9)
#include "../sparcv9/sparc.h"
#else
#include "../sparc/sparc.h"
#endif
#elif defined(i386)
#include "../i386/i386.h"
#elif defined(__ia64)
#include "../ia64/ia64.h"
#else
#error	ISA not supported
#endif

#include "process.h"

#ifdef	__cplusplus
extern "C" {
#endif

/* Address space constants	  cmd	which space		*/
#define	NSP	0		/* =	(no- or number-space)	*/
#define	ISP	1		/* ?	(object/executable)	*/
#define	DSP	2		/* /	(core file)		*/
#define	SSP	3		/* @	(source file addresses) */
#if defined(KADB)
#define	PSP	4		/* @	(kadb physical addresses) */
#endif

#define	STAR	4

#define	NSYM	0		/* no symbol space */
#define	ISYM	1		/* symbol in I space */
#define	DSYM	1		/* symbol in D space (== I space on VAX) */

#define	NVARS	10+26+26+1	/* variables [0..9,a..z,A..Z_] */
#define	PSYMVAR	(NVARS-1)	/* special variable used by psymoff() */
#define	ADB_DEBUG_MAX	9	/* max 'level' in db_printf(level, ...) */
#define	PROMPT_LEN_MAX	35	/* max length of prompt */
#define	LIVE_KERNEL_NAMELIST	"/dev/ksyms"
#define	LIVE_KERNEL_COREFILE	"/dev/mem"
#define	CALL_ARGS_MAX	20	/* max args in ::call command */


/* result type declarations */
char	*exform();

#ifndef	__STDC__		/* std C uses prototype in stdio.h. */
/* VARARGS */
int	printf();

char	*malloc(), *realloc(), *calloc();

#endif	/* (__STDC__ not defined) */

#ifdef KADB
void	*malloc(size_t), *realloc(void *, size_t), *calloc(size_t, size_t);
#endif

#ifdef KADB
int	ptrace();
#else
long	ptrace();
#endif

#ifdef KADB
char *strcpy(char *s1, const char *s2);
char *strncpy(char *s1, const char *s2, size_t n);
#else
#include <string.h>
#endif

#ifdef _LP64
unsigned long 	inkdot();
long	readreg();
#endif

/* miscellaneous globals */
char	*errflg;	/* error message, synchronously set */
char	*lp;		/* input buffer pointer */
int	interrupted;	/* was command interrupted ? */
#ifdef	_LP64
long	ditto;		/* last address expression */
long	expv;		/* expression value from last expr() call */
#else
int	ditto;		/* last address expression */
int	expv;
#endif
extern int	lastcom;	/* last command (=, /, ? or @) */
#ifdef _LP64
long	var[NVARS];	/* variables [0..9,a..z,A..Z_] */
#else
int	var[NVARS];	/* variables [0..9,a..z,A..Z_] */
#endif
char	sigpass[NSIG];	/* pass-signal-to-subprocess flags	  */
int	adb_pgrpid;	/* used by SIGINT and SIGQUIT signal handlers */
int	Fflag;		/* non-zero means force the takeover (:A) */

/*
 * Earlier versions of adb kept the registers within the "struct core".
 * On the sparc, I broke them out into their own separate structure
 * to make it easier to deal with the differences among adb, kadb and
 * adb -k.  This structure "allregs" is defined in "allregs.h" and the
 * variable (named "adb_regs") is declared only in accesssr.c.
 */
pstatus_t	Prstatus;
#ifdef	_LP64
pstatus32_t	Prstatus32;		/* We will define _SYSCALL32 */
					/* in the command line  to  */
					/* get the defs for pstatus32_t */
#endif

#ifdef sparc
prxregset_t	xregs;
#endif
#if !defined(i386) || !defined(KADB)
prfpregset_t	Prfpregs;
#endif

struct adb_raddr adb_raddr;

/*
 * Used for extended command processing
 */
struct ecmd {
	char *name;
	void (*func)();
	char *help;
};

#define	MAKE_LL(upper, lower)	\
	(((u_longlong_t)(upper) << 32) | (ulong_t)(lower))

extern	int errno;

int	hadaddress;	/* command had an address ? */
#ifdef	_LP64
addr_t	address;	/* address on command */
#else
int	address;	/* address on command */
#endif
int	hadcount;	/* command had a count ? */
int	count;		/* count on command */
int	length;		/* length on command */
int	phys_address;	/* kadb "@" command uses physical address */
int	phys_upper32;	/* kadb "@" command - allow for 64 bits of address */

extern int	radix;		/* radix for output */
extern int	maxoff;

char	nextc;		/* next character for input */
extern char	lastc;		/* previous input character */

int	xargc;		/* externally available argc */
extern int	wtflag;		/* -w flag ? */
char 	*isadir;	/* Subdirectory with ISA-specific stuff */

void	(*sigint)();
void	(*sigqit)();

/*
 *	Put prototypes here to clean up warnings
 */

/*
 * 	Ideally, we would change every single routine
 *	and use new style declaration here only, but
 *	that's probably another project in itself...
 */

/*
 *	kadb only routines declarations
 */
#ifdef KADB
void	asm_trap(int);
void 	bcopy();
int	close();
void	cmd();
void	doswitch();
void	_exitto();
void	_exitto64();
void	free();
void	getsn(char *, size_t);
char	*getsp();
int	in_prom();
void	killbuf();
#ifdef	sparc
int	lseek();
#endif
void	mach_fiximp();
void 	montrap();
int	read();
void	regs_to_core();
int	scopy();
void	settba();
void	sf_iflush(int *);
void	startup();
int	status_okay(int, char *, int);
char	*strcat(char *, const char *);
#ifdef	sparc
size_t	strlen(const char *);
#endif
int	strncmp(const char *, const char *, size_t);
int	strcmp();
void	spawn();
int	peek32();
short	peek();
int	Peekc();
int	pokec();
int	poke32();
int	pokes();
int	poketext();
void	prom_exit_to_mon(void);
void 	tryabort(int);
void	trypause();
unsigned long kernel_invoke(unsigned long (*)(),
			    unsigned long,
			    unsigned long []);
int physmem_read();
int physmem_write();
#endif	/* KADB */


/*
 *	adb only declarations
 */

#ifndef KADB
void	add_map_range();
void	change_dismode();
int	close();
void	closettyfd();
#ifdef	_SYSCALL32
void	convert_prstatus();
#endif
void	core_to_regs();
void	done();
void	endpcs();
void	enumerate_lwps();
#ifdef	_LP64
unsigned long find_rtldbase();
#else
int find_rtldbase();
#endif
void	free_shlib_map_ranges();
int	getfile();
int	get_platname();
void	killbuf();
void	kopen();
int	kread();
int	kwrite();
off_t	lseek(int, off_t, int);
void	printfuns();
int	proc_wait();
ssize_t read(int, void *, size_t);
void	setcor();
int	setty();
void	shell();
void	sigprint();
ssize_t write(int, const void *, size_t);
void	read_in_rtld();
void	read_in_shlib();
#ifdef	_LP64
void	read_in_shlib64();
char	*read_ld_debug64();
#endif
void	scan_linkmap();
int	setcor32();
int	setcor64();
void	set_lwp();
void	trampcheck();
void	trampnext();
void	xfer_regs();

#endif

/*
 * 	Common shared routines
 */

void		bpwait();
boolean_t	check_deferred_bkpt();
void		chkerr();
int		command(char *, char);
int		db_printf(short, ...);
void		delbp();
int		eol();
void		error(char *);
int		expr(int);
int		ext_slash();
int		findsym();
uint32_t	get();
void		getformat();
void		iclose(int, int);
struct asym	 *lookup_base(char *);
void		newline();
void		oclose();
void		print_dis(int, addr_t);
void		put(addr_t, int, int);
#ifdef _LP64
void		psymoff(long, int, char *);
#else
void		psymoff(int, int, char *);
#endif
char		rdc();
int		readproc(addr_t, char *, int);
void		scanform();
void		setbp();
void		setsym();
void		setvar();
char		*strchr(const char *, int);
int		ssymoff();
int		writeproc(addr_t, char *, int);


int	getreg();
int	writereg();
int	varchk();
void	printtrace();
char	nextchar();
char	readchar();
int	ext_ecmd();
struct ecmd *ext_getstruct();
void	subpcs();
void	flushbuf();
char	quotchar();
uint32_t	chkget();
int	bchkget();
int	getsig();
void	setup();
void	outofmem();
int	charpos();
void	endline();
void	printins();
void	printc();
void 	subtty();
void	adbtty();
int	runpcs();
void	resetcounts();
int	symhex();
int	setbrkpt();
void	getproc();
int	get_thread();
void	printfiles();
void	prints();
void	printregs();
int	fp_print();
void	printstack();
int	ka_main_loop();
int	ext_dol();
void	printmacros();
int	fenter();
#ifdef _LP64
unsigned long getLong();
#endif
void	setreg();
#if !defined(i386)
void	stacktop();
void	findentry();
#else
int	stacktop();		/* x86 returns an int */
int	findentry();
#endif
int	nextframe();
int	get_nrparms();
int	convdig();
void	printfpuregs();
void	ss_setup();
void 	wp_off();
#ifdef sparc
void	tbia();
#else
int	tbia();
#endif
void	ttysync();
#ifdef KADB
int	adb_ptrace();
#else
long	adb_ptrace();
#endif
int	exitproc();
int	extend_scan();
void	sort_globals();
int	iswp(int);
void	newsubtty();

#ifdef	__cplusplus
}
#endif

#endif	/* _ADB_ADB_H */
