/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * tty_in.c -- tty input handling routines
 */

#ident "@(#)tty_in.c   1.20   98/05/14 SMI"

#include <ctype.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "debug.h"
#include "err.h"
#include "main.h"
#include "menu.h"
#include "tty.h"
#include "boot.h"

FILE *Script_file;	/* Ptr to script file */
int Screen_active;	/* Non-zero if screen active */
int Script_line = 1;	/* Input line counter */

static int if_sp = 0;		/* If/else nest counter */
static int exec_flag = 1;	/* Zero if we're in conditional */
static int (*multi_char)(int) = 0;
static char *line_too_long = "%s: Line too long, line %d";


static int
rtx(int tout, int *cp)
{
	/* DOS version of read with timeout */
	time_t t;
	int c;

	if (tout) {
		t = time(0);
		while (!_kbhit())
			if (time(0) == (t + 2))
				return (0);	/* timeout */
	}
	c = _getch();
	if (cp)
		*cp = c;
	return (1);
}

static int
doublechar(int c)
{
	/*
	 *  Double-characer function key generator:
	 *
	 *  This routine is used to generate two-byte keystrokes for function
	 *  keys and cursor movement keys.  The first byte returned is always
	 *  zero (the way a PC BIOS does it), followed by the scan code byte
	 *  given by "c".
	 */

	static char next_char;

	if (!multi_char) {
		/*
		 *  If this is our first time thru, save the scancode byte
		 *  in "next_char", set the multi_char callback pointer, and
		 *  deliver a null char.
		 */

		multi_char = doublechar;
		next_char = c;
		return (0);

	} else {
		/*
		 *  On the second time thru, clear the multi char callback
		 *  and deliver the scan code byte.
		 */

		multi_char = 0;
		return (next_char);
	}
}

/*ARGSUSED0*/
static int
keystroke(char **bpp, char *kp)
{
	/*
	 *  Keystroke playback routine:
	 *
	 *  This routine is the playback interpretation workhorse.  It is
	 *  called whenever the script file parser encounters a keystroke
	 *  command (e.g, a function key, arrow key, etc).  It generates
	 *  the one- or two-byte keystroke scancode associated with the
	 *  current command (as defined by "kp"), and returns the first byte
	 *  of this code.
	 */

	if (exec_flag) {
		/*
		 *  Don't do anything (other than advance the input pointer)
		 *  unless the "if" flag is set.
		 */

		switch (*kp) {

		case 'S': return (' ');
		case 'T': return ('\t');
		case 'D': return ('\016');
		case 'L': return ('\177');
		case 'R': return ('\014');
		case 'U': return ('\020');

		case 'B': return (doublechar(0x0F));
		case 'H': return (doublechar(0x47));

		case 'F': return (doublechar(0x3A + atoi(&kp[1])));
		case 'E': return ((kp[2] == 'T')? '\r': doublechar(0x4F));
		case 'P': return (doublechar((kp[4] == 'U')? 0x49: 0x51));
		}

		fatal("Invalid playback key %c", *kp);
	}

	return (-1);
}

static char quote_buf[MAXLINE], *qbp;
static int quote_switch;

static int
quoted_byte(int n)
{
	/*
	 *  Deliver next byte of a qouted string:
	 *
	 *  Called thru the "multi_char" callback pointer when the input
	 *  script contains a quoted string that needs to be passed to the
	 *  normal input routines.
	 */

	static int cnt;

	if (!multi_char) {
		/*
		 *  If this is our first time thru, save the input byte
		 *  count and set the callback pointer.
		 */

		cnt = n;
		multi_char = quoted_byte;
	}

	if (--cnt <= 0) multi_char = 0;
	return ((cnt >= 0) ? *qbp++ : -1);
}

