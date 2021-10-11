/*
 * Copyright 1997 Sun Microsystems, Inc. All rights reserved.
 */


#pragma	ident	"@(#)envsen.c 1.3     97/05/20 SMI"

/*LINTLIBRARY*/


/*
 *	Administration program for the photon
 */

#include	<stdlib.h>
#include	<stdio.h>
#include	<sys/file.h>
#include	<sys/errno.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/param.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<errno.h>
#include	<malloc.h>
#include	<memory.h>
#include	<string.h>
#include	<ctype.h>
#include	<assert.h>
#include	<time.h>
#include	<dirent.h>
#include	<libintl.h>	/* gettext */
#include	<sys/sunddi.h>
#include	<sys/systeminfo.h>
#include	<sys/scsi/scsi.h>

#include	"common.h"
#include	"scsi.h"
#include	"state.h"
#include	<sys/scsi/targets/sesio.h>

#define	MAX_CARDS	7
#define	MAXDRIVES	7
#define	ENVSEN_DEV	"ses"

/* global variables */
extern	char	*whoami;
extern	int	Options;
extern	const	OPTION_A;
extern	const	OPTION_C;
extern	const	OPTION_D;
extern	const	OPTION_E;
extern	const	OPTION_P;
extern	const	PVERBOSE;
extern	const	SAVE;
extern	const	OPTION_T;
extern	const	OPTION_L;
extern	const	OPTION_F;
extern	const	OPTION_W;
extern	const	OPTION_Z;
extern	const	OPTION_Y;
extern	const	OPTION_CAPF;

struct envsen_data {
	struct  rsm_es_in  *rsm_es_in;
				/* pointer to data from device */
	char    tid; /* target id of device */
	char	port; /* port number where this card lives */
	char    *path; /* pathname of envsen device */
} envsen_data[MAX_CARDS];

static	int	get_envsen_data(char *, int, int);
static	int	get_port(char *);
static	void	disp_envsen_data(int, int);
static  struct rsm_es_in *read_envsen_data(char *);
static	void	write_envsen_data(struct rsm_es_out *, char *, int, int);
static	void	fill_rsm_es_out(struct rsm_es_out *);
static	void	printbuf(unsigned char *, int);

int	get_envsen_global_data(char **, int, char **, int *);
struct	rsm_es_in *find_card(int);

extern  char    *get_physical_name(char *);
extern	char	*bzero();
extern	char	*bcopy();
extern	int	scsi_get_envsen_data(int, char *, int);
extern	int	scsi_put_envsen_data(int, char *, int);


