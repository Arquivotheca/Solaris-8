#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stddef.h>
#include "colltbl.h"

extern int	secflag;
extern int	smtomflag;
extern int	s1tomflag;
extern int	c2to1flag;
extern int	c1to1flag;
extern int	curprim;
extern FILE	*strm;

#define	xpr(prog, i, j)\
	for (k=i; k <= j; k++)	fprintf(strm, "%s\n", prog[k])

#define	E_WRITE		1

const char *xfrm[] = {
	"",
	"size_t _strxfrm_(char *sout, const char *sin, size_t n)",
	"",
	"",
	"{",
	"int	i,  len = 0;",
	"unsigned int ui;",
	"char	c;",
	"const char	*c2;",
	"const char	*c3;",
	"char	*sptr, *secstr;",
	"	if (sout== NULL)  /* return length only */",
	"		n = 0;",
	"",
	"   secstr=sptr = sout + n - 1;",
	"    if (len++ < n) *secstr-- = 0x1;",
	"L0:   for (; *sin; ) {",
	"	ui=(unsigned char) *sin;",
	"	if (s1tomtab[ui].index)  /* try m-to-m substitution */",
	"	   for (i=s1tomtab[ui].index; smtomtab[i].exp; i++) {",
	"    	for (c2=sin+1, c3=smtomtab[i].exp; *c3 && *c2; c2++, c3++) ",
	"			if (*c2 != *c3) ",
	"				break;",
	"    		if (*c3)  {	/* not matched */",
	"		",
	"			continue;",
	"			}",
	"		else {	/* matched */",
	"			sin = c2;",
	"			for (c2=(char *) smtomtab[i].prim; *c2; ) ",
	"				if (len++ < n)",
	"					*sout++ = *c2++;",
	"				else ",
	"					for (c2++; *c2; c2++, len++);",
	"			for (c2=(char *) smtomtab[i].sec; *c2; ) {",
	"			",
	"			",
	"			",
	"			",
	"				if (len++ < n)",
	"					*secstr-- = *c2++;",
	"				else",
	"					for (c2++; *c2; c2++)",
	"						if (*c2 != 1)",
	"							len++;",
	"				}",
	"			goto L0;",
	"			}",
	"	   	}",
	"L1:	",
	"	if (s1tomtab[ui].prim)	{ /* try 1-to-m substitution */",
	"		sin++;",
	"		for (c2=(char *) s1tomtab[ui].prim; *c2; ) ",
	"				if (len++ < n)",
	"					*sout++ = *c2++;",
	"				else ",
	"					for (c2++; *c2; c2++, len++);",
	"		for (c2=(char *) s1tomtab[ui].sec; *c2; ) {",
	"			",
	"			",
	"			",
	"			",
	"				if (len++ < n)",
	"					*secstr-- = *c2++;",
	"				else",
	"					for (c2++; *c2; c2++)",
	"						if (*c2 != 1)",
	"							len++;",
	"				}",
	"		goto L0;",
	"		}",
	"		",
	"   	if (c1to1tab[ui].index) { /* try 2-to-1 collation */",
	"	 for (i=c1to1tab[ui].index, c=*(sin+1); c2to1tab[i].c ; i++) ",
	"	  	if (c2to1tab[i].c == c) { /* matched */",
	"			if (len++ < n)",
	"				*sout++ = c2to1tab[i].p;",
	"			if (c = c2to1tab[i].s)	{",
	"					",
	"					if (len++ < n)",
	"						*secstr-- = c;",
	"					",
	"				}",
	"			sin += 2;",
	"			goto L0;",
	"			}",
	"	   }",
	"",
	"   	if (c1to1tab[ui].p) {	/*finally, 1-to-1 collation element */",
	"		if (len++ < n)",
	"			*sout++ = c1to1tab[ui].p;",
	"		if (c = c1to1tab[ui].s) {",
	"					",
	"			   if (len++ < n)",
	"				*secstr-- = c;",
	"			   		",
	"			}",
	"			",
	"		}",
	"  	 sin++;",
	"   	}",
	"	",
	"	if (sout) {	/* reverse secondary string */",
	"		*secstr = 0;",
	"		for (i = sptr - secstr; i >= 0; i--) ",
	"			if (sout < secstr)",
	"				*sout++ = *sptr--;",
	"			else {",
	"				c = *sout;",
	"				*sout++ = *sptr;",
	"				*sptr-- = c;",
	"				i--;",
	"				}",
	"		}",
	"		*sout = 0; }	",
	"	return len;",
	"} ",
	};

