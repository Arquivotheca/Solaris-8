/*
 * Copyright 1997-1999 Sun Microsystems, Inc. All rights reserved.
 */
#pragma ident	"@(#)envsen.c	1.10	99/07/30 SMI"

/*LINTLIBRARY*/


/*
 *	Administration program for the photon
 */

/*
 * I18N message number ranges
 *  This file: 4000 - 4499
 *  Shared common messages: 1 - 1999
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
#include	<strings.h> /* For bzero/bcopy */

/*
 * For i18n
 */
#include 	<stgcom.h>

#include	<g_state.h>

#include	"ssadef.h"
#include	"scsi.h"
#include	"state.h"
#include	<sys/scsi/targets/sesio.h>
#include	"luxadm.h"

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

/*
extern	char	*bzero();
extern	char	*bcopy();
*/


void
cli_display_envsen_data(char **argv, int argc)
{
	int	nenvsen_devs = 0, port;
	char	*path_phys;


	nenvsen_devs = get_envsen_global_data(argv, argc, &path_phys, &port);

	if (nenvsen_devs) {
		(void) fprintf(stdout, MSGSTR(4000,
			"	       ENCLOSURE STATUS\n"));
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
		(void) fprintf(stdout, MSGSTR(10,
				" Error: Unable to allocate memory."));
		(void) fprintf(stdout, "\n");
		return (NULL);
	}
	(void) sprintf(dev_path, "%s/", path);

	P_DPRINTF("dev_path=%s\n", dev_path);
	if ((dir = opendir(dev_path)) == NULL) {
		(void) fprintf(stdout, MSGSTR(4001,
			"%s: could not opendir\n"), dev_path);
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
				(void) fprintf(stderr, MSGSTR(10,
					" Error: Unable to allocate memory."));
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
		(void) fprintf(stderr, MSGSTR(4002,
			"Could not open %s\n"), path);
		return (NULL);
	}

	buf = (struct rsm_es_in *)malloc(SCSI_RSM_ES_IN_PAGE_SIZE);
	if (buf == (struct rsm_es_in *)NULL) {
		(void) fprintf(stderr, MSGSTR(4003,
			"read_envsen_data: Could not alloc memory!\n"));
		return (NULL);
	}

	err  = g_scsi_rec_diag_cmd(fd, (uchar_t *)buf,
		SCSI_RSM_ES_IN_PAGE_SIZE, 0x4);
	(void) close(fd);
	if (err != 0) {
		(void) fprintf(stdout, MSGSTR(4004,
				"%s: recv diag failed status: %d\n\n"),
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

	(void) printf(MSGSTR(4005, "buffer = \n"));
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

			(void) fprintf(stdout, "      ");
			if (envsen_data[i].port == -1) {
				(void) fprintf(stdout, MSGSTR(4006,
					"CARD on ISP"));
			} else {
				(void) fprintf(stdout, MSGSTR(36,
					"CARD in tray %d"),
					envsen_data[i].port);
			}
			(void) fprintf(stdout, "\n\n");
			rsm_es_in = envsen_data[i].rsm_es_in;

			(void) fprintf(stdout, " ");
			if (rsm_es_in->abs != NULL) {
				(void) fprintf(stdout, MSGSTR(13,
					"Abnormal Condition: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(14,
					"Abnormal Condition: NO"));
			}
			(void) fprintf(stdout, "\n");

			(void) fprintf(stdout, " ");
			if (rsm_es_in->chk != NULL) {
				(void) fprintf(stdout, MSGSTR(21,
					"Immediate Attention: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(22,
					"Immediate Attention: NO"));
			}
			(void) fprintf(stdout, "\n");

			(void) fprintf(stdout, " ");
			if (rsm_es_in->alsen != NULL) {
				(void) fprintf(stdout, MSGSTR(12,
					"Alarm Status : *Sound*"));
			} else {
				(void) fprintf(stdout, MSGSTR(11,
					"Alarm Status : OK"));
			}
			(void) fprintf(stdout, "\n");

			(void) fprintf(stdout, " ");
			if (rsm_es_in->alenb != NULL) {
				(void) fprintf(stdout, MSGSTR(23,
					"Alarm Enabled: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(24,
					"Alarm Enabled: NO"));
			}
			(void) fprintf(stdout, "\n ");

			(void) fprintf(stdout, MSGSTR(38,
				"Alarm Setting : %d"), rsm_es_in->altime);
			(void) fprintf(stdout, "\n");

			(void) fprintf(stdout, " ");
			if (rsm_es_in->idsen != NULL) {
				(void) fprintf(stdout, MSGSTR(6,
					"Id Sense Bit: ON"));
			} else {
				(void) fprintf(stdout, MSGSTR(7,
					"Id Sense Bit: OFF"));
			}
			(void) fprintf(stdout, "\n");

			(void) fprintf(stdout, " ");
			if (rsm_es_in->dsdly != NULL) {
				(void) fprintf(stdout, MSGSTR(15,
					"Spin Delay: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(16,
					"Spin Delay: NO"));
			}
			(void) fprintf(stdout, "\n");

			for (j = 1; j <= rsm_es_in->max_pwms; j++) {
				(void) fprintf(stdout, " ");
				if (rsm_es_in->pwm[j-1].modfail != NULL) {
					(void) fprintf(stdout, MSGSTR(2,
					"Power Module %d: FAIL"), j);
				} else {
					(void) fprintf(stdout, MSGSTR(1,
					"Power Module %d: OK"), j);
				}
				(void) fprintf(stdout, "\n");
			}


			(void) fprintf(stdout, " ");
			if (rsm_es_in->fan.modfail != NULL) {
				(void) fprintf(stdout, MSGSTR(8,
					"Fan Module : FAIL"));
			} else {
				(void) fprintf(stdout, MSGSTR(9,
					"Fan Module : OK"));
			}
			(void) fprintf(stdout, "\n");

			(void) fprintf(stdout, " ");
			if (rsm_es_in->ovta.modfail != NULL) {
				(void) fprintf(stdout, MSGSTR(18,
					"Over Temperature : YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(19,
					"Over Temperature : NO"));
			}
			(void) fprintf(stdout, "\n");

			break;
		case 2:

			(void) fprintf(stdout, "      ");
			(void) fprintf(stdout, MSGSTR(36,
				"CARD in tray %d"),
				envsen_data[i].port);
			(void) fprintf(stdout, "	   ");
			(void) fprintf(stdout, MSGSTR(36,
				"CARD in tray %d"),
				envsen_data[i+1].port);
			(void) fprintf(stdout, "\n\n");
			rsm_es_in = envsen_data[i].rsm_es_in;
			rsm_es_in1 = envsen_data[i+1].rsm_es_in;

			(void) fprintf(stdout, " ");
			if (rsm_es_in->abs != NULL) {
				(void) fprintf(stdout, MSGSTR(13,
					"Abnormal Condition: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(14,
					"Abnormal Condition: NO"));
			}

			(void) fprintf(stdout, "     ");
			if (rsm_es_in1->abs != NULL) {
				(void) fprintf(stdout, MSGSTR(13,
					"Abnormal Condition: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(14,
					"Abnormal Condition: NO"));
			}
			(void) fprintf(stdout, "\n");

			(void) fprintf(stdout, " ");
			if (rsm_es_in->chk != NULL) {
				(void) fprintf(stdout, MSGSTR(21,
					"Immediate Attention: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(22,
					"Immediate Attention: NO"));
			}

			(void) fprintf(stdout, "   ");
			if (rsm_es_in1->chk != NULL) {
				(void) fprintf(stdout, MSGSTR(21,
					"Immediate Attention: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(22,
					"Immediate Attention: NO"));
			}
			(void) fprintf(stdout, "\n");


			(void) fprintf(stdout, " ");
			if (rsm_es_in->alsen != NULL) {
				(void) fprintf(stdout, MSGSTR(12,
					"Alarm Status : *Sound*"));
			} else {
				(void) fprintf(stdout, MSGSTR(11,
					"Alarm Status : OK"));
			}

			(void) fprintf(stdout, "     ");
			if (rsm_es_in->alsen != NULL) {
				(void) fprintf(stdout, MSGSTR(12,
					"Alarm Status : *Sound*"));
			} else {
				(void) fprintf(stdout, MSGSTR(11,
					"Alarm Status : OK"));
			}
			(void) fprintf(stdout, "\n");


			(void) fprintf(stdout, " ");
			if (rsm_es_in->alenb != NULL) {
				(void) fprintf(stdout, MSGSTR(23,
					"Alarm Enabled: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(24,
					"Alarm Enabled: NO"));
			}

			(void) fprintf(stdout, "	 ");
			if (rsm_es_in1->alenb != NULL) {
				(void) fprintf(stdout, MSGSTR(23,
					"Alarm Enabled: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(24,
					"Alarm Enabled: NO"));
			}
			(void) fprintf(stdout, "\n ");


			(void) fprintf(stdout, MSGSTR(38,
				"Alarm Setting : %d"),
				rsm_es_in->altime);
			(void) fprintf(stdout, "	  ");
			(void) fprintf(stdout, MSGSTR(38,
				"Alarm Setting : %d"),
				rsm_es_in1->altime);
			(void) fprintf(stdout, "\n ");

			if (rsm_es_in->idsen != NULL) {
				(void) fprintf(stdout, MSGSTR(6,
					"Id Sense Bit: ON"));
			} else {
				(void) fprintf(stdout, MSGSTR(7,
					"Id Sense Bit: OFF"));
			}

			(void) fprintf(stdout, "	  ");
			if (rsm_es_in1->idsen != NULL) {
				(void) fprintf(stdout, MSGSTR(6,
					"Id Sense Bit: ON"));
			} else {
				(void) fprintf(stdout, MSGSTR(7,
					"Id Sense Bit: OFF"));
			}
			(void) fprintf(stdout, "\n");


			(void) fprintf(stdout, " ");
			if (rsm_es_in->dsdly != NULL) {
				(void) fprintf(stdout, MSGSTR(15,
					"Spin Delay: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(16,
					"Spin Delay: NO"));
			}

			(void) fprintf(stdout, "	    ");
			if (rsm_es_in1->dsdly != NULL) {
				(void) fprintf(stdout, MSGSTR(15,
					"Spin Delay: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(16,
					"Spin Delay: NO"));
			}
			(void) fprintf(stdout, "\n");


			for (j = 1; j <= rsm_es_in->max_pwms; j++) {
			    if (j <= rsm_es_in1->max_pwms) {

				(void) fprintf(stdout, " ");
				if (rsm_es_in->pwm[j-1].modfail != NULL) {
					(void) fprintf(stdout, MSGSTR(2,
						"Power Module %d: FAIL"), j);
					} else {
						(void) fprintf(stdout,
						MSGSTR(1,
						"Power Module %d: OK"), j);
					}

					(void) fprintf(stdout, "	 ");
					if (rsm_es_in1->pwm[j-1].modfail
						!= NULL) {
						(void) fprintf(stdout,
						MSGSTR(2,
						"Power Module %d: FAIL"), j);
					} else {
						(void) fprintf(stdout,
						MSGSTR(1,
						"Power Module %d: OK"), j);
					}
					(void) fprintf(stdout, "\n");

			    } else {

				(void) fprintf(stdout, " ");
				if (rsm_es_in->pwm[j-1].modfail != NULL) {
					(void) fprintf(stdout, MSGSTR(2,
						"Power Module %d: FAIL"), j);
				} else {
					(void) fprintf(stdout, MSGSTR(1,
						"Power Module %d: OK"), j);
				}
				(void) fprintf(stdout, "\n");

			    }
			}

			for (; j <= rsm_es_in1->max_pwms; j++) {

				(void) fprintf(stdout,
					"		           ");
				if (rsm_es_in1->pwm[j-1].modfail != NULL) {
					(void) fprintf(stdout, MSGSTR(2,
				"Power Module %d: FAIL"), j);

				} else {
					(void) fprintf(stdout, MSGSTR(1,
				"Power Module %d: OK"), j);

				}
				(void) fprintf(stdout, "\n");

			}

			(void) fprintf(stdout, " ");
			if (rsm_es_in->fan.modfail != NULL) {
				(void) fprintf(stdout, MSGSTR(8,
					"Fan Module : FAIL"));
			} else {
				(void) fprintf(stdout, MSGSTR(9,
					"Fan Module : OK"));
			}
			(void) fprintf(stdout, "	    ");
			if (rsm_es_in1->fan.modfail != NULL) {
				(void) fprintf(stdout, MSGSTR(8,
					"Fan Module : FAIL"));
			} else {
				(void) fprintf(stdout, MSGSTR(9,
					"Fan Module : OK"));
			}
			(void) fprintf(stdout, "\n");


			(void) fprintf(stdout, " ");
			if (rsm_es_in->ovta.modfail != NULL) {
				(void) fprintf(stdout, MSGSTR(18,
					"Over Temperature : YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(19,
					"Over Temperature : NO"));
			}
			(void) fprintf(stdout, "     ");
			if (rsm_es_in1->ovta.modfail != NULL) {
				(void) fprintf(stdout, MSGSTR(18,
					"Over Temperature : YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(19,
					"Over Temperature : NO"));
			}
			(void) fprintf(stdout, "\n");

			break;
		case 3:
			(void) fprintf(stdout, "      ");
			(void) fprintf(stdout, MSGSTR(36,
				"CARD in tray %d"),
				envsen_data[i].port);
			(void) fprintf(stdout, "	 ");
			(void) fprintf(stdout, MSGSTR(36,
				"CARD in tray %d"),
				envsen_data[i+1].port);
			(void) fprintf(stdout, "	  ");
			(void) fprintf(stdout, MSGSTR(36,
				"CARD in tray %d"),
				envsen_data[i+2].port);
			(void) fprintf(stdout, "\n\n");
			rsm_es_in = envsen_data[i].rsm_es_in;
			rsm_es_in1 = envsen_data[i+1].rsm_es_in;
			rsm_es_in2 = envsen_data[i+2].rsm_es_in;

			(void) fprintf(stdout, " ");
			if (rsm_es_in->abs != NULL) {
				(void) fprintf(stdout, MSGSTR(13,
					"Abnormal Condition: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(14,
					"Abnormal Condition: NO"));
			}
			(void) fprintf(stdout, "    ");
			if (rsm_es_in1->abs != NULL) {
				(void) fprintf(stdout, MSGSTR(13,
					"Abnormal Condition: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(14,
					"Abnormal Condition: NO"));
			}
			(void) fprintf(stdout, "    ");
			if (rsm_es_in2->abs != NULL) {
				(void) fprintf(stdout, MSGSTR(13,
					"Abnormal Condition: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(14,
					"Abnormal Condition: NO"));
			}
			(void) fprintf(stdout, "\n");

			(void) fprintf(stdout, " ");
			if (rsm_es_in->chk != NULL) {
				(void) fprintf(stdout, MSGSTR(21,
					"Immediate Attention: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(22,
					"Immediate Attention: NO"));
			}
			(void) fprintf(stdout, "   ");
			if (rsm_es_in1->chk != NULL) {
				(void) fprintf(stdout, MSGSTR(21,
					"Immediate Attention: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(22,
					"Immediate Attention: NO"));
			}
			(void) fprintf(stdout, "   ");
			if (rsm_es_in2->chk != NULL) {
				(void) fprintf(stdout, MSGSTR(21,
					"Immediate Attention: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(22,
					"Immediate Attention: NO"));
			}
			(void) fprintf(stdout, "\n");

			(void) fprintf(stdout, " ");
			if (rsm_es_in->alsen != NULL) {
				(void) fprintf(stdout, MSGSTR(12,
					"Alarm Status : *Sound*"));
			} else {
				(void) fprintf(stdout, MSGSTR(11,
					"Alarm Status : OK"));
			}
			(void) fprintf(stdout, "	 ");
			if (rsm_es_in1->alsen != NULL) {
				(void) fprintf(stdout, MSGSTR(12,
					"Alarm Status : *Sound*"));
			} else {
				(void) fprintf(stdout, MSGSTR(11,
					"Alarm Status : OK"));
			}
			(void) fprintf(stdout, "	 ");
			if (rsm_es_in2->alsen != NULL) {
				(void) fprintf(stdout, MSGSTR(12,
					"Alarm Status : *Sound*"));
			} else {
				(void) fprintf(stdout, MSGSTR(11,
					"Alarm Status : OK"));
			}
			(void) fprintf(stdout, "\n");

			(void) fprintf(stdout, " ");
			if (rsm_es_in->alenb != NULL) {
				(void) fprintf(stdout, MSGSTR(23,
					"Alarm Enabled: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(24,
					"Alarm Enabled: NO"));
			}
			(void) fprintf(stdout, "	 ");
			if (rsm_es_in1->alenb != NULL) {
				(void) fprintf(stdout, MSGSTR(23,
					"Alarm Enabled: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(24,
					"Alarm Enabled: NO"));
			}
			(void) fprintf(stdout, "	 ");
			if (rsm_es_in2->alenb != NULL) {
				(void) fprintf(stdout, MSGSTR(23,
					"Alarm Enabled: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(24,
					"Alarm Enabled: NO"));
			}

			(void) fprintf(stdout, "\n ");

			(void) fprintf(stdout, MSGSTR(38,
				"Alarm Setting : %d"),
				rsm_es_in->altime);
			(void) fprintf(stdout, "	 ");
			(void) fprintf(stdout, MSGSTR(38,
				"Alarm Setting : %d"),
				rsm_es_in1->altime);
			(void) fprintf(stdout, "	 ");
			(void) fprintf(stdout, MSGSTR(38,
				"Alarm Setting : %d"),
				rsm_es_in2->altime);

			(void) fprintf(stdout, "\n  ");

			if (rsm_es_in->idsen != NULL) {
				(void) fprintf(stdout, MSGSTR(6,
					"Id Sense Bit: ON"));
			} else {
				(void) fprintf(stdout, MSGSTR(7,
					"Id Sense Bit: OFF"));
			}
			(void) fprintf(stdout, "	 ");
			if (rsm_es_in1->idsen != NULL) {
				(void) fprintf(stdout, MSGSTR(6,
					"Id Sense Bit: ON"));
			} else {
				(void) fprintf(stdout, MSGSTR(7,
					"Id Sense Bit: OFF"));
			}
			(void) fprintf(stdout, "	 ");
			if (rsm_es_in2->idsen != NULL) {
				(void) fprintf(stdout, MSGSTR(6,
					"Id Sense Bit: ON"));
			} else {
				(void) fprintf(stdout, MSGSTR(7,
					"Id Sense Bit: OFF"));
			}
			(void) fprintf(stdout, "\n");

			(void) fprintf(stdout, " ");
			if (rsm_es_in->dsdly != NULL) {
				(void) fprintf(stdout, MSGSTR(15,
					"Spin Delay: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(16,
					"Spin Delay: NO"));
			}
			(void) fprintf(stdout, "	    ");
			if (rsm_es_in1->dsdly != NULL) {
				(void) fprintf(stdout, MSGSTR(15,
					"Spin Delay: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(16,
					"Spin Delay: NO"));
			}
			(void) fprintf(stdout, "	    ");
			if (rsm_es_in2->dsdly != NULL) {
				(void) fprintf(stdout, MSGSTR(15,
					"Spin Delay: YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(16,
					"Spin Delay: NO"));
			}
			(void) fprintf(stdout, "\n");

			for (j = 1; j <= rsm_es_in->max_pwms; j++) {
				if (j <= rsm_es_in1->max_pwms) {
					if (j <= rsm_es_in2->max_pwms) {

					    (void) fprintf(stdout, " ");
					    if (rsm_es_in->pwm[j-1].modfail
						!= NULL) {

						(void) fprintf(stdout,
						MSGSTR(2,
						"Power Module %d: FAIL"), j);
					    } else {
						(void) fprintf(stdout,
						MSGSTR(1,
						"Power Module %d: OK"), j);
					    }

					    (void) fprintf(stdout, "	");
					    if (rsm_es_in1->pwm[j-1].modfail
						!= NULL) {

						(void) fprintf(stdout,
						MSGSTR(2,
						"Power Module %d: FAIL"), j);
					    } else {
						(void) fprintf(stdout,
						MSGSTR(1,
						"Power Module %d: OK"),
						j);
					    }

					    (void) fprintf(stdout, "	");
					    if (rsm_es_in2->pwm[j-1].modfail
						!= NULL) {
						(void) fprintf(stdout,
						MSGSTR(2,
						"Power Module %d: FAIL"), j);
					    } else {
						(void) fprintf(stdout,
						MSGSTR(1,
						"Power Module %d: OK"), j);
					    }
					    (void) fprintf(stdout, "\n");

					} else {

					    (void) fprintf(stdout, " ");
					    if (rsm_es_in->pwm[j-1].modfail
						!= NULL) {
						(void) fprintf(stdout,
						MSGSTR(2,
						"Power Module %d: FAIL"), j);
					    } else {
						(void) fprintf(stdout,
						MSGSTR(1,
						"Power Module %d: OK"), j);
					    }

					    (void) fprintf(stdout, "	");
					    if (rsm_es_in1->pwm[j-1].modfail
						!= NULL) {
						(void) fprintf(stdout,
						MSGSTR(2,
						"Power Module %d: FAIL"), j);

					    } else {
						(void) fprintf(stdout,
						MSGSTR(1,
						"Power Module %d: OK"), j);
					    }
					    (void) fprintf(stdout, "\n");

					}
				} else {
					if (j <= rsm_es_in2->max_pwms) {

					    (void) fprintf(stdout, " ");
					    if (rsm_es_in->pwm[j-1].modfail
						!= NULL) {

						(void) fprintf(stdout,
						MSGSTR(2,
						"Power Module %d: FAIL"), j);
					    } else {
						(void) fprintf(stdout,
						MSGSTR(1,
						"Power Module %d: OK"), j);
					    }

					    (void) fprintf(stdout,
					"			          ");
					    if (rsm_es_in2->pwm[j-1].modfail
						!= NULL) {

						(void) fprintf(stdout,
						MSGSTR(2,
						"Power Module %d: FAIL"), j);
					    } else {
						(void) fprintf(stdout,
						MSGSTR(1,
						"Power Module %d: OK"), j);
					    }
					    (void) fprintf(stdout, "\n");

					} else {

					    (void) fprintf(stdout, " ");
					    if (rsm_es_in->pwm[j-1].modfail
						!= NULL) {

						(void) fprintf(stdout,
						MSGSTR(2,
						"Power Module %d: FAIL"), j);
					    } else {
						(void) fprintf(stdout,
						MSGSTR(1,
						"Power Module %d: OK"), j);
					    }
					}
				}
			}
			for (; j <= rsm_es_in1->max_pwms; j++) {
				if (j <= rsm_es_in2->max_pwms) {

				    (void) fprintf(stdout,
					"			   ");
				    if (rsm_es_in1->pwm[j-1].modfail != NULL) {
					(void) fprintf(stdout, MSGSTR(2,
						"Power Module %d: FAIL"), j);
				    } else {
					(void) fprintf(stdout, MSGSTR(1,
						"Power Module %d: OK"), j);
				    }

				    (void) fprintf(stdout, "	");
				    if (rsm_es_in2->pwm[j-1].modfail != NULL) {
					(void) fprintf(stdout, MSGSTR(2,
						"Power Module %d: FAIL"), j);
				    } else {
					(void) fprintf(stdout, MSGSTR(1,
						"Power Module %d: OK"), j);
				    }
				    (void) fprintf(stdout, "\n");

				} else {

				    (void) fprintf(stdout,
					"		           ");
				    if (rsm_es_in1->pwm[j-1].modfail != NULL) {
					(void) fprintf(stdout, MSGSTR(2,
						"Power Module %d: FAIL"), j);
				    } else {
					(void) fprintf(stdout, MSGSTR(1,
						"Power Module %d: OK"), j);
				    }
				    (void) fprintf(stdout, "\n");

				}
			}
			for (; j <= rsm_es_in2->max_pwms; j++) {

				(void) fprintf(stdout,
			"			      		       ");
				if (rsm_es_in2->pwm[j-1].modfail != NULL) {
					(void) fprintf(stdout, MSGSTR(2,
						"Power Module %d: FAIL"), j);
				} else {
					(void) fprintf(stdout, MSGSTR(1,
						"Power Module %d: OK"), j);
				}
				(void) fprintf(stdout, "\n");

			}

			(void) fprintf(stdout, " ");
			if (rsm_es_in->fan.modfail != NULL) {
				(void) fprintf(stdout, MSGSTR(8,
					"Fan Module : FAIL"));
			} else {
				(void) fprintf(stdout, MSGSTR(9,
					"Fan Module : OK"));
			}

			(void) fprintf(stdout, "	   ");
			if (rsm_es_in1->fan.modfail != NULL) {
				(void) fprintf(stdout, MSGSTR(8,
					"Fan Module : FAIL"));
			} else {
				(void) fprintf(stdout, MSGSTR(9,
					"Fan Module : OK"));
			}

			(void) fprintf(stdout, "	   ");
			if (rsm_es_in2->fan.modfail != NULL) {
				(void) fprintf(stdout, MSGSTR(8,
					"Fan Module : FAIL"));
			} else {
				(void) fprintf(stdout, MSGSTR(9,
					"Fan Module : OK"));
			}
			(void) fprintf(stdout, "\n");

			(void) fprintf(stdout, " ");
			if (rsm_es_in->ovta.modfail != NULL) {
				(void) fprintf(stdout, MSGSTR(18,
					"Over Temperature : YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(19,
					"Over Temperature : NO"));
			}

			(void) fprintf(stdout, "     ");
			if (rsm_es_in1->ovta.modfail != NULL) {
				(void) fprintf(stdout, MSGSTR(18,
					"Over Temperature : YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(19,
					"Over Temperature : NO"));
			}

			(void) fprintf(stdout, "     ");
			if (rsm_es_in2->ovta.modfail != NULL) {
				(void) fprintf(stdout, MSGSTR(18,
					"Over Temperature : YES"));
			} else {
				(void) fprintf(stdout, MSGSTR(19,
					"Over Temperature : NO"));
			}
			(void) fprintf(stdout, "\n");

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
	    g_get_physical_name(argv[0])) == NULL) {

	    (void) fprintf(stderr, "%s: ", whoami);
	    (void) fprintf(stderr,
		MSGSTR(112, "Error: Invalid pathname (%s)"),
		argv[0]);
	    (void) fprintf(stderr, "\n");
	    exit(-1);
	}

	if (strstr(path_phys, ":ctlr")) {
		/*
		 * path is of a controller, not a device
		 */
		(void) fprintf(stderr, MSGSTR(4007,
			"%s: %s is a controller, not a disk.\n"
			"It has no user controllable LEDs\n"),
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
		(void) fprintf(stdout, MSGSTR(4008,
			"%s: specified port does not match the device!\n"),
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
		(void) fprintf(stderr, MSGSTR(4009,
			"%s: Invalid pathname %s port %d has %d devices\n"),
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
		(void) fprintf(stdout, MSGSTR(4010, "LED state is ON\n"));
	} else {
		(void) fprintf(stdout, MSGSTR(4011, "LED state is OFF\n"));
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
		(void) fprintf(stderr, MSGSTR(4012,
			"Could not open %s\n"), dev_path);
		return;
	}

	if (getenv("SSA_P_DEBUG") != NULL)
		printbuf((unsigned char *)rsm_es_out,
			SCSI_RSM_ES_OUT_PAGE_SIZE);

	err  = g_scsi_send_diag_cmd(fd, (uchar_t *)rsm_es_out,
				SCSI_RSM_ES_OUT_PAGE_SIZE);
	(void) close(fd);
	if (err != 0) {
		(void) fprintf(stdout, MSGSTR(4013,
			"%s: set diag failed status: %d\n\n"),
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
		*port = (int)strtol(argv[1], (char **)NULL, 10);
		/*
		 * XXX -- this is silly, but strtol and
		 * atoi don't seem to be setting errno properly
		 * so a normal check for EINVAL or ERANGE
		 * doesn't work.  Weird.  Oh well, this works.
		 */
		if (*port == 0 && argv[1][0] != '0') {
			(void) fprintf(stderr,
		MSGSTR(4020, " Error: Invalid tray number (%s)"), argv[1]);
			(void) fprintf(stderr, "\n");
			exit(-1);
		}
	} else {
		*port = -1;
	}

	if ((path_phys =
	    g_get_physical_name(argv[path_index])) == NULL) {
	    (void) fprintf(stderr, "%s: ", whoami);
	    (void) fprintf(stderr,
		MSGSTR(112, "Error: Invalid pathname (%s)"),
		argv[path_index]);
	    (void) fprintf(stderr, "\n");
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
				(void) fprintf(stderr, MSGSTR(4014,
					"That device is not in "
					"tray %x\n"), *port);
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
	    (void) fprintf(stderr, "%s: ", whoami);
	    (void) fprintf(stderr,
		MSGSTR(112, "Error: Invalid pathname (%s)"),
		argv[0]);
	    (void) fprintf(stderr, "\n");
	    exit(-1);
	}
	if (ndevices > 1) {
		(void) fprintf(stderr, MSGSTR(4015,
			"%s: You need to specify a tray number\n"),
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
		(void) fprintf(stdout, MSGSTR(4016, "Alarm is Enabled!\n"));
	} else {
		(void) fprintf(stdout, MSGSTR(4017, "Alarm is Disabled!\n"));
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
	    (void) fprintf(stderr, "%s: ", whoami);
	    (void) fprintf(stderr,
		MSGSTR(112, "Error: Invalid pathname (%s)"),
		argv[0]);
	    (void) fprintf(stderr, "\n");
	    exit(-1);
	}

	if (argc > 2) {
		/* printf("argv[2] = %s\n", argv[2]); */
		secs = atoi(argv[2]);
	}

	/* printf("argc=%d, secs = %d\n", argc, secs); */
	if (secs == -1) {
		(void) fprintf(stdout, MSGSTR(4018,
			"Current alarm setting = %d seconds\n"),
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
	    (void) fprintf(stderr, "%s: ", whoami);
	    (void) fprintf(stderr,
		MSGSTR(112, "Error: Invalid pathname (%s)"),
		argv[0]);
	    (void) fprintf(stderr, "\n");
	    exit(-1);
	}
	if (strstr(path_phys, "SUNW,pln") == NULL) {
		(void) fprintf(stderr, MSGSTR(4019, "%s: power_off of directly "
			"attached RSMs is not supported.\n"), whoami);
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
