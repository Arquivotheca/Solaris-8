/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)m4.c	6.21	97/04/28 SMI"

#include	<stdio.h>
#include	<signal.h>
#include	"m4.h"

#define	match(c, s)	(c == *s && (!s[1] || inpmatch(s+1)))


int	prev_char;

main(argc, argv)
int	argc;
char 	**argv;
{
	register t;
	int opt_end = 0;

#ifdef gcos
	tempfile = "m4*tempa";
#endif

#ifdef unix
	{
	static	sigs[] = {SIGHUP, SIGINT, SIGPIPE, 0};
	for (t = 0; sigs[t]; ++t)
		if (signal(sigs[t], SIG_IGN) != SIG_IGN)
			(void) signal(sigs[t], catchsig);
	}
	tempfile = mktemp("/tmp/m4aXXXXX");
	close(creat(tempfile, 0));
#endif

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	procnam = argv[0];
	getflags(&argc, &argv, &opt_end);
	initalloc();

	setfname("-");
	if (argc > 1) {
		--argc;
		++argv;
		if (strcmp(argv[0], "-")) {
			ifile[ifx] = m4open(&argv, "r", &argc);
			setfname(argv[0]);
		}
	}

	for (;;) {
		token[0] = t = getchr();
		token[1] = EOS;

		if (t == EOF) {
			if (ifx > 0) {
				fclose(ifile[ifx]);
				ipflr = ipstk[--ifx];
				continue;
			}

			getflags(&argc, &argv, &opt_end);

			if (argc <= 1)
				/*
				 * If dowrap() has been called, the m4wrap
				 * macro has been processed, and a linked
				 * list of m4wrap strings has been created.
				 * The list starts at wrapstart.
				 */
				if (wrapstart) {
					/*
					 * Now that EOF has been processed,
					 * display the m4wrap strings.
					 */
					showwrap();
					continue;
				} else
					break;
			--argc;
			++argv;

			if (ifile[ifx] != stdin)
				fclose(ifile[ifx]);

			if (strcmp(argv[0], "-"))
				ifile[ifx] = m4open(&argv, "r", &argc);
			else
				ifile[ifx] = stdin;

			setfname(argv[0]);
			continue;
		}

		if (isalpha(t) || t == '_') {
			register char	*tp = token+1;
			register	tlim = toksize;
			struct nlist	*macadd;  /* temp variable */

			while (isalnum(*tp = getchr()) || *tp == '_') {
				tp++;
				if (--tlim <= 0)
					error2(gettext(
					"more than %d chars in word"),
							toksize);
			}
			putbak(*tp);
			*tp = EOS;

			macadd = lookup(token);
			*Ap = (char *) macadd;
			if (macadd->def) {
				if ((char *) (++Ap) >= astklm) {
					--Ap;
					error2(gettext(
					"more than %d items on argument stack"),
					stksize);
				}

				if (Cp++ == NULL)
					Cp = callst;

				Cp->argp = Ap;
				*Ap++ = op;
				puttok(token);
				stkchr(EOS);
				t = getchr();
				putbak(t);

				if (t != '(')
					pbstr("()");
				else	/* try to fix arg count */
					*Ap++ = op;

				Cp->plev = 0;
			} else {
				puttok(token);
			}
		} else if (match(t, lquote)) {
			register	qlev = 1;

			for (;;) {
				token[0] = t = getchr();
				token[1] = EOS;

				if (match(t, rquote)) {
					if (--qlev > 0)
						puttok(token);
					else
						break;
				} else if (match(t, lquote)) {
					++qlev;
					puttok(token);
				} else {
					if (t == EOF)
						error(gettext(
						"EOF in quote"));
					putchr(t);
				}
			}
		} else if (match(t, lcom) && (strcmp(lcom, "#") ||
			prev_char != '$')) {

			/*
                        ** Don't expand commented macro (between lcom and
                        ** rcom).
                        ** What we know so far is that we have found the
                        ** left comment char (lcom).
                        ** Make sure we haven't found '#' (lcom) immediately
                        ** preceded by '$' because we want to expand "$#".
                        */

			puttok(token);
			for (;;) {
				token[0] = t = getchr();
				token[1] = EOS;
				if (match(t, rcom)) {
					puttok(token);
					break;
				} else {
					if (t == EOF)
						error(gettext(
						"EOF in comment"));
					putchr(t);
				}
			}
		} else if (Cp == NULL) {
			putchr(t);
		} else if (t == '(') {
			if (Cp->plev)
				stkchr(t);
			else {
				/* skip white before arg */
				while (isspace(t = getchr()))
					;

				putbak(t);
			}

			++Cp->plev;
		} else if (t == ')') {
			--Cp->plev;

			if (Cp->plev == 0) {
				stkchr(EOS);
				expand(Cp->argp, Ap-Cp->argp-1);
				op = *Cp->argp;
				Ap = Cp->argp-1;

				if (--Cp < callst)
					Cp = NULL;
			} else
				stkchr(t);
		} else if (t == ',' && Cp->plev <= 1) {
			stkchr(EOS);
			*Ap = op;

			if ((char *) (++Ap) >= astklm) {
				--Ap;
				error2(gettext(
					"more than %d items on argument stack"),
					stksize);
			}

			while (isspace(t = getchr()))
				;

			putbak(t);
		} 
		else {
			stkchr(t);
		}
	}

	if (Cp != NULL)
		error(gettext(
		"EOF in argument list"));

	delexit(exitstat, 1);
}

