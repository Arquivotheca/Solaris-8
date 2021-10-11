
/*
 * Copyright (c) 1991,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)io.c	1.11	98/01/24 SMI"

/*
 * This file contains I/O related functions.
 */
#include "global.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/tty.h>
#include <sys/termio.h>
#include <sys/termios.h>

#include "startup.h"
#include "misc.h"
#include "menu_partition.h"
#include "param.h"
#include "menu.h"

extern int	data_lineno;
extern char	*space2str();
extern long	strtol();

/*
 * This variable is used to determine whether a token is present in the pipe
 * already.
 */
static	char token_present = 0;

/*
 * This variable always gives us access to the most recent token type
 */
int	last_token_type = 0;

/*
 * Prototypes for ANSI C compilers
 */
static	int	sup_get_token(char *);


/*
 * This routine pushes the given character back onto the input stream.
 */
void
pushchar(c)
	int	c;
{

	(void) ungetc(c, stdin);
}

/*
 * This routine checks the input stream for an eof condition.
 */
int
checkeof()
{

	return (feof(stdin));
}

/*
 * This routine gets the next token off the input stream.  A token is
 * basically any consecutive non-white characters.
 */
char *
gettoken(inbuf)
	char	*inbuf;
{
	char	*ptr = inbuf;
	int	c, quoted = 0;

retoke:
	/*
	 * Remove any leading white-space.
	 */
	while ((isspace(c = getchar())) && (c != '\n'))
		;
	/*
	 * If we are at the beginning of a line and hit the comment character,
	 * flush the line and start again.
	 */
	if (!token_present && c == COMMENT_CHAR) {
		token_present = 1;
		flushline();
		goto retoke;
	}
	/*
	 * Loop on each character until we hit unquoted white-space.
	 */
	while (!isspace(c) || quoted && (c != '\n')) {
		/*
		 * If we hit eof, get out.
		 */
		if (checkeof())
			return (NULL);
		/*
		 * If we hit a double quote, change the state of quotedness.
		 */
		if (c == '"')
			quoted = !quoted;
		/*
		 * If there's room in the buffer, add the character to the end.
		 */
		else if (ptr - inbuf < TOKEN_SIZE)
			*ptr++ = (char)c;
		/*
		 * Get the next character.
		 */
		c = getchar();
	}
	/*
	 * Null terminate the token.
	 */
	*ptr = '\0';
	/*
	 * Peel off white-space still in the pipe.
	 */
	while (isspace(c) && (c != '\n'))
		c = getchar();
	/*
	 * If we hit another token, push it back and set state.
	 */
	if (c != '\n') {
		pushchar(c);
		token_present = 1;
	} else
		token_present = 0;
	/*
	 * Return the token.
	 */
	return (inbuf);
}

/*
 * This routine removes the leading and trailing spaces from a token.
 */
void
clean_token(cleantoken, token)
	char	*cleantoken, *token;
{
	char	*ptr;

	/*
	 * Strip off leading white-space.
	 */
	for (ptr = token; isspace(*ptr); ptr++)
		;
	/*
	 * Copy it into the clean buffer.
	 */
	(void) strcpy(cleantoken, ptr);
	/*
	 * Strip off trailing white-space.
	 */
	for (ptr = cleantoken + strlen(cleantoken) - 1;
			isspace(*ptr) && (ptr >= cleantoken); ptr--) {
		*ptr = '\0';
	}
}

/*
 * This routine checks if a token is already present on the input line
 */
int
istokenpresent()
{
	return (token_present);
}


/*
 * This routine flushes the rest of an input line if there is known
 * to be data in it.  The flush has to be qualified because the newline
 * may have already been swallowed by the last gettoken.
 */
void
flushline()
{
	if (token_present) {
		/*
		 * Flush the pipe to eol or eof.
		 */
		while ((getchar() != '\n') && !checkeof())
			;
		/*
		 * Mark the pipe empty.
		 */
		token_present = 0;
	}
}

/*
 * This routine returns the number of characters that are identical
 * between s1 and s2, stopping as soon as a mismatch is found.
 */
int
strcnt(s1, s2)
	char	*s1, *s2;
{
	int	i = 0;

	while ((*s1 != '\0') && (*s1++ == *s2++))
		i++;
	return (i);
}

/*
 * This routine converts the given token into an integer.  The token
 * must convert cleanly into an integer with no unknown characters.
 * If the token is the wildcard string, and the wildcard parameter
 * is present, the wildcard value will be returned.
 */
int
geti(str, iptr, wild)
	char	*str;
	int	*iptr, *wild;
{
	char	*str2;

	/*
	 * If there's a wildcard value and the string is wild, return the
	 * wildcard value.
	 */
	if (wild != NULL && strcmp(str, WILD_STRING) == 0)
		*iptr = *wild;
	else {
		/*
		 * Conver the string to an integer.
		 */
		*iptr = (int)strtol(str, &str2, 0);
		/*
		 * If any characters didn't convert, it's an error.
		 */
		if (*str2 != '\0') {
			err_print("`%s' is not an integer.\n", str);
			return (-1);
		}
	}
	return (0);
}

/*
 * This routine converts the given string into a block number on the
 * current disk.  The format of a block number is either a self-based
 * number, or a series of self-based numbers separated by slashes.
 * Any number preceeding the first slash is considered a cylinder value.
 * Any number succeeding the first slash but preceeding the second is
 * considered a head value.  Any number succeeding the second slash is
 * considered a sector value.  Any of these numbers can be wildcarded
 * to the highest possible legal value.
 */
