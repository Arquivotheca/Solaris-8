/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)codes.c	1.42	99/08/15 SMI"	/* SVr4.0 1.14	*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <libproc.h>

#include <ctype.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/fstyp.h>
#ifdef i386
#include <sys/sysi86.h>
#endif /* i386 */
#include <sys/unistd.h>
#include <sys/file.h>
#include <sys/tiuser.h>
#include <sys/timod.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/termios.h>
#include <sys/termiox.h>
#include <sys/jioctl.h>
#include <sys/filio.h>
#include <fcntl.h>
#include <sys/termio.h>
#include <sys/stermio.h>
#include <sys/ttold.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/utssys.h>
#include <sys/sysconfig.h>
#include <sys/statvfs.h>
#include <sys/kstat.h>
#include <sys/audioio.h>
#include <sys/vol.h>
#include <sys/cpc_impl.h>

#include "ramdata.h"
#include "proto.h"

#define	FCNTLMIN	F_DUPFD
#define	FCNTLMAX	F_UNSHARE
static const char *const FCNTLname[] = {
	"F_DUPFD",
	"F_GETFD",
	"F_SETFD",
	"F_GETFL",
	"F_SETFL",
	"F_O_GETLK",
	"F_SETLK",
	"F_SETLKW",
	"F_CHKFL",
	"F_DUP2FD",
	"F_ALLOCSP",
	"F_FREESP",
	NULL,		/* 12 */
	NULL,		/* 13 */
	"F_GETLK",
	NULL,		/* 15 */
	NULL,		/* 16 */
	NULL,		/* 17 */
	NULL,		/* 18 */
	NULL,		/* 19 */
	NULL,		/* 20 */
	NULL,		/* 21 */
	NULL,		/* 22 */
	"F_GETOWN",
	"F_SETOWN",
	"F_REVOKE",
	"F_HASREMOTELOCKS",
	"F_FREESP64",
	NULL,		/* 28 */
	NULL,		/* 29 */
	NULL,		/* 30 */
	NULL,		/* 31 */
	NULL,		/* 32 */
	"F_GETLK64",
	"F_SETLK64",
	"F_SETLKW64",
	NULL,		/* 36 */
	NULL,		/* 37 */
	NULL,		/* 38 */
	NULL,		/* 39 */
	"F_SHARE",
	"F_UNSHARE"
};

#define	SYSFSMIN	GETFSIND
#define	SYSFSMAX	GETNFSTYP
static const char *const SYSFSname[] = {
	"GETFSIND",
	"GETFSTYP",
	"GETNFSTYP"
};

#define	PLOCKMIN	UNLOCK
#define	PLOCKMAX	DATLOCK
static const char *const PLOCKname[] = {
	"UNLOCK",
	"PROCLOCK",
	"TXTLOCK",
	NULL,
	"DATLOCK"
};

#define	SCONFMIN	_CONFIG_NGROUPS
#define	SCONFMAX	_CONFIG_STACK_PROT
static const char *const SCONFname[] = {
	"_CONFIG_NGROUPS",		/*  2 */
	"_CONFIG_CHILD_MAX",		/*  3 */
	"_CONFIG_OPEN_FILES",		/*  4 */
	"_CONFIG_POSIX_VER",		/*  5 */
	"_CONFIG_PAGESIZE",		/*  6 */
	"_CONFIG_CLK_TCK",		/*  7 */
	"_CONFIG_XOPEN_VER",		/*  8 */
	"_CONFIG_HRESCLK_TCK",		/*  9 */
	"_CONFIG_PROF_TCK",		/* 10 */
	"_CONFIG_NPROC_CONF",		/* 11 */
	"_CONFIG_NPROC_ONLN",		/* 12 */
	"_CONFIG_AIO_LISTIO_MAX",	/* 13 */
	"_CONFIG_AIO_MAX",		/* 14 */
	"_CONFIG_AIO_PRIO_DELTA_MAX",	/* 15 */
	"_CONFIG_DELAYTIMER_MAX",	/* 16 */
	"_CONFIG_MQ_OPEN_MAX",		/* 17 */
	"_CONFIG_MQ_PRIO_MAX",		/* 18 */
	"_CONFIG_RTSIG_MAX",		/* 19 */
	"_CONFIG_SEM_NSEMS_MAX",	/* 20 */
	"_CONFIG_SEM_VALUE_MAX",	/* 21 */
	"_CONFIG_SIGQUEUE_MAX",		/* 22 */
	"_CONFIG_SIGRT_MIN",		/* 23 */
	"_CONFIG_SIGRT_MAX",		/* 24 */
	"_CONFIG_TIMER_MAX",		/* 25 */
	"_CONFIG_PHYS_PAGES",		/* 26 */
	"_CONFIG_AVPHYS_PAGES",		/* 27 */
	"_CONFIG_COHERENCY",		/* 28 */
	"_CONFIG_SPLIT_CACHE",		/* 29 */
	"_CONFIG_ICACHESZ",		/* 30 */
	"_CONFIG_DCACHESZ",		/* 31 */
	"_CONFIG_ICACHELINESZ",		/* 32 */
	"_CONFIG_DCACHELINESZ",		/* 33 */
	"_CONFIG_ICACHEBLKSZ",		/* 34 */
	"_CONFIG_DCACHEBLKSZ",		/* 35 */
	"_CONFIG_DCACHETBLKSZ",		/* 36 */
	"_CONFIG_ICACHE_ASSOC",		/* 37 */
	"_CONFIG_DCACHE_ASSOC",		/* 38 */
	NULL,				/* 39 */
	NULL,				/* 40 */
	NULL,				/* 41 */
	"_CONFIG_MAXPID",		/* 42 */
	"_CONFIG_STACK_PROT"		/* 43 */
};

#define	PATHCONFMIN	_PC_LINK_MAX
#define	PATHCONFMAX	_PC_CHOWN_RESTRICTED
static const char *const PATHCONFname[] = {
	"_PC_LINK_MAX",
	"_PC_MAX_CANON",
	"_PC_MAX_INPUT",
	"_PC_NAME_MAX",
	"_PC_PATH_MAX",
	"_PC_PIPE_BUF",
	"_PC_NO_TRUNC",
	"_PC_VDISABLE",
	"_PC_CHOWN_RESTRICTED"
};

static const struct ioc {
	uint_t	code;
	const char *name;
} ioc[] = {
	{ (uint_t)TCGETA,	"TCGETA"	},
	{ (uint_t)TCSETA,	"TCSETA"	},
	{ (uint_t)TCSETAW,	"TCSETAW"	},
	{ (uint_t)TCSETAF,	"TCSETAF"	},
	{ (uint_t)TCFLSH,	"TCFLSH"	},
	{ (uint_t)TIOCKBON,	"TIOCKBON"	},
	{ (uint_t)TIOCKBOF,	"TIOCKBOF"	},
	{ (uint_t)KBENABLED,	"KBENABLED"	},
	{ (uint_t)TCGETS,	"TCGETS"	},
	{ (uint_t)TCSETS,	"TCSETS"	},
	{ (uint_t)TCSETSW,	"TCSETSW"	},
	{ (uint_t)TCSETSF,	"TCSETSF"	},
	{ (uint_t)TCXONC,	"TCXONC"	},
	{ (uint_t)TCSBRK,	"TCSBRK"	},
	{ (uint_t)TCDSET,	"TCDSET"	},
	{ (uint_t)RTS_TOG,	"RTS_TOG"	},
	{ (uint_t)TIOCSWINSZ,	"TIOCSWINSZ"	},
	{ (uint_t)TIOCGWINSZ,	"TIOCGWINSZ"	},
	{ (uint_t)TIOCGETD,	"TIOCGETD"	},
	{ (uint_t)TIOCSETD,	"TIOCSETD"	},
	{ (uint_t)TIOCHPCL,	"TIOCHPCL"	},
	{ (uint_t)TIOCGETP,	"TIOCGETP"	},
	{ (uint_t)TIOCSETP,	"TIOCSETP"	},
	{ (uint_t)TIOCSETN,	"TIOCSETN"	},
	{ (uint_t)TIOCEXCL,	"TIOCEXCL"	},
	{ (uint_t)TIOCNXCL,	"TIOCNXCL"	},
	{ (uint_t)TIOCFLUSH,	"TIOCFLUSH"	},
	{ (uint_t)TIOCSETC,	"TIOCSETC"	},
	{ (uint_t)TIOCGETC,	"TIOCGETC"	},
	{ (uint_t)TIOCGPGRP,	"TIOCGPGRP"	},
	{ (uint_t)TIOCSPGRP,	"TIOCSPGRP"	},
	{ (uint_t)TIOCGSID,	"TIOCGSID"	},
	{ (uint_t)TIOCSTI,	"TIOCSTI"	},
	{ (uint_t)TIOCSSID,	"TIOCSSID"	},
	{ (uint_t)TIOCMSET,	"TIOCMSET"	},
	{ (uint_t)TIOCMBIS,	"TIOCMBIS"	},
	{ (uint_t)TIOCMBIC,	"TIOCMBIC"	},
	{ (uint_t)TIOCMGET,	"TIOCMGET"	},
	{ (uint_t)TIOCREMOTE,	"TIOCREMOTE"	},
	{ (uint_t)TIOCSIGNAL,	"TIOCSIGNAL"	},
	{ (uint_t)TIOCSTART,	"TIOCSTART"	},
	{ (uint_t)TIOCSTOP,	"TIOCSTOP"	},
	{ (uint_t)TIOCNOTTY,	"TIOCNOTTY"	},
	{ (uint_t)TIOCOUTQ,	"TIOCOUTQ"	},
	{ (uint_t)TIOCGLTC,	"TIOCGLTC"	},
	{ (uint_t)TIOCSLTC,	"TIOCSLTC"	},
	{ (uint_t)TIOCCDTR,	"TIOCCDTR"	},
	{ (uint_t)TIOCSDTR,	"TIOCSDTR"	},
	{ (uint_t)TIOCCBRK,	"TIOCCBRK"	},
	{ (uint_t)TIOCSBRK,	"TIOCSBRK"	},
	{ (uint_t)TIOCLGET,	"TIOCLGET"	},
	{ (uint_t)TIOCLSET,	"TIOCLSET"	},
	{ (uint_t)TIOCLBIC,	"TIOCLBIC"	},
	{ (uint_t)TIOCLBIS,	"TIOCLBIS"	},
	{ (uint_t)LDOPEN,	"LDOPEN"	},
	{ (uint_t)LDCLOSE,	"LDCLOSE"	},
	{ (uint_t)LDCHG,	"LDCHG"		},
	{ (uint_t)LDGETT,	"LDGETT"	},
	{ (uint_t)LDSETT,	"LDSETT"	},
	{ (uint_t)LDSMAP,	"LDSMAP"	},
	{ (uint_t)LDGMAP,	"LDGMAP"	},
	{ (uint_t)LDNMAP,	"LDNMAP"	},
	{ (uint_t)TCGETX,	"TCGETX"	},
	{ (uint_t)TCSETX,	"TCSETX"	},
	{ (uint_t)TCSETXW,	"TCSETXW"	},
	{ (uint_t)TCSETXF,	"TCSETXF"	},
	{ (uint_t)FIORDCHK,	"FIORDCHK"	},
	{ (uint_t)FIOCLEX,	"FIOCLEX"	},
	{ (uint_t)FIONCLEX,	"FIONCLEX"	},
	{ (uint_t)FIONREAD,	"FIONREAD"	},
	{ (uint_t)FIONBIO,	"FIONBIO"	},
	{ (uint_t)FIOASYNC,	"FIOASYNC"	},
	{ (uint_t)FIOSETOWN,	"FIOSETOWN"	},
	{ (uint_t)FIOGETOWN,	"FIOGETOWN"	},
#ifdef DIOCGETP
	{ (uint_t)DIOCGETP,	"DIOCGETP"	},
	{ (uint_t)DIOCSETP,	"DIOCSETP"	},
#endif
#ifdef DIOCGETC
	{ (uint_t)DIOCGETC,	"DIOCGETC"	},
	{ (uint_t)DIOCGETB,	"DIOCGETB"	},
	{ (uint_t)DIOCSETE,	"DIOCSETE"	},
#endif
#ifdef IFFORMAT
	{ (uint_t)IFFORMAT,	"IFFORMAT"	},
	{ (uint_t)IFBCHECK,	"IFBCHECK"	},
	{ (uint_t)IFCONFIRM,	"IFCONFIRM"	},
#endif
#ifdef LIOCGETP
	{ (uint_t)LIOCGETP,	"LIOCGETP"	},
	{ (uint_t)LIOCSETP,	"LIOCSETP"	},
	{ (uint_t)LIOCGETS,	"LIOCGETS"	},
	{ (uint_t)LIOCSETS,	"LIOCSETS"	},
#endif
#ifdef JBOOT
	{ (uint_t)JBOOT,	"JBOOT"		},
	{ (uint_t)JTERM,	"JTERM"		},
	{ (uint_t)JMPX,		"JMPX"		},
#ifdef JTIMO
	{ (uint_t)JTIMO,	"JTIMO"		},
#endif
	{ (uint_t)JWINSIZE,	"JWINSIZE"	},
	{ (uint_t)JTIMOM,	"JTIMOM"	},
	{ (uint_t)JZOMBOOT,	"JZOMBOOT"	},
	{ (uint_t)JAGENT,	"JAGENT"	},
	{ (uint_t)JTRUN,	"JTRUN"		},
	{ (uint_t)JXTPROTO,	"JXTPROTO"	},
#endif
	{ (uint_t)KSTAT_IOC_CHAIN_ID,	"KSTAT_IOC_CHAIN_ID"	},
	{ (uint_t)KSTAT_IOC_READ,	"KSTAT_IOC_READ"	},
	{ (uint_t)KSTAT_IOC_WRITE,	"KSTAT_IOC_WRITE"	},
	{ (uint_t)STGET,	"STGET"		},
	{ (uint_t)STSET,	"STSET"		},
	{ (uint_t)STTHROW,	"STTHROW"	},
	{ (uint_t)STWLINE,	"STWLINE"	},
	{ (uint_t)STTSV,	"STTSV"		},
	{ (uint_t)I_NREAD,	"I_NREAD"	},
	{ (uint_t)I_PUSH,	"I_PUSH"	},
	{ (uint_t)I_POP,	"I_POP"		},
	{ (uint_t)I_LOOK,	"I_LOOK"	},
	{ (uint_t)I_FLUSH,	"I_FLUSH"	},
	{ (uint_t)I_SRDOPT,	"I_SRDOPT"	},
	{ (uint_t)I_GRDOPT,	"I_GRDOPT"	},
	{ (uint_t)I_STR,	"I_STR"		},
	{ (uint_t)I_SETSIG,	"I_SETSIG"	},
	{ (uint_t)I_GETSIG,	"I_GETSIG"	},
	{ (uint_t)I_FIND,	"I_FIND"	},
	{ (uint_t)I_LINK,	"I_LINK"	},
	{ (uint_t)I_UNLINK,	"I_UNLINK"	},
	{ (uint_t)I_PEEK,	"I_PEEK"	},
	{ (uint_t)I_FDINSERT,	"I_FDINSERT"	},
	{ (uint_t)I_SENDFD,	"I_SENDFD"	},
	{ (uint_t)I_RECVFD,	"I_RECVFD"	},
	{ (uint_t)I_SWROPT,	"I_SWROPT"	},
	{ (uint_t)I_GWROPT,	"I_GWROPT"	},
	{ (uint_t)I_LIST,	"I_LIST"	},
	{ (uint_t)I_PLINK,	"I_PLINK"	},
	{ (uint_t)I_PUNLINK,	"I_PUNLINK"	},
	{ (uint_t)I_FLUSHBAND,	"I_FLUSHBAND"	},
	{ (uint_t)I_CKBAND,	"I_CKBAND"	},
	{ (uint_t)I_GETBAND,	"I_GETBAND"	},
	{ (uint_t)I_ATMARK,	"I_ATMARK"	},
	{ (uint_t)I_SETCLTIME,	"I_SETCLTIME"	},
	{ (uint_t)I_GETCLTIME,	"I_GETCLTIME"	},
	{ (uint_t)I_CANPUT,	"I_CANPUT"	},
#ifdef I_ANCHOR
	{ (uint_t)I_ANCHOR,	"I_ANCHOR"	},
#endif
#ifdef TI_GETINFO
	{ (uint_t)TI_GETINFO,	"TI_GETINFO"	},
	{ (uint_t)TI_OPTMGMT,	"TI_OPTMGMT"	},
	{ (uint_t)TI_BIND,	"TI_BIND"	},
	{ (uint_t)TI_UNBIND,	"TI_UNBIND"	},
#endif
#ifdef	TI_CAPABILITY
	{ (uint_t)TI_CAPABILITY,	"TI_CAPABILITY"	},
#endif
#ifdef TI_GETMYNAME
	{ (uint_t)TI_GETMYNAME,		"TI_GETMYNAME"},
	{ (uint_t)TI_GETPEERNAME,	"TI_GETPEERNAME"},
	{ (uint_t)TI_SETMYNAME,		"TI_SETMYNAME"},
	{ (uint_t)TI_SETPEERNAME,	"TI_SETPEERNAME"},
#endif
#ifdef V_PREAD
	{ (uint_t)V_PREAD,	"V_PREAD"	},
	{ (uint_t)V_PWRITE,	"V_PWRITE"	},
	{ (uint_t)V_PDREAD,	"V_PDREAD"	},
	{ (uint_t)V_PDWRITE,	"V_PDWRITE"	},
#if !defined(i386)
	{ (uint_t)V_GETSSZ,	"V_GETSSZ"	},
#endif /* !i386 */
#endif
	/* audio */
	{ (uint_t)AUDIO_GETINFO,	"AUDIO_GETINFO"},
	{ (uint_t)AUDIO_SETINFO,	"AUDIO_SETINFO"},
	{ (uint_t)AUDIO_DRAIN,		"AUDIO_DRAIN"},
	{ (uint_t)AUDIO_GETDEV,		"AUDIO_GETDEV"},
	{ (uint_t)AUDIO_DIAG_LOOPBACK,	"AUDIO_DIAG_LOOPBACK"},
	/* volume management (control ioctls) */
	{ (uint_t)VOLIOCMAP,	"VOLIOCMAP"},
	{ (uint_t)VOLIOCUNMAP,	"VOLIOCUNMAP"},
	{ (uint_t)VOLIOCEVENT,	"VOLIOCEVENT"},
	{ (uint_t)VOLIOCEJECT,	"VOLIOCEJECT"},
	{ (uint_t)VOLIOCDGATTR,	"VOLIOCDGATTR"},
	{ (uint_t)VOLIOCDSATTR,	"VOLIOCDSATTR"},
	{ (uint_t)VOLIOCDCHECK,	"VOLIOCDCHECK"},
	{ (uint_t)VOLIOCDINUSE,	"VOLIOCDINUSE"},
	{ (uint_t)VOLIOCDAEMON,	"VOLIOCDAEMON"},
	{ (uint_t)VOLIOCFLAGS,	"VOLIOCFLAGS"},
	{ (uint_t)VOLIOCDROOT,	"VOLIOCDROOT"},
	{ (uint_t)VOLIOCDSYMNAME, "VOLIOCDSYMNAME"},
	{ (uint_t)VOLIOCDSYMDEV, "VOLIOCDSYMDEV"},
	/* volume management (user ioctls) */
	{ (uint_t)VOLIOCINUSE,	"VOLIOCINUSE"},
	{ (uint_t)VOLIOCCHECK,	"VOLIOCCHECK"},
	{ (uint_t)VOLIOCCANCEL,	"VOLIOCCANCEL"},
	{ (uint_t)VOLIOCINFO,	"VOLIOCINFO"},
	{ (uint_t)VOLIOCSATTR,	"VOLIOCSATTR"},
	{ (uint_t)VOLIOCGATTR,	"VOLIOCGATTR"},
	{ (uint_t)VOLIOCROOT,	"VOLIOCROOT"},
	{ (uint_t)VOLIOCSYMNAME, "VOLIOCSYMNAME"},
	{ (uint_t)VOLIOCSYMDEV,	"VOLIOCSYMDEV"},
#ifdef _CPCIO_IOC
	{ (uint_t)CPCIO_BIND_EVENT,	"CPCIO_BIND_EVENT" },
	{ (uint_t)CPCIO_TAKE_SAMPLE,	"CPCIO_TAKE_SAMPLE" },
	{ (uint_t)CPCIO_RELE,		"CPCIO_RELE" },
#endif
	/* the old /proc ioctl() control codes */
#define	PIOC	('q'<<8)
	{ (uint_t)(PIOC|1),	"PIOCSTATUS"	},
	{ (uint_t)(PIOC|2),	"PIOCSTOP"	},
	{ (uint_t)(PIOC|3),	"PIOCWSTOP"	},
	{ (uint_t)(PIOC|4),	"PIOCRUN"	},
	{ (uint_t)(PIOC|5),	"PIOCGTRACE"	},
	{ (uint_t)(PIOC|6),	"PIOCSTRACE"	},
	{ (uint_t)(PIOC|7),	"PIOCSSIG"	},
	{ (uint_t)(PIOC|8),	"PIOCKILL"	},
	{ (uint_t)(PIOC|9),	"PIOCUNKILL"	},
	{ (uint_t)(PIOC|10),	"PIOCGHOLD"	},
	{ (uint_t)(PIOC|11),	"PIOCSHOLD"	},
	{ (uint_t)(PIOC|12),	"PIOCMAXSIG"	},
	{ (uint_t)(PIOC|13),	"PIOCACTION"	},
	{ (uint_t)(PIOC|14),	"PIOCGFAULT"	},
	{ (uint_t)(PIOC|15),	"PIOCSFAULT"	},
	{ (uint_t)(PIOC|16),	"PIOCCFAULT"	},
	{ (uint_t)(PIOC|17),	"PIOCGENTRY"	},
	{ (uint_t)(PIOC|18),	"PIOCSENTRY"	},
	{ (uint_t)(PIOC|19),	"PIOCGEXIT"	},
	{ (uint_t)(PIOC|20),	"PIOCSEXIT"	},
	{ (uint_t)(PIOC|21),	"PIOCSFORK"	},
	{ (uint_t)(PIOC|22),	"PIOCRFORK"	},
	{ (uint_t)(PIOC|23),	"PIOCSRLC"	},
	{ (uint_t)(PIOC|24),	"PIOCRRLC"	},
	{ (uint_t)(PIOC|25),	"PIOCGREG"	},
	{ (uint_t)(PIOC|26),	"PIOCSREG"	},
	{ (uint_t)(PIOC|27),	"PIOCGFPREG"	},
	{ (uint_t)(PIOC|28),	"PIOCSFPREG"	},
	{ (uint_t)(PIOC|29),	"PIOCNICE"	},
	{ (uint_t)(PIOC|30),	"PIOCPSINFO"	},
	{ (uint_t)(PIOC|31),	"PIOCNMAP"	},
	{ (uint_t)(PIOC|32),	"PIOCMAP"	},
	{ (uint_t)(PIOC|33),	"PIOCOPENM"	},
	{ (uint_t)(PIOC|34),	"PIOCCRED"	},
	{ (uint_t)(PIOC|35),	"PIOCGROUPS"	},
	{ (uint_t)(PIOC|36),	"PIOCGETPR"	},
	{ (uint_t)(PIOC|37),	"PIOCGETU"	},
	{ (uint_t)(PIOC|38),	"PIOCSET"	},
	{ (uint_t)(PIOC|39),	"PIOCRESET"	},
	{ (uint_t)(PIOC|43),	"PIOCUSAGE"	},
	{ (uint_t)(PIOC|44),	"PIOCOPENPD"	},
	{ (uint_t)(PIOC|45),	"PIOCLWPIDS"	},
	{ (uint_t)(PIOC|46),	"PIOCOPENLWP"	},
	{ (uint_t)(PIOC|47),	"PIOCLSTATUS"	},
	{ (uint_t)(PIOC|48),	"PIOCLUSAGE"	},
	{ (uint_t)(PIOC|49),	"PIOCNAUXV"	},
	{ (uint_t)(PIOC|50),	"PIOCAUXV"	},
	{ (uint_t)(PIOC|51),	"PIOCGXREGSIZE"	},
	{ (uint_t)(PIOC|52),	"PIOCGXREG"	},
	{ (uint_t)(PIOC|53),	"PIOCSXREG"	},
	{ (uint_t)(PIOC|101),	"PIOCGWIN"	},
	{ (uint_t)(PIOC|103),	"PIOCNLDT"	},
	{ (uint_t)(PIOC|104),	"PIOCLDT"	},
	{ (uint_t)0,		 NULL		}
};

