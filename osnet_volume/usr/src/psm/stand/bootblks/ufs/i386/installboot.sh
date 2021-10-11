#!/bin/sh
#
# Copyright (c) 1994-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)installboot.sh	1.2	97/03/27 SMI"
#

away() {
	echo $2 1>&2
	exit $1
}

Usage="Usage: `basename $0` pboot bootblk raw-device"

test $# -ne 3 && away 1 "$Usage"

PBOOT=$1
BOOTBLK=$2
DEVICE=$3
test ! -f $PBOOT && away 1 "$PBOOT: File not found"
test ! -f $BOOTBLK && away 1 "$BOOTBLK: File not found"
test ! -c $DEVICE && away 1 "$DEVICE: Not a character device"
test ! -w $DEVICE && away 1 "$DEVICE: Not writeable"

# pboot at block 0, label at blocks 1 and 2, bootblk from block 3 on
stderr=`dd if=$PBOOT of=$DEVICE bs=1b count=1 conv=sync 2>&1`
err=$? ; test $err -ne 0 && away $err "$stderr"
stderr=`dd if=$BOOTBLK of=$DEVICE bs=1b oseek=3 conv=sync 2>&1`
err=$? ; test $err -ne 0 && away $err "$stderr"
exit 0