static int
quoted_string(char **bpp, char *kp)
{
	/*
	 *  Read quoted string from script file:
	 *
	 *  This routine is called upon detecting a quoted string in a key-
	 *  word position in the script file.  Such strings are passed to
	 *  the standard input routines as-is (well, after doing the standard
	 *  escape processing).
	 */

	int j, n = 0;
	char *cp = *bpp;

	for (qbp = quote_buf; (*qbp = *cp++) != '"'; qbp++) {
		/*
		 *  Scan forward looking for the end of the quoted string,
		 *  copying each byte to the quote buffer as we go.
		 */

		switch (*qbp) {

		case 0:
		bogus:
			/*
			 *  This shouldn't happen;  Quoted strings must be
			 *  completely contained on a single line.
			 */

			fatal(line_too_long, Script, Script_line);
			/*NOTREACHED*/

		case '\\':
			/*
			 *  An escaped character.  Do the backslash thing!
			 */

			switch (*qbp = *cp++) {

			case 0: goto bogus;
			case 'a': *qbp = 007; break;
			case 'r': *qbp = '\r'; break;
			case 'n': *qbp = '\n'; break;
			case 'b': *qbp = '\b'; break;
			case 't': *qbp = '\t'; break;
			case 'f': *qbp = '\f'; break;

			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':

				for (cp--, j = *qbp = 0; j < 3; j++) {
					/*
					 *  Octal character escapes.  Up to
					 *  three digits allowed.
					 */

					*qbp = ((int)*qbp << 3) + (*cp++ - '0');
					if ((*cp < '0') || (*cp > '7')) break;
				}

				break;
			}
		}

		if (++n >= sizeof (quote_buf)) {
			/*
			 *  Make sure the string doesn't overflow our internal
			 *  buffer.  Blow up if it does.
			 */

			fatal("%s: String too long, line %d",
							    Script,
							    Script_line);
		}
	}

	*qbp = 0;
	*bpp = cp;
	qbp = quote_buf;
	return ((exec_flag && kp) ? quoted_byte(n) : -n);
}

/*ARGSUSED1*/
static int
comment(char **bpp, char *kp)
{
	/*
	 *  Script comment processor:
	 *
	 *  Advances the input pointer to the start of the next line.
	 */

	char *cp = *bpp;

	while (*cp != '\n') {
		/*
		 *  Find comment terminator, which must be in the input
		 *  buffer!
		 */

		if (!*cp++)
			fatal(line_too_long, Script, Script_line);
	}

	Script_line += 1;
	*bpp = cp + 1;
	return (-1);
}

static int
string_position(char **bpp, int defval)
{
	/*
	 *  Calculate screen position:
	 *
	 *  This routine is used to calculate row and column positions.  These
	 *  may be expressed as:
	 *
	 *	 <position> [ { + | - } <position> ... ]
	 *
	 *  where "<position>" may be a number or "*" to indicate the current
	 *  row or column (as given by "defval").  Returns the calculated pos-
	 *  ition and updates the buffer pointer at "bpp" to point beyond the
	 *  last character used.
	 */

	int n;
	long x;
	char *cp = *bpp;
	while (*cp && isspace(*cp)) cp++;

	if (isdigit(*cp)) {
		/*
		 *  If next character is a digit, use the standard library
		 *  routine to convert it to binary.
		 */

		x = strtol(cp, &cp, 10);

	} else if (*cp++ == '*') {
		/*
		 *  If next character is an asterisk, user wants the current
		 *  position as defined by "defval".
		 */

		x = defval;

	} else {
		/*
		 *  Anything else is an error ...
		 */

		fatal("%s: Syntax error, line %d", Script, Script_line);
	}

	while (*cp && isspace(*cp)) cp++;
	*bpp = cp;

	if (((n = (*cp == '+')) != 0) || (*cp == '-')) {
		/*
		 *  If caller wants to bias off of the position, recursively
		 *  evaluate that increment value and add it into what we've
		 *  already got.
		 */

		*bpp += 1;
		x += ((n ? 1 : -1) * string_position(bpp, defval));
	}

	return ((int)x);
}

