#!/bin/ksh
#
#ident	"@(#)symbindrep.ksh	1.3	97/11/23 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#


usage() {
	echo "usage: symbindrep -[$optlet] utility"
	echo "	-f <bindfromlist>"
	echo "		A colon sperated list of libraries that will have"
	echo "		symbol references tracked.  Only symbol references"
	echo "		originating from these libraries will be tracked."
	echo "		The default is to track symbol references from"
	echo "		all libraries"
	echo "	-t <bindtolist>"
	echo "		A colon seperated list of libraries to track"
	echo "		symbol bindings.  Only bindings to objects in"
	echo "		these objects will be tracked.  The default is to"
	echo "		track bindings to all objects."
	echo "	-l <bindreplib>"
	echo "		specify an alternate symbindrep.so to use."
}


bindto=""
bindfrom=""
symbindreplib="/opt/SUNWonld/lib/symbindrep.so.1"
symbindreplib64="/opt/SUNWonld/lib/sparcv9/symbindrep.so.1"

optlet="f:t:l:"

while getopts $optlet c
do

	case $c in
	f)
		bindfrom="$OPTARG"
		;;
	t)
		bindto="$OPTARG"
		;;
	l)
		symbindreplib="$OPTARG"
		symbindreplib64="$OPTARG"
		;;
	\?)
		usage
		exit 1
		;;
	esac
done
shift `expr $OPTIND - 1`


#
# Build environment variables
#

SYMBINDREP_BINDTO="$bindto" \
SYMBINDREP_BINDFROM="$bindfrom" \
LD_BIND_NOW=1 \
LD_AUDIT="$symbindreplib" \
LD_AUDIT_64="$symbindreplib64" \
$*

exit 0
