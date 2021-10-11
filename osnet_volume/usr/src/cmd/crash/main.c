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

#pragma ident	"@(#)main.c	1.28	99/09/21 SMI"

/*
 * This file contains code for the crash functions:  ?, help, redirect, and
 * quit, as well as the command interpreter.
 */

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/var.h>
#include <sys/user.h>
#include <setjmp.h>
#include <locale.h>
#include <sys/elf.h>
#include "crash.h"

#define	NARGS 25		/* number of arguments to one function */
#define	LINESIZE 256		/* size of function input line */

char	*namelist = "/dev/ksyms";
char	*dumpfile = "/dev/mem";
struct user	*ubp;		/* pointer to ublock buffer */
FILE	*fp;			/* output file pointer */
FILE	*rp;			/* redirect file pointer */
int	opipe = 0;		/* open pipe flag */
struct var	vbuf;		/* var structure buffer */
int	Procslot;		/* current process slot */
kthread_id_t	Curthread;	/* current thread */
int		Virtmode = 1;	/* virtual or physical mode flag */
void		(*pipesig)(int);	/* pipe signal catch */

Sym	*V, *Panic, *Start;	/* namelist symbol pointers */

jmp_buf		jmp, syn;	/* labels for jump */

/* function definition */
struct func {
	char	*name;
	char	*syntax;
	int	(*call) ();
	char	*description;
};

static int findstring(char *, char *);
static void pralias(struct func	*);

/* function calls */
static void getcmd();

static int	getfuncs(), getquit(), getredirect(), gethelp();
extern int	getas(), getbufhdr(), getbuffer(), getcallout(),
		getkfp(), getlcks(),
		getmblk(), getmblkusers(),
		getvfsarg(), getnm(), getod(),
		getpcb(), getproc(), getqrun(), getqueue(),
		getprnode(), getsnode(),
		getsocket(), getsoconfig(),
		getstack(), getstat(), getstream(),
		getstrstat(), gettrace(), getsymbol(), getthread(), gettty(),
		getuser(), getvar(), getvnode(), getvtop(),
		getbase(), getsearch(),
		getsearch(), getfile(), getdefproc(), getmode(),
		getmaj(), getsize(), getfindslot(), getfindaddr(), getvfssw(),
		getlinkblk(), getclass(),
		get_ufs_inode(), gettsdptbl(), getrtdptbl(),
		getdispq(), gettsproc(), getrtproc(), getkmastat(),
		getkmalog(), getkmausers(), getmutexinfo(),
		getrwlockinfo(), getsemainfo(), getlwp(),
		getnfsnode(), getcpu(), getdefthread(),
		getpcfsnode();

/* function table */
/* entries with NA description fields should be removed in next release */
static struct func	functab[] = {
	"as", "[-e] [-f] [-l] [-p] [-w filename] [proc[s]]",
	getas, "address space structures",
	"b", " ", getbuffer, "(buffer)",
	"base", "[-w filename] number[s]",
	getbase, "base conversions",
	"buf", " ", getbufhdr, "(bufhdr)",
	"buffer",
	"[-w filename] [-b|-c|-d|-x|-o|-i] (bufferslot |[-p] st_addr)",
	getbuffer, "buffer data",
	"bufhdr", "[-f] [-l] [-w filename] [[-p] tbl_entry[s]]",
	getbufhdr, "buffer headers",
	"c", " ", getcallout, "(callout)",
	"callout", "[-w filename]",
	getcallout, "callout table",
	"class", "[-w filename] [tbl_entry[s]]",
	getclass, "class table",
	"cpu", "[-w filename] cpu_addr",
	getcpu, "cpu structure",
	"defproc", "[-w filename] [-c | -r | slot]",
	getdefproc, "set default process slot",
	"defthread", "[-p] [-r] [-w filename] [-c address]",
	getdefthread, "set default thread",
	"dispq", "[-w filename] [tbl_entry[s]]",
	getdispq, "dispq table",
	"ds", "[-w filename] virtual_address[es]",
	getsymbol, "data address namelist search",
	"f", " ", getfile, "(file)",
	"file", "[-e] [-f] [-w filename] [[-p] address[es]]",
	getfile, "file table",
	"findaddr", "[-w filename] table slot",
	getfindaddr, "find address for given table and slot",
	"findslot", "[-w filename] virtual_address[es]",
	getfindslot, "find table and slot number for given address",
	"fs", " ", getvfssw, "(vfssw)",
	"help", "[-w filename] function[s]",
	gethelp, "help function",
	"kfp", "[-w filename] [thread_addr]",
	getkfp, "frame pointer for start of stack trace",
	"kmalog", "[-w filename] [slab|fail]",
	getkmalog, "kernel memory allocator transaction log",
	"kmastat", "[-w filename]",
	getkmastat, "kernel memory allocator statistics",
	"kmausers", "[-e] [-f] [-w filename] [cache name]",
	getkmausers, "kernel memory allocator users when KMF_AUDIT set",
	"l", " ", getlcks, "(lck)",
	"lck", "[-e] [-w filename] [[-p] tbl_entry[s]]",
	getlcks, "record lock tables",
	"linkblk", "[-e] [-w filename] [[-p] linkblk_addr[s]]",
	getlinkblk, "linkblk table",
	"lwp", "[-w filename] lwp_addr",
	getlwp, "lwp structure",
	"m", " ", getvfsarg, "(vfs)",
	"mblk", "[-e] [-f] [-w filename] [[-p] mblk_addr[s]]",
	getmblk, "allocated stream message block and data block headers",
	"mblkusers", "[-e] [-f] [-w filename]",
	getmblkusers, "mblk/dblk usage when KMF_AUDIT is enabled",
	"mode", "[-w filename] [v | p]",
	getmode, "address mode",
	"mount", " ", getvfsarg, "(vfs)",
	"mutex", "[-w filename] mutex_addr",
	getmutexinfo, "mutex structure",
	"nfs", " ", getnfsnode, "(nfsnode)",
	"nfsnode", "[-f] [-l] [-r] [-w filename] [[-p] st_addr]",
	getnfsnode, "display nfs remote nodes",
	"nm", "[-w filename] symbol[s]",
	getnm, "name search",
	"od",
	"[-w filename] [-c|-d|-x|-o|-a|-h] [-l|-i|-t|-b] "
		"[-s process] [-p] st_addr [count]",
	getod, "dump symbol values",
	"p", " ", getproc, "(proc)",
	"pcb", "[-w filename] [thread_addr]",
	getpcb, "process control block",
	"pcfsnode", "[-w filename] [node_address]",
	getpcfsnode, "display pcfs nodes",
	"prnode",
	"[-e] [-f] [-l] [-w filename] [[-p] tbl_entry[s]]",
	getprnode, "proc node",
	"proc",
	"[-e] [-f] [-l] [-w filename] "
		"[([-p] [-a] tbl_entry | #procid)... | -r]",
	getproc, "process table",
	"q", " ", getquit, "(quit)",
	"qrun", "[-w filename]",
	getqrun, "list of servicable stream queues",
	"queue", "[-e] [-f] [-w filename] [[-p] queue_addr[s]]",
	getqueue, "allocated stream queues",
	"quit", " ",
	getquit, "exit",
	"rd", " ", getod, "(od)",
	"redirect", "[-w filename] [-c | filename]",
	getredirect, "output redirection",
	"rtdptbl", "[-w filename] [tbl_entry[s]]",
	getrtdptbl, "real time dispatcher parameter table",
	"rtproc", "[-w filename]",
	getrtproc, "real time process table",
	"rwlock", "[-w filename] rwlock_addr",
	getrwlockinfo, "rwlock structure",
	"s", " ", getstack, "(stack)",
	"search",
	"[-w filename] [-m mask] [-s process] pattern [-p] st_addr length",
	getsearch, "memory search",
	"sema", "[-w filename] sema_addr",
	getsemainfo, "sema structure",
	"size", "[-x] [-w filename] structurename[s]",
	getsize, "symbol size",
	"snode", "[-e] [-f] [-l] [-w filename] [[-p] tbl_entry[s]]",
	getsnode, "special node",
	"socket", "[-e] [-f] [-l] [-w filename] [[-p] socket_addr[s]]",
	getsocket, "socket node",
	"soconfig", "[-f] [-l] [-w filename]",
	getsoconfig, "socket configuration",
	"stack", "[-w filename] [-u | -k] [-p] [thread]",
	getstack, "stack dump",
	"status", "[-w filename]",
	getstat, "system status",
	"stream", "[-e] [-f] [-w filename] [[-p] stream_addr[s]]",
	getstream, "allocated stream table slots",
	"strstat", "[-w filename]",
	getstrstat, "streams statistics",
	"t", " ", gettrace, "(trace)",
	"trace", "[-w filename] [[-p] thread_addr]",
	gettrace, "kernel stack trace",
	"thread", "[-e] [-f] [-l] [-w filename] slot number",
	getthread, "display thread table",
	"ts", "[-w filename] virtual_address[es]",
	getsymbol, "text address namelist search",
	"tsdptbl", "[-w filename] [tsdptbl_entry[s]]",
	gettsdptbl, "time sharing dispatcher parameter table",
	"tsproc", "[-w filename]",
	gettsproc, "time sharing process table",
	"tty",
	"[-e] [-f] [-w filename] [-l] "
		"[-t type [[-p] tbl_entry[s]] | [-p] st_addr]",
	gettty, "tty structures (valid types: pp, iu)",
	"u", " ", getuser, "(user)",
	"user", "[-e] [-f] [-l] [-w filename] [process]",
	getuser, "uarea",
	"ui", " ", get_ufs_inode, "(uinode)",
	"uinode", "[-d] [-e] [-f] [-l] [-r] [-w filename] [[-p] st_addr]",
	get_ufs_inode, "inode table",
	"v", " ", getvar, "(var)",
	"var", "[-w filename]",
	getvar, "system variables",
	"vfs", "[-f] [-w filename] [[-p] address[es]]",
	getvfsarg, "mounted vfs list",
	"vfssw", "[-f] [-w filename] [[-p] tbl_entry[s]]",
	getvfssw, "virtual file system switch table",
	"vnode", "[-w filename] [-l] [-p] vnode_addr[s]",
	getvnode, "vnode list",
	"vtop", "[-w filename] [-s process] st_addr[s]",
	getvtop, "virtual to physical address",
	"?", "[-w filename]",
	getfuncs, "print list of available commands",
	"!cmd", " ", NULL, "escape to shell",
	"hdr", " ", getbufhdr, "NA",
	"files", " ", getfile, "NA",
	"fp", " ", getkfp, "NA",
	"mnt", " ", getvfsarg, "NA",
	"dump", " ", getod, "NA",
	"ps", " ", getproc, "NA",
	"k", " ", getstack, "NA",
	"kernel", " ", getstack, "NA",
	"stk", " ", getstack, "NA",
	"ad", " ", gettty, "NA",
	"con", " ", gettty, "NA",
	"term", " ", gettty, "NA",
	"u_area", " ", getuser, "NA",
	"uarea", " ", getuser, "NA",
	"ublock", " ", getuser, "NA",
	"tunable", " ", getvar, "NA",
	"tunables", " ", getvar, "NA",
	"tune", " ", getvar, "NA",
	"calls", " ", getcallout, "NA",
	"call", " ", getcallout, "NA",
	"timeout", " ", getcallout, "NA",
	"time", " ", getcallout, "NA",
	"tout", " ", getcallout, "NA",
	NULL, NULL, NULL, NULL
};

