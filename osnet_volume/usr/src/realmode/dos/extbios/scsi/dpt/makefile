#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile        1.15     99/09/30 SMI

CM_DIR= ..\..\cassist\common\lib 
CFLAGS=/O /Gs /Fm /Fc /c /AS /G2 /X
INCLUDEDIR=/I..\..\common /I..\..\..\common\include /I..\..\..\inc /I..\..\cassist\common\include
OBJS=	$(CM_DIR)\crt0.obj \
	..\..\common\low.obj \
	..\..\common\pciconf.obj \
	..\..\common\util.obj \
	..\..\common\putnum.obj \
	..\..\common\kbchar.obj \
	dptcmd.obj \
	..\scsi.obj \
	..\..\common\iutil.obj

LIBS= $(CM_DIR)\common + $(CM_DIR)\slibce
install: all
	copy dpt.bef ..\..\..\proto\dpt.bef
all: dpt.bef

dpt.bef: $(OBJS)
	link /NOLOGO /M @<<link.dat
$(OBJS)
dpt.bef,
dpt.map,
$(LIBS);
<<

dptcmd.obj: dptcmd.c dpt.h
	cl $(CFLAGS) $(INCLUDEDIR) dptcmd.c

clean:
	if exist *.obj del *.obj
	if exist *.cod del *.cod
	if exist *.map del *.map
	if exist *.bef del *.bef
