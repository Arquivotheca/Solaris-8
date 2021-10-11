#
# Copyright (c) 1999 by Sun Microsystems, Inc.  
# All rights reserved.
#
# @(#)makefile	1.6	99/09/30 SMI

CM_DIR=..\..\cassist\common\lib
CFLAGS=/O /Fm /Fc /c /AS /Gs /G3 /X /DMSC60
INCLUDEDIR=	/I..\..\cassist\common\include \
		/I..\..\..\common\include \
		/I..\..\..\inc \
		/I..\..\common \
		/I..\..\..\inc\common \
		/I..\..\..\inc\i86 \
		/I..
OBJS1=	$(CM_DIR)\crt0.obj \
	..\..\common\low.obj \
	..\..\network\dssubs.obj \
	..\..\common\util.obj \
	..\..\common\putnum.obj \
	..\..\common\pciconf.obj \
	..\rplagent.obj \
	..\..\common\get_conf.obj

OBJS2=  ..\..\common\is_eisa.obj \
	..\..\common\iutil.obj \
	iprb.obj\
	..\..\common\__ntohs.obj \
	..\..\common\setvect.obj \
	..\..\network\network.obj

OBJS=$(OBJS1) $(OBJS2)
LIBS= $(CM_DIR)\common + $(CM_DIR)\slibce

install: all
	copy iprb.bef ..\..\..\proto\iprb.bef

all: iprb.bef

iprb.bef: $(OBJS)
	link /M @<<link.dat
$(OBJS1)+
$(OBJS2)
iprb.bef,
iprb.map,
$(LIBS);
<<

iprb.obj: iprb.c iprb.h ..\mii\mii.c ..\mii\mii.h ..\mii\miipriv.h ..\mii\miiregs.h
	cl $(CFLAGS) $(INCLUDEDIR) %s

clean:
	if exist *.obj del *.obj
	if exist *.cod del *.cod
	if exist *.map del *.map
	if exist *.bef del *.bef
