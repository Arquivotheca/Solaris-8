/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)print.c	1.43	99/08/15 SMI"

#define	_SYSCALL32	/* make 32-bit compat headers visible */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <termio.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/ulimit.h>
#include <sys/utsname.h>
#include <sys/vtrace.h>
#include <sys/kstat.h>
#include <sys/modctl.h>
#include <sys/acl.h>
#include <stropts.h>
#include <sys/schedctl.h>
#include <sys/isa_defs.h>
#include <sys/systeminfo.h>
#include <sys/cladm.h>
#include <sys/lwp.h>
#include <bsm/audit.h>
#include <libproc.h>
#include <sys/aio.h>
#include <sys/corectl.h>
#include <sys/cpc_impl.h>
#include "print.h"
#include "ramdata.h"
#include "proto.h"

static void grow(int nbyte);

#define	GROW(nb)	if (sys_leng+(nb) >= sys_ssize) grow(nb)

/*ARGSUSED*/
static void
prt_nov(int raw, long val)	/* print nothing */
{
}

/*ARGSUSED*/
static void
prt_dec(int raw, long val)	/* print as decimal */
{
	GROW(24);
	if (data_model == PR_MODEL_ILP32)
		sys_leng += sprintf(sys_string+sys_leng, "%d", (int)val);
	else
		sys_leng += sprintf(sys_string+sys_leng, "%ld", val);
}

/*ARGSUSED*/
static void
prt_uns(int raw, long val)	/* print as unsigned decimal */
{
	GROW(24);
	if (data_model == PR_MODEL_ILP32)
		sys_leng += sprintf(sys_string+sys_leng, "%u", (int)val);
	else
		sys_leng += sprintf(sys_string+sys_leng, "%lu", val);
}

/*ARGSUSED*/
static void
prt_oct(int raw, long val)	/* print as octal */
{
	GROW(24);
	if (data_model == PR_MODEL_ILP32)
		sys_leng += sprintf(sys_string+sys_leng, "%#o", (int)val);
	else
		sys_leng += sprintf(sys_string+sys_leng, "%#lo", val);
}

/*ARGSUSED*/
static void
prt_hex(int raw, long val)	/* print as hexadecimal */
{
	GROW(20);
	if (data_model == PR_MODEL_ILP32)
		sys_leng += sprintf(sys_string+sys_leng, "0x%.8X", (int)val);
	else
		sys_leng += sprintf(sys_string+sys_leng, "0x%.8lX", val);
}

/*ARGSUSED*/
static void
prt_hhx(int raw, long val)	/* print as hexadecimal (half size) */
{
	GROW(20);
	if (data_model == PR_MODEL_ILP32)
		sys_leng += sprintf(sys_string+sys_leng, "0x%.4X", (int)val);
	else
		sys_leng += sprintf(sys_string+sys_leng, "0x%.4lX", val);
}

/*ARGSUSED*/
static void
prt_dex(int raw, long val)  /* print as decimal if small, else hexadecimal */
{
	if (val & 0xff000000)
		prt_hex(0, val);
	else
		prt_dec(0, val);
}

/*ARGSUSED*/
static void
prt_llo(int raw, long val1, long val2)	/* print long long offset */
{
	int hival;
	int loval;

#ifdef	_LONG_LONG_LTOH
	hival = (int)val2;
	loval = (int)val1;
#else
	hival = (int)val1;
	loval = (int)val2;
#endif

	if (hival == 0)
		prt_dex(0, loval);
	else {
		GROW(18);
		sys_leng += sprintf(sys_string+sys_leng, "0x%.8X%.8X",
			hival, loval);
	}
}

static void
prt_stg(int raw, long val)	/* print as string */
{
	char *s = raw? NULL : fetchstring((long)val, 400);

	if (s == NULL)
		prt_hex(0, val);
	else {
		GROW((int)strlen(s)+2);
		sys_leng += sprintf(sys_string+sys_leng, "\"%s\"", s);
	}
}

static void
prt_rst(int raw, long val)	/* print as string returned from syscall */
{
	char *s = (raw || Errno || slowmode)? NULL :
			fetchstring((long)val, 400);

	if (s == NULL)
		prt_hex(0, val);
	else {
		GROW((int)strlen(s)+2);
		sys_leng += sprintf(sys_string+sys_leng, "\"%s\"", s);
	}
}