char
*inpmatch(s)
register char	*s;
{
	register char	*tp = token+1;

	while (*s) {
		*tp = getchr();

		if (*tp++ != *s++) {
			*tp = EOS;
			pbstr(token+1);
			return (0);
		}
	}

	*tp = EOS;
	return (token);
}

getflags(xargc, xargv, option_end)
register int	*xargc;
register char 	***xargv;
int *option_end;
{
	char	*arg;

	while (*xargc > 1) {
		arg = (*xargv)[1]; /* point arg to current argument */

		/*
		 * This argument is not an option if it equals "-" or if
		 * "--" has already been parsed.
		 */
		if (arg[0] != '-' || arg[1] == EOS || *option_end)
			break;
		if (arg[0] == '-' && arg[1] == '-' && arg[2] == '\0') {
			*option_end = 1;
		} else {
			switch (arg[1]) {
			case 'B':
				chkspace(&arg, xargc, xargv);
				bufsize = atoi(&arg[2]);
				break;
			case 'D':
				{
				register char *t;
				char *s[2];

				initalloc();

				chkspace(&arg, xargc, xargv);
				for (t = s[0] = &arg[2]; *t; t++)
					if (*t == '=') {
						*t++ = EOS;
						break;
					}

				s[1] = t;
				dodef(&s[-1], 2);
				break;
				}
			case 'H':
				chkspace(&arg, xargc, xargv);
				hshsize = atoi(&arg[2]);
				break;
			case 'S':
				chkspace(&arg, xargc, xargv);
				stksize = atoi(&arg[2]);
				break;
			case 'T':
				chkspace(&arg, xargc, xargv);
				toksize = atoi(&arg[2]);
				break;
			case 'U':
				{
				char *s[1];

				initalloc();
				chkspace(&arg, xargc, xargv);
				s[0] = &arg[2];
				doundef(&s[-1], 1);
				break;
				}
			case 'e':
#ifdef unix
				setbuf(stdout, (char *) NULL);
				signal(SIGINT, SIG_IGN);
#endif
				break;
			case 's':
				/* turn on line sync */
				sflag = 1;
				break;
			default:
				fprintf(stderr, gettext(
					"%s: bad option: %s\n"),
					procnam, arg);
				delexit(NOT_OK, 0);
			}
		} /* end else not "--" */

		(*xargv)++;
		--(*xargc);
	} /* end while options to process */
}

