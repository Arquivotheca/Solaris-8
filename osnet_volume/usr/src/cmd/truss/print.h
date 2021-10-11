/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)print.h	1.21	99/08/15 SMI"	/* SVr4.0 1.6   */

/*
 * Argument & return value print codes.
 */
#define	NOV	0		/* no value */
#define	DEC	1		/* print value in decimal */
#define	OCT	2		/* print value in octal */
#define	HEX	3		/* print value in hexadecimal */
#define	DEX	4		/* print value in hexadecimal if big enough */
#define	STG	5		/* print value as string */
#define	IOC	6		/* print ioctl code */
#define	FCN	7		/* print fcntl code */
#define	S86	8		/* print sysi86 code */
#define	UTS	9		/* print utssys code */
#define	OPN	10		/* print open code */
#define	SIG	11		/* print signal name plus flags */
#define	ACT	12		/* print signal action value */
#define	MSC	13		/* print msgsys command */
#define	MSF	14		/* print msgsys flags */
#define	SMC	15		/* print semsys command */
#define	SEF	16		/* print semsys flags */
#define	SHC	17		/* print shmsys command */
#define	SHF	18		/* print shmsys flags */
#define	PLK	19		/* print plock code */
#define	SFS	20		/* print sysfs code */
#define	RST	21		/* print string returned by sys call */
#define	SMF	22		/* print streams message flags */
#define	IOA	23		/* print ioctl argument */
#define	SIX	24		/* print signal, masked with SIGNO_MASK */
#define	MTF	25		/* print mount flags */
#define	MFT	26		/* print mount file system type */
#define	IOB	27		/* print contents of I/O buffer */
#define	HHX	28		/* print value in hexadecimal (half size) */
#define	WOP	29		/* print waitsys() options */
#define	SPM	30		/* print sigprocmask argument */
#define	RLK	31		/* print readlink buffer */
#define	MPR	32		/* print mmap()/mprotect() flags */
#define	MTY	33		/* print mmap() mapping type flags */
#define	MCF	34		/* print memcntl() function */
#define	MC4	35		/* print memcntl() (fourth) argument */
#define	MC5	36		/* print memcntl() (fifth) argument */
#define	MAD	37		/* print madvise() argument */
#define	ULM	38		/* print ulimit() argument */
#define	RLM	39		/* print get/setrlimit() argument */
#define	CNF	40		/* print sysconfig() argument */
#define	INF	41		/* print sysinfo() argument */
#define	PTC	42		/* print pathconf/fpathconf() argument */
#define	FUI	43		/* print fusers() input argument */
#define	IDT	44		/* print idtype_t, waitid() argument */
#define	LWF	45		/* print lwp_create() flags */
#define	ITM	46		/* print [get|set]itimer() arg */
#define	LLO	47		/* print long long offset */
#define	VTR	48		/* print vtrace() code */
#define	MOD	49		/* print modctl() code */
#define	WHN	50		/* print lseek() whence argument */
#define	ACL	51		/* print acl() code */
#define	AIO	52		/* print kaio() code */
#define	AUD	53		/* print auditsys() code */
#define	SAC	54		/* print schedctl() flags */
#define	UNS	55		/* print value in unsigned decimal */
#define	CLC	56		/* print cladm() command argument */
#define	CLF	57		/* print cladm() flag argument */
#define	COR	58		/* print corectl() subcode */
#define	CCO	59		/* print corectl() options */
#define	CPC	60		/* print cpc() subcode */
#define	HID	61		/* hidden argument, don't print */

/*
 * Print routines, indexed by print codes.
 */
extern void (* const Print[])();
