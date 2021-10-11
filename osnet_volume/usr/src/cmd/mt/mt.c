/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ident "@(#)mt.c 1.37 97/11/22 SMI"

/*
 * mt -- magnetic tape manipulation program
 */
#include <stdio.h>
#include <ctype.h>

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/mtio.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>


#define	equal(s1, s2)	(strcmp(s1, s2) == 0)
#define	MTASF		100	/* absolute file positioning; first file is 0 */

/*
 * This can't be DEFTAPE in mtio.h because that is currently the rewinding
 * unit which makes 'mt fsf' a questionable activity at best.
 */
#define	DEFAULT_NRW_TAPE	"/dev/rmt/0n"

static int mtfd;
static struct mtop mt_com;
static struct mtget mt_status;
static char *print_key(short key_code);
static void printreg(char *, register u_short, register char *);
static void status(struct mtget *);
static char *tape;

extern int errno;

static struct commands {
	char *c_name;
	int c_code;
	int c_ronly;
	int c_usecnt;
} com[] = {
	{ "weof",	MTWEOF,		0,	1 },
	{ "eof",	MTWEOF,		0,	1 },
	{ "fsf",	MTFSF,		1,	1 },
	{ "bsf",	MTBSF,		1,	1 },
	{ "asf",	MTASF,		1,	1 },
	{ "fsr",	MTFSR,		1,	1 },
	{ "bsr",	MTBSR,		1,	1 },
	{ "rewind",	MTREW,		1,	0 },
	{ "offline",	MTOFFL,		1,	0 },
	{ "rewoffl",	MTOFFL,		1,	0 },
	{ "status",	MTNOP,		1,	0 },
	{ "retension",	MTRETEN,	1,	0 },
	{ "erase",	MTERASE,	0,	0 },
	{ "eom",	MTEOM,		1,	0 },
	{ "nbsf",	MTNBSF,		1,	1 },
	{ "reserve",	MTIOCRESERVE,	1,	0 },
	{ "release",	MTIOCRELEASE,	1,	0 },
	{ "forcereserve",	MTIOCFORCERESERVE,	1,	0 },
	{ 0 }
};

#ifdef sun
static struct mt_tape_info tapes[] = MT_TAPE_INFO;
#endif sun