void gen_strxfrm()
{
int	subflag;
int	k;

	subflag = smtomflag | s1tomflag;

	xpr(xfrm, 1, 7);
	if (subflag)
		xpr(xfrm, 8, 8);
	if (smtomflag)
		xpr(xfrm, 9, 9);
	if (secflag)
		xpr(xfrm, 10, 10);
	xpr(xfrm, 11, 12);
	if (secflag)
		xpr(xfrm, 14, 17);
	else
		xpr(xfrm, 16, 17);
	if (smtomflag) {
		if (secflag)
			xpr(xfrm, 18, 49);
		else {
			xpr(xfrm, 18, 33);
			xpr(xfrm, 46, 49);
			}
		}
	if (s1tomflag) {
		if (secflag)
			xpr(xfrm, 50, 70);
		else {
			xpr(xfrm, 50, 56);
			xpr(xfrm, 69, 70);
			}
		}
	if (c2to1flag) {
		if (secflag)
			xpr(xfrm, 72, 86);
		else {
			xpr(xfrm, 72, 76);
			xpr(xfrm, 83, 86);
			}
		}
	if (c1to1flag) {
		if (secflag)
			xpr(xfrm, 88, 98);
		else {
			xpr(xfrm, 88, 90);
			xpr(xfrm, 98, 98);
			}
		}
	xpr(xfrm, 99, 102);
	if (secflag)
		xpr(xfrm, 103, 113);
	else
		xpr(xfrm, 114, 114);
	xpr(xfrm, 115, 116);

	if (fprintf(strm, "\n") == EOF)
		err(E_WRITE);
}


