/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)debug.c	1.4	99/10/08 SMI"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/reg.h>
#include <sys/psw.h>
#include <sys/trap.h>
#include <sys/debugreg.h>
#include <sys/salib.h>
#include <setjmp.h>
#include <sys/bootvfs.h>
#include <sys/link.h>
#include <sys/auxv.h>
#include <sys/bootlink.h>
#include <sys/dosemul.h>

extern short DOSsnarf_flag;

extern void enter_debug(unsigned long);
extern void get_cregs();
extern struct boot_fs_ops *get_default_fs(void);
extern void get_dregs();
extern void set_dregs();
extern int getchar();
extern uint_t p0_setpte(uint_t bpte);
extern void post_mountroot(char *, char *);
extern void putchar();

struct syminfo {
	char *name;
	ulong offset;
};

struct syminfo *symtab;
ulong symtab_size;
int num_syms;
ulong kernel_entry;
char *kernel_name = "kernel";
int debug_used = 0;
char *default_symbol_file = "/boot/solaris/bootbin.nm";
char *requested_symbol_file;

int debug_symtab(char *, int);
static ulong find_kernel_entry(ulong);
int get_syminfo(char **, char **, ulong *);
void pm_dump_regs(ulong *);
void symbol_find(ulong, struct syminfo *);


/*
 * Try to parse a hex ASCII string into a number.  Return 1 for success,
 * 0 for failure.  First argument point to the string pointer and is
 * updated to point to the character following the string.  Second argument
 * is where to store the number.
 */
static int
get_hex(char **str, ulong *valp)
{
	ulong answer = 0;
	char *p;

	for (p = *str; *p; p++) {
		if (*p >= '0' && *p <= '9')
			answer = (answer << 4) + *p - '0';
		else if (*p >= 'a' && *p <= 'f')
			answer = (answer << 4) + *p - 'a' + 10;
		else if (*p >= 'A' && *p <= 'F')
			answer = (answer << 4) + *p - 'A' + 10;
		else
			break;
	}
	if (p != *str) {
		*str = p;
		*valp = answer;
		return (1);
	}
	return (0);
}

/*
 * Try to convert 'length' chars at 'symbol' into a numeric address.
 * Input string need not be null-terminated.
 */
int
symbol_value(char *symbol, int length, ulong *result)
{
	ulong index;

	for (index = 0; index < num_syms; index++) {
		if (strncmp(symbol, symtab[index].name, length) == 0 &&
				symtab[index].name[length] == 0) {
			*result = symtab[index].offset;
			return (1);
		}
	}
	return (0);
}

int
get_symval(char **str, ulong *addr)
{
	char *p = *str;
	char *pe;

	for (pe = p; *pe && *pe != ' ' && *pe != '\t' && *pe != '+'; pe++)
		continue;

	if (symbol_value(p, pe - p, addr)) {
		*str = pe;
		return (1);
	}
	return (0);
}


/*
 * Try to read an address from *str, return the addr and update *str.
 * Address can be hex, a symbol or symbol+hexval.
 */
int
get_addr(char **str, ulong *addr)
{
	char *p = *str;
	ulong val;
	ulong val2;

	*addr = 0;
	if (get_symval(&p, &val)) {
		*addr = val;
		if (*p != '+') {
			/* string was just a symbol */
			*str = p;
			return (1);
		}
		p++;
	}
	/*
	 * There was no symbol, or we already handled "symbol+".  Look
	 * for a hex number or segment:offset pair followed by a terminater.
	 */
	if (get_hex(&p, &val)) {
		if (*p == ':') {
			p++;
			if (get_hex(&p, &val2) == 0) {
				return (0);
			}
			val *= 16;
			val += val2;
		}
		if (*p == 0 || *p == ' ' || *p == '\t') {
			*addr += val;
			*str = p;
			return (1);
		}
	}
	/* Syntax error */
	return (0);
}

/*
 * Look up the address to be printed in the symbol table.  If it matches
 * a symbol, print just the symbol name.  Otherwise try to print it as
 * symbol+offset.  Failing that just print the hex address.  In all cases
 * print the other args before and afterwards.
 */
