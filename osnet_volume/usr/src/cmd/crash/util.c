/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		Copyright (C) 1991  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)util.c	1.24	98/06/03 SMI"

/*
 * This file contains code for utilities used by more than one crash
 * function.
 */

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/param.h>
#include <setjmp.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/user.h>
#include <sys/cpuvar.h>
#include <sys/var.h>
#include <sys/proc.h>
#include <sys/elf.h>
#include <vm/as.h>
#include <sys/vmparam.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/kmem_impl.h>
#include "crash.h"

extern ssize_t kvm_as_read(kvm_t *, struct as *, uintptr_t, void *, size_t);

struct procslot *slottab;
void exit();
void free();

/* close pipe and reset file pointers */
void
resetfp(void)
{
	if (opipe == 1) {
		pclose(fp);
		signal(SIGPIPE, pipesig);
		opipe = 0;
		fp = stdout;
	} else {
		if ((fp != stdout) && (fp != rp) && (fp != NULL)) {
			fclose(fp);
			fp = stdout;
		}
	}
}

/* signal handling */
/*ARGSUSED*/
void
sigint(int sig)
{
	extern int *stk_bptr;

	signal(SIGINT, sigint);

	if (stk_bptr)
		free((char *)stk_bptr);
	fflush(fp);
	resetfp();
	fprintf(fp, "\n");
	longjmp(jmp, 0);
}

/* used in init.c to exit program */
void
fatal(char *message, ...)
{
	va_list args;

	va_start(args, message);
	fprintf(stderr, "crash: ");
	vfprintf(stderr, message, args);
	va_end(args);
	exit(1);
}

/* string to hexadecimal long conversion */
static long
hextol(char *s)
{
	long i, j;

	for (j = 0; s[j] != '\0'; j++)
		if ((s[j] < '0' || s[j] > '9') && (s[j] < 'a' || s[j] > 'f') &&
			(s[j] < 'A' || s[j] > 'F'))
			break;
	if (s[j] != '\0' || sscanf(s, "%lx", &i) != 1) {
		prerrmes("%c is not a digit or letter a - f\n", s[j]);
		return (-1);
	}
	return (i);
}


/* string to decimal long conversion */
static long
stol(char *s)
{
	long i, j;

	for (j = 0; s[j] != '\0'; j++)
		if ((s[j] < '0' || s[j] > '9'))
			break;
	if (s[j] != '\0' || sscanf(s, "%ld", &i) != 1) {
		prerrmes("%c is not a digit 0 - 9\n", s[j]);
		return (-1);
	}
	return (i);
}


/* string to octal long conversion */
static long
octol(char *s)
{
	long i, j;

	for (j = 0; s[j] != '\0'; j++)
		if ((s[j] < '0' || s[j] > '7'))
			break;
	if (s[j] != '\0' || sscanf(s, "%lo", &i) != 1) {
		prerrmes("%c is not a digit 0 - 7\n", s[j]);
		return (-1);
	}
	return (i);
}


/* string to binary long conversion */
static long
btol(char *s)
{
	long i, j;

	i = 0;
	for (j = 0; s[j] != '\0'; j++)
		switch (s[j]) {
			case '0':
				i = i << 1;
				break;
			case '1':
				i = (i << 1) + 1;
				break;
			default:
				prerrmes("%c is not a 0 or 1\n", s[j]);
				return (-1);
		}
	return (i);
}

/* string to number conversion */
long
strcon(char *string, char format)
{
	char *s;

	s = string;
	if (*s == '0') {
		if (strlen(s) == 1)
			return (0);
		switch (*++s) {
			case 'X':
			case 'x':
				format = 'h';
				s++;
				break;
			case 'B':
			case 'b':
				format = 'b';
				s++;
				break;
			case 'D':
			case 'd':
				format = 'd';
				s++;
				break;
			default:
				format = 'o';
		}
	}
	if (!format)
		format = 'd';
	switch (format) {
		case 'h':
			return (hextol(s));
		case 'd':
			return (stol(s));
		case 'o':
			return (octol(s));
		case 'b':
			return (btol(s));
		default:
			return (-1);
	}
}

