#!/usr/bin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)dirname.sh	1.7	94/09/08 SMI"	/* SVr4.0 1.6	*/

if [ $# -gt 1 ]
then
	if [ "$1" = "--" -a $# -le 2 ]
	then 
		shift
	else
		echo >&2 `gettext TEXT_DOMAIN "usage: dirname [ path ]"`
		exit 1
	fi
fi

#	First check for pathnames of form //*non-slash*/* in which case the 
#	dirname is /.
#	Otherwise, remove the last component in the pathname and slashes 
#	that come before it.
#	If nothing is left, dirname is "."
exec /usr/bin/expr \
	"${1:-.}/" : '\(/\)/*[^/]*//*$'  \| \
	"${1:-.}/" : '\(.*[^/]\)//*[^/][^/]*//*$' \| \
	.
