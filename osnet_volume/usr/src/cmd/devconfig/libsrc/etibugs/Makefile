#
#ident @(#)Makefile	99/04/06	1.19 SMI
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
#	Makefile for maintaining usr/src/libsrc/etibugs
#

include ../Makefile.lib
#include ../../../../lib/Makefile.lib

LIBDESTDIR	= .


# Override POST_PROCESS_O as used in .c.o rule in Makefile.master so
# that we don't stamp each object, yet still retain the method of setting
# OBJS seen below.
#POST_PROCESS_O  =
#
# passed in from upper level makefile for -g or -O optimization 
OPTFLAG		= -g
MCSSTRING       = $(RELEASE_CM)

#INCDIR1         = $(INCDESTDIR)
#INCLUDES	= -I$(INCDIR1)
#
#SCCSGET		= sccs get

CFLAGS		= ${OPTFLAG} ${INCLUDES}
CCFLAGS		= ${OPTFLAG} ${INCLUDES}
LINTFLAGS	= ${INCLUDES}
LDFLAGS		= ${CFLAGS} ${LIBDIRS}

#MODE		= 664
#INSTALL		= $(INS)
#INSTALLFLAGS	= -s -m $(MODE)

###
### Instructions:
###
### Fill in PRIVHDRS, PUBHDRS, SRCS, and LIBRARY name to build the library.
### PRIVHDRS = library private include (.h) files.  These files will
### be left in the current directory, and used only by this library.
### PUBHDRS  = library interface definition (protos, typedefs) files.
### These files will be installed in INCDESTDIR, to be accessed by
### programs that link this library.

PRIVHDRS	= private.h utility.h

PUBHDRS		=

INSTPUBHDRS	= $(PUBHDRS:%.h=$(INCDESTDIR)/%.h)

SRCS		= chg_data.c fieldtype.c post.c

.PARALLEL: $(OBJS)

OBJS		= $(SRCS:.c=.o)

LIBRARY		= libetibugs.a

INSTLIBRARY	= $(LIBRARY:%.a=$(LIBDESTDIR)/%.a)


.KEEP_STATE:

all:		$(LIBRARY) .WAIT install

#all:		$(LIBRARY)

$(LIBRARY):	$(OBJS)
		$(AR) $(ARFLAGS) $@ $(OBJS)

#install:        tmp_dirs $(INSTLIBRARY) $(INSTPUBHDRS)

install:	$(INSTLIBRARY)

msgs strip:

sccs:
		$(SCCSGET) $(SCCSGETFLAGS) $(SRCS) $(PRIVHDRS) $(PUBHDRS)

clean:
		$(RM) $(OBJS) *~ Makefile.bak

clobber:        clean
		$(RM) $(LIBRARY) $(INSTLIBRARY) $(INSTPUBHDRS)

sclean:		clean
		sccs clean

lint:		$(SRCS) $(PRIVHDRS) $(PUBHDRS)
		$(LINT.c) $(SRCS)

cstyle:		$(SRCS) $(PRIVHDRS) $(PUBHDRS)
		cstsyle $(SRCS) $(PRIVHDRS) $(PUBHDRS)

# 'makedepend' requires /usr/openwin/bin in path.
depend:		$(SRCS) $(PRIVHDRS) $(PUBHDRS)
		makedepend $(INCLUDES) $(SRCS)

#
# Rules that make the installed library and includes depend on the
# files in the current directory, so that they don't get installed
# unless they are new.
#

$(LIBDESTDIR)/%.a:	%.a
	$(INSTALL) $(INSTALLFLAGS) -f $(LIBDESTDIR) $<

$(INCDESTDIR)/%.h:	%.h
	$(INSTALL) $(INSTALLFLAGS) -f $(INCDESTDIR) $<

include ../../../Makefile.targ