int
getbn(str, iptr)
	char	*str;
	daddr_t	*iptr;
{
	char	*cptr, *hptr, *sptr;
	int	cyl, head, sect, wild;
	TOKEN	buf;

	/*
	 * Set cylinder pointer to beginning of string.
	 */
	cptr = str;
	/*
	 * Look for the first slash.
	 */
	while ((*str != '\0') && (*str != '/'))
		str++;
	/*
	 * If there wasn't one, convert string to an integer and return it.
	 */
	if (*str == '\0') {
		wild = physsects() - 1;
		if (geti(cptr, (int *)iptr, &wild))
			return (-1);
		return (0);
	}
	/*
	 * Null out the slash and set head pointer just beyond it.
	 */
	*str++ = '\0';
	hptr = str;
	/*
	 * Look for the second slash.
	 */
	while ((*str != '\0') && (*str != '/'))
		str++;
	/*
	 * If there wasn't one, sector pointer points to a .
	 */
	if (*str == '\0')
		sptr = str;
	/*
	 * If there was, null it out and set sector point just beyond it.
	 */
	else {
		*str++ = '\0';
		sptr = str;
	}
	/*
	 * Convert the cylinder part to an integer and store it.
	 */
	clean_token(buf, cptr);
	wild = ncyl + acyl - 1;
	if (geti(buf, &cyl, &wild))
		return (-1);
	if ((cyl < 0) || (cyl >= (ncyl + acyl))) {
		err_print("`%d' is out of range.\n", cyl);
		return (-1);
	}
	/*
	 * Convert the head part to an integer and store it.
	 */
	clean_token(buf, hptr);
	wild = nhead - 1;
	if (geti(buf, &head, &wild))
		return (-1);
	if ((head < 0) || (head >= nhead)) {
		err_print("`%d' is out of range.\n", head);
		return (-1);
	}
	/*
	 * Convert the sector part to an integer and store it.
	 */
	clean_token(buf, sptr);
	wild = sectors(head) - 1;
	if (geti(buf, &sect, &wild))
		return (-1);
	if ((sect < 0) || (sect >= sectors(head))) {
		err_print("`%d' is out of range.\n", sect);
		return (-1);
	}
	/*
	 * Combine the pieces into a block number and return it.
	 */
	*iptr = chs2bn(cyl, head, sect);
	return (0);
}

/*
 * This routine is the basis for all input into the program.  It
 * understands the semantics of a set of input types, and provides
 * consistent error messages for all input.  It allows for default
 * values and prompt strings.
 */
