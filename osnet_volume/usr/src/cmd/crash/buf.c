/* Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	 */
/* All Rights Reserved	*/

/* THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/* The copyright notice above does not evidence any	*/
/* actual or intended publication of such source code.	*/

/*
 * Copyright (C) 1986,1991, 1996 by Sun Microsystems, Inc
 * All rights reserved.
 *
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

#pragma ident	"@(#)buf.c	1.15	98/06/03 SMI"

/*
 * This file contains code for the crash functions: bufhdr, buffer, od.
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <sys/var.h>
#include <sys/proc.h>
#include <sys/fs/ufs_inode.h>
#include <sys/buf.h>
#include "crash.h"

#define	BSZ  1			/* byte size */
#define	SSZ  sizeof (short)	/* short size */
#define	ISZ  sizeof (int)	/* int size */
#define	LSZ  sizeof (long)	/* long size */

#define	DATE_FMT	"%a %b %e %H:%M:%S %Y\n"

/*
 * %a	abbreviated weekday name %b	abbreviated month name %e	day
 * of month %H	hour %M	minute %S	second %Y	year
 */

#define	SBUFSIZE	8192
#define	SBUFINTSZ	(SBUFSIZE/sizeof (int))
#define	UFS_INOPB		SBUFSIZE/sizeof (struct dinode)


static void *Buf;		/* symbol address */
static char bformat = 'x';	/* buffer format */
static int type = LSZ;		/* od type */
static char mode = 'x';		/* od mode */
/* Declare buffer as int to force word alignment */
static int  buffer[SBUFINTSZ];	/* buffer buffer */
static char time_buf[50];	/* holds date and time string */
static struct buf bbuf;		/* used by buffer for bufhdr */
static struct buf bhbuf;	/* used by prbufhdr */

static void prbinode(void);
static void prod(intptr_t, long, int, long);
static void prbalpha(void);
static void prbnum(void);
static void prbuffer(int, void *);
static void prbufhdr(int, int, void *, char *, int);

/* get arguments for bufhdr function */
int
getbufhdr(void)
{
	struct hbuf hbuf;
	struct buf *dp, *bp;
	int i;

	void *addr;
	int full = 0;
	int phys = 0;
	int lock = 0;
	int c;

	char *heading = "SLOT       MAJ/MIN     BLOCK  ADDRESS FOR      "
	    "BCK      AVF      AVB      FLAGS\n";

	readsym("hbuf", &Buf, sizeof (void *));

	optind = 1;
	while ((c = getopt(argcnt, args, "flpw:")) != EOF) {
		switch (c) {
			case 'f':
				full = 1;
				break;
			case 'w':
				redirect();
				break;
			case 'l':
				lock = 1;
				break;
			case 'p':
				phys = 1;
				break;
			default:
				longjmp(syn, 0);
		}
	}
	fprintf(fp, "BUFFER HEADER TABLE SIZE = %d\n", vbuf.v_buf);
	if (!full)
		fprintf(fp, "%s", heading);
	if (args[optind]) {
		do {
			if ((addr = (void *)strcon(args[optind], 'h')) !=
			    (void *)-1)
				prbufhdr(full, phys, addr, heading, lock);
		} while (args[++optind]);
	} else {
		for (i = 0; i < vbuf.v_hbuf; i++) {
			dp = (struct buf *)&((struct hbuf *)Buf)[i];
			readbuf(dp, 0, phys,
			    &hbuf, sizeof (hbuf), "buffer hash head");
			for (bp = hbuf.b_forw; bp != dp; bp = bhbuf.b_forw)
				prbufhdr(full, phys, bp, heading, lock);
		}
	}
	return (0);
}


