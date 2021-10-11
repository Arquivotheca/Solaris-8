#!/bin/ksh
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)whocalls.ksh	1.7	98/02/04 SMI"
#


usage() {
	echo "usage: whocalls [$optlet] <funcname> <utility> [utility arguements]"
	echo ""
	echo "	whocalls will audit all function bindings between"
	echo "	<utility> and any of its libraries.  Each time <funcname>"
	echo "	is called a stack-trace will be displayed"
	echo ""
	echo "	-l <wholib>"
	echo "		specify an alternate who.so to use."
	echo ""
	echo "	-s	when available also examine the .symtab for"
	echo "		local symbols when displaying a stack trace"
	echo "		(more expensive)"
	echo ""
}

optlet="sl:"

if [[ $# < 2 ]]; then
	usage
	exit 1
fi

wholib="/usr/lib/link_audit/who.so.1"
wholib64="/usr/lib/link_audit/sparcv9/who.so.1"
detail=""

while getopts $optlet c
do
	case $c in
	l)
		wholib="$OPTARG"
		wholib64="$OPTARG"
		;;
	s)
		detail="1"
		;;
	\?)
		usage
		exit 1
		;;
	esac
done

shift `expr $OPTIND - 1`
func=$1
shift 1


LD_AUDIT="$wholib" \
LD_AUDIT_64="$wholib64" \
WHO_DETAIL="$detail" \
WHOCALLS="$func" \
"$@"
exit 0
