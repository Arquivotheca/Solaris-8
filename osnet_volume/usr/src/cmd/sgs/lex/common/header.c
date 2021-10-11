/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)header.c	6.19	98/01/28 SMI"

#include "ldefs.c"

phead1()
{
	ratfor ? rhd1() : chd1();
}

chd1()
{
	if (*v_stmp == 'y')
		(void)  fprintf(fout, "#ident\t\"lex: %s %s\"\n",
		    (const char *)SGU_PKG, (const char *)SGU_REL);
	if (handleeuc) {
		fprintf(fout, "#ifndef EUC\n");
		fprintf(fout, "#define EUC\n");
		fprintf(fout, "#endif\n");
		fprintf(fout, "#include <stdio.h>\n");
		fprintf(fout, "#include <stdlib.h>\n");
		fprintf(fout, "#include <inttypes.h>\n");
		fprintf(fout, "#include <widec.h>\n");
		if (widecio) { /* -w option */
			fprintf(fout, "#define YYTEXT yytext\n");
			fprintf(fout, "#define YYLENG yyleng\n");
			fprintf(fout, "#ifndef __cplusplus\n");
			fprintf(fout, "#define YYINPUT input\n");
			fprintf(fout, "#define YYOUTPUT output\n");
			fprintf(fout, "#else\n");
			fprintf(fout, "#define YYINPUT lex_input\n");
			fprintf(fout, "#define YYOUTPUT lex_output\n");
			fprintf(fout, "#endif\n");
			fprintf(fout, "#define YYUNPUT unput\n");
		} else { /* -e option */
			fprintf(fout, "#include <limits.h>\n");
			fprintf(fout, "#include <sys/euc.h>\n");
			fprintf(fout, "#define YYLEX_E 1\n");
			fprintf(fout, "#define YYTEXT yywtext\n");
			fprintf(fout, "#define YYLENG yywleng\n");
			fprintf(fout, "#define YYINPUT yywinput\n");
			fprintf(fout, "#define YYOUTPUT yywoutput\n");
			fprintf(fout, "#define YYUNPUT yywunput\n");
		}
	} else { /* ASCII compatibility mode. */
		fprintf(fout, "#include <stdio.h>\n");
		fprintf(fout, "#include <stdlib.h>\n");
		fprintf(fout, "#include <inttypes.h>\n");
	}
	if (ZCH > NCH)
		(void) fprintf(fout, "# define U(x) ((x)&0377)\n");
	else
	(void) fprintf(fout, "# define U(x) x\n");
	(void) fprintf(fout, "# define NLSTATE yyprevious=YYNEWLINE\n");
	(void) fprintf(fout, "# define BEGIN yybgin = yysvec + 1 +\n");
	(void) fprintf(fout, "# define INITIAL 0\n");
	(void) fprintf(fout, "# define YYLERR yysvec\n");
	(void) fprintf(fout, "# define YYSTATE (yyestate-yysvec-1)\n");
	if (optim)
		(void) fprintf(fout, "# define YYOPTIM 1\n");
#ifdef DEBUG
	(void) fprintf(fout, "# define LEXDEBUG 1\n");
#endif
	(void) fprintf(fout, "# ifndef YYLMAX \n");
	(void) fprintf(fout, "# define YYLMAX BUFSIZ\n");
	(void) fprintf(fout, "# endif \n");
	(void) fprintf(fout, "#ifndef __cplusplus\n");
	if (widecio)
		(void) fprintf(fout,
		"# define output(c) (void)putwc(c,yyout)\n");
	else
		(void) fprintf(fout,
		"# define output(c) (void)putc(c,yyout)\n");
	(void) fprintf(fout, "#else\n");
	if (widecio)
		(void) fprintf(fout,
		"# define lex_output(c) (void)putwc(c,yyout)\n");
	else
		(void) fprintf(fout,
		"# define lex_output(c) (void)putc(c,yyout)\n");
	(void) fprintf(fout, "#endif\n");
	(void) fprintf(fout,
	"\n#if defined(__cplusplus) || defined(__STDC__)\n");
	(void) fprintf(fout,
	"\n#if defined(__cplusplus) && defined(__EXTERN_C__)\n");
	(void) fprintf(fout, "extern \"C\" {\n");
	(void) fprintf(fout, "#endif\n");
	(void) fprintf(fout, "\tint yyback(int *, int);\n"); /* ? */
	(void) fprintf(fout, "\tint yyinput(void);\n"); /* ? */
	(void) fprintf(fout, "\tint yylook(void);\n"); /* ? */
	(void) fprintf(fout, "\tvoid yyoutput(int);\n"); /* ? */
	(void) fprintf(fout, "\tint yyracc(int);\n"); /* ? */
	(void) fprintf(fout, "\tint yyreject(void);\n"); /* ? */
	(void) fprintf(fout, "\tvoid yyunput(int);\n"); /* ? */
	(void) fprintf(fout, "\tint yylex(void);\n");
	(void) fprintf(fout, "#ifdef YYLEX_E\n");
	(void) fprintf(fout, "\tvoid yywoutput(wchar_t);\n");
	(void) fprintf(fout, "\twchar_t yywinput(void);\n");
	(void) fprintf(fout, "#endif\n");

	/* XCU4: type of yyless is int */
	(void) fprintf(fout, "#ifndef yyless\n");
	(void) fprintf(fout, "\tint yyless(int);\n");
	(void) fprintf(fout, "#endif\n");
	(void) fprintf(fout, "#ifndef yywrap\n");
	(void) fprintf(fout, "\tint yywrap(void);\n");
	(void) fprintf(fout, "#endif\n");
	(void) fprintf(fout, "#ifdef LEXDEBUG\n");
	(void) fprintf(fout, "\tvoid allprint(char);\n");
	(void) fprintf(fout, "\tvoid sprint(char *);\n");
	(void) fprintf(fout, "#endif\n");
	(void) fprintf(fout,
	"#if defined(__cplusplus) && defined(__EXTERN_C__)\n");
	(void) fprintf(fout, "}\n");
	(void) fprintf(fout, "#endif\n\n");
	(void) fprintf(fout, "#ifdef __cplusplus\n");
	(void) fprintf(fout, "extern \"C\" {\n");
	(void) fprintf(fout, "#endif\n");
	(void) fprintf(fout, "\tvoid exit(int);\n");
	(void) fprintf(fout, "#ifdef __cplusplus\n");
	(void) fprintf(fout, "}\n");
	(void) fprintf(fout, "#endif\n\n");
	(void) fprintf(fout, "#endif\n");
	(void) fprintf(fout,
	"# define unput(c)"
	" {yytchar= (c);if(yytchar=='\\n')yylineno--;*yysptr++=yytchar;}\n");
	(void) fprintf(fout, "# define yymore() (yymorfg=1)\n");
	if (widecio) {
		(void) fprintf(fout, "#ifndef __cplusplus\n");
		(void) fprintf(fout, "%s%d%s\n",
"# define input() (((yytchar=yysptr>yysbuf?U(*--yysptr):getwc(yyin))==",
		ctable['\n'],
"?(yylineno++,yytchar):yytchar)==EOF?0:yytchar)");
		(void) fprintf(fout, "#else\n");
		(void) fprintf(fout, "%s%d%s\n",
"# define lex_input() (((yytchar=yysptr>yysbuf?U(*--yysptr):getwc(yyin))==",
		ctable['\n'],
"?(yylineno++,yytchar):yytchar)==EOF?0:yytchar)");
		(void) fprintf(fout, "#endif\n");
		(void) fprintf(fout,
		"# define ECHO (void)fprintf(yyout, \"%%ws\",yytext)\n");
		(void) fprintf(fout,
		"# define REJECT { nstr = yyreject_w(); goto yyfussy;}\n");
		(void) fprintf(fout, "#define yyless yyless_w\n");
		(void) fprintf(fout, "int yyleng;\n");

		/*
		 * XCU4:
		 * If %array, yytext[] contains the token.
		 * If %pointer, yytext is a pointer to yy_tbuf[].
		 */

		if (isArray) {
			(void) fprintf(fout, "#define YYISARRAY\n");
			(void) fprintf(fout, "wchar_t yytext[YYLMAX];\n");
		} else {
			(void) fprintf(fout, "wchar_t yy_tbuf[YYLMAX];\n");
			(void) fprintf(fout, "wchar_t * yytext = yy_tbuf;\n");
			(void) fprintf(fout, "int yytextsz = YYLMAX;\n");
			(void) fprintf(fout, "#ifndef YYTEXTSZINC\n");
			(void) fprintf(fout, "#define YYTEXTSZINC 100\n");
			(void) fprintf(fout, "#endif\n");
		}
	} else {
		(void) fprintf(fout, "#ifndef __cplusplus\n");
		(void) fprintf(fout, "%s%d%s\n",
"# define input() (((yytchar=yysptr>yysbuf?U(*--yysptr):getc(yyin))==",
		ctable['\n'],
"?(yylineno++,yytchar):yytchar)==EOF?0:yytchar)");
		(void) fprintf(fout, "#else\n");
		(void) fprintf(fout, "%s%d%s\n",
"# define lex_input() (((yytchar=yysptr>yysbuf?U(*--yysptr):getc(yyin))==",
		ctable['\n'],
"?(yylineno++,yytchar):yytchar)==EOF?0:yytchar)");
		(void) fprintf(fout, "#endif\n");
		fprintf(fout, "#define ECHO fprintf(yyout, \"%%s\",yytext)\n");
		if (handleeuc) {
			(void) fprintf(fout,
"# define REJECT { nstr = yyreject_e(); goto yyfussy;}\n");
			(void) fprintf(fout, "int yyleng;\n");
			(void) fprintf(fout, "size_t yywleng;\n");
			/*
			 * XCU4:
			 * If %array, yytext[] contains the token.
			 * If %pointer, yytext is a pointer to yy_tbuf[].
			 */
			if (isArray) {
				(void) fprintf(fout, "#define YYISARRAY\n");
				(void) fprintf(fout,
				"unsigned char yytext[YYLMAX*MB_LEN_MAX];\n");
				(void) fprintf(fout,
				"wchar_t yywtext[YYLMAX];\n");
			} else {
				(void) fprintf(fout,
				"wchar_t yy_twbuf[YYLMAX];\n");
				(void) fprintf(fout,
				"wchar_t yy_tbuf[YYLMAX*MB_LEN_MAX];\n");
				(void) fprintf(fout,
				"unsigned char * yytext ="
				"(unsigned char *)yy_tbuf;\n");
				(void) fprintf(fout,
				"wchar_t * yywtext = yy_twbuf;\n");
				(void) fprintf(fout,
						"int yytextsz = YYLMAX;\n");
				(void) fprintf(fout, "#ifndef YYTEXTSZINC\n");
				(void) fprintf(fout,
					"#define YYTEXTSZINC 100\n");
				(void) fprintf(fout, "#endif\n");
			}
		} else {
			(void) fprintf(fout,
"# define REJECT { nstr = yyreject(); goto yyfussy;}\n");
			(void) fprintf(fout, "int yyleng;\n");

			/*
			 * XCU4:
			 * If %array, yytext[] contains the token.
			 * If %pointer, yytext is a pointer to yy_tbuf[].
			 */
			if (isArray) {
				(void) fprintf(fout, "#define YYISARRAY\n");
				(void) fprintf(fout, "char yytext[YYLMAX];\n");
			} else {
				(void) fprintf(fout, "char yy_tbuf[YYLMAX];\n");
				(void) fprintf(fout,
				"char * yytext = yy_tbuf;\n");
				(void) fprintf(fout,
					"int yytextsz = YYLMAX;\n");
				(void) fprintf(fout, "#ifndef YYTEXTSZINC\n");
				(void) fprintf(fout,
					"#define YYTEXTSZINC 100\n");
				(void) fprintf(fout, "#endif\n");
			}
		}
	}
	(void) fprintf(fout, "int yymorfg;\n");
	if (handleeuc)
		(void) fprintf(fout, "extern wchar_t *yysptr, yysbuf[];\n");
	else
		(void) fprintf(fout, "extern char *yysptr, yysbuf[];\n");
	(void) fprintf(fout, "int yytchar;\n");
	(void) fprintf(fout, "FILE *yyin = {stdin}, *yyout = {stdout};\n");
	(void) fprintf(fout, "extern int yylineno;\n");
	(void) fprintf(fout, "struct yysvf { \n");
	(void) fprintf(fout, "\tstruct yywork *yystoff;\n");
	(void) fprintf(fout, "\tstruct yysvf *yyother;\n");
	(void) fprintf(fout, "\tint *yystops;};\n");
	(void) fprintf(fout, "struct yysvf *yyestate;\n");
	(void) fprintf(fout, "extern struct yysvf yysvec[], *yybgin;\n");
}

