#
# Copyright (c) 1999 Sun Microsystems, Inc. All rights reserved.
#
# @(#)makefile        1.57     00/09/14 SMI

CFLAGS=/O /Gs /Fc /c /AS /G2 /X
INCLUDEDIR=/I..\common /I..\..\inc /I..\..\common\include /I..\cassist\common\include

SUBDIRS= \
	ata chs dpt ncrs tmc
XMODS=


install: scsi.obj subsins
all: scsi.obj subsall
clean: subscln

subsins: $(SUBDIRS) $(XMODS)
	!makesub.bat $** install

subsall: $(SUBDIRS) $(XMODS)
	!makesub.bat $** all

subscln: $(SUBDIRS) $(XMODS)
	-del *.obj
	-del *.cod
	!makesub.bat $** clean

scsi.obj: scsi.c scsi.h
	cl $(CFLAGS) $(INCLUDEDIR) %s

