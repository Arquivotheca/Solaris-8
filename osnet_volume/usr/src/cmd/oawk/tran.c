/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)tran.c	1.9	96/08/27 SMI"

#include "stdio.h"
#include "awk.def"
#include "awk.h"

CELL *symtab[MAXSYM];	/* symbol table pointers */


wchar_t	**FS;	/* initial field sep */
wchar_t	**RS;	/* initial record sep */
wchar_t	**OFS;	/* output field sep */
wchar_t	**ORS;	/* output record sep */
wchar_t	**OFMT;	/* output format for numbers */
awkfloat *NF;	/* number of fields in current record */
awkfloat *NR;	/* number of current record */
wchar_t	**FILENAME;	/* current filename argument */


CELL	*recloc;	/* location of record */
CELL	*nrloc;		/* NR */
CELL	*nfloc;		/* NF */


syminit()
{
	static wchar_t L_0[] = L"0";
	static wchar_t L_zeronull[] = L"$zero&null";
	static wchar_t L_record[] = L"$record";
	static wchar_t L_FS[] = L"FS";
	static wchar_t L_OFS[] = L"OFS";
	static wchar_t L_ORS[] = L"ORS";
	static wchar_t L_RS[] = L"RS";
	static wchar_t L_OFMT[] = L"OFMT";
	static wchar_t L_space[] = L" ";
	static wchar_t L_newline[] = L"\n";
	static wchar_t L_dot6g[] = L"%.6g";
	static wchar_t L_FILENAME[] = L"FILENAME";
	static wchar_t L_NF[] = L"NF";
	static wchar_t L_NR[] = L"NR";


	setsymtab(L_0, tostring(L_0), 0.0, NUM|STR|CON|FLD, symtab);
	/* this one is used for if (x)... tests: */
	setsymtab(L_zeronull, tostring(L_NULL), 0.0, NUM|STR|CON|FLD, symtab);
	recloc = setsymtab(L_record, record, 0.0, STR|FLD, symtab);
	dprintf("recloc %o lookup %o\n",
		recloc, lookup(L_record, symtab, 0), NULL);
	FS = &setsymtab(L_FS, tostring(L_space), 0.0, STR|FLD, symtab)->sval;
	RS = &setsymtab(L_RS, tostring(L_newline), 0.0, STR|FLD, symtab)->sval;
	OFS = &setsymtab(L_OFS, tostring(L_space), 0.0, STR|FLD, symtab)->sval;
	ORS = &setsymtab(L_ORS, tostring(L_newline), 0.0, STR|FLD,
		symtab)->sval;
	OFMT = &setsymtab(L_OFMT, tostring(L_dot6g), 0.0, STR|FLD,
		symtab)->sval;
	FILENAME = &setsymtab(L_FILENAME, NULL, 0.0, STR|FLD, symtab)->sval;
	nfloc = setsymtab(L_NF, NULL, 0.0, NUM, symtab);
	NF = &nfloc->fval;
	nrloc = setsymtab(L_NR, NULL, 0.0, NUM, symtab);
	NR = &nrloc->fval;
}


CELL **makesymtab()
{
	int i;
	CELL **cp;


	cp = (CELL **) malloc(MAXSYM * sizeof (CELL *));
	if (cp == NULL)
		error(FATAL, "out of space in makesymtab");
	for (i = 0; i < MAXSYM; i++)
		cp[i] = 0;
	return (cp);
}


freesymtab(ap)	/* free symbol table */
CELL *ap;
{
	CELL *cp, **tp;
	int i;


	if (!(ap->tval & ARR))
		return;
	tp = (CELL **) ap->sval;
	for (i = 0; i < MAXSYM; i++) {
		for (cp = tp[i]; cp != NULL; cp = cp->nextval) {
			xfree(cp->nval);
			xfree(cp->sval);
			free(cp);
		}
	}
	xfree(tp);
}


CELL *setsymtab(n, s, f, t, tab)
wchar_t *n, *s;
awkfloat f;
unsigned t;
CELL **tab;
{
	register h;
	register CELL *p;
	CELL *lookup();


	if (n != NULL && (p = lookup(n, tab, 0)) != NULL) {
		xfree(s);
		dprintf("setsymtab found %o: %ws\n", p, p->nval, NULL);
		dprintf(" %ws %g %o\n", p->sval, p->fval, p->tval);
		return (p);
	}
	p = (CELL *) malloc(sizeof (CELL));
	if (p == NULL)
		error(FATAL, "symbol table overflow at %ws", n);
	p->nval = tostring(n);
	p->sval = s;
	p->fval = f;
	p->tval = t;
	h = hash(n);
	p->nextval = tab[h];
	tab[h] = p;
	dprintf("setsymtab set %o: %ws\n", p, p->nval, NULL);
	dprintf(" %ws %g %o\n", p->sval, p->fval, p->tval);
	return (p);
}


hash(s)	/* form hash value for string s */
register wchar_t *s;
{
	register unsigned hashval;


	for (hashval = 0; *s != '\0'; /* dummy */)
		hashval += *s++;
	return (hashval % MAXSYM);
}


