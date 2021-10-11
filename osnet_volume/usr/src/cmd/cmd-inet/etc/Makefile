#
#ident	"@(#)Makefile	1.17	99/08/29 SMI"
#
# Copyright (c) 1989,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#

SYMPROG=	hosts inetd.conf networks protocols \
	services netmasks sock2path

# New /etc/inet files shouldn't have /etc entries.
PROG= ipnodes datemsk.ndpd ipsecinit.sample
ETCPROG= $(SYMPROG) $(PROG)
SUBDIRS= dhcp init.d nca
TXTS= hosts.equiv named.boot

include ../../Makefile.cmd

all:=		TARGET= all
install:=	TARGET= install

ROOTVAR=	$(ROOT)/var
INETETCDIR=	$(ROOTETC)/inet
INETVARDIR=	$(ROOTVAR)/inet
DIRS= $(INETETCDIR) $(INETVARDIR)
SYMDIR= inet
ETCINETPROG=	$(ETCPROG:%=$(INETETCDIR)/%)
# Only old /etc/inet files get symlinks in /etc.
SYMETCPROG=	$(SYMPROG:%=sym_%)

FILEMODE= 0444
OWNER= root
GROUP= sys

.KEEP_STATE:

all: $(ETCPROG) $(TXTS) $(SUBDIRS)

install: all $(DIRS) $(ETCINETPROG) $(SYMETCPROG) $(SUBDIRS)

$(INETETCDIR)/% : %
	$(INS.file)

sym_% : %
	$(RM) $(ROOTETC)/$<
	$(SYMLINK) $(SYMDIR)/$< $(ROOTETC)/$<

$(DIRS):
	$(INS.dir)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:

# datemsk.ndpd is generated from datemsk.template because of a side-effect of
# SCCS.  Some of the datemsk.ndpd format strings include "%<letter>%", which
# SCCS confuses for ID keywords.  datemsk.template should quote the "%"
# with "\" and code below will filter out the "\".  Only datemsk.ndpd format
# strings next to each other need to be quoted.

datemsk.ndpd: datemsk.template
	@while read i; do echo $$i; done < datemsk.template > $@

clean clobber:
	$(RM) datemsk.ndpd

lint:
