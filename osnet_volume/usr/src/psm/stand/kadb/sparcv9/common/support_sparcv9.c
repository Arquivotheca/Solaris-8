/*
 * Copyright (c) 1995,1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)support_sparcv9.c	1.20	99/05/25 SMI"

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
#include <sys/privregs.h>
#include <sys/bootconf.h>
#include <sys/debug/debugger.h>
#include <sys/sysmacros.h>
#include <sys/obpdefs.h>
#include <sys/openprom.h>
#include <stdarg.h>
#include <adb.h>
#include <ptrace.h>
#include <allregs.h>
#include <sys/promif.h>
#include <sys/consdev.h>

int debugkadb = 0;
int switched;
int to_cpu;

extern int cur_cpuid;
extern struct bootops *bootops;
extern int pagesize;

#ifdef sun4u
extern int wp_mask;
#endif

extern int canstep(void);
extern char *map(int);
extern void reload_prom_callback();
extern void switch_cpu(int);
extern void sswait(void);
extern void wp_clrall();
extern void wp_off();
extern void wp_vwrite();
extern void wp_vaccess();
extern void wp_paccess();
extern void fiximp();

extern char *sprintf();		/* standalone lib uses char * */

extern void putchar(int);

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
	(void) prom_enter_mon();
#ifdef sun4u
	(void) reload_prom_callback();
#endif
}

/*
 * Print a character on console.
 */