main(argc, argv)
	char **argv;
{
	register char *cp;
	register struct commands *comp;
	register int open_flag;

	if (argc > 2 && (equal(argv[1], "-t") || equal(argv[1], "-f"))) {
		argc -= 2;
		tape = argv[2];
		argv += 2;
	} else
		if ((tape = getenv("TAPE")) == NULL)
			tape = DEFAULT_NRW_TAPE;
	if (argc < 2) {
		(void) fprintf(stderr,
		    "usage: mt [ -f device ] command [ count ]\n");
		exit(1);
	}
	cp = argv[1];
	for (comp = com; comp->c_name != NULL; comp++)
		if (strncmp(cp, comp->c_name, strlen(cp)) == 0)
			break;
	if (comp->c_name == NULL) {
		(void) fprintf(stderr, "mt: unknown command: %s\n", cp);
		exit(1);
	}

	if (comp->c_code == MTIOCFORCERESERVE)	{
		open_flag = O_RDONLY | O_NDELAY;
	} else	{
		open_flag = (comp->c_ronly)?O_RDONLY:O_RDWR;
	}
	if ((mtfd = open(tape, open_flag)) < 0) {

		/*
		 * Provide additional error message decoding since
		 * we need additional error codes to fix them problem.
		 */
		if (errno == EIO) {
			(void) fprintf(stderr,
			    "%s: no tape loaded or drive offline\n",
				tape);
		} else if (errno == EACCES) {
			(void) fprintf(stderr,
				"%s: write protected or reserved.\n", tape);
		} else {
			perror(tape);
		}
		exit(1);
	}

	if (comp->c_code == MTIOCFORCERESERVE ||
			comp->c_code == MTIOCRESERVE ||
				comp->c_code == MTIOCRELEASE)  {
		/*
		 * Handle all MTIOC ioctls used in
		 * reservation/release/takeownership.
		 */
		if (ioctl(mtfd, comp->c_code) < 0) {
			perror("mt");
			exit(2);
		}
	} else if (comp->c_code == MTASF) {
		/*
		 * Handle absolute file positioning.  Ask tape driver
		 * where tape is and then skip to desired file.  If
		 * driver doesn't support get location ioctl, rewind
		 * the tape and then space to the desired file.
		 */
		int usecnt;
		int mt_fileno;
		struct mtget mt_status;

		usecnt = argc > 2 && comp->c_usecnt;
		mt_fileno = usecnt ? atoi(argv[2]) : 1;
		if (mt_fileno < 0) {
			(void) fprintf(stderr, "mt: negative file number\n");
			exit(1);
		}
		(void) ioctl(mtfd, MTIOCGET, (char *)&mt_status);
		if (ioctl(mtfd, MTIOCGET, (char *)&mt_status) < 0) {
			perror("mt");
			exit(2);
		}
		/*
		 * Check if device supports reporting current file
		 * tape file position.  If not, rewind the tape, and
		 * space forward.
		 */
		if (!(mt_status.mt_flags & MTF_ASF)) {
			/* printf("mt: rewind\n"); */
			mt_status.mt_fileno = 0;
			mt_com.mt_count = 1;
			mt_com.mt_op = MTREW;
			if (ioctl(mtfd, MTIOCTOP, &mt_com) < 0) {
				(void) fprintf(stderr, "%s %s %d ",
					tape, comp->c_name, mt_fileno);
				perror("mt");
				exit(2);
			}
		}
		if (mt_fileno < mt_status.mt_fileno) {
			mt_com.mt_op = MTNBSF;
			mt_com.mt_count =  mt_status.mt_fileno - mt_fileno;
			/* printf("mt: bsf= %d\n", mt_com.mt_count); */
		} else {
			mt_com.mt_op = MTFSF;
			mt_com.mt_count =  mt_fileno - mt_status.mt_fileno;
			/* printf("mt: fsf= %d\n", mt_com.mt_count); */
		}
		if (ioctl(mtfd, MTIOCTOP, &mt_com) < 0) {
			(void) fprintf(stderr, "%s %s %d ", tape, comp->c_name,
				mt_fileno);
			perror("failed");
			exit(2);
		}

	/* Handle regular mag tape ioctls */
	} else if (comp->c_code != MTNOP) {
		int usecnt;

		mt_com.mt_op = comp->c_code;
		usecnt = argc > 2 && comp->c_usecnt;
		mt_com.mt_count = (usecnt ? atoi(argv[2]) : 1);
		if (mt_com.mt_count < 0) {
			(void) fprintf(stderr, "mt: negative repeat count\n");
			exit(1);
		}
		if (ioctl(mtfd, MTIOCTOP, &mt_com) < 0) {
			(void) fprintf(stderr, "%s %s %ld ", tape, comp->c_name,
				mt_com.mt_count);
			perror("failed");
			exit(2);
		}

	/* Handle status ioctl */
	} else {
		if (ioctl(mtfd, MTIOCGET, (char *)&mt_status) < 0) {
			perror("mt");
			exit(2);
		}
		status(&mt_status);
	}
	return (0);
}


/*
 * Interpret the status buffer returned
 */
