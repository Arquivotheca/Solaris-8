#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)_sactab.sh	1.9	94/10/18 SMI"	/* SVr4.0 1.2	*/

case "$MACH" in
  "u3b2"|"sparc"|"i386"|"ppc" )
	echo "# VERSION=1

zsmon:ttymon::0:/usr/lib/saf/ttymon	# " >_sactab
	;;
  * )
	echo "Unknown architecture."
	exit 1
	;;
esac
