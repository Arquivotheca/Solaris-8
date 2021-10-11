/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)lp.cat.c	1.17	98/06/26 SMI"	/* SVr4.0 1.9	*/

#include "stdio.h"
#include "termio.h"
#include "sys/types.h"
#include "errno.h"
#include "signal.h"
#include "sys/times.h"
#include "string.h"
#include "limits.h"

#include "lp.h"

#include <locale.h>

/**
 **	Begin Sun Additions for Parallel ports
 **/

extern char *_sys_errlist[];
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioccom.h>
#include <sys/ioctl.h>

#include <sys/bpp_io.h>
#include <sys/ecppsys.h>
#include <stropts.h>

/* for SPIF Parallel port */
#define	sIOC		('s'<<8)
#define	STC_SPPC	(sIOC|253)
#define	STC_GPPC	(sIOC|254)

#define	PP_PAPER_OUT	0x00001
#define	PP_ERROR	0x00002
#define	PP_BUSY		0x00004
#define	PP_SELECT	0x00008

/*
 * the parameter structure for the parallel port
 */
struct ppc_params_t {
	int		flags;		/* same as above */
	int		state;		/* status of the printer interface */
	int		strobe_w;	/* strobe width, in uS */
	int		data_setup;	/* data setup time, in uS */
	int		ack_timeout;	/* ACK timeout, in secs */
	int		error_timeout;	/* PAPER OUT, etc... timeout, in secs */
	int		busy_timeout;	/* BUSY timeout, in seconds */
};



static void printer_info(char *fmt, ...);

/*	These are the routines avaliable to others for use 	*/
int is_a_parallel_spif(int);
int spif_state(int);
int is_a_parallel_bpp(int);
int bpp_state(int);
int parallel_comm(int, int());
int get_ecpp_status(int fd);

#define PRINTER_ERROR_PAPER_OUT		1
#define PRINTER_ERROR_OFFLINE		2
#define PRINTER_ERROR_BUSY		3
#define PRINTER_ERROR_ERROR		4
#define PRINTER_ERROR_CABLE_POWER	5
#define PRINTER_ERROR_UNKNOWN		6
#define PRINTER_ERROR_TIMEOUT		7
#define	PRINTER_IO_ERROR		129

/**
 *	for SPIF PARALLEL interfaces
 **/

#if defined(DEBUG) && defined(NOTDEF)
char *SpifState(int state)
{
	static char buf[BUFSIZ];

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "State (0x%.4x) - (%s%s%s%s)\n", state,
		((state & PP_SELECT) ? "offline " : ""),
		((state & PP_BUSY) ? "busy " : ""),
		((state & PP_PAPER_OUT) ? "paper " : ""),
		((state & PP_ERROR) ? "error " : ""));

	return(buf);
}
#endif


is_a_parallel_spif(int fd)
{
	struct ppc_params_t pp;

	if (ioctl(fd, STC_GPPC, &pp) == 0  || errno == EIO) 
		return(1);
	return(0);
}



int spif_state(int fd)
{
	static int  init_timeouts = 1;
	struct ppc_params_t pp;

	if (init_timeouts) {	/* reset the timeout values to INFINITE */
		ioctl(fd, STC_GPPC, &pp);
		pp.busy_timeout = pp.busy_timeout = pp.busy_timeout = 0;
		ioctl(fd, STC_SPPC, &pp);
		init_timeouts = 0;
	}

	alarm(5);
	if (ioctl(fd, STC_GPPC, &pp) == 0) {
		int state;
		state = pp.state & 0xff;	 /* only driver status */

		alarm(0);	
#if defined(DEBUG) && defined(NOTDEF)
		logit("0x%x : %s", state, SpifState(pp.state));
#endif
	
		if (state == (PP_PAPER_OUT | PP_ERROR | PP_SELECT)) {
			/* paper is out */
			return(PRINTER_ERROR_PAPER_OUT);
		} else if (state & PP_BUSY) {
			/* printer is busy */
			return(PRINTER_ERROR_BUSY);
		} else if (state & PP_SELECT) {
			/* printer is offline */
			return(PRINTER_ERROR_OFFLINE);
		} else if (state & PP_ERROR) {
			/* printer is errored */
			return(PRINTER_ERROR_ERROR);
		} else if (state == PP_PAPER_OUT) {
			/* printer is off/unplugged */
			return(PRINTER_ERROR_CABLE_POWER);
		} else if (state) {
			/* unknown error */
			return(PRINTER_ERROR_UNKNOWN);
		} else
			return(0);
	} else if (errno == EINTR) {
		/* ioctl timed out */
		return(PRINTER_ERROR_TIMEOUT);
	}
	alarm(0);
	printer_info("ioctl(%d, STC_GPPS, &pp): %s\n", fd, _sys_errlist[errno]);
	return(-1);
}

