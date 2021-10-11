/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pcs.c	1.41	99/05/04 SMI"

/*
 * adb - process control
 */

#include "adb.h"
#include <stdio.h>
#include "ptrace.h"
#if defined(KADB)
#include <symtab.h>
#include <sys/modctl.h>
#include <sys/bootconf.h>
#include <sys/kobj.h>
#include <sys/kobj_impl.h>
#include <sys/debug/debug.h>
#endif /* KADB */
#if defined(KADB) && defined(i386)
#include <sys/frame.h>
#endif

#if defined(KADB)
/*
 * Global data structures for deferred breakpoints.
 */
extern char imod_name[];
extern char isym_name[];
extern int  is_deferred_bkpt;

/*
 * notification for kobj/krtld
 */
kobj_notify_list load_event;
#ifdef _LP64
unsigned int		kobj_notify_list32[5];
#endif
int	kobj_notified = 0;	/* 1 = we told kobj to notify us on modload */

#define	KOBJ_ADD  0
#define	KOBJ_REMOVE 1
void kadb_notify_kobj_deferred_bkpt(int);
void deferred_bkpt_notify_check();
#ifdef _LP64
extern void kobj_notify_kadb_wrapper(unsigned int type, struct modctl *modp);
#endif
#endif /* KADB */


/* breakpoints */
struct	bkpt *bkpthead;

int	loopcnt;
int	ndebug;
#ifdef _LP64
addr_t	savdot;
#else
int	savdot;
#endif

int	datalen = 1;

extern char *rtld_path;

struct bkpt *
bkptlookup(addr)
	addr_t addr;
{
	register struct bkpt *bp;

	for (bp = bkpthead; bp; bp = bp->nxtbkpt)
#if defined(KADB)	/* kadb requires an exact match */
		if (bp->flag && bp->loc == addr &&
		    ((!is_deferred_bkpt) ||
		    (strcmp(imod_name, bp->mod_name) == 0 &&
		    strcmp(isym_name, bp->sym_name) == 0)))
			break;
#else
		if (bp->flag && bp->loc <= addr && addr < bp->loc + bp->len)
			break;
#endif
	return (bp);
}

/* return true if bkptr overlaps any other breakpoint in the list */
static int
bpoverlap(struct bkpt *bkptr)
{
	register struct bkpt *bp;

	for (bp = bkpthead; bp; bp = bp->nxtbkpt) {
		if (bp == bkptr || bp->flag == 0 || bp->type == BPDEFR)
			continue;
		if (bkptr->loc + bkptr->len > bp->loc &&
		    bkptr->loc < bp->loc + bp->len)
			return (1);
	}

	return (0);
}

struct bkpt *
get_bkpt(addr_t where, int type, int len)
{
	struct bkpt *bkptr;

	db_printf(5, "get_bkpt: where=%X", where);
	/* If there is one there all ready, clobber it. */
	bkptr = bkptlookup(where);
	if (bkptr) {
		bkptr->loc = where;
		bkptr->flag = 0;
	}

	/* Look for the first free entry on the list. */
	for (bkptr = bkpthead; bkptr; bkptr = bkptr->nxtbkpt)
		if (bkptr->flag == 0) {
#if defined(KADB)
			bkptr->mod_name[0] = '\0';
			bkptr->sym_name[0] = '\0';
#endif
			break;
	}

	/* If there wasn't one, get one and link it in the list. */
	if (bkptr == 0) {
		bkptr = (struct bkpt *)((uintptr_t)malloc(sizeof (*bkptr)));
		if (bkptr == 0)
			error("bkpt: no memory");
		bkptr->nxtbkpt = bkpthead;
		bkpthead = bkptr;
		bkptr->flag = 0;
	}
#ifdef INSTR_ALIGN_MASK
	if (type == BPINST && (where & INSTR_ALIGN_MASK)) {
		error(BPT_ALIGN_ERROR);
	}
#endif INSTR_ALIGN_MASK
	bkptr->loc = where;		/* set the location */
	bkptr->type = type;		/* and the type */
	bkptr->len = len;		/* and the length */

#if defined(KADB)
	strcpy(bkptr->mod_name, imod_name);
	strcpy(bkptr->sym_name, isym_name);
	/*
	 * We have a deferred breakpoint.  We cannot set it so just
	 * return.
	 */
	if (is_deferred_bkpt) {
		bkptr->type = BPDEFR;
		/*
		 * Tell kobj that we need notification.
		 */
		kadb_notify_kobj_deferred_bkpt(KOBJ_ADD);

		printf("Deferred breakpoint logged\n");

		return (bkptr);
	}
#endif /* KADB */

#if !defined(__ia64)
	(void) readproc(bkptr->loc, (char *)&(bkptr->ins), SZBPT);
#endif
	db_printf(5, "get_bkpt: returns %X", bkptr);
	return (bkptr);
}

