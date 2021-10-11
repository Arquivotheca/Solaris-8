/* Copyright (c) 1991 by Sun Microsystems, Inc. */

#pragma	ident	"@(#)audio_clean.c	1.12	97/12/16 SMI"

/*
 * audio_clean - Clear any residual data that may be residing in the
 *		 in the audio device driver or chip.
 *
 *		 Usage: audio_clean -[isf] device_name information_label
 *		 Note that currently the same operation is performed for
 *		 all three flags.  Also, information is ignored because
 *		 audio device does not involve removable media.  It and
 *		 support for the flags is provided here so that the framework
 *		 is place if added functionality is required.
 */

#include <locale.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>
#include <libintl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/audioio.h>

#include <stropts.h>
#include <sys/ioctl.h>

#ifndef TEXT_DOMAIN
#define	TEXT_DOMAIN	"SUNW_OST_OSCMD"
#endif

#define	TRUE 		1
#define	FALSE 		0
#define	NUM_ARGUMENTS	3
#define	error_msg	(void) fprintf		/* For lint */
#define	BUF_SIZE 512

static void clear_info();
static void clear_prinfo();
static void usage();
static void first_field();

/* Local variables */
static char *prog; /* Name program invoked with */
static char prog_desc[] = "Clean the audio device"; /* Used by usage message */
static char prog_opts[] = "ifs"; /* For getopt, flags */
static char dminfo_str[] = "dminfo -v -n"; /* Cmd to xlate name to device */

static char *Audio_dev = "/dev/audio";	/* Device name of audio device */
static char *Inf_label;			/* label supplied on cmd line */
static int Audio_fd = -1;		/* Audio device file desc. */

/* Global variables */
extern int	getopt();
extern int	optind;
extern char	*optarg;
extern int	errno;
extern char	*strcat();

/*
 * main()		Main parses the command line arguments,
 *			opens the audio device, and calls clear_info().
 *			to set the info structure to a known state, and
 *			then perfroms an ioctl to set the device to that
 *			state.
 *
 *			Note that we use the AUDIO_SETINFO ioctl instead
 *			of the low level AUDIOSETREG command which is
 *			used to perform low level operations on the device.
 *			If a process had previously used AUDIOSETREG to monkey
 *			with the device, then the driver would have reset the
 *			chip when the process performed a close, so we don't
 *			worry about this case
 */

#ifdef AUDIO_CLEAN_DEBUG
static void print_prinfo();
static void print_info();
#endif

main(argc, argv)
int	argc;
char	**argv;
{
	int	err = 0;
	struct stat st;
	audio_info_t	info;
	int	i;
	int	forced = 0;  /* Command line options */
	int	initial = 0;
	int	standard = 0;
	char	cmd_str[BUF_SIZE];
	char	map[BUF_SIZE];
	FILE * fp;

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	prog = argv[0];		/* save program initiation name */

	/*
	 * Parse arguments.  Currently i, s and f all do the
	 * the same thing.
	 */

	if (argc != NUM_ARGUMENTS) {
		usage();
		goto error;
	}

	while ((i = getopt(argc, argv, prog_opts)) != EOF) {
		switch (i) {
		case 'i':
			initial = TRUE;
			if (standard || forced)
				err++;
			break;
		case 's':
			standard = TRUE;
			if (initial || forced)
				err++;
			break;
		case 'f':
			forced = TRUE;
			if (initial || standard)
				err++;
			break;
		case '?':
			err++;
			break;
		}
		if (err) {
			usage();
			exit(1);
		}
		argc -= optind;
		argv += optind;
	}

#ifdef NOTDEF
	Inf_label = argv[1]; /* Inf label not used now. This is for future */
	Inf_label = Inf_label;	/* For lint */
#endif

	*cmd_str = 0;
	(void) strcat(cmd_str, dminfo_str);
	(void) strcat(cmd_str, " ");
	(void) strcat(cmd_str, argv[0]); /* Agrv[0] is the name of the device */

	if ((fp = popen(cmd_str, "r")) == NULL) {
		error_msg(stderr, gettext("%s couldn't execute \"%s\"\n"), prog,
		    cmd_str);
		exit(1);
	}

	if (fread(map, 1, BUF_SIZE, fp) == 0) {
		error_msg(stderr, gettext("%s couldn't execute \"%s\"\n"), prog,
		    cmd_str);
		exit(1);
	}

	(void) pclose(fp);

	first_field(map, Audio_dev);  /* Put the 1st field in dev */

	/*
	 * Validate and open the audio device
	 */
	err = stat(Audio_dev, &st);

	if (err < 0) {
		error_msg(stderr, gettext("%s: cannot stat "), prog);
		perror(Audio_dev);
		exit(1);
	}
	if (!S_ISCHR(st.st_mode)) {
		error_msg(stderr, gettext("%s: %s is not an audio device\n"),
			prog, Audio_dev);
		exit(1);
	}

	/*
	 * Since the device /dev/audio can suspend if someone else is
	 * using it we check to see if we're going to hang before we
	 * do anything.
	 */
	/* Try it quickly, first */
	Audio_fd = open(Audio_dev, O_WRONLY | O_NDELAY);

	if ((Audio_fd < 0) && (errno == EBUSY)) {
		error_msg(stderr, gettext("%s: waiting for %s..."),
			prog, Audio_dev);
		(void) fflush(stderr);

		/* Now hang until it's open */
		Audio_fd = open(Audio_dev, O_WRONLY);
		if (Audio_fd < 0) {
			perror(Audio_dev);
			goto error;
		}
	} else if (Audio_fd < 0) {
		error_msg(stderr, gettext("%s: error opening "), prog);
		perror(Audio_dev);
		goto error;
	}


	/*
	 * Read the audio_info structure.
	 * Currently, we overwrite all these values that we get back,
	 * but this is a good test that GETINFO/SETINFO ioctls are
	 * supported.
	 */

	if (ioctl(Audio_fd, AUDIO_GETINFO, &info) != 0)  {
		perror("Ioctl error");
		goto error;
	}

#ifdef AUDIO_CLEAN_DEBUG
	print_info(&info);
#endif

	/*
	 * Clear the data structure. Clear_info set the info structure
	 * to a known state.
	 */
	clear_info(&info);

	if (ioctl(Audio_fd, AUDIO_SETINFO, &info) != 0) {
		perror(gettext("Ioctl error"));
		goto error;
	}


	(void) close(Audio_fd);			/* close output */
	exit(0);
	/*NOTREACHED*/
error:
	(void) close(Audio_fd);			/* close output */
	exit(1);
	/*NOTREACHED*/
}


