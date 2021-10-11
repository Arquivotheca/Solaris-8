#
#ident	"@(#)Makefile	1.5	92/07/14 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/osadmin/csub/Makefile
#

LIBRARY = csub.a

OBJECTS = Chdir.o Execvp.o Fopen.o Fread.o Fstat.o Malloc.o Open.o Read.o \
	Write.o argfopen.o bufsplit.o errafter.o errbefore.o errtext.o \
	filename.o fopenck.o num.o numd.o p2open.o pgmname.o stddef.o

include ../../../lib/Makefile.lib

LIBNAME = $(LIBRARY:%.a=%) # for lint library target
CPPFLAGS = -I../include $(CPPFLAGS.master)

.KEEP_STATE:

all: $(TXTS) $(LIBS)

install: all
	@echo "$(LIBRARY) is not installed."

lint: $(LINTLIB)

include ../../../lib/Makefile.targ