/****************************************************************************/

/**
 *	for BPP PARALLEL interfaces
 **/

int is_a_parallel_bpp(int fd)
{
	if (ioctl(fd, BPPIOC_TESTIO) == 0 || errno == EIO)
		return(1);
	return(0);
}


#if defined(DEBUG) && defined(NOTDEF)
char *BppState(int state)
{
	static char buf[BUFSIZ];

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "State (0x%.4x) - (%s%s%s%s)\n", state, 
		((state & BPP_SLCT_ERR) ?  "offline " : ""),
		((state & BPP_BUSY_ERR) ?  "busy " : ""),
		((state & BPP_PE_ERR) ?  "paper " : ""),
		((state & BPP_ERR_ERR) ?  "error " : ""));

	return(buf);
}
#endif

int bpp_state(int fd)
{
	if (ioctl(fd, BPPIOC_TESTIO)) {
		struct bpp_error_status  bpp_stat;
		int state;

		ioctl(fd, BPPIOC_GETERR, &bpp_stat);
		state = bpp_stat.pin_status;

#if defined(DEBUG) && defined(NOTDEF)	
		logit("%s", BppState(state));
#endif
	
		if (state == (BPP_PE_ERR | BPP_ERR_ERR | BPP_SLCT_ERR)) {
			/* paper is out */
			return(PRINTER_ERROR_PAPER_OUT);
		} else if (state & BPP_BUSY_ERR) {
			/* printer is busy */
			return(PRINTER_ERROR_BUSY);
		} else if (state & BPP_SLCT_ERR) {
			/* printer is offline */
			return(PRINTER_ERROR_OFFLINE);
		} else if (state & BPP_ERR_ERR) {
			/* printer is errored */
			return(PRINTER_ERROR_ERROR);
		} else if (state == BPP_PE_ERR) {
			/* printer is off/unplugged */
			return(PRINTER_ERROR_CABLE_POWER);
		} else if (state) {
			return(PRINTER_ERROR_UNKNOWN);
		} else
			return(0);
	}
	return(0);
}

/*
 * For ecpp parallel port
 */

int 
get_ecpp_status(int fd)
{
	int state;
	struct ecpp_transfer_parms transfer_parms;


	if (ioctl(fd, ECPPIOC_GETPARMS, &transfer_parms) == -1) {
		return(-1);
	}

	state = transfer_parms.mode;
	/*
	 * We don't know what all printers will return in
	 * nibble more, therefore if we support nibble mode we will
	 * force the printer to be in CENTRONICS mode.
	 */

	if (state == ECPP_NIBBLE_MODE) {
		transfer_parms.mode = ECPP_CENTRONICS;
		if (ioctl(fd, ECPPIOC_SETPARMS, &transfer_parms) == -1) {
			return(-1);
		} else {
			state = ECPP_CENTRONICS;
		}
	}
		

	return(state);
}
	
/**
 *	Common routines
 **/

/*ARGSUSED0*/
static void
ByeByeParallel(int sig)
{
	/* try to shove out the EOT */
	(void) write(1, "\004", 1);
	exit(0);
}


/*ARGSUSED0*/
static void
printer_info(char *fmt, ...)
{
	char mesg[BUFSIZ];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(mesg, fmt, ap);
	va_end(ap);
/*
	fprintf(stderr,
		"%%%%[ PrinterError: %s; source: parallel ]%%%%\n",
		mesg);
*/
	fprintf(stderr, "%s\n", mesg);
	fflush(stderr);
	fsync(2);

}