/* lseek and read */
void
readmem(void *addr, int mode, void *buf, size_t size, char *name)
{
	switch (mode) {
	case -1:
		if (kvm_uread(kd, (uintptr_t)addr, buf, size) != size)
			error("read error on %s at %p\n", name, addr);
		return;
	case 0:
		if (kvm_pread(kd, (uint64_t)addr, buf, size) != size)
			error("read error on %s at %p\n", name, addr);
		return;
	case 1:
		if (kvm_kread(kd, (uintptr_t)addr, buf, size) != size)
			error("read error on %s at %p\n", name, addr);
		return;
	}
	error("invalid mode %d passed to readmem", mode);
}

/* lseek to symbol name and read */
void
readsym(char *sym, void *buffer, unsigned size)
{
	Sym *np;

	if (!(np = symsrch(sym)))
		error("%s not found in symbol table\n", sym);
	readmem((void *)np->st_value, 1, buffer, size, sym);
}

/* lseek and read into given buffer */
void
readbuf(void *addr, off_t off, int phys, void *buffer, size_t size, char *name)
{
	if ((phys || !Virtmode) && (addr != (void *)-1))
		readmem(addr, 0, buffer, size, name);
	else if (addr != (void *)-1)
		readmem(addr, 1, buffer, size, name);
	else
		readmem((void *)off, 1, buffer, size, name);
}

/* error handling */
void
error(char *message, ...)
{
	va_list args;

	va_start(args, message);
	if (rp)
		vfprintf(stdout, message, args);
	vfprintf(fp, message, args);
	va_end(args);
	fflush(fp);
	resetfp();
	longjmp(jmp, 0);
}


/* print error message */
void
prerrmes(char *message, ...)
{
	va_list args;

	va_start(args, message);

	if ((rp && (rp != stdout)) || (fp != stdout))
		vfprintf(stdout, message, args);
	vfprintf(fp, message, args);
	va_end(args);
	fflush(fp);
}


/* simple arithmetic expression evaluation ( +  - & | * /) */
long
eval(char *string)
{
	int j, i;
	char rand1[ARGLEN];
	char rand2[ARGLEN];
	char *op;
	long addr1, addr2;
	Sym *sp;

	if (string[strlen(string) - 1] != ')') {
		prerrmes("(%s is not a well-formed expression\n", string);
		return (-1);
	}
	if (!(op = strpbrk(string, "+-&|*/"))) {
		prerrmes("(%s is not an expression\n", string);
		return (-1);
	}
	for (j = 0, i = 0; string[j] != *op; j++, i++) {
		if (string[j] == ' ')
			--i;
		else
			rand1[i] = string[j];
	}
	rand1[i] = '\0';
	j++;
	for (i = 0; string[j] != ')'; j++, i++) {
		if (string[j] == ' ')
			--i;
		else
			rand2[i] = string[j];
	}
	rand2[i] = '\0';
	if (!strlen(rand2) || strpbrk(rand2, "+-&|*/")) {
		prerrmes("(%s is not a well-formed expression\n", string);
		return (-1);
	}
	if (sp = symsrch(rand1))
		addr1 = sp->st_value;
	else if ((addr1 = strcon(rand1, NULL)) == -1)
		return (-1);
	if (sp = symsrch(rand2))
		addr2 = sp->st_value;
	else if ((addr2 = strcon(rand2, NULL)) == -1)
		return (-1);
	switch (*op) {
		case '+':
			return (addr1 + addr2);
		case '-':
			return (addr1 - addr2);
		case '&':
			return (addr1 & addr2);
		case '|':
			return (addr1 | addr2);
		case '*':
			return (addr1 * addr2);
		case '/':
			if (addr2 == 0) {
				prerrmes("cannot divide by 0\n");
				return (-1);
			}
			return (addr1 / addr2);
	}
	return (-1);
}

static maketab = 1;

void
makeslottab(void)
{
	proc_t *prp, pr;
	struct pid pid;
	void *practive;
	int i;

	if (maketab) {
		maketab = 0;
		practive = sym2addr("practive");
		slottab = (struct procslot *)
			malloc(vbuf.v_proc * sizeof (struct procslot));
		for (i = 0; i < vbuf.v_proc; i++) {
			slottab[i].p = 0;
			slottab[i].pid = -1;
		}
		readmem(practive, 1, &prp, sizeof (proc_t *), "practive");
		for (; prp != NULL; prp = pr.p_next) {
			readmem(prp, 1, &pr, sizeof (proc_t), "proc table");
			readmem(pr.p_pidp, 1, &pid,
				sizeof (struct pid), "pid table");
			i = pid.pid_prslot;
			if (i < 0 || i >= vbuf.v_proc)
				fatal("process slot out of bounds\n");
			slottab[i].p = prp;
			slottab[i].pid = pid.pid_id;
		}
	}
}

