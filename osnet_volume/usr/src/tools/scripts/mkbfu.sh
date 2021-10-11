#!/bin/ksh
#
# Copyright (c) 1993-1998 by Sun Microsystems, Inc.
# All rights reserved.
# 
#ident	"@(#)mkbfu.sh	1.2	99/08/11 SMI"
#
# Make archives suitable for bfu

fail() {
	echo $*
	exit 1
}

test $# -eq 2 || fail "Usage: $0 proto-dir archive-dir"

PROTO=$1
CPIODIR=$2

test -d $PROTO || fail "Proto directory does not exist."

cd $PROTO

rm -rf $CPIODIR
mkdir -p $CPIODIR

echo "Creating generic root archive:\t\c"
(	FILELIST=`ls . | grep -v usr | grep -v platform | sed -e "s@^@./@"`
	find $FILELIST -depth -print
	echo "./usr"
	echo "./platform"
) | cpio -ocB >${CPIODIR}/generic.root

echo "Creating generic usr archive:\t\c"
(	FILELIST=`ls ./usr | grep -v platform | sed -e "s@^@./usr/@"`
	find $FILELIST -depth -print | \
	    egrep -v -e "./usr/share/src"
	echo "./usr/platform"
) | cpio -ocB >${CPIODIR}/generic.usr

for i in `( cd platform; find * -prune -type d -print )`
do
	echo "Creating $i root archive:\t\c"
	(	FILELIST=`ls -l ./platform | grep -w $i | \
		    sed -e "s/  */ /g" | cut -d' ' -f 9 | \
		    sed -e "s@^@./platform/@"`
		find $FILELIST -depth -print
	) | cpio -ocB >${CPIODIR}/${i}.root
	echo "Creating $i usr archive:\t\c"
	(	FILELIST=`ls -l ./usr/platform | grep -w $i | \
		    sed -e "s/  */ /g" | cut -d' ' -f 9 | \
		    sed -e "s@^@./usr/platform/@"`
		find $FILELIST -depth -print
	) | cpio -ocB >${CPIODIR}/${i}.usr
done
