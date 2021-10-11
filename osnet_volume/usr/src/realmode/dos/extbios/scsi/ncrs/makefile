#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile        1.16     99/09/30 SMI

CM_DIR= ..\..\cassist\common\lib
CFLAGS=/O /Gs /Fm /Fc /c /AS /G2 /X
MLFLAGS=/Cp /Cx /nologo /c /Fl /Sa
INCLUDEDIR= \
	/I..\..\common \
	/I..\..\..\inc \
	/I..\..\..\common\include \
	/I..\..\cassist\common\include \

OBJS=	$(CM_DIR)\crt0.obj \
	..\..\common\low.obj \
     	..\..\common\util.obj \
     	..\..\common\putnum.obj \
	..\..\common\kbchar.obj \
	script.obj \
	53c710.obj \
	53c810.obj \
	init.obj \
	intr.obj \
	ncr.obj \
	ccb.obj \
	..\..\common\pciconf.obj \
	..\scsi.obj
IOBJS=	..\..\common\is_eisa.obj \
	..\..\common\iutil.obj

LIBS= $(CM_DIR)\common + $(CM_DIR)\slibce
install: all
	copy ncrs.bef ..\..\..\proto\ncrs.bef
all: ncrs.bef

ncrs.bef: $(OBJS) $(IOBJS)
	link /M @<<link.dat
$(OBJS) +
$(IOBJS)
ncrs.bef,
ncrs.map,
$(LIBS);
<<

ncr.obj: ncr.c ncr.h ncr_sni.h
	cl $(CFLAGS) $(INCLUDEDIR) ncr.c

53c710.obj: 53c710.c ncr.h 53c710.h
	cl $(CFLAGS) $(INCLUDEDIR) 53c710.c

53c810.obj: 53c810.c ncr.h 53c8xx.h
	cl $(CFLAGS) $(INCLUDEDIR) 53c810.c

init.obj: init.c ncr.h
	cl $(CFLAGS) $(INCLUDEDIR) init.c

intr.obj: intr.c ncr.h
	cl $(CFLAGS) $(INCLUDEDIR) intr.c

script.obj: script.c script.h scr.out
	cl $(CFLAGS) $(INCLUDEDIR) script.c

ccb.obj: ccb.c ncr.h
	cl $(CFLAGS) $(INCLUDEDIR) ccb.c

ncr.h: ncr_scsi.h intr.h script.h ncrops.h ncrdefs.h debug.h

clean:
	if exist *.obj del *.obj
	if exist *.cod del *.cod
	if exist *.map del *.map
	if exist *.bef del *.bef
