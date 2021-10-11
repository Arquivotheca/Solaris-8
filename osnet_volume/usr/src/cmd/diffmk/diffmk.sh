#!/usr/bin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#! /usr/bin/sh

#     Portions Copyright(c) 1988, Sun Microsystems, Inc.
#     All Rights Reserved

#     Makefile for echo

#ident	"@(#)diffmk.sh	1.5	93/01/11 SMI"        /* SVr4.0 1.2  */

PATH=/usr/bin
if test -z "$3" -o "$3" = "$1" -o "$3" = "$2"; then
	echo `gettext TEXT_DOMAIN "usage: diffmk name1 name2 name3 -- name3 must be different"`
	exit 1
fi
diff -e $1 $2 | (sed -n -e '
/[ac]$/{
	p
	a\
.mc |
: loop
	n
	/^\.$/b done1
	p
	b loop
: done1
	a\
.mc\
.
	b
}

/d$/{
	s/d/c/p
	a\
.mc *\
.mc\
.
	b
}'; echo '1,$p') | ed - $1| sed -e '
/^\.TS/,/.*\. *$/b pos
/^\.T&/,/.*\. *$/b pos
p
d
:pos
/^\.mc/d
' > $3