int
input(type, promptstr, delim, param, deflt, cmdflag)
	int		type;
	char		*promptstr;
	int		delim;
	u_ioparam_t	*param;
	int		*deflt;
	int		cmdflag;
{
	int		interactive, help, i, length, index, tied;
	daddr_t		bn;
	char		**str, **strings;
	TOKEN		token, cleantoken;
	TOKEN		token2, cleantoken2;
	struct		bounds *bounds;
	char		*s;
	int		value;
	int		cyls;
	float		nmegs;
	float		ngigs;
	char		shell_argv[MAXPATHLEN];

	/*
	 * Optional integer input has been added as a hack.
	 * Function result is 1 if user typed anything.
	 * Whatever they typed is returned in *deflt.
	 * This permits us to distinguish between "no value",
	 * and actually entering in some value, for instance.
	 */
	if (type == FIO_OPINT) {
		assert(deflt != NULL);
	}
reprompt:
	help = interactive = 0;
	/*
	 * If we are inputting a command, flush any current input in the pipe.
	 */
	if (cmdflag == CMD_INPUT)
		flushline();
	/*
	 * Note whether the token is already present.
	 */
	if (!token_present)
		interactive = 1;
	/*
	 * Print the prompt.
	 */
	fmt_print(promptstr);
	/*
	 * If there is a default value, print it in a format appropriate
	 * for the input type.
	 */
	if (deflt != NULL) {
		switch (type) {
		case FIO_BN:
			fmt_print("[%d, ", *deflt);
			pr_dblock(fmt_print, (daddr_t)*deflt);
			fmt_print("]");
			break;
		case FIO_INT:
			fmt_print("[%d]", *deflt);
			break;
		case FIO_CSTR:
		case FIO_MSTR:
			strings = (char **)param->io_charlist;
			for (i = 0, str = strings; i < *deflt; i++, str++)
				;
			fmt_print("[%s]", *str);
			break;
		case FIO_OSTR:
			fmt_print("[\"%s\"]", (char *)deflt);
			break;
		case FIO_SLIST:
			/*
			 * Search for a string matching the default
			 * value.  If found, use it.  Otherwise
			 * assume the default value is actually
			 * an illegal choice, and default to
			 * the first item in the list.
			 */
			s = find_string(param->io_slist, *deflt);
			if (s == (char *)NULL) {
				s = (param->io_slist)->str;
			}
			fmt_print("[%s]", s);
			break;
		case FIO_CYL:
			fmt_print("[%db, %dc, %1.2fmb, %1.2fgb]", *deflt,
				bn2c(*deflt), bn2mb(*deflt), bn2gb(*deflt));
			break;
		case FIO_OPINT:
			/* no default value for optional input type */
			fmt_print("[default]");
			break;
		default:
			err_print("Error: unknown input type.\n");
			fullabort();
		}
	}
	/*
	 * Print the delimiter character.
	 */
	fmt_print("%c ", delim);
	/*
	 * Get the token.  If we hit eof, exit the program gracefully.
	 */
	if (gettoken(token) == NULL)
		fullabort();

	/*
	 * check if the user has issued (!) , escape to shell
	 */
	if ((cmdflag == CMD_INPUT) && (token[0] == '!')) {

	    /* get the list of arguments to shell command */
		memset(shell_argv, 0, MAXPATHLEN);
		if (strlen(token) > 1)
			strcpy(shell_argv, &token[1]);

		/*
		 * collect all tokens till the end of line as arguments
		 */
		while (token_present && (gettoken(token) != NULL)) {
			strcat(shell_argv, " ");
			strcat(shell_argv, token);
		}

		/* execute the shell command */
		(void) execute_shell(shell_argv);
		redisplay_menu_list((char **)param->io_charlist);
		if (interactive) {
			goto reprompt;
		}
	}

	/*
	 * Certain commands accept up to two tokens
	 * Unfortunately, this is kind of a hack.
	 */
	token2[0] = 0;
	cleantoken2[0] = 0;
	if (type == FIO_CYL) {
		if (token_present) {
			if (gettoken(token2) == NULL)
				fullabort();
			clean_token(cleantoken2, token2);
		}
	}
	/*
	 * Echo the token back to the user if it was in the pipe or we
	 * are running out of a command file.
	 */
	if (!interactive || option_f) {
		if (token2[0] == 0) {
			fmt_print("%s\n", token);
		} else {
			fmt_print("%s %s\n", token, token2);
		}
	}
	/*
	 * If we are logging, echo the token to the log file.  The else
	 * is necessary here because the above printf will also put the
	 * token in the log file.
	 */
	else if (log_file) {
		log_print("%s %s\n", token, token2);
	}
	/*
	 * If the token was not in the pipe and it wasn't a command, flush
	 * the rest of the line to keep things in sync.
	 */
	if (interactive && cmdflag != CMD_INPUT)
		flushline();
	/*
	 * Scrub off the white-space.
	 */
	clean_token(cleantoken, token);
	/*
	 * If the input was a blank line and we weren't prompting
	 * specifically for a blank line...
	 */
	if ((strcmp(cleantoken, "") == 0) && (type != FIO_BLNK)) {
		/*
		 * If there's a default, return it.
		 */
		if (deflt != NULL) {
			if (type == FIO_OSTR) {
				/*
				 * Duplicate and return the default string
				 */
				return ((int)alloc_string((char *)deflt));
			} else if (type == FIO_SLIST) {
				/*
				 * If we can find a match for the default
				 * value in the list, return the default
				 * value.  If there's no match for the
				 * default value, it's an illegal
				 * choice.  Return the first value in
				 * the list.
				 */
				s = find_string(param->io_slist, *deflt);
				if (s == (char *)NULL) {
					return ((param->io_slist)->value);
				} else {
					return (*deflt);
				}
			} else if (type == FIO_OPINT) {
				/*
				 * The user didn't enter anything
				 */
				return (0);
			} else {
				return (*deflt);
			}
		}
		/*
		 * If the blank was not in the pipe, just reprompt.
		 */
		if (interactive) {
			goto reprompt;
		}
		/*
		 * If the blank was in the pipe, it's an error.
		 */
		err_print("No default for this entry.\n");
		cmdabort(SIGINT);
	}
	/*
	 * If token is a '?' or a 'h', it is a request for help.
	 */
	if ((strcmp(cleantoken, "?") == 0) ||
		(strcmp(cleantoken, "h") == 0) ||
			(strcmp(cleantoken, "help") == 0)) {
		help = 1;
	}
	/*
	 * Switch on the type of input expected.
	 */
	switch (type) {
	/*
	 * Expecting a disk block number.
	 */
	case FIO_BN:
		/*
		 * Parameter is the bounds of legal block numbers.
		 */
		bounds = (struct bounds *)&param->io_bounds;
		/*
		 * Print help message if required.
		 */
		if (help) {
			fmt_print("Expecting a block number from %d (",
				bounds->lower);
			pr_dblock(fmt_print, bounds->lower);
			fmt_print(") to %d (", bounds->upper);
			pr_dblock(fmt_print, bounds->upper);
			fmt_print(")\n");
			break;
		}
		/*
		 * Convert token to a disk block number.
		 */
		if (getbn(cleantoken, &bn))
			break;
		/*
		 * Check to be sure it is within the legal bounds.
		 */
		if ((bn < bounds->lower) || (bn > bounds->upper)) {
			err_print("`");
			pr_dblock(err_print, bn);
			err_print("' is out of range.\n");
			break;
		}
		/*
		 * If it's ok, return it.
		 */
		return (bn);
	/*
	 * Expecting an integer.
	 */
	case FIO_INT:
		/*
		 * Parameter is the bounds of legal integers.
		 */
		bounds = (struct bounds *)&param->io_bounds;
		/*
		 * Print help message if required.
		 */
		if (help) {
			fmt_print("Expecting an integer from %d",
				bounds->lower);
			fmt_print(" to %d\n", bounds->upper);
			break;
		}
		/*
		 * Convert the token into an integer.
		 */
		if (geti(cleantoken, (int *)&bn, (int *)NULL))
			break;
		/*
		 * Check to be sure it is within the legal bounds.
		 */
		if ((bn < bounds->lower) || (bn > bounds->upper)) {
			err_print("`%d' is out of range.\n", bn);
			break;
		}
		/*
		 * If it's ok, return it.
		 */
		return (bn);
	/*
	 * Expecting an integer, or no input.
	 */
	case FIO_OPINT:
		/*
		 * Parameter is the bounds of legal integers.
		 */
		bounds = (struct bounds *)&param->io_bounds;
		/*
		 * Print help message if required.
		 */
		if (help) {
			fmt_print("Expecting an integer from %d",
				bounds->lower);
			fmt_print(" to %d, or no input\n", bounds->upper);
			break;
		}
		/*
		 * Convert the token into an integer.
		 */
		if (geti(cleantoken, (int *)&bn, (int *)NULL))
			break;
		/*
		 * Check to be sure it is within the legal bounds.
		 */
		if ((bn < bounds->lower) || (bn > bounds->upper)) {
			err_print("`%d' is out of range.\n", bn);
			break;
		}
		/*
		 * For optional case, return 1 indicating that
		 * the user actually did enter something.
		 */
		*deflt = bn;
		return (1);
	/*
	 * Expecting a closed string.  This means that the input
	 * string must exactly match one of the strings passed in
	 * as the parameter.
	 */
	case FIO_CSTR:
		/*
		 * The parameter is a null terminated array of character
		 * pointers, each one pointing to a legal input string.
		 */
		strings = (char **)param->io_charlist;
		/*
		 * Walk through the legal strings, seeing if any of them
		 * match the token.  If a match is made, return the index
		 * of the string that was matched.
		 */
		for (str = strings; *str != NULL; str++)
			if (strcmp(cleantoken, *str) == 0)
				return (str - strings);
		/*
		 * Print help message if required.
		 */
		if (help) {
			print_input_choices(type, param);
		} else {
			err_print("`%s' is not expected.\n", cleantoken);
		}
		break;
	/*
	 * Expecting a matched string.  This means that the input
	 * string must either match one of the strings passed in,
	 * or be a unique abbreviation of one of them.
	 */
	case FIO_MSTR:
		/*
		 * The parameter is a null terminated array of character
		 * pointers, each one pointing to a legal input string.
		 */
		strings = (char **)param->io_charlist;
		length = index = tied = 0;
		/*
		 * Loop through the legal input strings.
		 */
		for (str = strings; *str != NULL; str++) {
			/*
			 * See how many characters of the token match
			 * this legal string.
			 */
			i = strcnt(cleantoken, *str);
			/*
			 * If it's not the whole token, then it's not a match.
			 */
			if ((u_int) i < strlen(cleantoken))
				continue;
			/*
			 * If it ties with another input, remember that.
			 */
			if (i == length)
				tied = 1;
			/*
			 * If it matches the most so far, record that.
			 */
			if (i > length) {
				index = str - strings;
				tied = 0;
				length = i;
			}
		}
		/*
		 * Print help message if required.
		 */
		if (length == 0) {

			if (help) {
				print_input_choices(type, param);
			} else {
				err_print("`%s' is not expected.\n",
					cleantoken);
			}
			break;
		}
		/*
		 * If the abbreviation was non-unique, it's an error.
		 */
		if (tied) {
			err_print("`%s' is ambiguous.\n", cleantoken);
			break;
		}
		/*
		 * We matched one.  Return the index of the string we matched.
		 */
		return (index);
	/*
	 * Expecting an open string.  This means that any string is legal.
	 */
	case FIO_OSTR:
		/*
		 * Print a help message if required.
		 */
		if (help) {
			fmt_print("Expecting a string\n");
			break;
		}
		/*
		 * alloc a copy of the string and return it
		 */
		return ((int)alloc_string(token));

	/*
	 * Expecting a blank line.
	 */
	case FIO_BLNK:
		/*
		 * We are always in non-echo mode when we are inputting
		 * this type.  We echo the newline as a carriage return
		 * only so the prompt string will be covered over.
		 */
		nolog_print("\015");
		/*
		 * If we are logging, send a newline to the log file.
		 */
		if (log_file)
			log_print("\n");
		/*
		 * There is no value returned for this type.
		 */
		return (0);

	/*
	 * Expecting one of the entries in a string list.
	 * Accept unique abbreviations.
	 * Return the value associated with the matched string.
	 */
	case FIO_SLIST:
		i = find_value((slist_t *)param->io_slist,
			cleantoken, &value);
		if (i == 1) {
			return (value);
		} else {
			/*
			 * Print help message if required.
			 */

			if (help) {
				print_input_choices(type, param);
			} else {
				if (i == 0)
					err_print("`%s' not expected.\n",
						cleantoken);
				else
					err_print("`%s' is ambiguous.\n",
						cleantoken);
			}
		}
		break;

	case FIO_CYL:
		/*
		 * Parameter is the bounds of legal block numbers.
		 */
		bounds = (struct bounds *)&param->io_bounds;
		assert(bounds->lower == 0);
		/*
		 * Print help message if required.
		 */
		if (help) {
			fmt_print("Expecting up to %d blocks,",
				bounds->upper);
			fmt_print(" %d cylinders, ", bn2c(bounds->upper));
			fmt_print(" %1.2f megabytes, ", bn2mb(bounds->upper));
			fmt_print("or %1.2f gigabytes\n", bn2gb(bounds->upper));
			break;
		}
		/*
		 * Parse the first token: try to find 'b', 'c' or 'm'
		 */
		s = cleantoken;
		while (*s && (isdigit(*s) || (*s == '.') || (*s == '$'))) {
			s++;
		}
		/*
		 * If we found a conversion specifier, second token is unused
		 * Otherwise, the second token should supply it.
		 */
		if (*s != 0) {
			value = *s;
			*s = 0;
		} else {
			value = cleantoken2[0];
		}
		/*
		 * If the token is the wild card, simply supply the max
		 * This order allows the user to specify the maximum in
		 * either blocks/cyls/megabytes - a convenient fiction.
		 */
		if (strcmp(cleantoken, WILD_STRING) == 0) {
			return (bounds->upper);
		}
		/*
		 * Allow the user to specify zero with no units,
		 * by just defaulting to cylinders.
		 */
		if (strcmp(cleantoken, "0") == 0) {
			value = 'c';
		}
		/*
		 * If there's a decimal point, but no unit specification,
		 * let's assume megabytes.
		 */
		if ((value == 0) && (strchr(cleantoken, '.') != NULL)) {
			value = 'm';
		}
		/*
		 * Handle each unit type we support
		 */
		switch (value) {
		case 'b':
			/*
			* Convert token to a disk block number.
			*/
			i = bounds->upper;
			if (geti(cleantoken, &value, &i))
				break;
			bn = value;
			/*
			* Check to be sure it is within the legal bounds.
			*/
			if ((bn < bounds->lower) || (bn > bounds->upper)) {
				err_print(
				"`%db' is out of the range %d to %d\n",
				bn, bounds->lower, bounds->upper);
				break;
			}
			/*
			 * Verify the block lies on a cylinder boundary
			 */
			if ((bn % spc()) != 0) {
				err_print("\
partition size must be a multiple of %d blocks to lie on a cylinder boundary\n",
					spc());
				err_print("\
%d blocks is approximately %d cylinders, %1.2f\
megabytes or %1.2f gigabytes\n", bn, bn2c(bn), bn2mb(bn), bn2gb(bn));
				break;
			}
			return (bn);
		case 'c':
			/*
			 * Convert token from a number of cylinders to
			 * a number of blocks.
			 */
			i = bn2c(bounds->upper);
			if (geti(cleantoken, &cyls, &i))
				break;
			/*
			 * Check the bounds - cyls is number of cylinders
			 */
			if (cyls > (bounds->upper/spc())) {
				err_print("`%dc' is out of range\n", cyls);
				break;
			}
			/*
			 * Convert cylinders to blocks and return
			 */
			return (cyls * spc());
		case 'm':
			/*
			 * Convert token from megabytes to a block number.
			 */
			if (sscanf(cleantoken, "%f2", &nmegs) == 0) {
				err_print("`%s' is not recognized\n",
					cleantoken);
				break;
			}
			/*
			 * Check the bounds
			 */
			if (nmegs > bn2mb(bounds->upper)) {
				err_print("`%1.2fmb' is out of range\n", nmegs);
				break;
			}
			/*
			 * Convert to blocks
			 */
			bn = mb2bn(nmegs);
			/*
			 * Round value up to nearest cylinder
			 */
			i = spc();
			bn = ((bn + (i-1)) / i) * i;
			return (bn);
		case 'g':
			/*
			 * Convert token from gigabytes to a block number.
			 */
			if (sscanf(cleantoken, "%f2", &ngigs) == 0) {
				err_print("`%s' is not recognized\n",
					cleantoken);
				break;
			}
			/*
			 * Check the bounds
			 */
			if (ngigs > bn2gb(bounds->upper)) {
				err_print("`%1.2fgb' is out of range\n", ngigs);
				break;
			}
			/*
			 * Convert to blocks
			 */
			bn = gb2bn(ngigs);
			/*
			 * Round value up to nearest cylinder
			 */
			i = spc();
			bn = ((bn + (i-1)) / i) * i;
			return (bn);
		default:
			err_print(
"Please specify units in either b(blocks), c(cylinders), m(megabytes) \
or g(gigabytes)\n");
			break;
		}
		break;

	/*
	 * If we don't recognize the input type, it's bad news.
	 */
	default:
		err_print("Error: unknown input type.\n");
		fullabort();
	}
	/*
	 * If we get here, it's because some error kept us from accepting
	 * the token.  If we are running out of a command file, gracefully
	 * leave the program.  If we are interacting with the user, simply
	 * reprompt.  If the token was in the pipe, abort the current command.
	 */
	if (option_f)
		fullabort();
	else if (interactive)
		goto reprompt;
	else
		cmdabort(SIGINT);
	/*
	 * Never actually reached.
	 */
	return (-1);
}


/*
 * Print input choices
 */
void
print_input_choices(type, param)
	int		type;
	u_ioparam_t	*param;
{
	char		**sp;
	slist_t		*lp;
	int		width;
	int		col;
	int		ncols;

	switch (type) {
	case FIO_CSTR:
		fmt_print("Expecting one of the following:\n");
		goto common;

	case FIO_MSTR:
		fmt_print("Expecting one of the following: ");
		fmt_print("(abbreviations ok):\n");
common:
		for (sp = (char **)param->io_charlist; *sp != NULL; sp++) {
			fmt_print("\t%s\n", *sp);
		}
		break;

	case FIO_SLIST:
		fmt_print("Expecting one of the following: ");
		fmt_print("(abbreviations ok):\n");
		/*
		 * Figure out the width of the widest string
		 */
		width = slist_widest_str((slist_t *)param->io_slist);
		width += 4;
		/*
		 * If the help messages are empty, print the
		 * possible choices in left-justified columns
		 */
		lp = (slist_t *)param->io_slist;
		if (*lp->help == 0) {
			col = 0;
			ncols = 60 / width;
			for (; lp->str != NULL; lp++) {
				if (col == 0)
					fmt_print("\t");
				ljust_print(lp->str,
					(++col == ncols) ? 0 : width);
				if (col == ncols) {
					col = 0;
					fmt_print("\n");
				}
			}
			if (col != 0)
				fmt_print("\n");
		} else {
			/*
			 * With help messages, print each choice,
			 * and help message, on its own line.
			 */
			for (; lp->str != NULL; lp++) {
				fmt_print("\t");
				ljust_print(lp->str, width);
				fmt_print("- %s\n", lp->help);
			}
		}
		break;

	default:
		err_print("Error: unknown input type.\n");
		fullabort();
	}

	fmt_print("\n");
}


