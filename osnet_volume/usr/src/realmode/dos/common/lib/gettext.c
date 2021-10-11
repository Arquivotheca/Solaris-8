/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * gettext.c -- routines to perform string translations
 */
#ident "@(#)gettext.c	1.8     95/05/16 SMI\n"

/*
 * This module implements the function
 *
 *	char *gettext(char *string)
 *
 * as a simple mechanism for runtime localization of program output
 * A single variables must be defined in the main module:
 *
 *	char	*myname;	/ * full pathname of main program * /
 *
 * The myname pointer is used to access the text replacement file that
 * has the filename extension .txt.  For example, if myname points to
 * the string "C:\SOLARIS\TESTPROG.COM", then "C:\SOLARIS\TESTPROG.TXT"
 * is expected to exist as the companion text file to the program file.
 * Typically, the assignment
 *
 *	myname = argv[0];
 *
 * should work.
 *
 * The text file should contain base strings in square brackets and
 * the replacement string follows in double quote marks:
 *
 *	[This is base string #%d\n]
 *	"This is the replacement string #%d\n"
 *
 * The string in square brackets must exactly match the character string in
 * quote marks in the C program.  For the above example, the replacemnt string
 * will be returned and output in the following printf:
 *
 *	printf(getttext("This is base string #%d\n"), number);
 *
 * Double_quotes within the replacement string should be avoided.
 * If double_quotes (or square brackets) are used, they must be escaped.
 * The rest of the text file is free form.
 *
 * The standard escape sequences are recognized and converted to single bytes,
 * includeing \### for octal escape sequences and \x## for hex escapes.  For
 * For newlines in the replacement string, use either \n, or use embedded CR 
 * and LF characters:
 * 
 *      [This is base string #%d\n]
 *      "This is the replacement string #%d,
 *       that spans a second line \07 that rings a bell,
 *       and a third line\n"
 * 
 * Suggestion:
 *   Always compose the gettext call with the character string on the same
 *   line.  Then the base strings can be extracted using "grep gettext xxx.c".
 */

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define	MAXPATH	256
extern char *myname;
static int esc = 0;

static int
scanc(FILE *fp)
{	/*
	 *  Fetch next character from translation file.  Performs escape
	 *  conversions that might be indicated.  Returns EOF upon reaching
	 *  end-of-file.
	 *
	 *  The global "esc" flag is set as a side effect.  Non-zero value
	 *  mean the character returned was escaped.
	 */

	char *xp;
	int x, xc;
	int radix = 8;
	static char hextab[] = "0123456789ABCDEF";


	if (((xc = getc(fp)) != EOF) && (esc = (xc == '\\'))) {
		/*
		 *  Next char is a backslash, perform escape processing.
		 */

		switch (xc = getc(fp)) {

			// Standard ANSI C escape sequences ...

			case 'a': xc = '\a'; break;
			case 'b': xc = '\b'; break;
			case 'f': xc = '\f'; break;
			case 'n': xc = '\n'; break;
			case 'r': xc = '\r'; break;
			case 't': xc = '\t'; break;
			case 'v': xc = '\v'; break;

			// Up to 2 hex digits
			case 'x': case 'X':
			{
				radix = 16;
				xc = '0';

				/*FALLTHROUGH*/
			}

			// Up to 3 octal digits
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
			{	
				int d, j = 3;
				x = xc; xc = 0;

				while (j-- && (xp = strchr(hextab, toupper(x)))
					   && ((d = (xp - hextab)) < radix)) {

					xc = (xc * radix) + d;
					x = getc(fp);
				}

				if (x != EOF) ungetc(x, fp);
				break;
			}
		}
	}

	return(xc);
}

static int
cmp (const void *p, const void *q)
{	// Qsort comparison for sorting string index
	return(strcmp(*(char **)p, *(char **)q));
}

char *
gettext(char *string)
{	/*
	 *  Translate an output "string".  If the source string appears in the
	 *  translate table, return its translation.  Otherwise, return the
	 *  string itself.
	 */

	int j;
	char **hi, **lo;
	static int first = 1;
	static int trcnt = 0;
	static char **trtab = 0;

	if (first && (myname != NULL)) {
		/*
		 *  First time thru; Process the *.txt file, convert it to
		 *  the translation table format, and build an index.
		 */

		FILE *fp;
		char path[MAXPATH];
		char *dot;

		first = 0;
		strcpy(path, myname);
		if ((dot = strrchr(path, '.')) == NULL)
			strcat(path, ".txt");
		else
			strcpy(strrchr(path, '.')+1, "txt");

		if (fp = fopen(path, "r")) {
			/*
			 *  File opened successfuly, now process it.  We make
			 *  to passes over the input; one to determine the size
			 *  of the translation table, and one to actually build
			 *  the table.
			 */

			int c = 0;
			int n = 0;

#			define	until(x) while (((c = scanc(fp)) != EOF)    \
						      && ((c != x) || esc))
			while (c != EOF) {
				/*
				 *  Count the total number of string trans-
				 *  lations (to "trcnt") and the total length	
				 *  of all strings (to "n" register).
				 */

				j = 0;
				until('['); until(']') j++;
				until('"'); until('"') j++;

				if (c != EOF) {
					/*
					 *  We got to the end of another trans-
					 *  lation;  Bump up the coutnters.
					 */

					n += (j+2);
					trcnt++;
				}
			}

			if ((j = trcnt) > 0) {
				/*
				 *  We found at least one translation in the
				 *  input file.  Build the translation table
				 *  and it's index.
				 */

				char *cp;

				if ((cp = malloc(n))
				&& (trtab = malloc(trcnt * sizeof(char **)))) {
					/*
					 *  All buffers successfully allocated,
					 *  rewind the input file and read 
					 *  thru it a second time.
					 */

					c = 0;
					rewind(fp);

					while (j-- > 0) {
						/*
						 *  Find next translation and
						 *  copy it into the buffer
						 *  we allocated.
						 */

						trtab[j] = cp;
						until('['); 
						until(']') *cp++ = c; 
						*cp++ = 0;
						until('"'); 
						until('"') *cp++ = c; 
						*cp++ = 0;
					}

					qsort(trtab, trcnt, 
					    sizeof(char **), cmp);

				} else {

					printf("no memory for xlate file\n");
					trcnt = 0;
				}
			}

			fclose(fp);

		} else {
			/*
			 *  Can't open the translation file.  Print a message
			 *  if we're debugging, otherwise assume that the user
			 *  understands American English (or West-coast techno-
			 *  jargon, as the case may be).
			 */

#			if (defined(DEBUG))
			{
				printf("Unable to open xlate file, \"%s\"\n", 
				    path);
			}
#									endif
		}
	}

	/* a null string is pretty easy to translate! */
	if (string == NULL)
		return(NULL);

	lo = trtab;
	hi = lo + trcnt;

	while ((j = (hi - lo)) > 0) {
		/*
		 *  Binary search thru the translate table, comparing base
		 *  strings to the target "string".  If we get a match, re-
		 *  turn the corresponding translation string.
		 */

		char **next = lo + (j >> 1);

		if (!(j = strcmp(string, *next))) return(strchr(*next, 0)+1);
		if (j > 0) lo = next+1;
		else hi = next;
	}

	return(string);
}
