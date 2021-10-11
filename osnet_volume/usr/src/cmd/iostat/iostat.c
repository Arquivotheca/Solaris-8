/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * rewritten from UCB 4.13 83/09/25
 * rewritten from SunOS 4.1 SID 1.18 89/10/06
 */

#pragma ident	"@(#)iostat.c	1.30	99/12/01 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <memory.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <inttypes.h>
#include <strings.h>
#include <sys/dklabel.h>
#include <sys/dktp/fdisk.h>
#include <sys/systeminfo.h>

#include "iostat.h"

kstat_ctl_t	*kc;		/* libkstat cookie */
static	uint_t	 ncpus;
static  struct cpu_sinfo *cpu_stat_list;

static	diskinfo_t 	*prevdisk;
static	diskinfo_t 	*firstdisk;
static  con_t 		*ctl_list;

static	cpu_stat_t	ocp_stat;	/* cpu stats at beginning of interval */
static	cpu_stat_t	ncp_stat;	/* cpu stats at end of interval */

static	char	*cmdname;

static char *one_blank = " ";
static char *two_blanks = "  ";

/*
 * count for number of lines to be emitted before a header is
 * shown again. Only used for the basic format.
 */
static	uint_t	tohdr = 1;

/*
 * If we're in raw format, have we printed a header? We only do it
 * once for raw but we emit it every REPRINT lines in non-raw format.
 * This applies only for the basic header. The extended header is
 * done only once in both formats.
 */
static	uint_t	hdr_out;

/*
 * Flags representing arguments from command line
 */
static	uint_t	do_tty;			/* show tty info (-t) */
static	uint_t	do_disk;		/* show disk info per selected */
					/* format (-d, -D, -e, -E, -x) */
static	uint_t	do_cpu;			/* show cpu info (-c) */
static	uint_t	do_interval;		/* do intervals (-I) */
static	int	do_partitions;		/* per-partition stats (-p) */
static	int	do_partitions_only;	/* per-partition stats only (-P) */
					/* no per-device stats for disks */
static	uint_t	do_conversions;		/* display disks as cXtYdZ (-n) */
static	uint_t	do_megabytes;		/* display data in MB/sec (-M) */
static  uint_t	do_controller;		/* display controller info (-C) */
static  uint_t	do_raw;			/* emit raw format (-r) */
static  uint_t	do_timestamp;		/* timestamp  each display (-T) */

/*
 * Definition of allowable types of timestamps
 */
#define	CDATE 1
#define	UDATE 2

/*
 * Default number of disk drives to be displayed in basic format
 */
#define	DEFAULT_LIMIT	4

static	uint_t	limit;			/* display no more than this */
					/* number of drives. (-l) */
static  uint_t	suppress_state;		/* skip state change messages */
static	uint_t	suppress_zero;		/* skip zero valued lines */
static  uint_t	show_mountpts;		/* show mount points */
static	int 	interval;		/* interval (seconds) to output */
static	int 	iter;			/* iterations from command line */

/*
 * Structure describing a disk explicitly selected for display on
 * the command line. See do_args() for setup.
 */
struct	disk_selection {
	struct disk_selection *next;
	char ks_name[KSTAT_STRLEN];
};

/*
 * Head of list of selected disk structs
 */
static	struct disk_selection *disk_selections;

#define	SMALL_SCRATCH_BUFLEN	64
#define	MED_SCRATCH_BUFLEN	257
#define	LARGE_SCRATCH_BUFLEN	1024
#define	DISPLAYED_NAME_FORMAT "%-9.9s"

static struct io_class io_class[] = {
	{ "disk",	IO_CLASS_DISK},
	{ "partition",	IO_CLASS_PARTITION},
	{ "tape",	IO_CLASS_TAPE},
	{ "nfs",	IO_CLASS_NFS},
	{ NULL,		0}
};

#define	DISK_HEADER_LEN	132
static	char	disk_header[DISK_HEADER_LEN];
static	uint_t 	dh_len;			/* disk header length for centering */
static  int 	lineout;		/* data waiting to be printed? */

/*
 * variables which capture information relative to state changes.
 *
 * num_old_devs != 0 -> one or more disk devices aren't here anymore
 * num_new_devs != 0 -> one or more new devices have shown up
 * new_names -> pointer to list of new disk device names
 * old_names -> pointer to list of removed disk devices
 *
 * num_old_mts != 0 -> one or more unmounts have occurred
 * num_new_mts != 0 -> one or more new mounts have occurred
 * new_mts -> pointer to list of new mountpt names
 * old_mts -> pointer to list of removed mounts
 */
typedef struct state_change {
	uint_t 		num_old_devs;
	uint_t 		num_new_devs;
	char 		**new_names;
	char 		**old_names;
	uint_t 		num_old_mts;
	uint_t 		num_new_mts;
	char 		**new_mts;
	char 		**old_mts;
} schange_t;

extern dir_info_t dlist[];  /* current list of devices attached */

static	double	getime;		/* elapsed time */
static	double	percent;	/* 100 / etime */
static	uint_t 	skip_first;	/* do we skip the first disk in the list? */

/*
 * List of functions to be called which will construct the desired output
 */
static format_t	*formatter_list;
static format_t *formatter_end;

static char *cpu_events[] = {
	"<<cpu[s] taken offline: ",
	"<<cpu[s] brought online: ",
};

#define	NUM_CPU_EVENTS (sizeof (cpu_events)/sizeof (char *))

static uint64_t	hrtime_delta(hrtime_t, hrtime_t);
static u_longlong_t	ull_delta(u_longlong_t, u_longlong_t);
static uint_t 	u32_delta(uint_t, uint_t);
static void display(char **, char *, uint_t);
static void setup(void (*nfunc)(void));
static void print_timestamp(void);
static void print_tty_hdr1(void);
static void print_tty_hdr2(void);
static void print_cpu_hdr1(void);
static void print_cpu_hdr2(void);
static void print_tty_data(void);
static void print_cpu_data(void);
static void print_err_hdr(void);
static void print_disk_header(void);
static void disk_dump(void);
static void controller_dump(void);
static void hdrout(void);
static void disk_errors(void);
static void do_showdisk(void);
static void check_for_changes(void);
static void do_newline(void);
static void push_out(char *, ...);
static void printhdr(int);
static void printxhdr(void);
static int  show_disk(diskinfo_t *);
static char *cpu_stat_init(void);
static int  cpu_stat_load(void);
static void usage(void);
static void init_disks(void);
static void select_disks(void);
static void diskinfo_load(void);
static void init_disk_errors(void);
static void show_disk_errors(diskinfo_t *);
static void find_disk(kstat_t *);
static void diff_two_sets(struct cpu_sinfo *, uint_t,
    struct cpu_sinfo *, uint_t, int *, int *);
static uint_t diff_two_lists(diskinfo_t *, diskinfo_t *, schange_t *);
static void do_args(int, char **);
static void do_format(char *);
static void cleanup_disk_list(diskinfo_t *);
static void add_controller(diskinfo_t *);
static void free_controller_list(void);
static void write_core_header(void);
static void do_disk_name(kstat_t *, diskinfo_t *);
static void safe_zalloc(void **, uint_t, int);
static void set_timer(int);
static void handle_sig(int);

int
main(int argc, char **argv)
{
	int i;
	char	err_header[SMALL_SCRATCH_BUFLEN];
	long	hz;
	uint_t  deltas;

	if ((kc = kstat_open()) == NULL)
		fail(1, "kstat_open(): can't open /dev/kstat");

	cmdname = argv[0];
	(void) memset(&ncp_stat, 0, sizeof (cpu_stat_t));
	do_args(argc, argv);
	do_format(err_header);

	hz = sysconf(_SC_CLK_TCK);
	if (do_cpu || (do_interval == 0))
		(void) cpu_stat_init();
	if (do_disk) {
		if (do_conversions)
			do_mnttab();
		init_disks();
	}

	/*
	 * Undocumented behavior - sending a SIGCONT will result
	 * in a new header being emitted. Used only if we're not
	 * doing extended headers. This is a historical
	 * artifact.
	 */
	if (!(do_disk & PRINT_VERTICAL))
		(void) signal(SIGCONT, printhdr);

	if (interval)
		set_timer(interval);
	do {
		if (do_conversions)
			do_mnttab();
		check_for_changes();
		if (do_tty || do_cpu) {
			deltas = 0;
			for (i = 0; i < CPU_STATES; i++)
				deltas += u32_delta(ocp_stat.cpu_sysinfo.cpu[i],
				    ncp_stat.cpu_sysinfo.cpu[i]);
			getime = (double)deltas;
			percent = (getime > 0.0) ? 100.0 / getime : 0.0;
			getime = (getime / ncpus) / hz;
			if (getime == 0.0)
				getime = (double)interval;
			if (getime == 0.0 || do_interval)
				getime = 1.0;
		}
		if (formatter_list) {
			format_t *tmp;
			tmp = formatter_list;
			while (tmp) {
				(tmp->nfunc)();
				tmp = tmp->next;
			}
			(void) fflush(stdout);
		}
		if (interval > 0 && iter != 1)
			(void) pause();
	} while (--iter);

	return (0);
}

/*
 * Some magic numbers used in header formatting.
 *
 * DISK_LEN = length of either "kps tps serv" or "wps rps util"
 *	      using 0 as the first position
 *
 * DISK_ERROR_LEN = length of "s/w h/w trn tot" with one space on
 *		either side. Does not use zero as first pos.
 *
 * DEVICE_LEN = length of "device" + 1 character.
 */

#define	DISK_LEN	11
#define	DISK_ERROR_LEN	16
#define	DEVICE_LEN	7

/*
 * Write out a two line header. What is written out depends on the flags
 * selected but in the worst case consists of a tty header, a disk header
 * providing information for 4 disks and a cpu header.
 *
 * The tty header consists of the word "tty" on the first line above the
 * words "tin tout" on the next line. If present the tty portion consumes
 * the first 10 characters of each line since "tin tout" is surrounded
 * by single spaces.
 *
 * Each of the disk sections is a 14 character "block" in which the name of
 * the disk is centered in the first 12 characters of the first line.
 *
 * The cpu section is an 11 character block with "cpu" centered over the
 * section.
 *
 * The worst case should look as follows:
 *
 * 0---------1--------2---------3---------4---------5---------6---------7-------
 *    tty        sd0           sd1           sd2           sd3           cpu
 *  tin tout kps tps serv  kps tps serv  kps tps serv  kps tps serv  us sy wt id
 *  NNN NNNN NNN NNN NNNN  NNN NNN NNNN  NNN NNN NNNN  NNN NNN NNNN  NN NN NN NN
 *
 * When -D is specified, the disk header looks as follows (worst case):
 *
 * 0---------1--------2---------3---------4---------5---------6---------7-------
 *     tty        sd0           sd1             sd2          sd3          cpu
 *   tin tout rps wps util  rps wps util  rps wps util  rps wps util us sy wt id
 *   NNN NNNN NNN NNN NNNN  NNN NNN NNNN  NNN NNN NNNN  NNN NNN NNNN NN NN NN NN
 */
static void
printhdr(int sig)
{
	diskinfo_t *disk;
	size_t slen;
	char fbuf[SMALL_SCRATCH_BUFLEN];
	char *sp;

	/*
	 * If we're here because a signal fired, reenable the
	 * signal.
	 */
	if (sig)
		(void) signal(SIGCONT, printhdr);
	/*
	 * Horizontal mode headers
	 *
	 * First line
	 */
	if (do_tty)
		print_tty_hdr1();

	if (do_disk & DISK_NORMAL) {
		for (disk = firstdisk; disk; disk = disk->next) {
			if (disk->selected) {
				if (disk->device_name)
					sp = disk->device_name;
				else
					sp = disk->ksp->ks_name;
				if (do_raw == 0) {
					slen = strlen(sp);
					if (slen > 0 && slen < DISK_LEN) {
						/*
						 * The length is less
						 * than the section
						 * which will be displayed
						 * on the next line.
						 * Center the entry.
						 */
						uint_t width;

						width = (DISK_LEN + 1)/2
						    + (slen / 2);
						(void) snprintf(fbuf,
						    sizeof (fbuf), "%*s",
						    width, sp);
						sp = fbuf;
					}
					push_out("%-14.14s", sp);
				} else {
					push_out(sp);
				}

			}
		}
	}

	if (do_cpu)
		print_cpu_hdr1();
	do_newline();

	/*
	 * Second line
	 */
	if (do_tty)
		print_tty_hdr2();

	if (do_disk & DISK_NORMAL) {
		for (disk = firstdisk; disk; disk = disk->next)
			if (disk->selected)
				push_out(disk_header);
	}

	if (do_cpu)
		print_cpu_hdr2();
	do_newline();

	tohdr = REPRINT;
}

/*
 * Write out the extended header centered over the core information.
 */
static void
write_core_header(void)
{
	char *edev = "extended device statistics";
	uint_t lead_space_ct;
	uint_t follow_space_ct;
	size_t edevlen;

	if (do_raw == 0) {
		/*
		 * The things we do to look nice...
		 *
		 * Center the core output header. Make sure we have the
		 * right number of trailing spaces for follow-on headers
		 * (i.e., cpu and/or tty and/or errors).
		 */
		edevlen = strlen(edev);
		lead_space_ct = dh_len - edevlen;
		lead_space_ct /= 2;
		if (lead_space_ct > 0) {
			follow_space_ct = dh_len - (lead_space_ct + edevlen);
			if (do_disk & DISK_ERRORS)
				follow_space_ct -= DISK_ERROR_LEN;
			if ((do_disk & DISK_EXTENDED) && do_conversions)
				follow_space_ct -= DEVICE_LEN;

			push_out("%1$*2$.*2$s%3$s%4$*5$.*5$s", one_blank,
			    lead_space_ct, edev, one_blank, follow_space_ct);
		} else
			push_out("%56s", edev);
	} else
		push_out(edev);
}

/*
 * In extended mode headers, we don't want to reprint the header on
 * signals as they are printed every time anyways.
 */
static void
printxhdr(void)
{

	/*
	 * Vertical mode headers
	 */
	if (do_disk & DISK_EXTENDED)
		setup(write_core_header);
	if (do_disk & DISK_ERRORS)
		setup(print_err_hdr);

	if (do_conversions) {
		setup(do_newline);
		if (do_disk & (DISK_EXTENDED | DISK_ERRORS))
			setup(print_disk_header);
		setup(do_newline);
	} else {
		if (do_tty)
			setup(print_tty_hdr1);
		if (do_cpu)
			setup(print_cpu_hdr1);
		setup(do_newline);

		if (do_disk & (DISK_EXTENDED | DISK_ERRORS))
			setup(print_disk_header);
		if (do_tty)
			setup(print_tty_hdr2);
		if (do_cpu)
			setup(print_cpu_hdr2);
		setup(do_newline);
	}
}

/*
 * Write out a line for this disk - note that show_disk writes out
 * full lines or blocks for each selected disk.
 */
static int
show_disk(diskinfo_t *disk)
{
	double rps, wps, tps, mtps, krps, kwps, kps, avw, avr, w_pct, r_pct;
	double wserv, rserv, serv;
	double iosize;	/* kb/sec or MB/sec */
	double etime, hr_etime;
	char *disk_name;
	u_longlong_t ldeltas;
	uint_t udeltas;
	uint64_t t_delta;
	uint64_t w_delta;
	uint64_t r_delta;
	int doit = 1;

	kstat_named_t *knp;
	int i;
	uint_t toterrs;
	char *fstr;

	/*
	 * Only do if we want IO stats - Avoids errors traveling this
	 * section if that's all we want to see.
	 */
	if (do_disk & DISK_IO_MASK) {
		/*
		 * A private protocol exists for controller
		 * disk_info_t structures. If the wcnt field
		 * in the new_kios structure is > 1 then this
		 * structure describes the aggregation of
		 * statistics from the individual disks on
		 * a controller. The wlastupdate field contains
		 * the total number of ns that the individual
		 * controllers used to generate their IO load.
		 * We average that time over the number of
		 * controllers.
		 */
		if (disk->new_kios.wcnt == 1) {
			/*
			 * For a single disk
			 */
			if (disk->last_snap) {
				t_delta = hrtime_delta(disk->last_snap,
				    disk->ksp->ks_snaptime);
			} else {
				t_delta = hrtime_delta(disk->ksp->ks_crtime,
				    disk->ksp->ks_snaptime);
			}
			disk->last_snap = disk->ksp->ks_snaptime;
		} else {
			/*
			 * A synthetic kstat describing a controller
			 */
			t_delta = disk->new_kios.wlastupdate;
			t_delta /= disk->new_kios.wcnt;
		}
		hr_etime = (double)t_delta;
		if (hr_etime == 0.0)
			hr_etime = (double)NANOSEC;
		etime = hr_etime / (double)NANOSEC;

		/* reads per second */
		udeltas = u32_delta(disk->old_kios.reads, disk->new_kios.reads);
		rps = (double)udeltas;
		rps /= etime;

		/* writes per second */
		udeltas = u32_delta(disk->old_kios.writes,
		    disk->new_kios.writes);
		wps = (double)udeltas;
		wps /= etime;

		tps = rps + wps;
			/* transactions per second */

		/*
		 * report throughput as either kb/sec or MB/sec
		 */

		if (!do_megabytes)
			iosize = 1024.0;
		else
			iosize = 1048576.0;

		ldeltas = ull_delta(disk->old_kios.nread, disk->new_kios.nread);
		if (ldeltas) {
			krps = (double)ldeltas;
			krps /= etime;
			krps /= iosize;
		} else
			krps = 0.0;

		ldeltas = ull_delta(disk->old_kios.nwritten,
		    disk->new_kios.nwritten);
		if (ldeltas) {
			kwps = (double)ldeltas;
			kwps /= etime;
			kwps /= iosize;
		} else
			kwps = 0.0;

		/*
		 * Blocks transferred per second
		 */
		kps = krps + kwps;

		/*
		 * Average number of write transactions waiting
		 */
		w_delta = hrtime_delta((u_longlong_t)disk->old_kios.wlentime,
		    (u_longlong_t)disk->new_kios.wlentime);
		if (w_delta) {
			avw = (double)w_delta;
			avw /= hr_etime;
		} else
			avw = 0.0;

		/*
		 * Average number of read transactions waiting
		 */
		r_delta = hrtime_delta(disk->old_kios.rlentime,
		    disk->new_kios.rlentime);
		if (r_delta) {
			avr = (double)r_delta;
			avr /= hr_etime;
		} else
			avr = 0.0;

		/*
		 * Average wait service time in milliseconds
		 */
		if (tps > 0.0 && (avw != 0.0 || avr != 0.0)) {
			mtps = 1000.0 / tps;
			if (avw != 0.0)
				wserv = avw * mtps;
			else
				wserv = 0.0;

			if (avr != 0.0)
				rserv = avr * mtps;
			else
				rserv = 0.0;
			serv = rserv + wserv;
		} else {
			rserv = 0.0;
			wserv = 0.0;
			serv = 0.0;
		}

		/* % of time there is a transaction waiting for service */
		t_delta = hrtime_delta(disk->old_kios.wtime,
		    disk->new_kios.wtime);
		if (t_delta) {
			w_pct = (double)t_delta;
			w_pct /= hr_etime;
			w_pct *= 100.0;
		} else
			w_pct = 0.0;

		/* % of time there is a transaction running */
		t_delta = hrtime_delta(disk->old_kios.rtime,
		    disk->new_kios.rtime);
		if (t_delta) {
			r_pct = (double)t_delta;
			r_pct /= hr_etime;
			r_pct *= 100.0;
		} else {
			r_pct = 0.0;
		}

		/* % of time there is a transaction running */
		if (do_interval) {
			rps	*= etime;
			wps	*= etime;
			tps	*= etime;
			krps	*= etime;
			kwps	*= etime;
			kps	*= etime;
		}
	}
	if (do_disk & (DISK_EXTENDED | DISK_ERRORS)) {
		if (disk->device_name)
			disk_name = disk->device_name;
		else
			disk_name = disk->ksp->ks_name;
		if ((!do_conversions) && ((suppress_zero == 0) ||
		    ((do_disk & DISK_EXTENDED) == 0))) {
			if (do_raw == 0)
				push_out(DISPLAYED_NAME_FORMAT,
				    disk_name);
			else
				push_out(disk_name);
		}
	}
	switch (do_disk & DISK_IO_MASK) {
	    case DISK_OLD:
		if (do_raw == 0)
			fstr = "%3.0f %3.0f %4.0f  ";
		else
			fstr = "%.0f,%.0f,%.0f";
		push_out(fstr, kps, tps, serv);
		break;
	    case DISK_NEW:
		if (do_raw == 0)
			fstr = "%3.0f %3.0f %4.1f  ";
		else
			fstr = "%.0f,%.0f,%.1f";
		push_out(fstr, rps, wps, r_pct);
		break;
	    case DISK_EXTENDED:
		if (suppress_zero) {
			if (rps == 0.0 && wps == 0.0 && krps == 0.0 &&
			    kwps == 0.0 && avw == 0.0 && avr == 0.0 &&
			    serv == 0.0 && w_pct == 0.0 && r_pct == 0.0)
				doit = 0;
			else if (do_conversions == 0) {
				if (do_raw == 0)
					push_out(DISPLAYED_NAME_FORMAT,
						disk_name);
				else
					push_out(disk_name);
			}
		}
		if (doit) {
			if (!do_conversions) {
				if (do_raw == 0) {
					fstr = " %6.1f %6.1f %6.1f %6.1f "
						"%4.1f %4.1f %6.1f %3.0f "
						"%3.0f ";
				} else {
					fstr = "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,"
						"%.1f,%.0f,%.0f";
				}
				push_out(fstr, rps, wps, krps, kwps, avw, avr,
					serv, w_pct, r_pct);
			} else {
				if (do_raw == 0) {
					fstr = " %6.1f %6.1f %6.1f %6.1f "
						"%4.1f %4.1f %6.1f %6.1f "
						"%3.0f %3.0f ";
				} else {
					fstr = "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,"
						"%.1f,%.1f,%.0f,%.0f";
				}
				push_out(fstr, rps, wps, krps, kwps, avw, avr,
					wserv, rserv, w_pct, r_pct);
			}
		}
		break;
	}
	if (do_disk & DISK_ERRORS) {
		if ((do_disk == DISK_ERRORS)) {
			if (do_raw == 0)
				push_out(two_blanks);
		}
		if (disk->disk_errs) {
			char *efstr;

			if (do_raw == 0)
				efstr = "%3d ";
			else
				efstr = "%d";
			toterrs = 0;
			knp = KSTAT_NAMED_PTR(disk->disk_errs);
			for (i = 0; i < 3; i++) {
				switch (knp[i].data_type) {
					case KSTAT_DATA_ULONG:
						push_out(efstr,
						    knp[i].value.ui32);
						toterrs += knp[i].value.ui32;
						break;
					case KSTAT_DATA_ULONGLONG:
						/*
						 * We're only set up to
						 * write out the low
						 * order 32-bits so
						 * just grab that.
						 */
						push_out(efstr,
							knp[i].value.ui32);
						toterrs += knp[i].value.ui32;
						break;
					default:
						break;
				}
			}
			push_out(efstr, toterrs);
		} else {
			if (do_raw == 0)
				push_out("  0   0   0   0 ");
			else
				push_out("0,0,0,0");
		}
	}
	if (suppress_zero == 0 || doit == 1) {
		if ((do_disk & (DISK_EXTENDED | DISK_ERRORS)) &&
			do_conversions) {
			push_out("%s", disk_name);
			if (show_mountpts && disk->dname) {
				mnt_t *mount_pt;
				char *lu;
				char lub[SMALL_SCRATCH_BUFLEN];

				lu = strrchr(disk->dname, '/');
				if (lu) {
					if (strcmp(disk->device_name, lu) == 0)
						lu = disk->dname;
					else {
						*lu = 0;
						(void) strcpy(lub, disk->dname);
						*lu = '/';
						(void) strcat(lub, "/");
						(void) strcat(lub,
						    disk->device_name);
						lu = lub;
					}
				} else
					lu = disk->device_name;
				mount_pt = lookup_mntent_byname(lu);
				if (mount_pt) {
					if (do_raw == 0)
						push_out(" (%s)",
						    mount_pt->mount_point);
					else
						push_out("(%s)",
						    mount_pt->mount_point);
				}
			}
		}
	}
	return (doit);
}

/*
 * Get list of cpu_stat KIDs for subsequent cpu_stat_load operations.
 */
static char *
cpu_stat_init(void)
{
	kstat_t *ksp;
	struct cpu_sinfo *list;
	uint_t cpu_ncpus;
	int *inset[2];
	char *abuf = NULL;
	long nconf_cpus;

	/*
	 * Don't cache this since it may change due
	 * to DR operations. We want to have a list
	 * that could hold kstats for all possible
	 * cpus.
	 */
	nconf_cpus = sysconf(_SC_NPROCESSORS_CONF);
	safe_zalloc((void *)&list, nconf_cpus *
		    sizeof (struct cpu_sinfo), 0);
	/*
	 * How may cpu kstats do we have?
	 */
	cpu_ncpus = 0;
	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
		if ((strncmp(ksp->ks_name, "cpu_stat", 8) == 0) &&
		    (kstat_read(kc, ksp, NULL) != -1)) {
			(void) memcpy(&list[cpu_ncpus].rksp, ksp,
				sizeof (kstat_t));
			list[cpu_ncpus].ksp = ksp;
			cpu_ncpus++;
		}
	}
	if (cpu_ncpus) {
		if (cpu_stat_list) {
			char cpu_buf[LARGE_SCRATCH_BUFLEN];
			char *cpup;
			int i;
			int j;
			int cpu_buf_used;
			int cpu_buf_used_event;
			char **event_name;
			size_t slen;

			safe_zalloc((void *)&inset[0], sizeof (int) * ncpus, 0);
			safe_zalloc((void *)&inset[1], sizeof (int) * cpu_ncpus,
				0);
			diff_two_sets(cpu_stat_list, ncpus, list, cpu_ncpus,
				inset[0], inset[1]);
			cpup = cpu_buf;
			*cpup = NULL;
			cpu_buf_used = 0;
			for (i = 0, event_name = cpu_events; i < NUM_CPU_EVENTS;
				i++, event_name++) {
				cpu_buf_used_event = 0;
				for (j = 0; j < ncpus; j++) {
					if (inset[i][j] != -1) {
						if (cpu_buf_used_event == 0) {
							(void) strcpy(cpup,
							    *event_name);
							while (*cpup)
								cpup++;
							cpu_buf_used = 1;
							cpu_buf_used_event = 1;
						}
						(void) sprintf(cpup, "%d,",
							inset[i][j]);
						while (*cpup)
							cpup++;
					} else {
						break;
					}
				}
				if (cpu_buf_used_event) {
					--cpup;
					*cpup++ = '>';
					*cpup++ = '>';
					*cpup++ = '\n';
				}
			}
			if (cpu_buf_used) {
				*cpup = '\0';
			}
			slen = strlen(cpu_buf);
			if (slen > 0) {
				safe_zalloc((void *) &abuf, slen + 1, 0);
				(void) strcpy(abuf, cpu_buf);
			}
			free(cpu_stat_list);
			free(inset[0]);
			free(inset[1]);
		}
		cpu_stat_list = list;
		ncpus = cpu_ncpus;
		if (abuf)
			(void) memset(&ncp_stat, 0, sizeof (ncp_stat));
		return (abuf);
	} else
		fail(1, "can't find any cpu statistics");
	/* NOTREACHED */
	return (NULL);
}

/*
 * Read the cpu stats for each CPU. Sum the cpu and vm stats
 * for all cpus into a struct which will be used for display.
 */
static int
cpu_stat_load(void)
{
	uint_t i, j;
	cpu_stat_t cs;
	uint_t *np, *tp;

	ocp_stat = ncp_stat;
	(void) memset(&ncp_stat, 0, sizeof (cpu_stat_t));

	/* Sum across all cpus */

	for (i = 0; i < ncpus; i++) {
		if (kstat_read(kc, cpu_stat_list[i].ksp, (void *)&cs) == -1)
			return (1);
		np = (uint_t *)&ncp_stat.cpu_sysinfo;
		tp = (uint_t *)&cs.cpu_sysinfo;
		for (j = 0; j < sizeof (cpu_sysinfo_t); j += sizeof (uint_t))
			*np++ += *tp++;
		np = (uint_t *)&ncp_stat.cpu_vminfo;
		tp = (uint_t *)&cs.cpu_vminfo;
		for (j = 0; j < sizeof (cpu_vminfo_t); j += sizeof (uint_t))
			*np++ += *tp++;
	}
	return (0);
}

