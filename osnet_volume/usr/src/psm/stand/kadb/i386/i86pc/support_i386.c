/*
 * Copyright (c) 1987-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)support_i386.c	1.25	99/08/19 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/user.h>
#include <time.h>
#include <tzfile.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/reg.h>
#include <sys/bootconf.h>
#include <sys/debug/debugger.h>
#include <sys/debug/debug.h>
#include "ptrace.h"
#include <sys/sysmacros.h>
#include <sys/debugreg.h>
#ifdef	__STDC__
#include <stdarg.h>
#else	/* __STDC__ */
#include <varargs.h>
#endif	/* __STDC__ */
#define	addr_t	unsigned
#include "process.h"

extern struct bootops *bootops;
extern int pagesize;

extern void putchar(int);
extern int printf(const char *, ...);
static void polled_putchar(int);
static void polled_putstr(char *);

int adb_more = 0;
static int more = 0;
static int one_line = 0;
int interrupted = 0;
extern cons_polledio_t polled_io;

int may_retrieve_char(void);
uchar_t retrieve_char(void);

void
dointr(doit)
{

	putchar('^');
	putchar('C');
	interrupted = 1;
	if (abort_jmp && doit) {
		_longjmp(abort_jmp, 1);
		/*NOTREACHED*/
	}
}

void
_exit(void)
{
	int c;

	printf("Type 'y' if you really want to reboot.  ");
	c = retrieve_char();
	if (c == 'y' || c == 'Y') {
		putchar(c);
		putchar('\n');
		printf("rebooting...\n");
		pc_reset();
	}
	printf("n\n");
}

/*
 * Print a character on console.
 */
void
putchar(int c)
{
	if (c == '\n') {
		(void) polled_putchar('\r');
		if (one_line || (adb_more && (++more >= adb_more))) {
			one_line = 0;
			polled_putstr("\n--More-- ");
			more = 0;
			c = retrieve_char();
			polled_putstr("\r        \r");
			if ((c == 'c') || (c == 'C') || (c == ('c' & 037)))
				dointr(1);
			else if (c == '\r')
				one_line = 1;
		} else
			(void) polled_putchar(c);
	} else
		(void) polled_putchar(c);
}

getchar()
{
	register int c;

	one_line = 0;
	while ((c = may_retrieve_char()) == -1)
		;
	if (c == '\r')
		c = '\n';
	if (c == 0177 || c == '\b') {
		putchar('\b');
		putchar(' ');
		c = '\b';
	}
	putchar(c);
	more = 0;
	return (c);
}

/*
 * Read a line into the given buffer and handles
 * erase (^H or DEL), kill (^U), and interrupt (^C) characters.
 * This routine ASSUMES a maximum input line size of LINEBUFSZ
 * to guard against overflow of the buffer from obnoxious users.
 * gets_p is same as gets but assumes there is a null terminated
 * primed string in buf
 */
gets_p(buf)
	char buf[];
{
	register char *lp = buf;
	register c;
	while (*lp) {
		putchar(*lp);
		lp++;
	}
	for (;;) {
		c = getchar() & 0177;
		switch (c) {
		case '[':
		case ']':
			if (lp != buf)
				goto defchar;
			putchar('\n');
			*lp++ = c;
			/* FALLTHROUGH */
		case '\n':
		case '\r':
			*lp++ = '\0';
			return;
		case '\b':
			lp--;
			if (lp < buf)
				lp = buf;
			continue;
		case 'u'&037:			/* ^U */
			lp = buf;
			putchar('^');
			putchar('U');
			putchar('\n');
			continue;
		case 'c'&037:
			dointr(1);
			/* MAYBE REACHED */
			/* fall through */
		default:
		defchar:
			if (lp < &buf[LINEBUFSZ-1]) {
				*lp++ = c;
			} else {
				putchar('\b');
				putchar(' ');
				putchar('\b');
			}
			break;
		}
	}
}

/*
 * Check for ^C on input
 */
void
tryabort(doit)
{

	if (may_retrieve_char() == ('c' & 037)) {
		dointr(doit);
		/* MAYBE REACHED */
	}
}

/*
 * Implement pseudo ^S/^Q processing along w/ handling ^C
 * We need to strip off high order bits as monitor cannot
 * reliably figure out if the control key is depressed when
 * may_retrieve_char() is called in certain circumstances.
 * Unfortunately, this means that s/q will work as well
 * as ^S/^Q and c as well as ^C when this guy is called.
 */
void
trypause()
{
	register int c;

	c = may_retrieve_char() & 037;

	if (c == ('s' & 037)) {
		while ((c = may_retrieve_char() & 037) != ('q' & 037)) {
			if (c == ('c' & 037)) {
				dointr(1);
				/* MAYBE REACHED */
			}
		}
	} else if (c == ('c' & 037)) {
		dointr(1);
		/* MAYBE REACHED */
	}
}

/*
 * Printn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 */
void
printn(ulong_t n, int b)
{
	char prbuf[11];
	register char *cp;

	if (b == 10 && (long)n < 0) {
		putchar('-');
		n = (unsigned)(-(long)n);
	}
	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
	} while (n);
	do
		putchar(*--cp);
	while (cp > prbuf);
}

/*
 * Scaled down version of C Library printf.
 */

void
prf(const char *fmt, va_list adx)
{
	register int b, c;
	register char *s;

loop:
	while ((c = *fmt++) != '%') {
	if (c == '\0')
			return;
		putchar(c);
	}
again:
	c = *fmt++;
	switch (c) {

	case 'l':
		goto again;
	case 'x': case 'X':
		b = 16;
		goto number;
	case 'd': case 'D':
	case 'u':		/* what a joke */
		b = 10;
		goto number;
	case 'o': case 'O':
		b = 8;
number:
		printn(va_arg(adx, ulong_t), b);
		break;
	case 'c':
		b = va_arg(adx, int);
		putchar(b);
		break;
	case 's':
		s = va_arg(adx, char *);
		while (c = *s++)
			putchar(c);
		break;
	}
	goto loop;
}


/*VARARGS1*/
printf(const char *fmt, ...)
{
	va_list x1;

	tryabort(1);
	va_start(x1, fmt);
	prf(fmt, x1);
	va_end(x1);
	return (0);
}

/*
 * Fake getpagesize() system call
 */
getpagesize()
{

	return (pagesize);
}

/*
 * Fake gettimeofday call
 * Needed for ctime - we are lazy and just
 * give a bogus approximate answer
 */
void
gettimeofday(struct timeval *tp, struct timezone *tzp)
{

	tp->tv_sec = (1989 - 1970) * 365 * 24 * 60 * 60;	/* ~1989 */
	tzp->tz_minuteswest = 8 * 60;	/* PDT: California ueber alles */
	tzp->tz_dsttime = DST_USA;
}

int errno;

caddr_t lim;	/* current hard limit (high water) */
caddr_t curbrk;	/* current break value */

void *
_sbrk(int incr)
{
/* curbrk and lim are usually the same value; they will only differ */
/* in cases where memory has been freed. In other words, the difference */
/* between lim and curbrk is our own private pool of available memory. */
/* Use this up before calling _sbrk again! */

	extern char end[];
	int pgincr;	/* #pages requested, rounded to next highest page */
	int nreq;	/* #bytes requested, rounded to next highest page */
	caddr_t val;

	if (nobrk) {	/* safety indicator - prevents recursive sbrk's */
		printf("sbrk:  late call\n");
		errno = ENOMEM;
		return ((caddr_t)-1);
	}
	if (lim == 0) {			/* initial _sbrk call */
		lim = (caddr_t)roundup((uintptr_t)end, pagesize);
		curbrk = lim;
	}
	if (incr == 0)
		return (curbrk);
	pgincr = btopr(incr);
	if ((curbrk + ptob(pgincr)) < (caddr_t)(end)) {
		printf("sbrk:  lim %x + %x "
		    "attempting to free program space %x\n",
		    lim, ptob(pgincr), (u_int)end);
		errno = EINVAL;
		return ((caddr_t)-1);
	}

	if ((curbrk + ptob(pgincr)) <= lim) {	/* have enough mem avail */
		return (curbrk += incr);
	} else {			 /* beyond lim - more pages needed */
		nreq = (roundup(incr - (lim - curbrk), pagesize));
		if (prom_alloc(0, nreq, 0) == 0) {
			errno = ENOMEM;
			return ((caddr_t)-1);
		}
		pagesused += pgincr;
		lim += incr;
	}
	val = curbrk;
	curbrk += incr;
	return (val);
}