pid_t
slot_to_pid(long slot)
{
	if (slot < 0 || slot >= vbuf.v_proc)
		return (NULL);
	if (maketab)
		makeslottab();
	return (slottab[slot].pid);
}

proc_t *
slot_to_proc(long slot)
{
	if (slot < 0 || slot >= vbuf.v_proc)
		return (NULL);
	if (maketab)
		makeslottab();
	return (slottab[slot].p);
}

/* convert proc address to proc slot */
int
proc_to_slot(intptr_t addr)
{
	int i;

	if (addr == NULL)
		return (-1);

	if (maketab)
		makeslottab();

	for (i = 0; i < vbuf.v_proc; i++)
		if (slottab[i].p == (proc_t *)addr)
			return (i);

	return (-1);

}

/* get current process slot number */
int
getcurproc(void)
{
	struct _kthread  thread;

	/* Read the current thread structure */
	readmem(Curthread, 1, &thread, sizeof (thread), "current thread");

	return (proc_to_slot((intptr_t)thread.t_procp));
}

kthread_id_t
getcurthread(void)
{
	struct cpu cpu;
	struct cpu **cpup;
	static Sym *panic_thread_sym;
	kthread_id_t panic_thread;
	void *cpu_addr = sym2addr("cpu");

	if (!panic_thread_sym)
		if ((panic_thread_sym = symsrch("panic_thread")) == NULL)
			error("Could not find symbol panic_thread\n");
	/*
	 * if panic_thread is set, that's our current thread.
	 */
	readmem((void *)panic_thread_sym->st_value, 1, &panic_thread,
		sizeof (panic_thread), "panic_thread address");

	if (panic_thread)
		return (panic_thread);

	/*
	 * if panic_thread is not set, look through the cpu array until
	 * we find a non-null entry (the cpu array may be sparse (e.g. sun4d)).
	 * read the cpu struct and return the cpu's current thread pointer.
	 */
	cpup = cpu_addr;
	do {
		readmem(cpup, 1, &cpu_addr, sizeof (intptr_t), "cpu addr");
		cpup++;
	} while (cpu_addr == NULL);

	readmem(cpu_addr, 1, &cpu, sizeof (cpu), "cpu structure");

	return (cpu.cpu_thread);

}


/* argument processing from **args */
void
getargs(long max, long *arg1, long *arg2, int phys)
{
	Sym *sp;
	long slot;

	if (maketab)
		makeslottab();

	/* range */
	if (strpbrk(args[optind], "-")) {
		range(max, arg1, arg2);
		return;
	}
	/* expression */
	if (*args[optind] == '(') {
		*arg1 = (eval(++args[optind]));
		return;
	}
	/* symbol */
	if ((sp = symsrch(args[optind])) != NULL) {
		*arg1 = (sp->st_value);
		return;
	}
	if (isasymbol(args[optind])) {
		prerrmes("%s not found in symbol table\n", args[optind]);
		*arg1 = -1;
		return;
	}
	/* slot number */
	if ((slot = strcon(args[optind], 'h')) == -1) {
		*arg1 = -1;
		return;
	}
	if ((slot >= Start->st_value) || ((phys || !Virtmode) &&
	    ((longlong_t)slot >= vtop(Start->st_value, Procslot)))) {
		/* address */
		*arg1 = slot;
	} else {
		if ((slot = strcon(args[optind], 'd')) == -1) {
			*arg1 = -1;
			return;
		}
		if ((slot < max) && (slot >= 0)) {
			*arg1 = slot;
			return;
		} else {
			prerrmes("%d is out of range\n", slot);
			*arg1 = -1;
			return;
		}
	}
}

/* get slot number in table from address */
long
getslot(long addr, long base, int size, int phys, long max)
{
	longlong_t pbase;
	long slot;

	if (phys || !Virtmode) {
		pbase = vtop(base, Procslot);
		if (pbase == -1LL)
			error("%x is an invalid address\n", base);
		slot = ((longlong_t)addr - pbase) / size;
	} else
		slot = (addr - base) / size;
	if ((slot >= 0) && (slot < max))
		return (slot);
	else
		return (-1);
}

