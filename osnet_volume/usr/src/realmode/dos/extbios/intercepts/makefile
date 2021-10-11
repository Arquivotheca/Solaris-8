#	@(#)makefile	1.3
#
# Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
#
 

CFLAGS=/Od /Fm /Fc /c /AT /Gs /G2 /X
MLFLAGS=/Cp /Cx /nologo /c /Fl /Sa
INCLUDEDIR=/I..\..\inc
COM_OBJS= low.obj \
	..\common\util.obj \
	..\common\putnum.obj \
	..\common\iutil.obj \
	int.obj

all: inter10.bef inter16.bef

inter10.bef: inter10.obj $(COM_OBJS)
	link /NOLOGO /M /NOD /NOE @<<link.dat
$(COM_OBJS)+
inter10.obj
inter10.bef;
<<
	copy inter10.bef ..\..\proto\inter10.bef;

inter16.bef: inter16.obj $(COM_OBJS)
	link /NOLOGO /M /NOD /NOE @<<link.dat
$(COM_OBJS)+
inter16.obj
inter16.bef;
<<
	copy inter16.bef ..\..\proto\inter16.bef;

inter10.obj: inter10.c global.h
	cl $(CFLAGS) /DMSC60 /DBEF $(INCLUDEDIR) inter10.c

inter16.obj: inter16.c global.h
	cl $(CFLAGS) /DMSC60 /DBEF $(INCLUDEDIR) inter16.c

int.obj: int.s
	ml $(MLFLAGS) /Taint.s

low.obj: low.s
	ml $(MLFLAGS) /Talow.s

clean:
	-del *.obj
	-del *.cod
	-del *.map
	-del *.bef

