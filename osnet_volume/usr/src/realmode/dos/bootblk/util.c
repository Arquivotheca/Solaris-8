/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)util.c	1.12	99/01/31 SMI\n"

#include "disk.h"
#include "chario.h"
#include "varargs.h"

/*
 *[]------------------------------------------------------------[]
 * | Place globals here						|
 *[]------------------------------------------------------------[]
 */
int		Charsout;
u_short		Malloc_start;
_malloc_p	mhead;

/*
 *[]------------------------------------------------------------[]
 * | collapse_util -- collapse malloc pointers that are next	|
 * | to each other.						|
 *[]------------------------------------------------------------[]
 */
static void
collapse_util(_malloc_p m)
{
	if (((u_short)m + m->size) == (u_short)m->next) {
		m->size += m->next->size;
		/* ---- REM: m->next->next could be zero ---- */
		m->next = m->next->next;
	}
}

/*
 *[]------------------------------------------------------------[]
 * | mallocinit_util -- Normally initMalloc will be called with |
 * | a size of 0. mallocinit_util() will then use all the 	|
 * | memory from addr to the end of the current segment. During |
 * | the test phase I'll use the current malloc stuff and give 	|
 * | that block to this malloc so that I can play around and 	|
 * | see if it works as expected.				|
 *[]------------------------------------------------------------[]
 */
static void
mallocinit_util(void)
{
	extern char _end;

	Malloc_start = (((u_short)&_end + 3) & ~3);
	mhead = (_malloc_p)Malloc_start;
	mhead->next = 0;
	mhead->size = 0 - Malloc_start - 4;
}

/*
 *[]------------------------------------------------------------[]
 * | malloc_util -- malloc space from the general pool.		|
 *[]------------------------------------------------------------[]
 */
u_short
malloc_util(u_short size)
{
	_malloc_p s;
	_malloc_p p;		/* ---- previous node needed for linking */
	_malloc_p n;		/* ---- node to make */

	if (!mhead) mallocinit_util();

	s = mhead;

	/*
	 * make sure there's room for the header when this node is freed
	 * back onto the list
	 */
	size = ((size + 3) & ~3) + sizeof(_malloc_t);

	p = s;
	while (s && (s->size < size)) {
		p = s;
		s = p->next;
	}

	/* ---- found node with space ---- */
	if (s) {
		n = (_malloc_p)((u_short)s + size);
		n->next = s->next;
		n->size = s->size - size;

		/* ---- This is the first node, adjust mhead ---- */
		if (s == p)
		  mhead = n;
		else
		  p->next = n;
		bzero_util((char *)s, size);
		return (u_short)s;
	}
	else {
		DPrint(DBG_MALLOC,
			("Malloc: Failed to alloc %d bytes\n", size));
		return 0;
	}
}

/*
 *[]------------------------------------------------------------[]
 * | free_util -- return space to pool.				|
 *[]------------------------------------------------------------[]
 */
void
free_util(u_short p, u_short size)
{
	_malloc_p m = (_malloc_p)p;
	_malloc_p s, n;

	/*
	 * If this was a general interface I'd make the next test a panic
	 * condition. Because it's in the strap code I might find a specific
	 * use for freeing up other memory locations within straps 64k.
	 * For now just print a warning.
	 * Rick McNeal -- 12-Jun-1995
	 */
	if (p < Malloc_start)
		DPrint(DBG_MALLOC,
			("Free: addr 0x%x before start 0x%x\n",
			p, Malloc_start));

	size = ((size + 3) & ~3) + sizeof(_malloc_t);

	/* .... should I have a magic cookie in _malloc_t? .... */

	/* ---- if this node is the first chunk update mhead ---- */
	if ((m < mhead) || (mhead == 0)) {
		m->next = mhead;
		m->size = size;
		mhead = m;
		collapse_util(mhead);
	}
	else {
		/* ---- find the two nodes that m fits between ---- */
		s = mhead;
		n = s->next;
		while (n && (m > n)) {
			s = n;
			n = s->next;
		}

		/* ---- link m into the chain, n could be zero ---- */
		m->next = n;
		m->size = size;
		collapse_util(m);

		s->next = m;
		collapse_util(s);
	}
}

/*
 *[]------------------------------------------------------------[]
 * | clear_util -- clear all screens				|
 *[]------------------------------------------------------------[]
 */
void
clear_util(void)
{
	extern _char_io_t console;
	_char_io_p p;

	for (p = &console; p; p = p->next)
		(*p->clear)(p);
}