void
subpcs(modif)
	int modif;
{
	register int check;
	int execsig;
	int runmode;
	struct bkpt *bkptr;
	char *comptr;
	int i, line, hitbp = 0;
	char *m;
	struct stackpos pos;
	int type, len;

	db_printf(4, "subpcs: modif='%c'", modif);
	execsig = 0;
	loopcnt = count;
	switch (modif) {

#if 0
	case 'D':
		dot = filextopc(dot);
		if (dot == 0)
			error("don't know pc for that source line");
		/* fall into ... */
#endif
	case 'd':
		bkptr = bkptlookup(dot);
		if (bkptr == 0)
			error("no breakpoint set");
		else if (kernel)
			(void) printf("Not possible with -k option.\n");
		else
			db_printf(2, "subpcs: bkptr=%X", bkptr);
		bkptr->flag = 0;
#if defined(KADB)
		deferred_bkpt_notify_check();
#endif
#if defined(KADB) && defined(sun4u)
		if (iswp(bkptr->type))
			ndebug--;
#endif
		return;

	case 'z':			/* zap all breakpoints */
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		for (bkptr = bkpthead; bkptr; bkptr = bkptr->nxtbkpt) {
			bkptr->flag = 0;
		}
		ndebug = 0;
#if defined(KADB)
		deferred_bkpt_notify_check();
#endif
		return;

#if 0
	case 'B':
		dot = filextopc(dot);
		if (dot == 0)
			error("don't know pc for that source line");
		/* fall into ... */
#endif
	case 'b':		/* set instruction breakpoint */
	case 'a':		/* set data breakpoint (access) */
	case 'w':		/* set data breakpoint (write) */
	case 'p':		/* set data breakpoint (exec) */
#if defined(KADB) && defined(i386)
	case 'P':		/* set data breakpoint (I/O) */
#endif
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
#if defined(KADB) && defined(sun4u)
	case 'f':		/* set data breakpoint (physical) */
		/*
		 * Fusion only has one virtual and one physical address
		 * watchpoint register, so let the user know when they've
		 * overwritten the existing watchpoint.
		 */
		if (ndebug) {
			if (modif == 'f') {
				if (bkptr = wplookup(BPPHYS)) {
					printf(
"overwrote previous watchpoint at PA %X\n",
					    bkptr->loc);
					bkptr->loc = dot;
					ndebug--;
				}
			} else if (modif != 'b' &&
			    (bkptr = wplookup(BPACCESS)) ||
			    (bkptr = wplookup(BPWRITE)) ||
			    (bkptr = wplookup(BPDBINS)) ||
			    (bkptr = wplookup(BPX86IO))) {
				printf(
"overwrote previous watchpoint at VA %X\n",
				    bkptr->loc);
				bkptr->loc = dot;
				ndebug--;
			}
		}
#endif	/* KADB && sun4u */
		if (modif == 'b') {
			type = BPINST;
			len = SZBPT;
		} else if (ndebug == NDEBUG) {
			error("bkpt: no more debug registers");
			/* NOTREACHED */
		} else if (modif == 'p') {
			type = BPDBINS;
			len = length;
#if defined(KADB)
			len = 1;
			ndebug++;
#endif
		} else if (modif == 'a') {
			type = BPACCESS;
			len = length;
#if defined(KADB)
			len = datalen;
			ndebug++;
#endif
		} else if (modif == 'w') {
			type = BPWRITE;
			len = length;
#if defined(KADB)
			len = datalen;
			ndebug++;
#endif
		}
#if defined(KADB) && defined(sun4u)
		else if (modif == 'f') {
			type = BPPHYS;
			len = 1;
			ndebug++;
		}
#endif
#if defined(KADB) && defined(i386)
		else if (modif == 'P') {
			extern int cpu_family;	/* I/O breakpoint supported? */
			if ((cpu_family == 0) || (*(int *)cpu_family < 5)) {
				error("bkpt: I/O breakpoint not supported"
				    " on this processor");
			}
			type = BPX86IO;
			len = 1;
			ndebug++;
		}
#endif
		bkptr = get_bkpt(dot, type, len);
		db_printf(2, "subpcs: bkptr=%X", bkptr);
		bkptr->initcnt = bkptr->count = count;
		bkptr->flag = BKPTSET;
		if (bpoverlap(bkptr)) {
			(void) printf("watched area overlaps other areas.\n");
			bkptr->flag = 0;
		}
		check = MAXCOM-1;
		comptr = bkptr->comm;
		(void) rdc(); lp--;
		do
			*comptr++ = readchar();
		while (check-- && lastc != '\n');
		*comptr = 0; lp--;
		if (check)
			return;
		error("bkpt command too long");
		/* NOTREACHED */
#if defined(KADB) && defined(i386)
	case 'I':
	case 'i':
		len = 8;
		if (expr(0))
			len = expv;
		switch (len) {
		case 8:
			printf("%x\n", inb(address)); break;
		/* Note: we're intentionally radix insensative */
		case 016:
		case 16:
		case 0x16:
			printf("%x\n", inw(address)); break;
		/* Note: we're intentionally radix insensative */
		case 032:
		case 32:
		case 0x32:
			printf("%X\n", inl(address)); break;
		default:
			printf("Only \":i\", \":i8\", \":i16\" or \":i32\" "
			    "are accepted\n");
			break;
		}
		return;

	case 'O':
	case 'o':
		len = 8;
		if (expr(0))
			len = expv;
		switch (len) {
		case 8:
			outb(address, count); break;
		case 016:
		case 16:
		case 0x16:
			outw(address, count); break;
		case 032:
		case 32:
		case 0x32:
			outl(address, count); break;
		default:
			printf("Only \":o\", \":o8\", \":o16\" or \":o32\" "
			    "are accepted\n");
			break;
		}
		outb(address, count);
		return;

#else
#ifndef KADB
	case 'i':
	case 't':
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		if (!hadaddress)
			error("which signal?");
		if (expv <= 0 || expv >= NSIG)
			error("signal number out of range");
		sigpass[expv] = modif == 'i';
		return;

	case 'l':
		if (!pid)
			error("no process");
		if (!hadaddress)
			error("no lwpid specified");
		db_printf(2, "subpcs: expv=%D, pid=%D", expv, pid);
		(void) set_lwp(expv, pid);
		return;

	case 'k':
		if (kernel)
			(void) printf("Not possible with -k option.\n");
		if (pid == 0)
			error("no process");
		printf("%d: killed", pid);
		endpcs();
		return;

	case 'r':
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		setup();
		runmode = PTRACE_CONT;
		subtty();
		db_printf(2, "subpcs: running pid=%D", pid);
		break;

	case 'A':			/* attach process */
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		if (pid)
			error("process already attached");
		if (!hadaddress)
			error("specify pid in current radix");
		if (ptrace(PTRACE_ATTACH, address) == -1)
			error("can't attach process");
		pid = address;
		bpwait(0);

		/*
		 *  From proc(4) man pages...
		 *
		 *  /proc/<pid>/object
		 *
		 *  The file name  a.out  appears in  the  /proc/<pid>/object
		 *  directory as an alias for the process's executable file.
		 *
		 *  The object directory makes it  possible  for  a
		 *  controlling process to gain access to the object file
		 *  and any shared libraries (and consequently the symbol
		 *  tables) without  having to know the actual path names
		 *  of the executable files.
		 */
		if ((symfil = (char *)malloc(256)) == NULL)
		    printf("warning: malloc failure - no link maps scanned\n");
		else {
		    sprintf(symfil, "/proc/%d/object/a.out", pid);
		    setsym();
		    setvar();
		    if (rtld_path) {
			read_in_rtld();
			scan_linkmap();
		    }
		}

		printf("process %d stopped at:\n", pid);
		print_dis(Reg_PC, 0);
		userpc = (addr_t)dot;
		return;

	case 'R':			/* release (detach) process */
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		if (!pid)
			error("no process");
		if (ptrace(PTRACE_DETACH, pid, readreg(Reg_PC), SIGCONT) == -1)
			error("can't detach process");
		pid = 0;
		return;
#endif !KADB
#endif !KADB && !i386

	case 'e':			/* execute instr. or routine */
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
#ifdef sparc
		/*
		 * Look for an npc that does not immediately follow pc.
		 * If that's the case, then look to see if the immediately
		 * preceding instruction was a call.  If so, we're in the
		 * delay slot of a pending call we that want to skip.
		 */
		if ((userpc + 4 != readreg(Reg_NPC)) &&
			/* Is the preceding instruction a CALL? */
			(((bchkget(userpc - 4, ISP) >> 6)  == SR_CALL_OP) ||
			/* Or, is the preceding instruction a JMPL? */
			(((bchkget(userpc - 4, ISP) >> 6) == SR_FMT3a_OP) &&
			((bchkget(userpc - 3, ISP) >> 2) == SR_JUMP_OP)))) {
			/* If there isn't a breakpoint there all ready */
			if (!bkptlookup(userpc + 4)) {
				bkptr = get_bkpt(userpc + 4, BPINST, SZBPT);
				bkptr->flag = BKPT_TEMP;
				bkptr->count = bkptr->initcnt = 1;
				bkptr->comm[0] = '\n';
				bkptr->comm[1] = '\0';
			}
			goto colon_c;
		}
		else
			modif = 's';	/* Behave as though it's ":s" */
		/* FALL THROUGH */
#endif sparc
#if defined(__ia64)
		m = disasm_opcode(userpc);
		if (strncmp(m, "br.call", 7) == 0) {
			if (!bkptlookup((userpc + 0x10) & ~0xf)) {
				bkptr = get_bkpt((userpc + 0x10) & ~0xf,
				    BPINST, SZBPT);
				bkptr->flag = BKPT_TEMP;
				bkptr->count = bkptr->initcnt = 1;
				bkptr->comm[0] = '\n';
				bkptr->comm[1] = '\0';
			}
			goto colon_c;
		} else
			modif = 's';
#endif /* __ia64 */
#ifdef i386
		/*
		 * Look for an call instr. If it is a call set break
		 * just after call so run thru the break point and stop
		 * at returned address.
		 */
		{
			unsigned op1;
			int	brkinc = 0;
			int	pc = readreg(REG_PC);

			op1 = (get(pc, DSP) & 0xff);
			/* Check for simple case first */
			if (op1 == 0xe8)
				brkinc = 5; /* length of break instr */
			else if (op1 == 0xff) {
				op1 = (get(pc+1, DSP) & 0xff);
				if ((op1 & 0x38) == 0x10) {
					if (op1 == 0x15)
						brkinc = 6; /* *disp32 */
					else if ((op1 &= 0xc0) == 0x80)
						brkinc = 6; /* disp32 */
					else if (op1 == 0x40)
						brkinc = 3; /* disp8 */
					else
						brkinc = 2; /* *reg, *(reg) */
				}
			}
			if (brkinc) {
				/*
				 * If there isn't a breakpoint there all ready
				 */
				if (!bkptlookup(userpc + brkinc)) {
					bkptr = get_bkpt(userpc + brkinc,
					    BPINST, SZBPT);
					bkptr->flag = BKPT_TEMP;
					bkptr->count = bkptr->initcnt = 1;
					bkptr->comm[0] = '\n';
					bkptr->comm[1] = '\0';
				}
				goto colon_c;
			}
			else
			    modif = 's';	/* Behave as though it's ":s" */
		}

		/* FALL THROUGH */
#endif i386

	case 's':
	case 'S':
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		if (pid) {
			execsig = getsig(signo);
			db_printf(2, "subpcs: execsig=%D", execsig);
		} else {
			setup();
			loopcnt--;
		}
		runmode = PTRACE_SINGLESTEP;
#if 0
		if (modif == 's')
			break;
		if ((pctofilex(userpc), filex) == 0)
			break;
		subtty();
		for (i = loopcnt; i > 0; i--) {
			line = (pctofilex(userpc), filex);
			if (line == 0)
				break;
			do {
				loopcnt = 1;
				if (runpcs(runmode, execsig)) {
					hitbp = 1;
					break;
				}
				if (interrupted)
					break;
			} while ((pctofilex(userpc), filex) == line);
			loopcnt = 0;
		}
#endif
		break;

#if defined(KADB) && defined(__ia64)
	case 'j':			/* Continue to next branch */
		{	extern int ia64_goto_next_branch;
			ia64_goto_next_branch = 1;
			modif = 'c';
			goto colon_c;
		}
		break;
#endif /* __ia64 */
	case 'u':			/* Continue to end of routine */
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		stacktop(&pos);
#ifdef	sparc
		savdot = pos.k_caller + 8;
#else	/* sparc */
		savdot = pos.k_caller;
#endif	/* sparc */
		db_printf(2, "subpcs: savdot=%X", savdot);
		bkptr = get_bkpt(savdot, BPINST, SZBPT);
		bkptr->flag = BKPT_TEMP;
		/* Associate this breakpoint with the caller's fp/sp. */
#if defined(KADB) && defined(i386)
		bkptr->count = pos.k_fp;
#endif
		bkptr->initcnt = 1;
		bkptr->comm[0] = '\n';
		bkptr->comm[1] = '\0';
		/* Fall through */

	case 'c':
	colon_c:
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		if (pid == 0)
			error("no process");
		runmode = PTRACE_CONT;
		execsig = getsig(signo);
		db_printf(2, "subpcs: execsig=%D", execsig);
		subtty();
		break;
#if defined(KADB) && defined(sun4u)
	case 'x':		/* switch cpu */
	{
		extern int to_cpu, switched;

		to_cpu = dot;
		if (to_cpu < 0 || to_cpu > NCPU)
			error("bad CPU number");
		hadaddress = 0;
		switched = 1;
		execsig = getsig(signo);
		runmode = PTRACE_CONT;
	}
	break;
#endif	/* KADB && sun4u */
	default:
		db_printf(3, "subpcs: bad modifier");
		error("bad modifier");
	}

	if (hitbp || (loopcnt > 0 && runpcs(runmode, execsig))) {
		m = "breakpoint at:\n";
	} else {
#if defined(KADB) && defined(sun4u)
		if ((int)tookwp())
			m = "watchpoint:\n";
		else
			m = "stopped at:\n";
#elif defined(KADB) && defined(i386)
		struct bkpt *tookhwbkpt(void);
		if ((int)tookhwbkpt())
			m = "hardware breakpoint:\n";
		else
			m = "stopped at:\n";
#elif defined(KADB)
		m = "stopped at:\n";
#else
		int sig = Prstatus.pr_lwp.pr_info.si_signo;
		int code = Prstatus.pr_lwp.pr_info.si_code;
		adbtty();
		delbp();
		if (sig == SIGTRAP &&
		    (code == TRAP_RWATCH ||
		    code == TRAP_WWATCH ||
		    code == TRAP_XWATCH)) {
			int pc = (int)Prstatus.pr_lwp.pr_info.si_pc;
			int addr = (int)Prstatus.pr_lwp.pr_info.si_addr;
			char *rwx;

			switch (code) {
			case TRAP_RWATCH:
				if (trapafter)
					rwx = "%16twas read at:\n";
				else
					rwx = "%16twill be read:\n";
				break;
			case TRAP_WWATCH:
				if (trapafter)
					rwx = "%16twas written at:\n";
				else
					rwx = "%16twill be written:\n";
				break;
			case TRAP_XWATCH:
				if (trapafter)
					rwx = "%16twas executed:\n";
				else
					rwx = "%16twill be executed:\n";
				break;
			}
			psymoff(addr, DSYM, rwx);
			print_dis(0, pc);
			dot = addr;
		} else {
			printf("stopped at:\n");
			print_dis(Reg_PC, 0);
		}
		resetcounts();
		return;
#endif
	}
	adbtty();
	delbp();
	printf(m);
	print_dis(Reg_PC, 0);
	resetcounts();
}