rhd1()
{
	(void) fprintf(fout, "integer function yylex(dummy)\n");
	(void) fprintf(fout, "define YYLMAX 200\n");
	(void) fprintf(fout, "define ECHO call yyecho(yytext,yyleng)\n");
	(void) fprintf(fout,
	"define REJECT nstr = yyrjct(yytext,yyleng);goto 30998\n");
	(void) fprintf(fout, "integer nstr,yylook,yywrap\n");
	(void) fprintf(fout, "integer yyleng, yytext(YYLMAX)\n");
	(void) fprintf(fout, "common /yyxel/ yyleng, yytext\n");
	(void) fprintf(fout,
	"common /yyldat/ yyfnd, yymorf, yyprev, yybgin, yylsp, yylsta\n");
	(void) fprintf(fout,
	"integer yyfnd, yymorf, yyprev, yybgin, yylsp, yylsta(YYLMAX)\n");
	(void) fprintf(fout, "for(;;){\n");
	(void) fprintf(fout, "\t30999 nstr = yylook(dummy)\n");
	(void) fprintf(fout, "\tgoto 30998\n");
	(void) fprintf(fout, "\t30000 k = yywrap(dummy)\n");
	(void) fprintf(fout, "\tif(k .ne. 0){\n");
	(void) fprintf(fout, "\tyylex=0; return; }\n");
	(void) fprintf(fout, "\t\telse goto 30998\n");
}

