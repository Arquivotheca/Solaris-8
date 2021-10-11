#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stddef.h>
#include "colltbl.h"


#define	ESCAPE(x)	(x == '\\' || x == '\"')

void	smm_compute();
void	s1m_compute();
void 	print_smmtab();
void	print_s1mtab();
void	print_c21tab();
void	print_c11tab();
void	prelude();
void	gen_c_file();

extern void	gen_strcoll();
extern void	gen_strxfrm();
extern void	gen_wscoll();
extern void	gen_wsxfrm();

extern int	smtomflag, s1tomflag, c2to1flag, c1to1flag;
extern collnd	colltbl[];
extern c2to1_nd *ptr2to1;
extern s1tom_nd s1tomtab[];
extern smtom_nd *ptrsmtom;
extern int	secflag;
extern int	keepflag;
extern char	*keepfile;

FILE	*strm;

#define	E_CREATE	0
#define	E_WRITE		1

char	cfilename[50];
#define	DOTC	".c"


void mvfile()
{
	char s[100];
	if (keepfile) {
		strcpy(s, "mv ");
		strcat(s, cfilename);
		strcat(s, " ");
		strcat(s, keepfile);
		system(s);
	}
}

void err(eno)
int	eno;
{
	if (eno != E_CREATE) {
		fclose(strm);
		if (keepflag)
			mvfile();
		else
			unlink(cfilename);
	}

	fprintf(stderr, "Cannot generate strcoll.so\n");
	exit(-1);
}


FILE	*openfile()
{

	strcpy(cfilename, tempnam(getenv("TMPDIR"), NULL));
	strcat(cfilename, DOTC);

	if ((strm = fopen(cfilename, "w")) == NULL)
		err(E_CREATE);
	return (strm);
}


void call_cc(outname)
char *outname;
{

#define	CMDLINE	"cc -O -K pic -G -o "
	char	shellcmd[100];
extern	int	Status;

	fclose(strm);
	if (keepflag) {
		mvfile();
		exit(0); /* don't create the dynamic module */
	}
	strcpy(shellcmd, CMDLINE);
	strcat(shellcmd, outname);
	strcat(shellcmd, " ");
	strcat(shellcmd, cfilename);
	if (system(shellcmd)) {
		fprintf(stderr,
		"colltbl: the dynamic module %s is not created successfully\n",
		outname);
		Status = -1;
	}
	unlink(cfilename);
}

/*
 *	generate a .c file.  We use this file to build a .so lib.
 */

void
genlib(outname)
char *outname;
{

	/* handle m-to-m and/or 1-to-m substitution */
	/* replace the "repl" string with the prim weights */
	/* also fill the sec weights if applicable */

	if (smtomflag)
		smm_compute();
	if (s1tomflag)
		s1m_compute();

	/* generate .c file */

	prelude();

	if (smtomflag) {
		print_smmtab();
		}
	if (s1tomflag || smtomflag)
		print_s1mtab();
	if (c2to1flag)
		print_c21tab();

	print_c11tab();

	gen_strcoll();
	gen_strxfrm();
	gen_wscoll();
	gen_wsxfrm();

	call_cc(outname);
}


	/* handle m-to-m  substitution */
	/* replace the "repl" string with the prim weights */
	/* also fill the sec weights if applicable */
	/* also set up the index field in the s1tom table */

void smm_compute()
{
	smtom_nd	*smmptr;
	c2to1_nd	*ptr1;
	char		*c1, *c2, *c3;
	char		c;
	int		j;
	unsigned char	ui;

	for (smmptr=ptrsmtom; smmptr != NULL; smmptr=smmptr->next) {
		if (secflag)
			c3 = smmptr->sec =
				(char *) malloc(strlen(smmptr->repl) + 1);

		for (c1=c2=smmptr->repl; *c1; /* dummy */) {
			for (ptr1=ptr2to1; ptr1 != NULL; ptr1=ptr1->next) {
				if ((ptr1->c[0] == c1[0]) &&
					(ptr1->c[1] == c1[1])) {
				/* match the 2-to-1 collation element */
						*c2++ = ptr1->pwt;
						if (secflag && ptr1->swt)
							*c3++ = ptr1->swt;
						break;
				}
			}
			if (ptr1)
				/* c1[0] and c1[1] combine as a coll element */
				c1 += 2;
			else {
				ui = (unsigned int) *c1;
				if (colltbl[ui].pwt) {
					/* char *c1 is not ignored */
					*c2++ = colltbl[ui].pwt;
					if (secflag && colltbl[ui].swt)
						*c3++ = colltbl[ui].swt;
				}
				c1++;
			}
		}
		*c2 ='\0';
		if (secflag) *c3 = '\0';
	}

	/* establish the index field of 1-to-m subst. table */
	for (j=0, smmptr=ptrsmtom; smmptr != NULL; /* dummy */) {
		j++;
		c = *(smmptr->exp);
		s1tomtab[c & 0xff].index = j;
		for (; smmptr!=NULL; smmptr=smmptr->next) {
			if ((unsigned) *(smmptr->exp) != c)
				break;
			j++;
		}
	}

}

	/* handle  1-to-m substitution */
	/* replace the "repl" string with the prim weights */
	/* also fill the sec weights if applicable */