/*
 *[]------------------------------------------------------------[]
 * | setcursor_util -- position cursor to request row & col	|
 *[]------------------------------------------------------------[]
 */
void
setcursor_util(int row, int col)
{
	extern _char_io_t console;
	_char_io_p p;

	for (p = &console; p; p = p->next)
		(*p->set)(p, row, col);
}

/*
 *[]------------------------------------------------------------[]
 * | putc_util -- put character out to all available screens	|
 *[]------------------------------------------------------------[]
 */
void
putc_util(char c)
{
	extern _char_io_t console;
	_char_io_p p;
	int s;

        if (c == '\t') {
                for (s = 8 - Charsout % 8; s > 0; s--)
			putc_util(' ');
		return;
	}
	else if (c == '\n') {
		Charsout = 0;
		putc_util('\r');
	} else
		Charsout++;
	for (p = &console; p; p = p->next)
		if (!(p->flags & CHARIO_IGNORE))
			(*p->putc)(p, c);
}

/*
 *[]------------------------------------------------------------[]
 * | getc_util -- get a character from the first available 	|
 * | input device.						|
 *[]------------------------------------------------------------[]
 */
char
getc_util(void)
{
	extern _char_io_t console;
	_char_io_p p;
	char c;

	do {
		for (p = &console; p; p = p->next)
			if ((*p->avail)(p)) {
				c =  (*p->getc)(p);
				if (c == 0)
					continue;
				if (c == '\r')
					putc_util('\n');
				else
					putc_util(c);
				return c;
			}
	} while (1);
}

/*
 *[]------------------------------------------------------------[]
 * | nowaitc_util -- see if a character is available for reading|
 * | from any of the input devices attached.			|
 *[]------------------------------------------------------------[]
 */
int
nowaitc_util(void)
{
	extern _char_io_t console;
	_char_io_p p;

	for (p = &console; p; p = p->next)
		if ((*p->avail)(p))
			return 1;
	return 0;
}

/*
 *[]------------------------------------------------------------[]
 * | gets_util -- place a string of characters into buffer	|
 *[]------------------------------------------------------------[]
 */
void
gets_util(char *s, int cc)
{
	int orig_cc = cc;
	int c;

	while (1) {
		c = getc_util();
		if (c == '\r' || c == '\n') {
			*s = '\0';
			break;
		}
		else if (c == '\b') {
			putc_util(' ');
			/* ---- don't add characters at the front ---- */
			if (cc == orig_cc)
				continue;
			*s-- = '\0';
			putc_util('\b');
			cc++;
		}
		else if (cc > 1) {
			*s++ = c;
			cc--;
		}
		else {
			/* ---- erase the overflow character ---- */
			putc_util('\b');
			putc_util(' ');
			putc_util('\b');
		}
	}
	return;
}

/*
 *[]------------------------------------------------------------[]
 * | setprint_util -- move cursor and call printf		|
 *[]------------------------------------------------------------[]
 */
void
setprint_util(row, col, format, va_alist)
int row, col;
char *format;
va_dcl
{
	va_list ap;

	va_start(ap);
	setcursor_util(row, col);
	doprint_util(format, ap);
}

/*
 *[]------------------------------------------------------------[]
 * | printf_util -- general printf routine			|
 *[]------------------------------------------------------------[]
 */
void
printf_util(format, va_alist)
char *format;
va_dcl
{
	va_list ap;

	va_start(ap);
	doprint_util(format, ap);
}

/*
 *[]------------------------------------------------------------[]
 * | HandleSeq_util -- home cooked routine to allow cursor	|
 * | control to be embedded into the printf format.		|
 * | Use ANSI sequences.					|
 *[]------------------------------------------------------------[]
 */
static char *
HandleSeq_util(char *f)
{
	int r, c;

	/*
	 * 'f' is currently pointing at a string containing the following
	 * pattern.
	 *	\x...
	 * The x is the function which we'll be switching on.
	 */
	c = *(f + 1);
	f += 2;

	switch(c) {
	      case '\\':	/* ---- escape the back slash ---- */
		putc_util('\\');
		break;

	      case 'b':		/* ---- back space sequence ---- */
		putc_util('\b');
		break;

	      case 't':		/* ---- tab character ---- */
		putc_util('\t');
		break;

	      case 'S':		/* ---- screen function ---- */
		if (*f == 'c') {
			clear_util();
			f++;
		}
		else if (*f == 'p') {
			r = strtol_util(++f, &f, 10);
			c = strtol_util(++f, &f, 10);
			setcursor_util(r, c);
		}
		break;

	      default:		/* ---- unknown seq., print as is ---- */
		putc_util('\\');
		putc_util((char)c);
		break;
	}
	return f;
}

