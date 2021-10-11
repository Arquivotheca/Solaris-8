/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)m4.h	6.10	95/04/25 SMI"

#include	<ctype.h>
#include	<locale.h>

#define	EOS	'\0'
#define	LOW7	0177
#define	MAXSYM	5
#define	PUSH	1
#define	NOPUSH	0
#define	OK	0
#define	NOT_OK	1

#define	putbak(c)	(ip < ibuflm? (*ip++ = (c)): error2(gettext(\
	"pushed back more than %d chars"), bufsize))
#define	stkchr(c)	(op < obuflm? (*op++ = (c)): error2(gettext(\
	"more than %d chars of argument text"), bufsize))
#define	sputchr(c, f)	(putc(c, f) == '\n'? lnsync(f): 0)
#define	putchr(c)	(Cp?stkchr(c):cf?(sflag?sputchr(c, cf):putc(c, cf)):0)

struct bs {
	int	(*bfunc)();
	char	*bname;
};

struct	call {
	char	**argp;
	int	plev;
};

struct	nlist {
	char	*name;
	char	*def;
	char	tflag;
	struct	nlist *next;
};

struct Wrap {
	char *wrapstr;
	struct Wrap *nxt;
};

extern FILE	*cf;
extern FILE	*ifile[];
extern FILE	*ofile[];
extern FILE	*xfopen();
extern FILE	*m4open();
extern char	**Ap;
extern char	**argstk;
extern char	*astklm;
extern char	*inpmatch();
extern char	*chkbltin();
extern char	*calloc();
extern char	*xcalloc();
extern char	*copy();
extern char	*mktemp();
extern char	*strcpy();
extern char	*fname[];
extern char	*ibuf;
extern char	*ibuflm;
extern char	*ip;
extern char	*ipflr;
extern char	*ipstk[10];
extern char	*obuf;
extern char	*obuflm;
extern char	*op;
extern char	*procnam;
extern char	*tempfile;
extern char	*token;
extern char	*toklm;
extern int	C;
extern int	getchr();
extern char	fnbuf[];
extern char	lcom[];
extern char	lquote[];
extern char	nullstr[];
extern char	rcom[];
extern char	rquote[];
extern char	type[];
extern int	bufsize;
extern void	catchsig();
extern int	fline[];
extern int	hshsize;
extern int	hshval;
extern int	ifx;
extern int	nflag;
extern int	ofx;
extern int	sflag;
extern int	stksize;
extern int	sysrval;
extern int	toksize;
extern int	trace;
extern int	exitstat;
extern long	ctol();
extern void	chkspace();
extern void	showwrap();
extern struct bs	barray[];
extern struct call	*Cp;
extern struct call	*callst;
extern struct nlist	**hshtab;
extern struct nlist	*install();
extern struct nlist	*lookup();
extern struct Wrap	*wrapstart;
