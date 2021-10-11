# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.16	99/10/09 SMI"
#
# cmd/geniconvtbl/Makefile.com


ITM	= geniconvtbl.so
PROG	= geniconvtbl

SRCSH1  = iconv_tm.h hash.h
SRCCH1  = itmcomp.h itm_util.h maptype.h
SRCSC1  = itmcomp.c assemble.c disassemble.c itm_util.c
SRCY1   = itm_comp.y
SRCL1   = itm_comp.l
SRCI1   = geniconvtbl.c


YTABC   = y.tab.c
YTABH   = y.tab.h
LEXYY   = lex.yy.c
YOUT    = y.output
MAPFILE	= ../mapfile



SRCSH	= $(SRCSH1:%.h=../%.h)
SRCCH	= $(SRCCH1:%.h=../%.h)
SRCSC	= $(SRCSC1:%.c=../%.c)
SRCI	= $(SRCI1:%.c=../%.c)
SRCY    = $(SRCY1:%.y=../%.y)
SRCL    = $(SRCL1:%.l=../%.l)

SRCYC	= $(SRCY:%.y=%.c)
SRCLC	= $(SRCL:%.l=%.c)

SRCS    = $(SRCSC) $(YTABC) $(LEXYY)
HDRS	= $(SRCCH1) $(ERNOSTRH)



SED	= sed
LEXSED	= ../lex.sed
YACCSED	= ../yacc.sed



# include ../../../lib/Makefile.lib
include ../../Makefile.cmd


ROOTDIRS32=	$(ROOTLIB)/iconv
ROOTDIRS64=	$(ROOTLIB)/iconv/$(MACH64)
ROOTITM32 =	$(ROOTDIRS32)/$(ITM)
ROOTITM64 =	$(ROOTDIRS64)/$(ITM)

#
# definition for some useful target like clean, 
OBJS	= $(SRCSC1:%.c=%.o) $(YTABC:.c=.o) $(LEXYY:.c=.o)

CHECKHDRS = $(HDRS%.h=%.check)

CLOBBERFILES=	$(ITM)
CLEANFILES = 	$(OBJS) $(YTABC) $(YTABH) $(LEXYY) $(YOUT) \
		$(POFILES) $(POFILE)

CPPFLAGS	+= -I. -I..
YFLAGS		+= -d -v
CFLAGS 		+= -D_FILE_OFFSET_BITS=64
LINTFLAGS64	+= -Xarch=v9

$(ITM) :=	CFLAGS += -G -K pic -dy -z text -h $@ -D_REENTRANT 
$(ITM) :=	sparc_CFLAGS += -xregs=no%appl
$(ITM) :=	sparcv9_CFLAGS += -xregs=no%appl



#
# Message catalog
#
POFILES= $(SRCSC1:%.c=%.po) $(SRCI1:%.c=%.po) \
		$(SRCY1:%.y=%.po) $(SRCL1:%.l=%.po)

POFILE= geniconvtbl_.po





.KEEP_STATE:

.PARALLEL: $(ITM) $(OBJS)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ -lgen $(LDLIBS)
	$(POST_PROCESS)

$(ITM): $(SRCI)
	$(CC) $(CFLAGS) -M $(MAPFILE) -o $@ $(SRCI) $(LDLIBS)
	$(POST_PROCESS_SO)

$(YTABC) $(YTABH): $(SRCY)
	$(YACC) $(YFLAGS) $(SRCY)
	@ $(MV) $(YTABC) $(YTABC)~
	@ $(SED) -f $(YACCSED) $(YTABC)~ > $(YTABC)
	@ $(RM) $(YTABC)~

$(LEXYY): $(SRCL) $(YTABH)
	$(LEX) -t $(SRCL) | $(SED) -f $(LEXSED) > $(LEXYY)


$(POFILE):  .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) >$@

$(POFILES): $(SRCSC) $(SRCI) $(SRCY) $(SRCL)

%.po:	../%.c
	$(COMPILE.cpp) $<  > $<.i
	$(BUILD.po)


lint : lint_SRCS1  lint_SRCS2


lint_SRCS1: $(SRCS)
	$(LINT.c) $(SRCS) $(LDLIBS)

lint_SRCS2: $(SRCI)
	$(LINT.c) $(SRCI) $(LDLIBS)



hdrchk: $(HDRCHECKS)

cstyle: $(SRCS)
	$(DOT_C_CHECK)

clean:
	$(RM) $(CLEANFILES)

debug:
	$(MAKE)	all COPTFLAG='' COPTFLAG64='' CFLAGS='-g -DDEBUG'


%.o:	%.c 
	$(COMPILE.c) $<

%.o:	../%.c
	$(COMPILE.c) $<



# install rule
# 
$(ROOTDIRS32)/%: $(ROOTDIRS32) %
	-$(INS.file)

$(ROOTDIRS64)/%: $(ROOTDIRS64) %
	-$(INS.file)

$(ROOTDIRS32): $(ROOTLIB)
	-$(INS.dir)

$(ROOTDIRS64): $(ROOTDIRS32)
	-$(INS.dir)

$(ROOTLIB) $(ROOTBIN):
	-$(INS.dir)

include ../../Makefile.targ

