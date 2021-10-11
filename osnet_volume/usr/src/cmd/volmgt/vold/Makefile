#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# Makefile for volume management daemon
#
#ident	"@(#)Makefile	1.40	98/12/04 SMI"
#
# cmd/volmgt/vold/Makefile
#

PROG=	vold

include ../../../lib/Makefile.lib
include ../../Makefile.cmd
include ../Makefile.volmgt

SED=	sed

OBJS=	nfs_server.o nfs_trace.o vold_err.o vold_main.o vold_proc.o \
	vold_node.o vold_util.o vold_dev.o vold_config.o vold_vol.o \
	vold_label.o vold_db.o vold_path.o vold_props.o vold_action.o \
	vold_obj.o vold_md4.o vold_mnt.o

TMPOBJ= nfs_prot.o

# drivers for devices
DEV_VERS=1
DEVS=	dev_cdrom.so.${DEV_VERS} dev_floppy.so.${DEV_VERS} \
	dev_test.so.${DEV_VERS} dev_pcmem.so.${DEV_VERS} \
	dev_rmscsi.so.${DEV_VERS}

# drivers for labels
LABEL_VERS=1
LABS=	label_test.so.${LABEL_VERS} label_cdrom.so.${LABEL_VERS} \
	label_sun.so.${LABEL_VERS} label_dos.so.${LABEL_VERS}

# databases
DB_VERS=1
DBS=	db_nis.so.${DB_VERS} db_mem.so.${DB_VERS}

SOFILES=	${LABS} ${DEVS} ${DBS}

CFLAGS += -v

$(PROG) :=	LDLIBS += -lthread -ldl -lnsl -ladm
$(SOFILES) :=	LDLIBS += -lc

# to get the correct DSO flags used for compilation/linking
# (should also use "-zdefs" for DYNFLAGS, but SOFILES aren't yet self-contained)
$(SOFILES) :=	CFLAGS += -K pic
$(SOFILES) :=	HSONAME =
$(SOFILES) :=	ZDEFS =

# for bug that causes db_nis.c to fail compilation (for some compilers?)
# with "-xO3" or "-xO2" on SPARC
db_nis.so.$(DB_VERS) :=	COPTFLAG = -xO1

#
# Rules for making shared objects out of .c files.  Works well if
# we have a one-to-one mapping.  Applies in all cases so far.
#
.SUFFIXES: .so.${DB_VERS} .so.${LABEL_VERS} .so.${DEV_VERS}  ${SUFFIXES}

.c.so.${DB_VERS}:
	${CC} ${CPPFLAGS} ${CFLAGS} -G -o $@ $< $(DYNFLAGS) $(LDLIBS)
	$(POST_PROCESS_SO)

.c.so.${LABEL_VERS}:
	${CC} ${CPPFLAGS} ${CFLAGS} -G -o $@ $< $(DYNFLAGS) $(LDLIBS)
	$(POST_PROCESS_SO)

.c.so.${DEV_VERS}:
	${CC} ${CPPFLAGS} ${CFLAGS} -G -o $@ $< $(DYNFLAGS) $(LDLIBS)
	$(POST_PROCESS_SO)

# for messaging catalog
#
POFILE= vold.po

XFILE=	${ROOT}/usr/include/rpcsvc/nfs_prot.x

SRCS=	${OBJS:%.o=%.c} ${DEVS:%.so.${DEV_VERS}=%.c} \
	${LABS:%.so.${LABEL_VERS}=%.c} ${DBS:%.so.${DB_VERS}=%.c}

CPPFLAGS += 	-D_REENTRANT -DSUN_THREADS -DLABEL_VERS=${LABEL_VERS}\
		-DDB_VERS=${DB_VERS} -DDEV_VERS=${DEV_VERS}


# install macros and rule
#
GROUP=		sys
ROOTVOLD=	${ROOTLIB}/vold
ROOTSOFILES=	${SOFILES:%=${ROOTVOLD}/%}

.KEEP_STATE:


all: $(PROG) $(SOFILES)

dso dsos libs: $(SOFILES)

catalog: $(POFILE)

$(POFILE): $(SRCS)
	$(RM) $@
	$(COMPILE.cpp) $(SRCS) | \
	$(XGETTEXT) $(XGETFLAGS) -
	$(SED) -e '/^domain/d' messages.po > $@
	$(RM) messages.po

${PROG}: ${OBJS} ${TMPOBJ}
	${LINK.c} ${OPT} -o $@ ${OBJS} ${TMPOBJ} ${LDLIBS}
	${POST_PROCESS}

lint :=	LINTFLAGS=-x -errchk=longptr64 -errhdr=`pwd` -fd \
		-Ncheck=macro -Nlevel=4

lint:
	$(LINT.c) ${SRCS}

cstyle:
	${CSTYLE} ${SRCS}

install: all ${ROOTVOLD} ${ROOTUSRSBINPROG} ${ROOTSOFILES}

clean:     
	${RM} $(OBJS) ${TMPOBJ} $(SOFILES) nfs_prot.c core a.out

clobber: clean
	${RM} $(PROG)

nfs_prot.c: $(XFILE)
	$(RPCGEN) -c $(XFILE) -o $@

${ROOTVOLD}/%: %
	${INS.file}

${ROOTVOLD}:
	${INS.dir}