/*
 * Determine what is different between two sets of cpu info.
 */
static void
diff_two_sets(struct cpu_sinfo *s1, uint_t ns1, struct cpu_sinfo *s2,
	uint_t ns2, int *l1, int *l2)
{
	int i;
	int j;
	struct cpu_sinfo *save1;
	struct cpu_sinfo *save2;
	int *tlist;
	int *stlist;
	uint_t num;
	int num_seen;

	stlist = l1;
	for (i = 0; i < ns1; i++)
		*l1++ = -1;
	l1 = stlist;
	stlist = l2;
	for (i = 0; i < ns2; i++)
		*l2++ = -1;
	l2 = stlist;
	/*
	 * Allocate space for a temporary list of what is in both
	 * s1 and s2. All we need is space for the minimum of the sizes
	 * of s1 and s2.
	 */
	if (ns1 > ns2)
		num = ns2;
	else
		num = ns1;
	safe_zalloc((void *)&tlist, num * sizeof (int), 0);
	stlist = tlist;
	for (i = 0; i < num; i++)
		*tlist++ = -1;
	tlist = stlist;

	/*
	 * Walk the list and determine what in s1 is also in s2.
	 */
	num_seen = 0;
	save1 = s1;
	save2 = s2;
	for (i = 0; i < ns1; i++, s1++) {
		s2 = save2;
		for (j = 0; j < ns2; j++, s2++) {
			/*
			 * We assume that no two kstats will have the
			 * same id unless they are truly the same.
			 */
			if (s1->rksp.ks_kid == s2->rksp.ks_kid) {
				*tlist = s1->rksp.ks_instance;
				tlist++;
				s1->seen = 1;
				s2->seen = 1;
				num_seen++;
				break;
			}
		}
	}
	/*
	 * At this point we have in tlist the list of items from s1
	 * which are in both s1 and s2. We cannot assume that if ns1 == ns2
	 * == num_seen that we don't have work to do since we could have
	 * an equal number of CPUs go off line and come online in the same
	 * period.
	 */
	s1 = save1;
	for (i = 0; i < ns1; i++, s1++) {
		if (s1->seen) {
			s1->seen = 0;
		} else {
			*l1++ = s1->rksp.ks_instance;
		}
	}
	s2 = save2;
	for (i = 0; i < ns2; i++, s2++) {
		if (s2->seen) {
			s2->seen = 0;
		} else {
			*l2++ = s2->rksp.ks_instance;
		}
	}
	free(stlist);
}

static void
usage(void)
{
	(void) fprintf(stderr,
	    "Usage: iostat [-cCdDeEImMnpPrstxz] "
	    " [-l n] [T d|u] [disk ...] [interval [count]]\n"
	    "\t\t-c: 	report percentage of time system has spent\n"
	    "\t\t\tin user/system/wait/idle mode\n"
	    "\t\t-C: 	report disk statistics by controller\n"
	    "\t\t-d: 	display disk Kb/sec, transfers/sec, avg. \n"
	    "\t\t\tservice time in milliseconds  \n"
	    "\t\t-D: 	display disk reads/sec, writes/sec, \n"
	    "\t\t\tpercentage disk utilization \n"
	    "\t\t-e: 	report device error summary statistics\n"
	    "\t\t-E: 	report extended device error statistics\n"
	    "\t\t-I: 	report the counts in each interval,\n"
	    "\t\t\tinstead of rates, where applicable\n"
	    "\t\t-l n:	Limit the number of disks to n\n"
	    "\t\t-m: 	Display mount points (most useful with -p)\n"
	    "\t\t-M: 	Display data throughput in MB/sec "
	    "instead of Kb/sec\n"
	    "\t\t-n: 	convert device names to cXdYtZ format\n"
	    "\t\t-p: 	report per-partition disk statistics\n"
	    "\t\t-P: 	report per-partition disk statistics only,\n"
	    "\t\t\tno per-device disk statistics\n"
	    "\t\t-r: 	Display data in comma separated format\n"
	    "\t\t-s: 	Suppress state change messages\n"
	    "\t\t-T d|u	Display a timestamp in date (d) or unix "
	    "time_t (u)\n"
	    "\t\t-t: 	display chars read/written to terminals\n"
	    "\t\t-x: 	display extended disk statistics\n"
	    "\t\t-z: 	Suppress entries with all zero values\n");
	exit(1);
}

/*
 * Terminate the program, emitting an error message
 * optionally emitting a perror message as well.
 */
void
fail(int do_perror, char *message, ...)
{
	va_list args;
	int lerrno;

	lerrno = errno;
	va_start(args, message);
	(void) fprintf(stderr, "%s: ", cmdname);
	(void) vfprintf(stderr, message, args);
	va_end(args);
	if (do_perror)
		(void) fprintf(stderr, ": %s", strerror(lerrno));
	(void) fprintf(stderr, "\n");
	exit(2);
}

/*
 * "Safe" allocators - if we return we're guaranteed
 * to have the desired space. We exit via fail
 * if we can't get the space.
 */
void
safe_zalloc(void **ptr, uint_t size, int free_first)
{
	if (free_first && *ptr != NULL)
		free(*ptr);
	if ((*ptr = (void *)malloc(size)) == NULL)
		fail(1, "malloc failed");
	(void) memset(*ptr, 0, size);
}

void
safe_alloc(void **ptr, uint_t size, int free_first)
{
	if (free_first && *ptr != NULL)
		free(*ptr);
	if ((*ptr = (void *)malloc(size)) == NULL)
		fail(1, "malloc failed");
}

void
safe_strdup(char *s1, char **s2)
{
	if (s1) {
		if ((*s2 = strdup(s1)) == 0)
			fail(1, "strdup");
	} else
		*s2 = 0;
}

/*
 * Sort based on ks_class, ks_module, ks_instance, ks_name
 */
static int
kscmp(diskinfo_t *ks1, diskinfo_t *ks2)
{
	int cmp;

	cmp = ks1->class - ks2->class;
	if (cmp)
		return (cmp);

	cmp = strcmp(ks1->ksp->ks_module, ks2->ksp->ks_module);
	if (cmp)
		return (cmp);
	cmp = ks1->ksp->ks_instance - ks2->ksp->ks_instance;
	if (cmp)
		return (cmp);

	if (ks1->device_name && ks2->device_name) {
		return (strcmp(ks1->device_name,  ks2->device_name));
	} else {
		return (strcmp(ks1->ksp->ks_name, ks2->ksp->ks_name));
	}
}

/*
 * Initialize various bits of information about disk and tape devices
 * attached to the system. If we're converting the kstat names to intelligible
 * names we call build_disk_list to build a linked list of infomation
 * about attached devices.
 */
static void
init_disks(void)
{
	diskinfo_t *disk, *prev, *comp;
	kstat_t *ksp;

	if (do_controller && (interval > 0))
		free_controller_list();
	if (do_conversions || do_controller)
		build_disk_list();
	if (firstdisk) {
		prevdisk = firstdisk;
		firstdisk = 0;
	}

	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
		/*
		 * Walk through the kstat list. Identify all of
		 * the IO related kstats and construct a diskinfo
		 * structure for that device. If we are doing
		 * only disk partitions then skip the 'partition'
		 * kstats. If we're only doing partitions then
		 * skip the 'disk' kstats.
		 */

		int i;

		if (ksp->ks_type != KSTAT_TYPE_IO)
			continue;

		for (i = 0; io_class[i].class_name != NULL; i++) {
			if (ksp->ks_class[0] == io_class[i].class_name[0]) {
				if (strcmp(ksp->ks_class,
				    io_class[i].class_name) == 0)
				break;
			}
		}
		if (io_class[i].class_name == NULL)
			continue;

		if (do_partitions_only == 0) {
			if (do_partitions == 0) {
				if ((ksp->ks_class[0] == 'p') &&
				    strcmp(ksp->ks_class,
				    "partition") == 0)
					continue;
			}
		} else {
			if ((ksp->ks_class[0] == 'd') &&
			    (strcmp(ksp->ks_class, "disk") == 0))
				continue;
		}

		/*
		 * Allocate a zeroed out diskinfo structure.
		 */
		safe_zalloc((void **)&disk, sizeof (diskinfo_t), 0);
		/*
		 * Load up the diskinfo struct
		 */
		disk->ksp = ksp;
		/*
		 * Copy current kstat since we'll need it later
		 * if we're doing more than one loop through.
		 * We need to save the actual kstat since what is
		 * pointed to may change.
		 */
		if (interval > 0)
			(void) memcpy(&disk->ks, ksp, sizeof (kstat_t));
		/*
		 * Needed to make diskinfo_load work correctly
		 */
		(void) memset((void *)&disk->new_kios, 0,
			    sizeof (kstat_io_t));
		disk->disk_errs = NULL;
		disk->class = io_class[i].class_priority;
		/*
		 * If we're converting the kstat name into a real device name
		 * go figure it out.
		 */
		if (do_conversions) {
			if ((ksp->ks_class[0] != 'n') ||
			    (strcmp(ksp->ks_class, "nfs") != 0))
				do_disk_name(ksp, disk);
			else
				disk->device_name =
					lookup_nfs_name(ksp->ks_name);
		} else
			disk->device_name = 0;
		/*
		 * Insertion sort on (ks_class, ks_module, ks_instance,
		 * ks_name)
		 */
		prev = 0;
		comp = firstdisk;
		while (comp) {
			if (kscmp(disk, comp) > 0) {
				prev = comp;
				comp = comp->next;
			} else {
				break;
			}
		}
		if (prev) {
			disk->next = prev->next;
			prev->next = disk;
		} else {
			disk->next = firstdisk;
			firstdisk = disk;
		}
	}

	if (firstdisk) {
		select_disks();
		if (do_disk & DISK_ERROR_MASK)
			init_disk_errors();
	} else
		fail(0, "No disks to measure");
}

