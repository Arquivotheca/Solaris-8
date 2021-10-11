/*
 * Copyright 1991 Sun Microsystems, Inc.
 */

#ifndef lint
static char sccsid[] = "@(#)parallel.c 1.8 95/10/31";
#endif

extern char *postbegin;

#include <stdio.h>
#include <errno.h>
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



extern char *block;
extern int head, tail;
extern int readblock(int);
extern FILE *fp_log;
static void printer_info(char *fmt, ...);

/*	These are the routines avaliable to others for use 	*/
int is_a_parallel_spif(int);
int spif_state(int);
int is_a_parallel_bpp(int);
int bpp_state(int);
int parallel_comm(int, int());
int get_ecpp_status(int);

#define PRINTER_ERROR_PAPER_OUT		1
#define PRINTER_ERROR_OFFLINE		2
#define PRINTER_ERROR_BUSY		3
#define PRINTER_ERROR_ERROR		4
#define PRINTER_ERROR_CABLE_POWER	5
#define PRINTER_ERROR_UNKNOWN		6
#define PRINTER_ERROR_TIMEOUT		7

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

	fprintf(stderr,
		"%%%%[ PrinterError: %s; source: parallel ]%%%%\n",
		mesg);
	fflush(stderr);
	fsync(2);

	if (fp_log != stderr) {
		fprintf(fp_log,
		   "%%%%[ PrinterError: %s; source: parallel ]%%%%\n",
		   mesg);
		fflush(fp_log);
	}
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
		fprintf(stderr, "%%%%[ status: idle ]%%%%\n");
		fflush(stderr);
		fsync(2);
		if (fp_log != stderr) {
			fprintf(fp_log, "%%%%[ status: idle ]%%%%\n");
			fflush(fp_log);
		}
	}
}


int
parallel_comm(int fd, int get_state())
{
	int  actual;		/* number of bytes successfully written */
	int count = 0;

	(void) signal(SIGTERM, ByeByeParallel);
	(void) signal(SIGQUIT, ByeByeParallel);
	(void) signal(SIGHUP, ByeByeParallel);
	(void) signal(SIGINT, ByeByeParallel);
	(void) signal(SIGALRM, SIG_IGN);

	/* is the device ready? */

	/* bracket job with EOT */
	wait_state(fd, get_state);
	(void) write(fd, "\004", 1);

/* 	write(fd, postbegin, strlen(postbegin)); */

	while (readblock(fileno(stdin)) > 0) {
		wait_state(fd, get_state);
		alarm(120);
		if ((actual = write(fd, block + head, tail - head)) == -1) {
			alarm(0);
		  	if (errno == EINTR) {
				printer_error(PRINTER_ERROR_TIMEOUT);
				sleep(30);
				continue;
			} else if (errno == EIO) {
				printer_info("I/O Error during write()");
				exit(2);
			}
		}
		alarm(0);
		if (actual >= 0)
			head += actual;

#if defined(DEBUG) && defined(NOTDEF)
		logit("Writing (%d) at 0x%x actual: %d, %s\n", count++, head,
			actual, (actual < 1 ? _sys_errlist[errno] : ""));
#endif
	}

	/* write the final EOT */
	wait_state(fd, get_state);
	(void) write(fd, "\004", 1);

	return (0);
}