static char *
string_value(char **bpp, int *lp)
{
	/*
	 *  Get relational operand:
	 *
	 *  Assertions and conditionals are built around string expressions
	 *  of the form "<string> X= <string>", where "<string>" may be a
	 *  quoted string or the row/column indication of text on the screen.
	 *  This routine returns a pointer to the next string value in the
	 *  input buffer at "bpp" (and stores the string length in "*lp").
	 */

	char *cp = *bpp;
	static char buf[sizeof (quote_buf)];

	/* Skip over any leading whitespace ... */
	while (*cp && isspace(*cp)) cp++;
	*bpp = cp;

	if (*cp == '"') {
		/*
		 * String is a quoted constant; use the "quoted_string"
		 * routine to isolate the text.
		 *
		 * NOTE: Passing a null keyword ptr tells "quoted_string"
		 * not to set up a multi-character callback pointer.
		 */

		*bpp = cp+1;
		cp = quote_buf;
		*lp = quoted_string(bpp, 0);

		if (quote_switch ^= 1) {
			/*
			 *  It's possible (tho stupid) to build expressions
			 *  of the form:  "string" X= "string".  We can't use
			 *  the "quote_buf" for both strings in this case, so
			 *  copy the first such string into our local buffer
			 *  instead.
			 */

			strcpy(buf, quote_buf);
			cp = buf;
		}

	} else if (*cp) {
		/*
		 *  String is given as a position on the screen.  These are
		 *  coded as "<row>,<col>" where "<row>" and "<col>" are the
		 *  simple position expresions recognized by the "string_pos-
		 *  ition" routine.
		 *
		 *  Screen strings are assumed to contain no spaces and length
		 *  is calculated accordingly.  Quoted string length is used
		 *  in comparisons (if there are any quoted strings in the
		 *  expression).
		 */

		int n, r = string_position(bpp, curli_tty());
		for (cp = *bpp; *cp && isspace(*cp); cp++);

		if (*cp++ != ',') {
			/*
			 *  There should be a comma here!  Generate syntax
			 *  error message if it's missing.
			 */

			fatal("%s: Syntax error, line %d", Script, Script_line);
		}

		*bpp = cp;
		n = string_position(bpp, curco_tty()+1);
		cp = (char *)&Cur_screen[(r * (maxco_tty()+1)) + n];

		if ((n = ((char *)Ecur_screen - cp)) <= 0) {
			/*
			 *  Caller has specified a position beyond the end
			 *  of the screen as we know it!
			 */

			fatal("%s: Invalid screen position, line %d",
							    Script,
							    Script_line);
		}

		for (r = 0; (r < n) && !isspace(cp[r]); r++);
		*lp = r;

	} else {
		/*
		 *  Fell off the end of the line before we got a complete
		 *  string spec.
		 */

		fatal(line_too_long, Script, Script_line);
	}

	return (cp);
}

static int
evaluate_condition(char **bpp)
{
	/*
	 *  Evaluate a string expression:
	 *
	 *  Evaluates the string expression at "bpp" and returns a non-zero
	 *  value if the expression is true.
	 *
	 *  NOTE: "ops" list must be searched right to left to get avoid
	 *	  ambiguity!
	 */

	static char *ops[] = { "=", "==", "!=", "<", "<=", ">", ">=" };
	int j, n = sizeof (ops) / sizeof (ops[0]);
	char *arg0, *arg1;
	char *cp = *bpp;
	int n0, n1;

	quote_switch = 0;
	arg0 = string_value(&cp, &n0);
	while (*cp && isspace(*cp)) cp++;
	while (n-- && strncmp(cp, ops[n], j = strlen(ops[n])));
	if (n < 0) fatal("%s: Invalid operator, line %d", Script, Script_line);

	*bpp = (cp + j);
	arg1 = string_value(bpp, &n1);
	j = strncmp(arg0, arg1, abs(__min(n0, n1)));

	switch (n) {
		/*
		 *  Return value is based on which comparison operator was
		 *  used ...
		 */

		case 0:
		case 1:	return (j == 0);
		case 2: return (j != 0);
		case 3: return (j <  0);
		case 4: return (j <= 0);
		case 5: return (j >  0);
		case 6: return (j >= 0);
	}
	/*NOTREACHED*/
}

/*ARGSUSED1*/
static int
assertion(char **bpp, char *kp)
{
	/*
	 *  Script assertion processor:
	 *
	 *  This routine processes ASSERTions encountered in the script file.
	 *  We evaluate the condition that follows the "ASSERT" keyword and
	 *  issue an error message if it's false.
	 */

	if (!evaluate_condition(bpp) && exec_flag) {
		/*
		 *  Assertion failed;  Print error message and blow up!
		 */

		fatal("%s: Assertion failure, line %d", Script, Script_line);
	}

	return (-1);
}