void
print_addr(char *before, ulong addr, char *after)
{
	struct syminfo s;

	symbol_find(addr, &s);
	if (s.name && s.offset)
		printf("%s%s+%lx%s", before, s.name, s.offset, after);
	else if (s.name)
		printf("%s%s%s", before, s.name, after);
	else
		printf("%s%lx%s", before, s.offset, after);
}

/*
 * db_gets reads and echoes an input string terminated by CR or NL
 * and returns the string without the terminator.  It has one special
 * debugger feature: it returns immediately if the first character
 * is ']'.
 */
void
db_gets(char *buf, int n)
{
	char *p;
	char c;

	p = buf;
	while ((c = getchar()) != '\r' && c != '\n') {
		if (c == '\b') {
			if (p > buf) {
				putchar(c);
				putchar(' ');
				putchar(c);
				p--;
			}
			continue;
		}
		putchar(c);
		if (p < buf + n) {
			*p++ = c;
		}
		if (buf[0] == ']')
			break;
	}
	printf("\n");

	if (p < buf + n)
		*p = 0;
	else
		buf[n - 1] = 0;
}

/* parse 'p' into up to 'count' null-terminated words */
int
db_parse(char *p, int count, char **args)
{
	int i;

	for (i = 0; i < count; i++) {
		while (*p == ' ' || *p == '\t')
			p++;

		if (*p == 0)
			break;

		args[i] = p;

		while (*p && *p != ' ' && *p != '\t')
			p++;

		if (*p)
			*p++ = 0;
	}
	return (i);
}

void
pm_dump_regs(ulong *regs)
{
	/*
	 * Assume for now that entry was from same
	 * protection level.  So there was no USS, UESP
	 * pushed and old ESP points to where UESP would
	 * have been in the register set.
	 */
	printf("eax: %lx, ebx: %lx, ecx: %lx, edx: %lx\n",
		regs[EAX], regs[EBX], regs[ECX], regs[EDX]);
	printf("esi: %lx, edi: %lx, ebp: %lx, esp: %lx\n",
		regs[ESI], regs[EDI], regs[EBP], (ulong)&regs[UESP]);
	printf("cs: %lx, eip: %lx, ds: %lx, es: %lx\n",
		regs[CS], regs[EIP], regs[DS], regs[ES]);
}

void
pm_dump_realregs(struct real_regs *rp)
{
	printf("ax: %x, bx: %x, cx: %x, dx: %x\n",
		AX(rp), BX(rp), CX(rp), DX(rp));
	printf("si: %x, di: %x, bp: %x, sp: %x\n",
		SI(rp), DI(rp), BP(rp), rp->esp.word.sp);
	printf("cs: %x, ip: %x, efl: %lx, ds: %x, es: %x\n",
		rp->cs, rp->ip, rp->eflags, rp->ds, rp->es);
}

void
pm_debug_usage()
{
	printf("Debugger commands are:\n"
		"]\t\tsingle step\n"
		"@ pathname\tread symbol table file\n"
		"b addr\t\tset breakpoint at address\n"
		"c\t\tcontinue execution\n"
		"d addr\t\tdump 16 bytes at address\n"
		"d\t\tdump next 16 bytes\n"
		"h\t\tprint this help list\n"
		"r\t\tprint the CPU registers\n"
		"s\t\tprint stack backtrace\n"
		"u\t\tgo to return address\n"
		"w addr val\twrite 4-byte value to address\n"
		"x\t\tprint the realmode CPU registers\n"
		"z\t\tclear breakpoint\n");
}

