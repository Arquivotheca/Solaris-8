#
# Copyright (c) 1989-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.10	99/01/25 SMI"
#
# lib/libtnf/Makefile
#

LIBRARY=	libtnf.a
VERS=		.1
OBJECTS=	$(MACH)dep.o	\
		ref.o		\
		abi.o		\
		util.o		\
		info.o		\
		datum.o		\
		record.o	\
		scalar.o	\
		array.o		\
		struct.o	\
		type.o		\
		error.o		\
		access.o	\
		reader.o

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
check :=	TARGET= check
install :=	TARGET= install
lint :=		TARGET= lint
install_h :=	TARGET= install_h
_msg :=		TARGET= _msg


include ../Makefile.lib

TEXT_DOMAIN=	SUNW_OST_OSLIB
XGETFLAGS=	-a
POFILE=		$(LIBRARY:%.a=%.po)
POFILES=	_error.po

SED=	sed
GREP=	grep
CP=	cp

LIBS=		$(DYNLIB)

MAPDIR=		./spec/$(MACH)
MAPFILE=	$(MAPDIR)/mapfile
MAPOPTS=	$(MAPFILE:%=-M %)
DYNFLAGS +=	$(MAPOPTS)
CLOBBERFILES +=	$(MAPFILE)

LDLIBS += -lc

HDRS=		tnf.h
ROOTHDRDIR=	$(ROOT)/usr/include/tnf
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)
$(ROOTHDRS) := 	FILEMODE = 0644
CHECKHDRS =	$(HDRS:%.h=%.check)

$(ROOTHDRS) :=	FILEMODE = 644

CPPFLAGS +=	-D_DATUM_MACROS -D_REENTRANT

LINTFLAGS +=	-y

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile


.KEEP_STATE:

all: spec .WAIT $(LIBS)

install: all spec $(ROOTLIBDIR) $(ROOTLIBS) $(ROOTLINKS)

install_h: $(ROOTHDRDIR) $(ROOTHDRS)

clean clobber: spec $(BUILD64)

lint:
	$(LINT.c) $(SRCS)

check: $(CHECKHDRS)

$(ROOTLIBDIR) $(ROOTHDRDIR):
	$(INS.dir)

$(ROOTHDRDIR)/% : %
	$(INS.file)

ASFLAGS=	-K pic -P -D_SYS_SYS_S -D_LOCORE -D_ASM -DPIC -DLOCORE $(CPPFLAGS)
BUILD.s=	$(AS) $(ASFLAGS) $< -o $@

pics/%.o:	./%.s
		$(BUILD.s)
		$(POST_PROCESS_O)

objs/%.o:	./%.s
		$(BUILD.s)
		$(POST_PROCESS_O)

_msg:	$(MSGDOMAIN) $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(POFILE): $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

$(POFILES):
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext *.[ch]*`
	$(SED) -e '/^# msg/d' -e '/^domain/d' messages.po > $@
	$(RM) messages.po

include ../Makefile.targ

spec:	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