void s1m_compute()
{
	c2to1_nd	*ptr1;
	char		*c1, *c2, *c3;
	int		i;
	unsigned char	ui;

	for (i=0; i<SZ_COLLATE; i++) {
		if (!s1tomtab[i].repl)
			continue;
		if (secflag)
			c3 = s1tomtab[i].sec =
				(char *) malloc(strlen(s1tomtab[i].repl) + 1);

		for (c1=c2=s1tomtab[i].repl; *c1; /* dummy */) {
			for (ptr1=ptr2to1; ptr1 != NULL; ptr1=ptr1->next) {
				if ((ptr1->c[0] == c1[0]) &&
					(ptr1->c[1] == c1[1])) {
					/* match the 2-to-1 collation element */
					*c2++ = ptr1->pwt;
					if (secflag && ptr1->swt)
						*c3++ = ptr1->swt;
					break;
				}
			}
			if (ptr1)
			/* c1[0] and c1[1] combine as a coll element */
				c1 += 2;
			else {
				ui = (unsigned char) *c1;
				if (colltbl[ui].pwt) {
				/* char *c1 is not ignored */
					*c2++ = colltbl[ui].pwt;
					if (secflag && colltbl[ui].swt)
					/* char *c1 has sec weight */
						*c3++ = colltbl[ui].swt;
				}
				c1++;
			}
		}
		*c2 = '\0';
		if (secflag)
			*c3 = '\0';
	}
}


void print_smmtab() {
	smtom_nd	*smmptr;
	char		*c1, *c2, *c3;
	char		c;

	/* define data structure of smtom table entry */
	fprintf(strm, "\ntypedef struct smtomnd {\n");
	fprintf(strm, "\tconst char *exp;\n");
	fprintf(strm, "\tconst unsigned char *prim;\n");
	if (secflag)
		fprintf(strm, "\tconst unsigned char *sec;\n");
	fprintf(strm, "\t}\tsmtomnd;\n");
	fprintf(strm, "static const smtomnd smtomtab[]= {\n");
	if (secflag)
		fprintf(strm, "\tZ3,\t\t\t/* dummy */\n");
	else
		fprintf(strm, "\tZ2,\t\t\t/* dummy */\n");

	for (smmptr=ptrsmtom; smmptr != NULL; /* dummy for cstyle */) {
		c = *(smmptr->exp);
		for (; smmptr!=NULL; smmptr=smmptr->next) {
			c1=smmptr->exp;
			if (*c1++ != c)
				break;
			fprintf(strm, "\t(const char *) \"");
			for (; *c1; c1++)
				if (ESCAPE(*c1)) {
					putc('\\', strm);
					putc(*c1, strm);
				} else
					putc(*c1, strm);

			fprintf(strm, "\", (const unsigned char *) \"");

			for (c1=smmptr->repl; *c1; c1++)
				fprintf(strm, "\\%o", *c1);
			if (secflag) {
				fprintf(strm, "\", (const unsigned char *)\"");
				for (c1=smmptr->sec; *c1; c1++)
					if (*c1 != 0x01)
						fprintf(strm, "\\%o", *c1);
			}
			fprintf(strm, "\", \n");
		}
		if (secflag)
			fprintf(strm, "\tZ3,\t\t\t/* dummy */\n");
		else
			fprintf(strm, "\tZ2,\t\t\t/* dummy */\n");
	}
	if (fprintf(strm, "\t};\n") == EOF)
		err(E_WRITE);
}


void print_s1mtab()
{
	int	i;
	char	*c1;

	fprintf(strm, "\ntypedef struct s1tomnd {\n");
	if (smtomflag)
		fprintf(strm, "\tconst int index;\n");
	if (s1tomflag) {
		fprintf(strm, "\tconst unsigned char *prim;\n");
		if (secflag)
			fprintf(strm, "\tconst unsigned char *sec;\n");
	}
	fprintf(strm, "\t}\ts1tomnd;\n\n");

	fprintf(strm, "static const s1tomnd s1tomtab[]= {");
	for (i=0; i < SZ_COLLATE; i++) {
		if (!(i % 16))
			fprintf(strm, "\n\t");

		if (smtomflag)
			fprintf(strm, "%d,", s1tomtab[i].index);

		if (!s1tomtab[i].repl) {
			if (s1tomflag) {
				if (secflag)
					fprintf(strm, "Z2, ");
				else
					fprintf(strm, "Z1, ");
			}
		} else {
			fprintf(strm, "/* char %c */", (char) i);
			fprintf(strm, " (const unsigned char *) \"");
			for (c1=s1tomtab[i].repl; *c1; c1++)
				fprintf(strm, "\\%o", *c1);

			if (secflag) {
				fprintf(strm, "\", (const unsigned char *)\"");
				for (c1=s1tomtab[i].sec; *c1; c1++)
					if (*c1 != 0x01)
						fprintf(strm, "\\%o", *c1);
			}
			fprintf(strm, "\", \n\t");
		}
	}
	if (fprintf(strm, "};\n") == EOF)
		err(E_WRITE);
}

