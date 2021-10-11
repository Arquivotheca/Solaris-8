#
#ident	"@(#)Makefile	1.31	00/09/14 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# ucbcmd/Makefile
#

# include global definitions; SRC should be defined in the shell.
# SRC is needed until RFE 1026993 is implemented.
include $(SRC)/Makefile.master
include $(SRC)/ucbcmd/Makefile.ucbcmd

COMMON_SUBDIRS=		\
	basename	\
	biff		\
	cc		\
	chown		\
	df		\
	du		\
	echo		\
	expr		\
	fastboot	\
	fasthalt	\
	file		\
	from		\
	groups		\
	install.d	\
	ld		\
	lint.d		\
	ln		\
	ls		\
	mkstr		\
	plot		\
	printenv	\
	ps		\
	rusage		\
	sed		\
	shutdown	\
	stty		\
	sum		\
	test		\
	touch		\
	tr		\
	tset		\
	ucblinks	\
	users		\
	vipw		\
	whereis		\
	whoami

sparc_SUBDIRS= sbcp

SUBDIRS= $(COMMON_SUBDIRS) $($(MACH)_SUBDIRS)

# commands messaged
MSGSUBDIRS = biff install.d mkstr rusage shutdown ucblinks

BWOSDIRS=

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
_msg:=          TARGET= _msg

# Since pre-5.0 system users were told to put /usr/5bin in their path to
# get the 5.0 system behavior, we need this link so that the PATH of
# /usr/5bin:/usr/ucb:/usr/bin does the right thing. (let's see, without
# this link, 5.0 behaves like UCB and 4.1 behaves like SysV... I'm so
# confused...)
ROOTUSR = $(ROOT)/usr
S5LINK = $(ROOTUSR)/5bin
DICTLINK = $(ROOTUSR)/dict

# Symbolic links for source compatibility.  Many utilities that
# were under /usr/ucb in SunOS 4.1, are now in the base;  the
# symbolic links are provided for compatibility.
LINKDEST=	../bin/$(@F)

$(CH) BINLINKS=		\
	arch		\
	clear		\
	e		\
	edit		\
	ex		\
	vedit		\
	vi		\
	view		\
	finger		\
	fmt		\
	fold		\
	ftp		\
	head		\
	hostid		\
	hostname	\
	logger		\
	mach		\
	more		\
	page		\
	pagesize	\
	netstat		\
	quota		\
	rcp		\
	rdate		\
	renice		\
	rlogin		\
	rsh		\
	ruptime		\
	rwho		\
	script		\
	tail		\
	talk		\
	tcopy		\
	telnet		\
	tftp		\
	uptime		\
	vacation	\
	vmstat		\
	w		\
	wc		\
	whois		\
	xstr

ROOTBINLINKS = $(BINLINKS:%=$(ROOTBIN)/%)

$(CH) OTHERLINKS = $(ROOTETC)/termcap \
		$(ROOTSHLIB)/zoneinfo/posixrules

$(S5LINK) :=		LINKDEST= bin
$(DICTLINK) :=		LINKDEST= share/lib/dict
$(ROOTBIN)/e :=		LINKDEST= ../bin/ex
$(ROOTBIN)/page :=	LINKDEST= ../bin/more
$(ROOTBIN)/quota :=	LINKDEST= ../lib/fs/ufs/quota
$(ROOTETC)/termcap :=	LINKDEST= ../usr/share/lib/termcap
$(ROOTSHLIB)/zoneinfo/posixrules := LINKDEST= US/Eastern

.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all clean clobber: $(SUBDIRS)

install: $(SUBDIRS) .WAIT rootlinks

# For messaging catalog
#
_msg:   $(MSGSUBDIRS)



bwos: $(BWOSDIRS) .WAIT $(SUBDIRS)

$(BWOSDIRS) $(SUBDIRS): FRC
	@if [ -f $@/Makefile  ]; then \
		cd $@; pwd; $(MAKE) $(TARGET); \
	 else \
		true; \
	 fi

rootlinks: $(S5LINK) $(DICTLINK) $(ROOTBINLINKS) $(OTHERLINKS)

$(S5LINK) $(DICTLINK) $(ROOTBINLINKS) $(OTHERLINKS):
	$(RM) $@; $(SYMLINK) $(LINKDEST) $@

FRC:
