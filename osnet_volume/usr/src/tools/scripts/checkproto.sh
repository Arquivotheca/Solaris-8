#!/bin/ksh -e
#
# Copyright (c) 1993-1998 by Sun Microsystems, Inc.
# All rights reserved.
# 
#ident	"@(#)checkproto.sh	1.2	99/12/06 SMI"
#
MACH=`uname -p`
PLIST=/tmp/protolist.$$

if [ $# = 0 -a "${CODEMGR_WS}" != "" ]; then
	WS=${CODEMGR_WS}
elif [ $# -ne 1 ]; then
	echo "usage: $0 <workspace>"
	exit 1
else
	WS=$1
fi

if [ ! -d ${WS} ]; then
	echo "${WS} is not a workspace"
	exit 1
fi

if [ -z "${SRC}" ]; then
	SRC=${WS}/usr/src
fi
PROTO=${WS}/proto/root_${MACH}
PKGDEFS=${SRC}/pkgdefs
EXCEPTION=${PKGDEFS}/etc/exception_list_${MACH}

rm -f $PLIST
protolist ${PROTO} > $PLIST
protocmp -e ${EXCEPTION} ${PLIST} ${PKGDEFS}

rm -f $PLIST
