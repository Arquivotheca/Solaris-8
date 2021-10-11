#! /bin/sh
#
# Copyright (c) 1994 Sun Microsystems, Inc. All rights reserved.
#
# @(#)inst9.sh	1.9	94/06/24 SMI

# Script run at the end of Driver Update installation

echo " "
echo "Please insert the Driver Update Distribution diskette into drive zero."
echo "Press <ENTER> when ready."
read x

mkdir /a/tmp/Drivers
cd /a/tmp/Drivers
cpio -icduB -I /dev/rdiskette0 -M "Insert Driver Update Distribution diskette %d.  Press <ENTER> when ready"

echo " "
echo "Please remove the diskette from drive zero."
echo "Press <ENTER> when ready."
read x

cd /a/tmp/Drivers
./installdu.sh /a
cd /
rm -rf /a/tmp/Drivers