const char *
ioctlname(uint_t code)
{
	const struct ioc *ip;
	const char *str = NULL;
	int c;

	for (ip = &ioc[0]; ip->name; ip++) {
		if (code == ip->code) {
			str = ip->name;
			break;
		}
	}

	if (str == NULL) {
		c = code >> 8;
		if (isascii(c) && isprint(c))
			(void) sprintf(code_buf, "(('%c'<<8)|%d)",
				c, code & 0xff);
		else
			(void) sprintf(code_buf, "0x%.4X", code);
		str = code_buf;
	}

	return (str);
}

const char *
fcntlname(int code)
{
	const char *str = NULL;

	if (code >= FCNTLMIN && code <= FCNTLMAX)
		str = FCNTLname[code-FCNTLMIN];
	return (str);
}

const char *
sfsname(int code)
{
	const char *str = NULL;

	if (code >= SYSFSMIN && code <= SYSFSMAX)
		str = SYSFSname[code-SYSFSMIN];
	return (str);
}

const char *
plockname(int code)
{
	const char *str = NULL;

	if (code >= PLOCKMIN && code <= PLOCKMAX)
		str = PLOCKname[code-PLOCKMIN];
	return (str);
}

/* ARGSUSED */
const char *
si86name(int code)
{
	const char *str = NULL;

#if defined(i386)
	switch (code) {
	case SI86SWPI:		str = "SI86SWPI";	break;
	case SI86SYM:		str = "SI86SYM";	break;
	case SI86CONF:		str = "SI86CONF";	break;
	case SI86BOOT:		str = "SI86BOOT";	break;
	case SI86AUTO:		str = "SI86AUTO";	break;
	case SI86EDT:		str = "SI86EDT";	break;
	case SI86SWAP:		str = "SI86SWAP";	break;
	case SI86FPHW:		str = "SI86FPHW";	break;
	case GRNON:		str = "GRNON";		break;
	case GRNFLASH:		str = "GRNFLASH";	break;
	case STIME:		str = "STIME";		break;
	case SETNAME:		str = "SETNAME";	break;
	case RNVR:		str = "RNVR";		break;
	case WNVR:		str = "WNVR";		break;
	case RTODC:		str = "RTODC";		break;
	case CHKSER:		str = "CHKSER";		break;
	case SI86NVPRT:		str = "SI86NVPRT";	break;
	case SANUPD:		str = "SANUPD";		break;
	case SI86KSTR:		str = "SI86KSTR";	break;
	case SI86MEM:		str = "SI86MEM";	break;
	case SI86TODEMON:	str = "SI86TODEMON";	break;
	case SI86CCDEMON:	str = "SI86CCDEMON";	break;
	case SI86CACHE:		str = "SI86CACHE";	break;
	case SI86DELMEM:	str = "SI86DELMEM";	break;
	case SI86ADDMEM:	str = "SI86ADDMEM";	break;
/* 71 through 74 reserved for VPIX */
	case SI86V86: 		str = "SI86V86";	break;
	case SI86SLTIME:	str = "SI86SLTIME";	break;
	case SI86DSCR:		str = "SI86DSCR";	break;
	case RDUBLK:		str = "RDUBLK";		break;
/* NFA entry point */
	case SI86NFA:		str = "SI86NFA";	break;
	case SI86VM86:		str = "SI86VM86";	break;
	case SI86VMENABLE:	str = "SI86VMENABLE";	break;
	case SI86LIMUSER:	str = "SI86LIMUSER";	break;
	case SI86RDID: 		str = "SI86RDID";	break;
	case SI86RDBOOT:	str = "SI86RDBOOT";	break;
/* Merged Product defines */
	case SI86SHFIL:		str = "SI86SHFIL";	break;
	case SI86PCHRGN:	str = "SI86PCHRGN";	break;
	case SI86BADVISE:	str = "SI86BADVISE";	break;
	case SI86SHRGN:		str = "SI86SHRGN";	break;
	case SI86CHIDT:		str = "SI86CHIDT";	break;
	case SI86EMULRDA: 	str = "SI86EMULRDA";	break;
	}
#endif /* i386 */

	return (str);
}

const char *
utscode(int code)
{
	const char *str = NULL;

	switch (code) {
	case UTS_UNAME:		str = "UNAME";	break;
	case UTS_USTAT:		str = "USTAT";	break;
	case UTS_FUSERS:	str = "FUSERS";	break;
	}

	return (str);
}

const char *
sconfname(int code)
{
	const char *str = NULL;

	if (code >= SCONFMIN && code <= SCONFMAX)
		str = SCONFname[code-SCONFMIN];
	return (str);
}

const char *
pathconfname(int code)
{
	const char *str = NULL;

	if (code >= PATHCONFMIN && code <= PATHCONFMAX)
		str = PATHCONFname[code-PATHCONFMIN];
	return (str);
}

const char *
sigarg(int arg)
{
	char *str = NULL;
	int sig = (arg & SIGNO_MASK);

	str = code_buf;
	arg &= ~SIGNO_MASK;
	if (arg & ~(SIGDEFER|SIGHOLD|SIGRELSE|SIGIGNORE|SIGPAUSE))
		(void) sprintf(str, "%s|0x%X", signame(sig), arg);
	else {
		(void) strcpy(str, signame(sig));
		if (arg & SIGDEFER)
			(void) strcat(str, "|SIGDEFER");
		if (arg & SIGHOLD)
			(void) strcat(str, "|SIGHOLD");
		if (arg & SIGRELSE)
			(void) strcat(str, "|SIGRELSE");
		if (arg & SIGIGNORE)
			(void) strcat(str, "|SIGIGNORE");
		if (arg & SIGPAUSE)
			(void) strcat(str, "|SIGPAUSE");
	}

	return ((const char *)str);
}