static void
printer_error(int error)
{
	switch (error) {
		case -1:
			printer_info("ioctl(): %s", _sys_errlist[errno]);
			break;
		case PRINTER_ERROR_PAPER_OUT:
			printer_info("out of paper");
			break;
		case PRINTER_ERROR_OFFLINE:
			printer_info("offline");
			break;
		case PRINTER_ERROR_BUSY:
			printer_info("busy");
			break;
		case PRINTER_ERROR_ERROR:
			printer_info("printer error");
			break;
		case PRINTER_ERROR_CABLE_POWER:
			printer_info("printer powered off or disconnected");
			break;
		case PRINTER_ERROR_UNKNOWN:
			printer_info("unknown error");
			break;
		case PRINTER_ERROR_TIMEOUT:
			printer_info("communications timeout");
			break;
		default:
			printer_info("get_status() failed");
	}
}


static void
wait_state(int fd, int get_state())
{
	int state;
	int was_faulted = 0;

	while (state = get_state(fd)) {
		was_faulted=1;
		printer_error(state);
		sleep(15);
	}

	if (was_faulted) {
		fprintf(stderr, "printer ok\n");
		fflush(stderr);
		fsync(2);
	}
}

/**
 **  end of Sun Additions for parallel port
 **/
#define	IDENTICAL(A,B)	(A.st_dev==B.st_dev && A.st_ino==B.st_ino)
#define ISBLK(A)	((A.st_mode & S_IFMT) == S_IFBLK)
#define ISCHR(A)	((A.st_mode & S_IFMT) == S_IFCHR)

#define E_SUCCESS	0
#define E_BAD_INPUT	1
#define E_BAD_OUTPUT	2
#define E_BAD_TERM	3
#define E_IDENTICAL	4
#define	E_WRITE_FAILED	5
#define	E_TIMEOUT	6
#define E_HANGUP	7
#define E_INTERRUPT	8

#define SAFETY_FACTOR	2.0
#define R(F)		(int)((F) + .5)
#define DELAY(N,D)	R(SAFETY_FACTOR * ((N) / (double)(D)))

extern int		sys_nerr;

extern char		*sys_errlist[],
			*getenv();

extern int		atoi();

char			buffer[BUFSIZ];

void			sighup(),
			sigint(),
			sigquit(),
			sigpipe(),
			sigalrm(),
			sigterm();

#if	defined(baudrate)
# undef	baudrate
#endif

int			baudrate();


int nop(int fd) { return (0); }
int spif_state(int);
int bpp_state(int);


/**
 ** main()
 **/