static void
pm_debug(ulong *regs, struct real_regs *rp)
{
	int more = 1;
	char inbuf[80];
#define	DB_ARGS	3
	char *args[DB_ARGS];
	int argc;
	char *p;
	ulong val;
	ulong p0_save;
	ulong *vp;
	ulong drs[8];
	ulong debug_status;
	short save_snarfflag;
	static nested;
	static jmp_buf env;

	/*
	 * Take a snapshot of the debug registers.  Adjust the debug
	 * parameters by changing these registers before they are
	 * restored on debugger exit.
	 *
	 * Save and clear debug status register so that nested entry
	 * gives correct state.
	 *
	 * Save and clear control register, with restore on exit, so
	 * that breakpoints in routines used by the debugger (especially
	 * printf) do not occur while debugging.
	 */
	get_dregs(drs);		/* get current debug state */
	debug_status = drs[DR_STATUS];	/* save debug status */
	drs[DR_STATUS] = 0;	/* clear debug status */
	val = drs[DR_CONTROL];	/* save debug control state */
	drs[DR_CONTROL] = 0;	/* prevent breakpoints while in debugger */
	set_dregs(drs);
	drs[DR_CONTROL] = val;	/* prepare to restore on debugger exit */

	if (nested) {
		longjmp(env, 0);
	}
	nested++;

	/*
	 * Make sure debugger output appears on the console even if
	 * debugger entry occurred during bootops and output was
	 * redirected.
	 */
	save_snarfflag = DOSsnarf_flag;
	DOSsnarf_flag = 0;

	/* Allow page 0 access from debugger */
	p0_save = p0_setpte(3);

	/*
	 * No entry message for single step interrupt because it
	 * should be an immediate response to the request.  Same
	 * for result of 'u' command.
	 *
	 * Otherwise report a breakpoint or give a generic message.
	 *
	 * Note that entry from enter_debug gives the generic message
	 * because that uses 'int 1' instruction.
	 */
	if (debug_status & DR_SINGLESTEP) {
		regs[EFL] &= ~PS_T;
	} else if (debug_status & DR_TRAP0) {
		/* Debug reg 0 is used for breakpoints */
		regs[EFL] |= PS_RF;	/* Prevent breakpoint on resume */
		printf("Breakpoint reached:\n");
	} else if (debug_status & DR_TRAP1) {
		/* Debug reg 1 is used for 'u' command */
		drs[DR_CONTROL] &= ~DR_ENABLE1;
	} else if ((debug_status & DR_TRAP2) && regs[EIP] == kernel_entry) {
		/*
		 * Debug reg 2 is used for catching kernel entry.
		 * Disable any breakpoints because boot.bin breakpoints
		 * cause a reboot once the kernel has set up its IDT.
		 * We do not prevent the user from setting new breakpoints
		 * in case it proves useful, but for most purposes any
		 * debugging beyond here should use kadb.
		 */
		printf("Starting %s%s:\n", kernel_name,
			(drs[DR_CONTROL] & DR_ENABLE0) ?
			".  boot.bin breakpoints are now disabled" : "");
		drs[DR_CONTROL] = 0;
	} else {
		printf("Entering boot debugger:\n");
	}

	if (setjmp(env)) {
		printf("Restarting debugger:\n");
	}

	debug_used = 1;

	while (more) {
		print_addr("[", regs[EIP], "]: ");

		db_gets(inbuf, sizeof (inbuf));

		argc = db_parse(inbuf, DB_ARGS, args);

		if (argc == 0 || args[0][1] != 0) {
			pm_debug_usage();
			continue;
		}

		/*
		 * The following parsing is not rigorous.  Commands
		 * that do not take arguments typically ignore any.
		 *
		 * Before wasting much time improving what is here,
		 * it might be worth reworking the syntax to be more
		 * kadb-like.
		 */
		switch (args[0][0]) {
		case ']':
			regs[EFL] |= PS_T;
			more = 0;
			break;
		case '@':
			if (argc != 2) {
				pm_debug_usage();
				continue;
			}
			if (get_default_fs() != 0) {
				/* Root is mounted.  Handle now */
				(void) debug_symtab(args[1], 0);
				break;
			}
			/* Use the requested file later */
			printf("Cannot read symbol table now because root "
				"is not mounted.\n");
			printf("Symbols will be read from file \"%s\" "
				"after root mount.\n", args[1]);

			requested_symbol_file =
				bkmem_alloc(strlen(args[1]) + 1);
			(void) strcpy(requested_symbol_file, args[1]);
			break;
		case 'b':
			if (argc != 2) {
				pm_debug_usage();
				continue;
			}
			p = args[1];
			if (get_addr(&p, &val) == 0) {
				printf("Unknown address: \"%s\"\n", args[1]);
				break;
			}
			/* For now we always use register 0 */
			drs[0] = val;
			drs[DR_CONTROL] |= DR_ENABLE0;

			/* Do not break immediately if already there */
			regs[EFL] |= PS_RF;
			break;
		case 'c':
			if (argc != 1) {
				pm_debug_usage();
				continue;
			}
			more = 0;
			break;
		case 'd':
			/*
			 * Display 16 bytes from specified address in both
			 * hex and printable character format.  If address
			 * is omitted, display the next 16 bytes after a
			 * previous dump.
			 */
			{
				char chrs[17];
				char *cp;
				int i;
				static ulong last_addr = 0;

				if (argc == 1) {
					val = last_addr + 16;
				} else if (argc != 2) {
					pm_debug_usage();
					continue;
				} else {
					p = args[1];
					if (get_addr(&p, &val) == 0) {
						printf("Unknown address: "
							"\"%s\"\n", args[1]);
						continue;
					}
				}
				last_addr = val;
				vp = (ulong *)val;
				cp = (char *)vp;
				for (i = 0; i < 16; i++) {
					if (cp[i] >= ' ' && cp[i] < 0x7f)
						chrs[i] = cp[i];
					else
						chrs[i] = '.';
				}
				chrs[i] = 0;
				printf("%lx: %lx %lx %lx %lx  %s\n",
					(ulong)vp, vp[0],
					vp[1], vp[2], vp[3], chrs);
			}
			break;
		case 'h': case '?':
			pm_debug_usage();
			break;
		case 'r':
			if (argc != 1) {
				pm_debug_usage();
				continue;
			}
			pm_dump_regs(regs);
			break;
		case 's':
			if (argc != 1) {
				pm_debug_usage();
				continue;
			}
			vp = (ulong *)regs[EBP];
			val = regs[EIP];
			while (vp != 0) {
				print_addr("", val, "");
				printf("(%lx, %lx, %lx) EBP = %lx\n",
				    vp[2], vp[3], vp[4], (ulong)vp);
				val = vp[1];
				if (vp[0] > (ulong)vp)
					vp = (ulong *)vp[0];
				else
					break;
			}
			break;
		case 'u':
			/*
			 * Set temporary breakpoint at return from present
			 * routine.
			 */
			if (argc != 1) {
				pm_debug_usage();
				continue;
			}
			vp = (ulong *)regs[EBP];
			/* For now we always use register 1 */
			drs[1] = vp[1];	/* Return address */
			drs[DR_CONTROL] |= DR_ENABLE1;
			more = 0;
			break;
		case 'w':
			if (argc != 3) {
				pm_debug_usage();
				continue;
			}
			p = args[1];
			if (get_addr(&p, (ulong *)&vp) == 0) {
				printf("Unknown address: \"%s\"\n", args[1]);
				break;
			}
			p = args[2];
			if (get_hex(&p, &val) == 0) {
				printf("\"%s\" is not a hex number\n", args[2]);
				break;
			}
			*vp = val;
			break;
		case 'x':
			if (argc != 1) {
				pm_debug_usage();
				continue;
			}
			pm_dump_realregs(rp);
			break;
		case 'z':
			if (argc != 1) {
				pm_debug_usage();
				continue;
			}
			drs[DR_CONTROL] &= ~DR_ENABLE0;
			break;
		default:
			pm_debug_usage();
			break;
		}
	}

	/* Restore previous page 0 protection */
	(void) p0_setpte(p0_save);

	/* Restore any output redirection on exit */
	DOSsnarf_flag = save_snarfflag;

	/* Restore debug state plus any changes */
	set_dregs(drs);

	nested = 0;
}

