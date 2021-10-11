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
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)crash.h	1.17	99/04/14 SMI"

#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <stdarg.h>
#include <sys/isa_defs.h>
#include <libelf.h>
#include <elf.h>
#include <sys/machelf.h>
#include <sys/thread.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/kmem_impl.h>
#include <kvm.h>

/* This file should include only command independent declarations */

#define	ARGLEN 40	/* max length of argument */

extern FILE	*fp;		/* output file */
extern int	 Procslot;	/* current process slot number */
extern int	 Virtmode;	/* current address translation mode */
extern jmp_buf	 syn;		/* syntax error label */
extern char	*args[];	/* argument array */
extern int	 argcnt;	/* number of arguments */
extern int	 optind;	/* argument index */
extern char	*optarg;	/* getopt argument */
extern char	*strtbl;	/* pointer to string table */
extern kvm_t	*kd;		/* descriptor for accessing kernel memory */
extern struct var    vbuf;	/* tunable variables buffer */
extern kthread_id_t  Curthread; /* pointer to current thread */

struct procslot {
	proc_t *p;
	pid_t   pid;
};

typedef struct kmem_cache_stat {
	uint_t	kcs_buf_size;
	uint_t	kcs_slab_size;
	uint_t	kcs_alloc;
	uint_t	kcs_alloc_fail;
	uint_t	kcs_buf_constructed;
	uint_t	kcs_buf_avail;
	uint_t	kcs_buf_total;
	uint_t	kcs_buf_max;
	uint_t	kcs_slab_create;
	uint_t	kcs_slab_destroy;
} kmem_cache_stat_t;

extern void getargs(long, long *, long *, int);	/* function to get arguments */
extern long strcon(char *, char); /* function to convert strings to long */
extern long eval(char *);	/* function to evaluate expressions */
extern Sym *symsrch(char *);	/* function for symbol search */
extern Sym *findsym(unsigned long);	/* function for symbol search */
extern void rdsymtab();
extern void prsymbol(char *, intptr_t);
extern int proc_to_slot(intptr_t);
extern void error(char *, ...);
extern void prerrmes(char *, ...);

extern longlong_t vtop(intptr_t, long);
extern int getvtop(void);
extern int getmode(void);

extern void (*pipesig)(int);
extern struct procslot *slottab;
extern jmp_buf  jmp;

extern Sym *Curproc, *Start;
extern Sym *Panic, *V;
extern long _userlimit;

extern int opipe;
extern FILE *rp;
extern void sigint(int);
extern void resetfp(void);
extern void readmem(void *, int, void *, size_t, char *);
extern void readsym(char *, void *, unsigned);
extern void readbuf(void *, off_t, int, void *, size_t, char *);
extern void makeslottab(void);
extern pid_t slot_to_pid(long);
extern proc_t *slot_to_proc(long);
extern int getcurproc(void);
extern kthread_id_t getcurthread(void);
extern long getslot(long, long, int, int, long);
extern FILE *fopen_output(char *);
extern void redirect(void);
extern void putch(char);
extern long setproc(void);
extern int isasymbol(char *);
extern void range(long, long *, long *);
extern kmem_cache_t *kmem_cache_find(char *);
extern int kmem_cache_find_all(char *, kmem_cache_t **, int);
extern int kmem_cache_apply(kmem_cache_t *, void (*)(void *, void *));
extern int kmem_cache_audit_apply(kmem_cache_t *cp,
		void (*func)(void *kaddr, void *buf, size_t size,
			    kmem_bufctl_audit_t *bcp));
extern int kmem_cache_getstats(kmem_cache_t *, kmem_cache_stat_t *);

extern void init_owner(void);
extern void add_owner(kmem_bufctl_audit_t *bcp, size_t size, size_t data_size);
extern void print_owner(char *itemstr, int mem_threshold, int cnt_threshold);
extern void fatal(char *, ...);

extern struct user *ubp;	/* ublock pointer */

extern unsigned setbf(long top, long bottom, int slot);
extern int getuser(void);
extern int getpcb(void);
extern int getstack(void);
extern int gettrace(void);
extern int getkfp(void);
extern void prsema(struct _ksema *);
extern void prmutex(kmutex_t *);
extern void prrwlock(struct _krwlock *rwp);
extern void prcondvar(struct _kcondvar *, char *);
extern void prvnode(struct vnode *, int);

extern int getproc(void);
extern int getdefproc(void);
extern int readsid(struct sess *);
extern int readpid(struct pid *);

extern int getbufhdr(void);
extern int getbuffer(void);
extern int getod(void);

extern intptr_t getaddr(char *);
extern int getcount(char *);
extern int prsldterm(int, intptr_t);

extern char *dumpfile;
extern char *namelist;

extern void init(void);

extern void *sym2addr();
extern char *addr2sym();

extern void *try_sym2addr();
extern char *try_addr2sym();