/*
 * Search a string list for a particular string.
 * Use minimum recognition, to accept unique abbreviations
 * Return the number of possible matches found.
 * If only one match was found, return the arbitrary value
 * associated with the matched string in match_value.
 */
int
find_value(slist, match_str, match_value)
	slist_t		*slist;
	char		*match_str;
	int		*match_value;
{
	int	i;
	int	nmatches;
	int	length;
	int	match_length;

	nmatches = 0;
	length = 0;

	match_length = strlen(match_str);

	for (; slist->str != NULL; slist++) {
		/*
		 * See how many characters of the token match
		 */
		i = strcnt(match_str, slist->str);
		/*
		 * If it's not the whole token, then it's not a match.
		 */
		if (i  < match_length)
			continue;
		/*
		 * If it ties with another input, remember that.
		 */
		if (i == length)
			nmatches++;
		/*
		 * If it matches the most so far, record that.
		 */
		if (i > length) {
			*match_value = slist->value;
			nmatches = 1;
			length = i;
		}
	}

	return (nmatches);
}


/*
 * Search a string list for a particular value.
 * Return the string associated with that value.
 */
char *
find_string(slist, match_value)
	slist_t		*slist;
	int		match_value;
{
	for (; slist->str != NULL; slist++) {
		if (slist->value == match_value) {
			return (slist->str);
		}
	}

	return ((char *)NULL);
}


