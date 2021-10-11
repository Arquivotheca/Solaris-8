#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile	1.4	98/05/29 SMI

CFLAGS=/nologo /O /c /X /Zi /Gs /G2 /Zd /DFARDATA /AT /Fm /Fx /Fc /Zp4
MLFLAGS=/nologo /DFARDATA=1 /DFARCODE=0 /Dmodel=SMALL /c /Cp /Cx /Fl /Sa /Sn /Zf /Zd /Zi /Sg
INCLUDEDIR=/I..\..\inc
MLOBJS= ask_disk.obj ask_time.obj ask_page.obj clrscr_c.obj clrscrn.obj \
	farbcopy.obj ll_shift.obj ls_shift.obj flush_kb.obj nwrd_key.obj \
	pause_ms.obj putchar.obj rd_cursr.obj prtstr_p.obj readkey.obj \
	rd_disk.obj prtstr_a.obj rs_shift.obj set_page.obj rsetdisk.obj \
	st_cursr.obj rl_shift.obj ul_mul.obj us_div.obj waitkey.obj us_mod.obj
CLOBJS= _ctype.obj atoi.obj atox.obj bcopy.obj itoa.obj \
	farbzero.obj memset.obj printf.obj putstr.obj strcspn.obj \
	strcpy.obj strcmp.obj strcat.obj strlen.obj strncmp.obj \
	strncat.obj testpt.obj strncpy.obj xtoa.obj
OBJECTS=$(MLOBJS) $(CLOBJS)
TARGET=small.lib

all: $(TARGET)

$(TARGET): $(OBJECTS)
	del $(TARGET)
	lib @<<smalllib.rsp
small.lib +ask_page +ask_time +pause_ms +clrscrn +clrscr_c +farbcopy &
	+putchar +putstr +prtstr_p +prtstr_a +printf &
	+rd_cursr +set_page +st_cursr +bcopy &
	+rd_disk +rsetdisk +readkey +waitkey +nwrd_key +flush_kb & 
	+strcmp +strlen +strncpy +strcat +strncat +strncmp +strcspn +strcpy &
	+itoa +xtoa +memset +_ctype +atoi +atox &
	+rs_shift +rl_shift +ls_shift +ll_shift +us_div +us_mod +ul_mul &
	+testpt, small.lst
<<

ask_disk.obj: ..\src\ask_disk.s
	@ml $(MLFLAGS) %s

ask_time.obj: ..\src\ask_time.s
	@ml $(MLFLAGS) %s

ask_page.obj: ..\src\ask_page.s
	@ml $(MLFLAGS) %s

clrscr_c.obj: ..\src\clrscr_c.s
	@ml $(MLFLAGS) %s

clrscrn.obj: ..\src\clrscrn.s
	@ml $(MLFLAGS) %s

farbcopy.obj: ..\src\farbcopy.s
	@ml $(MLFLAGS) %s

ll_shift.obj: ..\src\ll_shift.s
	@ml $(MLFLAGS) %s

ls_shift.obj: ..\src\ls_shift.s
	@ml $(MLFLAGS) %s

flush_kb.obj: ..\src\flush_kb.s
	@ml $(MLFLAGS) %s

nwrd_key.obj: ..\src\nwrd_key.s
	@ml $(MLFLAGS) %s

pause_ms.obj: ..\src\pause_ms.s
	@ml $(MLFLAGS) %s

putchar.obj: ..\src\putchar.s
	@ml $(MLFLAGS) %s

rd_cursr.obj: ..\src\rd_cursr.s
	@ml $(MLFLAGS) %s

prtstr_p.obj: ..\src\prtstr_p.s
	@ml $(MLFLAGS) %s

readkey.obj: ..\src\readkey.s
	@ml $(MLFLAGS) %s

rd_disk.obj: ..\src\rd_disk.s
	@ml $(MLFLAGS) %s

prtstr_a.obj: ..\src\prtstr_a.s
	@ml $(MLFLAGS) %s

rs_shift.obj: ..\src\rs_shift.s
	@ml $(MLFLAGS) %s

set_page.obj: ..\src\set_page.s
	@ml $(MLFLAGS) %s

rsetdisk.obj: ..\src\rsetdisk.s
	@ml $(MLFLAGS) %s

st_cursr.obj: ..\src\st_cursr.s
	@ml $(MLFLAGS) %s

rl_shift.obj: ..\src\rl_shift.s
	@ml $(MLFLAGS) %s

ul_mul.obj: ..\src\ul_mul.s
	@ml $(MLFLAGS) %s

us_div.obj: ..\src\us_div.s
	@ml $(MLFLAGS) %s

waitkey.obj: ..\src\waitkey.s
	@ml $(MLFLAGS) %s

us_mod.obj: ..\src\us_mod.s
	@ml $(MLFLAGS) %s

_ctype.obj: ..\src\_ctype.c
	@cl $(CFLAGS) $(INCLUDEDIR) $(INCLUDEDIR)\i86 %s

atoi.obj: ..\src\atoi.c
	@cl $(CFLAGS) $(INCLUDEDIR) $(INCLUDEDIR)\i86 %s

atox.obj: ..\src\atox.c
	@cl $(CFLAGS) $(INCLUDEDIR) $(INCLUDEDIR)\i86 %s

bcopy.obj: ..\src\bcopy.c
	@cl $(CFLAGS) $(INCLUDEDIR) %s

itoa.obj: ..\src\itoa.c
	@cl $(CFLAGS) $(INCLUDEDIR) %s

farbzero.obj: ..\src\farbzero.c
	@cl $(CFLAGS) $(INCLUDEDIR) %s

memset.obj: ..\src\memset.c
	@cl $(CFLAGS) $(INCLUDEDIR) %s

printf.obj: ..\src\printf.c
	@cl @<<printf.rsp
/nologo /c /X /Zi /Gs /G2 /Zd /DFARDATA /AS /Zp4 /I..\..\inc /I..\..\inc\common /I..\..\inc\i86 /I..\..\inc\sun ..\src\printf.c
<<

putstr.obj: ..\src\putstr.c
	@cl $(CFLAGS) $(INCLUDEDIR) %s

strcspn.obj: ..\src\strcspn.c
	@cl $(CFLAGS) $(INCLUDEDIR) %s

strcpy.obj: ..\src\strcpy.c
	@cl $(CFLAGS) $(INCLUDEDIR) %s

strcmp.obj: ..\src\strcmp.c
	@cl $(CFLAGS) $(INCLUDEDIR) %s

strcat.obj: ..\src\strcat.c
	@cl $(CFLAGS) $(INCLUDEDIR) %s

strlen.obj: ..\src\strlen.c
	@cl $(CFLAGS) $(INCLUDEDIR) %s

strncmp.obj: ..\src\strncmp.c
	@cl $(CFLAGS) $(INCLUDEDIR) %s

strncat.obj: ..\src\strncat.c
	@cl $(CFLAGS) $(INCLUDEDIR) %s

testpt.obj: ..\src\testpt.c
	@cl $(CFLAGS) $(INCLUDEDIR) %s

strncpy.obj: ..\src\strncpy.c
	@cl $(CFLAGS) $(INCLUDEDIR) %s

xtoa.obj: ..\src\xtoa.c
	@cl $(CFLAGS) $(INCLUDEDIR) %s

clean:
	if exist *.obj del *.obj
	if exist *.cod del *.cod
	if exist *.lst del *.lst
	if exist *.bin del *.lib
	if exist *.pdb del *.pdb
