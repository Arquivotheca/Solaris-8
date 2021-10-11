/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strprint.c	1.1	99/05/14 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <locale.h>
#include <unistd.h>
#include <widec.h>
#include <wctype.h>
#include "stabspf_impl.h"

#define	TXTSZ 16
#define	NUMSZ 36

/*
 * Use uppercase hex for addresses and offsets like truss and the ptools.
 * However, print data in lowerecase to provide a contrast.
 */
static char const *digits = "0123456789abcdef";

/*
 * print_hexadump() - prints out a classic dex dump of memory at the alligned
 *	address of addr uses max as the advised number of bytes to show.
 *
 * It may print more bytes than requested since it sill complete a line
 * of output that has 16 bytes.
 */
int
print_hexadump(FILE *s, size_t max, void const *addr)
{
	uint8_t *mem;
	int c;
	int lines;
	int line;
	char byte;
	char txt[TXTSZ + 1];
	char *t;
	char nums[NUMSZ + 1];
	char *n;
	int i;
	int retval = 0;

	/* Divide max by 16. */
	lines = (max >> 4);

	if ((max & 0xf) != 0) {
		++lines;
	}


	/* start dump at a 16-byte alligned address */
	byte = (uintptr_t)addr & 0xfUL;
	mem = (uint8_t *)((uintptr_t)addr & ~(0xfUL));

#ifdef _LP64
	retval +=  fprintf(s, " = 0x%.16lx + XXXX\n", (uintptr_t)mem);
#else  /* _ILP32 */
	retval += fprintf(s, " = 0x%.8x + XXXX\n", (uintptr_t)mem);
#endif /* _LP64 */

	/* Build the index line indicating the actual start of data. */
	retval +=  fputs("\tXXXX: ", s);
	for (i = 0; i < 16; i++) {
		if (i == byte) {
			retval +=  fputs("\\/", s);
		} else {
			retval += fprintf(s, "%2X", i);
		}
		if ((i & 3) == 3) {
			/* put an extra space every fourth byte */
			(void) putc(' ', s);
			++retval;
		}
	}
	retval += fprintf(s, " %*c\n", byte + 1, 'v');
	for (line = 0; line < lines; line++) {
		t = txt;
		n = nums;
		for (i = 0; i < TXTSZ; i++) {
			c = *mem++;
			/* build ascii field */
			if (c != '\0' && isascii(c) &&
			    isprint(c) && !iscntrl(c)) {
				*t++ = c;
			} else {
				*t++ = '.';
			}

			/* convert byte to hex */
			*(n + 1) = digits[c & 0xf];
			c >>= 4;
			*n = digits[c & 0xf];
			n += 2;

			if ((i & 3) == 3) {
				*n++ = ' ';
			}
		}
		*t = '\0';
		*n = '\0';
		retval += fprintf(s, "\t%.4X: %s %s\n",
		    (line << 4), nums, txt);
	}

	return (retval);
}

/*
 * print_strdump() - prints the string at addr to stream s surrounded
 *	by quotes, will print out multibyte characters according to locale.
 *	Non-printable characters are printed visibly either as
 *	control characters (ie. ^@ for \0) or in octal.
 *
 *	A trailing "..." is displayed if max is exceeded before end of string.
 *
 * Returns the number of characters written, if wchars are used then it will
 *	include the number of Process Code characters transformed and written.
 *
 * NOTE: max is advisory and may be exceeded if non-printable characters
 *	are displayed.
 */
int
print_strdump(FILE *s, size_t max, void const *addr)
{
	char *mem = (char *)addr;
	int c;
	wchar_t	wc;
	int len;
	int retval = 0;

	(void) putc('\"', s);
	++retval;
	while (max > 0) {
		/*
		 * Prevent sign extension in case c is printed
		 * in octal, this avoids masking it below.
		 */
		c = *(uchar_t *)mem;
#ifndef ABI_DEBUG
		if (c == '\0') {
			/* end of string */
			break;
		}
#endif	/* ABI_DEBUG */
		if (isascii(c)) {
			if (isprint(c)) { /* can print it */
				(void) putc(c, s);
				++retval;
				--max;
			} else if (iscntrl(c)) {
				/* control characters */
				if (max >= 2) {
					/* Fill in the escape sequence. */
					char escseq[3] = "\\_";

					/*
					 * Try to emit the character in the
					 * the same way it probably appeared
					 * in the code.
					 */
					switch (c) {
					case '\a':	/* Bell or alert. */
						escseq[1] = 'a';
						break;
					case '\b':	/* Backspace. */
						escseq[1] = 'b';
						break;
					case '\f':	/* Form feed. */
						escseq[1] = 'f';
						break;
					case '\n':	/* Newline. */
						escseq[1] = 'n';
						break;
					case '\r':	/* Carriage return. */
						escseq[1] = 'r';
						break;
					case '\t':	/* Horizontal tab. */
						escseq[1] = 't';
						break;
					case '\v':	/* Vertical tab. */
						escseq[1] = 'v';
						break;
					default:
						escseq[0] = '^';
						escseq[1] = c ^ 0100;
						break;
					}
					retval += fputs(escseq, s);
					max -= 2;
				} else {
					max = 0;
				}
			}
			++mem;
		} else if ((len = mbtowc(&wc, mem, MB_CUR_MAX)) > 0 &&
		    iswprint(wc)) {
			/* Multibyte time */
			(void) putwc(wc, s);
			mem += len;
			/* Only count this as a single character for output. */
			--max;
			++retval;
		} else {
			/* otherwise print it in octal */
			if (max >= 4) {
				(void) fprintf(s, "\\%03o", c);
				++mem;
				max -= 4;
				retval += 4;
			} else {
				max = 0;
			}
		}
	}

	(void) putc('\"', s);
	++retval;
	if (c != '\0') { /* we could have gone further */
		retval += fputs("...", s);
	}

	return (retval);
}

int
print_smartdump(FILE *s, size_t max, void const *addr)
{
	char *mem = (char *)addr;
	int c = *(uchar_t *)mem;
	wchar_t	wc;
	int retval;
	int srret;

	if ((srret = check_addr(addr, max)) == 0)
		return (0);

	if (isascii(c) ||
	    (mbtowc(&wc, mem, MB_CUR_MAX) > 0 &&
	    iswprint(wc))) {
		retval = print_strdump(s, srret, addr);
	} else {
		retval = print_hexadump(s, srret, addr);
	}

	return (retval);
}

#ifdef MAIN
main()
{
	char test[256];
	void *addr;
	int i;

	for (i = 0; i < 255; i++)
		test[i] = i+1;
	test[i] = '\0';

	addr = (void *)(test);

	print_strdump(stdout, 1024, addr);
	return (0);
}
#endif /* MAIN */
