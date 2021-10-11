#
#ident	"@(#)Makefile	1.1	98/10/09 SMI"
#

# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.

include $(SRC)/lib/Makefile.lib
include $(SRC)/cmd/sgs/Makefile.com

CLEANFILES += y.tab.c y.tab.h llib-ldemangle.ln lint.out

SRCATCOMMON= ../common/cafe_dem.y ../common/dem.c

COMMONSRC= ../common/demangle.c

LIBDEMANGLE= libdemangle.a

OBJFROMCOMMON= cafe_dem.o dem.o
OBJCOMMON= demangle.o
OBJS= $(OBJFROMCOMMON) $(OBJCOMMON)

LINTFLAGS64= -mu -D__sparcv9 -errchk=longptr64

WARNFLAGS= -erroff=E_STATEMENT_NOT_REACHED

all: $(LIBDEMANGLE)
install: all

$(LIBDEMANGLE):	$(OBJS)
	ar -cr $(LIBDEMANGLE)	$(OBJS)

cafe_dem.o: ../common/cafe_dem.y ../common/cafe_dem.h ../common/dem.h
	yacc ../common/cafe_dem.y
	$(COMPILE.c) -I../common -c y.tab.c -o cafe_dem.o $(WARNFLAGS)

dem.o: ../common/dem.c
	$(COMPILE.c) -I../common -c ../common/dem.c -o dem.o $(WARNFLAGS)

demangle.o: ../common/demangle.c
	$(COMPILE.c) -I../common -c ../common/demangle.c -o demangle.o

lint:		$(LINTOUT)

$(LINTOUT):	../common/llib-ldemangle
		$(LINT.c) -odemangle ../common/llib-ldemangle > $(LINTOUT) 2>&1

include $(SRC)/lib/Makefile.targ
include ../../Makefile.sub.64