static void
prt_rlk(int raw, long val)	/* print contents of readlink() buffer */
{
	char *s = (raw || Errno || slowmode || Rval1 <= 0)? NULL :
		fetchstring((long)val, (Rval1 > 400)? 400 : Rval1);

	if (s == NULL)
		prt_hex(0, val);
	else {
		GROW((int)strlen(s)+2);
		sys_leng += sprintf(sys_string+sys_leng, "\"%s\"", s);
	}
}

static void
prt_ioc(int raw, long val)	/* print ioctl code */
{
	const char *s = raw? NULL : ioctlname((int)val);

	if (s == NULL)
		prt_hex(0, val);
	else
		outstring(s);
}

static void
prt_ioa(int raw, long val)	/* print ioctl argument */
{
	const char *s;

	switch (sys_args[1]) {	/* cheating -- look at the ioctl() code */

	/* kstat ioctl()s */
	case KSTAT_IOC_READ:
	case KSTAT_IOC_WRITE:
#ifdef _LP64
		if (data_model == PR_MODEL_ILP32)
			prt_stg(raw, val + offsetof(kstat32_t, ks_name[0]));
		else
#endif
			prt_stg(raw, val + offsetof(kstat_t, ks_name[0]));
		break;

	/* streams ioctl()s */
	case I_LOOK:
		prt_rst(raw, val);
		break;
	case I_PUSH:
	case I_FIND:
		prt_stg(raw, val);
		break;
	case I_LINK:
	case I_UNLINK:
	case I_SENDFD:
		prt_dec(0, val);
		break;
	case I_SRDOPT:
		if (raw || (s = strrdopt(val)) == NULL)
			prt_dec(0, val);
		else
			outstring(s);
		break;
	case I_SETSIG:
		if (raw || (s = strevents(val)) == NULL)
			prt_hex(0, val);
		else
			outstring(s);
		break;
	case I_FLUSH:
		if (raw || (s = strflush(val)) == NULL)
			prt_dec(0, val);
		else
			outstring(s);
		break;

	/* tty ioctl()s */
	case TCSBRK:
	case TCXONC:
	case TCFLSH:
	case TCDSET:
		prt_dec(0, val);
		break;

	default:
		prt_hex(0, val);
		break;
	}
}

