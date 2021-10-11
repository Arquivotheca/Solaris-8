#
# Copyright (c) 1992 by Sun Microsystems, Inc.
#
#pragma	ident	"@(#)Makefile	1.16	99/06/18 SMI"
#
# cmd/sa/Makefile
#

include ../Makefile.cmd

LDLIBS += -lkstat

GREP=		grep

SADC= 		sadc
SADP= 		sadp
SAR= 		sar
TIMEX=		timex
SAG=		sag
SA1=		sa1
SA2=		sa2
PERF=		perf
S21PERF=	S21perf

# Executables produced
BINPROG=	$(TIMEX)
SBINPROG=	$(SAR) $(SAG)
LIBPROG=	$(SADC)
LIBSHELL=	$(SA1) $(SA2)
INITSHELL=	$(PERF)

PROGS=		$(BINPROG) $(SBINPROG) $(LIBPROG)
SHELLS=		$(INITSHELL) $(LIBSHELL)
TXTS= 		$(SADP).c README
ALL=		$(PROGS) $(SHELLS)

# Source files
SAG_OBJECTS=	$(SAG)a.o $(SAG)b.o
SADC_OBJECTS=	$(SADC).o
srcs=		$(TIMEX) $(SAR) $(SADC)
SRCS=		$(srcs:%=%.c) $(SAG_OBJECTS:%.o=%.c)
SHSRCS=		$(SHELLS:%=%.sh)

# Set of target install directories
LIBSAD=		$(ROOT)/usr/lib/sa
CROND=		$(ROOT)/var/spool/cron
CRONTABSD=	$(CROND)/crontabs
ETCINITD=	$(ROOTETC)/init.d
ETCRC2D=	$(ROOTETC)/rc2.d

# Set of target install files
SYSCRONTAB=	$(CRONTABSD)/sys
ROOTPROG=	$(BINPROG:%=$(ROOTBIN)/%)
ROOTUSBINPROG=	$(SBINPROG:%=$(ROOTUSRSBIN)/%)
ROOTLIBPROG=	$(LIBPROG:%=$(LIBSAD)/%)
ROOTLIBSHELL=	$(LIBSHELL:%=$(LIBSAD)/%)
ROOTINITSHELL=	$(INITSHELL:%=$(ETCINITD)/%)
ROOTSYMLINKS=	$(SBINPROG:%=$(ROOTBIN)/%)
ROOTS21PERF=	$(S21PERF:%=$(ETCRC2D)/%)

# Performance monitoring should not be enabled by default. Hence, these
# entries are comments. Note the difficulty of inserting a literal #
# in a makefile.... Wonderful parser here....
COMMENT_CHAR:sh=          echo \\043
ENTRY1=		'$(COMMENT_CHAR) 0 * * * 0-6 /usr/lib/sa/sa1'
ENTRY2=		'$(COMMENT_CHAR) 20,40 8-17 * * 1-5 /usr/lib/sa/sa1'
ENTRY3=		'$(COMMENT_CHAR) 5 18 * * 1-5 /usr/lib/sa/sa2 -s 8:00 -e 18:01 -i 1200 -A'

CLOBBERFILES=	$(PROGS) $(SHELLS)

# Conditionals
$(SYSCRONTAB)		:= OWNER = root
$(SYSCRONTAB)     	:= GROUP = sys
$(ETCINITD)/$(PERF)  	:= OWNER = root
$(ETCINITD)/$(PERF)  	:= GROUP = sys
$(ETCINITD)/$(PERF) 	:= FILEMODE = 0744
$(ROOTBIN)/$(TIMEX)	:= GROUP = sys
# $(ROOTUSRSBIN)/$(SADP) 	:= FILEMODE = 2555
# $(ROOTUSRSBIN)/$(SADP) 	:= GROUP = sys
$(LIBSAD)/$(SADC) 	:= FILEMODE = 0555
$(LIBSAD)/$(SADC) 	:= GROUP = bin

.KEEP_STATE:

all: 		$(ALL) $(TXTS)

$(SAG):		$(SAG_OBJECTS)
	$(LINK.c) -o $@ $(SAG_OBJECTS) $(LDLIBS)
	$(POST_PROCESS)

$(SADC):	$(SADC_OBJECTS)
	$(LINK.c) -o $@ $(SADC_OBJECTS) $(LDLIBS)
	$(POST_PROCESS)

# The edit of SYSCRONTAB must be done unconditionally because of the
# creation of this file by a different component (Adm) and the possible
# backdating.
install:	all $(ROOTPROG) $(ROOTUSBINPROG)     \
		$(ROOTINITSHELL)  $(ROOTLIBSHELL) $(ROOTSYMLINKS) \
		$(ROOTS21PERF)	  $(ROOTLIBPROG)
	@if [ -f $(SYSCRONTAB) ]; \
	then \
		if $(GREP) "sa1" $(SYSCRONTAB) >/dev/null 2>&1 ; then :; \
		else \
			echo $(ENTRY1) >> $(SYSCRONTAB); \
			echo $(ENTRY2) >> $(SYSCRONTAB); \
			echo "$(SYSCRONTAB) modified"; \
		fi; \
		if $(GREP) "sa2" $(SYSCRONTAB) >/dev/null 2>&1 ; then :; \
		else \
			echo $(ENTRY3) >> $(SYSCRONTAB); \
		fi; \
	fi

$(LIBSAD)/%: %
	$(INS.file)

$(ROOTSYMLINKS):
	-$(RM) $@; $(SYMLINK) ../sbin/`basename $@` $@

$(ETCRC2D)/%: $(ROOTINITSHELL)
	-$(RM) $@; $(LN) $< $@

$(ETCINITD)/%: %
	$(INS.file)

clean:
	$(RM) $(SAG_OBJECTS) $(SADC_OBJECTS) $(PROGS) $(SHELLS) $(SADP)

lint:	lint_SRCS

include ../Makefile.targ