static int
cpx(char *p, char *q)
{
	/*
	 *  Compare string with known keyword:
	 *
	 *  This routine compares the text at "p" with the given keyword
	 *  value (at "q").  Comparison is insensitive to case, hence we
	 *  can't use a simple strcmp.  Return values are the same as strcmp,
	 *  however.
	 */

	while (*q) {
		/*
		 *  Keep going until we reach the end of the keyword name
		 *  or until we know the text doesn't match.
		 */

		int c = toupper(*p);
		p++;

		if (c != *q++) {
			/*
			 *  Text doesn't match.  Return +/- one to note that
			 *  it compares greater/less, respectively.
			 */

			return ((c < q[-1]) ? -1 : 1);
		}
	}

	return (0);
}

static int
conditional(char **bpp, char *kp)
{
	/*
	 *  IF/THEN/ELSE processor:
	 *
	 *  This routine handles IF/THEN/ELSE constructs read from the script
	 *  file.  Defining conditions are string relational operations as
	 *  used in "assertions" (see above).
	 *
	 *  The current state of an IF/THEN/ELSE is reflected in the global
	 *  "exec_flag".  If the low bit of this flag is zero, it means that
	 *  we shouldn't be excuting any commands read from the script file.
	 *  The flag gets reset when we fall into the complimentray arm of
	 *  the appropriate conditional statement.
	 */

	static char if_cond[16];
	int n, flag = 4;
	char *cp;

	switch (*kp) {

	case 'I':
		/*
		 *  The "IF" part of a conditional construct ...
		 */

		if (if_sp >= sizeof (if_cond)) {
			/*
			 *  We have a limited amount of space in which to
			 *  record the state of nested conditionals.
			 */

			fatal("%s: Conditionals nested too deeply, line %d",
							    Script,
							    Script_line);
		}

		if_cond[if_sp++] = !evaluate_condition(bpp) + (exec_flag << 1);
		for (cp = *bpp; *cp && isspace(*cp); cp++);
		flag = 0;

		if (cpx(cp, "THEN")) {
			/*
			 *  Can't find the "THEN" clause that's supposed to
			 *  follow the condition ...
			 */

			fatal("%s: Missing THEN, line %d", Script, Script_line);
		}

		*bpp = cp + sizeof ("THEN") - 1;
		/*FALLTHROUGH*/

	case 'E':
		/*
		 *  The "ELSE" part of a conditional ...
		 */

		if (((n = if_sp-1) < 0) || (if_cond[n] & 4)) {
			/*
			 *  ELSE is invalid without a preceding IF, and only
			 *  one ELSE is allowed per IF.
			 */

			fatal("%s: Improper ELSE, line %d", Script,
							    Script_line);
		}

		if_cond[n] ^= 1;
		if_cond[n] |= flag;
		exec_flag = ((if_cond[n] & 3) == 3);

		break;

	case 'F':
		/*
		 *  The "FI" part of a conditional.  Pop the previous state
		 *  from the "if_cond" stack.
		 */

		if (if_sp-- <= 0) {
			/*
			 *  Can't have a "FI" without an "IF!
			 */

			fatal("%s: FI with no IF, line %d", Script,
							    Script_line);
		}

		exec_flag = (!if_sp || (if_cond[if_sp] & 2));
		break;
	}

	return (-1);
}