void
cli_display_envsen_data(char **argv, int argc)
{
	int	nenvsen_devs = 0, port;
	char	*path_phys;


	nenvsen_devs = get_envsen_global_data(argv, argc, &path_phys, &port);

	if (nenvsen_devs) {
		(void) fprintf(stdout, "	       ENCLOSURE STATUS\n");
		(void) fprintf(stdout, "\n");
		disp_envsen_data(nenvsen_devs, 0);
	}

}
int
get_envsen_data(char *path, int port, int complete_path)
{
	int ndevs = 0, dev_path_len, portid, is_pln;
	DIR *dir;
	struct dirent *dirent;
	char *s, *dev_name, *dev_path;

	is_pln = (strstr(path, "SUNW,pln") != NULL);
	if (complete_path) {
		s = strrchr(path, '@');
		if (s != NULL) {
			if (is_pln) {
				int port, target;
				sscanf(s, "@%d,%d", &port, &target);
				envsen_data[ndevs].tid = target;
			} else {
				int target;
				/* must be ISP */
				sscanf(s, "@%x", &target);
				envsen_data[ndevs].tid = target;
			}
		} else
				envsen_data[ndevs].tid = 0;
		if (envsen_data[ndevs].rsm_es_in = read_envsen_data(path))
			ndevs++;
		return (ndevs);
	}

	if ((dev_path = malloc(MAXNAMELEN)) == NULL) {
		(void) fprintf(stdout, "Could not allocate memory\n");
	}
	(void) sprintf(dev_path, "%s/", path);

	P_DPRINTF("dev_path=%s\n", dev_path);
	if ((dir = opendir(dev_path)) == NULL) {
		(void) fprintf(stdout, "%s: could not opendir\n", dev_path);
		return (ndevs);
	}
	dev_path_len = strlen(dev_path);
	while ((dirent = readdir(dir))  != NULL) {
		if (strstr(&dirent->d_name[0], ENVSEN_DEV)) {
			P_DPRINTF("get_envsen_data: scanning %s\n",
				dirent->d_name);
			/* found a envsen device */
			dev_name = malloc(strlen(dirent->d_name) + 1);
			if (dev_name == NULL) {
				(void) fprintf(stderr, "No memory!\n");
				return (ndevs);
			}
			(void) strcpy(dev_name, dirent->d_name);
			envsen_data[ndevs].path = dev_name;
			s = strrchr(dev_name, '@');
			/*
			 * We can have two different types of devices here:
			 * sd uses hex to differentiate targets and LUNs,
			 * ssd uses decimal to differentiate targets and LUNs.
			 */
			if (s != NULL) {
				if (is_pln) {
					int targ;
					sscanf(s, "@%d,%d", &portid, &targ);
					envsen_data[ndevs].port = portid;
					envsen_data[ndevs].tid = targ;
				} else {
					/* Assume ISP device */
					int targ;
					sscanf(s, "@%x", &targ);
					/*
					 * ISPs have no notion of a port so any
					 * ses is good enough
					 */
					envsen_data[ndevs].port = portid = port;
					envsen_data[ndevs].tid = targ;
				}
			} else {
				envsen_data[ndevs].tid = 0;
				envsen_data[ndevs].port = 0;
			}
			P_DPRINTF("port=%x, portid=%x\n", port, portid);
			if ((port != -1) && (port != portid))
				continue;
			P_DPRINTF("%s: tid=%x\n", dev_name,
				envsen_data[ndevs].tid);
			envsen_data[ndevs].rsm_es_in = read_envsen_data(
						strcat(dev_path, dev_name));
			dev_path[dev_path_len] = '\0';
			free(dev_name);
			if (envsen_data[ndevs].rsm_es_in)
				ndevs++;
			if (port != -1)
				return (ndevs);
		}
	}
	return (ndevs);
}