/*
 * fopen a file for output.  use process's real group id so that
 * crash users cannot write to files writable by group sys.
 */
FILE *
fopen_output(char *filename)
{
	FILE	*fp;
	gid_t	saved_gid = getegid();

	if (setgid(getgid()) < 0)
		error("unable to set gid to real gid\n");

	fp = fopen(filename, "a");

	if (setgid(saved_gid) < 0)
		fprintf(stdout, "unable to reset gid to effective gid\n");

	return (fp);
}


/* file redirection */
void
redirect(void)
{
	int i;
	FILE *ofp;

	ofp = fp;
	if (opipe == 1) {
		fprintf(stdout, "file redirection (-w) option ignored\n");
		return;
	}
	if (fp = fopen_output(optarg)) {
		fprintf(fp, "\n> ");
		for (i = 0; i < argcnt; i++)
			fprintf(fp, "%s ", args[i]);
		fprintf(fp, "\n");
	} else {
		fp = ofp;
		error("unable to open %s\n", optarg);
	}
}


/*
 * putch() recognizes escape sequences as well as characters and prints the
 * character or equivalent action of the sequence.
 */
void
putch(char c)
{
	c &= 0377;
	if (c < 040 || c > 0176) {
		fprintf(fp, "\\");
		switch (c) {
			case '\0':
				c = '0';
				break;
			case '\t':
				c = 't';
				break;
			case '\n':
				c = 'n';
				break;
			case '\r':
				c = 'r';
				break;
			case '\b':
				c = 'b';
				break;
			default:
				c = '?';
				break;
		}
	} else
		fprintf(fp, " ");
	fprintf(fp, "%c ", c);
}

/* sets process to input argument */
long
setproc(void)
{
	long slot;

	if ((slot = strcon(optarg, 'd')) == -1)
		error("\n");
	if ((slot > vbuf.v_proc) || (slot < 0))
		error("%d out of range\n", slot);
	return (slot);
}

/* check to see if string is a symbol or a hexadecimal number */
int
isasymbol(char *string)
{
	size_t i;

	for (i = strlen(string); i >= 1; i--)
		if (!isxdigit(*string++))
			return (1);
	return (0);
}


/* convert a string into a range of slot numbers */
void
range(long max, long *begin, long *end)
{
	size_t i, j, len, pos;
	char string[ARGLEN];
	char temp1[ARGLEN];
	char temp2[ARGLEN];

	(void) strcpy(string, args[optind]);
	len = strlen(string);
	if ((*string == '-') || (string[len - 1] == '-')) {
		fprintf(fp, "%s is an invalid range\n", string);
		*begin = -1;
		return;
	}
	pos = strcspn(string, "-");
	for (i = 0; i < pos; i++)
		temp1[i] = string[i];
	temp1[i] = '\0';
	for (j = 0, i = pos + 1; i < len; j++, i++)
		temp2[j] = string[i];
	temp2[j] = '\0';
	if ((*begin = stol(temp1)) == -1)
		return;
	if ((*end = stol(temp2)) == -1) {
		*begin = -1;
		return;
	}
	if (*begin > *end) {
		fprintf(fp, "%ld-%ld is an invalid range\n", *begin, *end);
		*begin = -1;
		return;
	}
	if (*end >= max)
		*end = max - 1;
}

/*
 * Find the kernel address of the cache with the specified name
 */
kmem_cache_t *
kmem_cache_find(char *name)
{
	Sym *kmem_null_cache_sym;
	intptr_t kmem_null_cache_addr;
	kmem_cache_t c, *cp;

	if ((kmem_null_cache_sym = symsrch("kmem_null_cache")) == 0)
		(void) error("kmem_null_cache not in symbol table\n");
	kmem_null_cache_addr = kmem_null_cache_sym->st_value;
	if (kvm_read(kd, kmem_null_cache_addr, (char *)&c, sizeof (c)) == -1)
		return (NULL);
	for (cp = c.cache_next; cp != (kmem_cache_t *)kmem_null_cache_addr;
	    cp = c.cache_next) {
		if (kvm_read(kd, (intptr_t)cp, (char *)&c, sizeof (c)) == -1)
			return (NULL);
		if (strcmp(c.cache_name, name) == 0)
			return (cp);
	}
	return (NULL);
}

