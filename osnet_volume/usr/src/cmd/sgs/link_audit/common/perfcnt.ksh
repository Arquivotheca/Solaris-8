#!/bin/ksh
#
#ident	"@(#)perfcnt.ksh	1.5	97/11/23 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#


usage() {
	echo "usage: perfcnt -[$optlet] utility [utility arguements]"
	echo "	-f <bindfromlist>"
	echo "		A colon seperated list of libraries that are to be"
	echo "		traced.  Only calls from these libraries will be"
	echo "		traced.  The default is to trace all calls"
	echo "	-t <bindtolist>"
	echo "		A colon seperated list of libraries that are to be"
	echo "		traced.  Only calls to these libraries will be"
	echo "		traced.  The default is to trace all calls"
	echo "	-l <perfcntlib>"
	echo "		specify an alternate perfcnt.so to use."
}


bindto=""
bindfrom=""
perfcntlib="/opt/SUNWonld/lib/perfcnt.so.1"
perfcntlib64="/opt/SUNWonld/lib/sparcv9/perfcnt.so.1"

optlet="f:t:l:"

if [[ $# < 1 ]]; then
	usage
	exit 1
fi

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
		perfcntlib="$OPTARG"
		perfcntlib64="$OPTARG"
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

PERFCNT_BINDTO="$bindto" \
PERFCNT_BINDFROM="$bindfrom" \
LD_AUDIT="$perfcntlib" \
LD_AUDIT_64="$perfcntlib64" \
$*

exit 0