/*
 * Is this breakpoint a watchpoint?
 */
int
iswp(int type)
{
	switch (type) {
	case BPACCESS:
	case BPWRITE:
	case BPDBINS:
	case BPPHYS:
	case BPX86IO:
		return (1);
	}
	return (0);
}

/* loop up a watchpoint bkpt structure by address */
struct bkpt *
wptlookup(addr)
	addr_t addr;
{
	register struct bkpt *bp;

	for (bp = bkpthead; bp; bp = bp->nxtbkpt)
		if (bp->flag && iswp(bp->type) &&
		    bp->loc <= addr && addr < bp->loc + bp->len)
			break;
	return (bp);
}

#if !defined(KADB)
/* each KADB platform has to define one of these */
/* this is the user-level generic one */
struct bkpt *
tookwp()
{
	register struct bkpt *bp = NULL;

	/* if we are sitting on a watchpoint, find its bkpt structure */
	int sig = Prstatus.pr_lwp.pr_info.si_signo;
	int code = Prstatus.pr_lwp.pr_info.si_code;
	if (sig == SIGTRAP &&
	    (code == TRAP_RWATCH ||
	    code == TRAP_WWATCH ||
	    code == TRAP_XWATCH))
		bp = wptlookup((unsigned)Prstatus.pr_lwp.pr_info.si_addr);
	return (bp);
}
#endif

