#!/bin/sh

#
#	Copyright (c) 1991, Sun Microsystems Inc.
#

#ident	"@(#)getmajor.sh	1.2	92/07/14 SMI"

if [ $# -ne 1 ]
then
	echo "Usage: `basename $0` modname" >&2
	exit 2
fi

exec awk -e "BEGIN	{found = 0}
/^$1[ 	]/	{print \$2; found = 1; exit 0}
END	{if (found == 0) exit 1}" </etc/name_to_major
