/*
 *  Copyright (c) 1993, Sun Microsystems Inc.
 */
#pragma ident   "@(#)cg14config.c 1.8     94/12/20 SMI"

#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/fbio.h>
#include <sys/cg14io.h>
#include <sys/cg14reg.h>
#include <errno.h>
#include <sys/stat.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define	EEPROM_OLD_LOC	"/usr/kvm/eeprom"
#define	EEPROM_NEW_LOC	"/usr/platform/sun4m/sbin/eeprom"


static const mon_spec_t msr[] = {
	1024,	768,	60,
	64000000, 0, 16, 128, 160, 1024, 0, 60, 2, 6, 29, 768, 0,

	1024,	768,	66,
	74000000, 0, 4, 124, 160, 1024, 0, 66, 1, 5, 39, 768, 0,

	1024,	768,	70,
	74000000, 0, 16, 136, 136, 1024, 0, 70, 2, 6, 32, 768, 0,

	1152,	900,	66,
	94000000, 0, 40, 64, 272, 1152, 0, 66, 2, 8, 27, 900, 0,

	1152,	900,	76,
	108000000, 0, 28, 64, 260, 1152, 0, 76, 2, 8, 33, 900, 0,

	1280,	1024,	66,
	117000000, 0, 24, 64, 280, 1280, 0, 66, 2, 8, 41, 1024, 0,

	1280,	1024,	76,
	135000000, 0, 32, 64, 288, 1280, 0, 76, 2, 8, 32, 1024, 0,

	1600,	1280,	66,
	189000000, 0, 0, 256, 384, 1600, 0, 66, 0, 10, 44, 1280, 0,

	1920,	1080,	72,
	216000000, 0, 48, 216, 376, 1920, 0, 72, 3, 3, 86, 1080, 0,

	0,	0,	0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};


static	int
usage(char *name)
{
	printf("Usage:  %s [-d cg14devname] [-r res] [-g gammaval] ", name);
	printf("[-G file] [-u degammaval] [-U file]\n");
	return (2);
}


static	void
parse_res(char *res_str, whf_t *res_whf)
{
	char *cp = res_str;
	char *p, *q;

	while (isspace(*cp))
		cp++;

	p = q = strtok(cp, "@x");
	while (*p) {
		if (!isdigit(*p))
			goto bad;
		p++;
	}

	res_whf->width = atoi(q);

	p = q = strtok(NULL, "@x");
	while (*p) {
		if (!isdigit(*p))
			goto bad;
		p++;
	}

	res_whf->height = atoi(q);

	p = q = strtok(NULL, "@x");
	while (*p) {
		if (!isdigit(*p))
			goto bad;
		p++;
	}

	res_whf->vfreq = atoi(q);
	return;
bad:
	fprintf(stderr, "Badly formed resolution syntax.\n");
	exit(2);
}


static	void
make_prom_res(whf_t *res_whf, char *cp)
{
/*
 *  XXX The 'screen' variable may NOT be the current console!
 *  Resolution 1280x1024@76 is taken care of here as a special
 *  case because prom only recognizes it as 1280x1024@76m (note the
 *  suffix 'm' for mutant mode).
 */
	if ((res_whf->width == 1280) && (res_whf->height == 1024) &&
		(res_whf->vfreq == 76))
		sprintf(cp, "screen:r%dx%dx%dm", res_whf->width,
			res_whf->height, res_whf->vfreq);
	else
		sprintf(cp, "screen:r%dx%dx%d", res_whf->width,
			res_whf->height, res_whf->vfreq);
}


static	int
console_set_res(whf_t *res_whf)
{
	static char pres[512];
	char sysbuf[512];
	char tmpbuf[100];

	make_prom_res(res_whf, pres);

	/*
	 * The eeprom command has been moved from /usr/kvm to platform
	 * specific directory since SunOS 5.5.
	 */
	if (access(EEPROM_NEW_LOC, F_OK) == 0)
		strcpy(tmpbuf, EEPROM_NEW_LOC);
	else if (access(EEPROM_OLD_LOC, F_OK) == 0)
		strcpy(tmpbuf, EEPROM_OLD_LOC);
	sprintf(sysbuf, "%s output-device=%s", tmpbuf, pres);

	if (system(sysbuf) < 0) {
		perror("Could not change console resolution");
		return (-1);
	}
	return (0);
}


static	int
non_console_set_res(int fd, struct mdi_set_resolution *msr, char *device)
{
	if (ioctl(fd, MDI_SET_RESOLUTION, msr) < 0) {
		if (errno == EBUSY) {
			fprintf(stderr, "%s: device busy.\n", device);
			exit(4);
		} else {
			fprintf(stderr, "Could not change %s device "
				"resolution", device);
		}
		return (-1);
	}
	return (0);
}


