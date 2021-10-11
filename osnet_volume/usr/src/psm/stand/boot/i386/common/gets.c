/*
 * Copyright (c) 1992-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)gets.c	1.4	96/03/23 SMI"

#include <sys/types.h>
#include <sys/salib.h>

extern int getchar();
extern void putchar();

/*
 * bgets():	Sort of like gets(), but not quite.
 *		Read from the console (using getchar) into string str,
 *		until a carriage return or until n-1 characters are read.
 *		Null terminate the string, and return.
 *		This all is made complicated by the fact that we must
 *		do our own echoing during input.
 *		N.B.: Returns the *number of characters in str*.
 */

int
bgets(str, n)
char	*str;
int	n;
{
	int 	c;
	int	t;
	char	*p;

	p = str;
	c = 0;

	while ((t = getchar()) != '\r') {
		putchar(t);
		if (t == '\b') {
			if (c) {
				printf(" \b");
				c--; p--;
			} else
				putchar(' ');
			continue;
		}
		if (c < n - 1) {
			*p++ = t;
			c++;
		}
	}
	*p = '\0';

	return (c);
}

/*
 * Version of bgets for bsh - boot shell command interpreter
 */

int
cgets(str, n)
char	*str;
int	n;
{
	int 	c;
	int	t;
	char	*p;

	p = str;
	c = 0;

	while ((t = getchar()) != '\r') {
		if ((t == '\4') && (c == 0))	/* check for CTL-D */
			return (-1);
		putchar(t);
		if (t == '\b') {
			if (c) {
				printf(" \b");
				c--; p--;
			} else
				putchar(' ');
			continue;
		}
		if (c < n - 2) {
			*p++ = t;
			c++;
		}
	}
	putchar('\r');
	putchar('\n');
	*p++ = '\n';
	*p = '\0';

	return (c);
}

#ifdef NOTYET
/*
 * bfgets():	Sort of like fgets(), but not quite.
 *		Read data from the open inode starting at offset,
 *		stopping when a	newline is encounted, or n-1 characters
 *		have been read, or EOF is reached. The string is then
 *		null terminated.
 *		N.B.: Returns the *number of characters in str*.
 */
int
bfgets(str, n, offset)
char	*str;
int	n;
int	offset;
{
	register int 	count, i;

	if ((count =
	    breadi((long)offset, physaddr(str), (long)(n-1), FALSE)) == 0)
		return (0);

	for (i = 0; i < count; i++)
		if (str[i] == '\n') {
			str[i] = '\0';
			break;
		}

	if (i >= count)
		str[count] = '\0';

	return (i);
}
#endif
