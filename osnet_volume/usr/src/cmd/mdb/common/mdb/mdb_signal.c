/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_signal.c	1.1	99/08/11 SMI"

#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <mdb/mdb_signal.h>
#include <mdb/mdb_debug.h>

static mdb_signal_f *sig_handlers[NSIG];
static void *sig_data[NSIG];

static void
sig_stub(int sig, siginfo_t *sip, void *ucp)
{
	sig_handlers[sig](sig, sip, (ucontext_t *)ucp, sig_data[sig]);
}

int
mdb_signal_sethandler(int sig, mdb_signal_f *handler, void *data)
{
	struct sigaction act;
	int status;

	ASSERT(sig > 0 && sig < NSIG && sig != SIGKILL && sig != SIGSTOP);

	sig_handlers[sig] = handler;
	sig_data[sig] = data;

	if (handler == SIG_DFL || handler == SIG_IGN) {
		act.sa_handler = handler;
		act.sa_flags = SA_RESTART;
	} else {
		act.sa_handler = sig_stub;
		act.sa_flags = SA_SIGINFO | SA_RESTART;
	}

	(void) sigfillset(&act.sa_mask);

	if ((status = sigaction(sig, &act, NULL)) == 0)
		(void) mdb_signal_unblock(sig);

	return (status);
}

mdb_signal_f *
mdb_signal_gethandler(int sig, void **datap)
{
	if (datap != NULL)
		*datap = sig_data[sig];

	return (sig_handlers[sig]);
}

int
mdb_signal_raise(int sig)
{
	return (kill(getpid(), sig));
}

int
mdb_signal_pgrp(int sig)
{
	return (kill(0, sig));
}

int
mdb_signal_block(int sig)
{
	sigset_t set;

	(void) sigemptyset(&set);
	(void) sigaddset(&set, sig);

	return (sigprocmask(SIG_BLOCK, &set, NULL));
}

int
mdb_signal_unblock(int sig)
{
	sigset_t set;

	(void) sigemptyset(&set);
	(void) sigaddset(&set, sig);

	return (sigprocmask(SIG_UNBLOCK, &set, NULL));
}

int
mdb_signal_blockall(void)
{
	sigset_t set;

	(void) sigfillset(&set);
	return (sigprocmask(SIG_BLOCK, &set, NULL));
}

int
mdb_signal_unblockall(void)
{
	sigset_t set;

	(void) sigfillset(&set);
	return (sigprocmask(SIG_UNBLOCK, &set, NULL));
}
