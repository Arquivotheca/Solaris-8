#
#ident	"@(#)Makefile	1.2	99/10/09 SMI"

# zsh version
VERSION = 3.0.6

MAN1FILES= 	zsh.1 zshall.1 zshbuiltins.1 zshcompctl.1 zshexpn.1 zshmisc.1 \
		zshoptions.1 zshparam.1 zshzle.1

include ../Makefile.cmd

SUBDIRS=	Src

all     :=      TARGET = all
install :=      TARGET = install
clean   :=      TARGET = clean
clobber :=      TARGET = clobber
lint    :=      TARGET = lint

ROOTSRC= $(ROOT)/usr/share/src/zsh
DISTFILES:sh= cat ./distfiles
ROOTDISTFILES= $(DISTFILES:%=$(ROOTSRC)/%)
$(ROOTDISTFILES) := FILEMODE= 0444
$(ROOTSRC)/configure := FILEMODE= 0555
DISTDIRS= Doc Etc Functions Misc Src StartupFiles Util
ROOTSRCDIRS=   $(DISTDIRS:%=$(ROOTSRC)/%)

.KEEP_STATE:

.NO_PARALLEL: $(ROOTSRCDIRS)

all:	$(SUBDIRS)

clean clobber wait: $(SUBDIRS)

install: install_src .WAIT install_man $(SUBDIRS)

$(SUBDIRS):     FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

install_src: $(ROOTSRC) .WAIT $(ROOTSRCDIRS) .WAIT $(ROOTDISTFILES)

install_man: $(ROOTMAN1FILES)

$(ROOTSRC) $(ROOTSRCDIRS):
	$(INS.dir)

$(ROOTSRC)/%: %
	$(INS.file)

include ../Makefile.targ