/*
 * Function: chkspace
 *
 * If there is a space between the option and its argument,
 * adjust argptr so that &arg[2] will point to beginning of the option argument.
 * This will ensure that processing in getflags() will work, because &arg[2]
 * will point to the beginning of the option argument whether or not we have
 * a space between the option and its argument.  If there is a space between
 * the option and its argument, also adjust xargv and xargc because we are
 * processing the next argument.
 */
void
chkspace(argptr, xargc, xargv)
char		**argptr;
register int	*xargc;
register char 	***xargv;
{
	if ((*argptr)[2] == EOS) {
		/* there is a space between the option and its argument */
		(*xargv)++; /* look at the next argument */
		--(*xargc);
		/*
		 * Adjust argptr if the option is followed by an
		 * option argument.
		 */
		if (*xargc > 1) {
			*argptr = (*xargv)[1];
			/* point &arg[2] to beginning of option argument */
			*argptr -= 2;
		}
	}
}

initalloc()
{
	static	done = 0;
	register	t;

	if (done++)
		return;

	hshtab = (struct nlist **) xcalloc(hshsize, sizeof (struct nlist *));
	callst = (struct call *) xcalloc(stksize/3+1, sizeof (struct call));
	Ap = argstk = (char **) xcalloc(stksize+3, sizeof (char *));
	ipstk[0] = ipflr = ip = ibuf = xcalloc(bufsize+1, sizeof (char));
	op = obuf = xcalloc(bufsize+1, sizeof (char));
	token = xcalloc(toksize+1, sizeof (char));

	astklm = (char *) (&argstk[stksize]);
	ibuflm = &ibuf[bufsize];
	obuflm = &obuf[bufsize];
	toklm = &token[toksize];

	for (t = 0; barray[t].bname; ++t) {
		static char	p[2] = {0, EOS};

		p[0] = t|~LOW7;
		install(barray[t].bname, p, NOPUSH);
	}
#ifdef unix
	install("unix", nullstr, NOPUSH);
#endif

#ifdef gcos
	install("gcos", nullstr, NOPUSH);
#endif
}

struct nlist	*
install(nam, val, mode)
char	*nam;
register char	*val;
{
	register struct nlist *np;
	register char	*cp;
	int		l;

	if (mode == PUSH)
		lookup(nam);	/* lookup sets hshval */
	else
		while (undef(nam))	/* undef calls lookup */
			;

	np = (struct nlist *) xcalloc(1, sizeof (*np));
	np->name = copy(nam);
	np->next = hshtab[hshval];
	hshtab[hshval] = np;

	cp = xcalloc((l = strlen(val))+1, sizeof (*val));
	np->def = cp;
	cp = &cp[l];

	while (*val)
		*--cp = *val++;
}

struct nlist	*
lookup(str)
char 	*str;
{
	unsigned char		*s1;
	register struct nlist	*np;
	static struct nlist	nodef;

	s1 = (unsigned char *)str;

	for (hshval = 0; *s1; )
		hshval += *s1++;

	hshval %= hshsize;

	for (np = hshtab[hshval]; np != NULL; np = np->next) {
		if (strcmp(str, np->name) == 0)
			return (np);
	}
	return (&nodef);
}

expand(a1, c)
char	**a1;
{
	register char	*dp;
	register struct nlist	*sp;

	sp = (struct nlist *) a1[-1];

	if (sp->tflag || trace) {
		int	i;

		fprintf(stderr, "Trace(%d): %s", Cp-callst, a1[0]);

		if (c > 0) {
			fprintf(stderr, "(%s", chkbltin(a1[1]));
			for (i = 2; i <= c; ++i)
				fprintf(stderr, ",%s", chkbltin(a1[i]));
			fprintf(stderr, ")");
		}
		fprintf(stderr, "\n");
	}

	dp = sp->def;

	for (; *dp; ++dp) {
		if (*dp&~LOW7) {
			(*barray[*dp&LOW7].bfunc)(a1, c);
		} else if (dp[1] == '$') {
			if (isdigit(*dp)) {
				register	n;
				if ((n = *dp-'0') <= c)
					pbstr(a1[n]);
				++dp;
			} else if (*dp == '#') {
				pbnum((long) c);
				++dp;
			} else if (*dp == '*' || *dp == '@') {
				register i = c;
				char **a = a1;

				if (i > 0)
					for (;;) {
						if (*dp == '@')
							pbstr(rquote);

						pbstr(a[i--]);

						if (*dp == '@')
							pbstr(lquote);

						if (i <= 0)
						break;

						pbstr(",");
					}
				++dp;
			} else
				putbak(*dp);
		} else
			putbak(*dp);
	}
}

