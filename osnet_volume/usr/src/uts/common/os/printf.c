/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)printf.c	1.6	99/11/24 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/inline.h>
#include <sys/varargs.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/syslog.h>
#include <sys/log.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/session.h>
#include <sys/stream.h>
#include <sys/kmem.h>
#include <sys/kobj.h>
#include <sys/atomic.h>
#include <sys/console.h>
#include <sys/cpuvar.h>
#include <sys/modctl.h>
#include <sys/reboot.h>
#include <sys/debug.h>
#include <sys/panic.h>
#include <sys/spl.h>

/*
 * In some debugging situations it's useful to log all messages to panicbuf,
 * which is persistent across reboots (on most platforms).  The range
 * panicbuf[panicbuf_log..PANICBUFSIZE-1] may be used for this purpose.
 * By default, panicbuf_log == PANICBUFSIZE and no messages are logged.
 * To enable panicbuf logging, set panicbuf_log to a small value, say 1K;
 * this will reserve 1K for panic information and 7K for message logging.
 */
uint32_t panicbuf_log = PANICBUFSIZE;
uint32_t panicbuf_index = PANICBUFSIZE;

static int aask, aok;
static int ce_to_sl[CE_IGNORE] = { SL_NOTE, SL_NOTE, SL_WARN, SL_FATAL };
static char ce_prefix[CE_IGNORE][10] = { "", "NOTICE: ", "WARNING: ", "" };
static char ce_suffix[CE_IGNORE][2] = { "", "\n", "\n", "" };

static void
cprintf(const char *fmt, va_list adx, int sl, const char *prefix,
	const char *suffix, void *site, int mid, int sid, int level)
{
	uint32_t msgid;
	size_t bufsize = LOG_MSGSIZE;
	char buf[LOG_MSGSIZE];
	char *bufp = buf;
	char *body, *msgp, *bufend;
	mblk_t *mp;
	int s, on_intr;
	size_t len;

	s = splhi();
	on_intr = CPU->cpu_on_intr ||
	    (interrupts_unleashed && (spltoipl(s) > LOCK_LEVEL));
	splx(s);

	STRLOG_MAKE_MSGID(fmt, msgid);

	if (strchr("^!?", fmt[0]) != NULL) {
		if (fmt[0] == '^')
			sl |= SL_CONSONLY;
		else if (fmt[0] == '!' ||
		    (prefix[0] == '\0' && !(boothowto & RB_VERBOSE)))
			sl = (sl & ~(SL_USER | SL_NOTE)) | SL_LOGONLY;
		fmt++;
	}

	if ((sl & SL_USER) && (MUTEX_HELD(&pidlock) || on_intr))
		sl = (sl & ~(SL_USER | SL_LOGONLY)) | SL_CONSOLE;

retry:
	bufend = bufp + bufsize;
	msgp = bufp;
	body = msgp += snprintf(msgp, bufend - msgp,
	    "%s: [ID %u FACILITY_AND_PRIORITY] ",
	    mod_containing_pc(site), msgid);
	msgp += snprintf(msgp, bufend - msgp, prefix);
	msgp += vsnprintf(msgp, bufend - msgp, fmt, adx);
	msgp += snprintf(msgp, bufend - msgp, suffix);
	len = strlen(body);

	if (((sl & SL_CONSONLY) && panicstr) || log_active == 0) {
		console_printf("%s", body);
		goto out;
	}

	if (msgp - bufp >= bufsize && !on_intr) {
		ASSERT(bufp == buf);
		bufsize = msgp - bufp + 1;
		bufp = kmem_alloc(bufsize, KM_NOSLEEP);
		if (bufp != NULL)
			goto retry;
		bufsize = LOG_MSGSIZE;
		bufp = buf;
	}

	mp = log_makemsg(mid, sid, level, sl, LOG_KERN, bufp,
	    MIN(bufsize, msgp - bufp + 1), on_intr);
	if (mp == NULL) {
		if ((sl & (SL_CONSOLE | SL_LOGONLY)) == SL_CONSOLE && !on_intr)
			console_printf("%s", body);
		goto out;
	}

	if (sl & SL_USER) {
		ssize_t resid;
		sess_t *sessp;

		mutex_enter(&pidlock);
		sessp = curproc->p_sessp;
		SESS_HOLD(sessp);
		TTY_HOLD(sessp);
		mutex_exit(&pidlock);
		if (sessp->s_vp)
			(void) vn_rdwr(UIO_WRITE, sessp->s_vp,
			    body, len, 0LL, UIO_SYSSPACE,
			    FAPPEND, (rlim64_t)LOG_HIWAT, kcred, &resid);
		mutex_enter(&pidlock);
		TTY_RELE(sessp);
		SESS_RELE(sessp);
		mutex_exit(&pidlock);
	}

	if (on_intr && !panicstr) {
		(void) putq(log_intrq, mp);
		softcall((void (*)(void *))log_flushq, log_intrq);
	} else {
		log_sendmsg(mp);
	}
out:
	if (panicbuf_log + len < PANICBUFSIZE) {
		uint32_t old, new;
		do {
			old = panicbuf_index;
			new = old + len;
			if (new >= PANICBUFSIZE)
				new = panicbuf_log + len;
		} while (cas32(&panicbuf_index, old, new) != old);
		bcopy(body, &panicbuf[new - len], len);
	}
	if (bufp != buf)
		kmem_free(bufp, bufsize);
}

