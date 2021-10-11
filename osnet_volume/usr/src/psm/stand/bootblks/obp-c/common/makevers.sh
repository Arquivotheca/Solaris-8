#!/bin/sh
#
# Copyright (c) 1991-1994, Sun Microsystems, Inc.
# All rights reserved.
#
#ident "@(#)makevers.sh	1.3	94/11/29 SMI"

DEV_CM=$1
ECHO=$2
BOOTER=$3
FILENAME=$4

BANNER="${BOOTER} 1.0 #"

test -f ${BOOTER}.version || echo 0 > ${BOOTER}.version
read OLDVERS < ${BOOTER}.version; export OLDVERS
VERS=`expr ${OLDVERS} + 1`
echo $VERS > ${BOOTER}.version

(
	SCCSSTRING="@(#)${FILENAME}\tDERIVED\t`date +%y/%m/%d` SMI"
	${ECHO} "/*" ; \
	${ECHO} " * This file is derived by makevers.sh" ; \
	${ECHO} " */\n" ; \
	${ECHO} "#pragma\tident\t\"${SCCSSTRING}\"\n" ; \
	${ECHO} "char ident[] = \"@(#)${BANNER}${VERS} "`date +%D`"\\\n\";" 
) > ${FILENAME}
