#
# ident	"@(#)Makefile	1.21	99/04/07 SMI"
#
# Copyright (c) 1989, 1997 by Sun Microsystems, Inc.
#
# cmd/cron/Makefile
#

include		../Makefile.cmd

CPPFLAGS +=	-I../../lib/libcmd/inc -D_FILE_OFFSET_BITS=64

ROOTVAR =	$(ROOT)/var

ROOTSPCRON =	$(ROOTVAR)/spool/cron
ROOTCROND =	$(ROOTETC)/cron.d
ROOTDEFAULT =	$(ROOTETC)/default
ROOTCRONTABS =	$(ROOTSPCRON)/crontabs
ROOTATJOBS =	$(ROOTSPCRON)/atjobs
ROOTLIBCRON =	$(ROOTLIB)/cron

PROG1 =		cron
PROG2 =		at atq atrm crontab
PROG =		$(PROG1) $(PROG2)

SCRIPT1 =	batch
SCRIPT2 =	logchecker
SCRIPT =	$(SCRIPT1) $(SCRIPT2)

FILES =		cron.dfl

POFILE=		$(PROG1)_cmd.po
POFILES=	at.po crontab.po funcs.po
XGETFLAGS= -a -x $(PROG1).xcl

$(POFILE):	$(POFILES)
	$(RM) $@; cat $(POFILES) > $@

ROOTDIRS =	$(ROOTSPCRON) $(ROOTCROND) $(ROOTDEFAULT)	\
		$(ROOTCRONTABS) $(ROOTATJOBS)

ROOTPROG =	$(PROG1:%=$(ROOTUSRSBIN)/%) $(PROG2:%=$(ROOTBIN)/%)	\
		$(SCRIPT1:%=$(ROOTBIN)/%) $(SCRIPT2:%=$(ROOTCROND)/%)	\
		$(FILES:%=$(ROOTDEFAULT)/%)

ROOTSETUP =	$(ROOTDEFAULT)/cron

ROOTSYMLINK =	$(ROOTLIBCRON) $(ROOTETC)/cron

cron :=		POBJS = cron.o  funcs.o elm.o
at :=		POBJS = at.o att1.o att2.o funcs.o permit.o
atrm :=		POBJS = atrm.o permit.o funcs.o
atq :=		POBJS = atq.o permit.o funcs.o
crontab :=	POBJS = crontab.o permit.o funcs.o

cron := LDLIBS += -lpam
at.o funcs.o permit.o crontab.o := CFLAGS += -v

OBJS =		cron.o funcs.o elm.o at.o att1.o att2.o permit.o	\
		atrm.o atq.o crontab.o

SRCS =		$(OBJS:%.o=%.c)

CLOBBERFILES =	$(SCRIPT)

$(ROOTLIBCRON) :=		SYMLNKDEST = ../../etc/cron.d
$(ROOTETC)/cron :=		SYMLNKDEST = ../usr/sbin/cron

$(ROOTBIN)/at :=		FILEMODE = 04755
$(ROOTBIN)/at :=		OWNER = root   
$(ROOTBIN)/at :=		GROUP = sys
$(ROOTBIN)/atrm :=		FILEMODE = 04755
$(ROOTBIN)/atrm :=		OWNER = root   
$(ROOTBIN)/atrm :=		GROUP = sys
$(ROOTBIN)/atq :=		FILEMODE = 04755
$(ROOTBIN)/atq :=		OWNER = root   
$(ROOTBIN)/atq :=		GROUP = sys
$(ROOTBIN)/crontab :=		FILEMODE = 04555
$(ROOTBIN)/crontab :=		OWNER = root
$(ROOTUSRSBIN)/cron :=		FILEMODE = 0555
$(ROOTUSRSBIN)/cron :=		OWNER = root
$(ROOTUSRSBIN)/cron :=		GROUP = sys

LDLIBS +=	-lnsl -lcmd -lbsm -lsecdb

.KEEP_STATE:

all :		$(PROG) $(SCRIPT) $(FILES)

install :	all $(ROOTPROG) $(ROOTSETUP) $(ROOTSYMLINK) 

$(PROG) :	$$(POBJS)
		$(LINK.c) $(POBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

att1.c :	att1.y
		yacc -d att1.y
		$(MV) y.tab.c att1.c
		$(MV) y.tab.h att1.h

att2.c :	att2.l att2.ed att1.c
		lex att2.l
		ed - lex.yy.c < att2.ed
		$(MV) lex.yy.c att2.c

# Don't re-install direcories installed by Targetdirs
#$(ROOTDIRS):
#		$(INS.dir)

$(ROOTCROND)/% $(ROOTDEFAULT)/% : %
		$(INS.file)

$(ROOTSYMLINK) :
		$(RM) $@; $(SYMLINK) $(SYMLNKDEST) $@

$(ROOTSETUP) :	$(ROOTDEFAULT)/cron.dfl
		$(RM) $@; $(MV) $(ROOTDEFAULT)/cron.dfl $@

clean :
		$(RM) $(OBJS) att1.h att1.c att2.c

lint :		lint_SRCS

strip :
		$(STRIP) $(PROG)

include		../Makefile.targ