/*
 * Return the width of the widest string in an slist
 */
int
slist_widest_str(slist)
	slist_t	*slist;
{
	int	i;
	int	width;

	width = 0;
	for (; slist->str != NULL; slist++) {
		if ((i = strlen(slist->str)) > width)
			width = i;
	}

	return (width);
}


/*
 * Print a string left-justified to a fixed width.
 */
void
ljust_print(str, width)
	char	*str;
	int	width;
{
	int	i;

	fmt_print("%s", str);
	for (i = width - strlen(str); i > 0; i--) {
		fmt_print(" ");
	}
}


/*
 * This routine is a modified version of printf.  It handles the cases
 * of silent mode and logging; other than that it is identical to the
 * library version.
 */
/*VARARGS1*/
void
fmt_print(char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	/*
	 * If we are running silent, skip it.
	 */
	if (option_s == 0) {
		/*
		 * Do the print to standard out.
		 */
		if (need_newline) {
			(void) printf("\n");
		}
		(void) vprintf(format, ap);
		/*
		 * If we are logging, also print to the log file.
		 */
		if (log_file) {
			if (need_newline) {
				(void) fprintf(log_file, "\n");
			}
			(void) vfprintf(log_file, format, ap);
			(void) fflush(log_file);
		}
	}

	need_newline = 0;

	va_end(ap);
}

/*
 * This routine is a modified version of printf.  It handles the cases
 * of silent mode; other than that it is identical to the
 * library version.  It differs from the above printf in that it does
 * not print the message to a log file.
 */
/*VARARGS1*/
void
nolog_print(char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	/*
	 * If we are running silent, skip it.
	 */
	if (option_s == 0) {
		/*
		 * Do the print to standard out.
		 */
		if (need_newline) {
			(void) printf("\n");
		}
		(void) vprintf(format, ap);
	}

	va_end(ap);

	need_newline = 0;
}

/*
 * This routine is a modified version of printf.  It handles the cases
 * of silent mode, and only prints the message to the log file, not
 * stdout.  Other than that is identical to the library version.
 */
/*VARARGS1*/
void
log_print(char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	/*
	 * If we are running silent, skip it.
	 */
	if (option_s == 0) {
		/*
		 * Do the print to the log file.
		 */
		if (need_newline) {
			(void) fprintf(log_file, "\n");
		}
		(void) vfprintf(log_file, format, ap);
		(void) fflush(log_file);
	}

	va_end(ap);

	need_newline = 0;
}


/*
 * This routine is a modified version of printf.  It prints the message
 * to stderr, and to the log file is appropriate.
 * Other than that is identical to the library version.
 */
/*VARARGS1*/
void
err_print(char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	/*
	 * Flush anything pending to stdout
	 */
	if (need_newline) {
		(void) printf("\n");
	}
	(void) fflush(stdout);
	/*
	 * Do the print to stderr.
	 */
	(void) vfprintf(stderr, format, ap);
	/*
	 * If we are logging, also print to the log file.
	 */
	if (log_file) {
		if (need_newline) {
			(void) fprintf(log_file, "\n");
		}
		(void) vfprintf(log_file, format, ap);
		(void) fflush(log_file);
	}
	va_end(ap);

	need_newline = 0;
}


/*
 * Print a number of characters from a buffer.  The buffer
 * does not need to be null-terminated.  Since the data
 * may be coming from a device, we cannot be sure the
 * data is not crud, so be rather defensive.
 */
