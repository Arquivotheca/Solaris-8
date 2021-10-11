/*
 * Copyright (c) 1997-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SUNOPTIONS_H
#define	_SUNOPTIONS_H

#pragma ident	"@(#)sunoptions.h	1.12	99/04/14 SMI"

/*
 * The following options are PP flags available in the reference implementation
 * but do not get compiled due to the elimination of the options.h file
 * This section selectively reintroduces them
 */

#define	HAVE_GETRUSAGE

/*
 * The following options are PP flags introduced as part of the Sun/Solaris
 * port.
 */

/* We may have to pull this out */
#define	SUNW_LIBNSL	/* conflicts for inet_addr, inet_ntoa */
#define	SUNW_LIBC	/* conflicts for setnetgrent, endnetgrent et-al */

/* Additions for Solaris 2 */
#define	SUNW_INITCHKIF	/* Check if any non-loopback interface is up */
#define	SUNW_DOMAINFROMNIS	/* Default domain name from NIS/NIS+ */
#define	SUNW_SETFILELIMIT	/* Set soft FD limit == hard limit */
#define	USELOOPBACK	/* Resolver library defaults to 127.0.0.1 */
#define	SUNW_CONFCHECK	/* Abort quickly if no /etc/resolv.conf or local */
			/* named */
#define	SUNW_AREWEINNAMED	/* Override _confcheck if proc is in.named */
#define	SUNW_DYNIFBUF	/* Dynamic allocation of buffer for SIOCGIFCONF ioctl */
#define	SUNW_LOGLEVEL	/* Enable syslog filtering via "limit loglevel n" */
#define	SUNW_OPENFDOFFSET	/* Open non-stdio fd:s with offset */
#define	SUNW_POLL	/* Use poll(2) instead of select(3) */
#define	SUNW_SYNONYMS	/* Include synonyms.h (libresolv) */
#define	SUNW_CONNECTTIMEOUT	/* Configurable libresolv retrans and retry */
#define	SUNW_HOSTS_FALLBACK	/* Configurable /etc/hosts fallback */
#define	SUNW_LISTEN_BACKLOG	/* Configurable listen(3N) backlog (named) */
#define	SUNW_EARLY_NS_INIT	/* Do ns_init() early during startup (named) */
#define	SUNW_REJECT_BOGUS_H_LENGTH	/* (libresolv) */
#define	SUNW_HNOK_UNDERSCORE	/* Allow underscore in hostnames (libresolv) */
#define	SUNW_MT_RESOLVER	/* MT hot extensions (libresolv) */
#define	SUNW_QSTREAM_CLEANUP	/* Avoid using free()d struct qstreams */
#define	SUNW_CANNOT_TRUST_POLL	/* poll(2) makes false promises */
#define	SUNW_SETHERRNO		/* ISC does not set h_errno in gethostbyname */
#define	SUNW_RECV_ECONNREFUSED	/* Mark nameserver bad if we receive refusal */
#define	SUNW_TRY_CONNECT_ALL	/* May try UDP connect() to any nameserver */
/* Work around C 4.2 compiler bug for x86 */
#if defined(__i386)
#define	SUNW_X86_COMPILER_BUG_CONST
#endif

/* If compiling an MT warm libresolv, we also need reentrancy */
#if	defined(SUNW_MT_RESOLVER) && !defined(_REENTRANT)
#define	_REENTRANT
#endif

/* SUNW_AREWEINNAMED and SUNW_CONFCHECCK are mutually inclusive */
#if	defined(SUNW_AREWEINNAMED) && !defined(SUNW_CONFCHECK)
#define	SUNW_CONFCHECK
#endif

/* End additions for Solaris 2 */

#endif /* _SUNOPTIONS_H */