/*
 * Find the kernel addresses of all caches with the specified name prefix
 */
int
kmem_cache_find_all(char *name, kmem_cache_t **cache_list, int max_caches)
{
	Sym *kmem_null_cache_sym;
	intptr_t kmem_null_cache_addr;
	kmem_cache_t c, *cp;
	int ncaches = 0;
	size_t len = strlen(name);

	if ((kmem_null_cache_sym = symsrch("kmem_null_cache")) == 0)
		(void) error("kmem_null_cache not in symbol table\n");
	kmem_null_cache_addr = kmem_null_cache_sym->st_value;
	if (kvm_read(kd, kmem_null_cache_addr, (char *)&c, sizeof (c)) == -1)
		return (0);
	for (cp = c.cache_next; cp != (kmem_cache_t *)kmem_null_cache_addr;
	    cp = c.cache_next) {
		if (kvm_read(kd, (intptr_t)cp, (char *)&c, sizeof (c)) == -1)
			return (NULL);
		if (strncmp(c.cache_name, name, len) == 0) {
			if (ncaches >= max_caches)
				return (ncaches);
			cache_list[ncaches++] = cp;
		}
	}
	return (ncaches);
}

static int
addrcmp(const void *a1, const void *a2)
{
	intptr_t u1 = *(intptr_t *)a1;
	intptr_t u2 = *(intptr_t *)a2;

	if (u1 < u2)
		return (-1);
	if (u1 > u2)
		return (1);
	return (0);
}

/*
 * Apply func to each allocated object in the specified kmem cache
 */
