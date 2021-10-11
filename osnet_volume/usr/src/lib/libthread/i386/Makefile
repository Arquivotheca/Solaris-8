#
#ident	"@(#)Makefile	1.47	99/06/08 SMI"
#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libthread/i386/Makefile
#
LIBTHREADDIR=..
LIBCDIR=../../libc
SYS=${LIBTHREADDIR}/sys
SYSARCH=${SYS}/i386
SYSCOM=${SYS}/common
COMMON=${LIBTHREADDIR}/common
LIBTHREAD.ARCH=${LIBTHREADDIR}/i386/ml
INLINE=${LIBTHREAD.ARCH}/thread.il
MAPDIR=../spec/i386

COMMONOBJS=	condvar.o callout.o disp.o lwp_subr.o ma.o mutex.o\
		preempt.o rwlock.o sema.o subr.o slp.o reaper.o\
		sig.o thread.o thr_subr.o tsd.o resv_tls.o\
		attr_subr.o pthr_cond.o pthr_mutex.o pthr_attr.o pthr_rwlock.o \
		pthread.o cancel.o thread_interface.o schedctl.o tdb_agent.o \
		rtsched.o

ARCHOBJS=	i386dep.o i386subr.o resume.o _mutex.o siglongjmp.o

SYSOBJS= 	fork1.o _fork1.o _fork.o  sigaction.o sleep.o\
		ucontext.o sigprocmask.o sigsuspend.o sigwait.o \
		cerror.o setitimer.o sigpending.o syscall_cancelpoint.o \
		alarm.o fcntl.o sigsetjmp.o

#
# Since assym.s is a derived file, the dependency must be explicit for
# all files including this file. (This is only actually required in the
# instance when the .make.state file does not exist.)
#
ASSYM_DEPS=	i386subr.o resume.o syscall_cancelpoint.o _mutex.o \
		sigsetjmp.o 

RESUMEOBJ=

CFILES=	${COMMON}/condvar.c ${COMMON}/disp.c ${COMMON}/lwp.c \
	${COMMON}/lwp_subr.c ${COMMON}/ma.c\
	${COMMON}/subr.c ${COMMON}/mutex.c ${COMMON}/preempt.c \
	${COMMON}/reaper.c ${COMMON}/rwlock.c \
	${COMMON}/sema.c ${COMMON}/thread.c  ${COMMON}/thr_subr.c \
	${COMMON}/tsd.c ${LIBTHREAD.ARCH}/i386dep.c ${SYSCOM}/sigaction.c\
	${COMMON}/slp.c\
	${COMMON}/resv_tls.c\
	${SYSCOM}/sleep.c ${COMMON}/sig.c ${SYSCOM}/fork1.c\
	${COMMON}/callout.c ${SYSCOM}/sigsuspend.c ${SYSCOM}/sigwait.c\
	${COMMON}/thread_interface.c ${SYSCOM}/setitimer.c\
	${COMMON}/pthread.c ${COMMON}/attr_subr.c\
	${COMMON}/cancel.c\
	$(COMMON}/pthr_cond.c $(COMMON}/pthr_mutex.c $(COMMON}/pthr_attr.c\
	${COMMON}/pthr_rwlock.c \
	${SYSCOM}/sigpending.c ${SYSARCH}/siglongjmp.c \
	${COMMON}/tdb_agent.c \
	${COMMON}/rtsched.c

HFILES = ${COMMON}/libthread.h ${COMMON}/libpthr.h

SFILES= ${SYSARCH}/alarm.s\
	${LIBTHREAD.ARCH}/i386subr.s\
	${LIBTHREAD.ARCH}/_mutex.s\
	${SYSARCH}/ucontext.s ${SYSARCH}/_fork.s ${SYSARCH}/_fork1.s \
	${SYSARCH}/syscall_cancelpoint.s ${SYSARCH}/sigsetjmp.s \
	${LIBTHREAD.ARCH}/resume.s

