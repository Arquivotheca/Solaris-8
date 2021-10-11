#! /usr/bin/sh

#ident	"@(#)file.sh	1.1	91/05/04 SMI"

#
# Copyright (c) 1991 by Sun Microsystems, Inc.
#

# This script invokes /usr/bin/file with -h option by default and
# turns off -h when passed with -L option.

CFLAG=
FFLAG=
MFLAG=
HFLAG=-h
USAGE="usage: file [-cL] [-f ffile] [-m mfile] file..."

while getopts cLf:m: opt
do
	case $opt in
	c)	CFLAG=-$opt;;
	L)	HFLAG= ;;
	m)	MFLAG=-$opt; MARG=$OPTARG;;
	f)	FFLAG=-$opt; FARG=$OPTARG;;
	\?)	echo $USAGE;
		exit 1;;
	esac
done
shift `expr $OPTIND - 1`
/usr/bin/file $HFLAG $CFLAG $MFLAG $MARG $FFLAG $FARG $*
