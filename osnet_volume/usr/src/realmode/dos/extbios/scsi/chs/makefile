#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)makefile	1.4	99/09/30 SMI"


CM_DIR= ..\..\cassist\common\lib
CFLAGS=/O /Gs /Fm /Fc /c /AS /G2 /X /DMSCSI_FEATURE
INCLUDEDIR=/I..\..\common /I..\..\..\inc /I..\..\..\common\include
OBJS=	$(CM_DIR)\crt0.obj \
	..\..\common\low.obj \
	..\..\common\get_conf.obj \
	..\..\common\pciconf.obj
OBJS1 = ..\..\common\util.obj \
	..\..\common\putnum.obj \
	..\..\common\kbchar.obj \
	chs.obj \
	..\scsi.obj \
	..\..\common\iutil.obj

LIBS= $(CM_DIR)\common + $(CM_DIR)\slibce
install: all
	copy chs.bef ..\..\..\proto\chs.bef
all: chs.bef

chs.bef: $(OBJS) $(OBJS1)
	link  /PACKC /NOLOGO /M @<<link.dat
$(OBJS) +
$(OBJS1)
chs.bef,
chs.map,
$(LIBS);
<<

chs.obj: chs.c chs.h
	cl $(CFLAGS) $(INCLUDEDIR) chs.c

clean:
	if exist *.obj del *.obj
	if exist *.cod del *.cod
	if exist *.map del *.map
	if exist *.bef del *.bef
