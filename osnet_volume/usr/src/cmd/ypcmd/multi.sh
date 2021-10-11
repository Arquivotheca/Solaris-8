#!/bin/sh
#
#ident	"@(#)multi.sh	1.5	99/03/21 SMI"
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# Script to examine hosts file and make "magic" entries for
# those hosts that have multiple IP addresses.
#
#

MAKEDBM=/usr/sbin/makedbm
STDHOSTS=/usr/lib/netsvc/yp/stdhosts
MULTIAWK=/usr/lib/netsvc/yp/multi.awk
MAP="hosts.byname"

USAGE="Usage: multi [-b] [-l] [-s] [-n] [hosts file]
Where:
	-b	Add YP_INTERDOMAIN flag to hosts map
	-l	Convert keys to lower case before creating map
	-s	Add YP_SECURE flag to hosts map
	-n	Add IPv6 and IPv4 host addresses to ipnodes map

	hosts file defaults to /etc/hosts"

while getopts blsn c
do
    case $c in
	b)	BFLAG=-b;;
	l)	LFLAG=-l;;
	s)	SFLAG=-s;;
	n)	NFLAG=-n;;
	\?)	echo "$USAGE"
		exit 2;;
    esac
done

if [ "$NFLAG" = "-n" ]
then
	MAP="ipnodes.byname"
fi

shift `expr $OPTIND - 1`

if [ "$1" ]
then
    HOSTS=$1
elif [ "$NFLAG" = "-n" ]
then
    HOSTS=/etc/inet/ipnodes
else
    HOSTS=/etc/hosts
fi

if [ "$HOSTS" = "-" ]
then
    unset HOSTS
fi

cd /var/yp/`domainname` && \
    sed -e '/^[ 	]*$/d' -e '/^#/d' -e 's/#.*$//' $HOSTS | \
    $STDHOSTS $NFLAG | \
    $MULTIAWK - | \
    $MAKEDBM $BFLAG $LFLAG $SFLAG - $MAP