/*
 * Given a list of disks that we want to see output for
 * go through the list of disks and mark any that are
 * in the desired output list as selected.
 */
static void
select_disks(void)
{
	diskinfo_t *disk;
	struct disk_selection *ds;
	diskinfo_t *fd;
	uint_t ndrives;

	ndrives = 0;
	for (disk = firstdisk; disk; disk = disk->next) {
		disk->selected = 0;
		for (ds = disk_selections; ds; ds = ds->next) {
			if (strcmp(disk->ksp->ks_name, ds->ks_name) == 0) {
				disk->selected = 1;
				ndrives++;
				break;
			}
		}
	}
	for (disk = firstdisk; disk; disk = disk->next) {
		if (disk->selected == 0) {
			/*
			 * Limit is only set in the one line mode (i.e., -x)
			 */
			if (limit) {
				if (ndrives < limit) {
					if (strncmp(disk->ksp->ks_name,
					    "fd", 2) != 0) {
						disk->selected = 1;
						ndrives++;
					} else
						fd = disk;
				} else
					break;
			} else {
				disk->selected = 1;
			}
		}
	}
	if (ndrives < limit)
		fd->selected = 1;
}

/*
 * Walk the kstat chain and read the kstat associated
 * with each entry.
 */
static void
diskinfo_load(void)
{
	diskinfo_t *disk;

	for (disk = firstdisk; disk; disk = disk->next) {
		if (disk->selected) {
			disk->old_kios = disk->new_kios;
			(void) kstat_read(kc, disk->ksp,
			    &disk->new_kios);
			if (disk->disk_errs) {
				(void) kstat_read(kc, disk->disk_errs, NULL);
			}
		}
	}
}

static void
init_disk_errors(void)
{
	kstat_t *ksp;

	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
		if ((ksp->ks_type == KSTAT_TYPE_NAMED) &&
			(strncmp(ksp->ks_class, "device_error", 12) == 0)) {
				find_disk(ksp);
			}
	}
}

static void
find_disk(kstat_t *ksp)
{
	diskinfo_t *disk;
	char	kstat_name[KSTAT_STRLEN];
	char	*dname = kstat_name;
	char	*ename = ksp->ks_name;

	while (*ename != ',') {
		*dname = *ename;
		dname++;
		ename++;
	}
	*dname = NULL;

	for (disk = firstdisk; disk; disk = disk->next) {
		if (disk->selected) {
			if (strcmp(disk->ksp->ks_name, kstat_name) == 0) {
				disk->disk_errs = ksp;
				return;
			}
		}
	}

}

static void
show_disk_errors(diskinfo_t *disk)
{
	kstat_named_t *knp;
	size_t  col;
	int	i;
	char	*dev_name;

	if (disk->device_name)
		dev_name = disk->device_name;
	else
		dev_name = disk->ksp->ks_name;
	if (do_conversions)
		push_out("%-16.16s", dev_name);
	else
		push_out("%-9.9s", dev_name);
	col = 0;
	knp = KSTAT_NAMED_PTR(disk->disk_errs);
	for (i = 0; i < disk->disk_errs->ks_ndata; i++) {
		col += strlen(knp[i].name);
		switch (knp[i].data_type) {
			case KSTAT_DATA_CHAR:
				push_out("%s: %-.16s ", knp[i].name,
				    &knp[i].value.c[0]);
				col += strlen(&knp[i].value.c[0]);
				break;
			case KSTAT_DATA_ULONG:
				push_out("%s: %d ", knp[i].name,
					knp[i].value.ui32);
				col += 4;
				break;
			case KSTAT_DATA_ULONGLONG:
				if (strcmp(knp[i].name, "Size") == 0) {
					push_out("%s: %2.2fGB <%lld bytes>\n",
					knp[i].name,
					(float)knp[i].value.ui64 /
					    DISK_GIGABYTE,
					knp[i].value.ui64);
					col = 0;
					break;
				}
				push_out("%s: %d ", knp[i].name,
					knp[i].value.ui32);
				col += 4;
				break;
			}
		if ((col >= 62) || (i == 2)) {
			do_newline();
			col = 0;
		}
	}
	if (col > 0) {
		do_newline();
	}
	do_newline();
}

#define	ARGS_STR	"tdDxCcIpPnmMeEszrT:l:"

void
do_args(int argc, char **argv)
{
	int 		c;
	int 		errflg = 0;
	extern char 	*optarg;
	extern int 	optind;

	while ((c = getopt(argc, argv, ARGS_STR)) != EOF)
		switch (c) {
		case 't':
			do_tty++;
			break;
		case 'd':
			do_disk |= DISK_OLD;
			break;
		case 'D':
			do_disk |= DISK_NEW;
			break;
		case 'x':
			do_disk |= DISK_EXTENDED;
			break;
		case 'C':
			do_controller++;
			break;
		case 'c':
			do_cpu++;
			break;
		case 'I':
			do_interval++;
			break;
		case 'p':
			do_partitions++;
			break;
		case 'P':
			do_partitions_only++;
			break;
		case 'n':
			do_conversions++;
			break;
		case 'M':
			do_megabytes++;
			break;
		case 'e':
			do_disk |= DISK_ERRORS;
			break;
		case 'E':
			do_disk |= DISK_EXTENDED_ERRORS;
			break;
		case 's':
			suppress_state = 1;
			break;
		case 'z':
			suppress_zero = 1;
			break;
		case 'm':
			show_mountpts = 1;
			break;
		case 'T':
			if (optarg) {
				if (*optarg == 'u')
					do_timestamp = UDATE;
				else if (*optarg == 'd')
					do_timestamp = CDATE;
				else
					errflg++;
			} else
				errflg++;
			break;
		case 'r':
			do_raw = 1;
			break;
		case 'l':
			limit = atoi(optarg);
			if (limit < 1)
				usage();
			break;
		case '?':
			errflg++;
	}

	if (errflg) {
		usage();
	}
	/* if no output classes explicity specified, use defaults */
	if (do_tty == 0 && do_disk == 0 && do_cpu == 0)
		do_tty = do_cpu = 1, do_disk = DISK_OLD;

	/*
	 * If conflicting options take the preferred
	 * -D and -x result in -x
	 * -d or -D and -e or -E gives only whatever -d or -D was specified
	 */
	if ((do_disk & DISK_EXTENDED) && (do_disk & DISK_NORMAL))
		do_disk &= ~DISK_NORMAL;
	if ((do_disk & DISK_NORMAL) && (do_disk & DISK_ERROR_MASK))
		do_disk &= ~DISK_ERROR_MASK;
	/*
	 * If limit == 0 then no command line limit was set, else if any of
	 * the flags that cause unlimited disks were not set,
	 * use the default of 4
	 */
	if (limit == 0) {
		if (!(do_disk & (DISK_EXTENDED | DISK_ERRORS |
		    DISK_EXTENDED_ERRORS)))
			limit = DEFAULT_LIMIT;
	}
	if (do_disk) {
		/*
		 * Choose drives to be displayed.  Priority
		 * goes to -in order- drives supplied as arguments,
		 * then any other active drives that fit.
		 */
		struct disk_selection **dsp = &disk_selections;
		while (optind < argc && !isdigit(argv[optind][0])) {
			safe_zalloc((void **)dsp, sizeof (**dsp), 0);
			(void) strncpy((*dsp)->ks_name, argv[optind],
			    KSTAT_STRLEN - 1);
			dsp = &((*dsp)->next);
			optind++;
		}
		/*
		 * Not needed? - safe_zalloc will always zero out next field
		 * or the original declaration is set to zero already.
		 */
		*dsp = NULL;
	}
	if (optind < argc) {
		if ((interval = atoi(argv[optind])) <= 0)
			fail(0, "negative interval");
		optind++;

		if (optind < argc) {
			if ((iter = atoi(argv[optind])) <= 0)
				fail(0, "negative count");
				optind++;
		}
	}
	if (interval == 0)
		iter = 1;
	if (optind < argc)
		usage();
}

/*
 * Driver for doing the extended header formatting. Will produce
 * the function stack needed to output an extended header based
 * on the options selected.
 */