/* This is here for the convenience of both adb and kadb */
char *
map(request)		 /* for debugging purposes only */
	int request;
{
	static char buffer[16];

	switch (request) {
	case PTRACE_TRACEME:	return "PTRACE_TRACEME";
	case PTRACE_PEEKTEXT:	return "PTRACE_PEEKTEXT";
	case PTRACE_PEEKDATA:	return "PTRACE_PEEKDATA";
	case PTRACE_PEEKUSER:	return "PTRACE_PEEKUSER";
	case PTRACE_POKETEXT:	return "PTRACE_POKETEXT";
	case PTRACE_POKEDATA:	return "PTRACE_POKEDATA";
	case PTRACE_POKEUSER:	return "PTRACE_POKEUSER";
	case PTRACE_CONT:	return "PTRACE_CONT";
	case PTRACE_KILL:	return "PTRACE_KILL";
	case PTRACE_SINGLESTEP:	return "PTRACE_SINGLESTEP";
	case PTRACE_ATTACH:	return "PTRACE_ATTACH";
	case PTRACE_DETACH:	return "PTRACE_DETACH";
	case PTRACE_GETREGS:	return "PTRACE_GETREGS";
	case PTRACE_SETREGS:	return "PTRACE_SETREGS";
	case PTRACE_GETFPREGS:	return "PTRACE_GETFPREGS";
	case PTRACE_SETFPREGS:	return "PTRACE_SETFPREGS";
	case PTRACE_READDATA:	return "PTRACE_READDATA";
	case PTRACE_WRITEDATA:	return "PTRACE_WRITEDATA";
	case PTRACE_READTEXT:	return "PTRACE_READTEXT";
	case PTRACE_WRITETEXT:	return "PTRACE_WRITETEXT";
	case PTRACE_GETFPAREGS:	return "PTRACE_GETFPAREGS";
	case PTRACE_SETFPAREGS:	return "PTRACE_SETFPAREGS";
	case PTRACE_GETWINDOW:	return "PTRACE_GETWINDOW";
	case PTRACE_SETWINDOW:	return "PTRACE_SETWINDOW";
	case PTRACE_SYSCALL:	return "PTRACE_SYSCALL";
	case PTRACE_DUMPCORE:	return "PTRACE_DUMPCORE";
	case PTRACE_SETWRBKPT:	return "PTRACE_SETWRBKPT";
	case PTRACE_SETACBKPT:	return "PTRACE_SETACBKPT";
	case PTRACE_CLRDR7:	return "PTRACE_CLRDR7";
	case PTRACE_TRAPCODE:	return "PTRACE_TRAPCODE";
	case PTRACE_SETBPP:	return "PTRACE_SETBPP";
	case PTRACE_WPPHYS:	return "PTRACE_WPPHYS";
	}
	(void) sprintf(buffer, "PTRACE_%d", request);
	return (buffer);
}

