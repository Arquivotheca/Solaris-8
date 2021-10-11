#!/bin/sh
#
# Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
#
#
#ident	"@(#)create_cklist.sh	1.3	99/04/13 SMI"

# create_cklist - creates a snapshot of the contents of the directories
#		  designated by env variable CKLISTPATH.
#		  Return error if the checklist file is already created.

cklistfile=$1
tmpckfile=${ASETDIR}/tmp/tmpcklist.$$

if [ -s $cklistfile ]
then
	echo
	echo create_cklist: the file already exists - $cklistfile
	exit 3
fi

if [ "${CKLISTPATH}" = "" ]
then
	echo
	echo Env variable CKLISTPATH undefined.
	echo Check ${ASETDIR}/asetenv file.
	echo $QUIT
	exit 3
fi
gooddir=false
OLDIFS=$IFS
IFS=":"
for i in ${CKLISTPATH}
do
	if [ ! -d $i ]
	then
		echo
		echo "create_cklist: Directory $i does not exist."
		echo "Check env variable \c"
		echo "$CKLISTPATH\c"
		echo " in ${ASETDIR}/asetenv file."
		continue
	else
		gooddir=true
		/usr/aset/util/nls -ldaC $i/* | $ADDCKSUM >> $tmpckfile
	fi
done

$CAT $tmpckfile | $SED "/cklist.${ASETSECLEVEL}/d" > $cklistfile
$RM $tmpckfile

IFS=$OLDIFS
if [ "$gooddir" = "false" ]
then
	# none of the directories were good
	echo
	echo Bad env variable $CKLISTPATH
	echo Check ${ASETDIR}/asetenv file.
	echo $QUIT
	exit 3
fi
