#
# Copyright (c) 1995 Sun Microsystems, Inc. All rights reserved.
#
 
# @(#)makefile        1.10     99/07/20 SMI

MLOBJS=low.obj util.obj kbchar.obj setup.obj __ntohs.obj \
	get_conf.obj pciconf.obj
CLOBJS=iutil.obj is_eisa.obj putnum.obj setvect.obj
OBJECTS=$(MLOBJS) $(CLOBJS)
MLFLAGS=/Cp /Cx /nologo /c /Fl /Sa
CFLAGS=/Fc /nologo /c /AS /Gs /G2 /X /I..\cassist\common\include /I..\..\common\include

install: all
all: $(MLOBJS) $(CLOBJS)

low.obj: low.s
	ml $(MLFLAGS) /Talow.s

util.obj: util.s
	ml $(MLFLAGS) /Tautil.s

kbchar.obj: kbchar.s
	ml $(MLFLAGS) /Takbchar.s

setup.obj: setup.s
	ml $(MLFLAGS) /Tasetup.s

__ntohs.obj: __ntohs.s
	ml $(MLFLAGS) /Ta__ntohs.s

get_conf.obj: get_conf.s
	ml $(MLFLAGS) /DFARCODE=0 /Dmodel=SMALL /Taget_conf.s

pciconf.obj: pciconf.s
	ml $(MLFLAGS) /DFARCODE=0 /Dmodel=SMALL /Tapciconf.s

iutil.obj: iutil.c
	cl $(CFLAGS) iutil.c

is_eisa.obj: is_eisa.c
	cl $(CFLAGS) is_eisa.c

putnum.obj: putnum.c
	cl $(CFLAGS) putnum.c

setvect.obj: setvect.c
	cl $(CFLAGS) setvect.c

clean:
	-del *.obj
	-del *.cod
	-del *.lst