const char *coll[] = {
	"",
	"int	_strcoll_(const char *s1, const char *s2)",
	"",
	"{",
	"int	prim1 = 0;",
	"int	prim2 = 0;",
	"unsigned int	ui;",
	"int		i;",
	"char c;",
	"static const unsigned char *nullstr = (unsigned char *) \"\";",
	"const char *c2;",
	"const char *c3;",
	"const unsigned char *svs1 = nullstr;",
	"const unsigned char *svs2 = nullstr;",
	"const unsigned char *ss1;",
	"const unsigned char *ss2;",
	"#define secdiff 0",
	"int	sec1	= 0;",
	"int	sec2	= 0;",
	"int	secdiff = 0;",
	"",
	"",
	"L0:   	if (*s1 == 0) {",
	"		prim1 = 0;",
	"		goto s2lab;",
	"		}",
	"",
	"	/* check for substitution strings */",
	"L1:   	ui = (unsigned char ) *s1;",
	"   	if (s1tomtab[ui].index)  /* try m-to-m substitution */",
	"	    for (i=s1tomtab[ui].index; smtomtab[i].exp; i++) {",
	"    	 for (c2=s1+1, c3=smtomtab[i].exp; *c3 && *c2; c2++, c3++) ",
	"			if (*c2 != *c3) ",
	"				break;",
	"    		   if (*c3)  {",
	"		   	",
	"			continue;",
	"		  	 }",
	"		   else  { /* match !! */",
	"			s1 = c2;/* advance the position of s1 */",
	"			svs1 = smtomtab[i].prim;",
	"			if (*svs1) {	/* non-zero primary weights */",
	"				ss1 = smtomtab[i].sec;",
	"				prim1 = 0;",
	"				goto s2lab;",
	"				}",
	"		else {	/* go finding non-zero primary weights */",
	"				goto L0;",
	"				}",
	"	   	  	 }",
	"		   }",
	"L2:	   ",
	"	if (s1tomtab[ui].prim) { /* try 1-to-m substitution */",
	"		s1 = s1 + 1;",
	"		svs1 = s1tomtab[ui].prim;",
	"		if (*svs1) {",
	"			prim1 = 0;",
	"			ss1 = s1tomtab[ui].sec;",
	"			goto s2lab;",
	"			}",
	"		else {",
	"			goto L0;",
	"			}",
	"		}",
	"",
	"	if (c1to1tab[ui].index) { /* try 2-to-1 collation */",
	"	for(i=c1to1tab[ui].index, c=*(s1+1); c2to1tab[i].c ; i++)",
	"			if (c2to1tab[i].c == c)  { /*matched */",
	"				s1 += 2;",
	"				prim1 = c2to1tab[i].p;",
	"				sec1 = c2to1tab[i].s;",
	"				goto s2lab;",
	"				}",
	"		}",
	"",
	"	if (c1to1tab[ui].p) { 	/*finally, 1-to-1 collation element */",
	"		s1++;",
	"		prim1 = c1to1tab[ui].p;",
	"		sec1 = c1to1tab[ui].s;",
	"		goto s2lab;",
	"		}",
	"	s1++;",
	"	goto L0;",
	"",
	"/*	When we reach this point, there must be some primary weight",
	" *		stored in prim1 or svs1, ",
	" *	or s1 is exhausted.",
	" *	",
	" *	The following block is to find the primary weight from s2",
	" */",
	"	",
	"s2lab:",
	"	if (*svs2)	/* svs2 holds some non-zero primary weights */",
	"		goto compare1;",
	"L3:   	if (*s2 == 0) {		/* s2 is exausted */",
	"		if (prim1 || *svs1) ",
	"		if (prim1) ",
	"			return 1;",
	"	 	else /* prims of s1 and s2 are exausted */",
	"			return secdiff;",
	"		}",
	"",
	"",
	"	/* check for substitution strings */",
	"   	ui = (unsigned char ) *s2;",
	"   	if (s1tomtab[ui].index)  /* try m-to-m substitution */",
	"	   for (i=s1tomtab[ui].index; smtomtab[i].exp; i++) {",
	"    for (c2=s2+1, c3=smtomtab[i].exp; *c3 && *c2; c2++, c3++) ",
	"			if (*c2 != *c3) ",
	"				break;",
	"    		if (*c3) { ",
	"  		",
	"			continue;",
	"			}",
	"   	  	else { /* match !! */",
	"			s2 = c2;",
	"			svs2 = smtomtab[i].prim;",
	"			if (*svs2) {",
	"				ss2 = smtomtab[i].sec;",
	"				goto compare1;",
	"				}",
	"			else {",
	"				goto L3;",
	"				}",
	"			}",
	"	   	}",
	"L4:",
	"	if (s1tomtab[ui].prim) { /* try 1-to-m substitution */",
	"		s2 = s2 + 1;",
	"		svs2 = s1tomtab[ui].prim;",
	"		if (*svs2) {",
	"			ss2 = s1tomtab[ui].sec;",
	"			goto compare1;",
	"			}",
	"		else {",
	"			goto L3;",
	"			}",
	"		}",
	"",
	"	if (c1to1tab[ui].index) { /* try 2-to-1 collation */",
	"	for(i=c1to1tab[ui].index, c=*(s2+1); c2to1tab[i].c ; i++)",
	"			if (c2to1tab[i].c == c)  { /*matched */",
	"				s2 += 2;",
	"				prim2 = c2to1tab[i].p;",
	"				sec2 = c2to1tab[i].s;",
	"				goto compare2;",
	"				}",
	"		}",
	"",
	"	if (c1to1tab[ui].p) { 	/*finally, 1-to-1 collation element */",
	"		prim2 = c1to1tab[ui].p;",
	"		sec2 = c1to1tab[ui].s;",
	"		s2++;",
	"		goto compare2;",
	"		}",
	"	s2++;",
	"	goto L3;",
	"	",
	"compare1:	/* s2 is the secondary weights of a subst string */",
	"   if (prim1) {",
	"	if (prim1 != *svs2)",
	"		return (prim1 - *svs2);	/* game over */",
	"	if (!secdiff && sec1)",
	"		secdiff = (sec1 - *ss2++);",
	"	",
	"	svs2++;",
	"	goto L0;",
	"	}",
	"   else {",
	"	if (*svs1) {",
	"	   	for (; *svs1 && *svs2; svs1++, svs2++) ",
	"		   if (*svs1 != *svs2)",
	"			return (*svs1 - *svs2);",
	"	   	if (!secdiff)",
	"		   for (; *ss1 && *ss2; ss1++, ss2++)",
	"			if (*ss1 != *ss2) {",
	"				secdiff = *ss1 - *ss2;",
	"				break;",
	"				}",
	"	  	if (*svs1) 		/* svs2 is exausted */",
	"		   goto L3;",
	"		/* else (!*svs1) */	/* svs1 is exausted */",
	"		goto L0;		",
	"	 	}",
	"	else	/* prim1==0 && *svs1== 0 */",
	"		return (-1);",
	"	}",
	"	",
	"",
	"compare2:",
	"   if (prim1) {",
	"	if (prim1 != prim2)",
	"		return (prim1 - prim2);",
	"	if (!secdiff)",
	"		secdiff = sec1 - sec2;",
	"	goto L0;",
	"	}",
	"   else {",
	"	if (*svs1 != prim2)",
	"		return (*svs1 - prim2);",
	"	if (!secdiff && sec2)",
	"		secdiff = *ss1++ - sec2; ",
	"	svs1++;",
	"	if (*svs1)	",
	"		goto L3;  ",
	"	goto L0; /* get primary weights from s1 */",
	"	}",
	"}",
	};


