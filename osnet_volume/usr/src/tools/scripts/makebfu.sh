#!/bin/ksh
#
# Copyright (c) 1993-1998 by Sun Microsystems, Inc.
# All rights reserved.
# 
#ident	"@(#)makebfu.sh	1.3	99/12/06 SMI"
#
# Builds bfu archives. If no arguments, uses the environment variables
# already set (by bldenv). One argument specifies an environment file
# like nightly or bldenv uses.

USAGE='Usage: $0 [ <env_file> ]'

if [ $# -gt 1 ]; then
	echo $USAGE
	exit 1
fi

if [ $# -eq 1 ]; then
	if [ -z "$OPTHOME" ]; then
		OPTHOME=/opt
		export OPTHOME
	fi
	#
	#       Setup environmental variables
	#
	if [ -f $1 ]; then
		. $1
	else
		if [ -f $OPTHOME/onbld/env/$1 ]; then
			. $OPTHOME/onbld/env/$1
		else
			echo "Cannot find env file as either $1 \c"
			echo "or $OPTHOME/onbld/env/$1"
			exit 1
		fi
	fi
fi

if [ -z "$ROOT" -o ! -d "$ROOT" ]; then
	echo '$ROOT must be set to a valid proto directory.'
	exit 1
fi

if [ -z "$CPIODIR" ]; then
	# $CPIODIR may not exist though, so no test for it
	echo '$CPIODIR must be set to a valid proto directory.'
	exit 1
fi

echo "Making archives from $ROOT in $CPIODIR."
mkbfu $ROOT $CPIODIR

if [ ! -z "${NIGHTLY_OPTIONS}" ]; then
	zflag=`echo ${NIGHTLY_OPTIONS} | grep z`
	if [ ! -z "$zflag" ]; then
		echo
		echo "Compressing archives in $CPIODIR."
		cd ${CPIODIR}
		gzip -v *
	fi
fi