void
do_format(char *err_header)
{
	char 	ch;
	char 	iosz;
	char    *fstr;

	disk_header[0] = 0;
	ch = (do_interval ? 'i' : 's');
	iosz = (do_megabytes ? 'M' : 'k');
	if (do_disk & DISK_ERRORS) {
		if (do_raw == 0) {
			(void) sprintf(err_header,
			    "s/w h/w trn tot ");
		} else
			(void) sprintf(err_header, "s/w,h/w,trn,tot");
	} else
		*err_header = NULL;
	switch (do_disk & DISK_IO_MASK) {
		case DISK_OLD:
			if (do_raw == 0)
				fstr = "%cp%c tp%c serv  ";
			else
				fstr = "%cp%c,tp%c,serv";
			(void) snprintf(disk_header, sizeof (disk_header),
			    fstr, iosz, ch, ch);
			break;
		case DISK_NEW:
			if (do_raw == 0)
				fstr = "rp%c wp%c util  ";
			else
				fstr = "%rp%c,wp%c,util";
			(void) snprintf(disk_header, sizeof (disk_header),
			    fstr, ch, ch);
			break;
		case DISK_EXTENDED:
			if (!do_conversions) {
				if (do_raw == 0)
					fstr = "device       r/%c    w/%c   "
					    "%cr/%c   %cw/%c wait actv  "
					    "svc_t  %%%%w  %%%%b %s";
				else
					fstr = "device,r/%c,w/%c,%cr/%c,%cw/%c,"
						"wait,actv,svc_t,%%%%w,"
						"%%%%b,%s";
				(void) snprintf(disk_header,
				    sizeof (disk_header),
				    fstr, ch, ch, iosz, ch, iosz,
				    ch, err_header);
			} else {
				if (do_raw == 0) {
					fstr = "    r/%c    w/%c   %cr/%c   "
					    "%cw/%c wait actv wsvc_t asvc_t  "
					    "%%%%w  %%%%b %sdevice";
				} else {
					fstr = "r/%c,w/%c,%cr/%c,%cw/%c,"
					    "wait,actv,wsvc_t,asvc_t,"
					    "%%%%w,%%%%b,%sdevice";
				}
				(void) snprintf(disk_header,
				    sizeof (disk_header),
				    fstr, ch, ch, iosz, ch, iosz,
				    ch, err_header);
			}
			break;
		default:
			break;
	}
	if (do_disk == DISK_ERRORS) {
		char *sep;

		if (!do_conversions) {
			if (do_raw == 0) {
				sep = "     ";
			} else
				sep = ",";
			(void) snprintf(disk_header, sizeof (disk_header),
			    "%s%s%s", "device", sep, err_header);
		} else {
			if (do_raw == 0) {
				(void) snprintf(disk_header,
				    sizeof (disk_header),
				    "  %s", err_header);
			} else
				(void) strcpy(disk_header, err_header);
		}
	} else {
		/*
		 * Need to subtract two characters for the % escape in
		 * the string.
		 */
		dh_len = strlen(disk_header) - 2;
	}
	/*
	 * -n *and* (-E *or* -e *or* -x)
	 */
	if (do_timestamp)
		setup(print_timestamp);
	if (do_conversions && (do_disk & PRINT_VERTICAL)) {
		if (do_tty)
			setup(print_tty_hdr1);
		if (do_cpu)
			setup(print_cpu_hdr1);
		if (do_tty || do_cpu)
			setup(do_newline);
		if (do_tty)
			setup(print_tty_hdr2);
		if (do_cpu)
			setup(print_cpu_hdr2);
		if (do_tty || do_cpu)
			setup(do_newline);
		if (do_tty)
			setup(print_tty_data);
		if (do_cpu)
			setup(print_cpu_data);
		if (do_tty || do_cpu)
			setup(do_newline);
		printxhdr();
		if (do_controller)
		    setup(controller_dump);
		setup(disk_dump);
	} else {
		/*
		 * All other options pass through here - note that errors are
		 * processed in show_disk (called via disk_dump).
		 */
		if (do_disk & PRINT_VERTICAL)
		    printxhdr();
		else
		    setup(hdrout);
		/*
		 * If we're printing vertically, disks and/or errors go first
		 * This just prints the first line....
		 */
		if (do_disk & PRINT_VERTICAL) {
			setup(do_showdisk);
			skip_first = 1;
		}
		if (do_tty)
			setup(print_tty_data);
		if (do_disk & ~PRINT_VERTICAL)
			setup(disk_dump);
		if (do_cpu)
			setup(print_cpu_data);
		setup(do_newline);
		if (do_disk & PRINT_VERTICAL)
			setup(disk_dump);
	}
	if (do_disk & DISK_EXTENDED_ERRORS)
		setup(disk_errors);
}

/*
 * Add a new function to the list of functions
 * for this invocation. Once on the stack the
 * function is never removed nor does its place
 * change.
 */
void
setup(void (*nfunc)(void))
{
	format_t *tmp;

	safe_alloc((void **)&tmp, sizeof (format_t), 0);
	tmp->nfunc = nfunc;
	tmp->next = 0;
	if (formatter_end)
		formatter_end->next = tmp;
	else
		formatter_list = tmp;
	formatter_end = tmp;

}

/*
 * The functions after this comment are devoted to printing
 * various parts of the header. They are selected based on the
 * options provided when the program was invoked. The functions
 * are either directly invoked in printhdr() or are indirectly
 * invoked by being placed on the list of functions used when
 * extended headers are used.
 */
void
print_tty_hdr1(void)
{
	char *fstr;
	char *dstr;

	if (do_raw == 0) {
		fstr = "%10.10s";
		dstr = "tty    ";
	} else {
		fstr = "%s";
		dstr = "tty";
	}
	push_out(fstr, dstr);
}

void
print_tty_hdr2(void)
{
	if (do_raw == 0)
		push_out("%-10.10s", " tin tout");
	else
		push_out("tin,tout");
}

void
print_cpu_hdr1(void)
{
	char *dstr;

	if (do_raw == 0)
		dstr = "     cpu";
	else
		dstr = "cpu";
	push_out(dstr);
}

void
print_cpu_hdr2(void)
{
	char *dstr;

	if (do_raw == 0)
		dstr = " us sy wt id";
	else
		dstr = "us,sy,wt,id";
	push_out(dstr);
}

/*
 * Assumption is that tty data is always first - no need for raw mode leading
 * comma.
 */
void
print_tty_data(void)
{
	char *fstr;
	uint_t deltas;
	float raw;
	float outch;

	if (do_raw == 0)
		fstr = " %3.0f %4.0f ";
	else
		fstr = "%.0f,%.0f";
	deltas = u32_delta(ocp_stat.cpu_sysinfo.rawch,
		ncp_stat.cpu_sysinfo.rawch);
	raw = (float)deltas;
	raw /= (float)getime;
	deltas = u32_delta(ocp_stat.cpu_sysinfo.outch,
		ncp_stat.cpu_sysinfo.outch);
	outch = (float)deltas;
	outch /= (float)getime;

	push_out(fstr, raw, outch);
}

/*
 * Write out CPU data
 */
void
print_cpu_data(void)
{
	char *fstr;
	uint_t udeltas[CPU_STATES];
	uint_t i;

	if (do_raw == 0)
		fstr = " %2.0f %2.0f %2.0f %2.0f";
	else
		fstr = "%.0f,%.0f,%.0f,%.0f";

	for (i = 0; i < CPU_STATES; i++)
		udeltas[i] = u32_delta(ocp_stat.cpu_sysinfo.cpu[i],
		    ncp_stat.cpu_sysinfo.cpu[i]);
	push_out(fstr,
	    (float)udeltas[CPU_USER] * (float)percent,
	    (float)udeltas[CPU_KERNEL] * (float)percent,
	    (float)udeltas[CPU_WAIT] * (float)percent,
	    (float)udeltas[CPU_IDLE] * (float)percent);
}

/*
 * Emit the appropriate header.
 */
void
hdrout(void)
{
	if (do_raw == 0) {
		if (--tohdr == 0)
			printhdr(0);
	} else if (hdr_out == 0) {
		printhdr(0);
		hdr_out = 1;
	}
}

/*
 * Write out disk errors when -e is specified.
 */
void
disk_errors(void)
{
	diskinfo_t *disk;
	/*
	 * Dump total error statistics
	 */
	for (disk = firstdisk; disk; disk = disk->next) {
		if (disk->selected && disk->disk_errs) {
			show_disk_errors(disk);
		}
	}
}

void
do_showdisk(void)
{
	firstdisk->new_kios.wcnt = 1;
	(void) show_disk(firstdisk);
}

/*
 * End of formatting functions
 */

/*
 * Clean up a controller list.
 */
void
free_controller_list(void)
{
	con_t *tmp;

	while (ctl_list) {
		tmp = ctl_list->next;
		free(ctl_list);
		ctl_list = tmp;
	}
}

/*
 * Write out the controller information. First sum the information
 * into a disk struct and then write it out as if it were a disk.
 * We don't do anything with wlastupdate and rlastupdate fields.
 */
void
controller_dump(void)
{
	con_t *ctl_ptr;

	if (ctl_list) {
		diskinfo_t curr_ctl;
		diskinfo_t *w;
		char cbuf[SMALL_SCRATCH_BUFLEN];

		ctl_ptr = ctl_list;
		while (ctl_ptr) {
			(void) memset(&curr_ctl, 0, sizeof (curr_ctl));
			w = ctl_ptr->d;
			while (w) {
				int i;
				kstat_io_t *ptr1;
				kstat_io_t *ptr2;

				curr_ctl.new_kios.wcnt++;
				for (i = 0, ptr1 = &curr_ctl.old_kios,
				    ptr2 = &w->old_kios; i < 2;
				    i++, ptr1 = &curr_ctl.new_kios,
				    ptr2 = &w->new_kios) {
					ptr1->nread += ptr2->nread;
					ptr1->nwritten += ptr2->nwritten;
					ptr1->reads += ptr2->reads;
					ptr1->writes += ptr2->writes;
					ptr1->wtime += ptr2->wtime;
					ptr1->wlentime += ptr2->wlentime;
					ptr1->rtime += ptr2->rtime;
					ptr1->rlentime += ptr2->rlentime;
				}
				if (w->last_snap) {
					ptr1->wlastupdate +=
					    (w->ksp->ks_snaptime -
					    w->last_snap);
				} else {
					ptr1->wlastupdate +=
					    (w->ksp->ks_snaptime -
					    w->ksp->ks_crtime);
				}
				if (curr_ctl.ksp != 0) {
					if (curr_ctl.ksp->ks_crtime >
					    w->ksp->ks_crtime)
						curr_ctl.ksp = w->ksp;
				} else
					curr_ctl.ksp = w->ksp;
				w = w->cn;
			}
			(void) snprintf(cbuf, sizeof (cbuf), "c%d",
			    ctl_ptr->cid);
			curr_ctl.device_name = cbuf;
			if (show_disk(&curr_ctl) != 0) {
				if (do_disk & PRINT_VERTICAL)
					(void) putchar('\n');
			}
			ctl_ptr = ctl_ptr->next;
		}
	}
}