#if defined(KADB)

/*
 * Tell krtld that we want to be notified everytime it loads a module.
 * type - indicates if we want to send the request for notification to krtld
 * or we want to tell krtld to cancel sending the notification.
 */
void
kadb_notify_kobj_deferred_bkpt(int type)
{
	int			retval;
	struct asym		*lookup();
	struct asym		*sym_ptr;
	int (*notify_func)(kobj_notify_list *) = NULL;
	void			kobj_notify_kadb_deferred_bkpt();
	void			set_jmp_env(), reset_jmp_env();
	char			*func_name;
#ifdef	_LP64
	unsigned long		(*func)();
	unsigned int		nargs;
	unsigned long		args[1];
	extern int elf64mode;
#endif

	/*
	 * We must make sure that we only call the kobj notify and
	 * remove routines only once while they are valid.
	 */
	if (type == KOBJ_ADD) {
		if (kobj_notified) {
			return;
		} else {
			func_name = "kobj_notify_add";
		}
	} else {	/* KOBJ_REMOVE */
		if (kobj_notified) {
			func_name = "kobj_notify_remove";
		} else {
			return;
		}
	}

	/*
	 * Find the location of the kobj notify routine and call it.
	 */
	if ((sym_ptr = lookup(func_name)) != NULL) {
		notify_func = (int (*)(kobj_notify_list *))(sym_ptr->s_value);
	} else {
		error("KADB can't find module load hook for deferred bkpt");
	}

	/*
	 * krtld links this data struct into its notification list
	 * so it must be global.  Pass in a callback function and
	 * tell krtld we want notification on a modload.
	 */
	load_event.kn_version = KOBJ_NVERSION_CURRENT;
	load_event.kn_func = kobj_notify_kadb_deferred_bkpt;
	load_event.kn_type = KOBJ_NOTIFY_MODLOAD;

	/*
	 * For x86 we need to save and retore the GS reg.
	 */
	set_jmp_env();

#ifdef _LP64
	func = (unsigned long(*)(kobj_notify_list *)) (notify_func);
	if (elf64mode)
		args[0] =  (unsigned long) &load_event;
#if !defined(__ia64)
	else {
		kobj_notify_list32[0] = load_event.kn_version;
		kobj_notify_list32[1] = (uint_t)kobj_notify_kadb_wrapper;
		kobj_notify_list32[2] = load_event.kn_type;
		kobj_notify_list32[4] = 0;
		kobj_notify_list32[5] = 0;
		args[0] = (unsigned long) &kobj_notify_list32[0];
	}
#endif /* __ia64 */

	if ((retval = kernel_invoke(func, 1, args)) < 0) {
		error("KADB can't set module load hook for deferred bkpt");
	}
#else
	if ((retval = ((*notify_func)(&load_event))) < 0) {
		error("KADB can't set module load hook for deferred bkpt");
	}
#endif
	reset_jmp_env();

	/*
	 * set these here after a successful call to krtld.
	 */
	if (type == KOBJ_ADD) {
			kobj_notified = 1;
	} else {	/* KOBJ_REMOVE */
			kobj_notified = 0;
	}
}

