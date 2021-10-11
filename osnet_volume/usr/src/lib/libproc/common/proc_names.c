/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)proc_names.c	1.5	99/08/15 SMI"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "libproc.h"

/*
 * Return the name of a fault.
 * Manufacture a name for unknown fault.
 */
char *
proc_fltname(int flt, char *buf, size_t bufsz)
{
	const char *name;
	size_t len;

	if (bufsz == 0)		/* force a program failure */
		return (NULL);

	switch (flt) {
	case FLTILL:	name = "FLTILL";	break;
	case FLTPRIV:	name = "FLTPRIV";	break;
	case FLTBPT:	name = "FLTBPT";	break;
	case FLTTRACE:	name = "FLTTRACE";	break;
	case FLTACCESS:	name = "FLTACCESS";	break;
	case FLTBOUNDS:	name = "FLTBOUNDS";	break;
	case FLTIOVF:	name = "FLTIOVF";	break;
	case FLTIZDIV:	name = "FLTIZDIV";	break;
	case FLTFPE:	name = "FLTFPE";	break;
	case FLTSTACK:	name = "FLTSTACK";	break;
	case FLTPAGE:	name = "FLTPAGE";	break;
	case FLTWATCH:	name = "FLTWATCH";	break;
	case FLTCPCOVF:	name = "FLTCPCOVF";	break;
	default:	name = NULL;		break;
	}

	if (name != NULL) {
		len = strlen(name);
		(void) strncpy(buf, name, bufsz);
	} else {
		len = snprintf(buf, bufsz, "FLT#%d", flt);
	}

	if (len >= bufsz)	/* ensure null-termination */
		buf[bufsz-1] = '\0';

	return (buf);
}

/*
 * Return the name of a signal.
 * Manufacture a name for unknown signal.
 */
char *
proc_signame(int sig, char *buf, size_t bufsz)
{
	char name[SIG2STR_MAX+4];
	size_t len;

	if (bufsz == 0)		/* force a program failure */
		return (NULL);

	/* sig2str() omits the leading "SIG" */
	(void) strcpy(name, "SIG");

	if (sig2str(sig, name+3) == 0) {
		len = strlen(name);
		(void) strncpy(buf, name, bufsz);
	} else {
		len = snprintf(buf, bufsz, "SIG#%d", sig);
	}

	if (len >= bufsz)	/* ensure null-termination */
		buf[bufsz-1] = '\0';

	return (buf);
}