int			main (argc, argv)
	int			argc;
	char			*argv[];
{
	register int		nin,
				nout,
				effective_rate,
				max_delay	= 0,
				n;

	int			report_rate;

	short			print_rate;

	struct stat		in,
				out;

	struct tms		tms;

	long			epoch_start,
				epoch_end;

	char			*TERM;

	int			(*func)(int fd);


	/*
	 * The Spooler can hit us with SIGTERM for three reasons:
	 *
	 *	- the user's job has been canceled
	 *	- the printer has been disabled while we were printing
	 *	- the Spooler heard that the printer has a fault,
	 *	  and the fault recovery is wait or beginning
	 *
	 * We should exit cleanly for the first two cases,
	 * but we have to be careful with the last. If it was THIS
	 * PROGRAM that told the Spooler about the fault, we must
	 * exit consistently.
	 *
	 * The method of avoiding any problem is to turn off the
	 * trapping of SIGTERM before telling the Spooler about
	 * the fault.
	 *
	 * Faults that we can detect:
	 *	- hangup (drop of carrier)
	 *	- interrupt (printer sent a break or quit character)
	 *	- SIGPIPE (output port is a FIFO, and was closed early)
	 *	- failed or incomplete write()
	 *	- excess delay in write() (handled with SIGALRM later)
	 *
	 * Pseudo-faults (errors in use):
	 *	- No input/output, or strange input/output
	 *	- Input/output identical
	 *	- No TERM defined or trouble reading Terminfo database
	 */
	signal (SIGTERM, sigterm);
	signal (SIGHUP, sighup);
	signal (SIGINT, sigint);
	signal (SIGQUIT, sigint);
	signal (SIGPIPE, sigpipe);


	if (argc > 1 && STREQU(argv[1], "-r")) {
		report_rate = 1;
		argc--;
		argv++;
	} else
		report_rate = 0;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	/*
	 * Stat the standard output to be sure it is defined.
	 */
	if (fstat(1, &out) < 0) {
		signal (SIGTERM, SIG_IGN);
		fprintf (
			stderr,
		gettext("Can't stat output (%s);\nincorrect use of lp.cat!\n"),
			PERROR
		);
		exit (E_BAD_OUTPUT);
	}

	/*
	 * Stat the standard input to be sure it is defined.
	 */
	if (fstat(0, &in) < 0) {
		signal (SIGTERM, SIG_IGN);
		fprintf (
			stderr,
		gettext("Can't stat input (%s);\nincorrect use of lp.cat!\n"),
			PERROR
		);
		exit (E_BAD_INPUT);
	}

	/*
	 * If the standard output is not a character special file or a
	 * block special file, make sure it is not identical to the
	 * standard input.
	 *
	 * If we are an ecpp parallel port in centronics mode treat
	 * ourselves as a bpp compatible device.
	 */

	if (is_a_parallel_bpp(1) || get_ecpp_status(1) == ECPP_CENTRONICS) {
		func = bpp_state;
        } else if (is_a_parallel_spif(1)) {
		func = spif_state;
        } else if (isatty(1)) {
		/* serial connection (probably) - continue as usual */
		func = nop;
        } else {
		func = nop;
        }

	if (!ISCHR(out) && !ISBLK(out) && IDENTICAL(out, in)) {
		signal (SIGTERM, SIG_IGN);
		fprintf (
			stderr,
	gettext("Input and output are identical; incorrect use of lp.cat!\n")
		);
		exit (E_IDENTICAL);
	}

	/*
	 * The effective data transfer rate is the lesser
	 * of the transmission rate and print rate. If an
	 * argument was passed to us, it should be a data
	 * rate and it may be lower still.
	 * Based on the effective data transfer rate,
	 * we can predict the maximum delay we should experience.
	 * But there are other factors that could introduce
	 * delay, so let's be generous; after all, we'd rather
	 * err in favor of waiting too long to detect a fault
	 * than err too often on false alarms.
	 */

	if (
		!(TERM = getenv("TERM"))
	     || !*TERM
	) {
		signal (SIGTERM, SIG_IGN);
		fprintf (
			stderr,
	gettext("No TERM variable defined! Trouble with the Spooler!\n")
		);
		exit (E_BAD_TERM);
	}
	if (
		!STREQU(TERM, NAME_UNKNOWN)
	     && tidbit(TERM, "cps", &print_rate) == -1
	) {
		signal (SIGTERM, SIG_IGN);
		fprintf (
			stderr,
gettext("Trouble identifying printer type \"%s\"; check the Terminfo database.\n"),
			TERM
		);
		exit (E_BAD_TERM);
	}
	if (STREQU(TERM, NAME_UNKNOWN))
		print_rate = -1;

	effective_rate = baudrate() / 10; /* okay for most bauds */
	if (print_rate != -1 && print_rate < effective_rate)
		effective_rate = print_rate;
	if (argc > 1 && (n = atoi(argv[1])) >= 0 && n < effective_rate)
		effective_rate = n;	  /* 0 means infinite delay */
	if (effective_rate)
		max_delay = DELAY(BUFSIZ, effective_rate);

	/*
	 * We'll use the "alarm()" system call to keep us from
	 * waiting too long to write to a printer in trouble.
	 */
	if (max_delay)
		signal (SIGALRM, sigalrm);

	/*
	 * While not end of standard input, copy blocks to
	 * standard output.
	 */
	while ((nin = read(0, buffer, BUFSIZ)) > 0) {
		char *ptr = buffer;

		/*
		 * We should be safe from incomplete writes to a full
		 * pipe, as long as the size of the buffer we write is
		 * a even divisor of the pipe buffer limit. As long as
		 * we read from files or pipes (not communication devices)
		 * this should be true for all but the last buffer. The
		 * last will be smaller, and won't straddle the pipe max
		 * limit (think about it).
		 */
#if	PIPE_BUF < BUFSIZ || (PIPE_MAX % BUFSIZ)
		this_wont_compile;
#endif
		if (report_rate)
			epoch_start = times(&tms);
		do {
			wait_state(1, func);

			if (max_delay)
				alarm (max_delay);
			nout = write(1, ptr, nin);
			alarm(0);
			if (nout < 0) {
				fprintf (
					stderr,
	gettext("Write failed (%s);\nperhaps the printer has gone off-line.\n"),
					PERROR
				);
				fflush(stderr);
				if (errno == EIO)
				/* I/O error on device, get lpcshed to retry */
					exit(PRINTER_IO_ERROR);
				else /* wait for printer to come back online */
					sleep(15);
			} else {
				nin -= nout;
				ptr += nout;
			}
		} while (nin > 0);

		if (max_delay)
			alarm (0);
		else if (report_rate) {
			epoch_end = times(&tms);
			if (epoch_end - epoch_start > 0)
				fprintf (
					stderr,
					"%d CPS\n",
		R((100 * BUFSIZ) / (double)(epoch_end - epoch_start))
				);
		}

	}

	exit (E_SUCCESS);
	/*NOTREACHED*/
}

