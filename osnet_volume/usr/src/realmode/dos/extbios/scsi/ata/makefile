#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile        1.12     99/09/30 SMI

CM_DIR= ..\..\cassist\common\lib 
CFLAGS=/O /Gs /Fm /Fc /c /AS /G2 /X
#CFLAGS=/DDEBUG /O /Gs /Fm /Fc /c /AS /G2 /X

INCLUDEDIR= \
	/I..\..\common \
	/I..\..\..\inc \
	/I..\..\..\common\include \
	/I..\..\cassist\common\include \

OBJS=	$(CM_DIR)\crt0.obj \
	..\..\common\low.obj \
	..\..\common\pciconf.obj \
	..\..\common\util.obj \
	..\..\common\putnum.obj \
	..\..\common\kbchar.obj \
	readphys.obj \
	ataconf.obj \
	ata.obj \
	atapi.obj \
	main.obj \
	spl.obj \
	..\scsi.obj

IOBJS=	..\..\common\iutil.obj

LIBS= $(CM_DIR)\common + $(CM_DIR)\slibce

install: all
	copy ata.bef ..\..\..\proto\ata.bef

all: ata.bef

ata.bef: $(OBJS)
	link /NOLOGO /M @<<link.dat
$(OBJS) +
$(IOBJS)
ata.bef,
ata.map,
$(LIBS);
<<


main.obj: main.c ata.h
	cl $(CFLAGS) $(INCLUDEDIR) main.c

ataconf.obj: ataconf.c ata.h
	cl $(CFLAGS) $(INCLUDEDIR) ataconf.c

atapi.obj: atapi.c ata.h atapi.h
	cl $(CFLAGS) $(INCLUDEDIR) atapi.c

ata.obj: ata.c ata.h
	cl $(CFLAGS) $(INCLUDEDIR) ata.c

readphys.obj: readphys.asm
	ml /I. /Zm /c /Cx readphys.asm

spl.obj: spl.asm
	ml /I. /Zm /c /Cx spl.asm

clean:
	if exist *.obj del *.obj
	if exist *.cod del *.cod
	if exist *.map del *.map
	if exist *.bef del *.bef


ATAPROBEOBJS= \
	ataprobe.obj \
	..\..\common\util.obj \
	..\..\common\putnum.obj \
	spl.obj \
	readphys.obj

ataprobe.obj: ataprobe.c ataprobe.h
	cl /c /AS /O /G2 $(INCLUDEDIR) ataprobe.c 

ataprobe: $(ATAPROBEOBJS)
	cl /Fm $(ATAPROBEOBJS) /link c:\vc\lib\  


chkext.obj: chkext.c 
	cl /c /AS /O /G2 chkext.c 

chkext: chkext.obj
	cl /Fm chkext.obj /link c:\vc\lib\  