struct rsm_es_in *
read_envsen_data(char *path)
{
	int fd, err;
	extern int errno;
	struct rsm_es_in	*buf;

	P_DPRINTF("Opening envsen device %s\n", path);
	if ((fd = open(path, O_RDONLY)) < 0) {
		(void) fprintf(stderr, "Could not open %s\n", path);
		return (NULL);
	}

	buf = (struct rsm_es_in *)malloc(SCSI_RSM_ES_IN_PAGE_SIZE);
	if (buf == (struct rsm_es_in *)NULL) {
		(void) fprintf(stderr,
				"read_envsen_data: Could not alloc memory!\n");
		return (NULL);
	}

	err  = scsi_get_envsen_data(fd, (char *)buf, SCSI_RSM_ES_IN_PAGE_SIZE);
	(void) close(fd);
	if (err != 0) {
		(void) fprintf(stdout,
				"%s: recv diag failed status: %d\n\n",
				whoami, errno);
		free(buf); /* Free it if we ain't gonna use it */
		buf = NULL;
	} else if (getenv("SSA_P_DEBUG") != NULL) {
		printbuf((unsigned char *)buf, SCSI_RSM_ES_IN_PAGE_SIZE);
		(void) fprintf(stdout, "%s: recv diag successful\n\n",
			whoami);
	}
	return (buf);
}
void
printbuf(unsigned char *buf, int len)
{
	int j = 0;

	(void) printf("buffer = \n");
	while (j < len) {
		(void) printf("	    %2x ", buf[j++]);
		if (j < len) (void) printf("%2x ", buf[j++]);
		if (j < len) (void) printf("%2x ", buf[j++]);
		if (j < len) (void) printf("%2x ", buf[j++]);
		if (j < len) (void) printf("%2x ", buf[j++]);
		if (j < len) (void) printf("%2x ", buf[j++]);
		if (j < len) (void) printf("%2x ", buf[j++]);
		if (j < len) (void) printf("%2x ", buf[j++]);
		(void) printf("\n");
	}
}
void
disp_envsen_data(int nenvsen_devs, int i)
{
	int j;
	struct	rsm_es_in	*rsm_es_in, *rsm_es_in1, *rsm_es_in2;


	switch (nenvsen_devs) {
		case 0:
			return;
		case 1:
			if (envsen_data[i].port == -1) {
				(void) fprintf(stdout, "      CARD on ISP\n\n");
			} else {
				(void) fprintf(stdout,
					"      CARD in tray %d\n\n",
					envsen_data[i].port);
			}
			rsm_es_in = envsen_data[i].rsm_es_in;
			(void) fprintf(stdout, " Abnormal Condition: %s\n",
				(rsm_es_in->abs) ? "YES" : "NO");
			(void) fprintf(stdout, " Immediate Attention: %s\n",
				(rsm_es_in->chk) ? "YES" : "NO");
			(void) fprintf(stdout, " Alarm Status : %s\n",
				(rsm_es_in->alsen) ? "*Sound*" : "OK");
			(void) fprintf(stdout, " Alarm Enabled : %s\n",
				(rsm_es_in->alenb) ? "YES" : "NO");
			(void) fprintf(stdout, " Alarm Setting : %d\n",
				rsm_es_in->altime);
			(void) fprintf(stdout, " Id Sense Bit: %s\n",
				(rsm_es_in->idsen) ? "ON" : "OFF");
			(void) fprintf(stdout, " Spin Delay : %s\n",
				(rsm_es_in->dsdly) ? "YES" : "NO");
			for (j = 1; j <= rsm_es_in->max_pwms; j++) {
				(void) fprintf(stdout,
				" Power Module %d: %s\n", j,
				(rsm_es_in->pwm[j-1].modfail) ? "FAIL" : "OK");
			}
			(void) fprintf(stdout, " Fan Module : %s\n",
				(rsm_es_in->fan.modfail) ? "FAIL" : "OK");
			(void) fprintf(stdout, " Over Temperature : %s\n",
				(rsm_es_in->ovta.modfail) ? "YES" : "NO");
			break;
		case 2:
			(void) fprintf(stdout, "      CARD in tray %d"
					"	   CARD in tray %d\n\n",
				envsen_data[i].port,
				envsen_data[i+1].port);
			rsm_es_in = envsen_data[i].rsm_es_in;
			rsm_es_in1 = envsen_data[i+1].rsm_es_in;
			(void) fprintf(stdout, " Abnormal Condition: %s"
					"     Abnormal Condition: %s\n",
				(rsm_es_in->abs) ? "YES" : "NO",
				(rsm_es_in1->abs) ? "YES" : "NO");
			(void) fprintf(stdout, " Immediate Attention: %s"
					"   Immediate Attention: %s\n",
				(rsm_es_in->chk) ? "YES" : "NO",
				(rsm_es_in1->chk) ? "YES" : "NO");
			(void) fprintf(stdout, " Alarm Status : %s"
					"     Alarm Status : %s\n",
				(rsm_es_in->alsen) ? "*Sound*" : "OK",
				(rsm_es_in->alsen) ? "*Sound*" : "OK");
			(void) fprintf(stdout, " Alarm Enabled: %s"
					"	 Alarm Enabled: %s\n",
				(rsm_es_in->alenb) ? "YES" : "NO",
				(rsm_es_in1->alenb) ? "YES" : "NO");
			(void) fprintf(stdout, " Alarm Setting : %d"
					"	  Alarm Setting : %d\n",
				rsm_es_in->altime, rsm_es_in1->altime);
			(void) fprintf(stdout, " Id Sense Bit: %s"
					"	  Id Sense Bit: %s\n",
				(rsm_es_in->idsen) ? "ON" : "OFF",
				(rsm_es_in1->idsen) ? "ON" : "OFF");
			(void) fprintf(stdout, " Spin Delay: %s"
					"	    Spin Delay: %s\n",
				(rsm_es_in->dsdly) ? "YES" : "NO",
				(rsm_es_in1->dsdly) ? "YES" : "NO");
			for (j = 1; j <= rsm_es_in->max_pwms; j++) {
				if (j <= rsm_es_in1->max_pwms) {
					(void) fprintf(stdout,
						" Power Module %d: %s"
						"	 Power Module %d: %s\n",
						j,
						(rsm_es_in->pwm[j-1].modfail)
							? "FAIL" : "OK",
						j,
						(rsm_es_in1->pwm[j-1].modfail)
							? "FAIL" : "OK");
				} else {
					(void) fprintf(stdout,
						" Power Module %d: %s\n", j,
						(rsm_es_in->pwm[j-1].modfail)
							? "FAIL" : "OK");
				}
			}
			for (; j <= rsm_es_in1->max_pwms; j++) {
				(void) fprintf(stdout, "		       "
				"    Power Module %d: %s\n", j,
						(rsm_es_in1->pwm[j-1].modfail)
							? "FAIL" : "OK");
			}
			(void) fprintf(stdout, " Fan Module : %s"
					"	    Fan Module : %s\n",
				(rsm_es_in->fan.modfail) ? "FAIL" : "OK",
				(rsm_es_in1->fan.modfail) ? "FAIL" : "OK");
			(void) fprintf(stdout, " Over Temperature : %s"
					"     Over Temperature : %s\n",
				(rsm_es_in->ovta.modfail) ? "YES" : "NO",
				(rsm_es_in1->ovta.modfail) ? "YES" : "NO");
			break;
		case 3:
			(void) fprintf(stdout, "      CARD in tray %d"
					"	 CARD in tray %d"
					"	  CARD in tray %d\n\n",
				envsen_data[i].port,
				envsen_data[i+1].port,
				envsen_data[i+2].port);
			rsm_es_in = envsen_data[i].rsm_es_in;
			rsm_es_in1 = envsen_data[i+1].rsm_es_in;
			rsm_es_in2 = envsen_data[i+2].rsm_es_in;
			(void) fprintf(stdout, " Abnormal Condition: %s"
					"    Abnormal Condition: %s"
					"    Abnormal Condition: %s\n",
				(rsm_es_in->abs) ? "YES" : "NO",
				(rsm_es_in1->abs) ? "YES" : "NO",
				(rsm_es_in2->abs) ? "YES" : "NO");
			(void) fprintf(stdout, " Immediate Attention: %s"
					"   Immediate Attention: %s"
					"   Immediate Attention: %s\n",
				(rsm_es_in->chk) ? "YES" : "NO",
				(rsm_es_in1->chk) ? "YES" : "NO",
				(rsm_es_in2->chk) ? "YES" : "NO");
			(void) fprintf(stdout, " Alarm Status : %s"
					"	 Alarm Status : %s"
					"	 Alarm Status : %s\n",
				(rsm_es_in->alsen) ? "*Sound*" : "OK",
				(rsm_es_in1->alsen) ? "*Sound*" : "OK",
				(rsm_es_in2->alsen) ? "*Sound*" : "OK");
			(void) fprintf(stdout, " Alarm Enabled: %s"
					"	 Alarm Enabled: %s"
					"	 Alarm Enabled: %s\n",
				(rsm_es_in->alenb) ? "YES" : "NO",
				(rsm_es_in1->alenb) ? "YES" : "NO",
				(rsm_es_in2->alenb) ? "YES" : "NO");
			(void) fprintf(stdout, " Alarm Setting : %d"
					"	 Alarm Setting : %d"
					"	 Alarm Setting : %d\n",
				rsm_es_in->altime, rsm_es_in1->altime,
				rsm_es_in2->altime);
			(void) fprintf(stdout, " Id Sense Bit: %s"
					"	 Id Sense Bit: %s"
					"	 Id Sense Bit: %s\n",
				(rsm_es_in->idsen) ? "ON" : "OFF",
				(rsm_es_in1->idsen) ? "ON" : "OFF",
				(rsm_es_in2->idsen) ? "ON" : "OFF");
			(void) fprintf(stdout, " Spin Delay: %s"
					"	    Spin Delay: %s"
					"	    Spin Delay: %s\n",
				(rsm_es_in->dsdly) ? "YES" : "NO",
				(rsm_es_in1->dsdly) ? "YES" : "NO",
				(rsm_es_in2->dsdly) ? "YES" : "NO");
			for (j = 1; j <= rsm_es_in->max_pwms; j++) {
				if (j <= rsm_es_in1->max_pwms) {
					if (j <= rsm_es_in2->max_pwms) {
						(void) fprintf(stdout,
						" Power Module %d: %s"
						"	Power Module %d: %s"
						"	Power Module %d: %s\n",
						j,
						(rsm_es_in->pwm[j-1].modfail)
							? "FAIL" : "OK",
						j,
						(rsm_es_in1->pwm[j-1].modfail)
							? "FAIL" : "OK",
						j,
						(rsm_es_in2->pwm[j-1].modfail)
							? "FAIL" : "OK");
					} else {
						(void) fprintf(stdout,
						" Power Module %d: %s"
						"	Power Module %d: %s\n",
						j,
						(rsm_es_in->pwm[j-1].modfail)
							? "FAIL" : "OK",
						j,
						(rsm_es_in1->pwm[j-1].modfail)
							? "FAIL" : "OK");
					}
				} else {
					if (j <= rsm_es_in2->max_pwms) {
						(void) fprintf(stdout,
						" Power Module %d: %s"
						"			    "
						"      Power Module %d: %s\n",
						j,
						(rsm_es_in->pwm[j-1].modfail)
							? "FAIL" : "OK",
						j,
						(rsm_es_in2->pwm[j-1].modfail)
							? "FAIL" : "OK");
					} else {
						(void) fprintf(stdout,
						" Power Module %d: %s",
						j,
						(rsm_es_in->pwm[j-1].modfail)
							? "FAIL" : "OK");
					}
				}
			}
			for (; j <= rsm_es_in1->max_pwms; j++) {
				if (j <= rsm_es_in2->max_pwms) {
					(void) fprintf(stdout,
				"			   Power Module %d: %s"
				"	Power Module %d: %s\n",
					j,
					(rsm_es_in1->pwm[j-1].modfail)
						? "FAIL" : "OK",
					j,
					(rsm_es_in2->pwm[j-1].modfail)
						? "FAIL" : "OK");
				} else {
					(void) fprintf(stdout,
					"		      "
					"     Power Module %d: %s\n",
					j,
					(rsm_es_in1->pwm[j-1].modfail)
						? "FAIL" : "OK");
				}
			}
			for (; j <= rsm_es_in2->max_pwms; j++) {
				(void) fprintf(stdout,
				"			      "
				"		       Power Module %d: %s\n",
					j,
					(rsm_es_in2->pwm[j-1].modfail)
						? "FAIL" : "OK");
			}
			(void) fprintf(stdout, " Fan Module : %s"
					"	   Fan Module : %s"
					"	   Fan Module : %s\n",
				(rsm_es_in->fan.modfail) ? "FAIL" : "OK",
				(rsm_es_in1->fan.modfail) ? "FAIL" : "OK",
				(rsm_es_in2->fan.modfail) ? "FAIL" : "OK");
			(void) fprintf(stdout, " Over Temperature : %s"
					"     Over Temperature : %s"
					"     Over Temperature : %s\n",
				(rsm_es_in->ovta.modfail) ? "YES" : "NO",
				(rsm_es_in1->ovta.modfail) ? "YES" : "NO",
				(rsm_es_in2->ovta.modfail) ? "YES" : "NO");
			break;
		default:
			disp_envsen_data(3, i);
			nenvsen_devs -= 3;
			(void) fprintf(stdout, "\n\n");
			disp_envsen_data(nenvsen_devs, i + 3);
			break;
	}
}

