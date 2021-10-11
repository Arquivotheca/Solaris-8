/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)print.c	1.66	99/05/04 SMI"

/*
 * adb - print routines ($ command)
 */

#define	REGNAMESINIT
#include "adb.h"
#include "symtab.h"
#include <fcntl.h>
#ifndef KADB
#include "fpascii.h"
#include <string.h>
#include <signal.h>
#include <siginfo.h>
#include <stdio.h>
#endif	/* !KADB */

extern int	infile;
extern int	outfile;

#define	MAXOFF	0x8000
int		maxoff = MAXOFF;

#define	MAXPOS	80
int		maxpos = MAXPOS;

/* breakpoints */
struct	bkpt *bkpthead;
extern int	datalen;
int		trapafter = 0;
int		_printf();		/* adb/kadb internal printf */
#ifndef KADB
static	void	whichsigs();
#else
extern	char	*sprintf();	/* KADB uses the one in stand/lib */
#endif

static
void
printmap(s, amap)
	char *s;
	struct map *amap;
{
	int count;
	struct map_range *mpr;

	printf("%s%\n", s);
	count = 1;
	for (mpr = amap->map_head; mpr; mpr = mpr->mpr_next) {
#ifdef _LP64
		printf("b%D = %16J ", count, mpr->mpr_b);
		printf("e%D = %16J ", count, mpr->mpr_e);
		printf("f%D = %16J ", count, mpr->mpr_f);
#else
		printf("b%D = %8X ", count, mpr->mpr_b);
		printf("e%D = %8X ", count, mpr->mpr_e);
		printf("f%D = %6X ", count, mpr->mpr_f);
#endif
		printf("`%s'\n", mpr->mpr_fd < 0 ? "-" : mpr->mpr_fn);
		count++;
	}
}
void
printtrace(modif)
	int modif;
{
	int i, stack;
	struct map_range *mpr;
#ifdef _LP64
	extern int elf64mode;
#endif

	db_printf(4, "printtrace: modif='%c', pid=%D", modif, pid);
	if (hadcount == 0)
		count = -1;
	switch (modif) {

	case '<':
		if (count == 0) {
			while (readchar() != '\n')
				;
			lp--;
			break;
		}
		if (rdc() == '<')
			stack = 1;
		else {
			stack = 0;
			lp--;
		}
		/* fall into ... */
	case '>': {
		char file[MAXNAMELEN];
		int index;
#ifndef	KADB
		char Ifile[MAXPATHLEN];
		char *str_start, *str_end;
		extern char *Ipath;
#endif

		index = 0;
		if (modif == '<')
			iclose(stack, 0);
		else
			oclose();
		if (rdc() != '\n') {
			do {
				file[index++] = lastc;
				if (index >= MAXNAMELEN)
					error("filename too long");
			} while (readchar() != '\n');
			file[index] = '\0';
			if (modif == '<') {
#ifdef KADB
				/*
				 * There is no include path for kadb.
				 */
				db_printf(1, "file is %s\n", file);
				infile = open(file, 0);
#else	/* KADB */
				/*
				 * If name does not contain a '/', scan
				 * down the include path for the file.
				 */
				if (strchr(file, '/') != NULL) {
					infile = open(file, 0);
						db_printf(2, "%s\n", Ifile);
				} else {
					str_start = Ipath;
					do {
						str_end =
						    strchr(str_start, ':');
						if (str_end != NULL)
							*str_end = '\0';

						(void) sprintf(Ifile, "%s/%s",
						    str_start, file);

						db_printf(2, "%s\n", Ifile);
						infile = open(Ifile, 0);

						if (str_end != NULL)
							*str_end = ':';
						str_start = str_end + 1;
					} while (str_end != NULL && infile < 0);

				}
#endif	/* KADB */
				if (infile < 0) {
					infile = 0;
					error("can't open");
				}

				var[9] = hadcount ? count : 1;
			} else {
				outfile = open(file, 1);
				if (outfile < 0) {
				    if ((outfile = creat(file, 0666)) < 0) {
					outfile = 1;
					error("can't create");
				    }
				}
				else
					(void) lseek(outfile, (long)0, 2);
			}
		} else
			if (modif == '<')
				iclose(-1, 0);
		lp--;
		}
		break;

	case 'p':
#if	!defined(KADB)
		if (kernel == 0) {
			printfuns();
			break;
		}
#endif !KADB
		if (hadaddress) {
			Curproc = (proc_t *)dot;
			getproc();
		}
		break;
	case 't':
		if (hadaddress) {
			Curthread = (kthread_id_t)dot;
			get_thread();
		}
		break;
	case 'f':
		printfiles();
		break;

#ifndef KADB
	case 'i':
		whichsigs();
		break;
#endif !KADB
	case 'D': {
			extern int adb_debug;

			if (!hadaddress) {
				printf("debugging adb itself at level = %D\n",
								    adb_debug);
				break;
			}
#ifndef _LP64
			if (address < 0)
				error("must have adb_debug_level >= 0");
#endif
			adb_debug = (address > ADB_DEBUG_MAX) ?
						ADB_DEBUG_MAX : (int)address;
			if (adb_debug)
				printf("debugging adb itself at level %D\n",
								adb_debug);
			else
				printf("debugging adb itself, turned off\n");
		}
		break;
	case 'P': {
			int len = 0;
			char *buf, *prmt;
			extern int cmd_line_prompt;
			extern char *prompt;

			buf = prmt = (char *)malloc(PROMPT_LEN_MAX);
			if (buf == NULL)
				outofmem();
			(void) rdc();
			lp--;
			do
				*prmt++ = readchar();
			while (++len <= PROMPT_LEN_MAX && lastc != '\n');
			lp--;
			if (len <= PROMPT_LEN_MAX) {
				--prmt;
				if (buf == prmt)
					prompt = NULL;
				else {
					*prmt = '\0';
					if (prompt == NULL || cmd_line_prompt) {
						prompt = (char *)malloc(len);
						cmd_line_prompt = 0;
					} else if ((size_t)len >
					    strlen(prompt) + 1)
						prompt = (char *)
							realloc(prompt, len);
					if (prompt == NULL)
						outofmem();
					(void) strcpy(prompt, buf);
				}
			} else
				error("too many characters in the prompt");
			free(buf);
			db_printf(9, "printtrace: prompt='%s'",
					    (prompt == NULL) ? "NULL" : prompt);
		}
		break;
	case 'd':
		if (hadaddress) {
			if (address < 2 || address > 16)
				error("must have 2 <= radix <= 16");
			radix = (int)address;
		}
		printf("radix=%d base ten", radix);
		break;

	case 'Q': case 'q': case '%':
#ifdef KADB
		_exit(0, 0);		/* args are bogus */
		break;
#else
		done();
#endif

	case 'w':
		maxpos = hadaddress ? (int)address : MAXPOS;
		break;

	case 's':
		maxoff = hadaddress ? (int)address : MAXOFF;
		break;

	case 'v':
		prints("variables\n");
		for (i = 0; i < NVARS; i++)
			if (var[i]) {
				char c;
				if (i < 10)
					c = i + '0';
				else if (i < 10 + 26)
					c = i - 10 + 'a';
				else if (i < 10 + 26 + 26)
					c = i - 36 + 'A';
				else
					c = '_';
				printc(c);
#ifdef	_LP64
				printf(" = %J\n", var[i]);
#else
				printf(" = %X\n", var[i]);
#endif
			}
		break;

	case 'm':
		printmap("? map", &txtmap);
		printmap("/ map", &datmap);
		break;

	case 0:
	case '?':
#ifndef KADB
		if (pid)
			printf("process id = %D\n", pid);
		else
			prints("no process\n");
		sigprint(signo);
#endif !KADB
		flushbuf();
		/* fall into ... */
	case 'r':
		printregs();
		return;
	/* Floating point register commands are machine-specific */
	case 'x':
	case 'X':
#ifdef sparc
	case 'y':
	case 'Y':
#endif
		fp_print(modif);	/* see ../${CPU}/print*.c */
		return;

	case 'C':
	case 'c':
		printstack(modif);
		return;

	case 'e': {
		register struct asym **p;
		register struct asym *s;

		for (p = globals; p < globals + nglobals; p++) {
			s = *p;
			switch (s->s_type) {

			case STT_OBJECT:
			case STT_FUNC:
#ifdef	_LP64
				printf("%s(%J):%12t%X\n", demangled(s),
				    s->s_value, (unsigned int)get(s->s_value));
#else
				printf("%s(%X):%12t%X\n", demangled(s),
				    (int)s->s_value, get((int)s->s_value));
#endif
			}
		}
		}
		break;

	case 'a':
		error("No algol 68 here");
		/*NOTREACHED*/


	case 'W':
		wtflag = 2;
#ifndef KADB
		close(fsym);
		close(fcor);
		kopen();		/* re-init libkvm code, if kernel */
		fsym = getfile(symfil, 1);
		mpr = txtmap.map_head;	/* update fd fields of the first */
					/* 2 map_ranges */
		mpr->mpr_fd = fsym; mpr->mpr_next->mpr_fd = fsym;
		fcor = getfile(corfil, 2);
		mpr = datmap.map_head;
		mpr->mpr_fd = fsym; mpr->mpr_next->mpr_fd = fcor;
#endif !KADB
		break;

#ifdef KADB
	case 'K':
		/* the read char specifies serial or net connection */
		/* jump to kernel agent for source level debugging */
		ka_main_loop(readchar());
		break;
	case 'M':
		printmacros();
		break;
#endif KADB
#if	!defined(KADB)
	case 'L':			/* Show all lwp ids. */
		if (!pid)
			error("no process");
		(void) enumerate_lwps(pid);
		break;

	case 'l':			/* Show current lwp's id. */
		if (!pid)
			error("no process");
		printf("%D\n", Prstatus.pr_lwp.pr_lwpid);
		break;

#ifdef sparc
	case 'V':
		if (rdc() == '\n' || !isdigit(lastc)) {
			extern int dismode;

			--lp;
			/* just print current mode */
			change_dismode(dismode, 1);
		} else {
			change_dismode(lastc-'0', 1);
		}
		break;
#endif
#endif	/* !defined(KADB) */

	case 'z':
		if (hadaddress)
			trapafter = (int)address;
		else
			printf("watchpoint mode is %s\n",
				trapafter? "trap-after" : "trap-before");
		break;

	case 'G':
		toggle_demangling();
		break;

	case 'g':
		if (hadaddress)
			set_demangle_mask(address);
		else
			disp_demangle_mask();
		break;

	default:
		if (ext_dol(modif))
			break;
		db_printf(3, "printrace: bad modifier");
		error("bad modifier");
	}
}