/*
 * debug_init() is called from bsetup() with symbol_check = 0 if
 * compiled for debugger early entry.  It is too early to read symbols.
 * It is always called from post_mountroot() where it uses the presence
 * of the symbol table file (even if empty) to trigger debug entry.
 */
void
debug_init(int symbol_check)
{
	ulong drs[8];
	static int first_time = 1;
	char *file;

	if (first_time) {
		/*
		 * We always want to clear the debug status register
		 * before using the debugger.
		 */
		get_dregs(drs);
		drs[DR_STATUS] = 0;
		set_dregs(drs);
	}

	if (symbol_check) {
		/*
		 * If we were asked to check for a symbol file, try
		 * to read symbols.  Enter the debugger unless there
		 * was no such file.
		 */
		if (requested_symbol_file) {
			file = requested_symbol_file;
			requested_symbol_file = 0;
		} else {
			file = default_symbol_file;
		}
		if (debug_symtab(file, 1) != -1)
			enter_debug(0);
		if (file != default_symbol_file)
			bkmem_free(file, strlen(file) + 1);
	} else if (first_time) {
		/* Just do a debugger entry without reading symbols */
		enter_debug(0);
	}
	first_time = 0;
}

/*
 * It is not safe to leave breakpoints enabled on entry to the kernel
 * because the kernel loads its own IDT and GDT.
 *
 * limit_debug() is called from loadnrun_elf just before passing
 * control to the loaded module.  It sets a special breakpoint at the
 * entry to the loaded module.  When that breakpoint strikes, all
 * breakpoints are disabled.
 *
 * We do not simply disable breakpoints here because the typical
 * case is that we have loaded both kernel/unix and misc/krtld (see
 * readfile.c for details).  It is safe to use breakpoints in boot.bin
 * during krtld processing.
 */