setfname(s)
register char 	*s;
{
	strcpy(fname[ifx], s);
	fname[ifx+1] = fname[ifx]+strlen(s)+1;
	fline[ifx] = 1;
	nflag = 1;
	lnsync(stdout);
}

lnsync(iop)
register FILE	*iop;
{
	static int cline = 0;
	static int cfile = 0;

	if (!sflag || iop != stdout)
		return;

	if (nflag || ifx != cfile) {
		nflag = 0;
		cfile = ifx;
		fprintf(iop, "#line %d \"", cline = fline[ifx]);
		fpath(iop);
		fprintf(iop, "\"\n");
	} else if (++cline != fline[ifx])
		fprintf(iop, "#line %d\n", cline = fline[ifx]);
}

fpath(iop)
register FILE	*iop;
{
	register	i;

	fprintf(iop, "%s", fname[0]);

	for (i = 1; i <= ifx; ++i)
		fprintf(iop, ":%s", fname[i]);
}

/* ARGUSED */
void
catchsig(i)
int i;
{
	i = 0;
#ifdef unix
	(void) signal(SIGHUP, SIG_IGN);
	(void) signal(SIGINT, SIG_IGN);
#endif
	delexit(NOT_OK, 0);
}

delexit(code, flushio)
int code;
int flushio;
{
	register i;

	cf = stdout;

/*
 *	if (ofx != 0) {
 *		ofx = 0;
 *		code = NOT_OK;
 *	}
*/
	ofx = 0;	/* ensure that everything comes out */
	for (i = 1; i < 10; i++)
		undiv(i, code);

	tempfile[7] = 'a';
	unlink(tempfile);

	/* flush standard I/O buffers, ie: call exit() not _exit() */
	if (flushio)
		exit(code);

	_exit(code);
}

puttok(tp)
register char *tp;
{
	if (Cp) {
		while (*tp)
			stkchr(*tp++);
	} else if (cf)
		while (*tp)
			sputchr(*tp++, cf);
}

pbstr(str)
register char *str;
{
	register char *p;

	for (p = str + strlen(str); --p >= str; )
		putbak(*p);
}

undiv(i, code)
register	i;
{
	register FILE *fp;
	register	c;

	if (i < 1 || i > 9 || i == ofx || !ofile[i])
		return;

	fclose(ofile[i]);
	tempfile[7] = 'a'+i;

	if (code == OK && cf) {
		fp = xfopen(tempfile, "r");

		while ((c = getc(fp)) != EOF)
			sputchr(c, cf);

		fclose(fp);
	}

	unlink(tempfile);
	ofile[i] = NULL;
}

char	*
copy(s)
register char *s;
{
	register char *p;

	p = xcalloc(strlen(s)+1, sizeof (char));
	strcpy(p, s);
	return (p);
}

pbnum(num)
long num;
{
	pbnbr(num, 10, 1);
}

pbnbr(nbr, base, len)
long	nbr;
register	base, len;
{
	register	neg = 0;

	if (base <= 0)
		return;

	if (nbr < 0)
		neg = 1;
	else
		nbr = -nbr;

	while (nbr < 0) {
		register int	i;
		if (base > 1) {
			i = nbr%base;
			nbr /= base;
#if (-3 % 2) != -1
			while (i > 0) {
				i -= base;
				++nbr;
			}
#endif
			i = -i;
		} else {
			i = 1;
			++nbr;
		}
		putbak(itochr(i));
		--len;
	}

	while (--len >= 0)
		putbak('0');

	if (neg)
		putbak('-');
}