/*
 * krtld has just loaded a module and has passed us the name of the module.
 * Check to see if we have a deferred breakpoint to deal with at this time.
 */
/* ARGSUSED */
void
kobj_notify_kadb_deferred_bkpt(unsigned int type, struct modctl *modp)
{
	struct bkpt *bp;
	void	set_deferred_bkpt();
	char	name[50];
	int	id;
	extern	int elf64mode;
	/*
	 * Search the table of breakpoints for a match for the
	 * module name passed in.  Call to set a breakpoint for
	 * any module name that matches.
	 */
#ifdef __sparcv9
	extern void convert_modctl(unsigned long, char *, int *);
	if (!elf64mode) convert_modctl((unsigned long) modp, name, &id);
	else {
#endif
		id = modp->mod_id;
		strcpy(name, modp->mod_modname);
#ifdef __sparcv9
	}
#endif

	for (bp = bkpthead; bp; bp = bp->nxtbkpt) {
		if ((bp->type != BPDEFR) || (bp->flag == 0))
			continue;

		if (strcmp(bp->mod_name, name) == 0) {
			/*
			 * Call to set the regular breakpoint.
			 */
			set_deferred_bkpt(bp, id);
		}
	}
}

/*
 * Set a regular bkpt. mod_id is used so that during symbol lookup,
 * we only look in that specific module.
 */