/* print buffer headers */
static void
prbufhdr(int full, int phys, void *addr, char *heading, int lock)
{
	int b_flags;
	int procslot;

	readbuf(addr, -1, phys, &bhbuf, sizeof (bhbuf), "buffer header");

	if (full)
		fprintf(fp, "%s", heading);
	fprintf(fp, "%p", addr);
	fprintf(fp, " %4u,%-5u %8x %8p",
		getemajor(bhbuf.b_dev) & L_MAXMAJ,
		geteminor(bhbuf.b_dev),
		bhbuf.b_blkno,
		bhbuf.b_un.b_addr);
	fprintf(fp, " %6lx %6lx",
		(intptr_t)bhbuf.b_forw,
		(intptr_t)bhbuf.b_back);
	fprintf(fp, " %6lx %6lx",
		(intptr_t)bhbuf.av_forw,
		(intptr_t)bhbuf.av_back);
	b_flags = bhbuf.b_flags;
	fprintf(fp, "%s%s%s%s%s%s%s%s%s%s%s\n",
		b_flags & B_WRITE ? " write" : "",
		b_flags & B_READ ? " read" : "",
		b_flags & B_DONE ? " done" : "",
		b_flags & B_ERROR ? " error" : "",
		b_flags & B_BUSY ? " busy" : "",
		b_flags & B_PHYS ? " phys" : "",
		b_flags & B_WANTED ? " wanted" : "",
		b_flags & B_AGE ? " age" : "",
		b_flags & B_ASYNC ? " async" : "",
		b_flags & B_DELWRI ? " delwri" : "",
		b_flags & B_STALE ? " stale" : "");
	if (full) {
		fprintf(fp, "\tBCNT ERR RESI   START  PROC  RELTIME\n");
		fprintf(fp, "\t%4ld %3d %4ld %8x",
			bhbuf.b_bcount,
			bhbuf.b_error,
			bhbuf.b_resid,
			bhbuf.b_start);
		procslot = proc_to_slot((long)bhbuf.b_proc);
		if (procslot == -1)
			fprintf(fp, "  - ");
		else
			fprintf(fp, " %4d", procslot);
		fprintf(fp, " %8x\n", bhbuf.b_reltime);
		fprintf(fp, "\n");
	}
	if (lock) {
		fprintf(fp, "\nLock information:\n");
		prsema(&(bhbuf.b_sem));
		prsema(&(bhbuf.b_io));
	}
}

/* get arguments for buffer function */
int
getbuffer(void)
{
	int phys = 0;
	int fflag = 0;
	void *addr;
	long arg1 = -1;
	long arg2 = -1;
	int c;

	optind = 1;
	while ((c = getopt(argcnt, args, "bcdrxiopw:")) != EOF) {
		switch (c) {
			case 'w':
				redirect();
				break;
			case 'p':
				phys = 1;
				break;
			case 'b':
				bformat = 'b';
				fflag++;
				break;
			case 'c':
				bformat = 'c';
				fflag++;
				break;
			case 'd':
				bformat = 'd';
				fflag++;
				break;
			case 'x':
				bformat = 'x';
				fflag++;
				break;
			case 'i':
				bformat = 'i';
				fflag++;
				break;
			case 'o':
				bformat = 'o';
				fflag++;
				break;
			default:
				longjmp(syn, 0);
				break;
		}
	}
	if (fflag > 1)
		longjmp(syn, 0);
	if (args[optind]) {
		getargs(vbuf.v_buf, &arg1, &arg2, phys);
		if (arg1 != -1) {
			addr = (void *)arg1;
			prbuffer(phys, addr);
		}
	} else
		longjmp(syn, 0);
	return (0);
}


/* print buffer */
static void
prbuffer(int phys, void *addr)
{
	size_t sz;

	readbuf(addr, -1, phys, &bbuf, sizeof (bbuf), "buffer");

	if (bbuf.b_un.b_addr == 0) {
		fprintf(fp, "	-\n");
		return;
	}
	sz = bbuf.b_bcount > sizeof (buffer) ? sizeof (buffer) : bbuf.b_bcount;
	readmem(bbuf.b_un.b_addr, 1, buffer, sz, "buffer");
	switch (bformat) {
		case 'b':
			prbalpha();
			break;
		case 'c':
			prbalpha();
			break;
		case 'd':
			prbnum();
			break;
		case 'x':
			prbnum();
			break;
		case 'i':
			prbinode();
			break;
		case 'o':
			prbnum();
			break;
		default:
			error("unknown format\n");
			break;
	}
}

/* print buffer in numerical format */
static void
prbnum(void)
{
	int *ip, i;

	for (i = 0, ip = (int *)buffer; ip != (int *)&buffer[SBUFINTSZ];
	    i++, ip++) {
		if (i % 4 == 0)
			fprintf(fp, "\n%5.5x:\t", i * 4);
		fprintf(fp, bformat == 'o' ? " %11.11o" :
			bformat == 'd' ? " %10.10u" : " %8.8x", *ip);
	}
	fprintf(fp, "\n");
}


/* print buffer in character format */
static void
prbalpha(void)
{
	char *cp;
	int i;

	for (i = 0, cp = (char *)buffer; cp != (char *)&buffer[SBUFINTSZ];
		i++, cp++) {
		if (i % (bformat == 'c' ? 16 : 8) == 0)
			fprintf(fp, "\n%5.5x:\t", i);
		if (bformat == 'c')
			putch(*cp);
		else
			fprintf(fp, " %4.4o", *cp & 0377);
	}
	fprintf(fp, "\n");
}


/* print buffer in inode format */
static void
prbinode(void)
{
	struct inode *uip;
	int i;
	time_t t;

	for (i = 1, uip = (struct inode *)buffer;
	    uip < (struct inode *)&buffer[SBUFINTSZ]; i++, uip++) {
		fprintf(fp, "\ni#: %ld  md: ", (bbuf.b_blkno - 2) *
			UFS_INOPB + i);
		switch (uip->i_mode & IFMT) {
			case IFCHR:
				fprintf(fp, "c");
				break;
			case IFBLK:
				fprintf(fp, "b");
				break;
			case IFDIR:
				fprintf(fp, "d");
				break;
			case IFREG:
				fprintf(fp, "f");
				break;
			case IFSOCK:
				fprintf(fp, "s");
				break;
			case IFIFO:
				fprintf(fp, "p");
				break;
			default:
				fprintf(fp, "-");
				break;
		}
		fprintf(fp, "\n%s%s%s%3x",
			uip->i_mode & ISUID ? "u" : "-",
			uip->i_mode & ISGID ? "g" : "-",
			uip->i_mode & ISVTX ? "t" : "-",
			uip->i_mode & 0777);
		fprintf(fp, "  ln: %u  uid: %u  gid: %u  sz: %lld",
			uip->i_nlink, uip->i_uid,
			uip->i_gid, uip->i_size);
		if ((uip->i_mode & IFMT) == IFCHR ||
		    (uip->i_mode & IFMT) == IFBLK ||
		    (uip->i_mode & IFMT) == IFIFO)
			fprintf(fp, "\nmaj: %d  min: %1.1o\n",
				getemajor(uip->i_dev),
				geteminor(uip->i_dev));

		t = (time_t)uip->i_atime;
		cftime(time_buf, DATE_FMT, &t);
		fprintf(fp, "\nat: %s", time_buf);
		t = (time_t)uip->i_mtime;
		cftime(time_buf, DATE_FMT, &t);
		fprintf(fp, "mt: %s", time_buf);
		t = (time_t)uip->i_ctime;
		cftime(time_buf, DATE_FMT, &t);
		fprintf(fp, "ct: %s", time_buf);
	}
	fprintf(fp, "\n");
}