static void
status(bp)
	register struct mtget *bp;
{
	register struct mt_tape_info *mt = NULL;
	struct mtdrivetype mdt;
	struct mtdrivetype_request mdt_req;
	char *name = (char *)NULL;

	/*
	 * Make a call to MTIOCGETDRIVETYPE ioctl, Also use old method
	 * of MT_TAPE_INFO for now, but MT_TAPE_INFO should dissapear in 2.7
	 */
	mdt_req.size = sizeof (struct mtdrivetype);
	mdt_req.mtdtp = &mdt;

	if (ioctl(mtfd, MTIOCGETDRIVETYPE, &mdt_req) == 0) {
		name = mdt.name;
	}

	for (mt = tapes; mt->t_type; mt++) {
		if (mt->t_type == bp->mt_type) {
			break;
		}
	}

	/* Handle SCSI tape drives specially. */
	if ((bp->mt_flags & MTF_SCSI)) {
		if (name == (char *)NULL) {
			if (mt->t_type == 0)
				name = "SCSI";
			else
				name = mt->t_name;
		}


		(void) printf("%s tape drive:\n", name);

		(void) printf("   sense key(0x%x)= %s   residual= %ld   ",
			bp->mt_erreg, print_key(bp->mt_erreg), bp->mt_resid);
		(void) printf("retries= %d\n", bp->mt_dsreg);
		(void) printf("   file no= %ld   block no= %ld\n",
			bp->mt_fileno, bp->mt_blkno);
	} else {
		/* Handle non-SCSI drives here. */
		if (mt->t_type == 0) {
			(void) printf("unknown tape drive type (0x%x)\n",
			    bp->mt_type);
			return;
		}
		(void) printf("%s tape drive:\n   residual= %ld", mt->t_name,
		    bp->mt_resid);
		printreg("   ds", (u_short)bp->mt_dsreg, mt->t_dsbits);
		printreg("   er", (u_short)bp->mt_erreg, mt->t_erbits);
		(void) putchar('\n');
	}
}


#ifdef	sun
/*
 * Define SCSI sense key error messages.
 *
 * The first 16 sense keys are SCSI standard
 * sense keys. The keys after this are
 * Sun Specifice 'sense' keys- e.g., crap.
 */

static char *standard_sense_keys[16] = {
	"No Additional Sense",		/* 0x00 */
	"Soft Error",			/* 0x01 */
	"Not Ready",			/* 0x02 */
	"Media Error",			/* 0x03 */
	"Hardware Error",		/* 0x04 */
	"Illegal Request",		/* 0x05 */
	"Unit Attention",		/* 0x06 */
	"Write Protected",		/* 0x07 */
	"Blank Check",			/* 0x08 */
	"Vendor Unique",		/* 0x09 */
	"Copy Aborted",			/* 0x0a */
	"Aborted Command",		/* 0x0b */
	"Equal Error",			/* 0x0c */
	"Volume Overflow",		/* 0x0d */
	"Miscompare Error",		/* 0x0e */
	"Reserved"			/* 0x0f */
};

static char *sun_sense_keys[] = {
	"fatal",			/* 0x10 */
	"timeout",			/* 0x11 */
	"EOF",				/* 0x12 */
	"EOT",				/* 0x13 */
	"length error",			/* 0x14 */
	"BOT",				/* 0x15 */
	"wrong tape media",		/* 0x16 */
	0
};

/*
 * Return the text string associated with the sense key value.
 */
static char *
print_key(short key_code)
{
	static char unknown[32];
	short i;
	if (key_code >= 0 && key_code <= 0x10) {
		return (standard_sense_keys[key_code]);
	}

	i = 0;
	while (sun_sense_keys[i]) {
		if ((i + 0x10) == key_code) {
			return (sun_sense_keys[i]);
		} else i++;
	}
	(void) sprintf(unknown, "unknown sense key: 0x%x",
	    (unsigned int) key_code);
	return (unknown);
}
#endif


/*
 * Print a register a la the %b format of the kernel's printf
 */
static void
printreg(char *s, register u_short v, register char *bits)
{
	register int i, any = 0;
	register char c;

	if (bits && *bits == 8)
		(void) printf("%s = %o", s, v);
	else
		(void) printf("%s = %x", s, v);
	bits++;
	if (v && bits) {
		(void) putchar('<');
		while ((i = *bits++) != 0) {
			if (v & (1 << (i-1))) {
				if (any)
					(void) putchar(',');
				any = 1;
				for (; (c = *bits) > 32; bits++)
					(void) putchar(c);
			} else
				for (; *bits > 32; bits++)
					;
		}
		(void) putchar('>');
	}
}
