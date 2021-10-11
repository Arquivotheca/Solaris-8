#!/usr/bin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)ld.sh	1.4	94/10/24 SMI"	/* SVr4.0 1.7	*/

#	Copyright (c) 1984 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#	Portions Copyright(c) 1988, Sun Microsystems, Inc.
#	All Rights Reserved

# ld command for BSD compatibility package:
#
#       BSD compatibility package libraries (/usr/ucblib) are
#       searched next to third to last.  SVr4 default libraries 
#       (/usr/ccs/lib and /usr/lib) are searched next to last and
#	last respectively.
#
#       Because the BSD compatibility package C library does not 
#       contain all the C library routines of /usr/ccs/lib/libc.a, 
#       the BSD package C library is named /usr/ucblib/libucb.a
#       and is passed explicitly to ld.  This ensures that libucb.a 
#       will be searched first for routines and that 
#       /usr/ccs/lib/libc.a will be searched afterwards for routines 
#       not found in /usr/ucblib/libucb.a.  Also because sockets is    
#       provided in libc under BSD, /usr/lib/libsocket and /usr/lib/nsl
#       are also included as default libraries.
#
#       NOTE: the -Y L, and -Y U, options of ld are not valid 

opts=
LIBS="-lucb -lresolv -lsocket -lnsl -lelf"

if [ $# -eq 0 ]
then
	exit 1
elif [ $# -gt 0 ]
then
	for i in $*
	do
		case $i in
			-r)
				LIBS=""
				opts="$opts $i"
				shift;;
			*)
				opts="$opts $i"
				shift;;
		esac
	done
fi

LD_RUN_PATH=/usr/ucblib /usr/ccs/bin/ld -YP,:/usr/ucblib:/usr/ccs/lib:/usr/lib $opts $LIBS
