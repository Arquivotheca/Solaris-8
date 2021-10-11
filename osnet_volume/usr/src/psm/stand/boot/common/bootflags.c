/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bootflags.c	1.6	99/05/04 SMI"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/reboot.h>
#include <sys/param.h>
#include <sys/salib.h>

extern int	verbosemode, cache_state;
extern char	*impl_arch_name;
extern char	*cmd_line_default_path;
extern void	set_default_filename(char *filename);

struct bootcode {
	char    letter;
	uint_t	bit;
} bootcode[] = {	/* see reboot.h */
	'a',    RB_ASKNAME,
	's',    RB_SINGLE,
	'i',    RB_INITNAME,
	'h',    RB_HALT,
	'b',    RB_NOBOOTRC,
	'd',    RB_DEBUG,
	'w',    RB_WRITABLE,
	'c',	RB_CONFIG,
	'r',	RB_RECONFIG,
	'v',	RB_VERBOSE,
	'k',	RB_KRTLD,
	'f',	RB_FLUSHCACHE,
	'x',	RB_NOBOOTCLUSTER,
	0,	0
};

/*
 * Parse command line to determine boot flags.  We create a
 * new string as a result of the parse which has our own
 * set of private flags removed.
 * Syntax: [file] [[-bootargs [argopts] ...] [--]] [client-prog-args]
 */

#define	isspace(c)	((c) == ' ' || (c) == '\t')

int
bootflags(char *cp)
{
	int i;
	int boothowto = 0;
	int end_of_args = 0;
	static char buf[256];
	char *op = buf;
	char *save_cp = cp;
	static char tmp_impl_arch[MAXNAMELEN];
	static char pathname[256];

	impl_arch_name = NULL;
	cmd_line_default_path = NULL;

	if (cp == NULL)
		return (0);

	/*
	 * skip over filename, if necessary
	 */
	if (*cp != '-')
		while (*cp && !isspace(*cp))
			*op++ = *cp++;
	/*
	 * Skip whitespace.
	 */
	while (isspace(*cp))
		*op++ = *cp++;

	if (*cp == '\0')
		return (0);

	/*
	 * consume the bootflags, if any.
	 */
	while (*cp && (*cp == '-')) {
		++cp;
		if (isspace(*cp))
			break;
		while (*cp && !(isspace(*cp))) {
			switch (*cp) {
			case '-':
				++cp;
				++end_of_args;
				break;
			case 'V':
				verbosemode = 1;
				break;
			case 'n':
				cache_state = 0;
				printf("Warning: boot will not enable cache\n");
				break;
			case 'I':
			case 'D': {
				char *p;

				if (*cp++ == 'D')
					cmd_line_default_path = p = pathname;
				else
					impl_arch_name = p = tmp_impl_arch;

				/* toss white space */
				while (isspace(*cp))
					cp++;
				while (*cp && !isspace(*cp))
					*p++ = *cp++;
				*p = '\0';
				--cp;	/* before the white space char */
				break;
			}
			default:
				for (i = 0; bootcode[i].letter; i++) {
					if (*cp == bootcode[i].letter) {
						boothowto |= bootcode[i].bit;
						break;
					}
				}
				break;
			}
			++cp;
			if (end_of_args)
				break;
		}

		/* the end is near */
		if (end_of_args)
			break;

		/* skip white space - then try again */
		while (isspace(*cp))
			++cp;
	}

	/*
	 * Update the output string only with the bootflags we're
	 * *supposed* to pass on to the standalone.
	 */
	if (boothowto) {
		*op++ = '-';
		for (i = 0; bootcode[i].letter; i++)
			if (bootcode[i].bit & boothowto)
				*op++ = bootcode[i].letter;
	}

	/*
	 * If there's anything else, ensure separation.
	 */
	if (*cp)
		*op++ = ' ';

	/*
	 * Copy the rest of the string, if any..
	 */
	while (*op++ = *cp++)
		;

	/*
	 * Now copy the resulting buffer back onto the original. Sigh.
	 */
	(void) strcpy(save_cp, buf);

	/*
	 * If a default filename is specified in the args, set it.
	 */
	if (cmd_line_default_path)
		set_default_filename(cmd_line_default_path);

	return (boothowto);
}