itochr(i)
register	i;
{
	if (i > 9)
		return (i-10+'A');
	else
		return (i+'0');
}

long
ctol(str)
register char *str;
{
	register sign;
	long num;

	while (isspace(*str))
		++str;
	num = 0;
	if (*str == '-') {
		sign = -1;
		++str;
	} else
		sign = 1;
	while (isdigit(*str))
		num = num*10 + *str++ - '0';
	return (sign * num);
}

min(a, b)
{
	if (a > b)
		return (b);
	return (a);
}

FILE	*
xfopen(name, mode)
char	*name,
	*mode;
{
	FILE	*fp;

	if ((fp = fopen(name, mode)) == NULL)
		error(gettext("can't open file"));

	return (fp);
}

/*
 * m4open
 *
 * Continue processing files when unable to open the given file argument.
 */
FILE	*
m4open(argvec, mode, argcnt)
char 	***argvec;
char	*mode;
int	*argcnt;
{
	FILE	*fp;
	char *arg;

	while (*argcnt > 0) {
		arg = (*argvec)[0]; /* point arg to current file name */
		if (arg[0] == '-' && arg[1] == EOS)
			return (stdin);
		else {
			if ((fp = fopen(arg, mode)) == NULL) {
				(void) fprintf(stderr, gettext(
				"m4: cannot open %s: "), arg);
				perror("");
				if (*argcnt == 1) {
					/* last arg therefore exit */
					error3();
				} else {
					exitstat = 1;
					(*argvec)++; /* try next arg */
					(*argcnt)--;
				}
			} else
				break;
		}
	}
	return (fp);
}

char *
xcalloc(nbr, size)
{
	register char	*ptr;

	if ((ptr = calloc((unsigned) nbr, (unsigned) size)) == NULL)
		error(gettext("out of storage"));

	return (ptr);
}

error2(str, num)
	char *str;
	int num;
{
	char buf[500];

	sprintf(buf, str, num);
	error(buf);
}

error(str)
	char *str;
{
	fprintf(stderr, "\n%s:", procnam);
	fpath(stderr);
	fprintf(stderr, ":%d %s\n", fline[ifx], str);
	error3();
}

error3()
{
	if (Cp) {
		register struct call	*mptr;

		/* fix limit */
		*op = EOS;
		(Cp+1)->argp = Ap+1;

		for (mptr = callst; mptr <= Cp; ++mptr) {
			register char	**aptr, **lim;

			aptr = mptr->argp;
			lim = (mptr+1)->argp-1;
			if (mptr == callst)
				fputs(*aptr, stderr);
			++aptr;
			fputs("(", stderr);
			if (aptr < lim)
				for (;;) {
					fputs(*aptr++, stderr);
					if (aptr >= lim)
						break;
					fputs(",", stderr);
				}
		}
		while (--mptr >= callst)
			fputs(")", stderr);

		fputs("\n", stderr);
	}
	delexit(NOT_OK, 1);
}

char *
chkbltin(s)
char  *s;
{
	static char	buf[24];

	if (*s & ~LOW7) {
		sprintf(buf, "<%s>", barray[*s&LOW7].bname);
		return (buf);
	}

	return (s);
}

int
getchr()
{
        prev_char = C;
	if (ip > ipflr)
		return (*--ip);
	C = feof(ifile[ifx]) ? EOF : getc(ifile[ifx]);
	if (C == '\n')
		fline[ifx]++;
	return (C);
}

/*
 * showwrap
 *
 * Loop through the list of m4wrap strings.  Call pbstr() so that the
 * string will be displayed, then delete the list entry and free the memory
 * allocated for it.
 */
void
showwrap()
{
	struct Wrap *prev;

	while (wrapstart) {
		pbstr(wrapstart->wrapstr);
		cfree(wrapstart->wrapstr);
		prev = wrapstart;
		wrapstart = wrapstart->nxt;
		free(prev);
	}
}
