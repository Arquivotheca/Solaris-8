#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# "@(#)makefile 1.10     99/09/30 SMI"
#

CM_DIR=..\..\cassist\common\lib
#CFLAGS=/O /Fm /Fc /c /AS /Gs /G2 /X /DMSC60 /DPCNDEBUG
CFLAGS=/O /Fm /Fc /c /AS /Gs /G2 /X /DMSC60
# INCLUDEDIR=/I..\..\..\inc\common /I..\..\..\inc\i86 /I..\..\common /I..
INCLUDEDIR=	/I..\..\cassist\common\include \
		/I..\..\..\common\include \
		/I..\..\..\inc  \
		/I..\..\common \
		/I..
OBJS1=	$(CM_DIR)\crt0.obj \
	..\..\common\low.obj \
	..\..\network\dssubs.obj \
	..\..\common\util.obj \
	..\..\common\kbchar.obj \
	..\..\common\putnum.obj \
	..\rplagent.obj \
	..\..\common\get_conf.obj
OBJS2=  ..\..\common\is_eisa.obj \
	..\..\common\pciconf.obj \
	..\..\common\iutil.obj \
	pcn.obj \
	..\..\common\__ntohs.obj \
	..\..\common\setvect.obj \
	..\..\network\network.obj

LIBS=$(CM_DIR)\common + $(CM_DIR)\slibce
OBJS=$(OBJS1) $(OBJS2)

install: all
	copy pcn.bef ..\..\..\proto\pcn.bef

all: pcn.bef

pcn.bef: $(OBJS)
	link /M @<<link.dat
$(OBJS1)+
$(OBJS2)
pcn.bef,
pcn.map,
$(LIBS);
<<

pcn.obj: pcn.c pcn.h
	cl $(CFLAGS) $(INCLUDEDIR) %s

pciconf.obj: pciconf.s
	ml /DFARCODE=0 /Dmodel=SMALL /Tapciconf.s

clean:
	-del *.obj
	-del *.cod
	-del *.map
	-del *.bef