void
set_deferred_bkpt(struct bkpt *bp, int mod_id)
{
	struct asym *sym_ptr;
	addr_t	sym_value = 0;
	extern int bkpt_curmod;
	extern void setbkpt();
	extern void trap_to_debugger();
	extern void set_kadb_dbg_env();
	extern void clear_kadb_dbg_env();

	/*
	 * When kadb goes to its command prompt, it sets up the
	 * environment it needs to be able to work.  However,
	 * deferred breakpoints work "under the covers" without the
	 * benefit of this environment setup.  On some architectures,
	 * this is OK because the execution environments are the
	 * same. On the sun4u, this is not the case so hooks have
	 * been installed to allow the setup a working environment.
	 */

	/*
	 * First, lookup the symbol since we can finally do it!
	 * Only look up the symbol in the module specified.
	 * If the symbol cannot be found in the module specified,
	 * remove the breakpoint from the breakpoint table and
	 * print an error message and give a command line prompt
	 * to the user to help corect the error.
	 */
	bkpt_curmod = mod_id;
	db_printf(8, "bkpt_curmod is %x\n", bkpt_curmod);

	if ((sym_ptr = lookup(bp->sym_name)) == NULL) {
		if (symhex(bp->sym_name)) {
			sym_value = expv;
		} else {
			bkpt_curmod = -1;
			printf("Deferred bkpt symbol:%s in mod:%s not found\n",
			    bp->sym_name, bp->mod_name);
			bp->flag = 0;	/* remove it from the list of bkpts */
			/* function to trap to enter debugger */
			trap_to_debugger();
			return;
		}
	} else {
		sym_value = (addr_t)(sym_ptr->s_value);
	}

	bkpt_curmod = -1;

	db_printf(8, "sym_value %J\n", sym_value);
	bp->loc = sym_value + bp->loc;
	bp->type = BPINST;

	set_kadb_dbg_env();
	setbrkpt(bp);
	clear_kadb_dbg_env();

	/*
	 * If this is the last deferred breakpoint, tell krtld not
	 * to notify us when a module is loaded.
	 */
	deferred_bkpt_notify_check();
}