void
limit_debug(char *name, ulong entry)
{
	ulong drs[8];

	kernel_name = name;
	kernel_entry = find_kernel_entry(entry);
	if (debug_used) {
		get_dregs(drs);
		drs[2] = kernel_entry;
		drs[DR_CONTROL] |= DR_ENABLE2;
		set_dregs(drs);
	}
}

void
trap(ulong *regs)
{
	extern short DOSsnarf_flag;
	ulong cregs[4];
	struct real_regs *rp = 0;

	switch (regs[TRAPNO]) {
	case T_SGLSTP:
		rp = (struct real_regs *)regs[EAX];
		break;
	default:
		/*
		 * Make sure output appears on the console even if
		 * debugger entry occurred during bootops and output was
		 * redirected.
		 */
		DOSsnarf_flag = 0;

		printf("Boot panic: trap type %x, error %x at %lx.\n",
			regs[TRAPNO], regs[ERR], regs[EIP]);
		get_cregs(cregs);
		printf("cr0: %x, cr2: %x, cr3: %x\n",
			cregs[0], cregs[2], cregs[3]);
		pm_dump_regs(regs);
		break;
	}

	pm_debug(regs, rp);
}

/*
 * Try to convert 'addr' into 'symbol+offset' form.  Failure is
 * indicated by returning a null symbol name and the original
 * address.
 *
 * Very simple linear lookup algorithm.  Probably fast enough
 * for this purpose.  Could change to binary search if not.
 */
void
symbol_find(ulong addr, struct syminfo *result)
{
	ulong index;

	result->name = 0;
	result->offset = addr;

	for (index = 0; index < num_syms; index++) {
		if (addr < symtab[index].offset) {
			if (index > 0) {
				index--;
				result->name = symtab[index].name;
				result->offset -= symtab[index].offset;
			}
			return;
		}
	}
}

/*
 * Attempt to read debugger symbols.  Return -1 if the file cannot be
 * opened.  Otherwise return the number of symbols actually found.
 */