int
kmem_cache_apply(kmem_cache_t *kcp, void (*func)(void *kaddr, void *buf))
{
	kmem_cache_t *cp;
	kmem_magazine_t *kmp, *mp;
	kmem_slab_t s, *sp;
	kmem_bufctl_t bc, *bcp;
	void *buf, *ubase, *kbase, **maglist;
	int magsize, magcnt, magmax;
	size_t magbsize, csize;
	int chunks, refcnt, flags, bufsize, chunksize, i, cpu_seqid;
	char *valid;
	int errcode = -1;
	Sym *ncpus_sym;
	intptr_t ncpus_addr;
	int ncpus;

	if ((ncpus_sym = symsrch("ncpus")) == 0)
		(void) error("ncpus not in symbol table\n");
	ncpus_addr = ncpus_sym->st_value;
	if (kvm_read(kd, ncpus_addr, (char *)&ncpus, sizeof (int)) == -1)
		return (-1);

	csize = KMEM_CACHE_SIZE(ncpus);
	cp = malloc(csize);

	if (kvm_read(kd, (ulong_t)kcp, (void *)cp, csize) == -1)
		goto out1;

	magsize = cp->cache_magazine_size;
	magbsize = sizeof (kmem_magazine_t) + (magsize - 1) * sizeof (void *);
	mp = malloc(magbsize);

	magmax = (cp->cache_fmag_total + 2 * ncpus + 100) * magsize;
	maglist = malloc(magmax * sizeof (void *));
	magcnt = 0;

	kmp = cp->cache_fmag_list;
	while (kmp != NULL) {
		if (kvm_read(kd, (ulong_t)kmp, (void *)mp, magbsize) == -1)
			goto out2;
		for (i = 0; i < magsize; i++) {
			maglist[magcnt] = mp->mag_round[i];
			if (++magcnt > magmax)
				goto out2;
		}
		kmp = mp->mag_next;
	}

	for (cpu_seqid = 0; cpu_seqid < ncpus; cpu_seqid++) {
		kmem_cpu_cache_t *ccp = &cp->cache_cpu[cpu_seqid];
		if (ccp->cc_rounds > 0 &&
		    (kmp = ccp->cc_loaded_mag) != NULL) {
			if (kvm_read(kd, (ulong_t)kmp, (void *)mp,
			    magbsize) == -1)
				goto out2;
			for (i = 0; i < ccp->cc_rounds; i++) {
				maglist[magcnt] = mp->mag_round[i];
				if (++magcnt > magmax)
					goto out2;
			}
		}
		if ((kmp = ccp->cc_full_mag) != NULL) {
			if (kvm_read(kd, (ulong_t)kmp, (void *)mp,
			    magbsize) == -1)
				goto out2;
			for (i = 0; i < magsize; i++) {
				maglist[magcnt] = mp->mag_round[i];
				if (++magcnt > magmax)
					goto out2;
			}
		}
	}

	qsort(maglist, magcnt, sizeof (void *), addrcmp);

	flags = cp->cache_flags;
	bufsize = cp->cache_bufsize;
	chunksize = cp->cache_chunksize;
	valid = malloc(cp->cache_slabsize / bufsize);
	ubase = malloc(cp->cache_slabsize + sizeof (kmem_bufctl_t));

	sp = cp->cache_nullslab.slab_next;
	while (sp != &kcp->cache_nullslab) {
		if (kvm_read(kd, (ulong_t)sp, (void *)&s, sizeof (s)) == -1 ||
		    s.slab_cache != kcp)
			goto out3;
		chunks = s.slab_chunks;
		refcnt = s.slab_refcnt;
		kbase = s.slab_base;
		if (kvm_read(kd, (ulong_t)kbase, (void *)ubase,
		    chunks * chunksize) == -1)
			goto out3;
		memset(valid, 1, chunks);
		bcp = s.slab_head;
		for (i = refcnt; i < chunks; i++) {
			if (flags & KMF_HASH) {
				if (kvm_read(kd, (ulong_t)bcp, (void *)&bc,
				    sizeof (bc)) == -1)
					goto out3;
				buf = bc.bc_addr;
			} else {
				bc = *((kmem_bufctl_t *)
				    ((intptr_t)bcp - (intptr_t)kbase +
				    (intptr_t)ubase));
				buf = (void *)((intptr_t)bcp -
				    cp->cache_offset);
			}
			valid[((intptr_t)buf - (intptr_t)kbase) / chunksize]
				= 0;
			bcp = bc.bc_next;
		}
		for (i = 0; i < chunks; i++) {
			void *kbuf = (char *)kbase + i * chunksize;
			void *ubuf = (char *)ubase + i * chunksize;
			if (valid[i] && bsearch(&kbuf, maglist, magcnt,
			    sizeof (void *), addrcmp) == NULL)
				(*func)(kbuf, ubuf);
		}
		sp = s.slab_next;
	}
	errcode = 0;
out3:
	free(valid);
	free(ubase);
out2:
	free(mp);
	free(maglist);
out1:
	free(cp);
	return (errcode);
}

/*
 * Apply func to each allocated object in the specified kmem cache.
 * Assumes that KMF_AUDIT is set which implies that KMF_HASH is set.
 */
int
kmem_cache_audit_apply(kmem_cache_t *cp,
	void (*func)(void *kaddr, void *buf, size_t size,
	kmem_bufctl_audit_t *bcp))
{
	kmem_bufctl_audit_t bc;
	kmem_cache_t c;
	kmem_slab_t *sp, s;
	kmem_buftag_t *btp;
	int i, chunks, chunksize, slabsize;
	void *kbase, *ubase;
	int bufsize;

	if (kvm_read(kd, (ulong_t)cp, (void *)&c, sizeof (c)) == -1) {
		perror("kvm_read kmem_cache");
		return (-1);
	}
	if (!(c.cache_flags & KMF_AUDIT)) {
		printf("KMF_AUDIT is not enabled for cache %s\n", c.cache_name);
		return (-1);
	}
	bufsize = c.cache_bufsize;
	chunksize = c.cache_chunksize;
	slabsize = c.cache_slabsize;
	ubase = malloc(slabsize);

	if (ubase == NULL) {
		perror("malloc");
		return (-1);
	}

	sp = c.cache_nullslab.slab_next;
	while (sp != &cp->cache_nullslab) {
		if (kvm_read(kd, (ulong_t)sp, (void *)&s, sizeof (s)) == -1 ||
		    s.slab_cache != cp) {
			fprintf(stderr, "error reading slab list in cache %s\n",
			    c.cache_name);
			break;
		}
		kbase = s.slab_base;
		chunks = s.slab_chunks;
		if (kvm_read(kd, (ulong_t)kbase, ubase,
		    chunks * chunksize) == -1) {
			fprintf(stderr, "error reading slab %p in cache %s\n",
			    (void *)sp, c.cache_name);
			break;
		}
		for (i = 0; i < chunks; i++) {
			void *buf = (void *)((intptr_t)ubase + i * chunksize);
			btp = (kmem_buftag_t *)((intptr_t)buf + c.cache_offset);
			if (btp->bt_bxstat !=
			    ((intptr_t)btp->bt_bufctl ^ KMEM_BUFTAG_ALLOC))
				continue;
			if (kvm_read(kd, (uintptr_t)btp->bt_bufctl, (void *)&bc,
			    sizeof (kmem_bufctl_audit_t)) == -1) {
				fprintf(stderr,
				    "error reading bufctl %p in cache %s\n",
				    (void *)btp->bt_bufctl, c.cache_name);
				free(ubase);
				return (0);
			}
			(*func)(bc.bc_addr, buf, bufsize, &bc);
		}
		sp = s.slab_next;
	}
	free(ubase);
	return (0);
}