static	int
dev_is_console_fb(int cg14fd)
{
	int fbfd;
	struct stat pbuf, cbuf;
	int stat;

	/* Is the console a frame buffer? */
	stat = system("/usr/sbin/prtconf -F > /dev/null");
	if (WIFEXITED(stat)) {
		if (WEXITSTATUS(stat))
			return (0);
	}

	/*
	 * Ok. We know the console is /dev/fb.
	 * Is the device /dev/fb too?
	 */
	if ((fbfd = open("/dev/fb", O_RDONLY)) < 0) {
		/* /dev/fb may not exist */
		return (0);
	}

	if ((fstat(cg14fd, &pbuf)) < 0) {
		perror("Could not stat.\n");
		exit(2);
	}

	if ((fstat(fbfd, &cbuf)) < 0) {
		perror("Could not stat.\n");
		exit(2);
	}

	/* Compare the dev_t's */
	if (cbuf.st_rdev != pbuf.st_rdev)
		return (0); /* The device is not /dev/fb */
	else
		return (1); /* The device is /dev/fb */
}


/* given the whf_t, find the corresponding (struct mdi_set_resolution) */
static	struct mdi_set_resolution *
get_timing_params(whf_t *res_whf)
{
	struct mon_spec *p;
	int i;

	/* Table walk */
	for (i = 0; msr[i].ms_whf.width; i++) {

		p = (struct mon_spec *)&msr[i];

		if (p->ms_whf.width == res_whf->width &&
				p->ms_whf.height == res_whf->height &&
				p->ms_whf.vfreq == res_whf->vfreq) {

			return (&(p->ms_msr));
		}
	}
	return ((struct mdi_set_resolution *)NULL);
}


/* Make sure the requested resolution is supported */
static	int
validate_resolution(whf_t *res_whf)
{
	int i;

	/* Table walk */
	for (i = 0; msr[i].ms_whf.width; i++) {

		if (msr[i].ms_whf.width == res_whf->width &&
				msr[i].ms_whf.height == res_whf->height &&
				msr[i].ms_whf.vfreq == res_whf->vfreq) {

			return (1);
		}
	}
	return (0);
}