CELL *lookup(s, tab, flag)	/* look for s in tab, flag must match */
register wchar_t *s;
CELL **tab;
{
	register CELL *p;


	for (p = tab[hash(s)]; p != NULL; p = p->nextval)
		if (wscmp(s, p->nval) == 0 &&
			(flag == 0 || flag == p->tval))
			return (p);	/* found it */
	return (NULL);	/* not found */
}


awkfloat setfval(vp, f)
register CELL *vp;
awkfloat f;
{
	dprintf("setfval: %o %g\n", vp, f, NULL);
/* imb */
	if (vp->tval & ARR)
		error(FATAL, "illegal reference to array %s", vp->nval);
	if ((vp->tval & (NUM | STR)) == 0)
		error(FATAL, "funny variable %o: %ws %ws %g %o", vp, vp->nval,
			vp->sval, vp->fval, vp->tval);
/* imb */
	if (vp == recloc)
		error(FATAL, "can't set $0");
	vp->tval &= ~STR;	/* mark string invalid */
	vp->tval |= NUM;	/* mark number ok */
	if ((vp->tval & FLD) && vp->nval == 0) {
		/*
		 * FLD really means that the string value was not
		 * "malloc"ed and should not be freed.  All fields
		 * have this property, but not all cells with this
		 * property are fields.  However, all cells with
		 * this property and with a NULL "nval" are fields.
		 * If we are setting the value of a field, indicate
		 * that the value of the record has to be recomputed,
		 * and if it's a higher field than the last one we
		 * assigned to, remember it for when we clear the
		 * fields out for the next record.
		 */
		donerec = 0;
		if (vp > maxmfld)
			maxmfld = vp;
	}
	return (vp->fval = f);
}


wchar_t *setsval(vp, s)
register CELL *vp;
wchar_t *s;
{
	dprintf("setsval: %o %ws\n", vp, s, NULL);
	if (vp->tval & ARR)
		error(FATAL, "illegal reference to array %ws", vp->nval);
	if ((vp->tval & (NUM | STR)) == 0)
		error(FATAL, "funny variable %o: %ws %ws %g %o", vp, vp->nval,
			vp->sval, vp->fval, vp->tval);
	if (vp == recloc)
		error(FATAL, "can't set $0");
	vp->tval &= ~NUM;
	vp->tval |= STR;
	if ((vp->tval & FLD) && vp->nval == 0) {
		/*
		 * See comment in "setfval".
		 */
		donerec = 0;
		if (vp > maxmfld)
			maxmfld = vp;
	}
	if (!(vp->tval&FLD))
		xfree(vp->sval);
	vp->tval &= ~FLD;
	return (vp->sval = tostring(s));
}


awkfloat getfval(vp)
register CELL *vp;
{


	if (vp->sval == record && donerec == 0)
		recbld();
	dprintf("getfval: %o", vp, NULL, NULL);
	if (vp->tval & ARR)
		error(FATAL, "illegal reference to array %ws", vp->nval);
	if ((vp->tval & (NUM | STR)) == 0)
		error(FATAL, "funny variable %o: %ws %ws %g %o", vp, vp->nval,
			vp->sval, vp->fval, vp->tval);
	if ((vp->tval & NUM) == 0) {
		/* the problem is to make non-numeric things */
		/* have unlikely numeric variables, so that */
		/* $1 == $2 comparisons sort of make sense when */
		/* one or the other is numeric */
		if (isanumber(vp->sval)) {
			vp->fval = watof(vp->sval);
			if (!(vp->tval & CON))
				/* don't change type of a constant */
				vp->tval |= NUM;
		}
		else
			vp->fval = 0.0;	/* not a very good idea */
	}
	dprintf("  %g\n", vp->fval, NULL, NULL);
	return (vp->fval);
}


wchar_t *getsval(vp)
register CELL *vp;
{
	char s[100];
	wchar_t ws[100];


	if (vp->sval == record && donerec == 0)
		recbld();
	dprintf("getsval: %o", vp, NULL, NULL);
	if (vp->tval & ARR)
		error(FATAL, "illegal reference to array %ws", vp->nval);
	if ((vp->tval & (NUM | STR)) == 0)
		error(FATAL, "funny variable %o: %ws %ws %g %o", vp, vp->nval,
			vp->sval, vp->fval, vp->tval);
	if ((vp->tval & STR) == 0) {
		if (!(vp->tval&FLD))
			xfree(vp->sval);
		if ((long long)vp->fval==vp->fval)
			sprintf(s, "%.20g", vp->fval);
		else
			sprintf(s, toeuccode(*OFMT), vp->fval);
		mbstowcs(ws, s, sizeof (ws) / sizeof (wchar_t));
		vp->sval = tostring(ws);
		vp->tval &= ~FLD;
		vp->tval |= STR;
	}
	dprintf("  %ws\n", vp->sval, NULL, NULL);
	return (vp->sval);
}


wchar_t *tostring(s)
register wchar_t *s;
{
	register wchar_t *p;


	p = (wchar_t *) malloc((wslen(s)+1)*sizeof (wchar_t));
	if (p == NULL)
		error(FATAL, "out of space in tostring on %ws", s);
	wscpy(p, s);
	return (p);
}


#ifndef yfree
yfree(a) char *a;
{
	printf("%o\n", a);
	free(a);
}
#endif
