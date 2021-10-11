#
#ident	"@(#)Makefile	1.8	99/09/14 SMI"
#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/adb/i386/Makefile
#

ADB_BASE_DIR	= ..
ISADIR		= .
COMMONDIR	= $(ADB_BASE_DIR)/common
SYSDIR		= $(ADB_BASE_DIR)/..

SRCS += \
    dk_vtoc.dbg \
    scsi_disk.dbg \
    systemdump.dbg

SRCS += \
	hat.dbg \
	hat.nxt.dbg \
	hwpp.dbg \
	hwpp.nxt.dbg \
	machpp.dbg \
	mcpu.dbg \
	machcpu.dbg \
	hatppp.dbg

SRCS += \
	xmount.dbg \
	xnode.dbg \
	segxdata.dbg


include	$(COMMONDIR)/Makefile.com
