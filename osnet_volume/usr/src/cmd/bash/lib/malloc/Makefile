#ident	"@(#)Makefile	1.1	99/10/08 SMI"
#
# Generated automatically from Makefile.in by configure.
# Skeleton Makefile for the GNU malloc code
#

srcdir = .
VPATH = .:.
topdir = ../..
BUILD_DIR = ../..

include ../../../Makefile.cmd

PROFILE_FLAGS = 
LOCAL_CFLAGS = 
CFLAGS += -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
LDFLAGS =  

DEFS = -DHAVE_CONFIG_H
LOCAL_DEFS = -DSHELL

INCLUDES = -I. -I../.. -I$(topdir) -I$(topdir)/lib

CFLAGS += ${PROFILE_FLAGS} ${INCLUDES} $(DEFS) $(LOCAL_DEFS) $(LOCAL_CFLAGS) \
	  $(MALLOC_CFLAGS)

.KEEP_STATE:

MALLOC_SOURCE = malloc.c
GMALLOC_SOURCE = gmalloc.c
NMALLOC_SOURCE = nmalloc.c
STUB_SOURCE = stub.c

ALLOCA_SOURCE = alloca.c
ALLOCA_OBJECT = alloca.o

MALLOC_SRC = malloc.c
MALLOC = @MALLOC@
ALLOCA = 

MALLOC_OBJS = malloc.o $(ALLOCA) stub.o
GMALLOC_OBJS = gmalloc.o $(ALLOCA) stub.o
NMALLOC_OBJS = nmalloc.o $(ALLOCA) stub.o
NMALLOC2_OBJS = nmalloc2.o $(ALLOCA) stub.o
NGMALLOC_OBJS = ngmalloc.o $(ALLOCA) stub.o
STUB_OBJS = $(ALLOCA) stub.o

all:		malloc

malloc: ${MALLOC_OBJS}
	${RM} libmalloc.a
	${AR} ${ARFLAGS} libmalloc.a ${MALLOC_OBJS}

nmalloc: ${NMALLOC_OBJS}
	${RM} libmalloc.a
	${AR} ${ARFLAGS} libmalloc.a ${NMALLOC_OBJS}

nmalloc2: ${NMALLOC2_OBJS}
	${RM} libmalloc.a
	${AR} ${ARFLAGS} libmalloc.a ${NMALLOC2_OBJS}

gmalloc: ${GMALLOC_OBJS}
	${RM} libmalloc.a
	${AR} ${ARFLAGS} libmalloc.a ${GMALLOC_OBJS}

ngmalloc: ${NGMALLOC_OBJS}
	${RM} libmalloc.a
	${AR} ${ARFLAGS} libmalloc.a ${NGMALLOC_OBJS}

stubmalloc: ${STUB_OBJS}
	${RM} libmalloc.a
	${AR} ${ARFLAGS} libmalloc.a ${STUB_OBJS}

alloca.o:	$(srcdir)/$(ALLOCA_SOURCE)
	$(CC) $(CFLAGS) -c $(srcdir)/$(ALLOCA_SOURCE)
	@- if test "$(ALLOCA_OBJECT)" != alloca.o ; then \
		mv $(ALLOCA_OBJECT) alloca.o >/dev/null 2>&1 ; \
	fi

mostlyclean clean:
	$(RM) *.o libmalloc.a

distclean realclean maintainer-clean: clean
	$(RM) Makefile

alloca.o: $(BUILD_DIR)/config.h
malloc.o: $(BUILD_DIR)/config.h $(topdir)/bashtypes.h getpagesize.h
nmalloc.o: $(BUILD_DIR)/config.h $(topdir)/bashtypes.h getpagesize.h
nmalloc2.o: $(BUILD_DIR)/config.h $(topdir)/bashtypes.h getpagesize.h
xmalloc.o: $(BUILD_DIR)/config.h $(topdir)/ansi_stdlib.h
gmalloc.o: $(BUILD_DIR)/config.h

# Rules for deficient makes, like SunOS and Solaris
stub.o: stub.c
malloc.o: malloc.c
gmalloc.o: gmalloc.c

include ../../../Makefile.targ
