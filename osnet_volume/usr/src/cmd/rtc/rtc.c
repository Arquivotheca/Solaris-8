/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma ident	"@(#)rtc.c	1.6	97/06/16 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <sys/sysi86.h>
#include <sys/errno.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>

static char *progname;
static char *zonefile = "/etc/rtc_config";
static FILE *zonefptr;
static char zone_info[256];
static char zone_lag[256];
static char tz[256] = "TZ=";
extern int errno;
int debug = 0;
int lag;
int errors_ok = 0; /* allow "rtc no-args" to be quiet when not configured */
static time_t clock_val;
static char zone_comment[] =
	"#\n"
	"#	This file (%s) contains information used to manage the\n"
	"#	x86 real time clock hardware.  The hardware is kept in\n"
	"#	the machine's local time for compatibility with other x86\n"
	"#	operating systems.  This file is read by the kernel at\n"
	"#	boot time.  It is set and updated by the /usr/sbin/rtc\n"
	"#	command.  The 'zone_info' field designates the local\n"
	"#	time zone.  The 'zone_lag' field indicates the number\n"
	"#	of seconds between local time and Greenwich Mean Time.\n"
	"#\n";

/*
 *	Open the configuration file and extract the
 *	zone_info and the zone_lag.  Return 0 if successful.
 */
int
open_zonefile()
{
	char b[256], *s;
	int lag_hrs;

	if ((zonefptr = fopen(zonefile, "r")) == NULL) {
		if (errors_ok == 0)
			fprintf(stderr, "%s: cannot open %s: errno = %d\n",
				progname, zonefile, errno);
		return (1);
	}

	for (;;) {
		if ((s = fgets(b, sizeof (b), zonefptr)) == NULL)
			break;
		if ((s = strchr(s, 'z')) == NULL)
			continue;
		if (strncmp(s, "zone_info", 9) == 0) {
			s += 9;
			while (*s != 0 && *s != '=')
				s++;
			if (*s == '=') {
				s++;
				while (*s != 0 && (*s == ' ' || *s == '\t'))
					s++;
				strncpy(zone_info, s, sizeof (zone_info));
				s = zone_info;
				while (*s != 0 && *s != '\n')
					s++;
				if (*s == '\n')
					*s = 0;
			}
		} else if (strncmp(s, "zone_lag", 8) == 0) {
			s += 8;
			while (*s != 0 && *s != '=')
				s++;
			if (*s == '=') {
				s++;
				while (*s != 0 && (*s == ' ' || *s == '\t'))
					s++;
				strncpy(zone_lag, s, sizeof (zone_lag));
				s = zone_lag;
				while (*s != 0 && *s != '\n')
					s++;
				if (*s == '\n')
					*s = 0;
			}
		}
	}
	lag = atoi(zone_lag);
	lag_hrs = lag / 3600;
	if (zone_info[0] == 0) {
		fprintf(stderr, "%s: zone_info field is invalid\n",
		    progname);
		zone_info[0] = 0;
		zone_lag[0] = 0;
		return (1);
	}
	if (zone_lag[0] == 0) {
		fprintf(stderr, "%s: zone_lag field is invalid\n",
		    progname);
		zone_lag[0] = 0;
		return (1);
	}
	if ((lag_hrs < -24) || (lag_hrs > 24)) {
		fprintf(stderr, "%s: a GMT lag of %d is out of range\n",
		    progname, lag_hrs);
		zone_info[0] = 0;
		zone_lag[0] = 0;
		return (1);
	}
	if (debug)
		fprintf(stderr, "zone_info = %s,   zone_lag = %s\n",
		    zone_info, zone_lag);
	if (debug)
		fprintf(stderr, "lag (decimal) is %d\n", lag);

	fclose(zonefptr);
	zonefptr = NULL;
	return (0);
}

void
display_zone_string(void)
{
	if (open_zonefile() == 0)
		printf("%s\n", zone_info);
	else
		printf("GMT\n");
}

long
set_zone(char *zone_string)
{
	struct tm *tm;
	long current_lag;

	(void) umask(0022);
	if ((zonefptr = fopen(zonefile, "w")) == NULL) {
		fprintf(stderr, "%s: cannot open %s: errno = %d\n",
			progname, zonefile, errno);
		return (0);
	}

	tz[3] = 0;
	(void) strncat(tz, zone_string, 253);
	if (debug)
		fprintf(stderr, "Time Zone string is '%s'\n", tz);

	putenv(tz);
	if (debug)
		system("env | grep TZ");

	time(&clock_val);

	tm = localtime(&clock_val);
	current_lag = tm->tm_isdst ? altzone : timezone;
	if (debug)
		printf("%s DST.    Lag is %d.\n", tm->tm_isdst ? "Is" :
		    "Is NOT",  tm->tm_isdst ? altzone : timezone);

	fprintf(zonefptr, zone_comment, zonefile);
	fprintf(zonefptr, "zone_info=%s\n", zone_string);
	fprintf(zonefptr, "zone_lag=%d\n", tm->tm_isdst ? altzone : timezone);
	fclose(zonefptr);
	zonefptr = NULL;
	return (current_lag);
}

void
correct_rtc_and_lag()
{
	struct tm *tm;
	long adjustment;
	long kernels_lag;
	long current_lag;

	if (open_zonefile())
		return;

	tz[3] = 0;
	(void) strncat(tz, zone_info, 253);
	if (debug)
		fprintf(stderr, "Time Zone string is '%s'\n", tz);

	putenv(tz);
	if (debug)
		system("env | grep TZ");

	time(&clock_val);
	tm = localtime(&clock_val);
	current_lag = tm->tm_isdst ? altzone : timezone;

	if (current_lag != lag) {	/* if file is wrong */
		if (debug)
			fprintf(stderr, "correcting file");
		(void) set_zone(zone_info);	/* then rewrite file */
	}

	sysi86(GGMTL, &kernels_lag);
	if (current_lag != kernels_lag) {
		if (debug)
			fprintf(stderr, "correcting kernel's lag");
		sysi86(SGMTL, current_lag);	/* correct the lag */
		sysi86(WTODC);			/* set the rtc to */
						/* new local time */
	}
}

void
initialize_zone(char *zone_string)
{
	long current_lag;

	/* write the config file */
	current_lag = set_zone(zone_string);

	/* correct the lag */
	sysi86(SGMTL, current_lag);

	/*
	 * set the unix time from the rtc,
	 * assuming the rtc was the correct
	 * local time.
	 */
	sysi86(RTCSYNC);
}

void
usage()
{
	static char Usage[] = "Usage:\n\
rtc [-c] [-z time_zone] [-?]\n";

	fprintf(stderr, Usage);
}

void
verbose_usage()
{
	static char Usage1[] = "\
	Options:\n\
	    -c\t\tCheck and correct for daylight savings time rollover.\n\
	    -z [zone]\tRecord the zone info in the config file.\n";

	fprintf(stderr, Usage1);
}

main(int argc, char *argv[])
{
	extern	int optind;
	extern	char *optarg;
	int c;

	progname = argv[0];

	if (argc == 1) {
		errors_ok = 1;
		display_zone_string();
	}

	while ((c = getopt(argc, argv, "cz:d")) != EOF) {
		switch (c) {
		case 'c':
			correct_rtc_and_lag();
			continue;
		case 'z':
			initialize_zone(optarg);
			continue;
		case 'd':
			debug = 1;
			continue;
		case '?':
			verbose_usage();
			exit(0);
		default:
			usage();
			exit(1);
		}
	}
	exit(0);
}