/* get arguments for od function */
int
getod(void)
{
	int phys = 0;
	long count = 1;
	long proc = 0;
	intptr_t addr = -1;
	int c;
	Sym *sp;
	int typeflag = 0;
	int modeflag = 0;

	optind = 1;
	while ((c = getopt(argcnt, args, "tilxcbdohapw:s:")) != EOF) {
		switch (c) {
			case 'w':
				redirect();
				break;
			case 's':
				proc = setproc();
				break;
			case 'p':
				phys = 1;
				break;
			case 'c':
				mode = 'c';
				if (!typeflag)
					type = BSZ;
				modeflag++;
				break;
			case 'a':
				mode = 'a';
				if (!typeflag)
					type = BSZ;
				modeflag++;
				break;
			case 'x':
				mode = 'x';
				if (!typeflag)
					type = LSZ;
				modeflag++;
				break;
			case 'd':
				mode = 'd';
				if (!typeflag)
					type = LSZ;
				modeflag++;
				break;
			case 'o':
				mode = 'o';
				if (!typeflag)
					type = LSZ;
				modeflag++;
				break;
			case 'h':
				mode = 'h';
				type = LSZ;
				typeflag++;
				modeflag++;
				break;
			case 'b':
				type = BSZ;
				typeflag++;
				break;
			case 't':
				type = SSZ;
				typeflag++;
				break;
			case 'i':
				type = ISZ;
				typeflag++;
				break;
			case 'l':
				type = LSZ;
				typeflag++;
				break;
			default:
				longjmp(syn, 0);
		}
	}
	if (typeflag > 1)
		error("only one type may be specified:  b, t, i, or l\n");
	if (modeflag > 1)
		error("only one mode may be specified:  a, c, o, d, or x\n");
	if (args[optind]) {
		if (*args[optind] == '(')
			addr = eval(++args[optind]);
		else if (sp = symsrch(args[optind]))
			addr = sp->st_value;
		else if (isasymbol(args[optind]))
			error("%s not found in symbol table\n", args[optind]);
		else
			addr = strcon(args[optind], 'h');
		if (addr == -1)
			error("\n");
		if (args[++optind])
			if ((count = strcon(args[optind], 'd')) == -1)
				error("\n");
		prod(addr, count, phys, proc);
	} else
		longjmp(syn, 0);
	return (0);
}

/* print dump */
static void
prod(intptr_t addr, long count, int phys, long proc)
{
	int j, i;
	long value;
	char *format;
	int precision;
	char hexchar[16];
	uint8_t value_byte[LSZ];
	char *cp;
	int nbytes;
	pid_t pid;
	int amode;

	if (phys || !Virtmode) {
		amode = 0;
	} else {
		pid = slot_to_pid(proc);
		if (pid == 0 || pid == 2 || pid == 3) {
			amode = 1;
		} else {
			(void) kvm_getproc(kd, pid);
			amode = -1;
		}
	}

	if (mode == 'h') {
		cp = hexchar;
		nbytes = 0;
	}

	for (i = 0; i < count; i++, addr += type) {
		readmem((void *)addr, amode, value_byte, type, "od data");
		value = 0;
#ifdef _BIG_ENDIAN
		for (j = 0; j < type; j++)
#else
		for (j = type - 1; j >= 0; j--)
#endif
			value = (value << 8) + value_byte[j];
		if (((mode == 'c') && ((i % 16) == 0)) ||
		    ((mode != 'a') && (mode != 'c') && (i % 4 == 0))) {
			if (i != 0) {
				if (mode == 'h') {
					fprintf(fp, "   ");
					for (j = 0; j < nbytes; j++) {
						if (hexchar[j] < 040 ||
						    hexchar[j] > 0176)
							fprintf(fp, ".");
						else
							fprintf(fp, "%c",
								hexchar[j]);
					}
					cp = hexchar;
					nbytes = 0;
				}
				fprintf(fp, "\n");
			}
			fprintf(fp, "%8.8p:  ", addr);
		}
		switch (mode) {
			case 'a':
				putc((char)value, fp);
				break;
			case 'c':
				putch((char)value);
				break;
			case 'o':
				format = "%.*lo   ";
				precision = 3 * type;
				fprintf(fp, format, precision, value);
				break;
			case 'd':
				format = "%.*ld   ";
				precision = (5 * type + 1) / 2;
				fprintf(fp, format, precision, value);
				break;
			case 'x':
				format = "%.*lx   ";
				precision = 2 * type;
				fprintf(fp, format, precision, value);
				break;
			case 'h':
				fprintf(fp, "%.16lx   ", value);
				*((long *)cp) = value;
				cp += sizeof (long);
				nbytes += sizeof (long);
				break;
		}
	}
	if (mode == 'h') {
		if (i % 4 != 0)
			for (j = 0; (j + (i % 4)) < 4; j++)
				fprintf(fp, "           ");
		fprintf(fp, "   ");
		for (j = 0; j < nbytes; j++)
			if (hexchar[j] < 040 || hexchar[j] > 0176)
				fprintf(fp, ".");
			else
				fprintf(fp, "%c", hexchar[j]);
	}
	fprintf(fp, "\n");
}
