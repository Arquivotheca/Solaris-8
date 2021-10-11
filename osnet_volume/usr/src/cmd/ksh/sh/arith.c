#ident	"@(#)arith.c	1.12	96/08/16 SMI"	/* From AT&T Toolchest */

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * shell arithmetic - uses streval library
 */

#include	"defs.h"
#include	"streval.h"

extern int	sh_lastbase;

#ifdef FLOAT
    extern double atof();
#endif /* FLOAT */

static longlong_t arith(ptr, lvalue, type, n)
char **ptr;
struct lval *lvalue;
longlong_t n;
{
	register longlong_t r= 0;
	char *str = *ptr;
	int	d;
	switch(type)
	{
	case ASSIGN:
	{
		register struct namnod *np = (struct namnod*)(lvalue->value);
		if(nam_istype(np, N_ARRAY))
			array_ptr(np)->cur[0] = lvalue->flag;
		nam_longput(np, n);
		break;
	}
	case LOOKUP:
	{
		register wchar_t c = mb_peekc((const char *)str);
		lvalue->value = (char*)0;
		if(sh_iswalpha(c))
		{
			char	save;
			register struct namnod *np;
			while (mb_nextc((const char **)&str),
				c = mb_peekc((const char*)str), sh_iswalnum(c));
			save = *str;
			*str = 0;
			np = nam_search(*ptr,sh.var_tree,N_ADD);
			*str = save;
			if (c=='(')
			{
				lvalue->value = (char*)e_function;
				str = *ptr;
				break;
			}
			else if(c=='[')
			{
				str =array_subscript(np,str);
			}
			else if(nam_istype(np,N_ARRAY))
				array_dotset(np,ARRAY_UNDEF);
			lvalue->value = (char*)np;
			if(nam_istype(np,N_ARRAY))
				lvalue->flag = array_ptr(np)->cur[0];
		}
		else
		{
#ifdef FLOAT
			char isfloat = 0;
#endif /* FLOAT */
			char *str_save = NULL;
			sh_lastbase = 10;
			while (str_save = str,
				(c= mb_nextc((const char **)&str)))
			switch(c)
			{
			case '#':
				sh_lastbase = r;
				r = 0;
				break;
			case '.':
			{
				/* skip past digits */
				if(sh_lastbase!=10)
					goto badnumber;
				while (str_save = str,
					(c= mb_nextc((const char **)&str))
						!= L'\0' && sh_iswdigit(c));
#ifdef FLOAT
				isfloat = 1;
				if(c=='e' || c == 'E')
				{
				dofloat:
					c = *str;
					if(c=='-'||c=='+')
						c= *++str;
					if(!isdigit(c))
						goto badnumber;
					while(c= *str++,isdigit(c));
				}
				else if(!isfloat)
					goto badnumber;
				set_float();
				r = atof(*ptr);
#endif /* FLOAT */
				goto breakloop;
			}
			default:
				if (sh_iswdigit(c))
					d = c - '0';
				else if (iswascii(c) && iswupper(c))
					d = c - ('A'-10); 
				else if (iswascii(c) && iswlower(c))
					d = c - ('a'-10); 
				else
					goto breakloop;
				if (d < sh_lastbase)
					r = sh_lastbase*r + d;
				else
				{
#ifdef FLOAT
					if(c == 0xe && sh_lastbase==10)
						goto dofloat;
#endif /* FLOAT */
					goto badnumber;
				}
			}
		breakloop:
			str = str_save;
		}
		break;

	badnumber:
		lvalue->value = (char*)e_number;
		return(r);
	
	}
	case VALUE:
	{
		register union Namval *up;
		register struct namnod *np;
		if(is_option(NOEXEC))
			return(0);
		np = (struct namnod*)(lvalue->value);
               	if (nam_istype (np, N_INTGER))
		{
#ifdef NAME_SCOPE
			if (nam_istype (np,N_CWRITE))
				np = nam_copy(np,1);
#endif
			if(nam_istype (np, N_ARRAY))
				up = &(array_find(np,A_ASSIGN)->namval);
			else
				up= &np->value.namval;
			if(nam_istype(np,N_INDIRECT))
				up = up->up;
			if(nam_istype (np, (N_BLTNOD)))
				r = (long)((*up->fp->f_vp)());
			else if(up->lp==NULL)
				r = 0;
#ifdef FLOAT
			else if(nam_istype (np, N_DOUBLE))
			{
				set_float();
       	                	r = *up->dp;
			}
#endif /* FLOAT */
			else
       	                	r = *up->lp;
		}
		else
		{
			if((str=nam_strval(np))==0 || *str==0)
				*ptr = 0;
			else
				r = streval(str, &str, arith);
		}
		return(r);
	}
	case ERRMSG:
        /* XPG4: exit status for test(1) > 1 */
		if (sh.cmdname && ((strcmp(sh.cmdname, "test") == 0) ||
				(strcmp(sh.cmdname, "[") == 0)))
	        cmd_shfail(*ptr, lvalue->value, ETEST);
		else
			sh_fail(*ptr,lvalue->value);
	}

	*ptr = str;
	return(r);
}

longlong_t sh_arith(str)
char *str;
{
	return(streval(str,&str, arith));
}