void
print_buf(buf, nbytes)
	char	*buf;
	int	nbytes;
{
	int	c;

	while (nbytes-- > 0) {
		c = *buf++;
		if (isascii(c) && isprint(c)) {
			fmt_print("%c", c);
		} else
			break;
	}
}


#ifdef	not
/*
 * This routine prints out a message describing the given ctlr.
 * The message is identical to the one printed by the kernel during
 * booting.
 */
void
pr_ctlrline(ctlr)
	register struct ctlr_info *ctlr;
{

	fmt_print("           %s%d at %s 0x%x ",
		ctlr->ctlr_cname, ctlr->ctlr_num,
		space2str(ctlr->ctlr_space), ctlr->ctlr_addr);
	if (ctlr->ctlr_vec != 0)
		fmt_print("vec 0x%x ", ctlr->ctlr_vec);
	else
		fmt_print("pri %d ", ctlr->ctlr_prio);
	fmt_print("\n");
}
#endif	not

/*
 * This routine prints out a message describing the given disk.
 * The message is identical to the one printed by the kernel during
 * booting.
 */
void
pr_diskline(disk, num)
	register struct disk_info *disk;
	int	num;
{
	struct	ctlr_info *ctlr = disk->disk_ctlr;
	struct	disk_type *type = disk->disk_type;

	fmt_print("    %4d. %s ", num, disk->disk_name);
	if (type != NULL) {
		fmt_print("<%s cyl %d alt %d hd %d sec %d>",
			type->dtype_asciilabel, type->dtype_ncyl,
			type->dtype_acyl, type->dtype_nhead,
			type->dtype_nsect);
	} else if (disk->disk_flags & DSK_RESERVED) {
		fmt_print("<drive not available: reserved>");
	} else if (disk->disk_flags & DSK_UNAVAILABLE) {
		fmt_print("<drive not available: formatting>");
	} else {
		fmt_print("<drive type unknown>",
			ctlr->ctlr_dname, disk->disk_dkinfo.dki_unit);
	}
	if (chk_volname(disk)) {
		fmt_print("  ");
		print_volname(disk);
	}
	fmt_print("\n");

	if (disk->devfs_name != NULL) {
		fmt_print("          %s\n", disk->devfs_name);
	} else {
		fmt_print("          %s%d at %s%d slave %d\n",
			ctlr->ctlr_dname, disk->disk_dkinfo.dki_unit,
			ctlr->ctlr_cname, ctlr->ctlr_num,
			disk->disk_dkinfo.dki_slave);
	}

#ifdef	OLD
	fmt_print("    %4d. %s at %s%d slave %d", num, disk->disk_name,
		ctlr->ctlr_cname, ctlr->ctlr_num, disk->disk_dkinfo.dki_slave);
	if (chk_volname(disk)) {
		fmt_print(": ");
		print_volname(disk);
	}
	fmt_print("\n");
	if (type != NULL) {
		fmt_print(
"           %s%d: <%s cyl %d alt %d hd %d sec %d>\n",
			ctlr->ctlr_dname, disk->disk_dkinfo.dki_unit,
			type->dtype_asciilabel, type->dtype_ncyl,
			type->dtype_acyl, type->dtype_nhead,
			type->dtype_nsect);
	} else {
		fmt_print("           %s%d: <drive type unknown>\n",
			ctlr->ctlr_dname, disk->disk_dkinfo.dki_unit);
	}
#endif	OLD
}

/*
 * This routine prints out a given disk block number in cylinder/head/sector
 * format.  It uses the printing routine passed in to do the actual output.
 */
void
pr_dblock(func, bn)
	void	(*func)();
	daddr_t	bn;
{

	(*func)("%d/%d/%d", bn2c(bn), bn2h(bn), bn2s(bn));
}

/*
 * This routine inputs a character from the data file.  It understands
 * the use of '\' to prevent interpretation of a newline.  It also keeps
 * track of the current line in the data file via a global variable.
 */
int
sup_inputchar()
{
	int	c;

	/*
	 * Input the character.
	 */
	c = getc(data_file);
	/*
	 * If it's not a backslash, return it.
	 */
	if (c != '\\')
		return (c);
	/*
	 * It was a backslash.  Get the next character.
	 */
	c = getc(data_file);
	/*
	 * If it was a newline, update the line counter and get the next
	 * character.
	 */
	if (c == '\n') {
		data_lineno++;
		c = getc(data_file);
	}
	/*
	 * Return the character.
	 */
	return (c);
}

/*
 * This routine pushes a character back onto the input pipe for the data file.
 */
void
sup_pushchar(c)
	int	c;
{

	(void) ungetc(c, data_file);
}

/*
 * Variables to support pushing back tokens
 */
static	int	have_pushed_token = 0;
static	TOKEN	pushed_buf;
static	int	pushed_token;

/*
 * This routine inputs a token from the data file.  A token is a series
 * of contiguous non-white characters or a recognized special delimiter
 * character.  Use of the wrapper lets us always have the value of the
 * last token around, which is useful for error recovery.
 */
int
sup_gettoken(buf)
	char	*buf;
{
	last_token_type = sup_get_token(buf);
	return (last_token_type);
}

