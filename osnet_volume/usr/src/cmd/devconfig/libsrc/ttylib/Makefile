#
#ident @(#)Makefile	99/10/10	1.10 SMI
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

include ../Makefile.lib

LIBDESTDIR	= .


MODE		= 664
INSTALL		= $(INS)
INSTALLFLAGS	= -s -m $(MODE)

MKMSGS		= ../../mkmsgs/mkmsgs

PRIVHDRS =	tty_color.h

PUBHDRS =	tty_utils.h tty_form.h tty_help.h tty_formtypes.h

DERIVED_SRCS =	ttylib_msgs.c
DERIVED_HDRS =	ttylib_msgs.h
DERIVED_FILES =	$(DERIVED_SRCS) $(DERIVED_HDRS)

SRCS =		tty_color.c tty_form.c tty_intro.c tty_help.c \
		tty_menu.c tty_utils.c tty_format.c 

.PARALLEL: $(OBJS)

DERIVED_OBJS	= $(DERIVED_SRCS:.c=.o)
OBJS		= $(SRCS:.c=.o)
OBJS		+= $(DERIVED_OBJS)

LIBRARY		= libtty.a


.KEEP_STATE:


all:		$(DERIVED_SRCS) .WAIT msgs .WAIT $(LIBRARY)

install:	all

$(LIBRARY):     $(OBJS)
		$(AR) $(ARFLAGS) $@ $(OBJS)

clean:		msgclean
		$(RM) $(OBJS) core *~ Makefile.bak

clobber:        clean
		$(RM) $(LIBRARY) $(LIBRARY) $(DERIVED_FILES)

lint:		$(DERIVED_SRCS) $(SRCS) $(PRIVHDRS) $(PUBHDRS)
		$(LINT.c) $(DERIVED_SRCS) $(SRCS)

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

TEXT_DOMAIN=SUNW_INSTALL_TTYLIB

#
# Requires that ttylib_msgs.[ch] target be built first
#
msgs:
	cp $(TEXT_DOMAIN).po ../messages

#
# This rule has the effect of building 3 objects: ttylib_msgs.c,
# ttylib_msgs.h and $(TEXT_DOMAIN).po.  Rather than detailing
# all three and building the entire set 3 times, we need only
# specify one.
#
ttylib_msgs.c:	msg_master
	@echo "Generating .po file and message sources from msg_master"
	$(MKMSGS) -d $(TEXT_DOMAIN) < msg_master

make-mo:
	sccs get make-mo
	chmod +x ./make-mo

mofile: $(TEXT_DOMAIN).po make-mo
	@echo "Making dummy-de .mo file"
	@./make-mo de
	@msgfmt -o $(TEXT_DOMAIN).mo de.po
	@rm -f de.[pm]o

msgclean:
	rm -f $(DERIVED_FILES) $(TEXT_DOMAIN).po 

.WAIT:

include ../../../Makefile.targ