void gen_strcoll()
{
int	subflag;
int	k;

	subflag = smtomflag | s1tomflag;

	xpr(coll, 1, 8);
	if (subflag) {
		xpr(coll, 9, 10);
		if (smtomflag)
			xpr(coll,  11, 11);
		xpr(coll,  12, 13);
		if (secflag)
			xpr(coll,  14, 15);
		}
	if (secflag)
		xpr(coll, 17, 19);
	else
		xpr(coll, 16, 16);

	/* program starts here */
	/* get primary weights from s1 */
	xpr(coll, 21, 28);
	if (smtomflag) {
		if (secflag)
			xpr(coll, 29, 51);
		else {
			xpr(coll, 29, 41);
			xpr(coll, 43, 51);
			}
		}
	if (s1tomflag) {
		if (secflag)
			xpr(coll, 52, 63);
		else {
			xpr(coll, 52, 56);
			xpr(coll, 58, 63);
			}
		}
	if (c2to1flag) {
		if (secflag)
			xpr(coll, 65, 73);
		else {
			xpr(coll, 65, 69);
			xpr(coll, 71, 73);
			}
		}
	if (c1to1flag) {
		if (secflag)
			xpr(coll, 75, 80);
		else {
			xpr(coll, 75, 77);
			xpr(coll, 79, 80);
			}
		}
	/* get primary weights from s2 */
	xpr(coll, 81, 91);
	if (subflag)
		xpr(coll, 92, 95);
	else {
		xpr(coll, 94, 94);
		xpr(coll, 96, 96);
		}
	xpr(coll, 97, 104);
	if (smtomflag) {
		if (secflag)
			xpr(coll, 105, 126);
		else {
			xpr(coll, 105, 117);
			xpr(coll, 119, 126);
			}
		}
	if (s1tomflag) {
		if (secflag)
			xpr(coll, 127, 137);
		else {
			xpr(coll, 127, 130);
			xpr(coll, 132, 137);
			}
		}
	if (c2to1flag) {
		if (secflag)
			xpr(coll, 139, 147);
		else {
			xpr(coll, 139, 143);
			xpr(coll, 145, 147);
			}
		}
	if (c1to1flag) {
		if (secflag)
			xpr(coll, 149, 154);
		else {
			xpr(coll, 149, 150);
			xpr(coll, 152, 154);
			}
		}
	xpr(coll, 155, 156);
	/* compare primary weights (and secondary weights if possible) */
	if (!subflag) {
		xpr(coll, 189, 189);
		xpr(coll, 191, 192);
		if (secflag)
			xpr(coll, 193, 194);
		xpr(coll, 195, 195);
		xpr(coll, 207, 207);
	} else {
		if (secflag)
			xpr(coll, 158, 207);
		else {
			xpr(coll, 158, 161);
			xpr(coll, 165, 172);
			xpr(coll, 179, 192);
			xpr(coll, 195, 199);
			xpr(coll, 202, 207);
			}
		}
	if (fprintf(strm, "\n") == EOF)
		err(E_WRITE);
}


const char *wcoll[] = {
	"",
	"int	_wscoll_(const wchar_t *s1, const wchar_t *s2)",
	"",
	"{",
	"int	prim1 = 0;",
	"int	prim2 = 0;",
	"unsigned int	ui;",
	"int		i, j;",
	"char c;",
	"static const unsigned char	*nullstr = (unsigned char *) \"\";",
	"const wchar_t *c2;",
	"const char *c3;",
	"const unsigned char *svs1 = nullstr;",
	"const unsigned char *svs2 = nullstr;",
	"const unsigned char *ss1;",
	"const unsigned char *ss2;",
	"	/* share the same code of coll[16]-coll[27] */",
	"",
	"L1:   	ui = *s1;",
	"	",
	"	if ((ui & WCHAR_CSMASK) == WCHAR_CS1) {",
	"	",
	"	",
	"	",
	"		ui = (ui & 0x7f) | 0x80;  /* convert to EUC char */",
	"	}",
	"   	if (s1tomtab[ui].index)  /* try m-to-m substitution */",
	"	    for (i=s1tomtab[ui].index; smtomtab[i].exp; i++) {",
	"    	  for (c2=s1+1, c3=smtomtab[i].exp; *c3 && *c2; c2++, c3++) {",
	"			j = *c2;",
	"			",
	"			if ((j & WCHAR_CSMASK) == WCHAR_CS1)",
	"				c = (j & 0x7f) | 0x80;",
	"	",
	"			else",
	"				c = (char) j;",
	"			if (c != *c3) ",
	"				break; }",
	"    		   if (*c3)  {",
	"		   	",
	"			continue;",
	"		  	 }",
	"		   else  { /* match !! */",
	"			s1 = c2; /* advance the position of s1 */",
	"			svs1 = smtomtab[i].prim;",
	"			if (*svs1) {	/* non-zero primary weights */",
	"				ss1 = smtomtab[i].sec;",
	"				prim1 = 0;",
	"				goto s2lab;",
	"				}",
	"		else {	/* go finding non-zero primary weights */",
	"				goto L0;",
	"				}",
	"	   	  	 }",
	"		   }",
	"L2:	   ",
	"	/* share the same code of coll[52]-coll[63] */",
	"",
	"	if (c1to1tab[ui].index) { /* try 2-to-1 collation */",
	"	   	j=*(s1+1);",
	"	",
	"		if ((j & WCHAR_CSMASK) == WCHAR_CS1)",
	"	",
	"	",
	"			c = (j & 0x7f) | 0x80;",
	"	",
	"	   	else ",
	"			c= (char) j;",
	"		for(i=c1to1tab[ui].index; c2to1tab[i].c ; i++)",
	"			if (c2to1tab[i].c == c)  { /*matched */",
	"				s1 += 2;",
	"				prim1 = c2to1tab[i].p;",
	"				sec1 = c2to1tab[i].s;",
	"				goto s2lab;",
	"				}",
	"		}",
	"C11S1:",
	"	/* share the same code of coll[75]-coll[100] */",
	"",
	"",
	"	/* check for substitution strings */",
	"   	ui = *s2;",
	"	",
	"	if ((ui & WCHAR_CSMASK) == WCHAR_CS1)",
	"		ui = (ui & 0x7f) | 0x80;  /* convert to EUC char */",
	"   	if (s1tomtab[ui].index)  /* try m-to-m substitution */",
	"	   for (i=s1tomtab[ui].index; smtomtab[i].exp; i++) {",
	"    	for (c2=s2+1, c3=smtomtab[i].exp; *c3 && *c2; c2++, c3++) {",
	"			j = *c2;",
	"			if ((j & WCHAR_CSMASK) == WCHAR_CS1)",
	"				c = (j & 0x7f) | 0x80;",
	"			else	c = (char) j;",
	"			if (c != *c3) ",
	"				break; }",
	"    		if (*c3) { ",
	"  	",
	"			continue;",
	"			}",
	"   	  	else { /* match !! */",
	"			s2 = c2;",
	"			svs2 = smtomtab[i].prim;",
	"			if (*svs2) {",
	"				ss2 = smtomtab[i].sec;",
	"				goto compare1;",
	"				}",
	"			else {",
	"				goto L3;",
	"				}",
	"			}",
	"	   	}",
	"L4:",
	"	/* share the same code of coll[127]-coll[137] */",
	"	if (c1to1tab[ui].index) { /* try 2-to-1 collation */",
	"	   	j=*(s2+1);",
	"	   ",
	"		if ((j & WCHAR_CSMASK) == WCHAR_CS1)",
	"	",
	"	",
	"			c = (j & 0x7f) | 0x80;",
	"	",
	"	   	else ",
	"			c= (char) j;",
	"	for(i=c1to1tab[ui].index; c2to1tab[i].c ; i++)",
	"			if (c2to1tab[i].c == c)  { /*matched */",
	"				s2 += 2;",
	"				prim2 = c2to1tab[i].p;",
	"				sec2 = c2to1tab[i].s;",
	"				goto compare2;",
	"				}",
	"		}",
	"C11S2:",
	"	/* share the same code of coll[149]-coll[207] */",
	};

void gen_wscoll()
{
int	subflag;
int	k;

	subflag = smtomflag | s1tomflag;

	xpr(wcoll, 1, 8);
	if (subflag) {
		xpr(wcoll, 9, 10);
		if (smtomflag)
			xpr(wcoll,  11, 11);
		xpr(wcoll,  12, 13);
		if (secflag)
			xpr(wcoll,  14, 15);
		}
	if (secflag)	/* usr the code from coll[] */
		xpr(coll, 17, 19);
	else
		xpr(coll, 16, 16);

	/* program starts here */
	/* get primary weights from s1 */
	xpr(coll, 21, 27);
	xpr(wcoll, 18, 25);

	if (smtomflag) {
		if (secflag)
			xpr(wcoll, 26, 55);
		else {
			xpr(wcoll, 26, 45);
			xpr(wcoll, 47, 55);
			}
		}
	if (s1tomflag) {
		if (secflag)
			xpr(coll, 52, 63);
		else {
			xpr(coll, 52, 56);
			xpr(coll, 58, 63);
			}
		}
	if (c2to1flag) {
		if (secflag)
			xpr(wcoll, 58, 76);
		else {
			xpr(wcoll, 58, 71);
			xpr(wcoll, 73, 76);
			}
		}
	if (c1to1flag) {
		if (secflag)
			xpr(coll, 75, 80);
		else {
			xpr(coll, 75, 77);
			xpr(coll, 79, 80);
			}
		}
	/* get primary weights from s2 */
	xpr(coll, 81, 91);
	if (subflag)
		xpr(coll, 92, 95);
	else {
		xpr(coll, 94, 94);
		xpr(coll, 96, 96);
		}
	xpr(coll, 97, 100);
	xpr(wcoll, 81, 84);
	if (smtomflag) {
		if (secflag)
			xpr(wcoll, 85, 110);
		else {
			xpr(wcoll, 85, 101);
			xpr(wcoll, 103, 110);
			}
		}
	if (s1tomflag) {
		if (secflag)
			xpr(coll, 127, 137);
		else {
			xpr(coll, 127, 130);
			xpr(coll, 132, 137);
			}
		}
	if (c2to1flag) {
		if (secflag)
			xpr(wcoll, 112, 130);
		else {
			xpr(wcoll, 112, 125);
			xpr(wcoll, 127, 130);
			}
		}
	if (c1to1flag) {
		if (secflag)
			xpr(coll, 149, 154);
		else {
			xpr(coll, 149, 150);
			xpr(coll, 152, 154);
			}
		}
	xpr(coll, 155, 156);
	/* compare primary weights (and secondary weights if possible) */
	if (!subflag) {
		xpr(coll, 189, 189);
		xpr(coll, 191, 192);
		if (secflag)
			xpr(coll, 193, 194);
		xpr(coll, 195, 195);
		xpr(coll, 207, 207);
	} else {
		if (secflag)
			xpr(coll, 158, 207);
		else {
			xpr(coll, 158, 161);
			xpr(coll, 165, 172);
			xpr(coll, 179, 192);
			xpr(coll, 195, 199);
			xpr(coll, 202, 207);
			}
		}
	if (fprintf(strm, "\n") == EOF)
		err(E_WRITE);
}


