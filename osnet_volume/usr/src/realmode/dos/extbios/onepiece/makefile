#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile        1.46     00/09/14 SMI

CFLAGS=/O /Fm /Fc /c /AS /Gs /G2 /X /DMSC60
INCLUDEDIR=\
	/I..\..\inc\i86 \
	/I..\..\inc \
	/I..\common \
	/I..\..\common\include \
	/I..\cassist\common\include

SUBDIRS=ieef iprb dnet pcn

XMODS=


install: rplagent.obj subsins
all: rplagent.obj subsall

rplagent.obj: rplagent.c rplagent.h
	cl $(CFLAGS) $(INCLUDEDIR) rplagent.c

subsins: $(SUBDIRS) $(XMODS)
	!makesub.bat $** install

subsall: $(SUBDIRS) $(XMODS)
	!makesub.bat $** all

subscln: $(SUBDIRS) $(XMODS)
	!makesub.bat $** clean

clean: subscln
	if exist *.obj del *.obj
	if exist *.cod del *.cod

