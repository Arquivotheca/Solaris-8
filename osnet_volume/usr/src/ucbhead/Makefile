#
#ident	"@(#)Makefile	1.10	94/04/01 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# ucbhead/Makefile
#
# include global definitions
include ../Makefile.master

LN= ln

HDRS=	\
a.out.h     assert.h    curses.h    dbm.h       \
regexp.h    resolv.h    setjmp.h    sgtty.h  \
signal.h    stdio.h     strings.h   struct.h    syscall.h  \
unistd.h    vfont.h     vfork.h

SYSHDRS= \
dir.h        fcntl.h      file.h       ioctl.h \
param.h      reboot.h     resource.h   rusage.h     signal.h \
sysmacros.h  tty.h        ttychars.h   types.h      vfs.h \
wait.h

UFSHDRS= quota.h

ROOTHDRS= $(HDRS:%=$(ROOT)/usr/ucbinclude/%)  \
	$(SYSHDRS:%=$(ROOT)/usr/ucbinclude/sys/%)  \
	$(UFSHDRS:%=$(ROOT)/usr/ucbinclude/ufs/%) 

DIRS= sys ufs
ROOTDIRS= $(ROOT)/usr/ucbinclude $(DIRS:%=$(ROOT)/usr/ucbinclude/%)

FCNTLH= $(ROOT)/usr/ucbinclude/fcntl.h
SYSFCNTLH= $(ROOT)/usr/ucbinclude/sys/fcntl.h

# Build rule alternatives for FCNTLH
# Inside the NSE, $(CH) comments out the second rule.
# Outside the NSE, the second rule overrides the first.
INS.FCNTLH=	$(RM) $@; $(LN) $(SYSFCNTLH) $@
$(CH)INS.FCNTLH= $(RM) $@; $(SYMLINK) sys/fcntl.h $@

# install rules
$(ROOT)/usr/ucbinclude/sys/%: sys/%
	$(INS.file)

$(ROOT)/usr/ucbinclude/ufs/%: ufs/%
	$(INS.file)

$(ROOT)/usr/ucbinclude/%: %
	$(INS.file)

.KEEP_STATE:

install_h: $(ROOTDIRS) $(ROOTHDRS) $(FCNTLH)

$(FCNTLH): $(SYSFCNTLH)
	$(INS.FCNTLH)

$(ROOTDIRS):
	$(INS.dir)