/*
 * So let's describe how this puppy works:
 *
 * Since LEDs are per-disk we need a disk node to identify what we want to
 * tweak.  Since the LED manipulation is done by manipulating the single per-
 * tobasco environmental card, we need to find the matching ses device to the
 * ssd that the user specified.  To do that we will look in /devices and get
 * the device node for disk the in the format `ssd@<port>,<tgt>:<partition>'
 * and extract the port number.  We need to match this to the environmental
 * card who's node has the format `ses@<port>,<tgt>:<Idunno>'.  If the user
 * specified a port number, it must match the port number the target device is
 * hanging from or the user is confused.
 *
 */
void
led(char **argv, int options, int argc)
{
	char    *path_phys, *s, *s1;
	int	port = -1, port1, target, ndevices;
	struct	rsm_es_out rsm_es_out;

	if (argv[1])
		port = atoi(argv[1]);

	if ((path_phys =
	    get_physical_name(argv[0])) == NULL) {
	    (void) fprintf(stderr,
		"%s: Invalid path name (%s)\n", whoami,  argv[0]);
	    exit(-1);
	}

	if (strstr(path_phys, ":ctlr")) {
		/*
		 * path is of a controller, not a device
		 */
		(void) fprintf(stderr,
			"%s: %s is a controller, not a disk.\n"
			"It has no user controllable LEDs\n",
			whoami, path_phys);
		exit(-1);
	}

	s = strrchr(path_phys, '/');
	if (s1 = strrchr(s, '@')) {
		if (strstr(s, "ssd@")) {
			port1 = -1;
			target = -1;
			sscanf(s1, "@%d,%d", &port1, &target);
		} else {
			port1 = -1;
			target = -1;
			sscanf(s1, "@%x,%x", &target, &port1);
			port1 = port;
		}
	}
	if ((port != -1) && (port != port1)) {
		(void) fprintf(stdout,
			"%s: specified port does not match the device!\n",
			whoami);
		exit(-1);
	}
	/*
	 * truncate the path to the controller name
	 */
	*(s +1) = '\0';

	P_DPRINTF("port=%d, target=%d\n", port1, target);
	ndevices = get_envsen_data(path_phys, port1, 0);
	if (ndevices != 1) {
		(void) fprintf(stderr,
			"%s: Invalid pathname %s port %d has %d devices\n",
			whoami, path_phys, port1, ndevices);
		exit(-1);
	}
	if (options & OPTION_E) {
		fill_rsm_es_out(&rsm_es_out);
		rsm_es_out.devstat[target%(MAXDRIVES+1)].dl = 1;
		write_envsen_data(&rsm_es_out, path_phys, port1,
			envsen_data[0].tid);
		ndevices = get_envsen_data(path_phys, port1, 0);
	} else if (options & OPTION_D) {
		fill_rsm_es_out(&rsm_es_out);
		rsm_es_out.devstat[target%(MAXDRIVES+1)].dl = 0;
		write_envsen_data(&rsm_es_out, path_phys, port1,
			envsen_data[0].tid);
		ndevices = get_envsen_data(path_phys, port1, 0);
	}
	if (envsen_data[0].rsm_es_in->devstat[target%(MAXDRIVES+1)].dl) {
		(void) fprintf(stdout, "LED state is ON\n");
	} else {
		(void) fprintf(stdout, "LED state is OFF\n");
	}
}

