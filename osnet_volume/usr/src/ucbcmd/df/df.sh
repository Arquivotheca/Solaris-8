#!/usr/bin/sh
#
# ident	"@(#)df.sh	1.5	93/04/21 SMI"
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
#
# Replace /usr/ucb/df
#

ARG=-k
count=1
num=$#

if [ $# -lt 1 ]
then
	/usr/sbin/df $ARG
        exit $?
fi

while [ "$count" -le "$num" ]
do
	flag=$1
	case $flag in
	'-a')
		ARG="$ARG -a"
		;;
	'-t')
		ARG="$ARG -F"
		shift
		if [ "$1" = "4.2" ]
		then
			ARG="$ARG ufs"
		else
			ARG="$ARG $1"
		fi
		count=`expr $count + 1`
		;;
	'-i')
		ARG="$ARG -F ufs -o i"
		;;
	*)
		ARG="$ARG $flag"
                ;;
	esac
	if [ "$count" -lt "$num" ]
	then
		shift
	fi
	count=`expr $count + 1`
done
/usr/sbin/df $ARG
exit $?
