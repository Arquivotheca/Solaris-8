#!/bin/sh
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident "@(#)rcs3.sh 1.7 99/04/03 SMI"
#
# this script is copied from the boot floppy very early by the install
# program and later run right after /usr is mounted.  its purpose is to
# preserve files from the floppy that should end up on the live system.
# there are two files we preserve: escd.rf and bootenv.rc.
#
# errors just cause us to give up.  the booting system is prepared for
# the case where these files did not get copied.
#
PRESERVE_LIST="/escd.rf /solaris/bootenv.rc"
FLOPPY_ROOT="/tmp/mnt$$"
DEST_DIR=/boot
TMP_ROOT=/tmp/root

VERS="sol_`uname -r | sed -e's/\.//g' -e 's/5/2/'`"
OPT="ro,foldcase"
case $VERS in
        sol_25)VERS="sol_25"; OPT="ro" ;;
        sol_251)VERS="sol_251"; OPT="ro";;
esac

#
# mount up the floppy
#
mkdir $FLOPPY_ROOT || exit 0
mount -F pcfs -o $OPT /dev/diskette0 $FLOPPY_ROOT 2> /dev/null || exit 0
#
# copy out the interesting files
#
for file in $PRESERVE_LIST
do
	src=${FLOPPY_ROOT}${file}
	dst=${TMP_ROOT}${DEST_DIR}${file}
	dstdir=`dirname $dst`
	[ -d $dstdir ] || mkdir -p $dstdir
	[ -f $src ] && cp $src $dst
	[ -f $dst ] && chmod 644 $dst
done
#
# cleanup
#
umount /dev/diskette0
rmdir $FLOPPY_ROOT