void
fill_rsm_es_out(struct rsm_es_out *rsm_es_out)
{
	(void) bzero(rsm_es_out, sizeof (struct rsm_es_out));
	rsm_es_out->page_code = 4;
	rsm_es_out->page_len = SCSI_RSM_ES_OUT_PAGE_LEN;
	rsm_es_out->encl_gd_len =  envsen_data[0].rsm_es_in->encl_gd_len;
	rsm_es_out->num_unit_types =  SCSI_RSM_NUM_UNIT_TYPES;
	rsm_es_out->alenb =  envsen_data[0].rsm_es_in->alenb;
	rsm_es_out->altime =  envsen_data[0].rsm_es_in->altime;
	rsm_es_out->max_drvs =  envsen_data[0].rsm_es_in->max_drvs;
	rsm_es_out->drv_dscp_len =  envsen_data[0].rsm_es_in->drv_dscp_len;
	(void) bcopy(&rsm_es_out->devstat[0],
			&envsen_data[0].rsm_es_in->devstat[0],
			sizeof (struct rsm_es_d_stat) *ES_RSM_MAX_DRIVES);
}
static void
write_envsen_data(struct rsm_es_out *rsm_es_out, char *path, int port,
		int target)
{
	int fd, err;
	extern int errno;
	char	dev_path[MAXNAMELEN];

	if (port != -1) {
		/* Generate name for SSA device */
		(void) sprintf(dev_path, "%s/%s@%d,%d:0",
			path, ENVSEN_DEV, port, target);
	} else {
		/* Generate name for ISP device */
		(void) sprintf(dev_path, "%s/%s@%x,0:0", path, ENVSEN_DEV,
			target);
	}

	P_DPRINTF("write_envsen_data: Opening envsen device %s "
		"port=%x target=%x\n", dev_path, port, target);
	if ((fd = open(dev_path, O_RDONLY)) < 0) {
		(void) fprintf(stderr, "Could not open %s\n", dev_path);
		return;
	}

	if (getenv("SSA_P_DEBUG") != NULL)
		printbuf((unsigned char *)rsm_es_out,
			SCSI_RSM_ES_OUT_PAGE_SIZE);

	err  = scsi_put_envsen_data(fd, (char *)rsm_es_out,
				SCSI_RSM_ES_OUT_PAGE_SIZE);
	(void) close(fd);
	if (err != 0) {
		(void) fprintf(stdout, "%s: set diag failed status: %d\n\n",
			whoami, errno);
	} else if (getenv("SSA_P_DEBUG") != NULL) {
		printbuf((unsigned char *)rsm_es_out, SCSI_RSM_ES_IN_PAGE_SIZE);
		(void) fprintf(stdout, "%s: set diag successful\n\n",
			whoami);
	}
}

