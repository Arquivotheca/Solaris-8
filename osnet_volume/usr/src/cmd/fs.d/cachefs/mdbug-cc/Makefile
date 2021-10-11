#
#ident	"@(#)Makefile	1.1	94/10/28 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
# cmd/fs.d/cachefs/mdbug
#

FSTYPE=		cachefs

include		../../Makefile.fstype

PROGOBJS = flist.o dbug.o

include		../Makefile.cachefs

ARFLAGS=	rc
CLOBBERFILES += $(LIB)
CPPFLAGS= -DDBUG_UNIX=1
CCFLAGS += -mt
LIB = libdbug.a

all : $(LIB)

$(LIB) : $(OBJS)
	$(RM) -f $(LIB)
	$(AR) $(ARFLAGS) $(LIB) $(OBJS)
