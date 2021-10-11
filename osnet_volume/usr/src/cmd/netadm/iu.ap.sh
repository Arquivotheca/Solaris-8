#!/sbin/sh
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)iu.ap.sh	1.27	99/04/29 SMI"	/* SVr4.0 1.3	*/

case "$MACH" in
  "u3b2" )
	echo "# /dev/console and /dev/contty autopush setup
#
# major	minor	lastminor	modules

    0	  -1	    0		ldterm
" >iu.ap
	;;
  "i386" )
	echo "# /dev/console and /dev/contty autopush setup
#
#       major minor   lastminor       modules

	wc	0	0	ldterm ttcompat
	asy	-1	0	ldterm ttcompat
	rts	-1	0	rts
	ipsecesp -1	0	ipsecesp
	ipsecah	-1	0	ipsecah
" > iu.ap
	;;
  "sparc" )
	echo "# /dev/console and /dev/contty autopush setup
#
#      major   minor lastminor	modules

	wc	0	0	ldterm ttcompat
	zs	0	63	ldterm ttcompat
	zs	131072	131135	ldterm ttcompat
	ptsl	0	47	ldterm ttcompat
	cvc	0	0	ldterm ttcompat
	mcpzsa	0	127	ldterm ttcompat
	mcpzsa	256	383	ldterm ttcompat
	stc	0	255	ldterm ttcompat
	se	0	255	ldterm ttcompat
	se	131072	131327	ldterm ttcompat
	se	16392	0	ldterm ttcompat
	su	0	1	ldterm ttcompat
	su	131072	131073	ldterm ttcompat
	rts	-1	0	rts
	ipsecesp -1	0	ipsecesp
	ipsecah	-1	0	ipsecah
" >iu.ap
	;;
  "ppc" )
	echo "# /dev/console and /dev/contty autopush setup
#
#       major minor   lastminor       modules

	wc	0	0	ldterm ttcompat
	asy	-1	0	ldterm ttcompat
	rts	-1	0	rts
	ipsecesp -1	0	ipsecesp
	ipsecah	-1	0	ipsecah
" > iu.ap
	;;
  * )
	echo "Unknown architecture."
	exit 1
	;;
esac