/*
 *[]------------------------------------------------------------[]
 * | doprint_util -- work horse of the printf routine		|
 *[]------------------------------------------------------------[]
 */
static void
doprint_util(format, ap)
char *format;
va_list ap;
{
        int dolong, fillzero, fillsize, doprec, precsize, ac;
        char c, *str;
        long val;

        while (*format) {
                while (*format && *format != '%') {
			if (*format == '\\')
			  format = HandleSeq_util(format);
			else
			  putc_util(*format++);
		}
                if (*format == '\0')
                        break;
                dolong = 0;
                fillzero = 0;
                fillsize = 0;
                doprec = 0;
                precsize = 0;
nexttoken:
                format++;
                switch(*format) {
                        case '0':
                                if (fillsize == 0) {
                                        fillzero = 1;
                                        goto nexttoken;
                                }
                                /* else drop through */
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9':
                                if (doprec)
                                        precsize = precsize * 10 + *format - '0';
                                else
                                        fillsize = fillsize * 10 + *format - '0';
                                goto nexttoken;
                        case 'l':
                                dolong = 1;
                                goto nexttoken;
                        case '.':
                                doprec = 1;
                                goto nexttoken;
                        case 'd':
                        case 'u':
                                if (dolong) {
                                        val = va_arg(ap, u_long);
                                        printnum_util(val, 10,
                                        	fillzero, fillsize);
                                }
                                else {
                                        val = va_arg(ap, u_short);
                                        printnum_util(val, 10,
                                        	fillzero, fillsize);
                                }
                                break;
			case 'X':
				val = va_arg(ap, u_short);
				printnum_util(va_arg(ap, u_short), 16, 1, 4);
				putc_util('.');
				printnum_util(val, 16, 1, 4);
				break;
                        case 'x':
                                if (dolong) {
                                        val = va_arg(ap, u_long);
                                        printnum_util(val, 16,
                                        	fillzero, fillsize);
                                }
                                else {
                                        val = va_arg(ap, u_short);
                                        printnum_util(val, 16,
                                        	fillzero, fillsize);
                                }
                                break;
                        case 'o':
                                if (dolong) {
                                        val = va_arg(ap, u_long);
                                        printnum_util(val, 8,
                                        	fillzero, fillsize);
                                }
                                else {
                                        val = va_arg(ap, u_short);
                                        printnum_util(val, 8,
                                        	fillzero, fillsize);
                                }
                                break;
                        case 'b':
                                if (dolong) {
                                        val = va_arg(ap, u_long);
                                        printnum_util(val, 2,
                                        	fillzero, fillsize);
                                }
                                else {
                                        val = va_arg(ap, u_short);
                                        printnum_util(val, 2,
                                        	fillzero, fillsize);
                                }
                                break;
                        case 's':
                                str = va_arg(ap, char *);
                                if (precsize) {
                                        for (ac = 0; ac < precsize; ac++)
                                                if (str[ac] == '\0')
                                                        break;
                                        if (ac < fillsize) {
                                                precsize -= fillsize - ac;
                                                for (; ac < fillsize; ac++)
                                                        putc_util(' ');
                                        }
                                        for(; precsize > 0; precsize--)
                                                putc_util(*str++);
                                }
                                else {
                                        for (ac = 0; str[ac] != '\0'; ac++);
                                        if (ac < fillsize)
                                                for (; ac < fillsize; ac++)
                                                        putc_util(' ');
                                        printf_util(str);
                                }
                                break;
                        case 'c':
                                c = va_arg(ap, int);
                                putc_util(c);
                                break;
                        default:
                                printf_util("Unknown print op %c\n", *format);
                                break;
                }
                format++;
        }
}

#define MAXSTR 32
/*
 *[]------------------------------------------------------------[]
 * | printnum_util -- convert v into an ascii string of base.	|
 *[]------------------------------------------------------------[]
 */
static void
printnum_util(u_long v, int base, int z, int s)
{
        int c;
        char maxstr[MAXSTR], *p;

        /* ---- Special case for value being equal to 0 ---- */
        if (v == 0) {
                for (s--; s > 0; s--)
                        if (z)
                                putc_util('0');
                        else
                                putc_util(' ');
                putc_util('0');
                return;
        }

        p = &maxstr[MAXSTR - 1];
        *p-- = '\0';
        while (v) {
                c = v % base;
                *p-- = c >= 10 ? c - 10 + 'A' : c + '0';
                v /= base;
                if (s > 0)
                        s--;
        }
        for (; s > 0; s--)
                if (z)
                        putc_util('0');
                else
                        putc_util(' ');
        while (*++p)
                putc_util(*p);
}

/*
 *[]------------------------------------------------------------[]
 * | toupper_util -- if needed convert the char to upper case	|
 *[]------------------------------------------------------------[]
 */
char
toupper_util(char c)
{
        if (c >= 'a' && c <= 'z')
                c = c - 'a' + 'A';
        return c;
}

/*
 *[]------------------------------------------------------------[]
 * | validc_util() -- returns non zero value if given blk	|
 * | number is valid. If blk is CLUSTER_AVAIL (which is 0) it's	|
 * | not a valid cluster. rd is true if the cluster is part of	|
 * | the root directory.					|
 *[]------------------------------------------------------------[]
 */
int
validc_util(short blk, int rd)
{
	if (rd && (blk == 0))
	  return 1;
	if (blk >= CLUSTER_EOF_16_0 && blk <= CLUSTER_EOF_16_8)
	  return 0;
	if (blk == CLUSTER_BAD_16)
	  return 0;
	if (blk >= CLUSTER_RES_16_0 && blk <= CLUSTER_RES_16_6)
	  return 0;
	return blk;
}

/*
 *[]------------------------------------------------------------[]
 * | strlen_util -- return the length of s			|
 *[]------------------------------------------------------------[]
 */
int
strlen_util(char *s)
{
        int v = 0;

        while (*s++)
                v++;
        return v;
}

/*
 *[]------------------------------------------------------------[]
 * | strncmp_util -- compare n characters of strings s and t	|
 *[]------------------------------------------------------------[]
 */
int
strncmp_util(char *s, char *t, int n)
{
        for (; n > 0; n--)
                if (*s++ != *t++)
                        return 1;
        return 0;
}

/*
 *[]------------------------------------------------------------[]
 * | strcmp_util -- compare strings				|
 *[]------------------------------------------------------------[]
 */
int
strcmp_util(char *s, char *t)
{
        while (*s && *t) {
                if (*s++ != *t++)
                        return 1;
        }
        if (*s != *t)
                return 1;
        else
                return 0;
}

/*
 *[]------------------------------------------------------------[]
 * | strtol_util -- convert string into binary number		|
 *[]------------------------------------------------------------[]
 */
u_long
strtol_util(char *s, char **sp, int base)
{
        u_long val, v;

        val = 0;
	while (*s == ' ')
		s++;
        if (base == 0) {
                if (*s == '0') {
                        if (*++s == 'x') {
                                s++;
                                base = 16;
                        }
                        else
                                base = 8;
                }
                else
                        base = 10;
        }
        while (*s) {
                if ((*s >= '0') && (*s <= '9'))
                        v = *s - '0';
                else if ((base > 10) &&
                	(toupper_util(*s) >= 'A') && (toupper_util(*s) <= 'F'))
                        v = toupper_util(*s) - 'A' + 10;
                else
			break;

                val = val * base + v;
                s++;
        }

	if (sp != (char **)0) {
		*sp = s;
	}
        return val;
}

/*
 *[]------------------------------------------------------------[]
 * | strchr_util -- find the first occurance of c in s		|
 *[]------------------------------------------------------------[]
 */
char *
strchr_util(char *s, char c)
{
        while (*s) {
                if (*s == c)
                        return s;
                s++;
        }
        return (char *)0;
}

/*
 *[]------------------------------------------------------------[]
 * | bcopy_util -- copy c chars from s to d			|
 *[]------------------------------------------------------------[]
 */
void
bcopy_util(char far *s, char far *d, int c)
{
        while (c--)
                *d++ = *s++;
}

/*
 *[]------------------------------------------------------------[]
 * | bzero_util -- zero string 					|
 *[]------------------------------------------------------------[]
 */
void
bzero_util(char *s, int c)
{
	memset_util(s, '\0', c);
}

/*
 *[]------------------------------------------------------------[]
 * | memset_util -- set string s to character c using count cc	|
 *[]------------------------------------------------------------[]
 */
void
memset_util(char *s, int c, int cc)
{
	while (cc--)
	  *s++ = c;
}