static int
match(char *cp)
{
	/*
	 *  Pattern matcher for SELCT arguments:
	 *
	 *  I would have prefered to use ed-like regular expressions here,
	 *  but the "regex" routines are not part of the ANSI standard and
	 *  therefore unavailable under DOS.  So we provide shell-like
	 *  globbing instead.  Which is just as well, I suppose, given that
	 *  it takes less code to implement and users are more likely to
	 *  be familure with the syntax.
	 */

	int x;
	char *xp;

	switch (*qbp++) {

	case 0:
		/*
		 *  We've reached the end of the pattern string.  If we're
		 *  also at the end of the target string, we have a match!
		 */

		return (*cp == 0);

	case '?':
		/*
		 *  We'll take anything but a null ...
		 */

		return (*cp++ ? match(cp) : 0);

	case '*':
		/*
		 *  We'll take anything -- including nulls -- but we want
		 *  the minimum we can get away with.
		 */

		for (xp = qbp; *cp; qbp = xp) {
			/*
			 *  Peel away the remaining target text byte by byte
			 *  until we get a recursive match ...
			 */

			if (match(cp++)) {
				/*
				 *  That's it -- Bail out!
				 */

				return (1);
			}
		}

		return (*qbp == 0);

	case '[':
		/*
		 *  Must be (or must NOT be) one of the characters enclosed
		 *  in brackets ...
		 */

		qbp += (x = (*(xp = qbp) == '^'));
		qbp += (*qbp == ']'); /* [[ */

		while (*qbp != ']') {
			/*
			 *  What happened to the terminating right bracket?
			 */

			if (!*qbp++) fatal("%s: Missing ']', line %d",
							    Script,
							    Script_line);
		}

		while ((xp != qbp) && (*xp++ != *cp));
		return (((xp == qbp++) ^ x) ? 0 : match(cp+1));
	}

	/* Everything else has to match exactly ... */
	return ((*cp++ == qbp[-1]) ? match(cp) : 0);
}

static int
selectit(int n)
{
	/*
	 *  Multi-char callback for SELECT command:
	 *
	 *  This routine generates enough DOWN-ARROW keystrokes to position
	 *  the cursor at the "n"th item of the current "Select_list", then
	 *  returns a SPACE to select that item.  It is called repeatedly out
	 *  of "readc" until the selection is complete.
	 */

	static int homein, cnt;

	if (!multi_char) {
		/*
		 *  This is the first time we've been called for this list.
		 *  Set the callback pointer and save the item "cnt".
		 */

		cnt = n;
		multi_char = selectit;

		if ((homein = (Selection_list->cursor != 0)) != 0) {
			/*
			 *  Cursor isn't positioned at the first element, so
			 *  we'll have to generate a HOME keystroke.  This is
			 *  a two-byte sequence, the first of which is zero:
			 */

			return (0);
		}

	} else if (homein) {
		/*
		 *  Generate the second byte of the two-byte HOME sequence.
		 */

		homein = 0;
		return (0x47);
	}

	if (!cnt) multi_char = 0;
	return (cnt-- ? 'j' : ' ');
}

/*ARGSUSED1*/
static int
selection(char **bpp, char *kp)
{
	/*
	 *  SELECT command processor:
	 *
	 *  The SELECT command takes a single argument which is used to
	 *  specify which item in the current "select_menu" the user wishes
	 *  to select.  This argument can be an integer index (zero based)
	 *  or a quoted string containing a shell-like pattern that is
	 *  matched against the item labels.
	 */

	int n = -1;
	char *cp = *bpp;

	while (*cp && isspace(*cp)) cp++;
	if (!*cp)
		fatal(line_too_long, Script, Script_line);

	if (isdigit(*cp)) {
		/*
		 *  If selection argument is a number, n, caller wants the
		 *  n'th item in the list ...
		 */

		n = strtol(cp, &cp, 10);
		*bpp = cp;

	} else if (*cp == '"') {
		/*
		 *  If selection argument is a quoted string, the string is
		 *  a shell-like pattern.
		 */

		*bpp = ++cp;
		(void) quoted_string(bpp, 0);

	} else {
		/*
		 *  Everything else is bogus!
		 */

		fatal("%s: Syntax error, line %d", Script, Script_line);
	}

	if (exec_flag) {
		/*
		 *  We've parsed the arguments, now perform the actual
		 *  selection ...
		 */

		if (!Selection_list) {
			/*
			 *  We can't select anything if we're not processing
			 *  a selection menu!
			 */

			fatal("%s: No selection menu, line %d",
							    Script,
							    Script_line);
		}

		if (n < 0) {
			/*
			 *  If selection argument is a pattern, search the
			 *  list for the first item that matches said pattern.
			 */

			while ((++n < Selection_list->nitems) &&
			    ((Selection_list->list[n].flags & MF_UNSELABLE) ||
				    !match(Selection_list->list[n].string))) {
				/*
				 *  Must reset the pattern pointer after each
				 *  failed match!
				 */

				qbp = quote_buf;
			}
		}

		if ((n >= Selection_list->nitems) ||
		    (Selection_list->list[n].flags & MF_UNSELABLE)) {
			/*
			 *  Indicated item number must be within range and
			 *  must be selectable!
			 */

			fatal("%s: Invalid selection, line %d",
							    Script,
							    Script_line);
		}

		return (selectit(n));
	}

	return (-1);
}

