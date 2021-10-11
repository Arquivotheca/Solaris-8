#
#ident	"@(#)Makefile	1.11	00/09/14 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/vi/Makefile

SUBDIRS=	misc port
TXTS=		PRESERVE README 
POFILES=        $(SUBDIRS:%=%/%.po)

include ../Makefile.cmd
#
# For message catalogue file
#
POFILE= vi.po

OWNER=	root
GROUP=	sys

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
_msg:=          TARGET= catalog

# this directory is made in ../../Targetdirs
ROOTINITD= $(ROOTETC)/init.d

ROOTLINK= $(ROOTETC)/rc2.d/S80PRESERVE

ETCINITF= PRESERVE
ROOTETCINITF= $(ETCINITF:%=$(ROOTINITD)/%)
$(ROOTETCINITF):=	FILEMODE =	744

$(ROOTINITD)/% : %
	$(INS.file)

.KEEP_STATE:

all:	$(SUBDIRS)

install: $(ETCINITF) $(SUBDIRS) $(ROOTETCINITF) $(ROOTLINK)

$(ROOTLINK): $(ROOTETCINITF)
	-$(RM) $@
	-$(LN) -f $(ROOTETCINITF) $@

clean lint: $(SUBDIRS)

clobber: $(SUBDIRS)	clobber_local

clobber_local:
	$(RM) $(CLOBBERFILES)

#
# For message catalogue file
#
_msg: $(SUBDIRS)
	$(RM) $(POFILE)
	cat $(POFILES) > $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	cp $(POFILE) $(MSGDOMAIN)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
