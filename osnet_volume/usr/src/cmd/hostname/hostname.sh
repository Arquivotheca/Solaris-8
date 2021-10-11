#!/usr/bin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

# Copyright (c) 1988, Sun Microsystems, Inc.
# All Rights Reserved.

#ident	"@(#)hostname.sh	1.4	96/08/27 SMI"	/* SVr4.0 1.2	*/


if [ $# -eq 0 ]; then
	/bin/uname -n
elif [ $# -eq 1 ]; then
	/bin/uname -S $1
     else
	echo `/bin/gettext 'Usage: hostname [name]'`
	exit 1
fi
