#! /bin/sh
#
# Copyright (c) 1995 Sun Microsystems, Inc. All rights reserved.
#
# @(#)inst9.sh	1.1	95/05/15 SMI

# Script run at the end of INTERGRAPH installation
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
