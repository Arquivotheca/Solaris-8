#!/usr/bin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#       Copyright(c) 1988, Sun Microsystems, Inc.
#       All Rights Reserved

#ident	"@(#)arch.sh	1.7	95/12/14 SMI"	/* SVr4.0 1.2	*/

# On sparc systems, arch returns sun4 (historical artifact)
# while arch -k returns `uname -m`. On all other systems,
# arch == arch -k == uname -m.

USAGE="Usage: $0 [ -k | archname ]"
UNAME=/usr/bin/uname
ECHO=/usr/bin/echo

case $# in
0)	OP=major;;
1)	case $1 in
	-k)		OP=minor;;
	-?)		$ECHO $USAGE;
			exit 1;;
	*)		OP=compat;;
	esac;;
*)	$ECHO $USAGE;
	exit 1;;
esac

MINOR=`$UNAME -m`

case `$UNAME -p` in
sparc)  MAJOR=sun4;;
*)	MAJOR=$MINOR;;
esac

case $OP in
major)	$ECHO $MAJOR;;
minor)	$ECHO $MINOR;;
compat) [ $1 = $MAJOR ] ; exit ;;
esac

exit 0
