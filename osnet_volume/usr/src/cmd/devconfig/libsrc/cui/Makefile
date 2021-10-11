#
#ident @(#)Makefile	1.33	99/04/06 SMI
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
#	usr/src/cmd/devconfig/libsrc/cui/Makefile
#

include ../Makefile.lib
#include ../../../../lib/Makefile.lib

LIBDESTDIR	=	.


# Override POST_PROCESS_O as used in .cc.o rule in Makefile.master so
# that we don't stamp each object, yet still retain the method of setting
# OBJS seen below.
#POST_PROCESS_O  =

# passed in from upper level makefile for -g or -O optimization 
OPTFLAG		= -g
CPOPTFLAG	= -g
MCSSTRING       = $(RELEASE_CM)

#INCDIR1         = $(INCDESTDIR)
#INCLUDES	= -I$(INCDIR1)

#SCCSGET		= sccs get

CCFLAGS		= $($(MACH)_CCFLAGS) ${CPOPTFLAG} ${INCLUDES} -DSVR4 -DVOID_MALLOC
LINTFLAGS	= ${INCLUDES}
LDFLAGS		= ${CCFLAGS} ${LIBDIRS}

MODE		= 664
INSTALL		= $(INS)
INSTALLFLAGS	= -s -m $(MODE)

###
### Instructions:
###
### Fill in PRIVHDRS, PUBHDRS, SRCS, and LIBRARY name to build the library.
### PRIVHDRS = library private include (.h) files.  These files will
### be left in the current directory, and used only by this library.
### PUBHDRS  = library interface definition (protos, typedefs) files.
### These files will be installed in INCDESTDIR, to be accessed by
### programs that link this library.

PRIVHDRS =	abbrev.h app.h area.h \
		basewin.h button.h \
		caption.h color.h composite.h control.h \
		cuiall.h cuilib.h cuimenu.h cuiproto.h cursvmem.h \
		display.h \
		emanager.h exclusive.h \
		flags.h footer.h \
		gauge.h \
		helpwin.h hyper.h \
		item.h \
		keybd.h \
		list.h litem.h \
		mbutton.h memconf.h message.h mitem.h \
		notice.h \
		popwin.h precomp.h private.h \
		restab.h \
		separator.h shell.h spothelp.h strtab.h symtab.h \
		syscurses.h \
		text.h topic.h txtpanel.h \
		utility.h \
		vcontrol.h \
		widget.h window.h \
		xbutton.h

PUBHDRS =	cui.h cuitypes.h memcheck.h stringid.h

INSTPUBHDRS =	$(PUBHDRS:%.h=$(INCDESTDIR)/%.h)

SRCS =		abbrev.cc api.cc app.cc area.cc array.cc \
		basewin.cc border.cc button.cc \
		callbacks.cc caption.cc checkhelp.cc color.cc composite.cc \
		control.cc cuicomp.cc cuilib.cc \
		display.cc \
		emanager.cc error.cc exclusive.cc \
		footer.cc \
		gauge.cc \
		helpwin.cc hyper.cc \
		item.cc \
		keybd.cc \
		list.cc litem.cc \
		mbutton.cc memcheck.cc memory.cc menu.cc mitem.cc \
		names.cc notice.cc \
		popwin.cc \
		resource.cc restab.cc \
		separator.cc shell.cc spothelp.cc stringid.cc strings.cc \
		strtab.cc symtab.cc \
		text.cc topic.cc trace.cc txtpanel.cc \
		usrfield.cc \
		vcontrol.cc \
		widget.cc window.cc winio.cc \
		xbutton.cc

.PARALLEL: $(OBJS)

OBJS		= $(SRCS:.cc=.o)

LIBRARY         = libcui.a

CUICOMP		= cuicomp
CUI		= cui
INSTLIBRARY	= $(LIBRARY:%.a=$(LIBDESTDIR)/%.a)
INSTCUICOMP	= $(CUICOMP:%=$(LIBDESTDIR)/%)

.KEEP_STATE:

all:		$(LIBRARY) .WAIT install

#all:		$(LIBRARY)

$(LIBRARY):	$(OBJS)
		$(AR) $(ARFLAGS) $@ $(OBJS)

#install:        tmp_dirs $(INSTLIBRARY) $(INSTPUBHDRS) $(INSTCUICOMP) $(CUI)

#install:        $(INSTLIBRARY) $(CUI)

install:        $(CUI)


#install:

strip:

sccs:
		$(SCCSGET) $(SCCSGETFLAGS) $(SRCS) $(PRIVHDRS) $(PUBHDRS)

clean:
		$(RM) $(OBJS) $(CUI).o $(CUICOMP) $(CUI) core \
		      *~ Makefile.bak

clobber:        clean
		$(RM) $(LIBRARY) $(INSTLIBRARY) $(INSTPUBHDRS) $(INSTCUICOMP)

lint:		$(SRCS) $(PRIVHDRS) $(PUBHDRS)
		$(LINT.c) $(SRCS)

cstyle:		$(SRCS) $(PRIVHDRS) $(PUBHDRS)
		cstsyle $(SRCS) $(PRIVHDRS) $(PUBHDRS)

$(CUICOMP): $(CUICOMP).o $(LIBRARY)
	$(LINK.cc) -o $(CUICOMP) $(CUICOMP).o -L. -lcui -lpanel -lmenu -lform -lcurses

$(CUI): $(CUI).o $(LIBRARY)
	$(LINK.cc) -o $(CUI) $(CUI).o -L. -lcui -lpanel -lmenu -lform -lcurses

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

$(LIBDESTDIR)/$(CUICOMP):	$(CUICOMP)
	$(INSTALL) -s -m 755 -f $(LIBDESTDIR) $(CUICOMP)

$(INCDESTDIR)/%.h:	%.h
	$(INSTALL) $(INSTALLFLAGS) -f $(INCDESTDIR) $<

.cc.o:
	${CCC} -c ${CCFLAGS} $< -o $*.o

TEXT_DOMAIN=SUNW_INSTALL_CUILIB

xgettext:	$(TEXT_DOMAIN).po

msgs: $(TEXT_DOMAIN).po
	@mv $(TEXT_DOMAIN).po ../messages

$(TEXT_DOMAIN).po:	
	@xgettext -s -c "i18n:" -m "" $(SRCS)
	@sed	-e "/^# msg/d" \
		-e "/^domain[ 	][ 	]*\"messages\"/d" \
		-e "s?^#[ 	][ 	]*[/ ]\*/*?#?" \
		-e "s?[ 	][ 	]*\*/??" \
		messages.po > $(TEXT_DOMAIN).po.sed
	@mv $(TEXT_DOMAIN).po.sed $(TEXT_DOMAIN).po
	@rm -f messages.po

LOCALE_DIR:sh =echo /usr/lib/locale/$LOCALE/LC_MESSAGES

mtest: $(PROGRAM) msgs
	cp ../messages/$(TEXT_DOMAIN).po .
	pig	$(TEXT_DOMAIN).po
	cp $(TEXT_DOMAIN).po msgfile.saved
	msgfmt -o $(TEXT_DOMAIN).mo $(TEXT_DOMAIN).po
	mv $(TEXT_DOMAIN).mo $(LOCALE_DIR)
	rm $(TEXT_DOMAIN).po