const char *wxfrm[] = {
	"",
	"size_t _wsxfrm_(wchar_t *sout, const wchar_t *sin, size_t n)",
	"",
	"",
	"{ int	i,  len = 0;",
	"unsigned int ui;",
	"char	c;",
	"const wchar_t	*c2; wchar_t j;",
	"const unsigned char	*uc;",
	"const char	*c3;",
	"wchar_t	*sptr, *secstr;",
	"	if (sout== NULL)  /* return length only */",
	"		n = 0;",
	"",
	"   secstr=sptr = sout + n - 1;",
	"   if (len++ < n) *secstr-- = 0x1;",
	"L0:   for (; *sin; ) {",
	"	ui = *sin;",
	"	",
	"	if ((ui & WCHAR_CSMASK) == WCHAR_CS1) ",
	"		ui = (ui & 0x7f) | 0x80;  /* convert to EUC char */",
	"	",
	"	",
	"	",
	"	",
	"	if (s1tomtab[ui].index)  /* try m-to-m substitution */",
	"	   for (i=s1tomtab[ui].index; smtomtab[i].exp; i++) {",
	"    	for (c2=sin+1, c3=smtomtab[i].exp; *c3 && *c2; c2++, c3++) {",
	"			j = *c2;",
	"		",
	"			if ((j & WCHAR_CSMASK) == WCHAR_CS1)",
	"					c = (j & 0x7f) | 0x80;",
	"			else",
	"				c = (char) j;",
	"				",
	"			if (c != *c3) ",
	"				break;",
	"			}",
	"    		if (*c3)  {	/* not matched */",
	"	",
	"			continue;	",
	"			}",
	"		else {	/* matched */",
	"			sin = c2;",
	"			for (uc= smtomtab[i].prim; *uc; ) ",
	"				if (len++ < n) {",
	"					if (*uc & 0x80)",
	"				*sout++ = WCHAR_CS1 | (*uc & 0x7f);",
	"					else ",
	"					*sout++ = *uc++; }",
	"				else ",
	"					for (uc++; *uc; uc++, len++);",
	"			for (uc= smtomtab[i].sec; *uc; ) {",
	"						",
	"						",
	"						",
	"						",
	"				if (len++ < n) {",
	"					if (*uc & 0x80)",
	"				*secstr-- = WCHAR_CS1 | (*uc & 0x7f);",
	"					else ",
	"					*secstr-- = *uc++; }",
	"				else",
	"					for (uc++; *uc; uc++)",
	"						if (*uc != 1)",
	"							len++;",
	"				}",
	"			goto L0;",
	"			}",
	"	   	}",
	"L1:	",
	"	if (s1tomtab[ui].prim)	{ /* try 1-to-m substitution */",
	"		sin++;",
	"		for (uc=s1tomtab[ui].prim; *uc; ) ",
	"				if (len++ < n) {",
	"					if (*uc & 0x80)",
	"				*sout++ = WCHAR_CS1 | (*uc & 0x7f);",
	"					else ",
	"					*sout++ = *uc++; }",
	"				else ",
	"					for (uc++; *uc; uc++, len++);",
	"		for (uc=s1tomtab[ui].sec; *uc; ) {",
	"						",
	"						",
	"						",
	"						",
	"				if (len++ < n) {",
	"					if (*uc & 0x80)",
	"				   *sout++ = WCHAR_CS1 | (*uc & 0x7f);",
	"					else ",
	"					   *secstr-- = *uc++; }",
	"				else",
	"					for (uc++; *uc; uc++)",
	"						if (*uc != 1)",
	"							len++;",
	"				}",
	"		goto L0;",
	"		}",
	"		",
	"   	if (c1to1tab[ui].index) { /* try 2-to-1 collation */",
	"	  	j=*(sin+1);",
	"	  ",
	"		if ((j & WCHAR_CSMASK) == WCHAR_CS1)",
	"	",
	"	",
	"			c = (j & 0x7f) | 0x80;",
	"	",
	"		else ",
	"			c= (char) j;",
	"	   for (i=c1to1tab[ui].index; c2to1tab[i].c ; i++) ",
	"	  	if (c2to1tab[i].c == c) { /* matched */",
	"			if (len++ < n) {",
	"				c = c2to1tab[i].p;",
	"				if (c & 0x80)",
	"				*sout++ = (c & 0x7f) | WCHAR_CS1;",
	"				else *sout++ = c; }",
	"				*sout++ = c2to1tab[i].p; }",
	"			if (c = c2to1tab[i].s)	{",
	"					",
	"					if (len++ < n) {",
	"					if (c & 0x80)",
	"				*secstr-- = WCHAR_CS1 | (c & 0x7f);",
	"					else",
	"						*secstr-- = c; }",
	"					",
	"				}",
	"			sin += 2;",
	"			goto L0;",
	"			}",
	"	   }",
	"L2:",
	"   	if (c = c1to1tab[ui].p) { ",
	"		if (len++ < n) {",
	"			if (c & 0x80)",
	"				*sout++ = (c & 0x7f) | WCHAR_CS1;",
	"			else",
	"				*sout++ = c; }",
	"		if (c = c1to1tab[ui].s) {",
	"				",
	"			   if (len++ < n) {",
	"				if (c & 0x80)",
	"				*sout++ = (c & 0x7f) | WCHAR_CS1;",
	"				else",
	"					*secstr-- = c; }",
	"				",
	"			}",
	"			",
	"		}",
	"  	 sin++;",
	"   	}",
	};