void print_c21tab()
{
	c2to1_nd	*cptr1;
	char		c;
	int		i;


	/* define data structure of c2to1 table entry */
	fprintf(strm, "\ntypedef struct c2to1nd {\n");
	fprintf(strm, "\tconst char c;\n");
	fprintf(strm, "\tconst unsigned char p;\n");
	if (secflag) fprintf(strm, "\tconst unsigned char s;\n");
	fprintf(strm, "\t}\tc2to1nd;\n\n");

	fprintf(strm, "static const c2to1nd c2to1tab[] = {\n");
	if (secflag)
		fprintf(strm, "\tZ3,\t/* dummy */\n");
	else
		fprintf(strm, "\tZ2,\t/* dummy */\n");

	for (cptr1 = ptr2to1; cptr1; /* dummy */) {
		for (c=cptr1->c[0]; cptr1; cptr1=cptr1->next) {
			if (c != cptr1->c[0])
				break;
			if (ESCAPE(cptr1->c[1]))
				fprintf(strm, "\t'\\%c', ", cptr1->c[1]);
			else
				fprintf(strm, "\t'%c', ", cptr1->c[1]);

			if (secflag) {
				fprintf(strm, "%d, ", cptr1->pwt);
				if (cptr1->swt == 0x01)
					fprintf(strm, "0, \n");
				else
					fprintf(strm, "%d, \n", cptr1->swt);
			} else
				fprintf(strm, "%d,\n", cptr1->pwt);
		}
		if (secflag)
			fprintf(strm, "\tZ3,\t/* dummy */\n");
		else
			fprintf(strm, "\tZ2,\t/* dummy */\n");
	}

	if (fprintf(strm, "\t};\n") == EOF)
		err(E_WRITE);
}


void print_c11tab()
{
	int	i;
	char	c;
	c2to1_nd	*ptr1;

	if (c2to1flag) /* compute index information */
		for (i=0, ptr1=ptr2to1; ptr1; /* dummy */) {
			i++;
			c=ptr1->c[0];
			colltbl[(unsigned char) c].index = i;
			for (; ptr1; ptr1=ptr1->next, i++)
				if (c != ptr1->c[0])
					break;
		}

	/* define the c1to1nd data structute */
	fprintf(strm, "\ntypedef struct c1to1nd {\n");
	if (c2to1flag)
		fprintf(strm, "\tconst int index;\n");
	if (c1to1flag) {
		fprintf(strm, "\tconst unsigned char p;\n");
		if (secflag)
			fprintf(strm, "\tconst unsigned char s;\n");
	}
	fprintf(strm, "\t}\tc1to1nd;\n\n");

	fprintf(strm, "static const c1to1nd c1to1tab[] = {");
	for (i=0; i<SZ_COLLATE; i++) {
		if (!(i%16))
			fprintf(strm, "\n\t");
		if (c2to1flag)
			fprintf(strm, "%d,", colltbl[i].index);

		if (c1to1flag) {
			if (colltbl[i].pwt) {
				if (secflag) {
					fprintf(strm, "%d, ", colltbl[i].pwt);
					if (colltbl[i].swt == 0x01)
						fprintf(strm, "0, ");
					else
						fprintf(strm, "%d, ",
							colltbl[i].swt);
					fprintf(strm, "\n\t");
				} else
					fprintf(strm, "%d,\n\t",
						colltbl[i].pwt);
			} else {
				if (secflag)
					fprintf(strm, "Z2, ");
				else
					fprintf(strm, "Z1, ");
			}
		}
	}
	if (fprintf(strm, "};\n\n") == EOF)
		err(E_WRITE);
}






void prelude()
{
	strm = openfile();
	fprintf(strm, "#include <stdio.h>\n");
	fprintf(strm, "#include <widec.h>\n");
	fprintf(strm, "#define Z1	0\n");
	fprintf(strm, "#define Z2	0, 0\n");
	fprintf(strm, "#define Z3	0, 0, 0\n");
}