/*ARGSUSED0*/
static int
print_screen(char **bpp, char *kp)
{
	/*
	 *  Print the screen contents:
	 *
	 *  This command may be used to dump the current screen contents to
	 *  the "debug.txt" file.
	 */

	if (exec_flag) {
		/*
		 *  Don't do anything unless the conditional "exec" flag
		 *  is set. Screens dumps are delimited by rows of asterisks
		 *  and the line number of the print request.
		 */

		int j;
		char *cp;
		int col = (maxco_tty() + 1);

		if (!Debug_file) init_debug();
		j = fprintf(Debug_file, "\n*** Print screen request: %s, "
					    "line %d ", Script, Script_line)-1;

		while (j++ < col) fputc('*', Debug_file);

		for (cp = (char *)Cur_screen; cp < (char *)Ecur_screen; cp++) {
			/*
			 *  Print each byte of the screen ...
			 */

			if (col++ >= maxco_tty()) {
				/*
				 *  The screen buffer doesn't include newline
				 *  characters, so we have to insert one at
				 *  the end of each row.
				 */

				fputc('\n', Debug_file);
				col = 0;
			}

			fputc(*cp & 0x7F, Debug_file);
		}

		j = fprintf(Debug_file, "\n*** Cursor at %d,%d ",
							    curli_tty(),
							    curco_tty()+1);

		while (j++ < maxco_tty()) fputc('*', Debug_file);
		fputc('\n', Debug_file);
	}

	return (-1);
}

/*ARGSUSED0*/
static int
waitforit(char **bpp, char *kp)
{
	/*
	 *  "Wait" command interpreter:
	 *
	 *  Holds the current screen until user presses ENTER.  Only works
	 *  when the "-P" flag is used.
	 */

	while (exec_flag && Screen_active) {
		/*
		 *  We only do this if the screen is active and we're not
		 *  stepping thru an inactive arm of a conditional.
		 */

		int c;
		(void) rtx(0, &c);
		if ((c == '\n') || (c == '\r')) break;
	}

	return (-1);
}

/*ARGSUSED0*/
static int
quit(char **bpp, char *kp)
{
	/*
	 *  Interpret the DONE command:
	 *
	 *  Just like control-C!
	 */

	if (exec_flag) done(0);
	return (-1);
}

static struct keyword { char nam[12]; int (*rtn)(char **, char *); } cmds[] = {
	/*
	 *  Table of keywords and keyword interpreter routines for use in
	 *  playback mode.  These must be in lexigraphical order to allow
	 *  for binary sarch of the table.
	 */

	{ "\"",		quoted_string },
	{ "#",		comment },
	{ "ASSERT",	assertion },
	{ "BACKTAB",	keystroke },
	{ "DONE",	quit },
	{ "DOWN",	keystroke },
	{ "ELSE",	conditional },
	{ "END",	keystroke },
	{ "ENTER",	keystroke },
	{ "F1",		keystroke },
	{ "F10",	keystroke },
	{ "F11",	keystroke },
	{ "F12",	keystroke },
	{ "F2",		keystroke },
	{ "F3",		keystroke },
	{ "F4",		keystroke },
	{ "F5",		keystroke },
	{ "F6",		keystroke },
	{ "F7",		keystroke },
	{ "F8",		keystroke },
	{ "F9",		keystroke },
	{ "FI",		conditional },
	{ "HOME",	keystroke },
	{ "IF",		conditional },
	{ "LEFT",	keystroke },
	{ "PAGEDOWN",	keystroke },
	{ "PAGEUP",	keystroke },
	{ "PRINT",	print_screen },
	{ "RIGHT",	keystroke },
	{ "SELECT",	selection },
	{ "SPACE",	keystroke },
	{ "TAB",	keystroke },
	{ "UP",		keystroke },
	{ "WAIT",	waitforit }
};