static const char *const systable[] = {
	NULL,			/*  0 */
	"_exit",		/*  1 */
	"fork",			/*  2 */
	"read",			/*  3 */
	"write",		/*  4 */
	"open",			/*  5 */
	"close",		/*  6 */
	"wait",			/*  7 */
	"creat",		/*  8 */
	"link",			/*  9 */
	"unlink",		/* 10 */
	"exec",			/* 11 */
	"chdir",		/* 12 */
	"time",			/* 13 */
	"mknod",		/* 14 */
	"chmod",		/* 15 */
	"chown",		/* 16 */
	"brk",			/* 17 */
	"stat",			/* 18 */
	"lseek",		/* 19 */
	"getpid",		/* 20 */
	"mount",		/* 21 */
	"umount",		/* 22 */
	"setuid",		/* 23 */
	"getuid",		/* 24 */
	"stime",		/* 25 */
	"ptrace",		/* 26 */
	"alarm",		/* 27 */
	"fstat",		/* 28 */
	"pause",		/* 29 */
	"utime",		/* 30 */
	"stty",			/* 31 */
	"gtty",			/* 32 */
	"access",		/* 33 */
	"nice",			/* 34 */
	"statfs",		/* 35 */
	"sync",			/* 36 */
	"kill",			/* 37 */
	"fstatfs",		/* 38 */
	"pgrpsys",		/* 39 */
	"xenix",		/* 40 */
	"dup",			/* 41 */
	"pipe",			/* 42 */
	"times",		/* 43 */
	"profil",		/* 44 */
	"plock",		/* 45 */
	"setgid",		/* 46 */
	"getgid",		/* 47 */
	"signal",		/* 48 */
	"msgsys",		/* 49 */
	"sysi86",		/* 50 */
	"acct",			/* 51 */
	"shmsys",		/* 52 */
	"semsys",		/* 53 */
	"ioctl",		/* 54 */
	"uadmin",		/* 55 */
	"uexch",		/* 56 */
	"utssys",		/* 57 */
	"fdsync",		/* 58 */
	"execve",		/* 59 */
	"umask",		/* 60 */
	"chroot",		/* 61 */
	"fcntl",		/* 62 */
	"ulimit",		/* 63 */

	/* The following 6 entries were reserved for the UNIX PC */
	NULL,			/* 64 */
	NULL,			/* 65 */
	NULL,			/* 66 */
	NULL,			/* 67 */
	NULL,			/* 68 */
	NULL,			/* 69 */

	/* Obsolete RFS-specific entries */
	NULL,			/* 70 */
	NULL,			/* 71 */
	NULL,			/* 72 */
	NULL,			/* 73 */
	NULL,			/* 74 */
	NULL,			/* 75 */
	NULL,			/* 76 */
	NULL,			/* 77 */
	NULL,			/* 78 */

	"rmdir",		/* 79 */
	"mkdir",		/* 80 */
	"getdents",		/* 81 */
	NULL,			/* 82 */
	NULL,			/* 83 */
	"sysfs",		/* 84 */
	"getmsg",		/* 85 */
	"putmsg",		/* 86 */
	"poll",			/* 87 */
	"lstat",		/* 88 */
	"symlink",		/* 89 */
	"readlink",		/* 90 */
	"setgroups",		/* 91 */
	"getgroups",		/* 92 */
	"fchmod",		/* 93 */
	"fchown",		/* 94 */
	"sigprocmask",		/* 95 */
	"sigsuspend",		/* 96 */
	"sigaltstack",		/* 97 */
	"sigaction",		/* 98 */
	"sigpending",		/* 99 */
	"context",		/* 100 */
	"evsys",		/* 101 */
	"evtrapret",		/* 102 */
	"statvfs",		/* 103 */
	"fstatvfs",		/* 104 */
	NULL,			/* 105 */
	"nfssys",		/* 106 */
	"waitid",		/* 107 */
	"sigsendsys",		/* 108 */
	"hrtsys",		/* 109 */
	"acancel",		/* 110 */
	"async",		/* 111 */
	"priocntlsys",		/* 112 */
	"pathconf",		/* 113 */
	"mincore",		/* 114 */
	"mmap",			/* 115 */
	"mprotect",		/* 116 */
	"munmap",		/* 117 */
	"fpathconf",		/* 118 */
	"vfork",		/* 119 */
	"fchdir",		/* 120 */
	"readv",		/* 121 */
	"writev",		/* 122 */
	"xstat",		/* 123 */
	"lxstat",		/* 124 */
	"fxstat",		/* 125 */
	"xmknod",		/* 126 */
	"clocal",		/* 127 */
	"setrlimit",		/* 128 */
	"getrlimit",		/* 129 */
	"lchown",		/* 130 */
	"memcntl",		/* 131 */
	"getpmsg",		/* 132 */
	"putpmsg",		/* 133 */
	"rename",		/* 134 */
	"uname",		/* 135 */
	"setegid",		/* 136 */
	"sysconfig",		/* 137 */
	"adjtime",		/* 138 */
	"systeminfo",		/* 139 */
	NULL,			/* 140 */
	"seteuid",		/* 141 */
	"vtrace",		/* 142 */
	"fork1",		/* 143 */
	"sigtimedwait",		/* 144 */
	"lwp_info",		/* 145 */
	"yield",		/* 146 */
	"lwp_sema_wait",	/* 147 */
	"lwp_sema_post",	/* 148 */
	"lwp_sema_trywait",	/* 149 */
	NULL,			/* 150 */
	NULL,			/* 151 */
	"modctl",		/* 152 */
	"fchroot",		/* 153 */
	"utimes",		/* 154 */
	"vhangup",		/* 155 */
	"gettimeofday",		/* 156 */
	"getitimer",		/* 157 */
	"setitimer",		/* 158 */
	"lwp_create",		/* 159 */
	"lwp_exit",		/* 160 */
	"lwp_suspend",		/* 161 */
	"lwp_continue",		/* 162 */
	"lwp_kill",		/* 163 */
	"lwp_self",		/* 164 */
	"lwp_setprivate",	/* 165 */
	"lwp_getprivate",	/* 166 */
	"lwp_wait",		/* 167 */
	"lwp_mutex_unlock",	/* 168 */
	"lwp_mutex_lock",	/* 169 */
	"lwp_cond_wait",	/* 170 */
	"lwp_cond_signal",	/* 171 */
	"lwp_cond_broadcast",	/* 172 */
	"pread",		/* 173 */
	"pwrite",		/* 174 */
	"llseek",		/* 175 */
	"inst_sync",		/* 176 */
	NULL,			/* 177 */
	"kaio",			/* 178 */
	"cpc",			/* 179 */
	NULL,			/* 180 */
	NULL,			/* 181 */
	NULL,			/* 182 */
	NULL,			/* 183 */
	NULL,			/* 184 */
	"acl",			/* 185 */
	"auditsys",		/* 186 */
	"processor_bind",	/* 187 */
	"processor_info",	/* 188 */
	"p_online",		/* 189 */
	"sigqueue",		/* 190 */
	"clock_gettime",	/* 191 */
	"clock_settime",	/* 192 */
	"clock_getres",		/* 193 */
	"timer_create",		/* 194 */
	"timer_delete",		/* 195 */
	"timer_settime",	/* 196 */
	"timer_gettime",	/* 197 */
	"timer_getoverrun",	/* 198 */
	"nanosleep",		/* 199 */
	"facl",			/* 200 */
	"door",			/* 201 */
	"setreuid",		/* 202 */
	"setregid",		/* 203 */
	"install_utrap",	/* 204 */
	"signotify",		/* 205 */
	"schedctl",		/* 206 */
	"pset",			/* 207 */
	NULL,			/* 208 */
	"resolvepath",		/* 209 */
	"signotifywait",	/* 210 */
	"lwp_sigredirect",	/* 211 */
	"lwp_alarm",		/* 212 */
	"getdents64",		/* 213 */
	"mmap64",		/* 214 */
	"stat64",		/* 215 */
	"lstat64",		/* 216 */
	"fstat64",		/* 217 */
	"statvfs64",		/* 218 */
	"fstatvfs64",		/* 219 */
	"setrlimit64",		/* 220 */
	"getrlimit64",		/* 221 */
	"pread64",		/* 222 */
	"pwrite64",		/* 223 */
	"creat64",		/* 224 */
	"open64",		/* 225 */
	"rpcmod",		/* 226 */
	NULL,			/* 227 */
	NULL,			/* 228 */
	NULL,			/* 229 */
	"so_socket",		/* 230 */
	"so_socketpair",	/* 231 */
	"bind",			/* 232 */
	"listen",		/* 233 */
	"accept",		/* 234 */
	"connect",		/* 235 */
	"shutdown",		/* 236 */
	"recv",			/* 237 */
	"recvfrom",		/* 238 */
	"recvmsg",		/* 239 */
	"send",			/* 240 */
	"sendmsg",		/* 241 */
	"sendto",		/* 242 */
	"getpeername",		/* 243 */
	"getsockname",		/* 244 */
	"getsockopt",		/* 245 */
	"setsockopt",		/* 246 */
	"sockconfig",		/* 247 */
	"ntp_gettime",		/* 248 */
	"ntp_adjtime",		/* 249 */
	"lwp_mutex_unlock",	/* 250 */
	"lwp_mutex_trylock",	/* 251 */
	"lwp_mutex_init",	/* 252 */
	"cladm",		/* 253 */
	"lwp_sigtimedwait",	/* 254 */
	"umount2"		/* 255 */
};

/* SYSEND == max syscall number + 1 */
#define	SYSEND	(sizeof (systable) / sizeof (systable[0]))

/*
 * Return the name of a system call.
 * Manufacture a name for unknown system call.
 */
char *
proc_sysname(int sys, char *buf, size_t bufsz)
{
	const char *name;
	size_t len;

	if (bufsz == 0)		/* force a program failure */
		return (NULL);

	if (sys >= 0 && sys < SYSEND)
		name = systable[sys];
	else
		name = NULL;

	if (name != NULL) {
		len = strlen(name);
		(void) strncpy(buf, name, bufsz);
	} else {
		len = snprintf(buf, bufsz, "SYS#%d", sys);
	}

	if (len >= bufsz)	/* ensure null-termination */
		buf[bufsz-1] = '\0';

	return (buf);
}