INC=-I. -I${LIBTHREADDIR}/inc -I${COMMON} -I${SYS}/inc -I${LIBTHREAD.ARCH} \
    -I${LIBCDIR}/inc -I${LIBCDIR}/i386/inc

# Set DBG to null if you do not wish to turn DEBUG on
DBG= -DBUG_1156578

#
# NOTE: ARCHOBJS are listed first because the .init of i386subr.o
#	must be called before the init inside of rtld.o.  This is
#	because the call to ld_concurrency must have libthread
#	properly initialized so that it can make calls back into
#	libthread.
#
OBJECTS = ${ARCHOBJS} ${COMMONOBJS} ${SYSOBJS}

LWP.O_IFDEBUG = if test -n "`echo $(DBG)|grep DEBUG`"; \
		then echo "lwp.o";\
		fi

OBJECTS += $(LWP.O_IFDEBUG:sh)

LIBRARY= libthread.a
LIBRARY_CRES= libthread_cres.a
DYNLIB_CRES= libthread_cres.so.1
LIB_PIC= libthread_pic.a
LIBRARY_T= libthread_trace.a
LIBRARY_IT= libthread_itrace.a
DYNLIB_T= libthread_trace.so.1
DYNLIB_IT= libthread_itrace.so.1
VERS=	.1

#include global definitions for libraries
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
MAPOPTS=	$(MAPFILE:%=-M %)

# Use CPP_EOPTS to switch on TLS, e.g. "make libthread.a CPP_EOPTS=-DTLS"
CPP_EOPTS=
CPPFLAGS += ${INC} ${DBG} -Di386 ${CPP_EOPTS} -D_REENTRANT

# Use C_EOPTS to add c flags, such as -g
C_EOPTS=
#pics/i386dep.o	:=	CFLAGS += -DBROKEN_CREATE

INLINE_IF_NOTLS= if test -n "`echo $(EOPTS)|grep TLS`"; \
		then echo "";\
		else echo ${INLINE};\
		fi
CFLAGS +=  ${INLINE_IF_NOTLS:sh}
DYNFLAGS += $(MAPOPTS) -znodelete -zinitfirst

ASFLAGS += -P -D__STDC__ -DLOCORE -D_ASM -D_SYS_SYS_S ${CPPFLAGS}
ARFLAGS = rcu


TRACE_OBJS= utrace.o gethrtime.o
DY_TRACE_OBJS= utrace_pic.o gethrtime.o

$(LIBRARY) 	:= AROBJS=$(OBJS)
$(LIBRARY_CRES) := AROBJS=$(OBJS)

$(LIBRARY_T) 	:= OBJS_T=$(OBJS:objs/%=objs_t/%)
$(LIBRARY_T) 	:= AROBJS=$(OBJS_T) $(TRACE_OBJS)
$(LIBRARY_T)	:= CPPFLAGS += -DUTRACE

$(LIBRARY_IT) 	:= OBJS_IT=$(OBJS:objs/%=objs_it/%)
$(LIBRARY_IT) 	:= AROBJS=$(OBJS_IT) $(TRACE_OBJS)
$(LIBRARY_IT)	:= CPPFLAGS += -DITRACE

$(LIB_PIC) 	:= AROBJS=$(PICS)

$(LIBPIC_CRES) 	:= AROBJS=$(PICS)

$(DYNLIB_T) 	:= PICS_T=$(PICS:pics/%=pics_t/%) ${DY_TRACE_OBJS}
$(DYNLIB_T) 	:= CPPFLAGS += -K PIC -DUTRACE
$(DYNLIB_T) 	:= ASFLAGS += -DPIC
$(DYNLIB_T) 	:= DYNFLAGS = -ztext -h $(DYNLIB_T)

