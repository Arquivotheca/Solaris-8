#!/bin/ksh
#
#ident	"@(#)sotruss.ksh	1.9	98/02/04 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#


usage() {
	echo "usage: sotruss [-F:-T:-o:-f] utility [utility arguements]"
	echo "	-F <bindfromlist>"
	echo "		A colon seperated list of libraries that are to be"
	echo "		traced.  Only calls from these libraries will be"
	echo "		traced.  The default is to trace calls from the"
	echo "		main executable"
	echo "	-T <bindtolist>"
	echo "		A colon seperated list of libraries that are to be"
	echo "		traced.  Only calls to these libraries will be"
	echo "		traced.  The default is to trace all calls"
	echo "	-o <outputfile>"
	echo "		sotruss output will be directed to 'outputfile'."
	echo "		by default it is placed on stdout"
	echo "	-f"
	echo "		Follow all children created by fork() and also"
	echo "		print truss output for the children.  This also"
	echo "		causes a 'pid' to be added to each truss output line"
}


bindto=""
bindfrom=""
outfile=""
noindentopt=""
trusslib="/usr/lib/link_audit/truss.so.1"
trusslib64="/usr/lib/link_audit/sparcv9/truss.so.1"
pidopt=""
noexitopt="1"

optlet="eF:T:o:fl:i"

if [[ $# < 1 ]]; then
	usage
	exit 1
fi

while getopts $optlet c
do

	case $c in
	F)
		bindfrom="$OPTARG"
		;;
	T)
		bindto="$OPTARG"
		;;
	o)
		outfile="$OPTARG"
		;;
	l)
		trusslib="$OPTARG"
		trusslib64="$OPTARG"
		;;
	f)
		pidopt="1"
		;;
	i)
		noindentopt="1"
		;;
	e)
		noexitopt=""
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

TRUSS_BINDTO="$bindto" \
TRUSS_BINDFROM="$bindfrom" \
TRUSS_OUTPUT="$outfile" \
TRUSS_PID="$pidopt" \
TRUSS_NOINDENT="$noindentopt" \
TRUSS_NOEXIT="$noexitopt" \
LD_AUDIT="$trusslib" \
LD_AUDIT_64="$trusslib64" \
"$@"

exit 0