#define	ALL_O_FLAGS \
	(O_NDELAY|O_APPEND|O_SYNC|O_DSYNC|O_NONBLOCK\
	|O_CREAT|O_TRUNC|O_EXCL|O_NOCTTY|O_LARGEFILE|O_RSYNC)

const char *
openarg(int arg)
{
	char *str = code_buf;

	switch (arg & ~ALL_O_FLAGS) {
	default:
		return ((char *)NULL);
	case O_RDONLY:
		(void) strcpy(str, "O_RDONLY");
		break;
	case O_WRONLY:
		(void) strcpy(str, "O_WRONLY");
		break;
	case O_RDWR:
		(void) strcpy(str, "O_RDWR");
		break;
	}

	if (arg & O_NDELAY)
		(void) strcat(str, "|O_NDELAY");
	if (arg & O_APPEND)
		(void) strcat(str, "|O_APPEND");
	if (arg & O_SYNC)
		(void) strcat(str, "|O_SYNC");
	if (arg & O_DSYNC)
		(void) strcat(str, "|O_DSYNC");
	if (arg & O_NONBLOCK)
		(void) strcat(str, "|O_NONBLOCK");
	if (arg & O_CREAT)
		(void) strcat(str, "|O_CREAT");
	if (arg & O_TRUNC)
		(void) strcat(str, "|O_TRUNC");
	if (arg & O_EXCL)
		(void) strcat(str, "|O_EXCL");
	if (arg & O_NOCTTY)
		(void) strcat(str, "|O_NOCTTY");
	if (arg & O_LARGEFILE)
		(void) strcat(str, "|O_LARGEFILE");
	if (arg & O_RSYNC)
		(void) strcat(str, "|O_RSYNC");

	return ((const char *)str);
}

