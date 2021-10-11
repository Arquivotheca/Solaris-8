#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.1	99/10/06 SMI"
#
# lib/libldap/Makefile.com

LIBRARY= libldap.a
VERS= .3

LDAPOBJS=	abandon.o             getentry.o            referral.o \
		add.o                 getfilter.o           regex.o \
		addentry.o            getmsg.o              rename.o \
		bind.o                getref.o              request.o \
		cache.o               getvalues.o           result.o \
		charset.o             kbind.o               saslbind.o \
		cldap.o               sbind.o 		    compare.o    \
		search.o 	      controls.o            sort.o \
		delete.o              srchpref.o	    disptmpl.o \
		tmplout.o 	      dsparse.o             \
		error.o               ufn.o \
		extensions.o          unbind.o 	            extop.o    \
		url.o         \
		free.o   modify.o              utils.o \
		friendly.o            modrdn.o    notif.o    Version.o \
		getattr.o             open.o                \
		getdn.o               option.o \
		getdxbyname.o         os-ip.o  

BEROBJS=	bprint.o	      decode.o \
		encode.o 	   \
		io.o		      i18n.o

UTILOBJS=	line64.o	log.o


SECOBJS=	cram_md5.o	md5.o	secutil.o

OBJECTS=	$(LDAPOBJS)	$(BEROBJS)	$(UTILOBJS)	$(SECOBJS)
# include library definitions
include ../../Makefile.lib

LDAPINC=	$(SRC)/lib/libldap3/include -I$(SRC)/lib/libldap/include
LDAP_FLAGS=	-DLDAP_REFERRALS -DCLDAP -DLDAP_DNS -DSUN

MAPFILE=	$(MAPDIR)/mapfile

SRCS=		$(LDAPOBJS:%.o=../common/%.c)	$(BEROBJS:%.o=../ber/%.c) \
		$(UTILOBJS:%.o=../util/%.c)	$(SECOBJS:%.o=../sec/%.c) 

LIBS =		$(DYNLIB)

# definitions for lint

$(LINTLIB):= 	SRCS=../common/llib-lldap
$(LINTLIB):= 	LINTFLAGS=-nvx $(LOCFLAGS)
$(LINTLIB):= 	TARGET_ARCH=

LINTFLAGS=	-u  -I$(LDAPINC)
LINTFLAGS64=	-u  -I$(LDAPINC)
LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)


CLEANFILES += 	$(LINTOUT) $(LINTLIB)
CLOBBERFILES +=	$(MAPFILE)

# Local Libldap definitions

LOCFLAGS +=	-D_SYS_STREAM_H -D_REENTRANT -DSVR4 -DSUNW_OPTIONS \
		-DTHREAD_SUNOS5_LWP -DSOUNDEX -DSTR_TRANSLATION \
		$(LDAP_FLAGS) -I$(LDAPINC)

CFLAGS +=	-v $(LOCFLAGS) 
CFLAGS64 +=	-v $(LOCFLAGS)
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-lsocket -lnsl -lresolv -lc

.KEEP_STATE:

lint: $(LINTLIB)

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ../../libldap/common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ../ber/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ../../libldap/ber/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ../../libldap/util/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ../../libldap/sec/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)


# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)