void
putchar(int c)
{
	if (c == '\n') {
		(void) prom_putchar('\r');
		if (one_line || (adb_more && (++more >= adb_more))) {
			one_line = 0;
			prom_printf("\n--More-- ");
			more = 0;
			c = retrieve_char();
			prom_printf("\r        \r");
			if ((c == 'c') || (c == 'C') || (c == ('c' & 037)))
				dointr(1);
			else if (c == '\r')
				one_line = 1;
		} else
			(void) prom_putchar(c);
	} else
		(void) prom_putchar(c);
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

void *
_sbrk(int incr)
{
	extern char start[], end[];
	static caddr_t lim;
	caddr_t val;
	register int i;

	if (nobrk) {
		printf("sbrk:  late call\n");
		errno = ENOMEM;
		return ((caddr_t)-1);
	}
	if (lim == 0) {
		lim = (caddr_t)roundup((uintptr_t)end, pagesize);
	}
	if (incr == 0)
		return (lim);
	incr = btopr(incr);
	if ((lim + ptob(incr)) >= (caddr_t)((uint_t)start + DEBUGSIZE)) {
		printf("sbrk:  lim %x + %x exceeds %x\n", lim,
		    ptob(incr), (uint_t)start + DEBUGSIZE);
		errno = EINVAL;
		return ((caddr_t)-1);
	}
	if ((val = BOP_ALLOC(bootops, lim, ptob(incr), BO_NO_ALIGN)) != lim) {
		printf("sbrk: BOP_ALLOC failed.\n");
		errno = EINVAL;
		return ((caddr_t)-1);
	}
	pagesused += incr;
	for (i = 0; i < incr; i++, lim += pagesize) {
	}
	return (val);
}

#define	PHYSOFF(p, o)	\
	((physadr)(p)+((o)/sizeof (((physadr)0)->r[0])))

/*
 * Fake ptrace - ignores pid and signals
 * Otherwise it's about the same except the "child" never runs,
 * flags are just set here to control the action in bpwait().
 */
ptrace(int request, int pid, char *addr, int data, char *addr2)
{
	int rv = 0;

	db_printf(5, "ptrace: %s, pid=%X addr=%X, data=%X, addr2=%X sp=%X",
	    map(request), pid, addr, data, addr2, getsp());

	switch (request) {
	case PTRACE_TRACEME:	/* do nothing */
		break;

	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKDATA:
		{		/* start of block */
		int success;
		char *dstaddr;

		rv = 0;
		success = 0;
		rv = peek32((int *)addr);
		db_printf(2, "peek32 returned %X\n", rv);
		success |= (errno == 0);
		if (errno) {
			dstaddr = (char *)&rv;
			*((short *)dstaddr) = peek((short *)addr);
			success |= (errno == 0);
			if (errno) {
				*dstaddr = Peekc(addr);
				success |= (errno == 0);
				*(dstaddr+1) = Peekc(addr+1);
				success |= (errno == 0);
			}
			*((short *)dstaddr+2) = peek((short *)(addr+2));
			success |= (errno == 0);
			if (errno) {
				*(dstaddr+2) = Peekc(addr+2);
				success |= (errno == 0);
				*(dstaddr+3) = Peekc(addr+3);
				success |= (errno == 0);
			}
		}

		if (!success) {
			rv = -1;
			errno = EFAULT;
		}
		}	/* end of block */
		break;

	case PTRACE_PEEKUSER:
		break;

	case PTRACE_POKEUSER:
		break;

	case PTRACE_POKETEXT:
		rv = poketext((int *)addr, data);
		break;

	case PTRACE_POKEDATA:
		{		/* start of block */
		int success;
		char *datap;

		rv = 0;
		success = 0;
		rv = poke32((int *)addr, data);
		db_printf(2, "poke32 returned %X\n", rv);
		success |= (errno == 0);
		if (errno) {
			datap = (char *)&data;
			rv = pokes((short *)addr, *(short *)datap);
			success |= (errno == 0);
			if (errno) {
				rv = pokec(addr, *datap);
				success |= (errno == 0);
				rv = pokec(addr+1, *(datap+1));
				success |= (errno == 0);
			}
			rv = pokes((short *)(addr+2), *(short *)(datap+2));
			success |= (errno == 0);
			if (errno) {
				rv = pokec(addr+2, *(datap+2));
				success |= (errno == 0);
				rv = pokec(addr+3, *(datap+3));
				success |= (errno == 0);
			}
		}

		if (!success) {
			rv = -1;
			errno = EFAULT;
		}
		}	/* end of block */
		break;

	case PTRACE_SINGLESTEP:
		dotrace = 1;
		/* FALLTHROUGH */
	case PTRACE_CONT:
		dorun = 1;
		if ((uintptr_t)addr != 1) {
			writereg(Reg_PC, (ulong_t)addr);
		}
		break;

	case PTRACE_WRITETEXT:
	case PTRACE_WRITEDATA:
		rv = scopy(addr2, addr, data);
		break;

	case PTRACE_READTEXT:
	case PTRACE_READDATA:
		rv = scopy(addr, addr2, data);
		break;

	case PTRACE_WRITEPHYS:
		rv = physmem_write((unsigned long)addr, addr2, (size_t)data);
		break;

	case PTRACE_READPHYS:
		rv = physmem_read((unsigned long) addr, addr2, (size_t)data);
		break;

	/*
	 * Data breakpoints (i.e. watchpoints). Note that only one
	 * virtual and one physical watchpoint are allowed at a time
	 * on Fusion.
	 */
	case PTRACE_CLRBKPT:		/* clear WP */
		if (addr == (char *)0)	/* clear all WP */
			wp_clrall();
		else
			wp_off(addr);
		break;
	case PTRACE_SETWR:		/* WP - write to vaddr */
		wp_vwrite(addr, wp_mask);
		break;
	case PTRACE_SETAC:		/* WP - (r/w) to vaddr */
		wp_vaccess(addr, wp_mask);
		break;
	case PTRACE_WPPHYS:		/* WP - (r/w) to paddr */
		wp_paccess(addr, wp_mask);
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


/*ARGSUSED*/
void
bpwait(int mode)
{
	static char errmsg[60];

	db_printf(2, "bpwait: switched %d dotrace %d\n", switched, dotrace);

	if (switched) {
		switch_cpu(to_cpu);
		switched = 0;
	} else if (dotrace) {
		if (!canstep()) {
			sprintf(errmsg, "Can't step cpu %d", cur_cpuid);
			errflg = errmsg;
			dotrace = 0;
			return;
		}
		sswait();
		dotrace = 0;
	} else
		doswitch();

	signo = 0;
	flushbuf();
	userpc = (addr_t)readreg(Reg_PC);
	db_printf(2, "bpwait: user PC is %X, user NPC is %X",
	    userpc, (addr_t)readreg(Reg_NPC));
	tryabort(0);		/* set interrupt if user is trying to abort */
	ttysync();
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

extern char target_bootname[];
extern char target_bootargs[];
extern char aline[];
extern char *module_path;
extern char start[];
extern char *default_msg;
extern int elf64mode;
ulong_t start_addr = (ulong_t)start;

/*
 * Property intercept routines for kadb, so that it can
 * tell unix its real name, and its real bootargs. We
 * also let it figure out our virtual start and end addresses
 * rather than hardcoding them somewhere nasty.
 */
static int
kadb_getprop(struct bootops *bop, char *name, void *buf)
{
	if (strcmp("whoami", name) == 0) {
		(void) strcpy(buf, aline);
	} else if (strcmp("boot-args", name) == 0) {
		(void) strcpy(buf, target_bootargs);
	} else if (strcmp("debugger-start", name) == 0) {
		caddr32_t tmp_addr;
		tmp_addr = (caddr32_t)start_addr;
		if (elf64mode)
			bcopy(&start_addr, buf, sizeof (caddr_t));
		else
			bcopy(&tmp_addr, buf, sizeof (caddr32_t));
	} else if (strcmp("module-path", name) == 0) {
		(void) strcpy(buf, module_path);
	} else if (strcmp("boot-message", name) == 0) {
		/*
		 * If boot has logged a message for the kernel,
		 * just return that, otherwise check if we need
		 * to return a message if we used the default-file.
		 */
		if (BOP_GETPROPLEN(bop->bsys_super, name) || (!default_msg))
			return (BOP_GETPROP(bop->bsys_super, name, buf));
		(void) strcpy(buf, default_msg);
	} else
		return (BOP_GETPROP(bop->bsys_super, name, buf));
	return (0);
}

static int
kadb_getproplen(struct bootops *bop, char *name)
{
	if (strcmp("whoami", name) == 0) {
		return (strlen(aline) + 1);
	} else if (strcmp("boot-args", name) == 0) {
		return (strlen(target_bootargs) + 1);
	} else if (strcmp("debugger-start", name) == 0) {
		return (sizeof (void *));
	} else if (strcmp("module-path", name) == 0) {
		return (strlen(module_path) + 1);
	} else if (strcmp("boot-message", name) == 0) {
		/*
		 * If boot has logged a message for the kernel,
		 * just return that, otherwise check if we need
		 * to return a message if we used the default-file.
		 */
		if (BOP_GETPROPLEN(bop->bsys_super, name) || (!default_msg))
			return (BOP_GETPROPLEN(bop->bsys_super, name));
		return (strlen(default_msg) + 1);
	} else
		return (BOP_GETPROPLEN(bop->bsys_super, name));
}

int
kadb_1275_call(void *p)
{
	boot_cell_t *args = (boot_cell_t *)p;
	char	*name;
	int	(*bsys_1275_call)(void *);

	name = boot_cell2ptr(args[0]);
	if (strcmp(name, "getprop") == 0) {
		args[5] = boot_int2cell(kadb_getprop(bootops,
		    boot_cell2ptr(args[3]), boot_cell2ptr(args[4])));
		return (BOOT_SVC_OK);
	} else if (strcmp(name, "getproplen") == 0) {
		args[4] = boot_int2cell(kadb_getproplen(bootops,
		    boot_cell2ptr(args[3])));
		return (BOOT_SVC_OK);
	}
	bsys_1275_call = (int (*)(void *))bootops->bsys_super->bsys_1275_call;
	return ((bsys_1275_call)(p));
}

/* ARGSUSED */
void
early_startup(union sunromvec *rump, int shim, struct bootops *buutops)
{
	extern struct bootops *bootops;
	static struct bootops kadb_bootops;
	extern int kadb_1275_wrapper(void *);

	/*
	 * Save parameters from boot in obvious globals, and set
	 * up the bootops to intercept property look-ups.
	 */
	bootops = buutops;

	prom_init("kadb", rump);
	if (debugkadb)
		(void) prom_enter_mon();

	if (BOP_GETVERSION(bootops) != BO_VERSION) {
		prom_printf("WARNING: %d != %d => %s\n",
		    BOP_GETVERSION(bootops), BO_VERSION,
		    "mismatched version of /boot interface.");
	}

	bcopy((caddr_t)bootops, (caddr_t)&kadb_bootops,
	    sizeof (struct bootops));

	kadb_bootops.bsys_super = bootops;
	kadb_bootops.bsys_1275_call = (uint64_t)kadb_1275_wrapper;

	bootops = &kadb_bootops;

	(void) fiximp();
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
	int rv;
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