getreg(regnam)
	char regnam;
{
	register int i;
	register char c;
	char *olp = lp;
	char name[30];

	olp = lp;
	i = 0;
	if (isalpha(regnam)) {
		name[i++] = regnam;
		c = readchar();
		while (isalnum(c) && i < 30) {
			name[i++] = c;
			c = readchar();
		}
		name[i] = 0;
		for (i = 0; i < NREGISTERS; i++) {
			if (strcmp(name, regnames[i]) == 0) {
				lp--;
				return (i);
			}
		}
#ifdef sparc
		for (i = 0; i < NALTREGISTERS; i++) {
			if (strcmp(name, altregname[i].name) == 0) {
				lp--;
				return (altregname[i].reg);
			}
		}
#elif defined(__ia64)
		{
			extern int stacked_reg(char *);

			if (i = stacked_reg(name)) {
				lp--;
				return (i);
			}
		}
#endif
	}
	lp = olp;
	return (-1);
}

void
print_dis(int regnum, addr_t pc)
{
#ifdef _LP64
	unsigned int upper;
#endif

	if (pc != 0) {
		dot = pc;
		regnum = -1;
	} else if ((dot = readreg(regnum)) == 0)
		return;
	db_printf(2, "printsym: dot=%J pc=%J", dot, pc);
	psymoff(dot, kernel ? DSYM : ISYM, ":%16t");

#ifndef KADB
	/* Try to avoid an unnecessary read() of the debuggee. */
	if (!(Prstatus.pr_flags & PR_PCINVAL)) {
		struct bkpt *bp = NULL;

		/*
		 * If we're stopped at a breakpoint, the instruction in the
		 * Prstatus will be the trap.
		 */
		if ((unsigned long)Prstatus.pr_lwp.pr_instr ==
				(unsigned long)bpt && (bp = bkptlookup(dot)))
#ifdef	_LP64
		{
			upper = ((bp->ins) >> 32);
			Prstatus.pr_lwp.pr_instr = (unsigned long)upper;
			db_printf(2,
			    "bp->ins %J pr_instr is %J dot is %J instr is %X\n",
			    (unsigned long)bp->ins, Prstatus.pr_lwp.pr_instr,
			    dot, t_srcinstr(chkget(dot, ISP)));
		}
#else
			Prstatus.pr_lwp.pr_instr = (unsigned long) bp->ins;
#endif
		if (bp && (bp->flag == BKPT_TEMP))
			bp->flag = 0;
		printins('i', ISP,
			((regnum == Reg_PC && Prstatus.pr_lwp.pr_instr) ?
			(unsigned int)Prstatus.pr_lwp.pr_instr :
			t_srcinstr(chkget(dot, ISP))));
		printc('\n');
		return;
	}
#endif
	printins('i', ISP, t_srcinstr(chkget(dot, ISP)));
	printc('\n');
}

#ifndef KADB
/*
 * indicate which signals are passed to the subprocess.
 * implements the $i command
 */
static void
whichsigs()
{
	register i;

	for (i = 0; i < NSIG; i++)
		if (sigpass[i]) {
			printf("%R\t", i);
			sigprint(i);
			printf("\n");
		}
}

void
sigprint(signo)
	int signo;
{
	const struct siginfolist *lisp = &_sys_siginfolist[signo-1];
	char *sp, *scp, sbuf[12];

	if (sig2str(signo, sbuf) < 0)
		return;

	prints("SIG");
	prints(sbuf);
	prints(": ");
	prints(strsignal(signo));

	if (signo != 0 && lisp->vsiginfo != NULL &&
	    sigcode > 0 && sigcode <= lisp->nsiginfo &&
	    (scp = lisp->vsiginfo[sigcode-1]) != NULL) {
		prints(" (");
		prints(scp);
		prints(")");
	}
}
#endif !KADB