#define	PHYSOFF(p, o)	\
	((physadr)(p)+((o)/sizeof (((physadr)0)->r[0])))

dbregset_t  dr_registers;
extern uchar_t const dbreg_control_enable_to_bkpts_table[256];
extern int cur_cpuid; /* cpu currently running kadb */

/* Given a data breakpoint mask, compute next available breakpoint */
/* Pick from 3 to 0, allowing user processes to use 0 to 3 */

static int  dr_nextbkpt[16] = {
	3,  /* 0x0: 3 clr, 2 clr, 1 clr, 0 clr */
	3,  /* 0x1: 3 clr, 2 clr, 1 clr, 0 set */
	3,  /* 0x2: 3 clr, 2 clr, 1 set, 0 clr */
	3,  /* 0x3: 3 clr, 2 clr, 1 set, 0 set */
	3,  /* 0x4: 3 clr, 2 set, 1 clr, 0 clr */
	3,  /* 0x5: 3 clr, 2 set, 1 clr, 0 set */
	3,  /* 0x6: 3 clr, 2 set, 1 set, 0 clr */
	3,  /* 0x7: 3 clr, 2 set, 1 set, 0 set */
	2,  /* 0x8: 3 set, 2 clr, 1 clr, 0 clr */
	2,  /* 0x9: 3 set, 2 clr, 1 clr, 0 set */
	2,  /* 0xa: 3 set, 2 clr, 1 set, 0 clr */
	2,  /* 0xb: 3 set, 2 clr, 1 set, 0 set */
	1,  /* 0xc: 3 set, 2 set, 1 clr, 0 clr */
	1,  /* 0xd: 3 set, 2 set, 1 clr, 0 set */
	0,  /* 0xe: 3 set, 2 set, 1 set, 0 clr */
	-1, /* 0xf: 3 set, 2 set, 1 set, 0 set */
};

/*
 * Fake ptrace - ignores pid and signals
 * Otherwise it's about the same except the "child" never runs,
 * flags are just set here to control action elsewhere.
 */
/* ARGSUSED */
ptrace(int request, int pid, char *addr, int data, char *addr2)
{
	int rv = 0;
	register int i;

	switch (request) {
	case PTRACE_TRACEME:	/* do nothing */
		break;

	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKDATA:
		rv = peekl(addr);
		break;

	case PTRACE_PEEKUSER:
		break;

	case PTRACE_POKEUSER:
		break;

	case PTRACE_POKETEXT:
		rv = poketext(addr, data);
		break;

	case PTRACE_POKEDATA:
		rv = pokel(addr, data);
		break;

	case PTRACE_SINGLESTEP:
		dotrace = 1;
		/* FALLTHROUGH */
	case PTRACE_CONT:
		dorun = 1;
		if ((uintptr_t)addr != 1) {
			reg->r_pc = (int)addr;
		}
		break;

	case PTRACE_WRITETEXT:
	case PTRACE_WRITEDATA:
	case PTRACE_WRITEPHYS:
		rv = wcopy(addr2, addr, data);
		break;

	case PTRACE_READTEXT:
	case PTRACE_READDATA:
	case PTRACE_READPHYS:
		rv = rcopy(addr, addr2, data);
		break;

/* hardware breakpoints */
	case PTRACE_CLRBKPT:		/* clear WP */
		dr_registers.debugreg[0] = 0;
		dr_registers.debugreg[1] = 0;
		dr_registers.debugreg[2] = 0;
		dr_registers.debugreg[3] = 0;
		dr_registers.debugreg[DR_CONTROL] = 0;

		clear_kadb_debug_registers(cur_cpuid);
		break;

	case PTRACE_SETWR:		/* WP - write to vaddr */
		i = dr_nextbkpt[dbreg_control_enable_to_bkpts_table
		    [(uchar_t)dr_registers.debugreg[DR_CONTROL]]];
		if (i < 0)
			error("hardware break point over flow on write");

		dr_registers.debugreg[DR_CONTROL] &= ~
		    DR_ENABLE0 << (i * DR_ENABLE_SIZE);
		dr_registers.debugreg[DR_CONTROL] &= ~
		    (DR_RW_READ | DR_LEN_MASK) <<
		    (DR_CONTROL_SHIFT + i * DR_CONTROL_SIZE);
		dr_registers.debugreg[i] = (int)addr;
		dr_registers.debugreg[DR_CONTROL] |=
		    (DR_RW_WRITE | ((data - 1) << 2)) <<
		    (DR_CONTROL_SHIFT + i * DR_CONTROL_SIZE);
		dr_registers.debugreg[DR_CONTROL] |=
		    DR_ENABLE0 << (i * DR_ENABLE_SIZE);

		set_kadb_debug_registers(cur_cpuid);
		break;

	case PTRACE_SETAC:		/* WP - (r/w) to vaddr */
		i = dr_nextbkpt[dbreg_control_enable_to_bkpts_table
		    [(uchar_t)dr_registers.debugreg[DR_CONTROL]]];
		if (i < 0)
			error("hardware break point over flow on access");

		dr_registers.debugreg[DR_CONTROL] &= ~
		    DR_ENABLE0 << (i * DR_ENABLE_SIZE);
		dr_registers.debugreg[DR_CONTROL] &= ~
		    (DR_RW_READ | DR_LEN_MASK) <<
		    (DR_CONTROL_SHIFT + i * DR_CONTROL_SIZE);
		dr_registers.debugreg[i] = (int)addr;
		dr_registers.debugreg[DR_CONTROL] |=
		    (DR_RW_READ | ((data - 1) << 2)) <<
		    (DR_CONTROL_SHIFT + i * DR_CONTROL_SIZE);
		dr_registers.debugreg[DR_CONTROL] |=
		    DR_ENABLE0 << (i * DR_ENABLE_SIZE);

		set_kadb_debug_registers(cur_cpuid);
		break;

	case PTRACE_SETREGS:
		rv = scopy(addr, (caddr_t)reg, sizeof (struct regs));
		break;

	case PTRACE_GETREGS:
		rv = scopy((caddr_t)reg, addr, sizeof (struct regs));
		break;

	case PTRACE_SETBPP:
		i = dr_nextbkpt[dbreg_control_enable_to_bkpts_table
		    [(uchar_t)dr_registers.debugreg[DR_CONTROL]]];
		if (i < 0)
			error("hardware break point over flow on instruction");

		dr_registers.debugreg[DR_CONTROL] &= ~
		    DR_ENABLE0 << (i * DR_ENABLE_SIZE);
		dr_registers.debugreg[DR_CONTROL] &= ~
		    (DR_RW_READ | DR_LEN_MASK) <<
		    (DR_CONTROL_SHIFT + i * DR_CONTROL_SIZE);
		dr_registers.debugreg[i] = (int)addr;
		dr_registers.debugreg[DR_CONTROL] |=
		    DR_RW_EXECUTE << (DR_CONTROL_SHIFT + i * DR_CONTROL_SIZE);
		dr_registers.debugreg[DR_CONTROL] |=
		    DR_ENABLE0 << (i * DR_ENABLE_SIZE);

		set_kadb_debug_registers(cur_cpuid);
		break;

	case PTRACE_X86IO:
		i = dr_nextbkpt[dbreg_control_enable_to_bkpts_table
		    [(uchar_t)dr_registers.debugreg[DR_CONTROL]]];
		if (i < 0)
			error("hardware break point over flow on I/O");

		dr_registers.debugreg[DR_CONTROL] &= ~
		    DR_ENABLE0 << (i * DR_ENABLE_SIZE);
		dr_registers.debugreg[DR_CONTROL] &= ~
		    (DR_RW_READ | DR_LEN_MASK) <<
		    (DR_CONTROL_SHIFT + i * DR_CONTROL_SIZE);
		dr_registers.debugreg[i] = (int)addr;
		dr_registers.debugreg[DR_CONTROL] |=
		    (DR_RW_IO_RW | ((data - 1) << 2)) <<
		    (DR_CONTROL_SHIFT + i * DR_CONTROL_SIZE);
		dr_registers.debugreg[DR_CONTROL] |=
		    DR_ENABLE0 << (i * DR_ENABLE_SIZE);

		set_kadb_debug_registers(cur_cpuid);
		break;

	case PTRACE_KILL:
	case PTRACE_ATTACH:
	case PTRACE_DETACH:
	default:
		errno = EINVAL;
		rv = -1;
		break;
	}
	return (rv);
}

/*
 * Return the ptr in sp at which the character c appears;
 * NULL if not found
 */


char *
index(char *sp, char c)
{

	do {
		if (*sp == c)
			return (sp);
	} while (*sp++);
	return (NULL);
}

/*
 * Return the ptr in sp at which the character c last
 * appears; NULL if not found
 */

char *
rindex(char *sp, char c)
{
	register char *r;

	r = NULL;
	do {
		if (*sp == c)
			r = sp;
	} while (*sp++);
	return (r);
}

extern char *module_path;

/*
 * Property intercept routines for kadb.
 */
static int
kadb_getprop(struct bootops *bop, char *name, void *buf)
{
	if (strcmp("module-path", name) == 0) {
		(void) strcpy(buf, module_path);
		return (strlen(module_path) + 1);
	} else {
		return (BOP_GETPROP(bop->bsys_super, name, buf));
	}
}

static int
kadb_getproplen(struct bootops *bop, char *name)
{
	if (strcmp("module-path", name) == 0) {
		return (strlen(module_path) + 1);
	} else {
		return (BOP_GETPROPLEN(bop->bsys_super, name));
	}
}

void
init_bootops(struct bootops *buutops)
{
	extern struct bootops *bootops;
	static struct bootops kadb_bootops;

	/*
	 * Save parameters from boot in obvious globals, and set
	 * up the bootops to intercept property look-ups.
	 */
	bootops = buutops;

	bcopy((caddr_t)bootops, (caddr_t)&kadb_bootops,
	    sizeof (struct bootops));

	kadb_bootops.bsys_super = bootops;
	kadb_bootops.bsys_getprop = kadb_getprop;
	kadb_bootops.bsys_getproplen = kadb_getproplen;

	bootops = &kadb_bootops;
}

/*
 * may_retrieve_char:
 *	Return -1 if there are no characters available, else return
 *	a character.
 */
int
may_retrieve_char(void)
{
	char c;
	unsigned long   (*func)();
	unsigned long   args[1];

	/*
	 * If the kernel hasn't registered a callback, call into the
	 * promif library.
	 */
	if (polled_io.cons_polledio_ischar == NULL) {
		return (prom_mayget());
	} else {

		/*
		 * The ischar callback is non blocking.
		 * It returns TRUE if a character
		 * is available.  False otherwise.
		 *
		 * The kernel_invoke function is used because
		 * it will adjust the stack pointer, stack frame,
		 * etc. if the kernel is running in 32 bit mode.
		 */
		func = (unsigned long(*)())polled_io.cons_polledio_ischar;
		args[0] = (unsigned long) polled_io.cons_polledio_argument;

		if (kernel_invoke(func, 1, args)) {

			/*
			 * If the character is available,
			 * the getchar callback will return it.
			 */
			func = (unsigned long(*)())
				polled_io.cons_polledio_getchar;
			args[0] = (unsigned long)
				polled_io.cons_polledio_argument;

			c = kernel_invoke(func, 1, args);

			return (c);

		} else {

			return (-1);
		}
	}
}

/*
 * retrieve_char
 *	Retrieve a character. This function waits
 *	until a character is available.
 */
uchar_t
retrieve_char(void)
{
	int c;

	/*
	 * Block until a character is available
	 */
	while ((c = may_retrieve_char()) == -1)
		;

	return ((uchar_t)c);
}

static void
polled_putchar(int c)
{
	unsigned long   (*func)();
	unsigned long   args[2];

	/*
	 * If the kernel hasn't registered a callback, call into the
	 * promif library.
	 */
	if (polled_io.cons_polledio_putchar == NULL) {
		prom_putchar(c);
	} else {

		/*
		 * The kernel_invoke function is used because
		 * it will adjust the stack pointer, stack frame,
		 * etc. if the kernel is running in 32 bit mode.
		 */
		func = (unsigned long(*)())polled_io.cons_polledio_putchar;
		args[0] = (unsigned long) polled_io.cons_polledio_argument;
		args[1] = c;

		(void) kernel_invoke(func, 2, args);
	}
}

static void
polled_putstr(char *str)
{
	while (*str != '\0')
		polled_putchar(*str++);
}
