#
#ident	"@(#)Makefile	1.2	99/10/09 SMI"

MAN1FILES=	bzip2.1
MAN3FILES=	libbz2.3

include ../Makefile.cmd

SUBDIRS=	spec .WAIT $(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

# definitions for install_h target
HDRS=           bzlib.h
ROOTHDRDIR=     $(ROOT)/usr/include
ROOTHDRS=       $(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=
$(ROOTHDRS) := FILEMODE = 0644

.KEEP_STATE:

all:	$(SUBDIRS)

clean clobber lint:	$(SUBDIRS)

install: install_h .WAIT install_src .WAIT install_man $(SUBDIRS)

install_man: $(ROOTMAN1FILES) $(ROOTMAN3FILES)

# install rule for install_h target
$(ROOTHDRDIR)/%: %
	$(INS.file)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

ROOTSRC= $(ROOT)/usr/share/src/bzip2
DISTFILES:sh= cat ./distfiles
ROOTDISTFILES= $(DISTFILES:%=$(ROOTSRC)/%)
$(ROOTDISTFILES) := FILEMODE= 0444

install_src: $(ROOTSRC) .WAIT $(ROOTDISTFILES)

install_h: $(ROOTHDRS)

$(ROOTSRC):
	$(INS.dir)

$(ROOTSRC)/%: %
	$(INS.file)

include ../Makefile.targ
