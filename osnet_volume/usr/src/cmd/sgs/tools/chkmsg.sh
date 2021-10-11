#!/bin/sh
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)chkmsg.sh	1.9	99/12/06 SMI"

TOOLDIR="${SRC}/cmd/sgs/tools/"

#
# remove the temporary files
#
rm -f CATA_MSG_INTL_LIST CATA_MSG_ORIG_LIST
rm -f MSG_INTL_LIST MSG_ORIG_LIST

while getopts "m:" Arg
do
	case $Arg in
	m)	nawk -f ${TOOLDIR}/catalog.awk $OPTARG ;;
	\?)	echo "usage: chkmsg -m msgfile source-files" ; exit 1 ;;
	esac
done
shift `expr $OPTIND - 1`

if [ $# -eq 0 ]; then
	echo "usage: chkmsg -m msgfile source-files"
	exit 1
fi

#
# Sort the MSG_INTL() and MSG_ORIG() entries.
#
if [ -s CATA_MSG_INTL_LIST ] ; then
	sort CATA_MSG_INTL_LIST | uniq > _TMP
	mv _TMP CATA_MSG_INTL_LIST
fi
if [ -s CATA_MSG_ORIG_LIST ] ; then
	sort CATA_MSG_ORIG_LIST | uniq > _TMP
	mv _TMP CATA_MSG_ORIG_LIST
fi

#
# Generate the lists for the source files and sort them
#
nawk -f  ${TOOLDIR}/getmessage.awk	$*

if [ -s MSG_INTL_LIST ] ; then
	sort MSG_INTL_LIST | uniq > _TMP
	mv _TMP MSG_INTL_LIST
fi
if [ -s MSG_ORIG_LIST ] ; then
	sort MSG_ORIG_LIST | uniq > _TMP
	mv _TMP MSG_ORIG_LIST
fi

#
# Start checking
#
Error=0

#
# Check MESG_INTL message
#
comm -23 CATA_MSG_INTL_LIST MSG_INTL_LIST > _TMP 2> /dev/null
if [ -s _TMP ]; then
    echo
    echo "messages exist between _START_ and _END_ but do not use MSG_INTL()"
    cat _TMP | sed "s/^/	/"
    Error=1
fi
rm -f _TMP

comm -13 CATA_MSG_INTL_LIST MSG_INTL_LIST > _TMP 2> /dev/null
if [ -s _TMP ]; then
    echo
    echo "use of MSG_INTL() but messages do not exist between _START_ and _END_"
    cat _TMP | sed "s/^/	/"
    Error=1
fi
rm -f _TMP

#
# Check MESG_ORIG message
#
comm -23 CATA_MSG_ORIG_LIST MSG_ORIG_LIST > _TMP 2> /dev/null
if [ -s _TMP ]; then
    echo
    echo "messages exist after _END_ but do not use MSG_ORIG()"
    cat _TMP | sed "s/^/	/"
    Error=1
fi
rm -f _TMP

comm -13 CATA_MSG_ORIG_LIST MSG_ORIG_LIST > _TMP 2> /dev/null
if [ -s _TMP ]; then
    echo
    echo "use of MSG_ORIG() but messages do not exist after _END_"
    cat _TMP | sed "s/^/	/"
    Error=1
fi
rm -f _TMP

#
# remove the temporary files
#
rm -f CATA_MSG_INTL_LIST CATA_MSG_ORIG_LIST
rm -f MSG_INTL_LIST MSG_ORIG_LIST

exit $Error
