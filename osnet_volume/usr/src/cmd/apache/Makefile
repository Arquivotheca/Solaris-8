#
#ident "@(#)Makefile	1.4	99/11/10 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#
include ../Makefile.cmd
include Makefile.paths

SFWMAKEFILE=Makefile.sfw

SUBDIRS= src conf htdocs cgi-bin icons mod_perl

TOPDIRS= $(PREFIX) $(DATADIR) $(SYSCONFTOP)
ROOTTOPDIRS=$(TOPDIRS:%=$(ROOT)/%)

ROOTSRC= $(ROOT)/usr/share/src/apache
DISTFILES:sh= cat ./distfiles
EXFILES:sh= cat ./exfiles
ROOTDISTFILES= $(DISTFILES:%=$(ROOTSRC)/%)
ROOTEXFILES= $(EXFILES:%=$(ROOTSRC)/%)
DISTDIRS= src src/ap src/helpers src/include src/lib src/lib/expat-lite \
	  src/main src/modules src/modules/example src/modules/experimental \
	  src/modules/extra src/modules/proxy src/modules/standard \
	  src/os src/os/os2 src/os/bs2000 src/os/tpf src/os/tpf/samples \
	  src/os/unix src/os/win32 src/os/win32/installer \
	  src/os/win32/installer/installdll \
	  src/os/win32/installer/installdll/test src/regex \
	  src/support src/support/SHA1 cgi-bin conf htdocs htdocs/manual \
	  htdocs/manual/mod htdocs/manual/images htdocs/manual/misc \
	  htdocs/manual/search htdocs/manual/vhosts icons icons/small \
	  logs mod_perl mod_perl/t mod_perl/t/docs mod_perl/t/docs/auth \
	  mod_perl/t/docs/STAGE mod_perl/t/docs/STAGE/u1 \
	  mod_perl/t/docs/dirmagic mod_perl/t/docs/subr mod_perl/t/net \
	  mod_perl/t/net/perl mod_perl/t/net/perl/io mod_perl/t/net/perl/noenv \
	  mod_perl/t/internal mod_perl/t/modules mod_perl/t/conf \
	  mod_perl/t/TestDirectives mod_perl/lib mod_perl/lib/Apache \
	  mod_perl/lib/Apache/Constants mod_perl/lib/Bundle mod_perl/apaci \
	  mod_perl/src mod_perl/src/modules mod_perl/src/modules/perl \
	  mod_perl/src/modules/ApacheModulePerl mod_perl/Constants \
	  mod_perl/Table mod_perl/Util mod_perl/File mod_perl/Apache \
	  mod_perl/Connection mod_perl/htdocs mod_perl/htdocs/manual \
	  mod_perl/htdocs/manual/mod mod_perl/eg mod_perl/Log mod_perl/Server \
	  mod_perl/URI mod_perl/faq mod_perl/Symbol mod_perl/Leak \
	  mod_perl/ModuleConfig mod_perl/benchmark mod_perl/PerlRunXS

ROOTSRCDIRS=$(DISTDIRS:%=$(ROOTSRC)/%)
$(ROOTDISTFILES) := FILEMODE= 0444

all:=		TARGET=all
install:=	TARGET=install
clean:=		TARGET=clean
clobber:=	TARGET=clobber
lint:=		TARGET=lint

.KEEP_STATE:

.NO_PARALLEL: $(ROOTSRCDIRS)

all clean clobber lint: $(SUBDIRS)

install: $(ROOTTOPDIRS) .WAIT $(ROOTHTTPDPROXYCACHEDIR) $(SUBDIRS) \
	 install_src

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) -f $(SFWMAKEFILE) $(TARGET)

$(ROOTTOPDIRS):=	OWNER=root
$(ROOTTOPDIRS):=	GROUP=bin
$(ROOTTOPDIRS):=	FILEMODE=0755

$(ROOTHTTPDPROXYCACHEDIR):=	OWNER=nobody
$(ROOTHTTPDPROXYCACHEDIR):=	GROUP=nobody

$(ROOTTOPDIRS) $(ROOTHTTPDPROXYCACHEDIR):
	$(INS.dir)

$(ROOTSRC) $(ROOTSRCDIRS):
	$(INS.dir)

install_src: $(ROOTSRC) .WAIT $(ROOTSRCDIRS) .WAIT $(ROOTDISTFILES)

CSCOPE	= cscope-fast
CSDIR	= src
CSDIRS	= main os/unix modules/proxy modules/standard modules/perl ap support
CSDIRS  += lib/expat-lite
CSPATHS	= $(CSDIRS:%=$(CSDIR)/%)
ROOTHDRDIR = $(ROOT)/usr/include
CSINCS	= $(ROOTHDRDIR:%=-I%) $(CSPATHS:%=-I%)

.PRECIOUS: cscope.out

cscope.out: cscope.files FRC
	$(CSCOPE) -b -f `pwd`/cscope.out

cscope.files: FRC
	@-$(RM) cscope.files
	echo "$(CSINCS)" > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
		-type d -name '.del-*' -prune -o -type f \
		\( -name '*.[ch]' -o -name 'Makefile*' \) \
		-print >> cscope.files
	@wc -l cscope.files

.SUFFIXES:

$(ROOTEXFILES):=	FILEMODE=555

$(ROOTSRC)/%: %
	$(INS.file)

FRC:
