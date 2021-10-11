/*
 * Copyright (c) 1985-1994 by Sun Microsystems, Inc.
 */

#ident	"@(#)get.c	1.3	96/03/11 SMI"

extern int bgets(char *, int);

int
gets(char *buf)
{
	/* call PC routine in stand/boot/i86pc/gets.c */
	return (bgets(buf, 80));
}