#define	CS0_BOUNDARY	0x007f

void gen_wsxfrm()
{
int	subflag;
int	k;

	subflag = smtomflag | s1tomflag;

	xpr(wxfrm, 1, 7);
	if (subflag)
		xpr(wxfrm, 8, 9);
	if (secflag)
		xpr(wxfrm, 10, 10);
	xpr(wxfrm, 11, 12);
	if (secflag)
		xpr(wxfrm, 14, 24);
	else
		xpr(wxfrm, 16, 24);
	if (smtomflag) {
		xpr(wxfrm, 25, 45);
		if (curprim > CS0_BOUNDARY)
		xpr(wxfrm, 46, 51);
		else
		xpr(wxfrm, 49, 51);
		if (secflag) {
			xpr(wxfrm, 52, 57);
			if (secflag > CS0_BOUNDARY)
				xpr(wxfrm, 58, 60);
		xpr(wxfrm, 61, 70);
		} else
		xpr(wxfrm, 67, 70);
		}
	if (s1tomflag) {
	xpr(wxfrm, 71, 74);
		if (curprim > CS0_BOUNDARY)
		xpr(wxfrm, 75, 80);
		else
		xpr(wxfrm, 78, 80);
		if (secflag) {
			xpr(wxfrm, 81, 86);
			if (secflag > CS0_BOUNDARY)
				xpr(wxfrm, 87, 89);
		xpr(wxfrm, 90, 97);
		} else
		xpr(wxfrm, 96, 97);
		}
	if (c2to1flag) {
	xpr(wxfrm, 99, 111);
		if (curprim > CS0_BOUNDARY)
		xpr(wxfrm, 112, 115);
		else
		xpr(wxfrm, 116, 116);
		if (secflag) {
			xpr(wxfrm, 117, 119);
			if (secflag > CS0_BOUNDARY)
				xpr(wxfrm, 120, 122);
			xpr(wxfrm, 123, 130);
		} else
			xpr(wxfrm, 126, 130);
		}
	if (c1to1flag) {
	xpr(wxfrm, 131, 132);
		if (curprim > CS0_BOUNDARY)
		xpr(wxfrm, 133, 136);
		else
		xpr(wxfrm, 136, 136);
		if (secflag) {
			xpr(wxfrm, 137, 139);
			if (secflag > CS0_BOUNDARY)
				xpr(wxfrm, 140, 142);
			xpr(wxfrm, 143, 147);
		} else
			xpr(wxfrm, 147, 147);
		}
	/* share the same code as strxfrm */
	xpr(xfrm, 99, 102);
	if (secflag)
		xpr(xfrm, 103, 113);
	else
		xpr(xfrm, 114, 114);
	xpr(xfrm, 115, 116);

	if (fprintf(strm, "\n") == EOF)
		err(E_WRITE);
}