/*
 * Get statistics for the specified kmem cache
 */
int
kmem_cache_getstats(kmem_cache_t *kcp, kmem_cache_stat_t *kcsp)
{
	kmem_cache_t *cp;
	kmem_slab_t s, *sp;
	Sym *ncpus_sym;
	intptr_t ncpus_addr;
	int ncpus, cpu_seqid;
	size_t csize;
	int errcode = -1;

	if ((ncpus_sym = symsrch("ncpus")) == 0)
		(void) error("ncpus not in symbol table\n");
	ncpus_addr = ncpus_sym->st_value;
	if (kvm_read(kd, ncpus_addr, (char *)&ncpus, sizeof (int)) == -1)
		return (-1);

	csize = KMEM_CACHE_SIZE(ncpus);
	cp = malloc(csize);

	if (kvm_read(kd, (ulong_t)kcp, (void *)cp, csize) == -1)
		goto out;

	kcsp->kcs_buf_size	= cp->cache_bufsize;
	kcsp->kcs_slab_size	= cp->cache_slabsize;
	kcsp->kcs_alloc		= cp->cache_global_alloc +
		cp->cache_depot_alloc;
	kcsp->kcs_alloc_fail	= cp->cache_alloc_fail;
	kcsp->kcs_buf_avail = cp->cache_fmag_total * cp->cache_magazine_size;
	kcsp->kcs_buf_total	= cp->cache_buftotal;
	kcsp->kcs_buf_max	= cp->cache_bufmax;
	kcsp->kcs_slab_create	= cp->cache_slab_create;
	kcsp->kcs_slab_destroy	= cp->cache_slab_destroy;

	for (cpu_seqid = 0; cpu_seqid < ncpus; cpu_seqid++) {
		kmem_cpu_cache_t *ccp = &cp->cache_cpu[cpu_seqid];
		if (ccp->cc_rounds > 0)
			kcsp->kcs_buf_avail += ccp->cc_rounds;
		if (ccp->cc_full_mag)
			kcsp->kcs_buf_avail += ccp->cc_magsize;
		kcsp->kcs_alloc += ccp->cc_alloc;
	}

	if (cp->cache_constructor != NULL && !(cp->cache_flags & KMF_DEADBEEF))
		kcsp->kcs_buf_constructed = kcsp->kcs_buf_avail;
	else
		kcsp->kcs_buf_constructed = 0;

	for (sp = cp->cache_freelist; sp != &kcp->cache_nullslab;
	    sp = s.slab_next) {
		if (kvm_read(kd, (ulong_t)sp, (void *)&s, sizeof (s)) == -1 ||
		    s.slab_cache != kcp)
			goto out;
		kcsp->kcs_buf_avail += s.slab_chunks - s.slab_refcnt;
	}

	errcode = 0;
out:
	free(cp);
	return (errcode);
}

struct allocowner {
	struct allocowner *head;
	struct allocowner *next;
	size_t	signature;
	uint_t	num;
	size_t	data_size;
	size_t	total_size;
	uint_t	depth;
	intptr_t	pc[KMEM_STACK_DEPTH];
};

static struct allocowner *ownerhash = NULL;
static int ownersize = 0;	/* Total number of entries */
static int ownercnt = 0;	/* Number of entries in use */

