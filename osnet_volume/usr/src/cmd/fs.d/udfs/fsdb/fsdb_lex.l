%{
/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fsdb_lex.l	1.4	99/02/13 SMI"

#include <locale.h>
#include "y.tab.h"

extern base;

int	cmd_no = 1;

%}

%e 2000

WS	[ \t]

%%

\n		{ cmd_no++; return NL; }
\.		{ return DOT; }

;{WS}*:{WS}*base{WS}*			{ return BASE; }
^{WS}*:{WS}*base{WS}*			{ return BASE; }
{WS}*:{WS}*block{WS}*			{ return BLOCK; }
;{WS}*:{WS}*cd				{ return CD; }
^{WS}*:{WS}*cd				{ return CD; }
:{WS}*directory{WS}*			{ return DIRECTORY; }
:{WS}*file{WS}*			{ return TFILE; }
;{WS}*:{WS}*find{WS}*			{ return FIND; }
^{WS}*:{WS}*find{WS}*			{ return FIND; }
{WS}*:{WS}*fill{WS}*			{ return FILL; }
:{WS}*inode{WS}*			{ return INODE; }
;{WS}*:{WS}*ls			{ return LS; }
^{WS}*:{WS}*ls			{ return LS; }
;{WS}*:{WS}*override{WS}*		{ return OVERRIDE; }
^{WS}*:{WS}*override{WS}*		{ return OVERRIDE; }
;{WS}*:{WS}*prompt{WS}*		{ return PROMPT; }
^{WS}*:{WS}*prompt{WS}*		{ return PROMPT; }
;{WS}*:{WS}*pwd{WS}*			{ return PWD; }
^{WS}*:{WS}*pwd{WS}*			{ return PWD; }
;{WS}*:{WS}*quit{WS}*			{ return QUIT; }
^{WS}*:{WS}*quit{WS}*			{ return QUIT; }
:{WS}*tag{WS}*			{ return TAG; }
;{WS}*:{WS}*!{WS}*			{ return BANG; }
^{WS}*:{WS}*!{WS}*			{ return BANG; }


:{WS}*avd			{ return AVD; }
:{WS}*mvds			{ return MVDS; }
:{WS}*rvds			{ return RVDS; }
:{WS}*ints			{ return INTS; }
:{WS}*fsds			{ return FSDS; }
:{WS}*root			{ return ROOT; }

:{WS}*attz			{ return ATTZ; }
:{WS}*atye			{ return ATYE; }
:{WS}*atmo			{ return ATMO; }
:{WS}*atda			{ return ATDA; }
:{WS}*atho			{ return ATHO; }
:{WS}*atmi			{ return ATMI; }
:{WS}*atse			{ return ATSE; }
:{WS}*atce			{ return ATCE; }
:{WS}*athu			{ return ATHU; }
:{WS}*atmic			{ return ATMIC; }

:{WS}*cttz			{ return CTTZ; }
:{WS}*ctye			{ return CTYE; }
:{WS}*ctmo			{ return CTMO; }
:{WS}*ctda			{ return CTDA; }
:{WS}*ctho			{ return CTHO; }
:{WS}*ctmi			{ return CTMI; }
:{WS}*ccte			{ return CTSE; }
:{WS}*ctce			{ return CTCE; }
:{WS}*cthu			{ return CTHU; }
:{WS}*ctmic			{ return CTMIC; }

:{WS}*mttz			{ return MTTZ; }
:{WS}*mtye			{ return MTYE; }
:{WS}*mtmo			{ return MTMO; }
:{WS}*mtda			{ return MTDA; }
:{WS}*mtho			{ return MTHO; }
:{WS}*mtmi			{ return MTMI; }
:{WS}*mtse			{ return MTSE; }
:{WS}*mtce			{ return MTCE; }
:{WS}*mthu			{ return MTHU; }
:{WS}*mtmic			{ return MTMIC; }


:{WS}*gid			{ return GID; }
:{WS}*ln			{ return LN; }
:{WS}*md			{ return MD; }
:{WS}*maj			{ return MAJ; }
:{WS}*min			{ return MIO; }
:{WS}*nm			{ return NM; }
:{WS}*sz			{ return SZ; }
:{WS}*uid			{ return UID; }
:{WS}*uniq			{ return UNIQ; }


[-0-9a-zA-Z._]+		{ yylval.strval = yytext; return WORD; }

.		{ return yytext[0]; }

%%

yyerror()
{
	fprintf(stderr,
		gettext("Syntax error line : %d token : %s \n"),
		cmd_no, yytext);
}

/*
:{WS}*fsds			{ return FSDS; }
:{WS}*root			{ return ROOT; }
*/