void
vprintf(const char *fmt, va_list adx)
{
	cprintf(fmt, adx, SL_CONSOLE | SL_NOTE, "", "", caller(), 0, 0, 0);
}

/*PRINTFLIKE1*/
void
printf(const char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	cprintf(fmt, adx, SL_CONSOLE | SL_NOTE, "", "", caller(), 0, 0, 0);
	va_end(adx);
}

void
vuprintf(const char *fmt, va_list adx)
{
	cprintf(fmt, adx, SL_CONSOLE | SL_LOGONLY | SL_USER | SL_NOTE,
	    "", "", caller(), 0, 0, 0);
}

/*PRINTFLIKE1*/
void
uprintf(const char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	cprintf(fmt, adx, SL_CONSOLE | SL_LOGONLY | SL_USER | SL_NOTE,
	    "", "", caller(), 0, 0, 0);
	va_end(adx);
}

void
vcmn_err(int ce, const char *fmt, va_list adx)
{
	if (ce == CE_PANIC)
		vpanic(fmt, adx);
	if ((uint_t)ce < CE_IGNORE)
		cprintf(fmt, adx, ce_to_sl[ce] | SL_CONSOLE,
		    ce_prefix[ce], ce_suffix[ce], caller(), 0, 0, 0);
}

/*PRINTFLIKE2*/
void
cmn_err(int ce, const char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	if (ce == CE_PANIC)
		vpanic(fmt, adx);
	if ((uint_t)ce < CE_IGNORE)
		cprintf(fmt, adx, ce_to_sl[ce] | SL_CONSOLE,
		    ce_prefix[ce], ce_suffix[ce], caller(), 0, 0, 0);
	va_end(adx);
}

int
assfail(const char *a, const char *f, int l)
{
	if (aask)  {
		printf("ASSERTION CAUGHT: %s, file: %s, line: %d", a, f, l);
		debug_enter(NULL);
	}

	if (!aok && !panicstr)
		panic("assertion failed: %s, file: %s, line: %d", a, f, l);

	return (0);
}

int
strlog(short mid, short sid, char level, ushort_t sl, char *fmt, ...)
{
	if (sl & log_active) {
		va_list adx;
		va_start(adx, fmt);
		cprintf(fmt, adx, sl, "", "", caller(), mid, sid, level);
		va_end(adx);
	}
	return (1);
}

int
vstrlog(short mid, short sid, char level, ushort_t sl, char *fmt, va_list adx)
{
	if (sl & log_active)
		cprintf(fmt, adx, sl, "", "", caller(), mid, sid, level);
	return (1);
}
