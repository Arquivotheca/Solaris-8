#
# Copyright (c) 1995,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.15	99/03/30 SMI"

PROG=		acctcms acctcom acctcon acctcon1 acctcon2 \
		acctdisk acctdusg acctmerg accton acctprc acctprc1 \
		acctprc2 acctwtmp closewtmp fwtmp \
		wtmpfix utmp2wtmp
SHFILE1=	acct chargefee ckpacct dodisk lastlogin\
		monacct nulladm prctmp prdaily prtacct \
		remove runacct shutacct startup turnacct
SHFILE2=	ptecms.awk ptelus.awk
SUBDIRS=	lib
ALL=		$(PROG) $(SHFILE1) $(SHFILE2) holidays

SRCS=		$(PROG:%=%.c)
SHFILE1SRCS=	$(SHFILE1:%=%.sh)

TXTS=		diskusg.c

BINPROG=	acctcom
LIBPROG=	acctcms acctcon acctcon1 acctcon2 acctdisk \
		acctdusg acctmerg accton acctprc acctprc1 acctprc2 \
		acctwtmp closewtmp fwtmp utmp2wtmp \
		wtmpfix chargefee ckpacct dodisk monacct \
		lastlogin nulladm prctmp prdaily prtacct \
		remove runacct shutacct startup turnacct \
		ptecms.awk ptelus.awk
ETCPROG=	holidays
INITPROG=	acct

include		../Makefile.cmd

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

acctcom := LDLIBS += lib/a.a
acctcms acctcon acctcon1 acctmerg acctprc1 acctprc := LDLIBS += lib/a.a


LIBACCTD=	$(ROOTLIB)/acct
ETCACCTD=	$(ROOTETC)/acct
ETCINITD=	$(ROOTETC)/init.d
VARADMD=	$(ROOT)/var/adm
ACCTDIR=	$(VARADMD)/acct
ACCTSUBDIRS=	$(ACCTDIR)/nite $(ACCTDIR)/fiscal $(ACCTDIR)/sum
WKDIRS=		$(ACCTDIR) $(ACCTSUBDIRS)

# DIRS is directories to create. $(ETCINITD) [aka: /etc/init.d] is created
# in /usr/src/Targetdirs and hence should be assumed to exist.
DIRS=		$(LIBACCTD) $(ETCACCTD) $(WKDIRS)

USRBINPROG=	$(BINPROG:%=$(ROOTBIN)/%)
LIBACCTPROG=	$(LIBPROG:%=$(LIBACCTD)/%)
ETCACCTPROG=	$(ETCPROG:%=$(ETCACCTD)/%)
ETCINITPROG=	$(INITPROG:%=$(ETCINITD)/%)

$(LIBACCTD) :=		DIRMODE=	755
$(ETCACCTD) :=		OWNER=		adm
$(ETCACCTD) :=		GROUP=		adm
$(ETCACCTD) :=		DIRMODE=	755
$(WKDIRS) :=		OWNER=		adm
$(WKDIRS) :=		GROUP=		adm
$(WKDIRS) :=		DIRMODE=	775
$(LIBACCTD)/accton:=	OWNER=		root
$(LIBACCTD)/accton:=	GROUP=		adm
$(LIBACCTD)/accton :=	FILEMODE=	04755
$(ETCINITPROG) :=	OWNER=		root
$(ETCINITPROG) :=	GROUP=		sys
$(ETCINITPROG) :=	FILEMODE=	0744
$(ETCACCTPROG) :=	FILEMODE=	0644

.KEEP_STATE:

.PARALLEL: $(ALL)

all: $(SUBDIRS) .WAIT $(ALL) $(TXTS)

install: all .WAIT $(DIRS) .WAIT $(USRBINPROG) $(LIBACCTPROG) $(ETCACCTPROG) \
	$(ETCINITPROG)

THIS_YEAR:sh=	date +%Y

holidays:	FRC
	@if grep $(THIS_YEAR) holidays > /dev/null 2>&1;\
	then \
		:;\
	else \
		$(ECHO) "building holidays";\
		( \
		$(ECHO) "* @(#)holidays\tJanuary 1, `date +%Y`";\
		$(ECHO) "*";\
		$(ECHO) "* Prime/Nonprime Table for UNIX Accounting System";\
		$(ECHO) "*";\
		$(ECHO) "* Curr\tPrime\tNon-Prime";\
		$(ECHO) "* Year\tStart\tStart";\
		$(ECHO) "*";\
		$(ECHO) "  `date +%Y`\t0800\t1800";\
		$(ECHO) "*";\
		$(ECHO) "* only the first column (month/day) is significiant.";\
		$(ECHO) "*";\
		$(ECHO) "* month/day\tCompany";\
		$(ECHO) "* \t\tHoliday";\
		$(ECHO) "*";\
		$(ECHO) "1/1\t\tNew Years Day";\
		$(ECHO) "7/4\t\tIndep. Day";\
		$(ECHO) "12/25\t\tChristmas" ) > holidays;\
	fi

$(DIRS):
	$(INS.dir)

$(LIBACCTD)/% : %
	$(INS.file)

$(ETCACCTD)/% : %
	$(INS.file)

$(ETCINITD)/% : %
	$(INS.file)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:

clean: $(SUBDIRS)

clobber: $(SUBDIRS)
	$(RM) $(PROG) $(SHFILE1) holidays

lint:	$(SUBDIRS)
	$(LINT.c) $(SRCS)
