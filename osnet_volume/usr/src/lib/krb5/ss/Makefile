#
# Copyright (c) 1997-1999, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident   "@(#)Makefile 1.1     99/07/18 SMI"
#
# lib/krb5/ss/Makefile
#
LIBRARY= libss.a
VERS = .1
MAPDIR= spec/$(MACH)

SUBDIRS= spec

SSOBJS= \
	std_rqs.o \
	invocation.o help.o \
	execute_cmd.o listen.o parse.o error.o prompt.o \
	request_tbl.o list_rqs.o pager.o requests.o \
	data.o

OBJECTS= $(SSOBJS)

# libss build rules
objs/%.o profs/%.o pics/%.o: %.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

# include library definitions
include ../Makefile.lib

# Override the value of ROOTLIBDIR set in Makefile.lib since we want
# to install the library in $ROOT/usr/lib/krb5
ROOTLIBDIR = $(ROOT)/usr/lib/krb5

include $(SRC)/lib/gss_mechs/mech_krb5/Makefile.mech_krb5

#override INS.liblink
INS.liblink=	-$(RM) $@; $(SYMLINK) $(LIBLINKPATH)$(LIBLINKS)$(VERS) $@

CPPFLAGS +=     -DHAVE_LIBSOCKET=1 -DHAVE_LIBNSL=1 -DHAS_STRDUP=1 \
		-DUSE_DIRENT_H=1 -DWAIT_USES_INT=1 -DPOSIX_SIGNALS=1 \
		-DUSE_SIGPROCMASK=1 -DRETSIGTYPE=void \
		-DHAVE_STDARG_H=1 -DHAVE_STDLIB_H=1 -DHAVE_COMPILE=1 \
		-I$(SRC)/lib/gss_mechs/mech_krb5/include \
		-I$(SRC)/lib

$(PICS) := 	CFLAGS += -xF
$(PICS) := 	CCFLAGS += -xF

MAPFILE= $(MAPDIR)/mapfile
CLOBBERFILES += $(MAPFILE)

DYNFLAGS += -M $(MAPFILE)
LIBS  = $(DYNLIB)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

LDLIBS += -R $(KRUNPATH) $(KMECHLIB) -lnsl -lmp -lc -ldl

TEXT_DOMAIN = SUNW_OST_OSLIB
POFILE = $(LIBRARY:%.a=%.po)
POFILES = generic.po

.KEEP_STATE:

all :=          TARGET= all
clean :=        TARGET= clean
clobber :=      TARGET= clobber
delete :=       TARGET= delete
install :=      TARGET= install
lint :=         TARGET= lint
_msg :=         TARGET= _msg
package :=      TARGET= package

all: $(SUBDIRS) .WAIT $(DYNLIB)

clean clobber: $(SUBDIRS)

$(ROOTLIBDIR):
	$(INS.dir)

install: $(SUBDIRS) $(ROOTLIBDIR) $(ROOTLIBS) $(ROOTLINKS)

spec $(MACH) $(MACH64):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

check:	$(CHECKHDRS)

$(DYNLIB):	$(MAPFILE)

# include library targets
include ../../Makefile.targ

lint: $(SUBDIRS) .WAIT $(LINTLIB)

FRC:

_msg: $(MSGDOMAIN) .WAIT $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(POFILE): $(DERIVED_FILES) .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

generic.po: FRC
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext *.[ch]`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po

$(MSGDOMAIN):
	$(INS.dir)