char	*args[NARGS];		/* argument array */
int	argcnt;			/* argument count */
static char	outfile[100];	/* output file for redirection */
static int	tabsize;	/* size of function table */

void
main(int argc, char **argv)
{
	struct func	*a, *f;
	int		c, i, found;
	extern int	opterr;
	size_t		arglength;

	(void) setlocale(LC_ALL, "");
	if (setjmp(jmp))
		exit(1);
	fp = stdout;
	strcpy(outfile, "stdout");
	optind = 1;		/* remove in next release */
	opterr = 0;		/* suppress getopt error messages */

	for (tabsize = 0, f = functab; f->name; f++, tabsize++)
		if (strcmp(f->description, "NA") == 0)
			break;

	while ((c = getopt(argc, argv, "d:n:w:")) != EOF) {
		switch (c) {
			case 'd':
				dumpfile = optarg;
				break;
			case 'n':
				namelist = optarg;
				break;
			case 'w':
				strncpy(outfile, optarg, ARGLEN);
				if (!(rp = fopen_output(outfile)))
					fatal("unable to open %s\n", outfile);
				break;
			default:
				fatal("usage: crash [-d dumpfile] "
					"[-n namelist] [-w outfile]\n");
		}
	}
	/* backward compatible code */
	if (argv[optind]) {
		dumpfile = argv[optind++];
		if (argv[optind])
			namelist = argv[optind++];
		if (argv[optind])
			fatal("usage: crash [-d dumpfile] "
				"[-n namelist] [-w outfile]\n");
	}
	/* remove in SVnext release */
	if (rp)
		fprintf(rp, "dumpfile = %s, namelist = %s, outfile = %s\n",
			dumpfile, namelist, outfile);
	fprintf(fp, "dumpfile = %s, namelist = %s, outfile = %s\n",
						dumpfile, namelist, outfile);
	init();

	setjmp(jmp);  /* a call to error() will warp us back here */

	for (;;) {

		getcmd();
		if (argcnt == 0)
			continue;
		if (rp) {
			fp = rp;
			fprintf(fp, "\n> ");
			for (i = 0; i < argcnt; i++)
				fprintf(fp, "%s ", args[i]);
			fprintf(fp, "\n");
		}
		found = 0;
		for (f = functab; f->name; f++)
			if (strcmp(f->name, args[0]) == 0) {
				found = 1;
				break;
			}
		if (!found) {
			arglength = strlen(args[0]);
			for (f = functab; f->name; f++) {
				if (strcmp(f->description, "NA") == 0)
					break;	/* remove in next release */
				if (strncmp(f->name, args[0], arglength) == 0) {
					found++;
					a = f;
				}
			}
			if (found) {
				if (found > 1)
					error("%s is an ambiguous function "
						"name\n", args[0]);
				else
					f = a;
			}
		}
		if (found) {
			if (strcmp(f->description, "NA") == 0)
				pralias(f);
			if (setjmp(syn)) {
				while (getopt(argcnt, args, "") != EOF);
				if (*f->syntax == ' ') {
					for (a = functab; a->name; a++)
						if ((a->call == f->call) &&
						    (*a->syntax != ' '))
							error("%s: usage: "
								"%s %s\n",
								f->name,
								f->name,
								a->syntax);
				} else
					error("%s: usage: %s %s\n",
						f->name, f->name, f->syntax);
			} else
				(*(f->call)) ();
		} else
			prerrmes("unrecognized function name\n");
		fflush(fp);
		resetfp();
	}
}

/* returns argcnt, and args contains all arguments */
static void
getcmd()
{
	char	*p;
	int	i;
	static char	line[LINESIZE + 1];
	FILE	*ofp;

	ofp = fp;
	printf("> ");
	fflush(stdout);
	if (fgets(line, LINESIZE, stdin) == NULL)
		exit(0);
	line[LINESIZE] = '\n';
	p = line;
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	if (*p == '!') {
		system(p + 1);
		argcnt = 0;
	} else {
		for (i = 0; i < NARGS; i++) {
			if (*p == '\n') {
				*p = '\0';
				break;
			}
			while (*p == ' ' || *p == '\t')
				p++;
			args[i] = p;
			if (strlen(args[i]) == 1)
				break;
			if (*p == '!') {
				p = args[i];
				if (strlen(++args[i]) == 1)
					error("no shell command after '!'\n");
				pipesig = signal(SIGPIPE, SIG_IGN);
				if ((fp = popen(++p, "w")) == NULL) {
					fp = ofp;
					error("cannot open pipe\n");
				}
				if (rp != NULL)
					error("cannot use pipe with "
						"redirected output\n");
				opipe = 1;
				break;
			}
			if (*p == '(')
				while ((*p != ')') && (*p != '\n'))
					p++;
			while (*p != ' ' && *p != '\n')
				p++;
			if (*p == ' ' || *p == '\t')
				*p++ = '\0';
		}
		args[i] = NULL;
		argcnt = i;
	}
}


static void
prfuncs()
{
	int	i, j, len;
	struct func *ff;
	char	tempbuf[20];

	len = (tabsize + 3) / 4;
	for (i = 0; i < len; i++) {
		ff = functab + i;
		for (j = 0; j < 4; j++) {
			if (*ff->description != '(')
				fprintf(fp, "%-15s", ff->name);
			else {
				tempbuf[0] = 0;
				strcat(tempbuf, ff->name);
				strcat(tempbuf, " ");
				strcat(tempbuf, ff->description);
				fprintf(fp, "%-15s", tempbuf);
			}
			ff += len;
			if ((ff - functab) >= tabsize)
				break;
		}
		fprintf(fp, "\n");
	}
	fprintf(fp, "\n");
}

/* get arguments for ? function */
static int
getfuncs()
{
	int	c;

	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w':
				redirect();
				break;
			default:
				longjmp(syn, 0);
		}
	}
	prfuncs();
	return (0);
}

