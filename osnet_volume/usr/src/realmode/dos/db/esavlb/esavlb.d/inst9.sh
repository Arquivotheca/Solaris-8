#! /bin/sh
#
# Copyright (c) 1995 Sun Microsystems, Inc. All rights reserved.
# @(#)inst9.sh	1.3	95/12/12 SMI
#

# Script run at the end of ESAVLB installation
# Enable esa driver probing for VLb cards for all slots through setting 
# esa_vlb_probe to 0xffff

if grep -s 'set.*esa_vlb_probe' /a/etc/system >/dev/null 2>&1
then
	:
else
	ed /a/etc/system <<STOPESA > /dev/null 2>&1
a
set	esa:esa_vlb_probe =  0xffff
.
w
q
STOPESA
fi