static int
readc(int tout, int *cp)
{
	/*
	 *  Read with timeout:
	 *
	 *  Stores next input character at *cp, or returns 0 if we time out
	 *  before the character appears.  If we're in playback mode, the
	 *  timeout value is ignored (i.e, the next input character is always
	 *  ready).
	 */

	int j, k;
	static char buf[MAXLINE];
	static char *bp = buf;

	if (!Script_file) {
		/*
		 *  Normal input mode:  Use the OS-specific read with timeout
		 *  routine.
		 */

		return (rtx(tout, cp));
	}

	if (multi_char && ((j = multi_char(0)) >= 0)) {
		/*
		 *  We're not done processing the previous playback command.
		 *  Return the next character from that sequence.
		 */

		*cp = j;
		return (1);
	}

	for (;;) {
		/*
		 *  Read the playback script, returning keystrokes as indi-
		 *  cated therein.
		 */

		struct keyword *ep, *lo = cmds;
		struct keyword *hi = &cmds[sizeof (cmds) / sizeof (cmds[0])];

		if (!*bp && !fgets(bp = buf, sizeof (buf), Script_file)) {
			/*
			 *  EOF on the script file.  What we do next depends
			 *  on how the file was specified on the command line.
			 *  If "-P" was used, return to manual input mode;
			 *  If "-p" was used, exit.
			 */

			if (if_sp != 0) {
				/*
				 *  All if statements should be complete by
				 *  now.  If not, we've got an error.
				 */

				fatal("%s: IF with no FI, line %d",
							    Script,
							    Script_line);
			}

			fclose(Script_file); Script_file = 0;
			if (!Screen_active) done(0);
			beep_tty();

			return (readc(tout, cp));
		}

		while (*bp && isspace(*bp)) {
			/*
			 *  Skip over any leading whitespace, counting new-
			 *  lines as we go.
			 */

			Script_line += (*bp++ == '\n');
		}

		if (*bp != 0) {
			/*
			 *  We've found what should be a keyword.  Search
			 *  the keyword table to make sure it's valid.
			 */

			while ((j = (hi - lo)) > 0) {
				/*
				 *  A textbook binary search ...
				 */

				ep = lo + (j >> 1);

				if (!(k = cpx(bp, ep->nam))) break;
				else if (k < 0) hi = ep;
				else lo = ep+1;
			}

			if (j <= 0) {
				/*
				 *  Syntax error in the script file.  Print
				 *  the file name and line number in the
				 *  error message.
				 */

				fatal("%s: Invalid keyword, line %d",
							    Script,
							    Script_line);
			}

			bp += strlen(ep->nam);
			j = ep->rtn(&bp, ep->nam);

			if (j >= 0) {
				/*
				 *  The keyword interpreter has a produced a
				 *  character for us to return to our caller!
				 */

				*cp = j;
				return (1);
			}
		}
	}
}

#ifdef DEBUG
int
enter_debug()
{
	extern	void refresh_tty(int);
#ifndef __lint
	unsigned short dcall = 0xfe00;
	unsigned short dsub = 0xff;
	_asm {
		/*
		 * Use DOS "debug" call
		 */

		push  bx
		mov   bx, dsub
		mov   ax, dcall
		int   21h
		pop   bx
	}
#endif
	refresh_tty(1);
	return (0);
}
#endif