/* print all function names in columns */

/* print function information */
static void
prhelp(string)
	char	*string;
{
	int	found = 0;
	struct func	*ff, *a, *aa;

	for (ff = functab; ff->name; ff++) {
		if (strcmp(ff->name, string) == 0) {
			found = 1;
			break;
		}
	}
	if (!found)
		error("%s does not match in function list\n", string);
	if (strcmp(ff->description, "NA") == 0)	/* remove in next release */
		pralias(ff);
	if (*ff->description == '(') {
		for (a = functab; a->name != NULL; a++)
			if ((a->call == ff->call) && (*a->description != '('))
				break;
		fprintf(fp, "%s %s\n", ff->name, a->syntax);
		if (findstring(a->syntax, "tbl_entry"))
			fprintf(fp, "\ttbl_entry = slot number | address "
				"| symbol | expression | range\n");
		if (findstring(a->syntax, "st_addr"))
			fprintf(fp,
				"\tst_addr = address | symbol | expression\n");
		fprintf(fp, "%s\n", a->description);
	} else {
		fprintf(fp, "%s %s\n", ff->name, ff->syntax);
		if (findstring(ff->syntax, "tbl_entry"))
			fprintf(fp, "\ttbl_entry = slot number | address "
				"| symbol | expression | range\n");
		if (findstring(ff->syntax, "st_addr"))
			fprintf(fp,
				"\tst_addr = address | symbol | expression\n");
		fprintf(fp, "%s\n", ff->description);
	}
	fprintf(fp, "alias: ");
	for (aa = functab; aa->name != NULL; aa++)
		if ((aa->call == ff->call) && (strcmp(aa->name, ff->name)) &&
		    strcmp(aa->description, "NA"))
			fprintf(fp, "%s ", aa->name);
	fprintf(fp, "\n");
	fprintf(fp, "\tacceptable aliases are uniquely identifiable "
		"initial substrings\n");
}

/* get arguments for help function */
static int
gethelp()
{
	int	c;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w':
				redirect();
				break;
			default:
				longjmp(syn, 0);
		}
	}
	if (args[optind]) {
		do {
			prhelp(args[optind++]);
		} while (args[optind]);
	} else
		prhelp("help");
	return (0);
}

/* find tbl_entry or st_addr in syntax string */
static int
findstring(syntax, substring)
	char	*syntax;
	char	*substring;
{
	char	string[81];
	char	*token;

	strcpy(string, syntax);
	token = strtok(string, "[] ");
	while (token) {
		if (strcmp(token, substring) == 0)
			return (1);
		token = strtok(NULL, "[] ");
	}
	return (0);
}

/* this function and all obsolete aliases should be removed in next release */
/* print valid function names for obsolete aliases */
static void
pralias(ff)
	struct func	*ff;
{
	struct func	*a;

	fprintf(fp, "Valid calls to this function are:  ");
	for (a = functab; a->name; a++)
		if ((a->call == ff->call) && (strcmp(a->name, ff->name)) &&
		    (strcmp(a->description, "NA")))
			fprintf(fp, "%s ", a->name);
	error("\nThe alias %s is not supported on this processor\n",
		ff->name);
}


/* terminate crash session */
static int
getquit()
{
	if (rp)
		fclose(rp);
	exit(0);
	return (0);
}

/* print results of redirect function */
static void
prredirect(char *string, int close)
{
	if (close)
		if (rp) {
			fclose(rp);
			rp = NULL;
			strcpy(outfile, "stdout");
			fp = stdout;
		}
	if (string) {
		if (rp) {
			fclose(rp);
			rp = NULL;
		}
		if (!(rp = fopen_output(string)))
			error("unable to open %s\n", string);
		fp = rp;
		strncpy(outfile, string, ARGLEN);
	}
	fprintf(fp, "outfile = %s\n", outfile);
	if (rp)
		fprintf(stdout, "outfile = %s\n", outfile);
}

/* get arguments for redirect function */
static int
getredirect()
{
	int	c;
	int	close = 0;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:c")) != EOF) {
		switch (c) {
			case 'w':
				redirect();
				break;
			case 'c':
				close = 1;
				break;
			default:
				longjmp(syn, 0);
		}
	}
	if (args[optind])
		prredirect(args[optind], close);
	else
		prredirect(NULL, close);
	return (0);
}