const char *
whencearg(int arg)
{
	const char *str = NULL;

	switch (arg) {
	case SEEK_SET:	str = "SEEK_SET";	break;
	case SEEK_CUR:	str = "SEEK_CUR";	break;
	case SEEK_END:	str = "SEEK_END";	break;
	}

	return (str);
}

#define	IPC_FLAGS	(IPC_ALLOC|IPC_CREAT|IPC_EXCL|IPC_NOWAIT)

static char *
ipcflags(int arg)
{
	char *str = code_buf;

	if (arg&0777)
		(void) sprintf(str, "0%.3o", arg&0777);
	else
		*str = '\0';

	if (arg & IPC_ALLOC)
		(void) strcat(str, "|IPC_ALLOC");
	if (arg & IPC_CREAT)
		(void) strcat(str, "|IPC_CREAT");
	if (arg & IPC_EXCL)
		(void) strcat(str, "|IPC_EXCL");
	if (arg & IPC_NOWAIT)
		(void) strcat(str, "|IPC_NOWAIT");

	return (str);
}

const char *
msgflags(int arg)
{
	char *str;

	if (arg == 0 || (arg & ~(IPC_FLAGS|MSG_NOERROR|0777)) != 0)
		return ((char *)NULL);

	str = ipcflags(arg);

	if (arg & MSG_NOERROR)
		(void) strcat(str, "|MSG_NOERROR");

	if (*str == '|')
		str++;
	return ((const char *)str);
}

