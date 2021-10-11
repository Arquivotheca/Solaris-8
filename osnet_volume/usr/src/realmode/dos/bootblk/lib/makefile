#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile	1.4	99/01/31 SMI

MLFLAGS=/nologo /c /Cp /Cx /Fl /Sa /Sn /Zd /Zi /Sg
ML= ml

OBJS=ul_mul.obj us_div.obj us_mod.obj ll_shift.obj int.obj rl_shift.obj
TARGET=smalllib.lib

all: $(TARGET)
$(TARGET): $(OBJS)
	if exist $(TARGET) del $(TARGET)
	lib $(TARGET) +ul_mul.obj +us_div.obj +us_mod.obj +ll_shift.obj \
		+rl_shift.obj +int.obj, tinylib.lst

clean:
	if exist *.obj del *.obj
	if exist *.lst del *.lst
	if exist $(TARGET) del $(TARGET)

ul_mul.obj: ul_mul.s
	$(ML) $(MLFLAGS) %s

us_div.obj: us_div.s
	$(ML) $(MLFLAGS) %s

us_mod.obj: us_mod.s
	$(ML) $(MLFLAGS) %s

ll_shift.obj: ll_shift.s
	$(ML) $(MLFLAGS) %s

rl_shift.obj: rl_shift.s
	$(ML) $(MLFLAGS) %s

int.obj: int.s
	$(ML) $(MLFLAGS) %s