int
debug_symtab(char *filename, int no_file_silent)
{
	int nmfd;
	int size;
	struct stat statbuf;
	char *buffer;
	char *p;
	ulong val;
	char *symbol;
	char *symend;
	char *avail;
	char *blkend;
	ulong count;

	if ((nmfd = open(filename, O_RDONLY)) == -1) {
		if (no_file_silent == 0) {
			printf("Cannot open %s for reading.\n", filename);
			printf("Debugger will not use symbols.\n");
		}
		return (-1);
	}

	if (fstat(nmfd, &statbuf) || statbuf.st_size == 0) {
		printf("Symbol table file %s is empty.\n", filename);
		(void) close(nmfd);
		return (0);
	}

	buffer = bkmem_alloc(statbuf.st_size + 1);
	if (buffer == 0) {
		printf("Cannot allocate %ld byte buffer for symbols.\n",
			statbuf.st_size + 1);
		(void) close(nmfd);
		return (0);
	}

	size = read(nmfd, buffer, statbuf.st_size);
	(void) close(nmfd);
	if (size != statbuf.st_size) {
		printf("Cannot read symbol information from %s.\n", filename);
		bkmem_free(buffer, statbuf.st_size + 1);
		return (0);
	}
	buffer[size] = 0;

	/* Scan through buffer once to count real symbols */
	for (p = buffer, count = 0; get_syminfo(&p, &symbol, &val); ) {
		if (val == 0)
			continue;
		if ((symend = strchr(symbol, '\n')) == 0)
			continue;
		while (symend > symbol && symend[-1] == '\r')
			symend--;
		if (symend == symbol)
			continue;
		count++;
	}

	if (count == 0) {
		printf("No symbols found in file %s.\n", filename);
		(void) close(nmfd);
		bkmem_free(buffer, statbuf.st_size + 1);
		return (0);
	}

	/*
	 * Looks like the symbol table contains useable symbols.
	 * Free the old one (if any) before reading the new one.
	 */
	if (symtab) {
		bkmem_free((char *)symtab, symtab_size);
		symtab = 0;
		symtab_size = 0;
		num_syms = 0;
	}

	symtab_size = count * sizeof (struct syminfo);
	symtab = (struct syminfo *)bkmem_alloc(symtab_size);
	if (symtab == 0) {
		printf("Cannot allocate %d bytes for symbol table.\n");
		bkmem_free(buffer, statbuf.st_size + 1);
		symtab_size = 0;
		return (0);
	}

	/* Scan through buffer again to fill in table */
	avail = 0;
	blkend = 0;
	for (p = buffer, count = 0; get_syminfo(&p, &symbol, &val); ) {
		if (val == 0)
			continue;
		if ((symend = strchr(symbol, '\n')) == 0)
			continue;
		while (symend > symbol && symend[-1] == '\r')
			symend--;
		if (symend == symbol)
			continue;
		size = symend - symbol;
		if (avail == 0 || avail + size + 1 > blkend) {
			avail = bkmem_alloc(512);
			blkend = avail + 512;
		}
		if (avail == 0)
			continue;
		(void) strncpy(avail, symbol, size);
		symtab[count].name = avail;
		avail[size] = 0;
		avail += size + 1;
		symtab[count].offset = val;
		count++;
	}
	num_syms = count;

	bkmem_free(buffer, statbuf.st_size + 1);
	printf("Read %d symbols from %s.\n", num_syms, filename);
	return (num_syms);
}

/*
 * Parse the next line of symbol table which is an in-core image
 * of the output from "nm -vx boot.bin.elf".  Returns 0 when it
 * reaches the end, 1 otherwise.  Scan from address at 'p' and
 * update it to point to the next line.  Return symbol name and
 * value at 'symbol' and 'valp'.  Return 0 if the line was not
 * parsed successfully.  Symbol name has not been null-terminated.
 */
int
get_syminfo(char **p, char **symbol, ulong *valp)
{
	char *startline = *p;
	char *endline;
	char *startnum;

	startline = *p;
	endline = strchr(startline, '\n');
	if (endline == 0)
		return (0);

	*valp = 0;
	*p = endline + 1;

	startnum = strchr(startline, '|');
	*endline = 0;
	*symbol = strrchr(startline, '|');
	*endline = '\n';

	if (startline == 0 || *symbol == 0)
		return (1);

	startnum += 3;		/* move past '|0x' */
	(void) get_hex(&startnum, valp);
	*symbol += 1;		/* move past '|' */
	return (1);
}

/*
 * If elfbootvec points at a valid boot vector with an EB_AUXV record
 * whose auxiliary vector contains an AT_ENTRY record, return the
 * AT_ENTRY value (this is the normal case of kernel/unix being
 * "interpreted" by misc/krtld).  Otherwise return the entry passed
 * in which is the entry point of an un-interpreted ELF (typically
 * kadb).
 */
static ulong
find_kernel_entry(ulong entry)
{
	Elf32_Boot *ebp;
	auxv_t *auxv;
	extern Elf32_Boot *elfbootvec;

	if (elfbootvec == 0) {
		return (entry);
	}

	for (ebp = elfbootvec; ebp->eb_tag != EB_NULL; ebp++) {
		switch (ebp->eb_tag) {
		case EB_AUXV:
			for (auxv = (auxv_t *)ebp->eb_un.eb_ptr;
					auxv->a_type != AT_NULL; auxv++) {
				switch (auxv->a_type) {
				case AT_ENTRY:
					return ((ulong)auxv->a_un.a_ptr);
				}
			}
			break;
		}
	}
	return (entry);
}
