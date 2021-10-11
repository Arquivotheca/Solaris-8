#! /bin/sh
#
# Copyright (c) 1995 Sun Microsystems, Inc. All rights reserved.
#
# @(#)inst9.sh	1.3	95/06/20 SMI

# Script run at the end of EDJ installation
# Disable pcplusmp module to workaround AMI bios bug on EDJ motherboards.

if grep -s 'set.*apic_forceload' /a/etc/system >/dev/null 2>&1
then
	:
else
	ed /a/etc/system <<STOP'2' > /dev/null 2>&1
a
set	pcplusmp:apic_forceload = -1
.
w
q
STOP2
fi

# Remove entries from esa.conf file to reduce likelihood of probing conflicts.

if grep -s '#removed these entries' /a/kernel/drv/esa.conf >/dev/null 2>&1
then
	:
else
	ed /a/kernel/drv/esa.conf <<STOP'3' > /dev/null 2>&1
/0xac00
.-1i
#removed these entries to avoid PCI probe conflicts
.
.+1,$s/^/#/
w
q
STOP3
fi
