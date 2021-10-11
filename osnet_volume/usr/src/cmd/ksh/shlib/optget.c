#ident	"@(#)optget.c	1.11	96/08/16 SMI"	/* From AT&T Toolchest */

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * G. S. Fowler
 * AT&T Bell Laboratories
 *
 * command line option parse assist
 *
 *	-- or ++ terminates option list
 *
 *	return:
 *		0	no more options
 *		'?'	unknown option opt_option
 *		':'	option opt_option requires an argument
 *		'#'	option opt_option requires a numeric argument
 *
 *	conditional compilation:
 *
 *		KSHELL	opt_num and opt_argv disabled
 */

#include	"csi.h"
#ifdef KSHELL
#include	"sh_config.h"
#endif

longlong_t	opt_index;		/* argv index			*/
int		opt_char;		/* char pos in argv[opt_index]	*/
int		opt_plus = 0;	/* if set, then don't accept + */
#ifndef KSHELL
long		opt_num;		/* # numeric argument		*/
char**		opt_argv;		/* argv				*/
#endif
char*		opt_arg;		/* {:,#} string argument	*/
char		opt_option[3];		/* current flag {-,+} + option	*/

longlong_t	opt_pindex;		/* prev opt_index for backup	*/
int		opt_pchar;		/* prev opt_char for backup	*/

extern char*	strchr();

#ifndef KSHELL
extern long	strtol();
#endif

/* CSI assumption1(ascii) made here. See csi.h. */
int
optget(argv, opts)
register char**	argv;
char*		opts;
{
	register int	c;
	register char*	s;
#ifndef KSHELL
	char*		e;
#endif

	opt_pindex = opt_index;
	opt_pchar = opt_char;
	/*
	 * VSC/tp6 - Case 6:
	 * optstring=":" optvar="" args="-a fred" OPTIND="2" OPTARG="a fred"
	 * getopts "$optstring" optvar $args
	 * OPTARG should be unset
	 */
	opt_arg = 0;
	for (;;)
	{
		if (!opt_char)
		{
			if (!opt_index)
			{
				opt_index++;
#ifndef KSHELL
				opt_argv = argv;
#endif
			}
			if (!(s = argv[opt_index]) || ((opt_option[0] = *s++) != '-' && (!opt_plus || opt_option[0] != '+')) || !*s) {
				/*
				 * Reset optchar as opt_index has changed
				 * VSC/getopts.sh/tp6-Case 3
				 */
				if (opt_index > opt_pindex)
					opt_char = 0;
				return(0);
			}
			if (*s++ == opt_option[0] && !*s)
			{
				/*
				 * Reset optchar as opt_index has changed
				 * VSC/getopts.sh/tp6-Case 3
				 */
				opt_char = 0;
				opt_index++;
				return(0);
			}
			opt_char++;
		}
		if (opt_option[1] = c = argv[opt_index][opt_char++]) break;
		opt_char = 0;
		opt_index++;
	}
#ifndef KSHELL
	opt_num = 0;
#endif
	if (c == ':' || c == '#' || c == '?' || !(s = mbschr(opts, c)))
	{
#ifdef KSHELL
		/*
		 * Reset optchar as opt_index has changed
		 * VSC/getopts.sh/tp6-Case 3
		 */
		opt_char = 0;
		opt_index++;	/* Increment OPTIND before return */
		return('?');
#else
		if (c < '0' || c > '9' || !(s = strchr(opts, '#')) || s == opts) return('?');
		c = *--s;
#endif
	}
	if (*++s == ':' || *s == '#')
	{
		if (!*(opt_arg = &argv[opt_index++][opt_char]))
		{
			if (!(opt_arg = argv[opt_index]))
			{
				if (*(s + 1) != '?') c = ':';
			}
			else
			{
				opt_index++;
				if (*(s + 1) == '?')
				{
					if (*opt_arg == '-' || (opt_plus && *opt_arg == '+'))
					{
						if (*(opt_arg + 1)) opt_index--;
						opt_arg = 0;
					}
#ifndef KSHELL
					else if (*s++ == '#')
					{
						opt_num = strtol(opt_arg, &e, 0);
						if (*e) opt_arg = 0;
					}
#endif
				}
			}
		}
#ifndef KSHELL
		if (*s == '#' && opt_arg)
		{
			opt_num = strtol(opt_arg, &e, 0);
			if (*e) c = '#';
		}
#endif
		opt_char = 0;
	}
	else if(argv[opt_index][opt_char]==0)
	{
		opt_char=0;
		opt_index++;
	}
	return(c);
}