int
getc_tty(void)
{
	static int pushedc = -1;
	static int lastc = -1;
	int c, x;

	/* checked for character saved from last time */
	if (pushedc != -1) {
		c = pushedc;
		pushedc = -1;
		return (lastc = c);
	}

	for (;;) {

		/* read the first character with no timeout */
		(void) readc(0, &c);

		/* check for read error */
		if (c == EOF)
			fatal("getc_tty: EOF on stdin");

		/* check for control-C */
		if (c == '\003')
			fatal("interrupted by ^C");

#ifdef DEBUG
		/* check for 2 control-Z */
		if (c == '\032' && lastc == '\032') {
			lastc = '\0';
			enter_debug();
			return ('\0');
		}
#endif
		lastc = c;

		/* do the "standard" translations and check for escapes */
		switch (c) {
		case '\r':
			return ('\n');
		case '\016':
			return (TTYCHAR_DOWN);
		case '\b':
		case '\177':
			return (TTYCHAR_LEFT);
		case '\013':
		case '\020':
			return (TTYCHAR_UP);
		case '\014':
			return (TTYCHAR_RIGHT);
		case '\026':
			return (TTYCHAR_PGDOWN);
		case '\0':
		case 0xe0:
			/* DOS-type escape sequence, check for more chars */
			if (readc(1, &c) == 0)
				return ('\0');		/* timed out */

			/* interpret the escape sequence */
			switch (c) {
			case 0x47:
				return (TTYCHAR_HOME);
			case 0x48:
				return (TTYCHAR_UP);
			case 0x49:
				return (TTYCHAR_PGUP);
			case 0x4f:
				return (TTYCHAR_END);
			case 0x50:
				return (TTYCHAR_DOWN);
			case 0x51:
				return (TTYCHAR_PGDOWN);
			case 0x4b:
			case 0x53:
				return (TTYCHAR_LEFT);
			case 0x4d:
				return (TTYCHAR_RIGHT);
			case 0xf:
				return (TTYCHAR_BKTAB);
			default:
				/* check for "F" keys */
				if ((c >= 0x3b) && (c <= 0x43))
					return (FKEY(c - 0x3b + 1));
				else {
					/* unexpected sequence */
					beep_tty();
					continue;
				}
			}
		case '\033':
			/* ansi-type escape code */
			if (readc(1, &c) == 0)
				return ('\033');	/* timed out */

			/*
			 * if not an escape sequence,
			 * return each character individually
			 */
			if ((c != '[') && (c != 'O')) {
				pushedc = c;
				return ('\033');
			}

			/* get the next character in the sequence */
			if (readc(1, &c) == 0) {
				/* unexpected partial sequence */
				beep_tty();
				continue;
			}

			/* interpret the escape sequence */
			if ((c >= 'P') && (c <= 'X'))
				return (FKEY(c - 'P' + 1));
			else if (((x = c) == '1') || (x == '2')) {
				/* openwin F-key?  look for rest of sequence */
				if (readc(1, &c) == 0) {
					/* unexpected partial sequence */
					beep_tty();
					continue;
				}
				if (x == '2') c += 9;
				else if (c >= '7') c--;
				if ((c >= '1') && (c <= '9')) {
					/* suck up the '~' */
					(void) readc(1, NULL);
					return (FKEY(c - '1' + 1));
				}
			}

			switch (c) {
			case '\0':
				return (TTYCHAR_END);
			case 'A':
				return (TTYCHAR_UP);
			case 'B':
				return (TTYCHAR_DOWN);
			case 'C':
				return (TTYCHAR_RIGHT);
			case 'D':
				return (TTYCHAR_LEFT);
			case '6':
				(void) readc(1, NULL);	/* suck up the '~' */
				return (TTYCHAR_PGDOWN);
			case '3':
				(void) readc(1, NULL);	/* suck up the '~' */
				return (TTYCHAR_PGUP);
			case '2':
				(void) readc(1, NULL);	/* suck up the '~' */
				return (TTYCHAR_HOME);
			case '5':
				(void) readc(1, NULL);	/* suck up the '~' */
				return (TTYCHAR_END);
			default:
				beep_tty();	/* unexpected sequence */
				continue;
			}
		default:
			return (c);
		}
	}

	/*NOTREACHED*/
}

/*
 * keyname_tty -- return the name of an F-key (or the enter key)
 */

char *
keyname_tty(int c, int escmode)
{
	static char fkey[] = "F0";
	static char ekey[] = "Esc-0";
	static char okey[] = "0";

	if (c == '\n')
		return ("Enter");
	else if (c & TTYCHAR_FKEY) {
		if (escmode) {
			ekey[4] = '0' + (c & TTYCHAR_FKEY_MSK);
			return (ekey);
		} else {
			fkey[1] = '0' + (c & TTYCHAR_FKEY_MSK);
			return (fkey);
		}
	} else {
		/* "other" key, just make a string out of it */
		okey[0] = c;
		return (okey);
	}
}