/**
 ** sighup() - CATCH A HANGUP (LOSS OF CARRIER)
 **/

void			sighup ()
{
	char			*msg = HANGUP_FAULT;

	
	signal (SIGTERM, SIG_IGN);
	signal (SIGHUP, SIG_IGN);
	msg[strlen(msg) - 2] = '.';
	fprintf (stderr, gettext(msg));
	exit (E_HANGUP);
}

/**
 ** sigint() - CATCH AN INTERRUPT
 **/

void			sigint ()
{
	signal (SIGTERM, SIG_IGN);
	signal (SIGINT, SIG_IGN);
	fprintf (stderr, gettext(INTERRUPT_FAULT));
	exit (E_INTERRUPT);
}

/**
 ** sigpipe() - CATCH EARLY CLOSE OF PIPE
 **/

void			sigpipe ()
{
	signal (SIGTERM, SIG_IGN);
	signal (SIGPIPE, SIG_IGN);
	fprintf (stderr, gettext(PIPE_FAULT));
	exit (E_INTERRUPT);
}

/**
 ** sigalrm() - CATCH AN ALARM
 **/

void			sigalrm ()
{
	signal (SIGTERM, SIG_IGN);
	fprintf (
		stderr,
	gettext("Excessive write delay; perhaps the printer has gone off-line.\n")
	);
	exit (E_TIMEOUT);
}

/**
 ** sigterm() - CATCH A TERMINATION SIGNAL
 **/

void			sigterm ()
{
	signal (SIGTERM, SIG_IGN);
	/*
	 * try to flush the output queue in the case of ecpp port.
	 * ignore the return code as this may not be the ecpp.
	 */
	ioctl(1, I_FLUSH, FLUSHW);
	exit (E_SUCCESS);
}

/**
 ** baudrate() - RETURN BAUD RATE OF OUTPUT LINE
 **/

static int		baud_convert[] =
{
	0, 50, 75, 110, 135, 150, 200, 300, 600, 1200,
	1800, 2400, 4800, 9600, 19200, 38400, 57600,
	76800, 115200, 153600, 230400, 307200, 460800
};

int			baudrate ()
{
	struct termio		tm;
	struct termios		tms;
	int			speed;

	if (ioctl(1, TCGETS, &tms) < 0) {
		if (ioctl(1, TCGETA, &tm) < 0)
			return (1200);
		else
			speed = tm.c_cflag&CBAUD;
	} else
		speed = cfgetospeed(&tms);

	return (speed ? baud_convert[speed] : 1200);
}
