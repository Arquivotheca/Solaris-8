#
#ident @(#)Makefile	1.29	99/04/16 SMI
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#	Makefile for maintaining usr/src/libsrc/dvc
#

include ../Makefile.lib

LIBDESTDIR	= .


MODE		= 664
INSTALL		= $(INS)
INSTALLFLAGS	= -s -m $(MODE)

MKMSGS		= ../../mkmsgs/mkmsgs


###
### Instructions:
###
### Fill in PRIVHDRS, PUBHDRS, SRCS, and LIBRARY name to build the library.
### PRIVHDRS = library private include (.h) files.  These files will
### be left in the current directory, and used only by this library.
### PUBHDRS  = library interface definition (protos, typedefs) files.
### These files will be installed in INCDESTDIR, to be accessed by
### programs that link this library.

PRIVHDRS =	conf.h dev.h util.h win.h

PUBHDRS =	dvc.h

DERIVED_HDRS=	dvc_msgs.h

SRCS =		chk.c cat.c conf.c conf_attr.c conf_file.c \
		conf_parse.c dev.c dev_ops.c devi.c \
		mod.c pmi.c typ.c util.c win.c win_ops.c xmalloc.c 

DERIVED_SRCS =	dvc_msgs.c

DERIVED_FILES =	$(DERIVED_SRCS) $(DERIVED_HDRS)
DERIVED_FILES += $(DERIVED_SRCS:.c=.o)

OBJS		= $(SRCS:.c=.o)
OBJS		+= $(DERIVED_SRCS:.c=.o)

LIBRARY		= libdvc.a


.KEEP_STATE:

all:		$(DERIVED_SRCS) .WAIT msgs .WAIT $(LIBRARY)

install:	all

$(LIBRARY):     $(OBJS)
		$(AR) $(ARFLAGS) $@ $(OBJS)

clean:		msgclean
		$(RM) $(OBJS) core *~ Makefile.bak

clobber:        clean
		$(RM) $(LIBRARY) $(DERIVED_FILES)

lint:		$(DERIVED_SRCS) $(SRCS) $(PRIVHDRS) $(PUBHDRS) $(DERIVED_HDRS)
		$(LINT.c) $(DERIVED_SRCS) $(SRCS) $(DERIVED_HDRS)

cstyle:		$(SRCS) $(PRIVHDRS) $(PUBHDRS)
		cstsyle $(SRCS) $(PRIVHDRS) $(PUBHDRS)


#
# Rules that make the installed library and includes depend on the
# files in the current directory, so that they don't get installed
# unless they are new.
#

$(LIBDESTDIR)/%.a:	%.a
	$(INSTALL) $(INSTALLFLAGS) -f $(LIBDESTDIR) $<

$(INCDESTDIR)/%.h:	%.h
	$(INSTALL) $(INSTALLFLAGS) -f $(INCDESTDIR) $<

TEXT_DOMAIN=SUNW_INSTALL_DVC

#
# Requires that dvc_msgs.[ch] target be built first
#
msgs:
	cp $(TEXT_DOMAIN).po ../messages


#
# This rule has the effect of building 3 objects: dvc_msgs.c,
# dvc_msgs.h and $(TEXT_DOMAIN).po.  Rather than detailing
# all three and building the entire set 3 times, we need
# only specify one.
#
dvc_msgs.c:	msg_master
	@echo "${LEVEL}" 'Generating .po file and dvc_msgs.[ch] from msg_master'
	$(MKMSGS) -d $(TEXT_DOMAIN) < msg_master

mofile: $(TEXT_DOMAIN).po make-mo
	@echo "${LEVEL}" 'Making dummy-de .mo file'
	@./make-mo de
	@msgfmt -o $(TEXT_DOMAIN).mo de.po
	@rm -f de.[pm]o

msgclean:
	rm -f dvc_msgs.[ch] $(TEXT_DOMAIN).po

.WAIT:

