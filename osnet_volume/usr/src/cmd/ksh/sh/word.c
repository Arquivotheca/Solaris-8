#ident	"@(#)word.c	1.8	98/02/11 SMI"	/* From AT&T Toolchest */

/*
 * UNIX shell
 *
 * S. R. Bourne
 * Rewritten by David Korn
 * AT&T Bell Laboratories
 *
 */

#include	"defs.h"
#include	"sym.h"
#include	"builtins.h"
#include	"io.h"
#ifdef	NEWTEST
#   include	"test.h"
#endif	/* NEWTEST */

static void setupalias();
static wchar_t here_copy();
static int here_tmp();
static int qtrim();
static int qtrim_wcs(wchar_t *);

/* This module defines the following routines */
void	match_paren(int, int, int);

/* This module references these external routines */
extern char	*sh_tilde(char *);

/* ========	character handling for command lines	========*/

/*
 * Get the next word and put it on the top of the stak
 * Determine the type of word and set sh.wdnum and sh.wdset accordingly
 * Returns the token type
 */

/* CSI assumption1(ascii) made here. See csi.h. */
sh_lex()
{
	register wchar_t c;
	register wchar_t d;
	int	i, di;
	register char *argp;
	register int tildp;
	int offset;
	char chk_keywd;
	int 	alpha = 0;
	sh.wdnum=0;
	sh.wdval = 0;
	/* condition needed to check for keywords, name=value */
	chk_keywd = (sh.reserv!=0 && !(sh.wdset&IN_CASE)) || (sh.wdset&KEYFLG);
	sh.wdset &= ~KEYFLG;
	sh.wdarg = (struct argnod*)stakseek(ARGVAL);
	sh.wdarg->argnxt.ap = 0;
	offset = staktell();
	tildp = -1;
	while(1)
	{
		while((c=io_nextc(), sh_iswblank(c)));

		if (c==COMCHAR)
		{
			while ((c=io_readc()) != NL && c != ENDOF);
			io_unreadc(c);
		}
		else	 /* out of comment - white space loop */
			break;
	}
	if(c=='~')
		tildp = offset;
	if(!iswmeta(c))
	{
		do
		{
			if(c==LITERAL)
			{
				match_paren(LITERAL, LITERAL, UseStak);
				alpha = -1;
			}
			else
			{
				if(staktell()==offset && chk_keywd &&
					sh_iswalpha(c))
					alpha++;
				stakputwc(c);
				if(c == ESCAPE)
					stakputwc(io_readc());

				if(alpha>0)
				{
					if (c == '[')
						match_paren('[', ']', UseStak);
					else if(c=='=')
					{
						sh.wdset |= KEYFLG;
						tildp = staktell();
						alpha = 0;
					}
					else if(!sh_iswalnum(c))
						alpha = 0;
				}
				if(wqotchar(c))
					match_paren((int)(c), (int)(c),
						UseStak);
			}
			d = c;
			c = io_nextc();

			if(d==DOLLAR && c==LBRACE)
			{
				stakputwc(c);
				match_paren(LBRACE, RBRACE, UseStak);
				c = io_nextc();
			}
			else if(c==LPAREN && wpatchar(d))
			{
				stakputwc(c);
				match_paren(LPAREN, RPAREN, UseStak);
				c = io_nextc();
			}
			else if(tildp>=0 && 
				(c == '/' || c==':' || iswmeta(c)))
			{
				/* check for tilde expansion */
				stakputascii(0);
				argp=sh_tilde((char *)stakptr(tildp));
				if(argp)
				{
					stakset(stakptr(0),tildp);
					stakputs(argp);
				}
				else
					stakset(stakptr(0),staktell()-1);
				tildp = -1;
			}
			/* tilde substitution after : in variable assignment */
			/* left in as unadvertised compatibility feature */
			if(c==':' && (sh.wdset&KEYFLG))
				tildp = staktell()+1;
		}
		while(!iswmeta(c));
		sh.wdarg = (struct argnod*)stakfreeze(1);
		argp = sh.wdarg->argval;
		io_unreadc(c);
#ifdef	NEWTEST
		if(sh.wdset&IN_TEST)
		{
			if(sh.wdset&TEST_OP1)
			{
				if(argp[0]=='-' && argp[2]==0 &&
					/* mbschr() not needed here */
					strchr(test_unops,argp[1]))
				{
					sh.wdnum = argp[1];
					sh.wdval = TESTUNOP;
				}
				else if(argp[0]=='!' && argp[1]==0)
				{
					sh.wdval = '!';
				}
				else
					sh.wdval = 0;
				sh.wdset &= ~TEST_OP1;
				return(sh.wdval);
			}
			i = sh_lookup(argp, test_optable);
			switch(i)
			{
			case TEST_END:
				return(sh.wdval=ETSTSYM);

			default:
				if(sh.wdset&TEST_OP2)
				{
					sh.wdset &= ~TEST_OP2;
					sh.wdnum = i;
					return(sh.wdval=TESTBINOP);	
				}

			case TEST_OR: case TEST_AND:
			case 0:
				return(sh.wdval = 0);
			}
		}
#endif	/*NEWTEST */
		if(argp[1]==0 &&
			(di=argp[0], sh_isdigit(di)) &&
			(c=='>' || c=='<'))
		{
			sh_lex();
			sh.wdnum |= (IODIGFD|(di-'0'));
		}
		else
		{
			/*check for reserved words and aliases */
			sh.wdval = (sh.reserv!=0?sh_lookup(argp,tab_reserved):0);
			/* for unity database software, allow select to be aliased */
			if((sh.reserv!=0 && (sh.wdval==0||sh.wdval==SELSYM)) || (sh.wdset&CAN_ALIAS))
			{
				/* check for aliases */
				struct namnod* np;
				if((sh.wdset&(IN_CASE|KEYFLG))==0 &&
					(np=nam_search(argp,sh.alias_tree,N_NOSCOPE))
					&& !nam_istype(np,M_FLAG)
					&& (argp=nam_strval(np)))
				{
					setupalias(argp,np);
					ClearPeekn(&st);
					nam_ontype(np,M_FLAG);
					sh.wdset |= KEYFLG;
					return(sh_lex());
				}
			}
		}
	}
	else if (wdipchar(c))
	{
		sh.wdval = (int)(c);
		d = io_nextc();
		if(d==c)
		{
			sh.wdval = (int)(c)|SYMREP;
			if(c=='<')
			{
				if((d=io_nextc())=='-')
					sh.wdnum |= IOSTRIP;
				else
					io_unreadc(d);
			}
			/* arithmetic evaluation ((expr)) */
			else if(c == LPAREN && sh.reserv != 0)
			{
				stakputascii(DQUOTE);
				match_paren(LPAREN, RPAREN, UseStak);
				*stakptr(staktell()-1) = DQUOTE;
				c = io_nextc();
				if(c != ')')
				{
					/*
					 * process as nested () command
					 * for backward compatibility
					 */
					stakputascii(')');
					stakputwc(c);
					sh.wdarg = (struct argnod*)stakfreeze(1);
					qtrim(argp = sh.wdarg->argval);
					setupalias(argp,(struct namnod*)0);

					sh.wdval = '(';
					io_unreadc(L'(');
				}
				else
				{
					sh.wdarg= (struct argnod*)stakfreeze(1);
					return(EXPRSYM);
				}
			}
		}
		else if(c=='|')
		{
			if(d=='&')
				sh.wdval = COOPSYM;
			else
				io_unreadc(d);
		}
#ifdef DEVFD
		else if(d==LPAREN && wiochar(c))
			sh.wdval = (c=='>'?OPROC:IPROC);
#endif	/* DEVFD */
		else if(c==';' && d=='&')
			sh.wdval = ECASYM;
		else
			io_unreadc(d);
	}
	else
	{
		if((sh.wdval=(int)(c))==ENDOF)
		{
			sh.wdval=EOFSYM;
			if(st.standin->ftype==F_ISALIAS)
				io_pop(1);
		}
		if(st.iopend && weolchar(c))
		{
			int	n;

			if(sh.owdval || is_option(NOEXEC))
				n = getlineno(1);
			if(((c = here_copy(st.iopend))==0 || c == (wchar_t)WEOF)
				&& sh.owdval)
			{
				sh.owdval = ('<'|SYMREP);
				sh.wdval = EOFSYM;
				sh.olineno = n;
				sh_syntax();
			}
			st.iopend=0;
		}
	}
	sh.reserv=0;
	return(sh.wdval);
}

/* CSI assumption1(ascii) made here. See csi.h. */
static void setupalias(string,np)
char *string;
struct namnod *np;
{
	register struct fileblk *f;
	register int line;
	f = new_of(struct fileblk,0);
	line = st.standin->flin;
	io_push(f);
	io_sopen(string);
	f->flin = line-1;
	f->ftype = F_ISALIAS;
	f->feval = (char**)np;
	/* add trailing new-line if needed to avoid recursion */
	if((f->flast=st.peekn.buf[0])==MARK && np)
		f->flast = '\n';
}

/*
 * read until matching <close>
 */

void match_paren(open,close, staktype)
register int open, close;
int	staktype;
{
	register wchar_t c;
	register int count = 1;
	register int quoted = 0;
	int was_dollar=0;
	int line = st.standin->flin;
	if(open==LITERAL)
		if (staktype == UseWStak)
			wstakputascii(DQUOTE);
		else
			stakputascii(DQUOTE);
	while(count)
	{
		/* check for unmatched <open> */
		if(quoted || open==LITERAL)
			c = io_readc();
		else
			c = io_nextc();
		if(c==0)
		{
			/* eof before matching quote */
			/* This keeps old shell scripts running */
			if(filenum(st.standin)!=F_STRING || is_option(NOEXEC))
			{
				sh.olineno = line;
				sh.owdval = open;
				sh.wdval = EOFSYM;
				sh_syntax();
			}	
			io_unreadc(0);
			c = close;
		}
		if(c == NL)
		{
			if(open=='[')
			{
				io_unreadc(c);
				break;
			}
			sh_prompt(0);
		}
		else if(c == close)
		{
			if(!quoted)
				count--;
		}
		else if(c == open && !quoted)
			count++;
		if((open == LITERAL) && (wescchar(c) || c=='"'))
			if (staktype == UseWStak)
				wstakputascii(ESCAPE);
			else
				stakputascii(ESCAPE);
		if (staktype == UseWStak)
			wstakputwc(c);
		else
			stakputwc(c);
		if(open==LITERAL)
			continue;
		if(!quoted)
		{

			switch(c)
			{
				case '<':
				case '>':
					if(open==LBRACE)
					{
						/* reserved for future use */
						sh.wdval = (int)(c);
						sh_syntax();
					}
					break;
				case LITERAL:
				case '"':
				case '`':
					/* check for nested '', "", and `` */
					if(open==close)
						break;
					if(c==LITERAL)
						if (staktype == UseWStak)
							stakset(stakptr(0),staktell()-sizeof(wchar_t));
						else
							stakset(stakptr(0),staktell()-1);
					match_paren(c, c, staktype);
					break;
				case LPAREN:
					if(was_dollar && open!=LPAREN)
						match_paren(LPAREN, RPAREN,
							staktype);
					break;
			}
			was_dollar = (c==DOLLAR);
		}
		if (c==ESCAPE)
			quoted = 1 - quoted;
		else
			quoted = 0;
	}
	if(open==LITERAL)
		if (staktype == UseWStak)
			*(wchar_t *)stakptr(staktell()-sizeof(wchar_t)) = 
				(wchar_t)DQUOTE;
		else
			*stakptr(staktell()-1) = DQUOTE;
	return;
}

/*
 * read in here-document from script
 * small non-quoted here-documents are stored as strings 
 * quoted here documents, and here-documents without special chars are
 * treated like file redirection
 */

/* CSI assumption1(ascii) made here. See csi.h. */
static wchar_t here_copy(ioparg)
struct ionod	*ioparg;
{
	register wchar_t	c;
	register char	*bufp;
	register struct ionod *iop;
	wchar_t	*dp;
	int		fd = -1;
	int		match;
	int		i;
	wchar_t		savec = 0;
	int		special = 0;
	int		nosubst;
	char		obuff[IOBSIZE+MB_LEN_MAX+1];

	if(iop=ioparg)
	{
		wchar_t	*wdelim_save;
		int stripflg = iop->iofile&IOSTRIP;
		register int nlflg;
		here_copy(iop->iolst);
		wdelim_save = iop->iodelim = mbstowcs_alloc(iop->ioname);
		/* check for and strip quoted characters in ends */
                nosubst = qtrim_wcs(iop->iodelim);
		if(stripflg)
			while(*iop->iodelim=='\t')
				iop->iodelim++;
		dp = iop->iodelim;
		match = 0;
		nlflg = stripflg;
		bufp = obuff;
		sh_prompt(0);	
		do
		{
			if(nosubst || savec==ESCAPE) {
				c = io_readc();
			} else {
				c = io_nextc();
			}
			if ((savec = c) == 0 || savec == (wchar_t)WEOF)
				break;
			else if(c!=ESCAPE || savec==ESCAPE)
				special |= wescchar(c);
			if(c=='\n')
			{
				if(match>0 && iop->iodelim[match]==0)
				{
					savec =1;
					break;
				}
				if(match>0)
					goto trymatch;
				sh_prompt(0);	
				nlflg = stripflg;
				match = 0;
				goto copy;
			}
			else if(c=='\t' && nlflg)
				continue;
			nlflg = 0;
			/* try matching delimiter when match>=0 */
			if(match>=0)
			{
			trymatch:
				if(iop->iodelim[match]==c)
				{
					match++;
					continue;
				}
				else if(--match>=0)
				{
					io_unreadc(c);
					dp = iop->iodelim;
					c = *dp++;
				}
			}
		copy:
			do
			{
				if(bufp + wcbytes(c) >= &obuff[IOBSIZE])
				{
					if(fd < 0)
						fd = here_tmp(iop);
					write(fd,obuff,(unsigned)(bufp-obuff));
					bufp = obuff;
				}
				bufp += sh_wctomb(bufp, c);
			}
			while(c!='\n' && --match>=0 && (c= *dp++));
		}
		while(savec!=0 && savec!=(wchar_t)WEOF);
		if(i = (nosubst|!special))
                        iop->iofile &= ~IODOC;
		if(fd < 0)
		{
	                if(i)
				fd = here_tmp(iop);
			else
			{
	                        iop->iofile |= IOSTRG;
				*bufp = 0;
				iop->ioname = stakcopy(obuff);
				xfree((void *)wdelim_save);
				return(savec);
			}
		}
		if(bufp > obuff)
			write(fd, obuff, (unsigned)(bufp-obuff));
		close(fd);
		xfree((void *)wdelim_save);
	}
	return(savec);
}

/*
 * create a temporary file for a here document
 */

static int here_tmp(iop)
register struct ionod *iop;
{
	register int fd = io_mktmp((char*)0);
	iop->ioname = stakcopy(io_tmpname);
	iop->iolst=st.iotemp;
	st.iotemp=iop;
	return(fd);
}


/*
 * trim quotes and the escapes
 * returns non-zero if string is quoted 0 otherwise
 */

static int qtrim(string)
char *string;
{
	char *sp = string;
	char *dp = sp;
	register wchar_t c;
	register int quote = 0;

	while (c= mb_nextc((const char **)&sp))
	{
		if(c == ESCAPE)
		{
			quote = 1;
			c = mb_nextc((const char **)&sp);
		}
		else if(c == '"')
		{
			quote = 1;
			continue;
		}
		dp += sh_wctomb(dp, c);
	}
	*dp = 0;
	return(quote);
}

static int qtrim_wcs(string)
wchar_t *string;
{
	wchar_t *sp = string;
	register wchar_t *dp = sp;
	register wchar_t c;
	register int quote = 0;

	while (c= *sp++)
	{
		if(c == ESCAPE)
		{
			quote = 1;
			c = *sp++;
		}
		else if(c == '"')
		{
			quote = 1;
			continue;
		}
		*dp++ = c;
	}
	*dp = 0;
	return(quote);
}