/*
 * Dump out disk information. Set wcnt in the new_kios kstat to indicate this
 * is only one disk.
 */
void
disk_dump(void)
{
	diskinfo_t *disk;

	if (skip_first == 0)
		disk = firstdisk;
	else if (firstdisk)
		disk = firstdisk->next;
	else
		disk = NULL;

	while (disk) {
	    if (disk->selected) {
			disk->new_kios.wcnt = 1;
			if (show_disk(disk)) {
				if (do_disk & PRINT_VERTICAL)
					do_newline();
		}
	    }
	    disk = disk->next;
	}
}

/*
 * See if the kstat chain has changed since we did the last pass
 * through. If so, reinitialize our state and go back around.
 */
void
check_for_changes(void)
{
	uint_t 	done;
	uint_t 	schange;
	schange_t chg_info;

	done = 0;
	while (done == 0) {
		char *rv1;
		uint_t cpu_change = 0;
		uint_t disk_change = 0;

		(void) memset(&chg_info, 0, sizeof (chg_info));
		schange = kstat_chain_update(kc);
		if (schange) {
			/*
			 * A change has occurred. Figure out if it is
			 * one we care about.
			 */
			rv1 = 0;
			if (do_cpu || (do_interval == 0)) {
				rv1 = cpu_stat_init();
				if (rv1) {
					cpu_change = 1;
				}
			}
			if (do_disk) {
				/*
				 * Don't know if it was from a CPU change or a
				 * disk. Need to update our idea of disks and
				 * do diskinfo_load() in the next iteration.
				 */
				init_disks();
				if (prevdisk) {
					disk_change = diff_two_lists(firstdisk,
					    prevdisk, &chg_info);
				}
			}
		}
		if (schange == 0 || suppress_state == 1 ||
			((disk_change == 0) && (cpu_change == 0))) {
			done = 1;
			if (do_cpu || (do_interval == 0)) {
				if (cpu_stat_load() != 0)
					(void) printf("CPU STAT LOAD ERROR\n");
			}
			if (do_disk)
				diskinfo_load();
		} else  {
			/*
			 * Something to go public about.
			 */
			(void) printf("\n");
			if (rv1) {
				(void) printf("%s", rv1);
				free(rv1);
			}
			if (chg_info.new_names) {
				display(chg_info.new_names, "added",
					chg_info.num_new_devs);
			}
			if (chg_info.old_names) {
				display(chg_info.old_names, "removed",
					chg_info.num_old_devs);
			}
			if (chg_info.new_mts) {
				display(chg_info.new_mts, "mounted",
					chg_info.num_new_mts);
			}
			if (chg_info.old_mts) {
				display(chg_info.old_mts, "unmounted",
					chg_info.num_old_mts);
			}
		}
		if (chg_info.new_names)
			free(chg_info.new_names);
		if (chg_info.old_names)
			free(chg_info.old_names);
		if (chg_info.new_mts)
			free(chg_info.new_mts);
		if (chg_info.old_mts)
			free(chg_info.old_mts);
		if (schange && do_disk && prevdisk) {
			cleanup_disk_list(prevdisk);
			prevdisk = 0;
		}
	}
}

/*
 * Walk through two lists first determining what is common between the
 * two lists, after that determine what has been added or removed
 * from the system. We differentiate betwen nfs entries (placed on
 * the mountlists) and all others.
 */
static uint_t
diff_two_lists(diskinfo_t *new, diskinfo_t *old, schange_t *chg_info)
{
	diskinfo_t *tmp;
	diskinfo_t *tmp1;
	uint_t	oct;
	uint_t	nct;
	uint_t  new_mt_ct;
	uint_t  old_mt_ct;

	/*
	 * Find out how many entries are in the old and new lists.
	 * Allocate enough space to handle the case of all the old
	 * entries go away and none of the new entries are in the
	 * old list.
	 */
	nct = 0;
	tmp = new;
	while (tmp) {
		nct++;
		tmp = tmp->next;
	}

	oct = 0;
	tmp = old;
	while (tmp) {
		oct++;
		tmp = tmp->next;
	}

	/*
	 * Allocate pointers. We will fill these in with actual
	 * field names later but won't actually allocate space.
	 */
	safe_alloc((void **)&chg_info->new_names, nct * sizeof (char *), 0);
	safe_alloc((void **)&chg_info->new_mts, nct * sizeof (char *), 0);
	safe_alloc((void **)&chg_info->old_names, oct * sizeof (char *), 0);
	safe_alloc((void **)&chg_info->old_mts, oct * sizeof (char *), 0);

	/*
	 * Walk the new and old lists looking for what is common
	 * to both. If found, mark the items in both lists as seen.
	 */

	tmp = new;
	while (tmp) {
		tmp1 = old;
		while (tmp1) {
			if (tmp1->seen == 0) {
				/*
				 * We use the kstat creation time as
				 * a cheap way of determining whether
				 * two kstats are the same. The assumption
				 * is that no two kstats have the same
				 * creation time.
				 */
				if (tmp->ks.ks_crtime == tmp1->ks.ks_crtime) {
					tmp->seen = 1;
					tmp1->seen = 1;
					tmp->new_kios = tmp1->new_kios;
					tmp->last_snap = tmp1->last_snap;
					break;
				}
			}
			tmp1 = tmp1->next;
		}
		tmp = tmp->next;
	}

	/*
	 * Now walk each list one time looking for items not marked
	 * as seen.  For the 'new' list those are drives which have
	 * been added. For the 'old' list those are drives which have
	 * been removed. We just stuff the pointers to the disk name
	 * into the new_names and old_names array.
	 */

	tmp = new;
	new_mt_ct = 0;
	nct = 0;
	while (tmp) {
		if (tmp->seen == 0) {
			if ((tmp->ks.ks_class[0] == 'n') &&
			    (strcmp(tmp->ks.ks_class, "nfs") == 0)) {
				if (tmp->device_name)
					chg_info->new_mts[new_mt_ct] =
					    tmp->device_name;
				else
					chg_info->new_mts[new_mt_ct] =
					    tmp->ks.ks_name;
				new_mt_ct++;
			} else {
				if (tmp->device_name)
					chg_info->new_names[nct] =
					    tmp->device_name;
				else
					chg_info->new_names[nct] =
					    tmp->ks.ks_name;
				nct++;
			}
		} else
			tmp->seen = 0;
		tmp = tmp->next;
	}

	/*
	 * Give back unused space if we didn't
	 * see any old or new entries.
	 */
	if (nct == 0) {
		free(chg_info->new_names);
		chg_info->new_names = 0;
	}
	if (new_mt_ct == 0) {
		free(chg_info->new_mts);
		chg_info->new_mts = 0;
	}
	chg_info->num_new_mts = new_mt_ct;
	chg_info->num_new_devs = nct;

	/*
	 * Look through the old list and check what was not
	 * seen. These are "unmounted" disks.
	 */
	tmp = old;
	oct = 0;
	old_mt_ct = 0;
	while (tmp) {
		if (tmp->seen == 0) {
			if ((tmp->ks.ks_class[0] == 'n') &&
			    (strcmp(tmp->ks.ks_class, "nfs") == 0)) {
				if (tmp->device_name)
					chg_info->old_mts[old_mt_ct] =
					    tmp->device_name;
				else
					chg_info->old_mts[old_mt_ct] =
					    tmp->ks.ks_name;
				old_mt_ct++;
			} else {
				if (tmp->device_name)
					chg_info->old_names[oct] =
					    tmp->device_name;
				else
					chg_info->old_names[oct] =
					    tmp->ks.ks_name;
				oct++;
			}
		}
		tmp = tmp->next;
	}

	/*
	 * Give back unused space
	 */
	if (oct == 0) {
		free(chg_info->old_names);
		chg_info->old_names = 0;
	}
	if (old_mt_ct == 0) {
		free(chg_info->old_mts);
		chg_info->old_mts = 0;
	}
	chg_info->num_old_mts = old_mt_ct;
	chg_info->num_old_devs = oct;

	if (new_mt_ct || old_mt_ct || oct || nct)
		return (1);
	else
		return (0);
}

/*
 * Walk a list of disk_info_t structures freeing things that are pointed at
 * (except for the kstat pointers) and then freeing the structure. At the end
 * the list head is set to zero.
 */
void
cleanup_disk_list(diskinfo_t *list)
{
	diskinfo_t *tmp;

	while (list) {
		if (list->device_name) {
			free(list->device_name);
			list->device_name = NULL;
		}
		if (list->dname) {
			free(list->dname);
			list->dname = NULL;
		}
		tmp = list;
		list = list->next;
		free(tmp);
	}
}

/*
 * Find the controller this entry belongs to or put a new one
 * in place.
 */
