#!/bin/ksh
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
# 
#ident	"@(#)elfcmp.sh	1.3	99/11/01 SMI"
#
# elfcmp - compare significant sections in two ELF files
#
# usage: elfcmp [-v] [-s "section [section ...]" <f1> <f2>
#
# Author:  Dan.Mick@west.sun.com
#

VERBOSE=0
SECTIONLIST=""

usage() {
	echo 'Usage: elfcmp [-v] [-s "section [section ...]" <f1> <f2>' 1>&2
	exit 1
}

while [[ $# > 0 ]]
do
	case "$1" in
	-v)
		VERBOSE=1
		;;
	-s)
		SECTIONLIST="$2"
		shift
		;;
	-*)
		usage
		;;
	*)
		break
		;;
	esac
	shift
done

if [[ $# != 2 ]]
then
	usage
fi

TMP1=/tmp/elfcmp.1.$$
TMP2=/tmp/elfcmp.2.$$
trap "rm -f $TMP1 $TMP2" EXIT HUP INT QUIT PIPE TERM

# get section lists for both files into temp files

if [[ "$SECTIONLIST" = "" ]]
then
	dump -h "$1" | grep '\[[0-9]' | awk '{print $7}' | sort >$TMP1
	dump -h "$2" | grep '\[[0-9]' | awk '{print $7}' | sort >$TMP2
else
	echo "$SECTIONLIST" >$TMP1
	echo "$SECTIONLIST" >$TMP2
fi

# determine and print which ones aren't in both of the input files

NOT_IN_1=$(comm -23 $TMP1 $TMP2)
if [[ ! -z "$NOT_IN_1" ]]
then
	echo "Section(s) $NOT_IN_1 not in $1"
fi
NOT_IN_2=$(comm -13 $TMP1 $TMP2)
if [[ ! -z "$NOT_IN_2" ]]
then
	echo "Section(s) $NOT_IN_2 not in $1"
fi

# for all the sections which *are* common, do the following

for s in $(comm -12 $TMP1 $TMP2)
do
	dump -s -n $s "$1" | sed '/:/d' >$TMP1
	dump -s -n $s "$2" | sed '/:/d' >$TMP2
	if cmp -s $TMP1 $TMP2
	then
		echo "Section $s is the same"
	else
		echo "Section $s differs"
		if [[ $VERBOSE = 1 ]]
		then
			dump -sv -n $s "$1" | sed '/:/d' >$TMP1
			dump -sv -n $s "$2" | sed '/:/d' >$TMP2
			diff -c $TMP1 $TMP2
		fi
	fi
done
