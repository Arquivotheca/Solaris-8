#
#ident	"@(#)Makefile	1.6	98/01/05 SMI"
#

# Copyright (c) 1997 by Sun Microsystems, Inc.
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

LINTFLAGS +=

all: $(LIBDEMANGLE)
install: all

$(LIBDEMANGLE):	$(OBJS)
	ar -cr $(LIBDEMANGLE)	$(OBJS)

cafe_dem.o: ../common/cafe_dem.y ../common/cafe_dem.h ../common/dem.h
	yacc ../common/cafe_dem.y
	$(CC) -I../common -c y.tab.c -o cafe_dem.o

dem.o: ../common/dem.c
	$(CC) -I../common -c ../common/dem.c -o dem.o

demangle.o: ../common/demangle.c
	$(CC) -I../common -c ../common/demangle.c -o demangle.o

lint:		$(LINTOUT)

$(LINTOUT):	../common/llib-ldemangle
		$(LINT.c) -odemangle ../common/llib-ldemangle > $(LINTOUT) 2>&1


include $(SRC)/lib/Makefile.targ