/*
 * Call this early on when parsing the command line symbol names to
 * tell us if we have a deferred breakpoint.
 *
 * symbol - is the module name
 *
 * Return: 1 if this is a deferred bkpt, 0 if it is not.
 *
 */
boolean_t
check_deferred_bkpt(char *symbol)
{
	char	buf[MAXCOM];
	char	c;
	int	i;
	int	do_bkpt_now();
	extern	char lastc;
	extern	char *lp;
	extern	char isymbol[];
	extern	void readsym();

	/*
	 * By deferred breakpoint command syntax, the next symbol must
	 * be a '#'.
	 */
	if (*lp != '#')
		return (B_FALSE);

	strcpy(imod_name, symbol);

	lastc = *++lp;	/* increment to the next character */
	lp++;		/* point command buffer to char after lastc */

	/*
	 * Get the symbol name.
	 */
	readsym(0);
	strcpy(isym_name, isymbol);

	lastc = *--lp;

	/*
	 * This code is hokey, but it is used to enforce the command
	 * syntax for deferred breakpoints in that only deferred
	 * breakpoint commands {:b, :d} can use the mod_name#symbol
	 * syntax.  For all other commands, generate a syntax error.
	 */
	strcpy(buf, lp);
	i = 0;
	for (c = buf[0]; c != ':'; c = buf[++i]) {
		if ((c == '\n') || (c == ';') || i == (MAXCOM - 1)) {
			error("Bad command syntax");
		}
	}

	c = buf[++i];
	if ((c != 'b') && (c != 'd')) {
		error("Bad command syntax");
	}

	/*
	 * If we can set the breakpoint now, do it.
	 * Otherwise handle it as a deferred breakpoint.
	 */
	if (do_bkpt_now(imod_name, isym_name) == NULL) {
		is_deferred_bkpt = 1;
		expv = 0;
	}

	return (B_TRUE);
}

/*
 * For the given module name and symbol name, determine if we
 * can set a breakpoint now, or if we must defer because the
 * module in question has not yet been loaded.
 * Returns 0 if we must set a deferred breakpoint, or 1 if
 * we can set a breakpoint now.
 */
int
do_bkpt_now(char *modname, char *symname)
{
	int	mod_id;
	struct asym *s;
	extern int bkpt_curmod;	/* used for scoping in symbol lookup */
	extern int find_mod_id();

	/*
	 * -1 means that the module has not yet been loaded.
	 */
	if ((mod_id = find_mod_id(modname)) == -1) {
		return (0);
	}

	bkpt_curmod = mod_id;
	if ((s = lookup(symname)) == NULL) {
		if (symhex(symname)) {
			return (1);
		} else {
			bkpt_curmod = -1;
			error("Symbol not found");
		}
	}
	expv = s->s_value;
	bkpt_curmod = -1;
	return (1);
}

/*
 * Go through the link list of breakpoints and if there are no deferred
 * ones in the list, tell krtld that we don't need to be notified when
 * it loads a module.
 */
void
deferred_bkpt_notify_check()
{
	struct bkpt *bp;

	for (bp = bkpthead; bp; bp = bp->nxtbkpt) {
		if ((bp->type == BPDEFR) && (bp->flag != 0))
			return;
	}

	kadb_notify_kobj_deferred_bkpt(KOBJ_REMOVE);
}
#endif /* KADB */