main(int argc, char *argv[])
{
	int cg14fd, i;
	int dflg = 0;
	int rflg = 0;
	int gflg = 0;
	int uflg = 0;
	int Gflg = 0;
	int Uflg = 0;
	char c;
	char device[512] = "/dev/fb"; /* unless changed by dflg. */
	struct mdi_cfginfo cfginfo;
	struct fbgattr gattr;
	whf_t res_whf;
	char *res_str;
	double gammaval, degammaval;
	char gammafile[512], degammafile[512];

	struct mdi_set_gammalut glut;
	struct mdi_set_degammalut deglut;
	unsigned short red[MDI_CMAP_ENTRIES], green[MDI_CMAP_ENTRIES],
			blue[MDI_CMAP_ENTRIES];
	u_char degamma[MDI_CMAP_ENTRIES];


	/* Must have some args */
	if (argc < 2)
		usage(argv[0]);

/* Validate args */
	while ((c = getopt(argc, argv, "d:r:g:G:u:U:")) != EOF)
		switch (c) {
		case 'd':
			dflg++;
			strcpy(device, optarg);
			break;
		case 'r':
			rflg++;
			res_str = optarg;
			break;
		case 'g':
			gflg++;
			gammaval = atof(optarg);
			break;
		case 'G':
			Gflg++;
			strcpy(gammafile, optarg);
			break;
		case 'u':
			uflg++;
			degammaval = atof(optarg);
			break;
		case 'U':
			Uflg++;
			strcpy(degammafile, optarg);
			break;
		default:
			usage(argv[0]);
			break;
		}

	if (rflg) {
		(void) parse_res(res_str, &res_whf);
	}

	if (gflg && gammaval < 1.0e-6) {
		fprintf(stderr, "Gammaval must be greater than 0.\n");
		exit(4);
	}
	if (uflg && degammaval < 1.0e-6) {
		fprintf(stderr, "Degammaval must be greater than 0.\n");
		exit(4);
	}

	if (gflg && Gflg) {
		fprintf(stderr, "-g and -G are mutually exclusive\n");
		exit(2);
	}

	if (uflg && Uflg) {
		fprintf(stderr, "-u and -U are mutually exclusive\n");
		exit(2);
	}

	/* open device */
	if ((cg14fd = open(device, O_RDWR)) == -1) {
		char buf[100];
		sprintf(buf, "Could not open %s device", device);
		perror(buf);
		exit(1);
	}

	if (ioctl(cg14fd, FBIOGATTR, &gattr) == -1) {
		perror("FBIOGATTR failed.");
		exit(2);
	}

	if (gattr.real_type != FBTYPE_MDICOLOR) {
		fprintf(stderr, "%s device is not a cgfourteen.\n", device);
		exit(1);
	}

	if (ioctl(cg14fd, MDI_GET_CFGINFO, &cfginfo) == -1) {
		perror("GET_CFGINFO failed.");
		exit(2);
	}

	if (rflg) {
		/* We must assume the user is using 8bit mode.  Sigh */
		if (res_whf.width * res_whf.height > cfginfo.mdi_size) {
			fprintf(stderr,
				"Not enough VRAM for this resolution.\n");
			exit(3);
		}

		/* Make sure it's one we support */
		if (validate_resolution(&res_whf) == 0) {
			fprintf(stderr, "Unsupported cgfourteen resolution.\n");
			exit(2);
		}

		if ((int) dev_is_console_fb(cg14fd)) {
			if (console_set_res(&res_whf) != 0)
				exit(2);
		} else {
			struct mdi_set_resolution *msr;

			/* Convert whf_t to mdi_set_resolution */
			msr = get_timing_params(&res_whf);
			if (msr == (struct mdi_set_resolution *)NULL) {
				fprintf(stderr, "Resolution not supported.\n");
				exit(2);
			}

			if (non_console_set_res(cg14fd, msr, device) != 0)
				exit(2);
		}
	}

	if (gflg) {
		double C, T;
		unsigned short v;

		for (i = 0; i < MDI_CMAP_ENTRIES; i++) {
			C = (double) i / 255.0;
			T = pow(C, 1.0/gammaval);
			v = T * 1023.0 + 0.5;
			red[i] = green[i] = blue[i] = v;
		}

	} else if (Gflg) {
		FILE *gstrm;

		gstrm = fopen(gammafile, "r");
		if (gstrm == (FILE *)NULL) {
			fprintf(stderr, "Could not open %s\n", gammafile);
			exit(2);
		}
		for (i = 0; i < MDI_CMAP_ENTRIES; i++) {
			fscanf(gstrm, "%hi %hi %hi",
				&red[i], &green[i], &blue[i]);
			if (red[i] > (u_short)((MDI_CMAP_ENTRIES << 2) - 1) ||
			    green[i] > (u_short)((MDI_CMAP_ENTRIES << 2) - 1) ||
			    blue[i] > (u_short)((MDI_CMAP_ENTRIES << 2) - 1)) {

				fprintf(stderr,
					"Gamma file data out of range.\n");
				exit(4);
			}
		}
	}

	if (gflg || Gflg) {

		glut.index = 0;
		glut.count = MDI_CMAP_ENTRIES;
		glut.red = red;
		glut.green = green;
		glut.blue = blue;

		if (ioctl(cg14fd, MDI_SET_GAMMALUT, &glut) < 0) {
			perror("Could not set gammalut.");
			exit(2);
		}
	}

	if (uflg) {
		double J, D;
		u_char v;

		for (i = 0; i < MDI_CMAP_ENTRIES; i++) {
			J = (double) i / 255.0;
			D = pow(J, degammaval);
			v = D * 255.0 + 0.5;
			degamma[i] = v;
		}

	} else if (Uflg) {
		FILE *ustrm;
		u_int tmp[MDI_CMAP_ENTRIES];

		ustrm = fopen(degammafile, "r");
		if (ustrm == (FILE *)NULL) {
			fprintf(stderr, "Could not open %s\n", degammafile);
			exit(2);
		}
		for (i = 0; i < MDI_CMAP_ENTRIES; i++) {
			fscanf(ustrm, "%i", &tmp[i]);
			if (tmp[i] > (MDI_CMAP_ENTRIES - 1)) {
				fprintf(stderr,
					"Degamma file data out of range.\n");
				exit(4);
			}
			degamma[i] = (u_char)tmp[i];
		}
	}

	if (uflg || Uflg) {

		deglut.index = 0;
		deglut.count = MDI_CMAP_ENTRIES;
		deglut.degamma = degamma;

		if (ioctl(cg14fd, MDI_SET_DEGAMMALUT, &deglut) < 0) {
			perror("Could not set degammalut.");
			exit(2);
		}

		if (ioctl(cg14fd, MDI_GAMMA_CORRECT,
				MDI_GAMMA_CORRECTION_OFF) < 0) {
			perror("Could not turn on degamma.");
			exit(2);
		}
	}

	return (0);
}
