#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

# "@(#)makefile   1.17   99/09/30 SMI"

CM_LIB= ..\..\cassist\common\lib
CFLAGS=/O /Fm /Fc /c /AS /Gs /G2 /X /DMSC60
INCLUDEDIR=	\
	/I..\..\cassist\common\include \
	/I..\..\..\common\include \
	/I..\..\..\inc \
	/I..\..\common \
	/I..
OBJS1=	$(CM_LIB)\crt0.obj \
 	..\..\common\low.obj \
	..\..\common\pciconf.obj \
	..\..\network\dssubs.obj \
	..\..\common\util.obj \
	..\..\common\kbchar.obj \
	..\..\common\putnum.obj \
	..\rplagent.obj \
	..\..\common\get_conf.obj
OBJS2=	..\..\common\is_eisa.obj \
	..\..\common\iutil.obj \
	ieef.obj \
	spl.obj \
	..\..\common\__ntohs.obj \
	..\..\common\setvect.obj \
	..\..\network\network.obj

OBJS=$(OBJS1) $(OBJS2)
LIBS= $(CM_LIB)\common + $(CM_LIB)\slibce

install: all
	copy ieef.bef ..\..\..\proto\ieef.bef

all: ieef.bef

ieef.bef: $(OBJS)
	link /M @<<link.dat
$(OBJS1)+
$(OBJS2)
ieef.bef,
ieef.map,
$(LIBS);
<<
ieef.obj: ieef.c ieef.h makefile
	cl $(CFLAGS) $(INCLUDEDIR) %s

spl.obj: spl.s
	ml $(MLFLAGS) /Taspl.s

clean:
	if exist *.obj del *.obj
	if exist *.cod del *.cod
	if exist *.map del *.map
	if exist *.exe del *.exe
	if exist *.bef del *.bef