phead2()
{
	if (!ratfor)
		chd2();
}

chd2()
{
	(void) fprintf(fout, "#ifdef __cplusplus\n");
	(void) fprintf(fout,
	"/* to avoid CC and lint complaining yyfussy not being used ...*/\n");
	(void) fprintf(fout, "static int __lex_hack = 0;\n");
	(void) fprintf(fout, "if (__lex_hack) goto yyfussy;\n");
	(void) fprintf(fout, "#endif\n");
	(void) fprintf(fout, "while((nstr = yylook()) >= 0)\n");
	(void) fprintf(fout, "yyfussy: switch(nstr){\n");
	(void) fprintf(fout, "case 0:\n");
	(void) fprintf(fout, "if(yywrap()) return(0); break;\n");
}

ptail()
{
	if (!pflag)
		ratfor ? rtail() : ctail();
	pflag = 1;
}

ctail()
{
	(void) fprintf(fout, "case -1:\nbreak;\n");		/* for reject */
	(void) fprintf(fout, "default:\n");
	(void) fprintf(fout,
	"(void)fprintf(yyout,\"bad switch yylook %%d\",nstr);\n");
	(void) fprintf(fout, "} return(0); }\n");
	(void) fprintf(fout, "/* end of yylex */\n");
}

rtail()
{
	register int i;
	(void) fprintf(fout,
	"\n30998 if(nstr .lt. 0 .or. nstr .gt. %d)goto 30999\n", casecount);
	(void) fprintf(fout, "nstr = nstr + 1\n");
	(void) fprintf(fout, "goto(\n");
	for (i = 0; i < casecount; i++)
		(void) fprintf(fout, "%d,\n", 30000+i);
	(void) fprintf(fout, "30999),nstr\n");
	(void) fprintf(fout, "30997 continue\n");
	(void) fprintf(fout, "}\nend\n");
}

statistics()
{
	(void) fprintf(errorf,
"%d/%d nodes(%%e), %d/%d positions(%%p), %d/%d (%%n), %ld transitions,\n",
	tptr, treesize, nxtpos-positions, maxpos, stnum + 1, nstates, rcount);
	(void) fprintf(errorf,
	"%d/%d packed char classes(%%k), ", pcptr-pchar, pchlen);
	if (optim)
		(void) fprintf(errorf,
		" %d/%d packed transitions(%%a), ", nptr, ntrans);
	(void) fprintf(errorf, " %d/%d output slots(%%o)", yytop, outsize);
	(void) putc('\n', errorf);
}