void
add_controller(diskinfo_t *e)
{
	int cid;
	con_t *tmp;
	con_t *prev;

	if (e->dname) {
		/*
		 * See if we can find the controller id.
		 */
		cid = -1;
		if (sscanf(e->dname, "/dev/dsk/c%dt", &cid) == 1) {
			/*
			 * Found one. Now see if we've got in in the list.
			 * Could optimize by adding min and max but we don't
			 * do it yet.
			 *
			 * Walk the list (which is in order by controller id)
			 * looking for the appropriate list head. If we find
			 * it just add the entry at the head of the chain
			 * of disk_info entries. We use the cn field in the
			 * disk_info struct to link the disks for a controller
			 * together.
			 */
			if (ctl_list) {
				tmp = ctl_list;
				prev = 0;
				while (tmp) {
					if (tmp->cid < cid) {
						prev = tmp;
						tmp = tmp->next;
					} else
						break;
				}
				/*
				 * Found either a list head or the place
				 * where one should go.
				 */
				if (tmp) {
					if (tmp->cid == cid) {
						/*
						 * Found the list head for this
						 * disk. Just stick it at the
						 * beginning of the disk_info
						 * list.
						 */
						e->cn = tmp->d;
						tmp->d = e;
					} else {
						/*
						 * No list head. Need to create
						 * one and and link it into the
						 * proper place.
						 */
						safe_alloc((void **)&tmp,
						    sizeof (con_t), 0);
						tmp->cid = cid;
						tmp->d = e;
						if (prev) {
							tmp->next = prev->next;
							prev->next = tmp;
						} else {
							tmp->next = ctl_list;
							ctl_list = tmp;
						}
					}
				} else {
					/*
					 * Add at the end of the list.
					 */
					safe_alloc((void **)&tmp,
					    sizeof (con_t), 0);
					tmp->cid = cid;
					tmp->d = e;
					tmp->next = 0;
					prev->next = tmp;
				}
			} else {
				/*
				 * First entry in controller sweepstakes.
				 * Put it in the ctl_list variable.
				 */
				safe_alloc((void **)&ctl_list,
				    sizeof (con_t), 0);
				ctl_list->cid = cid;
				ctl_list->d = e;
				ctl_list->next = 0;
			}
		}
	}
}

/*
 * Write a newline out and clear the lineout flag.
 */
static void
do_newline(void)
{
	if (lineout) {
		(void) putchar('\n');
		lineout = 0;
	}
}

/*
 * Generalized printf function that determines what extra
 * to print out if we're in raw mode. At this time we
 * don't care about errors.
 */
static void
push_out(char *message, ...)
{
	va_list args;

	va_start(args, message);
	if (do_raw && lineout == 1)
		(void) putchar(',');
	(void) vprintf(message, args);
	va_end(args);
	lineout = 1;
}

/*
 * Emit the header string when -e is specified.
 */
static void
print_err_hdr(void)
{
	char obuf[SMALL_SCRATCH_BUFLEN];

	if (do_conversions == 0) {
		if (!(do_disk & DISK_EXTENDED)) {
			(void) snprintf(obuf, sizeof (obuf),
			    "%11s", one_blank);
			push_out(obuf);
		}
	} else if (do_disk == DISK_ERRORS)
		push_out(two_blanks);
	else
		push_out(one_blank);
	push_out("---- errors --- ");
}

/*
 * Emit the header string when -e is specified.
 */
static void
print_disk_header(void)
{
	push_out(disk_header);
}

/*
 * Write out a timestamp. Format is all that goes out on
 * the line so no use of push_out.
 *
 * Write out as decimal reprentation of time_t value
 * (-T u was specified) or the string returned from
 * ctime() (-T d was specified).
 */
static void
print_timestamp(void)
{
	time_t t;

	if (time(&t) != -1) {
		if (do_timestamp == UDATE) {
			(void) printf("%ld\n", t);
		} else if (do_timestamp == CDATE) {
			char *cpt;

			cpt = ctime(&t);
			if (cpt) {
				(void) printf(cpt);
			}
		}
	}
}

/*
 * No, UINTMAX_MAX isn't the right thing here since
 * it is #defined to be either INT32_MAX or INT64_MAX
 * depending on the whether _LP64 is defined.
 *
 * We want to handle the odd future case of having
 * ulonglong_t be more than 64 bits but we have
 * no nice #define MAX value we can drop in place
 * without having to change this code in the future.
 */

u_longlong_t
ull_delta(u_longlong_t old, u_longlong_t new)
{
	if (new >= old)
		return (new - old);
	else
		return ((UINT64_MAX - old) + new + 1);
}

/*
 * Return the number of ticks delta between two hrtime_t
 * values. Attempt to cater for various kinds of overflow
 * in hrtime_t - no matter how improbable.
 */
uint64_t
hrtime_delta(hrtime_t old, hrtime_t new)
{
	uint64_t del;

	if ((new >= old) && (old >= 0L))
		return (new - old);
	else {
		/*
		 * We've overflowed the positive portion of an
		 * hrtime_t.
		 */
		if (new < 0L) {
			/*
			 * The new value is negative. Handle the
			 * case where the old value is positive or
			 * negative.
			 */
			uint64_t n1;
			uint64_t o1;

			n1 = -new;
			if (old > 0L)
				return (n1 - old);
			else {
				o1 = -old;
				del = n1 - o1;
				return (del);
			}
		} else {
			/*
			 * Either we've just gone from being negative
			 * to positive *or* the last entry was positive
			 * and the new entry is also positive but *less*
			 * than the old entry. This implies we waited
			 * quite a few days on a very fast system between
			 * iostat displays.
			 */
			if (old < 0L) {
				uint64_t o2;

				o2 = -old;
				del = UINT64_MAX - o2;
			} else {
				del = UINT64_MAX - old;
			}
			del += new;
			return (del);
		}
	}
}

/*
 * Take the difference of an unsigned 32
 * bit int attempting to cater for
 * overflow.
 */
uint_t
u32_delta(uint_t old, uint_t new)
{
	if (new >= old)
		return (new - old);
	else
		return ((UINT32_MAX - old) + new + 1);
}

#define	STATE_CHANGE_LINE_LENGTH	60

/*
 * Dump out a list of strings, splitting the
 * output into STATE_CHANGE_LINE_LENGTH chunks.
 * prefix the strings with "<<" and terminate
 * with ">>".
 */

static void
display(char **lofdata, char *m, uint_t nents)
{
	uint_t i;
	uint_t cc;
	size_t slen;

	cc = 2;
	(void) printf("<<");
	for (i = 0; i < nents; i++, lofdata++) {
		slen = strlen(*lofdata);
		if ((cc + slen) <= STATE_CHANGE_LINE_LENGTH) {
			cc += (uint_t)slen + 1;
		} else {
			(void) printf("\n  ");
			cc = 2;
		}
		(void) printf("%s ", *lofdata);
	}
	slen = strlen(m);
	if ((cc + slen + 2) > STATE_CHANGE_LINE_LENGTH)
		(void) printf("\n  ");
	(void) printf("%s>>\n\n", m);
}

/*
 * Convert the kstat name into the real device name
 */
void
do_disk_name(struct kstat *ksp, diskinfo_t *disk)
{
	disk_list_t *d;
	char *tmpnm;
	int partition;

	/*
	 * Unless this is overidden later in the routine,
	 * it will cause the caller to emit the kstat name.
	 */
	disk->device_name = NULL;

	d = lookup_ks_name(ksp->ks_name, dlist);
	if (d) {
		if (do_partitions || do_partitions_only) {
			tmpnm = ksp->ks_name;
			while (*tmpnm && *tmpnm != ',')
				tmpnm++;
			if (*tmpnm == ',') {
				tmpnm++;
				partition = (int)(*tmpnm - 'a');
			} else
				partition = -1;
		} else
			partition = -1;

		/*
		 * This is a regular slice. Create the name and
		 * copy it for use by the calling routine.
		 */
		if (partition >= 0 && partition < NDKMAP) {
			if (d->flags & SLICES_OK) {
				char dbuf[SMALL_SCRATCH_BUFLEN];

				(void) snprintf(dbuf, sizeof (dbuf),
				    "%ss%d", d->dsk, partition);
				safe_strdup(dbuf,
				    &disk->device_name);
			}
		} else if (partition > 0 && (d->flags & PARTITIONS_OK)) {
			/*
			 * An Intel fdisk partition possibly. See if
			 * if falls in the range of allowable
			 * partitions. The fdisk partions show up
			 * after the traditional slices so we determine
			 * which partition we're in and return that.
			 * The NUMPART + 1 is not a mistake. There are
			 * currently FD_NUMPART + 1 partitions that
			 * show up in the device directory.
			 */
			partition -= NDKMAP;
			if (partition >= 0 && partition < (FD_NUMPART + 1)) {
				char pbuf[SMALL_SCRATCH_BUFLEN];

				(void) snprintf(pbuf,
				    sizeof (pbuf), "%sp%d",
				    d->dsk, partition);
				safe_strdup(pbuf,
				    &disk->device_name);
			}
		} else {
			/*
			 * This is the main device name.
			 */
			safe_strdup(d->dsk, &disk->device_name);
		}
		safe_strdup(d->dname, &disk->dname);
		if (do_controller && (partition == -1))
			add_controller(disk);
		else
			disk->cn = 0;
	}
}

/*
 * Create and arm the timer. Used only when an interval has been specified.
 * Used in lieu of poll to ensure that we provide info for exactly the
 * desired period.
 */
void
set_timer(int interval)
{
	timer_t t_id;
	itimerspec_t time_struct;
	struct sigevent sig_struct;
	struct sigaction act;

	bzero(&sig_struct, sizeof (struct sigevent));
	bzero(&act, sizeof (struct sigaction));

	/* Create timer */
	sig_struct.sigev_notify = SIGEV_SIGNAL;
	sig_struct.sigev_signo = SIGUSR1;
	sig_struct.sigev_value.sival_int = 0;

	if (timer_create(CLOCK_REALTIME, &sig_struct, &t_id) != 0) {
		fail(1, "Timer creation failed");
	}

	act.sa_handler = handle_sig;

	if (sigaction(SIGUSR1, &act, NULL) != 0) {
		fail(1, "Could not set up signal handler");
	}

	time_struct.it_value.tv_sec = interval;
	time_struct.it_value.tv_nsec = 0;
	time_struct.it_interval.tv_sec = interval;
	time_struct.it_interval.tv_nsec = 0;

	/* Arm timer */
	if ((timer_settime(t_id, 0, &time_struct, NULL)) != 0) {
		fail(1, "Setting timer failed");
	}
}
/* ARGSUSED */
void
handle_sig(int x)
{
}