static void
prt_fcn(int raw, long val)	/* print fcntl code */
{
	const char *s = raw? NULL : fcntlname(val);

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_s86(int raw, long val)	/* print sysi86 code */
{

	const char *s = raw? NULL : si86name(val);

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_uts(int raw, long val)	/* print utssys code */
{
	const char *s = raw? NULL : utscode(val);

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_msc(int raw, long val)	/* print msgsys command */
{
	const char *s = raw? NULL : msgcmd(val);

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_msf(int raw, long val)	/* print msgsys flags */
{
	const char *s = raw? NULL : msgflags((int)val);

	if (s == NULL)
		prt_oct(0, val);
	else
		outstring(s);
}

static void
prt_smc(int raw, long val)	/* print semsys command */
{
	const char *s = raw? NULL : semcmd(val);

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_sef(int raw, long val)	/* print semsys flags */
{
	const char *s = raw? NULL : semflags((int)val);

	if (s == NULL)
		prt_oct(0, val);
	else
		outstring(s);
}

static void
prt_shc(int raw, long val)	/* print shmsys command */
{
	const char *s = raw? NULL : shmcmd(val);

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_shf(int raw, long val)	/* print shmsys flags */
{
	const char *s = raw? NULL : shmflags((int)val);

	if (s == NULL)
		prt_oct(0, val);
	else
		outstring(s);
}

static void
prt_sfs(int raw, long val)	/* print sysfs code */
{
	const char *s = raw? NULL : sfsname(val);

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_opn(int raw, long val)	/* print open code */
{
	const char *s = raw? NULL : openarg(val);

	if (s == NULL)
		prt_oct(0, val);
	else
		outstring(s);
}

static void
prt_sig(int raw, long val)	/* print signal name plus flags */
{
	const char *s = raw? NULL : sigarg((int)val);

	if (s == NULL)
		prt_hex(0, val);
	else
		outstring(s);
}

static void
prt_six(int raw, long val)	/* print signal name, masked with SIGNO_MASK */
{
	const char *s = raw? NULL : sigarg((int)val & SIGNO_MASK);

	if (s == NULL)
		prt_hex(0, val);
	else
		outstring(s);
}

static void
prt_act(int raw, long val)	/* print signal action value */
{
	const char *s;

	if (raw)
		s = NULL;
	else if (val == (int)SIG_DFL)
		s = "SIG_DFL";
	else if (val == (int)SIG_IGN)
		s = "SIG_IGN";
	else if (val == (int)SIG_HOLD)
		s = "SIG_HOLD";
	else
		s = NULL;

	if (s == NULL)
		prt_hex(0, val);
	else
		outstring(s);
}

static void
prt_smf(int raw, long val)	/* print streams message flags */
{
	switch (val) {
	case 0:
		prt_dec(0, val);
		break;
	case RS_HIPRI:
		if (raw)
			prt_hhx(0, val);
		else
			outstring("RS_HIPRI");
		break;
	default:
		prt_hhx(0, val);
		break;
	}
}

static void
prt_plk(int raw, long val)	/* print plock code */
{
	const char *s = raw? NULL : plockname(val);

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_mtf(int raw, long val)	/* print mount flags */
{
	const char *s = raw? NULL : mountflags(val);

	if (s == NULL)
		prt_hex(0, val);
	else
		outstring(s);
}

static void
prt_mft(int raw, long val)	/* print mount file system type */
{
	if (val >= 0 && val < 256)
		prt_dec(0, val);
	else if (raw)
		prt_hex(0, val);
	else
		prt_stg(raw, val);
}

#define	ISREAD(code) \
	((code) == SYS_read || (code) == SYS_pread || (code) == SYS_pread64 || \
	(code) == SYS_recv || (code) == SYS_recvfrom)
#define	ISWRITE(code) \
	((code) == SYS_write || (code) == SYS_pwrite || \
	(code) == SYS_pwrite64 || (code) == SYS_send || (code) == SYS_sendto)
static void
prt_iob(int raw, long val)  /* print contents of read() or write() I/O buffer */
{
	int syscall = Pstatus(Proc)->pr_lwp.pr_what;
	int fdp1 = sys_args[0]+1;
	ssize_t nbyte = ISWRITE(syscall)? sys_args[2] :
		((Errno || slowmode)? 0 : Rval1);
	int elsewhere = FALSE;		/* TRUE iff dumped elsewhere */
	char buffer[IOBSIZE];

	iob_buf[0] = '\0';

	if (Pstatus(Proc)->pr_lwp.pr_why == PR_SYSEXIT && nbyte > IOBSIZE) {
		if (ISREAD(syscall))
			elsewhere = prismember(&readfd, fdp1);
		else
			elsewhere = prismember(&writefd, fdp1);
	}

	if (nbyte <= 0 || elsewhere)
		prt_hex(0, val);
	else {
		int nb = nbyte > IOBSIZE? IOBSIZE : nbyte;

		if (Pread(Proc, buffer, (size_t)nb, (long)val) != nb)
			prt_hex(0, val);
		else {
			iob_buf[0] = '"';
			showbytes(buffer, nb, iob_buf+1);
			(void) strcat(iob_buf,
				(nb == nbyte)?
				    (const char *)"\"" : (const char *)"\"..");
			if (raw)
				prt_hex(0, val);
			else
				outstring(iob_buf);
		}
	}
}
#undef	ISREAD
#undef	ISWRITE

static void
prt_idt(int raw, long val)	/* print idtype_t, waitid() argument */
{
	const char *s = raw? NULL : idtype_enum(val);

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_wop(int raw, long val)	/* print waitid() options */
{
	const char *s = raw? NULL : woptions((int)val);

	if (s == NULL)
		prt_oct(0, val);
	else
		outstring(s);
}

static void
prt_whn(int raw, long val)	/* print lseek() whence argument */
{
	const char *s = raw? NULL : whencearg(val);

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

/*ARGSUSED*/
static void
prt_spm(int raw, long val)	/* print sigprocmask argument */
{
	const char *s = NULL;

	if (!raw) {
		switch (val) {
		case SIG_BLOCK:		s = "SIG_BLOCK";	break;
		case SIG_UNBLOCK:	s = "SIG_UNBLOCK";	break;
		case SIG_SETMASK:	s = "SIG_SETMASK";	break;
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static const char *
mmap_protect(long arg)
{
	char *str = code_buf;

	if (arg & ~(PROT_READ|PROT_WRITE|PROT_EXEC))
		return ((char *)NULL);

	if (arg == PROT_NONE)
		return ("PROT_NONE");

	*str = '\0';
	if (arg & PROT_READ)
		(void) strcat(str, "|PROT_READ");
	if (arg & PROT_WRITE)
		(void) strcat(str, "|PROT_WRITE");
	if (arg & PROT_EXEC)
		(void) strcat(str, "|PROT_EXEC");
	return ((const char *)(str+1));
}

static const char *
mmap_type(long arg)
{
	char *str = code_buf;

	switch (arg&MAP_TYPE) {
	case MAP_SHARED:
		(void) strcpy(str, "MAP_SHARED");
		break;
	case MAP_PRIVATE:
		(void) strcpy(str, "MAP_PRIVATE");
		break;
	default:
		(void) sprintf(str, "%ld", arg&MAP_TYPE);
		break;
	}

	arg &= ~(_MAP_NEW|MAP_TYPE);

	if (arg & ~(MAP_FIXED|MAP_RENAME|MAP_NORESERVE|MAP_ANON))
		(void) sprintf(str+strlen(str), "|0x%lX", arg);
	else {
		if (arg & MAP_FIXED)
			(void) strcat(str, "|MAP_FIXED");
		if (arg & MAP_RENAME)
			(void) strcat(str, "|MAP_RENAME");
		if (arg & MAP_NORESERVE)
			(void) strcat(str, "|MAP_NORESERVE");
		if (arg & MAP_ANON)
			(void) strcat(str, "|MAP_ANON");
	}

	return ((const char *)str);
}

static void
prt_mpr(int raw, long val)	/* print mmap()/mprotect() flags */
{
	const char *s = raw? NULL : mmap_protect(val);

	if (s == NULL)
		prt_hhx(0, val);
	else
		outstring(s);
}

static void
prt_mty(int raw, long val)	/* print mmap() mapping type flags */
{
	const char *s = raw? NULL : mmap_type(val);

	if (s == NULL)
		prt_hhx(0, val);
	else
		outstring(s);
}

/*ARGSUSED*/
static void
prt_mcf(int raw, long val)	/* print memcntl() function */
{
	const char *s = NULL;

	if (!raw) {
		switch (val) {
		case MC_SYNC:		s = "MC_SYNC";		break;
		case MC_LOCK:		s = "MC_LOCK";		break;
		case MC_UNLOCK:		s = "MC_UNLOCK";	break;
		case MC_ADVISE:		s = "MC_ADVISE";	break;
		case MC_LOCKAS:		s = "MC_LOCKAS";	break;
		case MC_UNLOCKAS:	s = "MC_UNLOCKAS";	break;
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_mad(int raw, long val)	/* print madvise() argument */
{
	const char *s = NULL;

	if (!raw) {
		switch (val) {
		case MADV_NORMAL:	s = "MADV_NORMAL";	break;
		case MADV_RANDOM:	s = "MADV_RANDOM";	break;
		case MADV_SEQUENTIAL:	s = "MADV_SEQUENTIAL";	break;
		case MADV_WILLNEED:	s = "MADV_WILLNEED";	break;
		case MADV_DONTNEED:	s = "MADV_DONTNEED";	break;
		case MADV_FREE:		s = "MADV_FREE";	break;
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_mc4(int raw, long val)	/* print memcntl() (fourth) argument */
{
	if (val == 0)
		prt_dec(0, val);
	else if (raw)
		prt_hhx(0, val);
	else {
		char *s = NULL;

		switch (sys_args[2]) { /* cheating -- look at memcntl func */
		case MC_ADVISE:
			prt_mad(0, val);
			return;

		case MC_SYNC:
			if ((val & ~(MS_SYNC|MS_ASYNC|MS_INVALIDATE)) == 0) {
				*(s = code_buf) = '\0';
				if (val & MS_SYNC)
					(void) strcat(s, "|MS_SYNC");
				if (val & MS_ASYNC)
					(void) strcat(s, "|MS_ASYNC");
				if (val & MS_INVALIDATE)
					(void) strcat(s, "|MS_INVALIDATE");
			}
			break;

		case MC_LOCKAS:
		case MC_UNLOCKAS:
			if ((val & ~(MCL_CURRENT|MCL_FUTURE)) == 0) {
				*(s = code_buf) = '\0';
				if (val & MCL_CURRENT)
					(void) strcat(s, "|MCL_CURRENT");
				if (val & MCL_FUTURE)
					(void) strcat(s, "|MCL_FUTURE");
			}
			break;
		}

		if (s == NULL)
			prt_hhx(0, val);
		else
			outstring(++s);
	}
}

static void
prt_mc5(int raw, long val)	/* print memcntl() (fifth) argument */
{
	char *s;

	if (val == 0)
		prt_dec(0, val);
	else if (raw || (val & ~VALID_ATTR))
		prt_hhx(0, val);
	else {
		s = code_buf;
		*s = '\0';
		if (val & SHARED)
			(void) strcat(s, "|SHARED");
		if (val & PRIVATE)
			(void) strcat(s, "|PRIVATE");
		if (val & PROT_READ)
			(void) strcat(s, "|PROT_READ");
		if (val & PROT_WRITE)
			(void) strcat(s, "|PROT_WRITE");
		if (val & PROT_EXEC)
			(void) strcat(s, "|PROT_EXEC");
		if (*s == '\0')
			prt_hhx(0, val);
		else
			outstring(++s);
	}
}

static void
prt_ulm(int raw, long val)	/* print ulimit() argument */
{
	const char *s = NULL;

	if (!raw) {
		switch (val) {
		case UL_GFILLIM:	s = "UL_GFILLIM";	break;
		case UL_SFILLIM:	s = "UL_SFILLIM";	break;
		case UL_GMEMLIM:	s = "UL_GMEMLIM";	break;
		case UL_GDESLIM:	s = "UL_GDESLIM";	break;
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_rlm(int raw, long val)	/* print get/setrlimit() argument */
{
	const char *s = NULL;

	if (!raw) {
		switch (val) {
		case RLIMIT_CPU:	s = "RLIMIT_CPU";	break;
		case RLIMIT_FSIZE:	s = "RLIMIT_FSIZE";	break;
		case RLIMIT_DATA:	s = "RLIMIT_DATA";	break;
		case RLIMIT_STACK:	s = "RLIMIT_STACK";	break;
		case RLIMIT_CORE:	s = "RLIMIT_CORE";	break;
		case RLIMIT_NOFILE:	s = "RLIMIT_NOFILE";	break;
		case RLIMIT_VMEM:	s = "RLIMIT_VMEM";	break;
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_cnf(int raw, long val)	/* print sysconfig code */
{
	const char *s = raw? NULL : sconfname(val);

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_inf(int raw, long val)	/* print sysinfo code */
{
	const char *s = NULL;

	if (!raw) {
		switch (val) {
		case SI_SYSNAME:	s = "SI_SYSNAME";	break;
		case SI_HOSTNAME:	s = "SI_HOSTNAME";	break;
		case SI_RELEASE:	s = "SI_RELEASE";	break;
		case SI_VERSION:	s = "SI_VERSION";	break;
		case SI_MACHINE:	s = "SI_MACHINE";	break;
		case SI_ARCHITECTURE:	s = "SI_ARCHITECTURE";	break;
		case SI_HW_SERIAL:	s = "SI_HW_SERIAL";	break;
		case SI_HW_PROVIDER:	s = "SI_HW_PROVIDER";	break;
		case SI_SRPC_DOMAIN:	s = "SI_SRPC_DOMAIN";	break;
		case SI_SET_HOSTNAME:	s = "SI_SET_HOSTNAME";	break;
		case SI_SET_SRPC_DOMAIN: s = "SI_SET_SRPC_DOMAIN"; break;
		case SI_PLATFORM:	s = "SI_PLATFORM";	break;
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_ptc(int raw, long val)	/* print pathconf code */
{
	const char *s = raw? NULL : pathconfname(val);

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_fui(int raw, long val)	/* print fusers() input argument */
{
	const char *s = raw? NULL : fuiname(val);

	if (s == NULL)
		prt_hhx(0, val);
	else
		outstring(s);
}

static void
prt_lwf(int raw, long val)	/* print lwp_create() flags */
{
	char *s;

	if (val == 0)
		prt_dec(0, val);
	else if (raw || (val & ~(LWP_DETACHED|LWP_SUSPENDED|__LWP_ASLWP)))
		prt_hhx(0, val);
	else {
		s = code_buf;
		*s = '\0';
		if (val & LWP_DETACHED)
			(void) strcat(s, "|LWP_DETACHED");
		if (val & LWP_SUSPENDED)
			(void) strcat(s, "|LWP_SUSPENDED");
		if (val & __LWP_ASLWP)
			(void) strcat(s, "|__LWP_ASLWP");
		outstring(++s);
	}
}

static void
prt_itm(int raw, long val)	/* print [get|set]itimer() arg */
{
	const char *s = NULL;

	if (!raw) {
		switch (val) {
		case ITIMER_REAL:	s = "ITIMER_REAL";	break;
		case ITIMER_VIRTUAL:	s = "ITIMER_VIRTUAL";	break;
		case ITIMER_PROF:	s = "ITIMER_PROF";	break;
#ifdef ITIMER_REALPROF
		case ITIMER_REALPROF:	s = "ITIMER_REALPROF";	break;
#endif
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_vtr(int raw, long val)	/* print vtrace() code */
{
	const char *s = NULL;

	if (!raw) {
		switch (val) {
		case VTR_INIT:		s = "VTR_INIT";		break;
		case VTR_FILE:		s = "VTR_FILE";		break;
		case VTR_EVENTMAP:	s = "VTR_EVENTMAP";	break;
		case VTR_EVENT:		s = "VTR_EVENT";	break;
		case VTR_START:		s = "VTR_START";	break;
		case VTR_PAUSE:		s = "VTR_PAUSE";	break;
		case VTR_RESUME:	s = "VTR_RESUME";	break;
		case VTR_INFO:		s = "VTR_INFO";		break;
		case VTR_FLUSH:		s = "VTR_FLUSH";	break;
		case VTR_RESET:		s = "VTR_RESET";	break;
		case VTR_TEST:		s = "VTR_TEST";		break;
		case VTR_PROCESS:	s = "VTR_PROCESS";	break;
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_mod(int raw, long val)	/* print modctl() code */
{
	const char *s = NULL;

	if (!raw) {
		switch (val) {
		case MODLOAD:		s = "MODLOAD";		break;
		case MODUNLOAD:		s = "MODUNLOAD";	break;
		case MODINFO:		s = "MODINFO";		break;
		case MODRESERVED:	s = "MODRESERVED";	break;
		case MODCONFIG:		s = "MODCONFIG";	break;
		case MODADDMAJBIND:	s = "MODADDMAJBIND";	break;
		case MODGETPATH:	s = "MODGETPATH";	break;
		case MODGETPATHLEN:	s = "MODGETPATHLEN";	break;
		case MODREADSYSBIND:	s = "MODREADSYSBIND";	break;
		case MODGETMAJBIND:	s = "MODGETMAJBIND";	break;
		case MODGETNAME:	s = "MODGETNAME";	break;
		case MODSIZEOF_DEVID:	s = "MODSIZEOF_DEVID";	break;
		case MODGETDEVID:	s = "MODGETDEVID";	break;
		case MODSIZEOF_MINORNAME: s = "MODSIZEOF_MINORNAME"; break;
		case MODGETMINORNAME:	s = "MODGETMINORNAME";	break;
		case MODGETFBNAME:	s = "MODGETFBNAME";	break;
		case MODEVENTS:		s = "MODEVENTS";	break;
		case MODREREADDACF:	s = "MODREREADDACF";	break;
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_acl(int raw, long val)	/* print acl() code */
{
	const char *s = NULL;

	if (!raw) {
		switch (val) {
		case GETACL:		s = "GETACL";		break;
		case SETACL:		s = "SETACL";		break;
		case GETACLCNT:		s = "GETACLCNT";	break;
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_aio(int raw, long val)	/* print kaio() code */
{
	const char *s = NULL;
	char buf[32];

	if (!raw) {
		switch (val & ~AIO_POLL_BIT) {
		case AIOREAD:		s = "AIOREAD";		break;
		case AIOWRITE:		s = "AIOWRITE";		break;
		case AIOWAIT:		s = "AIOWAIT";		break;
		case AIOCANCEL:		s = "AIOCANCEL";	break;
		case AIONOTIFY:		s = "AIONOTIFY";	break;
		}
		if (s != NULL && (val & AIO_POLL_BIT)) {
			(void) strcpy(buf, s);
			(void) strcat(buf, "|AIO_POLL_BIT");
			s = (const char *)buf;
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_aud(int raw, long val)	/* print auditsys() code */
{
	const char *s = NULL;

	if (!raw) {
		switch (val) {
		case BSM_GETAUID:	s = "BSM_GETAUID";	break;
		case BSM_SETAUID:	s = "BSM_SETAUID";	break;
		case BSM_GETAUDIT:	s = "BSM_GETAUDIT";	break;
		case BSM_SETAUDIT:	s = "BSM_SETAUDIT";	break;
		case BSM_GETUSERAUDIT:	s = "BSM_GETUSERAUDIT";	break;
		case BSM_SETUSERAUDIT:	s = "BSM_SETUSERAUDIT";	break;
		case BSM_AUDIT:		s = "BSM_AUDIT";	break;
		case BSM_AUDITUSER:	s = "BSM_AUDITUSER";	break;
		case BSM_AUDITSVC:	s = "BSM_AUDITSVC";	break;
		case BSM_AUDITON:	s = "BSM_AUDITON";	break;
		case BSM_AUDITCTL:	s = "BSM_AUDITCTL";	break;
		case BSM_GETKERNSTATE:	s = "BSM_GETKERNSTATE";	break;
		case BSM_SETKERNSTATE:	s = "BSM_SETKERNSTATE";	break;
		case BSM_GETPORTAUDIT:	s = "BSM_GETPORTAUDIT";	break;
		case BSM_REVOKE:	s = "BSM_REVOKE";	break;
		case BSM_AUDITSTAT:	s = "BSM_AUDITSTAT";	break;
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_sac(int raw, long val)	/* print schedctl() flags */
{
	char *s;

	if (val == 0)
		prt_dec(0, val);
	else if (raw || (val &
	    ~(SC_STATE|SC_BLOCK|SC_PRIORITY|SC_PREEMPT|SC_DOOR)))
		prt_hhx(0, val);
	else {
		s = code_buf;
		*s = '\0';
		if (val & SC_STATE)
			(void) strcat(s, "|SC_STATE");
		if (val & SC_BLOCK)
			(void) strcat(s, "|SC_BLOCK");
		if (val & SC_PRIORITY)
			(void) strcat(s, "|SC_PRIORITY");
		if (val & SC_PREEMPT)
			(void) strcat(s, "|SC_PREEMPT");
		if (val & SC_DOOR)
			(void) strcat(s, "|SC_DOOR");
		outstring(++s);
	}
}

static void
prt_cor(int raw, long val)	/* print corectl() subcode */
{
	const char *s = NULL;

	if (!raw) {
		switch (val) {
		case CC_SET_OPTIONS:	s = "CC_SET_OPTIONS";		break;
		case CC_GET_OPTIONS:	s = "CC_GET_OPTIONS";		break;
		case CC_SET_GLOBAL_PATH: s = "CC_SET_GLOBAL_PATH";	break;
		case CC_GET_GLOBAL_PATH: s = "CC_GET_GLOBAL_PATH";	break;
		case CC_SET_PROCESS_PATH: s = "CC_SET_PROCESS_PATH";	break;
		case CC_GET_PROCESS_PATH: s = "CC_GET_PROCESS_PATH";	break;
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_cco(int raw, long val)	/* print corectl() options */
{
	char *s;

	if (val == 0)
		prt_dec(0, val);
	else if (raw || (val & ~CC_OPTIONS))
		prt_hhx(0, val);
	else {
		s = code_buf;
		*s = '\0';
		if (val & CC_GLOBAL_PATH)
			(void) strcat(s, "|CC_GLOBAL_PATH");
		if (val & CC_PROCESS_PATH)
			(void) strcat(s, "|CC_PROCESS_PATH");
		if (val & CC_GLOBAL_SETID)
			(void) strcat(s, "|CC_GLOBAL_SETID");
		if (val & CC_PROCESS_SETID)
			(void) strcat(s, "|CC_PROCESS_SETID");
		if (val & CC_GLOBAL_LOG)
			(void) strcat(s, "|CC_GLOBAL_LOG");
		if (*s == '\0')
			prt_hhx(0, val);
		else
			outstring(++s);
	}
}

static void
prt_cpc(int raw, long val)	/* print cpc() subcode */
{
	const char *s = NULL;

	if (!raw) {
		switch (val) {
		case CPC_BIND_EVENT:	s = "CPC_BIND_EVENT";	break;
		case CPC_TAKE_SAMPLE:	s = "CPC_TAKE_SAMPLE";	break;
		case CPC_USR_EVENTS:	s = "CPC_USR_EVENTS";	break;
		case CPC_SYS_EVENTS:	s = "CPC_SYS_EVENTS";	break;
		case CPC_INVALIDATE:	s = "CPC_INVALIDATE";	break;
		case CPC_RELE:		s = "CPC_RELE";		break;
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

void
outstring(const char *s)
{
	int len = strlen(s);

	GROW(len);
	(void) strcpy(sys_string+sys_leng, s);
	sys_leng += len;
}

static void
grow(int nbyte)		/* reallocate format buffer if necessary */
{
	while (sys_leng+nbyte >= sys_ssize) {
		sys_string = realloc(sys_string, sys_ssize *= 2);
		if (sys_string == NULL)
			abend("cannot reallocate format buffer", 0);
	}
}

static void
prt_clc(int raw, long val)
{
	const char *s = NULL;

	if (!raw) {
		switch (val) {
		case CL_INITIALIZE:	s = "CL_INITIALIZE";	break;
		case CL_CONFIG:		s = "CL_CONFIG";	break;
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

static void
prt_clf(int raw, long val)
{
	const char *s = NULL;

	if (!raw) {
		switch (sys_args[0]) {
		case CL_CONFIG:
			switch (sys_args[1]) {
			case CL_NODEID:
				s = "CL_NODEID";		break;
			case CL_HIGHEST_NODEID:
				s = "CL_HIGHEST_NODEID";	break;
			}
			break;
		case CL_INITIALIZE:
			switch (sys_args[1]) {
			case CL_GET_BOOTFLAG:
				s = "CL_GET_BOOTFLAG";		break;
			}
			break;
		}
	}

	if (s == NULL)
		prt_dec(0, val);
	else
		outstring(s);
}

/*
 * Array of pointers to print functions, one for each format.
 */
void (* const Print[])() = {
	prt_nov,	/* NOV -- no value */
	prt_dec,	/* DEC -- print value in decimal */
	prt_oct,	/* OCT -- print value in octal */
	prt_hex,	/* HEX -- print value in hexadecimal */
	prt_dex,	/* DEX -- print value in hexadecimal if big enough */
	prt_stg,	/* STG -- print value as string */
	prt_ioc,	/* IOC -- print ioctl code */
	prt_fcn,	/* FCN -- print fcntl code */
	prt_s86,	/* S86 -- print sysi86 code */
	prt_uts,	/* UTS -- print utssys code */
	prt_opn,	/* OPN -- print open code */
	prt_sig,	/* SIG -- print signal name plus flags */
	prt_act,	/* ACT -- print signal action value */
	prt_msc,	/* MSC -- print msgsys command */
	prt_msf,	/* MSF -- print msgsys flags */
	prt_smc,	/* SMC -- print semsys command */
	prt_sef,	/* SEF -- print semsys flags */
	prt_shc,	/* SHC -- print shmsys command */
	prt_shf,	/* SHF -- print shmsys flags */
	prt_plk,	/* PLK -- print plock code */
	prt_sfs,	/* SFS -- print sysfs code */
	prt_rst,	/* RST -- print string returned by syscall */
	prt_smf,	/* SMF -- print streams message flags */
	prt_ioa,	/* IOA -- print ioctl argument */
	prt_six,	/* SIX -- print signal, masked with SIGNO_MASK */
	prt_mtf,	/* MTF -- print mount flags */
	prt_mft,	/* MFT -- print mount file system type */
	prt_iob,	/* IOB -- print contents of I/O buffer */
	prt_hhx,	/* HHX -- print value in hexadecimal (half size) */
	prt_wop,	/* WOP -- print waitsys() options */
	prt_spm,	/* SPM -- print sigprocmask argument */
	prt_rlk,	/* RLK -- print readlink buffer */
	prt_mpr,	/* MPR -- print mmap()/mprotect() flags */
	prt_mty,	/* MTY -- print mmap() mapping type flags */
	prt_mcf,	/* MCF -- print memcntl() function */
	prt_mc4,	/* MC4 -- print memcntl() (fourth) argument */
	prt_mc5,	/* MC5 -- print memcntl() (fifth) argument */
	prt_mad,	/* MAD -- print madvise() argument */
	prt_ulm,	/* ULM -- print ulimit() argument */
	prt_rlm,	/* RLM -- print get/setrlimit() argument */
	prt_cnf,	/* CNF -- print sysconfig() argument */
	prt_inf,	/* INF -- print sysinfo() argument */
	prt_ptc,	/* PTC -- print pathconf/fpathconf() argument */
	prt_fui,	/* FUI -- print fusers() input argument */
	prt_idt,	/* IDT -- print idtype_t, waitid() argument */
	prt_lwf,	/* LWF -- print lwp_create() flags */
	prt_itm,	/* ITM -- print [get|set]itimer() arg */
	prt_llo,	/* LLO -- print long long offset arg */
	prt_vtr,	/* VTR -- print vtrace() code */
	prt_mod,	/* MOD -- print modctl() subcode */
	prt_whn,	/* WHN -- print lseek() whence arguiment */
	prt_acl,	/* ACL -- print acl() code */
	prt_aio,	/* AIO -- print kaio() code */
	prt_aud,	/* AUD -- print auditsys() code */
	prt_sac,	/* SAC -- print schedctl() flags */
	prt_uns,	/* DEC -- print value in unsigned decimal */
	prt_clc,	/* CLC -- print cladm command argument */
	prt_clf,	/* CLF -- print cladm flag argument */
	prt_cor,	/* COR -- print corectl() subcode */
	prt_cco,	/* CCO -- print corectl() options */
	prt_cpc,	/* CPC -- print cpc() subcode */
	prt_dec,	/* HID -- hidden argument, not normally called */
};