const char *
semflags(int arg)
{
	char *str;

	if (arg == 0 || (arg & ~(IPC_FLAGS|SEM_UNDO|0777)) != 0)
		return ((char *)NULL);

	str = ipcflags(arg);

	if (arg & SEM_UNDO)
		(void) strcat(str, "|SEM_UNDO");

	if (*str == '|')
		str++;
	return ((const char *)str);
}

const char *
shmflags(int arg)
{
	char *str;

	if (arg == 0 || (arg & ~(IPC_FLAGS|SHM_RDONLY|SHM_RND|0777)) != 0)
		return ((char *)NULL);

	str = ipcflags(arg);

	if (arg & SHM_RDONLY)
		(void) strcat(str, "|SHM_RDONLY");
	if (arg & SHM_RND)
		(void) strcat(str, "|SHM_RND");

	if (*str == '|')
		str++;
	return ((const char *)str);
}

#define	MSGCMDMIN	IPC_RMID
#define	MSGCMDMAX	IPC_STAT
static const char *const MSGCMDname[MSGCMDMAX+1] = {
	"IPC_RMID",
	"IPC_SET",
	"IPC_STAT",
};

#define	SEMCMDMIN	IPC_RMID
#define	SEMCMDMAX	SETALL
static const char *const SEMCMDname[SEMCMDMAX+1] = {
	"IPC_RMID",
	"IPC_SET",
	"IPC_STAT",
	"GETNCNT",
	"GETPID",
	"GETVAL",
	"GETALL",
	"GETZCNT",
	"SETVAL",
	"SETALL",
};

#define	SHMCMDMIN	IPC_RMID
#ifdef	SHM_UNLOCK
#define	SHMCMDMAX	SHM_UNLOCK
#else
#define	SHMCMDMAX	IPC_STAT
#endif
static const char *const SHMCMDname[SHMCMDMAX+1] = {
	"IPC_RMID",
	"IPC_SET",
	"IPC_STAT",
#ifdef	SHM_UNLOCK
	"SHM_LOCK",
	"SHM_UNLOCK",
#endif
};

const char *
msgcmd(int arg)
{
	const char *str = NULL;

	if (arg >= MSGCMDMIN && arg <= MSGCMDMAX)
		str = MSGCMDname[arg-MSGCMDMIN];
	return (str);
}

const char *
semcmd(int arg)
{
	const char *str = NULL;

	if (arg >= SEMCMDMIN && arg <= SEMCMDMAX)
		str = SEMCMDname[arg-SEMCMDMIN];
	return (str);
}

const char *
shmcmd(int arg)
{
	const char *str = NULL;

	if (arg >= SHMCMDMIN && arg <= SHMCMDMAX)
		str = SHMCMDname[arg-SHMCMDMIN];
	return (str);
}

