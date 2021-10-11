#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile        1.12     99/09/30 SMI

CM_DIR= ..\..\cassist\common\lib
CFLAGS=/O /Gs /Fm /Fc /c /AS /G2 /X
INCLUDEDIR=/I..\..\common /I..\..\..\inc /I..\..\..\common\include
OBJS=	$(CM_DIR)\crt0.obj \
	..\..\common\low.obj \
	..\..\common\util.obj \
	..\..\common\putnum.obj \
	..\..\common\kbchar.obj \
	..\..\common\get_conf.obj \
	.\bcopy.obj \
	.\tmc850.obj \
	..\scsi.obj \
	..\..\common\iutil.obj

LIBS= $(CM_DIR)\common + $(CM_DIR)\slibce
install: all
	copy tmc.bef ..\..\..\proto\tmc.bef
all: tmc.bef

tmc.bef: $(OBJS)
	link /NOLOGO /M @<<link.dat
$(OBJS)
tmc.bef,
tmc.map,
$(LIBS);
<<

tmc850.obj: tmc850.c tmc850.h
	cl $(CFLAGS) $(INCLUDEDIR) tmc850.c
bcopy.obj: bcopy.asm
	ml /I. /Zm /c /Cx bcopy.asm

clean:
	if exist *.obj del *.obj
	if exist *.exe del *.exe
	if exist *.cod del *.cod
	if exist *.map del *.map
	if exist *.bef del *.bef
	if exist *.lst del *.lst
