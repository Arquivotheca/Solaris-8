/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)rcfile.c	1.1	99/01/31 SMI\n"

#include "bootblk.h"

extern char *strchr_util(char *, char);

typedef struct option_table {
	char	*name;			/* name of entry */
					/* function to call on match if ... */
	char	*(*func)(char *, struct option_table *);
	int	*var;			/* .. this is not zero */
} _option_table_t, *_option_table_p;

char *setload_strap(char *, _option_table_p);
char *setchar_strap(char *, _option_table_p);
char *setint_strap(char *, _option_table_p);
void parserc_strap(char *, int);
void SearchOptions_strap(char *);

extern int reboot_timeout;
extern char *TitleStr;

_option_table_t OptionTable[] = {
	{ "timeout=", setint_strap, &reboot_timeout },
	{ "load=", setload_strap, (int *)0 },		// XXX Can we ditch this?
	{ "title=", setchar_strap, (int *)&TitleStr },	// XXX Can we ditch this?
	{ (char *)0, 0, (int *)0}
};

extern char *bootfile;
extern char far *load_addr;

/*
 * Parse the strap.rc file.  The function is strap-specific for consistency
 * with old strap.com and bootblk behavior.
 *
 * In future we might wish to make the same functionality available to
 * bootblk.  One possibility is to introduce an equivalent file for bootblk.
 * Another is to make bootblk read bootenv.rc and look for bootblk-related
 * options.  If we do that, we should probably change strap to do it too.
 *
 * Originally the strap.rc file could contain option lines (word "Options"
 * followed by one or more name=value pairs), empty lines, comment lines
 * (strating with '#') and boot file lines (description in quotes followed
 * by the filename and any options).  If more than one boot file was given
 * strap would put up a menu for the user to choose.  This behavior was
 * intended for allowing the user to choose between ufsboot and inetboot
 * but is obsolete since their unification into boot.bin.  So the form of
 * the lines remains unchanged (the boot file is preceeded by a descriptive
 * string) but no menu is presented and the last such entry is used, or
 * the program default if none.  Boot file options are ignored.
 */
void
parserc_strap(char *buf, int size)
{
	char *p;
	char *boot_options = 0;

        while (buf && *buf) {
        	/*
        	 * Comment character found. Just dump the rest of the
        	 * line.
        	 */
		if (*buf == '#') {
			if (buf = strchr_util(buf, '\r'))
				buf++;
			else
				break;
                }
                /*
                 * Options keyword found. Need to process everything on the
                 * rest of this current line as an option statement.
                 */
		else if ((*buf == 'O') && !strncmp_util(buf, "Options", 7)) {
			if (p = strchr_util(buf, '\r'))
				*p++ = '\0';
			if (buf = strchr_util(buf, ' ')) {
				SearchOptions_strap(buf + 1);
			}
			buf = p;
		/*
		 * At this point either the next character is a double quote
		 * which starts a valid line or we need to chuck the
		 * character. That's why I advance buf pointer in the if
		 * statement.
		 */
		} else if (*buf++ == '"') {
                        while (*buf && *buf != '"')
                                buf++;

			/* ---- null terminate user_name string ---- */
			if (*buf == '"')
				*buf++ = '\0';
			/* else
			 *	Syntax error folks. I'm not going to worry
			 *	about it for two reasons. 1) only intelligent
			 *	engineers :-) should be creating this file
			 *	and 2) the alogrithm will just exit the loop
			 *	creating null poniters for the boot_name.
			 */

			/* ---- search for start of boot_name ---- */
                        while (*buf && (*buf == ' ' || *buf == '\t'))
                                buf++;

                        bootfile = buf;
                        boot_options = 0;

			/*
			 * look for either end of line or the start of the
			 * optional arguments
			 */
                        while (*buf && *buf != '\r' && *buf != ' ' &&
                            *buf != '\t')
                                buf++;

			/* ---- null terminate boot_name ---- */
			if (*buf == 0) {
				/* ---- boot_name is followed by EOF ---- */
				break;
                        }
                        if (*buf == '\r') {
				/* ---- boot_name is followed by newline ---- */
				*buf++ = '\0';
                        } else if (*buf && *buf != '\r') {
				/* ---- boot_name is followed by options ---- */
                                *buf++ = '\0';

				while (*buf && (*buf == ' ' || *buf == '\t'))
					buf++;

				boot_options = buf;

				while (*buf && *buf != '\r')
					buf++;
				if (*buf)
					*buf++ = '\0';
			}
                }
        }
	SearchOptions_strap(boot_options);
}

void
SearchOptions_strap(char *buf)
{
	_option_table_p op;

	do {
		for (op = &OptionTable[0]; op->name; op++)
		  if (strncmp_util(buf, op->name, strlen_util(op->name)) == 0) {
			  buf = (*op->func)(strchr_util(buf, '=') + 1, op);
			  break;
		  }

		/* ---- if we didn't find a match leave, error message? ---- */
		if (op->name == (char *)0)
		  break;

		/* ---- eat the white space ---- */
		while ((*buf == ' ') || (*buf == '\t'))
		  buf++;
	} while (*buf);
}

/*
 *[]------------------------------------------------------------[]
 * | This routine is called because someone has set 'load=...'	|
 * | in the strap.rc file. The syntax is: xxxx.xxxx		|
 * | The first four digits are the segment number and the second|
 * | four are the offset. The number are always interpreted as	|
 * | hex and only the significant numbers need to be present.	|
 * | i.e. 0.8000 or 800.0 are both valid and represent the same	|
 * | physical address. The dot '.' can be any non valid hex	|
 * | character.							|
 * | Rick McNeal -- 12-Jun-1995					|
 *[]------------------------------------------------------------[]
 */
char *
setload_strap(char *s, _option_table_p op)
{
	seg_ptr LoadAddr;

	LoadAddr.s.segp = strtol_util(s, &s, 16);
	LoadAddr.s.offp = strtol_util(s + 1, &s, 16);
	load_addr = LoadAddr.p;
	return s;
}

/*
 *[]------------------------------------------------------------[]
 * | setchar_strap -- stores a pointer to the string found in	|
 * | the rc file in a global variable. The length of the string	|
 * | is either the end of line or terminated by a double quote.	|
 *[]------------------------------------------------------------[]
 */
char *
setchar_strap(char *s, _option_table_p op)
{
	char *p;

	/*
	 * If the string value starts with a quote character use everything
	 * up to the next quote returning the buffer pointer after our last
	 * quote character. Else assume they want everything.
	 */
	if ((*s == '"') && (p = strchr_util(++s, '"')))
	  *p++ = '\0';
	else
	  p = (char *)0;
	*((char **)op->var) = s;

	return p;
}

/*
 *[]------------------------------------------------------------[]
 * | setint_strap -- converts character string found in rc file	|
 * | to integer and stores that in the global variable for later|
 * | use by strap.						|
 *[]------------------------------------------------------------[]
 */
char *
setint_strap(char *s, _option_table_p op)
{
	*op->var = strtol_util(s, &s, 0);
	return s;
}