void
init_owner(void)
{
	if (ownerhash != NULL)
		free(ownerhash);
	ownerhash = NULL;
	ownersize = 0;
	ownercnt = 0;
}

static void
ownerhash_grow(void)
{
	struct allocowner *ao, *aoend;

	ownersize <<= 1;
	if (ownersize == 0)
		ownersize = 1024;
	ownerhash = realloc(ownerhash, ownersize * sizeof (struct allocowner));

	if (ownerhash == NULL) {
		init_owner();
		error("Out of memory\n");
	}

	for (ao = ownerhash, aoend = ownerhash + ownersize; ao < aoend; ao++)
		ao->head = NULL;

	for (ao = ownerhash, aoend = ownerhash + ownercnt; ao < aoend; ao++) {
		size_t bucket = ao->signature & (ownersize - 1);
		ao->next = ownerhash[bucket].head;
		ownerhash[bucket].head = ao;
	}
}

void
add_owner(kmem_bufctl_audit_t *bcp, size_t size, size_t data_size)
{
	int j;
	size_t bucket, signature;
	struct allocowner *ao;

	if (ownercnt >= ownersize)
		ownerhash_grow();

	signature = data_size;
	for (j = 0; j < bcp->bc_depth; j++)
		signature += bcp->bc_stack[j];

	bucket = signature & (ownersize - 1);
	for (ao = ownerhash[bucket].head; ao != NULL; ao = ao->next) {
		if (ao->signature == signature) {
			size_t difference = 0;
			difference |= (ao->data_size - data_size);
			difference |= (ao->depth - bcp->bc_depth);
			for (j = 0; j < bcp->bc_depth; j++)
				difference |= (ao->pc[j] - bcp->bc_stack[j]);
			if (difference == 0) {
				ao->num++;
				ao->total_size += size;
				return;
			}
		}
	}

	ao = &ownerhash[ownercnt++];
	ao->next = ownerhash[bucket].head;
	ownerhash[bucket].head = ao;
	ao->signature = signature;
	ao->num = 1;
	ao->data_size = data_size;
	ao->total_size = size;
	ao->depth = bcp->bc_depth;
	for (j = 0; j < bcp->bc_depth; j++)
		ao->pc[j] = bcp->bc_stack[j];
}

static int
ownercmp(const void *a1, const void *a2)
{
	const struct allocowner *ao1 = a1;
	const struct allocowner *ao2 = a2;
	return (ao2->total_size - ao1->total_size);
}

void
print_owner(char *itemstr, int mem_threshold, int cnt_threshold)
{
	int j;
	struct allocowner *ao, *aoend;

	qsort(ownerhash, ownercnt, sizeof (struct allocowner), ownercmp);

	for (ao = ownerhash, aoend = ownerhash + ownercnt; ao < aoend; ao++) {
		if (ao->total_size < mem_threshold &&
		    ao->num < cnt_threshold)
			continue;
		fprintf(fp, "%lu bytes for %d %s with data size %lu:\n",
		    ao->total_size, ao->num, itemstr, ao->data_size);
		for (j = 0; j < ao->depth; j++) {
			fprintf(fp, "\t ");
			prsymbol(NULL, ao->pc[j]);
		}
	}
}

/*
 * Routines to convert kernel symbol to kernel address, and vice versa.
 * There are two versions of each conversion.  The version beginning
 * with try_ will return NULL if they don't find a match.  The others
 * longjmp out if they fail, so make sure you do these calls before
 * you start allocating memory that will not be freed.
 */
void *
try_sym2addr(char *sym_name)
{
	Sym* sym = symsrch(sym_name);
	if (sym == NULL)
		return (NULL);
	return ((void *) sym->st_value);
}

char *
try_addr2sym(void *addr)
{
	Sym *sym = findsym((ulong_t)addr);
	if (sym == NULL)
		return (NULL);
	return (strtbl + sym->st_name);
}

void *
sym2addr(char *sym_name)
{
	void *addr = try_sym2addr(sym_name);
	if (addr == NULL)
		error("cannot find symbol \"%s\" in symbol table\n", sym_name);
	return (addr);
}

char *
addr2sym(void *addr)
{
	char *sym = try_addr2sym(addr);
	if (sym == NULL)
		error("cannot find symbol for 0x%08x in symbol table\n", addr);
	return (sym);
}
