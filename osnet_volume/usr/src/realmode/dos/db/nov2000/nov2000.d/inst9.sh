#! /bin/sh
#
# Copyright (c) 1996 Sun Microsystems, Inc. All rights reserved.
#
# @(#)inst9.sh	1.9	96/10/18 SMI

# Script run at the end of Novell 2000/2000plus installation

rem_drv -b /a smc
rem_drv -b /a iee
rem_drv -b /a elink
rem_drv -b /a el
rem_drv -b /a eepro
rem_drv -b /a pcn
# rem_drv -b /a fmvel
rem_drv -b /a tiqmouse

# Remove disabling of nei driver. See postinstall comments for details.

if grep -s 'exclude.*nei' /a/etc/system >/dev/null 2>&1
then
	ed /a/etc/system <<STOP'2' > /dev/null 2>&1
g/exclude.*nei/d
w
q
STOP2
fi

# Enable nei driver.
if grep -s 'nei_forceload' /a/etc/system >/dev/null 2>&1
then
	:
else
	ed /a/etc/system <<STOP'3' > /dev/null 2>&1
a
set	nei:nei_forceload = 1
.
w
q
STOP3
fi