const char *
strrdopt(int arg)	/* streams read option (I_SRDOPT I_GRDOPT) */
{
	const char *str = NULL;

	switch (arg) {
	case RNORM:	str = "RNORM";		break;
	case RMSGD:	str = "RMSGD";		break;
	case RMSGN:	str = "RMSGN";		break;
	}

	return (str);
}

const char *
strevents(int arg)	/* bit map of streams events (I_SETSIG & I_GETSIG) */
{
	char *str = code_buf;

	if (arg & ~(S_INPUT|S_HIPRI|S_OUTPUT|S_MSG|S_ERROR|S_HANGUP))
		return ((char *)NULL);

	*str = '\0';
	if (arg & S_INPUT)
		(void) strcat(str, "|S_INPUT");
	if (arg & S_HIPRI)
		(void) strcat(str, "|S_HIPRI");
	if (arg & S_OUTPUT)
		(void) strcat(str, "|S_OUTPUT");
	if (arg & S_MSG)
		(void) strcat(str, "|S_MSG");
	if (arg & S_ERROR)
		(void) strcat(str, "|S_ERROR");
	if (arg & S_HANGUP)
		(void) strcat(str, "|S_HANGUP");

	return ((const char *)(str+1));
}

const char *
tiocflush(int arg)	/* bit map passsed by TIOCFLUSH */
{
	char *str = code_buf;

	if (arg & ~(FREAD|FWRITE))
		return ((char *)NULL);

	*str = '\0';
	if (arg & FREAD)
		(void) strcat(str, "|FREAD");
	if (arg & FWRITE)
		(void) strcat(str, "|FWRITE");

	return ((const char *)(str+1));
}

const char *
strflush(int arg)	/* streams flush option (I_FLUSH) */
{
	const char *str = NULL;

	switch (arg) {
	case FLUSHR:	str = "FLUSHR";		break;
	case FLUSHW:	str = "FLUSHW";		break;
	case FLUSHRW:	str = "FLUSHRW";	break;
	}

	return (str);
}

#define	ALL_MOUNT_FLAGS	(MS_RDONLY|MS_FSS|MS_DATA|MS_NOSUID|MS_REMOUNT| \
	MS_NOTRUNC|MS_OVERLAY|MS_OPTIONSTR|MS_GLOBAL)

const char *
mountflags(int arg)	/* bit map of mount syscall flags */
{
	char *str = code_buf;

	if (arg & ~ALL_MOUNT_FLAGS)
		return ((char *)NULL);

	*str = '\0';
	if (arg & MS_RDONLY)
		(void) strcat(str, "|MS_RDONLY");
	if (arg & MS_FSS)
		(void) strcat(str, "|MS_FSS");
	if (arg & MS_DATA)
		(void) strcat(str, "|MS_DATA");
	if (arg & MS_NOSUID)
		(void) strcat(str, "|MS_NOSUID");
	if (arg & MS_REMOUNT)
		(void) strcat(str, "|MS_REMOUNT");
	if (arg & MS_NOTRUNC)
		(void) strcat(str, "|MS_NOTRUNC");
	if (arg & MS_OVERLAY)
		(void) strcat(str, "|MS_OVERLAY");
	if (arg & MS_OPTIONSTR)
		(void) strcat(str, "|MS_OPTIONSTR");
	if (arg & MS_GLOBAL)
		(void) strcat(str, "|MS_GLOBAL");
	return (*str? (const char *)(str+1) : "0");
}

const char *
svfsflags(ulong_t arg)	/* bit map of statvfs syscall flags */
{
	char *str = code_buf;

	if (arg & ~(ST_RDONLY|ST_NOSUID|ST_NOTRUNC)) {
		(void) sprintf(str, "0x%lx", arg);
		return (str);
	}
	*str = '\0';
	if (arg & ST_RDONLY)
		(void) strcat(str, "|ST_RDONLY");
	if (arg & ST_NOSUID)
		(void) strcat(str, "|ST_NOSUID");
	if (arg & ST_NOTRUNC)
		(void) strcat(str, "|ST_NOTRUNC");
	return (*str? (const char *)(str+1) : "0");
}

const char *
fuiname(int arg)	/* fusers() input argument */
{
	const char *str = NULL;

	switch (arg) {
	case F_FILE_ONLY:	str = "F_FILE_ONLY";		break;
	case F_CONTAINED:	str = "F_CONTAINED";		break;
	}

	return (str);
}

const char *
fuflags(int arg)	/* fusers() output flags */
{
	char *str = code_buf;

	if (arg & ~(F_CDIR|F_RDIR|F_TEXT|F_MAP|F_OPEN|F_TRACE|F_TTY)) {
		(void) sprintf(str, "0x%x", arg);
		return (str);
	}
	*str = '\0';
	if (arg & F_CDIR)
		(void) strcat(str, "|F_CDIR");
	if (arg & F_RDIR)
		(void) strcat(str, "|F_RDIR");
	if (arg & F_TEXT)
		(void) strcat(str, "|F_TEXT");
	if (arg & F_MAP)
		(void) strcat(str, "|F_MAP");
	if (arg & F_OPEN)
		(void) strcat(str, "|F_OPEN");
	if (arg & F_TRACE)
		(void) strcat(str, "|F_TRACE");
	if (arg & F_TTY)
		(void) strcat(str, "|F_TTY");
	return (*str? (const char *)(str+1) : "0");
}
