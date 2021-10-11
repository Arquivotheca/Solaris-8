#! /bin/sh
#
# Copyright (c) 1996 Sun Microsystems, Inc.
# All rights reserved.
#
#ident "@(#)installdu.sh	1.22	96/01/19 SMI"

# installdu.sh -- install Driver Update patch
#
ROOTDIR=
if [ -n "$1" ]; then
	ROOTDIR="-R $1"
fi

if [ -f cpioimage.Z ]; then
	uncompress cpioimage
	if [ $? != 0 ]; then
		exit 1;
	fi
fi
if [ -f cpioimage ]; then
	cpio -icudB <cpioimage
	if [ $? != 0 ]; then
		exit 1;
	fi
	rm cpioimage
fi

for f in *
do
	if [ -d $f ]; then
		(cd $f; ../installpatch $ROOTDIR -u `pwd`)
	fi
done
