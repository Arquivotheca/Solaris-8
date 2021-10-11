#
# Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
#
 
# @(#)makefile        1.7     95/08/31 SMI

CFLAGS=/Fm /Fc /c /AS /Gs /G2 /X /I..\common /I..\..\inc /I..\..\common\include /I..\cassist\common\include

install all: dssubs.obj network.obj

dssubs.obj: dssubs.s
	masm dssubs.s

network.obj: network.c
	cl $(CFLAGS) network.c

clean:
	-del *.obj
	-del *.cod
	-del *.lst