/*
 * clear_info(info)	- Set the info structure to a known state.
 *			  Several of the field that are modified here are
 *			  read-only and will not be modified in the device
 *			  driver.  We set them all here for completeness.
 *			  Thess values were the values present after rebooting
 *			  a 4.1.1 rev B system.
 */

static void
clear_info(info)
audio_info_t *info;
{
	/*
	 * bug #4053328 fixed - no further use of our own
	 * cleaning of info structure. Use macro from sys/audioio.h
	 */
	AUDIO_INITINFO(info);
}

#ifdef AUDIO_CLEAN_DEBUG
static void
print_info(info)
audio_info_t *info;
{
	print_prinfo(&info->play);
	print_prinfo(&info->record);
	printf("monitor_gain %d\n", info->monitor_gain);
}
#endif


#ifdef AUDIO_CLEAN_DEBUG
static void
print_prinfo(prinfo)
audio_prinfo_t *prinfo;
{
	/* The following values decribe audio data encoding: */
	printf("sample_rate %d\n",	prinfo->sample_rate);
	printf("channels %d\n", 	prinfo->channels);
	printf("precision %d\n", 	prinfo->precision);
	printf("encoding %d\n", 	prinfo->encoding);

	/* The following values control audio device configuration */
	printf("gain %d\n", 	prinfo->gain);
	printf("port %d\n", 	prinfo->port);
	printf("vail_ports %d\n", 	prinfo->avail_ports);

	/* These are Reserved for future use, but we clear them  */
	printf("_xxx[0] %d\n", 	prinfo->_xxx[0]);
	printf("_xxx[1] %d\n", 	prinfo->_xxx[1]);
	printf("_xxx[2] %d\n", 	prinfo->_xxx[2]);

	/* The following values describe driver state */
	printf("samples %d\n", 	prinfo->samples);
	printf("eof %d\n", 	prinfo->eof);
	printf("pause %d\n", 	prinfo->pause);
	printf("error %d\n", 	prinfo->error);
	printf("waiting %d\n", 	prinfo->waiting);
	printf("balance %d\n", 	prinfo->balance);

	/* The following values are read-only state flags */
	printf("open %d\n", 	prinfo->open);
	printf("active %d\n", 	prinfo->active);
}
#endif



/*
 * usage()		- Print usage message.
 */

static void
usage()
{
	error_msg(stderr,
		gettext("usage: %s [-s|-f|-i] device info_label\n"), prog);
}


/*
 * first_field(string, item)	- return the first substring in string
 *				  before the ':' in "item"
 */
static void
first_field(string, item)
char	*string, *item;
{
	item = string;

	while (*item != ':')
		item++;
	*item = 0;
}