int
get_port(char *path_phys)
{
	char *s, *s1;
	int port = -1, target;

	/*
	 * Parse a physical path to a pluto device and find port (tray) no.
	 */

	s = strrchr(path_phys, '/');
	if (s && (s1 = strrchr(s, '@'))	&&
		(strstr(s, "ssd@") || strstr(s, "ses@"))) {
		port = -1;
		target = -1;
		sscanf(s1, "@%d,%d", &port, &target);
	} else {
		/*
		 * This is either a pln or a sd and has no specific port.
		 */
		port = -1;
	}
	return (port);
}

int
get_envsen_global_data(char **argv, int argc, char **path, int *port)
{
	char 	*path_phys, *char_ptr;
	int	path_index = 0, ndevs = 0, port1;

	if (argc > 1) {
		*port = atoi(argv[1]);
	} else {
		*port = -1;
	}

	if ((path_phys =
	    get_physical_name(argv[path_index])) == NULL) {
	    (void) fprintf(stderr,
		"%s: Invalid path name (%s)\n",
			whoami, argv[path_index]);
	    exit(-1);
	}

	if ((char_ptr = strstr(path_phys, ":ctlr")) == NULL) {
		if (strstr(path_phys, "ssd@")) {
			/*
			 * path could be the pathname of the envsen device.
			 */
			port1 = get_port(path_phys);
			if (*port != -1 && *port != port1) {
				/*
				 * Specified device on wrong port --
				 * user error
				 */
				(void) fprintf(stderr, "That device is not in "
					"tray %x\n", *port);
				return (ndevs);
			}
			*port = port1;
			char_ptr = strrchr(path_phys, '/');
			*char_ptr = '\0';
			ndevs = get_envsen_data(path_phys, *port, 0);
			*path = path_phys;
			P_DPRINTF("path_phys=%s\n", path_phys);
			return (ndevs);
		} else {
			/*
			 * path could be the pathname of the envsen device.
			 */
			char_ptr = strrchr(path_phys, '/');
			*char_ptr = '\0';
			ndevs = get_envsen_data(path_phys, *port, 0);
			*path = path_phys;
			P_DPRINTF("path_phys=%s\n", path_phys);
			return (ndevs);
		}
	}

	P_DPRINTF("ctrlr path_phys=%s, port=%d\n", path_phys, *port);
	*path = path_phys;
	*char_ptr = '\0';
	return (get_envsen_data(path_phys, *port, 0));
}
void
alarm_enable(char **argv, int options, int argc)
{
	int ndevices, port;
	char *path_phys;
	struct rsm_es_out rsm_es_out;

	ndevices = get_envsen_global_data(argv, argc, &path_phys, &port);

	if (ndevices == 0) {
		(void) fprintf(stderr, "%s: Invalid pathname\n",
			whoami);
		exit(-1);
	}
	if (ndevices > 1) {
		(void) fprintf(stderr,
			"%s: You need to specify a tray number\n",
			whoami);
		exit(-1);
	}
	if (options & OPTION_E) {
		fill_rsm_es_out(&rsm_es_out);
		rsm_es_out.alenb = 1;
		write_envsen_data(&rsm_es_out, path_phys, port,
			envsen_data[0].tid);
		get_envsen_data(path_phys, port, 0);
	} else if (options & OPTION_D) {
		fill_rsm_es_out(&rsm_es_out);
		rsm_es_out.alenb = 0;
		write_envsen_data(&rsm_es_out, path_phys, port,
			envsen_data[0].tid);
		get_envsen_data(path_phys, port, 0);
	}
	if (envsen_data[0].rsm_es_in->alenb) {
		(void) fprintf(stdout, "Alarm is Enabled!\n");
	} else {
		(void) fprintf(stdout, "Alarm is Disabled!\n");
	}
}
void
alarm_set(char **argv, int argc)
{
	int ndevices, port, secs = -1;
	char *path_phys;
	struct rsm_es_out rsm_es_out;

	ndevices = get_envsen_global_data(argv, argc, &path_phys, &port);

	if (ndevices != 1) {
		(void) fprintf(stderr, "%s: Invalid pathname\n",
			whoami);
		exit(-1);
	}

	if (argc > 2) {
		/* printf("argv[2] = %s\n", argv[2]); */
		secs = atoi(argv[2]);
	}

	/* printf("argc=%d, secs = %d\n", argc, secs); */
	if (secs == -1) {
		(void) fprintf(stdout, "Current alarm setting = %d seconds\n",
			envsen_data[0].rsm_es_in->altime);
	} else {
		fill_rsm_es_out(&rsm_es_out);
		rsm_es_out.altime = secs;
		write_envsen_data(&rsm_es_out, path_phys, port,
				envsen_data[0].tid);
	}
}
void
power_off(char **argv, int argc)
{
	int ndevices, port;
	char *path_phys;
	struct rsm_es_out rsm_es_out;

	ndevices = get_envsen_global_data(argv, argc, &path_phys, &port);

	if (ndevices != 1) {
		(void) fprintf(stderr, "%s: Invalid pathname\n",
			whoami);
		exit(-1);
	}
	if (strstr(path_phys, "SUNW,pln") == NULL) {
		(void) fprintf(stderr, "%s: power_off of directly "
			"attached RSMs is not supported.\n", whoami);
		exit(-1);
	}

	fill_rsm_es_out(&rsm_es_out);
	rsm_es_out.rpoff = 1;
	write_envsen_data(&rsm_es_out, path_phys, port, envsen_data[0].tid);
}

struct rsm_es_in *
find_card(int port)
{
	int i;

	for (i = 0; i < MAX_CARDS; i++) {
		if (port == envsen_data[i].port) {
			P_DPRINTF("find_card: found card %d\n", i);
			return (envsen_data[i].rsm_es_in);
		}
	}

	return ((struct rsm_es_in *)NULL);
}