$(DYNLIB_IT) 	:= PICS_IT=$(PICS:pics/%=pics_it/%) ${DY_TRACE_OBJS}
$(DYNLIB_IT) 	:= CPPFLAGS += -K PIC -DITRACE
$(DYNLIB_IT) 	:= ASFLAGS += -DPIC
$(DYNLIB_IT) 	:= DYNFLAGS = -ztext -h $(DYNLIB_IT)

LDLIBS +=	-ldl -lc

# include common library targets
include ../../Makefile.targ

$(LIB_PIC) := DIR = pics
$(LIB_PIC): pics $$(PICS)
	$(BUILD.AR)
	$(POST_PROCESS_A)

$(LIBRARY_CRES): objs $$(OBJS)
	$(BUILD.AR)
	$(POST_PROCESS_A)

$(LIBRARY_T): objs_t $$(OBJS_T)
	$(BUILD.AR)
	$(POST_PROCESS_A)

$(LIBRARY_IT): objs_it $$(OBJS_IT)
	$(BUILD.AR)
	$(POST_PROCESS_A)

$(DYNLIB_CRES): pics $$(PICS)
	$(CC) -o $@ -dy -G $(DYNFLAGS) $(PICS) $(LDLIBS)
	$(POST_PROCESS_SO)

$(DYNLIB_T): pics_t $$(PICS_T)
	$(CC) -o $@ -dy -G $(DYNFLAGS) $(PICS_T) $(LDLIBS)
	$(POST_PROCESS_SO)

$(DYNLIB_IT): pics_it $$(PICS_IT)
	$(CC) -o $@ -dy -G $(DYNFLAGS) $(PICS_IT) $(LDLIBS)
	$(POST_PROCESS_SO)

$(DYNLIB):	$(MAPFILE)

SRCS=	$(CFILES)
CLEANFILES += assym.s genassym

# definitions for lint

$(LINTLIB):= SRCS=../common/llib-lthread
LINTFLAGS=	-u -I..
LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES +=	$(LINTOUT) $(LINTLIB)
CLOBBERFILES	+=	$(MAPFILE)

.KEEP_STATE:

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

all: ${LIBRARY} 
install_static: $(ROOTLIBDIR)/${LIBRARY}

lint:		$(LINTLIB)

$(ASSYM_DEPS:%=objs/%):  assym.s
$(ASSYM_DEPS:%=pics/%):  assym.s

assym.s: genassym ${INLINE}
	./genassym > assym.s
genassym: ${LIBTHREAD.ARCH}/genassym.c ${LIBTHREADDIR}/inc/utrace.h
	$(NATIVECC) -O $(INC) $(CPPFLAGS.master) \
		${LIBTHREAD.ARCH}/genassym.c -o genassym

objs/%.o pics/%.o: ${LIBCDIR}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ${COMMON}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ${LIBTHREAD.ARCH}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ${LIBTHREAD.ARCH}/%.s ${INLINE}
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ${SYSARCH}/%.s ${INLINE}
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ${SYSARCH}/%.c $(HFILES) \
		   ${INLINE}  $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ${SYSCOM}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# Trced objects

objs_t/%.o pics_t/%.o: ${COMMON}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs_t/%.o pics_t/%.o: ${LIBTHREAD.ARCH}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs_t/%.o pics_t/%.o: ${LIBTHREAD.ARCH}/%.s ${INLINE}
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

objs_t/%.o pics_t/%.o: ${SYSARCH}/%.s ${INLINE}
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

objs_t/%.o pics_t/%.o: ${SYSCOM}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# Trced objects

objs_it/%.o pics_it/%.o: ${COMMON}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs_it/%.o pics_it/%.o: ${LIBTHREAD.ARCH}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs_it/%.o pics_it/%.o: ${LIBTHREAD.ARCH}/%.s ${INLINE}
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

objs_it/%.o pics_it/%.o: ${SYSARCH}/%.s ${INLINE}
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

objs_it/%.o pics_it/%.o: ${SYSCOM}/%.c $(HFILES) \
		   ${INLINE} $(LIBTHREADDIR)/inc/utrace.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)