static int
sup_get_token(buf)
	char	*buf;
{
	char	*ptr = buf;
	int	c, quoted = 0;

	/*
	 * First check for presence of push-backed token.
	 * If so, return it.
	 */
	if (have_pushed_token) {
		have_pushed_token = 0;
		bcopy(pushed_buf, buf, TOKEN_SIZE+1);
		return (pushed_token);
	}
	/*
	 * Zero out the returned token buffer
	 */
	bzero(buf, TOKEN_SIZE + 1);
	/*
	 * Strip off leading white-space.
	 */
	while ((isspace(c = sup_inputchar())) && (c != '\n'))
		;
	/*
	 * Read in characters until we hit unquoted white-space.
	 */
	for (; !isspace(c) || quoted; c = sup_inputchar()) {
		/*
		 * If we hit eof, that's a token.
		 */
		if (feof(data_file))
			return (SUP_EOF);
		/*
		 * If we hit a double quote, change the state of quoting.
		 */
		if (c == '"') {
			quoted = !quoted;
			continue;
		}
		/*
		 * If we hit a newline, that delimits a token.
		 */
		if (c == '\n')
			break;
		/*
		 * If we hit any nonquoted special delimiters, that delimits
		 * a token.
		 */
		if (!quoted && (c == '=' || c == ',' || c == ':' ||
			c == '#' || c == '|' || c == '&' || c == '~'))
				break;
		/*
		 * Store the character if there's room left.
		 */
		if (ptr - buf < TOKEN_SIZE)
			*ptr++ = (char)c;
	}
	/*
	 * If we stored characters in the buffer, then we inputted a string.
	 * Push the delimiter back into the pipe and return the string.
	 */
	if (ptr - buf > 0) {
		sup_pushchar(c);
		return (SUP_STRING);
	}
	/*
	 * We didn't input a string, so we must have inputted a known delimiter.
	 * store the delimiter in the buffer, so it will get returned.
	 */
	buf[0] = c;
	/*
	 * Switch on the delimiter.  Return the appropriate value for each one.
	 */
	switch (c) {
	case '=':
		return (SUP_EQL);
	case ':':
		return (SUP_COLON);
	case ',':
		return (SUP_COMMA);
	case '\n':
		return (SUP_EOL);
	case '|':
		return (SUP_OR);
	case '&':
		return (SUP_AND);
	case '~':
		return (SUP_TILDE);
	case '#':
		/*
		 * For comments, we flush out the rest of the line and return
		 * an eol.
		 */
		while ((c = sup_inputchar()) != '\n' && !feof(data_file))
			;
		if (feof(data_file))
			return (SUP_EOF);
		else
			return (SUP_EOL);
	/*
	 * Shouldn't ever get here.
	 */
	default:
		return (SUP_STRING);
	}
}

/*
 * Push back a token
 */
void
sup_pushtoken(token_buf, token_type)
	char	*token_buf;
	int	token_type;
{
	/*
	 * We can only push one token back at a time
	 */
	assert(have_pushed_token == 0);

	have_pushed_token = 1;
	bcopy(token_buf, pushed_buf, TOKEN_SIZE+1);
	pushed_token = token_type;
}


/*
 * Get an entire line of input.  Handles logging, comments,
 * and EOF.
 */
void
get_inputline(line, nbytes)
	char	*line;
	int	nbytes;
{
	char	*p = line;
	int	c;

	/*
	 * Remove any leading white-space and comments
	 */
	do {
		while ((isspace(c = getchar())) && (c != '\n'))
			;
	} while (c == COMMENT_CHAR);
	/*
	 * Loop on each character until end of line
	 */
	while (c != '\n') {
		/*
		 * If we hit eof, get out.
		 */
		if (checkeof()) {
			fullabort();
		}
		/*
		 * Add the character to the buffer.
		 */
		if (nbytes > 1) {
			*p++ = (char)c;
			nbytes --;
		}
		/*
		 * Get the next character.
		 */
		c = getchar();
	}
	/*
	 * Null terminate the token.
	 */
	*p = 0;
	/*
	 * Indicate that we've emptied the pipe
	 */
	token_present = 0;
	/*
	 * If we're running out of a file, echo the line to
	 * the user, otherwise if we're logging, copy the
	 * input to the log file.
	 */
	if (option_f) {
		fmt_print("%s\n", line);
	} else if (log_file) {
		log_print("%s\n", line);
	}
}


/*
 * execute the shell escape command
 */
int
execute_shell(s)
char *s;
{
struct termio	termio;
struct termios	tty;
int		tty_flag, i, j;

	tty_flag = -1;

	if (*s == NULL)
		strcpy(s, getenv("SHELL"));
		/* save tty information */

	if (isatty(0)) {

		if (ioctl(0, TCGETS, &tty) == 0)
			tty_flag = 1;
		else {
			if (ioctl(0, TCGETA, &termio) == 0) {
				tty_flag = 0;
				tty.c_iflag = termio.c_iflag;
				tty.c_oflag = termio.c_oflag;
				tty.c_cflag = termio.c_cflag;
				tty.c_lflag = termio.c_lflag;
				for (i = 0; i < NCC; i++)
					tty.c_cc[i] = termio.c_cc[i];
			}
		}
	}

	(void) system(s);


	/* Restore tty information */

	if (isatty(0)) {

		if (tty_flag > 0)
			ioctl(0, TCSETSW, &tty);
		else if (tty_flag == 0) {
			termio.c_iflag = tty.c_iflag;
			termio.c_oflag = tty.c_oflag;
			termio.c_cflag = tty.c_cflag;
			termio.c_lflag = tty.c_lflag;
			for (j = 0; j < NCC; j++)
				termio.c_cc[j] = tty.c_cc[j];
			ioctl(0, TCSETAW, &termio);
		}

		if (isatty(1)) {
			fmt_print("\n[Hit Return to continue] \n");
			fflush(stdin);
			if (getchar() == EOF)
				fullabort();
		}
	}
	return (0);
}
