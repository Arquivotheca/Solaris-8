/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)adm.c	1.56	99/10/18 SMI"

/*LINTLIBRARY*/


/*
 * Administration program for SENA, RSM and SSA
 * subsystems and individual FC_AL devices.
 */

/*
 * I18N message number ranges
 *  This file: 2000 - 2999
 *  Shared common messages: 1 - 1999
 */

/* #define		 _POSIX_SOURCE 1 */


/*	Includes	*/
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
#include	<string.h>
#include	<ctype.h>
#include	<sys/sunddi.h>
#include	<sys/ddi.h>		/* for min */
#include	<sys/scsi/scsi.h>
#include	<strings.h>
#include	<sys/stat.h>
#include	<kstat.h>
#include	<sys/mkdev.h>
#include	<locale.h>
#include	<nl_types.h>
#include	<termio.h>		/* For password */
#include	<signal.h>
#include	<dirent.h>
#include	<l_common.h>
#include	<l_error.h>
#include	<stgcom.h>
#include	<a_state.h>
#include	"common.h"
#include	"hot.h"
#include	"luxadm.h"
#include	<sys/fibre-channel/fca/usocio.h> /* for kstat usoc structs */


/*	Global variables	*/
static		struct termios	termios;
static		int termio_fd;
char	*dtype[16]; /* Also used in hotplug.c */

/*	Internal functions	*/
static	void	adm_download(char **, char *, char *);
static	int	adm_fcode(int, char *);
static	void	display_link_status(char **);
static	int	ib_present_chk(struct l_state_struct *, int);
static	void	pho_display_config(char *);
static	void	display_disk_msg(struct l_disk_state_struct *,
		struct l_state_struct *, Bp_elem_st *, int);
static	void	print_individual_state(int, int);
static	void	up_password(char **);
static	void	up_encl_name(char **, int);
static	void	adm_power_off(char **, int, int);
static	void	adm_led(char **, int);
static	void	dump(char **);
static	void	adm_start(char **, int);
static	void	adm_stop(char **, int);
static	void	temperature_messages(struct l_state_struct *, int);
static	void	ctlr_messages(struct l_state_struct *, int, int);
static	void	fan_messages(struct l_state_struct *, int, int);
static	void	ps_messages(struct l_state_struct *, int, int);
static	void	abnormal_condition_display(struct l_state_struct *);
static	void	loop_messages(struct l_state_struct *, int, int);
static	void	revision_msg(struct l_state_struct *, int);
static	void	mb_messages(struct l_state_struct *, int, int);
static	void	back_plane_messages(struct l_state_struct *, int, int);
static	int	non_encl_fc_disk_display(Path_struct *, L_inquiry, int);
int		n_get_non_encl_list(WWN_list **, int);
static	void	n_rem_list_entry(uchar_t,  struct sf_al_map *,
		WWN_list **);
static	void	ssa_probe();
static	void	non_encl_probe();
static	void	pho_probe();
static	void	display_disk_info(L_inquiry, L_disk_state,
		Path_struct *, struct mode_page *, int, char *, int);
static	void	display_port_status(int d_state_flag);
static	void	display_fc_disk(struct path_struct *, char *, sf_al_map_t *,
		L_inquiry, int);
static	int	adm_display_config(char **, int, int);
static	void	temp_decode(Temp_elem_st *);
static	void	disp_degree(Temp_elem_st *);
static	void	trans_decode(Trans_elem_st *trans);
static	void	trans_messages(struct l_state_struct *, int);
static	void	ib_decode(Ctlr_elem_st *);
static	void	fan_decode(Fan_elem_st *);
static	void	adm_forcelip(char **);
static	void	adm_inquiry(char **);
static	int	get_enclStatus(char *, char *, int);
static	void	adm_port_offline_online(char **, int);
void		print_errString(int, char *);
int		print_devState(char *, char *, int, int, int);



/*
 * Given an error number, this functions
 * calls the g_get_errString() to print a
 * corresponding error message to the stderr.
 * g_get_errString() always returns an error
 * message, even in case of undefined error number.
 * So, there is no need to check for a NULL pointer
 * while printing the error message to the stdout.
 *
 * RETURNS: N/A
 *
 */
void
print_errString(int errnum, char *devpath)
{

char	*errStr;

	errStr = g_get_errString(errnum);

	if (devpath == NULL) {
		(void) fprintf(stderr,
				"%s \n\n", errStr);
	} else {
		(void) fprintf(stdout,
				"%s - %s.\n\n", errStr, devpath);
	}

	/* free the allocated memory for error string */
	if (errStr != NULL)
		(void) free(errStr);
}




/*
 * Gets the device's state from the SENA IB and
 * checks whether device is offlined, bypassed
 * or if the slot is empty and prints it to the
 * stdout.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
print_devState(char *devname, char *ppath, int fr_flag, int slot,
						int verbose_flag)
{
L_state		l_state;
int		err;
int		i, elem_index = 0;
uchar_t		device_off, ib_status_code, bypass_a_en, bypass_b_en;
Bp_elem_st	bpf, bpr;


	if ((err = l_get_status(ppath, &l_state, verbose_flag)) != 0) {
		(void) print_errString(err, ppath);
		return (err);
	}

	for (i = 0; i <  (int)l_state.ib_tbl.config.enc_num_elem; i++) {
		elem_index++;
		if (l_state.ib_tbl.config.type_hdr[i].type == ELM_TYP_BP) {
			break;
		}
		elem_index += l_state.ib_tbl.config.type_hdr[i].num;
	}
	(void) bcopy((const void *)
			&(l_state.ib_tbl.p2_s.element[elem_index]),
			(void *)&bpf, sizeof (bpf));
	(void) bcopy((const void *)
			&(l_state.ib_tbl.p2_s.element[elem_index + 1]),
			(void *)&bpr, sizeof (bpr));

	if (fr_flag) {
		device_off = l_state.drv_front[slot].ib_status.dev_off;
		bypass_a_en = l_state.drv_front[slot].ib_status.bypass_a_en;
		bypass_b_en = l_state.drv_front[slot].ib_status.bypass_b_en;
		ib_status_code = l_state.drv_front[slot].ib_status.code;
	} else {
		device_off = l_state.drv_rear[slot].ib_status.dev_off;
		bypass_a_en = l_state.drv_rear[slot].ib_status.bypass_a_en;
		bypass_b_en = l_state.drv_rear[slot].ib_status.bypass_b_en;
		ib_status_code = l_state.drv_rear[slot].ib_status.code;
	}
	if (device_off) {
		(void) fprintf(stdout,
				MSGSTR(2000,
				"%s is offlined and bypassed.\n"
				" Could not get device specific"
				" information.\n\n"),
				devname);
	} else if (bypass_a_en && bypass_b_en) {
		(void) fprintf(stdout,
				MSGSTR(2001,
				"%s is bypassed (Port:AB).\n"
				" Could not get device specific"
				" information.\n\n"),
				devname);
	} else if (ib_status_code == S_NOT_INSTALLED) {
		(void) fprintf(stdout,
				MSGSTR(2002,
				"Slot %s is empty.\n\n"),
				devname);
	} else if (((bpf.code != S_NOT_INSTALLED) &&
		((bpf.byp_a_enabled || bpf.en_bypass_a) &&
		(bpf.byp_b_enabled || bpf.en_bypass_b))) ||
		((bpr.code != S_NOT_INSTALLED) &&
		((bpr.byp_a_enabled || bpr.en_bypass_a) &&
		(bpr.byp_b_enabled || bpr.en_bypass_b)))) {
		(void) fprintf(stdout,
				MSGSTR(2003,
				"Backplane(Port:AB) is bypassed.\n"
				" Could not get device specific"
				" information for"
				" %s.\n\n"), devname);
	} else {
		(void) fprintf(stderr,
				MSGSTR(33,
				" Error: converting"
				" %s to physical path.\n"
				" Invalid pathname.\n"),
				devname);
	}
	return (-1);
}



static void
adm_bypass_enable(char **argv, int argc, int bypass_flag)
{
int		path_index = 0, err = 0;
L_inquiry	inq;
char		*path_phys = NULL;
Path_struct	*path_struct;

	if ((err = l_convert_name(argv[path_index], &path_phys,
		&path_struct, Options & PVERBOSE)) != 0) {
		/*
		 * In case we did not find the device
		 * in the /devices directory.
		 *
		 * Only valid for pathnames like box,f1
		 */
		if (path_struct->ib_path_flag) {
			path_phys = path_struct->p_physical_path;
		} else {
			(void) fprintf(stdout,
					MSGSTR(33,
						" Error: converting"
						" %s to physical path.\n"
						" Invalid pathname.\n"),
					argv[path_index]);
			if (err != -1) {
				(void) print_errString(err, argv[path_index]);
			}
			exit(-1);
		}
	}
	if (path_struct->ib_path_flag) {
		if (Options & OPTION_F) {
			E_USEAGE();
			exit(-1);
		}
		/*
		 * We are addressing a disk using a path
		 * format type box,f1 and no disk
		 * path was found.
		 * So set the Force flag so no reserved/busy
		 * check is performed.
		 */
		if (err = d_dev_bypass_enable(path_struct,
			bypass_flag, OPTION_CAPF,
			Options & OPTION_A,
			Options & PVERBOSE)) {
			(void) print_errString(err, argv[path_index]);
			exit(-1);
		}
		return;
	}

	if (err = g_get_inquiry(path_phys, &inq)) {
		(void) print_errString(err, argv[path_index]);
		exit(-1);
	}
	if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) != 0) ||
		(strncmp((char *)inq.inq_vid, "SUN     ",
		sizeof (inq.inq_vid)) &&
		(inq.inq_dtype == DTYPE_ESI))) {
		if ((!((Options & OPTION_F) ||
			(Options & OPTION_R))) ||
			((Options & OPTION_R) &&
			(Options & OPTION_F))) {
			E_USEAGE();
			exit(-1);
		}
		if (err = d_bp_bypass_enable(path_phys, bypass_flag,
			Options & OPTION_A,
			Options & OPTION_F,
			Options & OPTION_CAPF,
			Options & PVERBOSE)) {
		    (void) print_errString(err, argv[path_index]);
		    exit(-1);
		}
	} else if (inq.inq_dtype == DTYPE_DIRECT) {
		if (Options & OPTION_F) {
			E_USEAGE();
			exit(-1);
		}
		if (err = d_dev_bypass_enable(path_struct,
			bypass_flag, Options & OPTION_CAPF,
			Options & OPTION_A,
			Options & PVERBOSE)) {
			(void) print_errString(err, argv[path_index]);
			exit(-1);
		}
	}
}




/*
 * adm_download() Download subsystem microcode.
 * Path must point to a LUX IB or SSA controller.
 *
 * RETURNS:
 *	None.
 */
static	void
adm_download(char **argv, char *file_name, char *wwn)
{
int		path_index = 0, err = 0;
char		*path_phys = NULL;
L_inquiry	inq;
Path_struct	*path_struct;

	while (argv[path_index] != NULL) {
		/*
		 * See what kind of device we are talking to.
		 */
		if ((err = l_convert_name(argv[path_index], &path_phys,
			&path_struct, Options & PVERBOSE)) != 0) {
			(void) fprintf(stdout,
					MSGSTR(33,
						" Error: converting"
						" %s to physical path.\n"
						" Invalid pathname.\n"),
					argv[path_index]);
			if (err != -1) {
				(void) print_errString(err, argv[path_index]);
			}
			exit(-1);
		}
		if (err = g_get_inquiry(path_phys, &inq)) {
			(void) print_errString(err, argv[path_index]);
			exit(-1);
		}
		if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) != 0) ||
			(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) &&
			(inq.inq_dtype == DTYPE_ESI))) {
		/*
		 * Again this is like the ssaadm code in that the name
		 * is still not defined before this code must be released.
		 */
			/*
			 * Can only update the WWN on SSA's
			 */
			if (Options & OPTION_W) {
				(void) fprintf(stderr,
			MSGSTR(2004, "The WWN is not programmable "
				"on this subsystem.\n"));
				exit(-1);
			}
			if (err = l_download(path_phys,
				file_name, (Options & SAVE),
				(Options & PVERBOSE))) {
				(void) print_errString(err,
					(err == L_OPEN_PATH_FAIL) ?
					argv[path_index]: file_name);
				exit(-1);
			}
		} else {
			if (!file_name) {
				file_name = SSAFIRMWARE_FILE;
				(void) fprintf(stdout,
				MSGSTR(2005, "  Using file %s.\n"), file_name);
			}

			if (p_download(path_phys,
				file_name, 1, (Options & PVERBOSE),
					(uchar_t *)wwn)) {
				(void) fprintf(stderr,
				MSGSTR(2006, "Download Failed.\n"));
				P_ERR_PRINT;
				exit(-1);
			}
		}
		path_index++;
	}
}



/*
 *	Download host bus adapter FCode to all supported cards.
 *
 *	Specify a directory that holds the FCode files, or
 *	it will use the default dir.  Each file is dealt to
 *	the appropriate function.
 *
 *	-p prints current versions only, -d specifies a directory to load
 */
static	int
adm_fcode(int verbose, char *dir)
{
	struct stat statbuf;
	struct dirent *dirp;
	DIR	*dp;
	FILE	*fp;
	char	fbuf[BUFSIZ];
	char	file[MAXPATHLEN];
	int	strfound = 0;

	/* Find all adapters and print the current FCode version */
	if (Options & OPTION_P) {
		if (verbose) {
			(void) fprintf(stdout,
			    MSGSTR(2214, "  Searching for FC/S cards:\n"));
		}
		(void) fc_update(Options & PVERBOSE, 0, NULL);
		if (verbose) {
			(void) fprintf(stdout,
			    MSGSTR(2215, "\n  Searching for FC100/S cards:\n"));
		}
		(void) fcal_update(Options & PVERBOSE, NULL);
		if (verbose) {
			(void) fprintf(stdout,
		MSGSTR(2216, "\n  Searching for FC100/P, FC100/2P cards:\n"));
		}
		(void) q_qlgc_update(Options & PVERBOSE, NULL);

	/* Send files to the correct function for loading to the HBA */
	} else {

		if (!dir) {
			dir = FCODE_DIR;
			(void) fprintf(stdout, MSGSTR(2217,
			    "  Using directory %s"), dir);
		} else if (verbose) {
			(void) fprintf(stdout, MSGSTR(2217,
			    "  Using directory %s"), dir);
		}
		if (lstat(dir, &statbuf) < 0) {
			(void) fprintf(stderr, MSGSTR(134,
			    "%s: lstat() failed - %s\n"),
			    dir, strerror(errno));
			return (1);
		}
		if (S_ISDIR(statbuf.st_mode) == 0) {
		(void) fprintf(stderr,
		    MSGSTR(2218, "Error: %s is not a directory.\n"), dir);
			return (1);
		}
		if ((dp = opendir(dir)) == NULL) {
			(void) fprintf(stdout, MSGSTR(2219,
			    "  Error Cannot open directory %s\n"), dir);
			return (1);
		}

	while ((dirp = readdir(dp)) != NULL) {
		if (strcmp(dirp->d_name, ".") == 0 ||
		    strcmp(dirp->d_name, "..") == 0) {
			continue;
		}
		sprintf(file, "%s/%s", dir, dirp->d_name);

		if ((fp = fopen(file, "r")) == NULL) {
			(void) fprintf(stderr,
			    MSGSTR(2220,
				"Error: fopen() failed to open file "
				"%s\n"), file);
			closedir(dp);
			return (1);
		}
		while ((fgets(fbuf, BUFSIZ, fp)) != NULL) {
			if (strstr(fbuf, "SUNW,socal") != NULL) {
				(void) fprintf(stdout, MSGSTR(2221,
				    "\n  Using file: %s\n"), file);
				(void) fcal_update(Options & PVERBOSE, file);
				strfound++;
				break;
			} else if (strstr(fbuf, "soc") != NULL) {
				(void) fprintf(stdout, MSGSTR(2221,
				    "\n  Using file: %s\n"), file);
				(void) fc_update(Options & PVERBOSE, 0, file);
				strfound++;
				break;
			} else if (strstr(fbuf, "SUNW,ifp") != NULL) {
				(void) fprintf(stdout, MSGSTR(2221,
				    "\n  Using file: %s\n"), file);
			(void) q_qlgc_update(Options & PVERBOSE, file);
				strfound++;
				break;
			}
		}
		if (!strfound) {
			(void) fprintf(stderr, MSGSTR(2222,
			"\nError: %s is not a valid Fcode file.\n"), file);
		} else {
			strfound = 0;
		}
		fclose(fp);
	}
	closedir(dp);
	}
	return (0);
}



/*
 * display_link_status() Reads and displays the link status.
 *
 * RETURNS:
 *	none.
 */
static	void
display_link_status(char **argv)
{
AL_rls		*rls = NULL, *n;
int		path_index = 0, err = 0;
char		*path_phys = NULL;
Path_struct	*path_struct;


	while (argv[path_index] != NULL) {
		if ((err = l_convert_name(argv[path_index], &path_phys,
			&path_struct, Options & PVERBOSE)) != 0) {
			(void) fprintf(stdout,
					MSGSTR(33,
						" Error: converting"
						" %s to physical path.\n"
						" Invalid pathname.\n"),
					argv[path_index]);
			if (err != -1) {
				(void) print_errString(err, argv[path_index]);
			}
			exit(-1);
		}
		if (err = g_rdls(path_phys, &rls, Options & PVERBOSE)) {
		    (void) print_errString(err, argv[path_index]);
		    exit(-1);
		}
		n = rls;
		if (n != NULL) {
			(void) fprintf(stdout,
			MSGSTR(2007, "\nLink Error Status "
				"information for loop:%s\n"),
				n->driver_path);
			(void) fprintf(stdout, MSGSTR(2008, "al_pa   lnk fail "
			"   sync loss   signal loss   sequence err"
			"   invalid word   CRC\n"));
		}
		while (n) {
			(void) fprintf(stdout,
			"%x\t%-12d%-12d%-14d%-15d%-15d%-12d\n",
			n->al_ha,
			n->payload.rls_linkfail, n->payload.rls_syncfail,
			n->payload.rls_sigfail, n->payload.rls_primitiverr,
			n->payload.rls_invalidword, n->payload.rls_invalidcrc);
			n = n->next;
		}

		path_index++;
	}
	(void) fprintf(stdout,
		MSGSTR(2009, "NOTE: These LESB counts are not"
		" cleared by a reset, only power cycles.\n"
		"These counts must be compared"
		" to previously read counts.\n"));
}



/*
 * ib_present_chk() Check to see if IB 0 or 1 is present in the box.
 *
 * RETURN:
 *	1 if ib present
 *	0 otherwise
 */
static	int
ib_present_chk(struct l_state_struct *l_state, int which_one)
{
Ctlr_elem_st	ctlr;
int	i;
int	elem_index = 0;
int	result = 1;

	for (i = 0; i < (int)l_state->ib_tbl.config.enc_num_elem; i++) {
	    elem_index++;		/* skip global */
	    if (l_state->ib_tbl.config.type_hdr[i].type == ELM_TYP_IB) {
		(void) bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + which_one],
			(void *)&ctlr, sizeof (ctlr));
		if (ctlr.code == S_NOT_INSTALLED) {
			result = 0;
		}
		break;
	    }
	    elem_index += l_state->ib_tbl.config.type_hdr[i].num;
	}
	return (result);
}



/*
 * print_individual_state() Print individual disk status.
 *
 * RETURNS:
 *	none.
 */
static void
print_individual_state(int status, int port)
{
	if (status & L_OPEN_FAIL) {
		(void) fprintf(stdout, " (");
		(void) fprintf(stdout,
		MSGSTR(28, "Open Failed"));
		(void) fprintf(stdout, ")  ");
	} else if (status & L_NOT_READY) {
		(void) fprintf(stdout, " (");
		(void) fprintf(stdout,
			MSGSTR(20, "Not Ready"));
		(void) fprintf(stdout, ")    ");
	} else if (status & L_NOT_READABLE) {
		(void) fprintf(stdout, "(");
		(void) fprintf(stdout,
		MSGSTR(88, "Not Readable"));
		(void) fprintf(stdout, ")  ");
	} else if (status & L_SPUN_DWN_D) {
		(void) fprintf(stdout, " (");
		(void) fprintf(stdout,
		MSGSTR(68, "Spun Down"));
		(void) fprintf(stdout, ")    ");
	} else if (status & L_SCSI_ERR) {
		(void) fprintf(stdout, " (");
		(void) fprintf(stdout,
		MSGSTR(70, "SCSI Error"));
		(void) fprintf(stdout, ")   ");
	} else if (status & L_RESERVED) {
		if (port == PORT_A) {
			(void) fprintf(stdout,
			MSGSTR(2010,
				" (Rsrv cnflt:A) "));
		} else if (port == PORT_B) {
			(void) fprintf(stdout,
			MSGSTR(2011,
				" (Rsrv cnflt:B) "));
		} else {
			(void) fprintf(stdout,
			MSGSTR(2012,
				" (Reserve cnflt)"));
		}
	} else if (status & L_NO_LABEL) {
		(void) fprintf(stdout, "(");
		(void) fprintf(stdout,
			MSGSTR(92, "No UNIX Label"));
		(void) fprintf(stdout, ") ");
	}
}



/*
 * display_disk_msg() Displays status for
 * an individual SENA device.
 *
 * RETURNS:
 *	none.
 */
static	void
display_disk_msg(struct l_disk_state_struct *dsk_ptr,
	struct l_state_struct *l_state, Bp_elem_st *bp, int front_flag)
{
int	loop_flag = 0;
int	a_and_b = 0;
int	state_a = 0, state_b = 0;

	if (dsk_ptr->ib_status.code == S_NOT_INSTALLED) {
		(void) fprintf(stdout,
			MSGSTR(30, "Not Installed"));
			(void) fprintf(stdout, " ");
		if (dsk_ptr->ib_status.fault ||
			dsk_ptr->ib_status.fault_req) {
			(void) fprintf(stdout, "(");
			(void) fprintf(stdout,
				MSGSTR(2013, "Faulted"));
			(void) fprintf(stdout,
						")           ");
		} else if (dsk_ptr->ib_status.ident ||
			dsk_ptr->ib_status.rdy_to_ins ||
			dsk_ptr->ib_status.rmv) {
			(void) fprintf(stdout,
				MSGSTR(2014,
						"(LED Blinking)      "));
		} else {
			(void) fprintf(stdout,
						"                    ");
		}
	} else if (dsk_ptr->ib_status.dev_off) {
		(void) fprintf(stdout, MSGSTR(2015, "Off"));
		if (dsk_ptr->ib_status.fault || dsk_ptr->ib_status.fault_req) {
			(void) fprintf(stdout, "(");
			(void) fprintf(stdout,
				MSGSTR(2016, "Faulted"));
			(void) fprintf(stdout,
					")                      ");
		} else if (dsk_ptr->ib_status.bypass_a_en &&
			dsk_ptr->ib_status.bypass_b_en) {
			(void) fprintf(stdout,
				MSGSTR(2017,
					"(Bypassed:AB)"));
			(void) fprintf(stdout,
					"                  ");
		} else if (dsk_ptr->ib_status.bypass_a_en) {
			(void) fprintf(stdout,
				MSGSTR(2018,
					"(Bypassed: A)"));
			(void) fprintf(stdout,
					"                  ");
		} else if (dsk_ptr->ib_status.bypass_b_en) {
			(void) fprintf(stdout,
				MSGSTR(2019,
					"(Bypassed: B)"));
			(void) fprintf(stdout,
					"                  ");
		} else {
			(void) fprintf(stdout,
					"                              ");
		}
	} else {
		(void) fprintf(stdout, MSGSTR(2020, "On"));

		if (dsk_ptr->ib_status.fault || dsk_ptr->ib_status.fault_req) {
			(void) fprintf(stdout, " (");
			(void) fprintf(stdout,
				MSGSTR(2021, "Faulted"));
			(void) fprintf(stdout, ")      ");
		} else if (dsk_ptr->ib_status.bypass_a_en &&
			dsk_ptr->ib_status.bypass_b_en) {
			(void) fprintf(stdout, " ");
			(void) fprintf(stdout,
				MSGSTR(2022, "(Bypassed:AB)"));
			(void) fprintf(stdout, "  ");
		} else if (ib_present_chk(l_state, 0) &&
			dsk_ptr->ib_status.bypass_a_en) {
			/*
			 * Before printing that the port is bypassed
			 * verify that there is an IB for this port.
			 * If not then don't print.
			 */
			(void) fprintf(stdout, " ");
			(void) fprintf(stdout,
				MSGSTR(2023, "(Bypassed: A)"));
			(void) fprintf(stdout, "  ");
		} else if (ib_present_chk(l_state, 1) &&
			dsk_ptr->ib_status.bypass_b_en) {
			(void) fprintf(stdout, " ");
			(void) fprintf(stdout,
				MSGSTR(2024, "(Bypassed: B)"));
			(void) fprintf(stdout, "  ");
		} else if ((bp->code != S_NOT_INSTALLED) &&
				((bp->byp_a_enabled || bp->en_bypass_a) &&
				!(bp->byp_b_enabled || bp->en_bypass_b))) {
			(void) fprintf(stdout,
				MSGSTR(2025,
					" (Bypassed BP: A)"));
		} else if ((bp->code != S_NOT_INSTALLED) &&
				((bp->byp_b_enabled || bp->en_bypass_b) &&
				!(bp->byp_a_enabled || bp->en_bypass_a))) {
			(void) fprintf(stdout,
				MSGSTR(2026,
					"(Bypassed BP: B)"));
		} else if ((bp->code != S_NOT_INSTALLED) &&
				((bp->byp_a_enabled || bp->en_bypass_a) &&
				(bp->byp_b_enabled || bp->en_bypass_b))) {
			(void) fprintf(stdout,
				MSGSTR(2027,
					"(Bypassed BP:AB)"));
		} else {
			state_a = dsk_ptr->g_disk_state.d_state_flags[PORT_A];
			state_b = dsk_ptr->g_disk_state.d_state_flags[PORT_B];
			a_and_b = state_a & state_b;

			if (dsk_ptr->l_state_flag & L_NO_LOOP) {
				(void) fprintf(stdout,
				MSGSTR(2028,
					" (Loop not accessible)"));
				loop_flag = 1;
			} else if (dsk_ptr->l_state_flag & L_INVALID_WWN) {
				(void) fprintf(stdout,
				MSGSTR(2029,
					" (Invalid WWN)  "));
			} else if (dsk_ptr->l_state_flag & L_INVALID_MAP) {
				(void) fprintf(stdout,
				MSGSTR(2030,
					" (Login failed) "));
			} else if (dsk_ptr->l_state_flag & L_NO_PATH_FOUND) {
				(void) fprintf(stdout,
				MSGSTR(2031,
					" (No path found)"));
			} else if (a_and_b) {
				print_individual_state(a_and_b, PORT_A_B);
			} else if (state_a && (!state_b)) {
				print_individual_state(state_a, PORT_A);
			} else if ((!state_a) && state_b) {
				print_individual_state(state_b, PORT_B);
			} else if (state_a || state_b) {
				/* NOTE: Double state - should do 2 lines. */
				print_individual_state(state_a | state_b,
								PORT_A_B);
			} else {
				(void) fprintf(stdout, " (");
				(void) fprintf(stdout,
					MSGSTR(29, "O.K."));
				(void) fprintf(stdout,
					")         ");
			}
		}
		if (loop_flag) {
			(void) fprintf(stdout, "          ");
		} else if (strlen(dsk_ptr->g_disk_state.node_wwn_s)) {
			(void) fprintf(stdout, "%s",
			dsk_ptr->g_disk_state.node_wwn_s);
		} else {
			(void) fprintf(stdout, "                ");
		}
	}
	if (front_flag) {
		(void) fprintf(stdout, "    ");
	}
}



/*
 * pho_display_config() Displays device status
 * information for a SENA enclosure.
 *
 * RETURNS:
 *	none.
 */
static void
pho_display_config(char *path_phys)
{
L_state		l_state;
Bp_elem_st	bpf, bpr;
int		i, j, elem_index = 0, err = 0;


	/* Get global status */
	if (err = l_get_status(path_phys, &l_state,
			(Options & PVERBOSE))) {
	    (void) print_errString(err, path_phys);
	    exit(-1);
	}

	/*
	 * Look for abnormal status.
	 */
	if (l_state.ib_tbl.p2_s.ui.ab_cond) {
		abnormal_condition_display(&l_state);
	}

	(void) fprintf(stdout,
		MSGSTR(2032, "                                 DISK STATUS \n"
		"SLOT   FRONT DISKS       (Node WWN)         "
		" REAR DISKS        (Node WWN)\n"));
	/*
	 * Print the status for each disk
	 */
	for (j = 0; j <  (int)l_state.ib_tbl.config.enc_num_elem; j++) {
		elem_index++;
		if (l_state.ib_tbl.config.type_hdr[j].type == ELM_TYP_BP)
			break;
		elem_index += l_state.ib_tbl.config.type_hdr[j].num;
	}
	(void) bcopy((const void *)
		&(l_state.ib_tbl.p2_s.element[elem_index]),
		(void *)&bpf, sizeof (bpf));
	(void) bcopy((const void *)
		&(l_state.ib_tbl.p2_s.element[elem_index + 1]),
		(void *)&bpr, sizeof (bpr));

	for (i = 0; i < (int)l_state.total_num_drv/2; i++) {
		(void) fprintf(stdout, "%-2d     ", i);
		display_disk_msg(&l_state.drv_front[i], &l_state, &bpf, 1);
		display_disk_msg(&l_state.drv_rear[i], &l_state, &bpr, 0);
		(void) fprintf(stdout, "\n");
	}



	/*
	 * Display the subsystem status.
	 */
	(void) fprintf(stdout,
		MSGSTR(2033,
	"                                SUBSYSTEM STATUS\nFW Revision:"));
	for (i = 0; i < sizeof (l_state.ib_tbl.config.prod_revision); i++) {
		(void) fprintf(stdout, "%c",
			l_state.ib_tbl.config.prod_revision[i]);
	}
	(void) fprintf(stdout, MSGSTR(2034, "   Box ID:%d"),
		l_state.ib_tbl.box_id);
	(void) fprintf(stdout, "   ");
	(void) fprintf(stdout, MSGSTR(90, "Node WWN:"));
	for (i = 0; i < 8; i++) {
		(void) fprintf(stdout, "%1.2x",
		l_state.ib_tbl.config.enc_node_wwn[i]);
	}
	/* Make sure NULL terminated  although it is supposed to be */
	if (strlen((const char *)l_state.ib_tbl.enclosure_name) <=
		sizeof (l_state.ib_tbl.enclosure_name)) {
		(void) fprintf(stdout, MSGSTR(2035, "   Enclosure Name:%s\n"),
			l_state.ib_tbl.enclosure_name);
	}

	/*
	 *
	 */
	elem_index = 0;
	/* Get and print CONTROLLER messages */
	for (i = 0; i < (int)l_state.ib_tbl.config.enc_num_elem; i++) {
	    elem_index++;		/* skip global */
	    switch (l_state.ib_tbl.config.type_hdr[i].type) {
		case ELM_TYP_PS:
			ps_messages(&l_state, i, elem_index);
			break;
		case ELM_TYP_FT:
			fan_messages(&l_state, i, elem_index);
			break;
		case ELM_TYP_BP:
			back_plane_messages(&l_state, i, elem_index);
			break;
		case ELM_TYP_IB:
			ctlr_messages(&l_state, i, elem_index);
			break;
		case ELM_TYP_LN:
			/*
			 * NOTE: I just use the Photon's message
			 * string here and don't look at the
			 * language code. The string includes
			 * the language name.
			 */
			if (l_state.ib_tbl.config.type_hdr[i].text_len != 0) {
				(void) fprintf(stdout, "%s\t",
				l_state.ib_tbl.config.text[i]);
			}
			break;
		case ELM_TYP_LO:	/* Loop configuration */
			loop_messages(&l_state, i, elem_index);
			break;
		case ELM_TYP_MB:	/* Loop configuration */
			mb_messages(&l_state, i, elem_index);
			break;

	    }
		/*
		 * Calculate the index to each element.
		 */
		elem_index += l_state.ib_tbl.config.type_hdr[i].num;
	}
/*
	if (Options & OPTION_V) {
		adm_display_verbose(l_state);
	}
*/
	(void) fprintf(stdout, "\n");
}



/*
 * Change the FPM (Front Panel Module) password of the
 * subsystem associated with the IB addressed by the
 * enclosure or pathname to name.
 *
 */
static void
intfix(void)
{
	if (termio_fd) {
		termios.c_lflag |= ECHO;
		ioctl(termio_fd, TCSETS, &termios);
	}
	exit(SIGINT);
}


/*
 * up_password() Changes the password for SENA enclosure.
 *
 * RETURNS:
 *	none.
 */
static void
up_password(char **argv)
{
int		path_index = 0, err = 0;
char		password[1024];
char		input[1024];
int		i, j, matched, equal;
L_inquiry	inq;
void		(*sig)();
char		*path_phys = NULL;
Path_struct	*path_struct;


	if ((termio_fd = open("/dev/tty", O_RDONLY)) == -1) {
		(void) fprintf(stderr,
		MSGSTR(2036, "Error: tty open failed.\n"));
		exit(-1);
	}
	ioctl(termio_fd, TCGETS, &termios);
	sig = sigset(SIGINT, (void (*)())intfix);
	/*
	 * Make sure path valid and is to a PHO
	 * before bothering operator.
	 */
	if ((err = l_convert_name(argv[path_index], &path_phys,
		&path_struct, Options & PVERBOSE)) != 0) {
		(void) fprintf(stdout,
			MSGSTR(33,
				" Error: converting"
				" %s to physical path.\n"
				" Invalid pathname.\n"),
				argv[path_index]);
		if (err != -1) {
			(void) print_errString(err, argv[path_index]);
		}
		exit(-1);
	}
	if (err = g_get_inquiry(path_phys, &inq)) {
		(void) print_errString(err, argv[path_index]);
		exit(-1);
	}
	if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) == 0) &&
			(!(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) &&
			(inq.inq_dtype == DTYPE_ESI)))) {
		/*
		 * Again this is like the ssaadm code in that the name
		 * is still not defined before this code must be released.
		 */
		(void) fprintf(stderr,
		MSGSTR(2037, "Error: Enclosure is not a %s\n"),
			ENCLOSURE_PROD_ID);
		exit(-1);
	}
	(void) fprintf(stdout,
			MSGSTR(2038,
			"Changing FPM password for subsystem %s\n"),
			argv[path_index]);

	equal = 0;
	while (!equal) {
		memset(input, 0, sizeof (input));
		memset(password, 0, sizeof (password));
		(void) fprintf(stdout,
		MSGSTR(2039, "New password: "));

		termios.c_lflag &= ~ECHO;
		ioctl(termio_fd, TCSETS, &termios);

		(void) gets(input);
		(void) fprintf(stdout,
		MSGSTR(2040, "\nRe-enter new password: "));
		(void) gets(password);
		termios.c_lflag |= ECHO;
		ioctl(termio_fd, TCSETS, &termios);
		for (i = 0; input[i]; i++) {
			if (!isdigit(input[i])) {
				(void) fprintf(stderr,
			MSGSTR(2041, "\nError: Invalid password."
			" The password"
			" must be 4 decimal-digit characters.\n"));
				exit(-1);
			}
		}
		if (i && (i != 4)) {
			(void) fprintf(stderr,
			MSGSTR(2042, "\nError: Invalid password."
			" The password"
			" must be 4 decimal-digit characters.\n"));
			exit(-1);
		}
		for (j = 0; password[j]; j++) {
			if (!isdigit(password[j])) {
				(void) fprintf(stderr,
			MSGSTR(2043, "\nError: Invalid password."
			" The password"
			" must be 4 decimal-digit characters.\n"));
				exit(-1);
			}
		}
		if (i != j) {
			matched = -1;
		} else for (i = matched = 0; password[i]; i++) {
			if (password[i] == input[i]) {
				matched++;
			}
		}
		if ((matched != -1) && (matched == i)) {
			equal = 1;
		} else {
			(void) fprintf(stdout,
			MSGSTR(2044, "\npassword: They don't match;"
			" try again.\n"));
		}
	}
	(void) fprintf(stdout, "\n");
	sscanf(input, "%s", password);
	(void) signal(SIGINT, sig);	/* restore signal handler */

	/*  Send new password to IB */
	if (l_new_password(path_phys, input)) {
		(void) print_errString(err, path_phys);
		exit(-1);
	}
}



/*
 * up_encl_name() Update the enclosures logical name.
 *
 * RETURNS:
 *	none.
 */
static void
up_encl_name(char **argv, int argc)
{
int		i, rval, al_pa, path_index = 0, err = 0;
L_inquiry	inq;
Box_list	*b_list = NULL;
uchar_t		node_wwn[WWN_SIZE], port_wwn[WWN_SIZE];
char		wwn1[(WWN_SIZE*2)+1], name[1024], *path_phys = NULL;
Path_struct	*path_struct;

	(void) memset(name, 0, sizeof (name));
	(void) memset(&inq, 0, sizeof (inq));
	(void) sscanf(argv[path_index++], "%s", name);
	for (i = 0; name[i]; i++) {
		if ((!isalnum(name[i]) &&
			((name[i] != '#') &&
			(name[i] != '-') &&
			(name[i] != '_') &&
			(name[i] != '.'))) || i >= 16) {
			(void) fprintf(stderr,
			MSGSTR(2045, "Error: Invalid enclosure name.\n"));
			(void) fprintf(stderr, MSGSTR(2046,
			"Usage: %s [-v] subcommand {a name consisting of"
			" 1-16 alphanumeric characters}"
			" {enclosure... | pathname...}\n"), whoami);
			exit(-1);
		}
	}

	if (((Options & PVERBOSE) && (argc != 5)) ||
		(!(Options & PVERBOSE) && (argc != 4))) {
		(void) fprintf(stderr,
		MSGSTR(114, "Error: Incorrect number of arguments.\n"));
		(void) fprintf(stderr,  MSGSTR(2047,
		"Usage: %s [-v] subcommand {a name consisting of"
		" 1-16 alphanumeric characters}"
		" {enclosure... | pathname...}\n"), whoami);
		exit(-1);
	}

	if ((err = l_convert_name(argv[path_index], &path_phys,
		&path_struct, Options & PVERBOSE)) != 0) {
		(void) fprintf(stdout,
				MSGSTR(33,
				" Error: converting"
				" %s to physical path.\n"
				" Invalid pathname.\n"),
				argv[path_index]);
		if (err != -1) {
			(void) print_errString(err, argv[path_index]);
		}
		exit(-1);
	}
	/*
	 * Make sure we are talking to an IB.
	 */
	if (err = g_get_inquiry(path_phys, &inq)) {
		(void) print_errString(err, argv[path_index]);
		exit(-1);
	}
	if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) == 0) &&
			(!(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) &&
			(inq.inq_dtype == DTYPE_ESI)))) {
		/*
		 * Again this is like the ssaadm code in that the name
		 * is still not defined before this code must be released.
		 */
		(void) fprintf(stderr,
		MSGSTR(2048, "Error: Pathname does not point to a %s"
		" enclosure\n"), ENCLOSURE_PROD_NAME);
		exit(-1);
	}

	if (err = g_get_wwn(path_phys, port_wwn, node_wwn, &al_pa,
		Options & PVERBOSE)) {
		(void) print_errString(err, argv[path_index]);
		exit(-1);
	}

	for (i = 0; i < WWN_SIZE; i++) {
		(void) sprintf(&wwn1[i << 1], "%02x", node_wwn[i]);
	}
	if ((err = l_get_box_list(&b_list, Options & PVERBOSE)) != 0) {
		(void) print_errString(err, argv[path_index]);
		exit(-1);
	}
	if (b_list == NULL) {
		(void) fprintf(stdout,
			MSGSTR(93, "No %s enclosures found "
			"in /dev/es\n"), ENCLOSURE_PROD_NAME);
		exit(-1);
	} else if (l_duplicate_names(b_list, wwn1, name,
		Options & PVERBOSE)) {
		(void) fprintf(stdout,
		MSGSTR(2049, "Warning: The name you selected, %s,"
		" is already being used.\n"
		"Please choose a unique name.\n"
		"You can use the \"probe\" subcommand to"
		" see all of the enclosure names.\n"),
		name);
		(void) l_free_box_list(&b_list);
		exit(-1);
	}
	(void) l_free_box_list(&b_list);

	/*  Send new name to IB */
	if (rval = l_new_name(path_phys, name)) {
		(void) print_errString(rval, path_phys);
		exit(-1);
	}
	if (Options & PVERBOSE) {
		(void) fprintf(stdout,
			MSGSTR(2050, "The enclosure has been renamed to %s\n"),
			name);
	}
}


static int
get_enclStatus(char *phys_path, char *encl_name, int off_flag)
{
int	found_pwrOnDrv = 0, slot;
int	found_pwrOffDrv = 0, err = 0;
L_state	l_state;

	if ((err = l_get_status(phys_path,
				&l_state, Options & PVERBOSE)) != 0) {
		(void) print_errString(err, encl_name);
		return (err);
	}

	if (off_flag) {
		for (slot = 0; slot < l_state.total_num_drv/2;
							slot++) {
			if (((l_state.drv_front[slot].ib_status.code !=
							S_NOT_INSTALLED) &&
			(!l_state.drv_front[slot].ib_status.dev_off)) ||
			((l_state.drv_rear[slot].ib_status.code !=
							S_NOT_INSTALLED) &&
			(!l_state.drv_rear[slot].ib_status.dev_off))) {
				found_pwrOnDrv++;
				break;
			}
		}
		if (!found_pwrOnDrv) {
			(void) fprintf(stdout,
				MSGSTR(2051,
				"Notice: Drives in enclosure"
				" \"%s\" have already been"
				" powered off.\n\n"),
				encl_name);
			return (-1);
		}
	} else {
		for (slot  = 0; slot < l_state.total_num_drv/2;
							slot++) {
			if (((l_state.drv_front[slot].ib_status.code !=
							S_NOT_INSTALLED) &&
			(l_state.drv_front[slot].ib_status.dev_off)) ||
			((l_state.drv_rear[slot].ib_status.code !=
							S_NOT_INSTALLED) &&
			(l_state.drv_rear[slot].ib_status.dev_off))) {
				found_pwrOffDrv++;
				break;
			}
		}
		if (!found_pwrOffDrv) {
			(void) fprintf(stdout,
				MSGSTR(2052,
				"Notice: Drives in enclosure"
				" \"%s\" have already been"
				" powered on.\n\n"),
				encl_name);
			return (-1);
		}
	}
	return (0);
}



/*
 * Powers off a list of SENA enclosure(s)
 * and disk(s) which is provided by the user.
 *
 * RETURNS:
 *	none.
 */
static void
adm_power_off(char **argv, int argc, int off_flag)
{
int		path_index = 0, err = 0;
L_inquiry	inq;
char		*path_phys = NULL;
Path_struct	*path_struct;

	while (argv[path_index] != NULL) {
		if ((err = l_convert_name(argv[path_index], &path_phys,
			&path_struct, Options & PVERBOSE)) != 0) {
			/*
			 * In case we did not find the device
			 * in the /devices directory.
			 *
			 * Only valid for pathnames like box,f1
			 */
			if (path_struct->ib_path_flag) {
				path_phys = path_struct->p_physical_path;
			} else {
				(void) fprintf(stdout,
					MSGSTR(33,
				" Error: converting"
				" %s to physical path.\n"
				" Invalid pathname.\n"),
					argv[path_index]);
				if (err != -1) {
					(void) print_errString(err,
							argv[path_index]);
				}
				path_index++;
				continue;
			}
		}
		if (path_struct->ib_path_flag) {
			/*
			 * We are addressing a disk using a path
			 * format type box,f1.
			 */
			if (err = l_dev_pwr_up_down(path_phys,
			    path_struct, off_flag, Options & PVERBOSE,
			    Options & OPTION_CAPF)) {
				/*
				 * Is it Bypassed... try to give more
				 * informtaion.
				 */
				print_devState(argv[path_index],
					path_struct->p_physical_path,
					path_struct->f_flag, path_struct->slot,
					Options & PVERBOSE);
			}
			path_index++;
			continue;
		}

		if (err = g_get_inquiry(path_phys, &inq)) {
			(void) print_errString(err, argv[path_index]);
			path_index++;
			continue;
		}
		if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) != 0) ||
			(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) &&
			(inq.inq_dtype == DTYPE_ESI))) {

			if (get_enclStatus(path_phys, argv[path_index],
						off_flag) != 0) {
				path_index++;
				continue;
			}
			/* power off SENA enclosure. */
			if (err = l_pho_pwr_up_down(argv[path_index], path_phys,
			    off_flag, Options & PVERBOSE,
			    Options & OPTION_CAPF)) {
				(void) print_errString(err, argv[path_index]);
			}
		} else if (inq.inq_dtype == DTYPE_DIRECT) {
			if (err = l_dev_pwr_up_down(path_phys,
			    path_struct, off_flag, Options & PVERBOSE,
			    Options & OPTION_CAPF)) {
				(void) print_errString(err, argv[path_index]);
			}
		} else {
			/*
			 * SSA section:
			 * Note:  Within power_off we will
			 * parse the controller and tray number
			 * arguments.  If these fail, then
			 * luxadm will exit.  If everything
			 * works, then we have to strip two arguments
			 * off of argv.
			 */
			power_off(&argv[path_index], argc - path_index);
			path_index++;
		}
		path_index++;
	}
}



/*
 * adm_led() The led_request subcommand requests the subsystem
 * to display the current state or turn off, on, or blink
 * the yellow LED associated with the disk specified by the
 * enclosure or pathname.
 *
 * RETURNS:
 *	none.
 */
static void
adm_led(char **argv, int led_action)
{
int		path_index = 0, err = 0;
sf_al_map_t	map;
L_inquiry	inq;
Dev_elem_st	status;
char		*path_phys = NULL;
Path_struct	*path_struct;

	while (argv[path_index] != NULL) {
		if ((err = l_convert_name(argv[path_index], &path_phys,
			&path_struct, Options & PVERBOSE)) != 0) {
			/* Make sure we have a device path. */
			if (path_struct->ib_path_flag) {
				path_phys = path_struct->p_physical_path;
			} else {
				(void) fprintf(stdout,
					MSGSTR(33,
				" Error: converting"
				" %s to physical path.\n"
				" Invalid pathname.\n"),
					argv[path_index]);
				if (err != -1) {
					(void) print_errString(err,
							argv[path_index]);
				}
				exit(-1);
			}
			p_error_msg_ptr = NULL;
		}
		if (!path_struct->ib_path_flag) {
			if (err = g_get_inquiry(path_phys, &inq)) {
				(void) print_errString(err, argv[path_index]);
				exit(-1);
			}
			if (inq.inq_dtype != DTYPE_DIRECT) {
				(void) fprintf(stdout,
				MSGSTR(2053,
				"Error: pathname must be to a disk device.\n"
				" %s\n"), argv[path_index]);
				exit(-1);
			}
		}

		/*
		 * See if we are in fact talking to a loop or not.
		 */
		if (!g_get_dev_map(path_phys, &map,
			(Options & PVERBOSE)) != 0) {

			if (led_action == L_LED_ON) {
				(void) fprintf(stdout,
				MSGSTR(2054,
				"The led_on functionality is not applicable "
				"to this subsystem.\n"));
				exit(-1);
			}

			if (err = l_led(path_struct, led_action, &status,
				(Options & PVERBOSE))) {
				(void) print_errString(err, argv[path_index]);
				exit(-1);
			}
			switch (led_action) {
			case L_LED_STATUS:
			    if (status.fault || status.fault_req) {
				if (!path_struct->slot_valid) {
					(void) fprintf(stdout,
					MSGSTR(2055, "LED state is ON for "
					"device:\n  %s\n"), path_phys);
				} else {
					(void) fprintf(stdout,
					(path_struct->f_flag) ?
					MSGSTR(2056, "LED state is ON for "
					"device in location: front,slot %d\n")
					: MSGSTR(2057, "LED state is ON for "
					"device in location: rear,slot %d\n"),
					path_struct->slot);
				}
			    } else if (status.ident ||
				status.rdy_to_ins || status.rmv) {
				if (!path_struct->slot_valid) {
					(void) fprintf(stdout,
					MSGSTR(2058,
					"LED state is BLINKING for "
					"device:\n  %s\n"), path_phys);
				} else {
					(void) fprintf(stdout,
					(path_struct->f_flag) ?
				MSGSTR(2059, "LED state is BLINKING for "
					"device in location: front,slot %d\n")
				: MSGSTR(2060, "LED state is BLINKING for "
					"device in location: rear,slot %d\n"),
					path_struct->slot);
				}
			    } else {
				if (!path_struct->slot_valid) {
					(void) fprintf(stdout,
					MSGSTR(2061, "LED state is OFF for "
					"device:\n  %s\n"), path_phys);
				} else {
					(void) fprintf(stdout,
					(path_struct->f_flag) ?
					MSGSTR(2062, "LED state is OFF for "
					"device in location: front,slot %d\n")
					: MSGSTR(2063, "LED state is OFF for "
					"device in location: rear,slot %d\n"),
					path_struct->slot);
				}
			    }
			    break;
			}
		} else {
			Options &= ~(OPTION_E | OPTION_D);
			switch (led_action) {
			case L_LED_STATUS:
				break;
			case L_LED_RQST_IDENTIFY:
				(void) fprintf(stdout,
				MSGSTR(2064, "Blinking is not supported"
				" by this subsystem.\n"));
				exit(-1);
				/*NOTREACHED*/
			case L_LED_ON:
				Options |= OPTION_E;
				break;
			case L_LED_OFF:
				Options |= OPTION_D;
				break;
			}
			led(&argv[path_index], Options, 0);
		}
		path_index++;
	}
}



/*
 * dump() Dump information
 *
 * RETURNS:
 *	none.
 */
static void
dump(char **argv)
{
uchar_t		*buf;
int		path_index = 0, err = 0;
L_inquiry	inq;
char		hdr_buf[MAXNAMELEN];
Rec_diag_hdr	*hdr, *hdr_ptr;
char		*path_phys = NULL;
Path_struct	*path_struct;

	/*
	 * get big buffer
	 */
	if ((hdr = (struct rec_diag_hdr *)g_zalloc(MAX_REC_DIAG_LENGTH)) ==
								NULL) {
		(void) print_errString(L_MALLOC_FAILED, NULL);
		exit(-1);
	}
	buf = (uchar_t *)hdr;

	while (argv[path_index] != NULL) {
		if ((err = l_convert_name(argv[path_index], &path_phys,
			&path_struct, Options & PVERBOSE)) != 0) {
			(void) fprintf(stdout,
				MSGSTR(33,
					" Error: converting"
					" %s to physical path.\n"
					" Invalid pathname.\n"),
				argv[path_index]);
			if (err != -1) {
				(void) print_errString(err, argv[path_index]);
			}
			exit(-1);
		}
		if (err = g_get_inquiry(path_phys, &inq)) {
			(void) print_errString(err, argv[path_index]);
		} else {
			(void) g_dump(MSGSTR(2065, "INQUIRY data:   "),
			(uchar_t *)&inq, 5 + inq.inq_len, HEX_ASCII);
		}

		(void) memset(buf, 0, MAX_REC_DIAG_LENGTH);
		if (err = l_get_envsen(path_phys, buf, MAX_REC_DIAG_LENGTH,
			(Options & PVERBOSE))) {
		    (void) print_errString(err, argv[path_index]);
		    exit(-1);
		}
		(void) fprintf(stdout,
			MSGSTR(2066, "\t\tEnvironmental Sense Information\n"));

		/*
		 * Dump all pages.
		 */
		hdr_ptr = hdr;

		while (hdr_ptr->page_len != 0) {
			(void) sprintf(hdr_buf, MSGSTR(2067, "Page %d:   "),
				hdr_ptr->page_code);
			(void) g_dump(hdr_buf, (uchar_t *)hdr_ptr,
				HEADER_LEN + hdr_ptr->page_len, HEX_ASCII);
			hdr_ptr += ((HEADER_LEN + hdr_ptr->page_len) /
				sizeof (struct	rec_diag_hdr));
		}
		path_index++;
	}
	(void) free(buf);
}



/*
 * display_socal_stats() Display socal driver kstat information.
 *
 * RETURNS:
 *	none.
 */
static void
display_socal_stats(int port, char *socal_path, struct socal_stats *fc_stats)
{
int		i;
int		header_flag = 0;
char		status_msg_buf[MAXNAMELEN];
int		num_status_entries;

	(void) fprintf(stdout, MSGSTR(2068,
		"\tInformation for FC Loop on port %d of"
		" FC100/S Host Adapter\n\tat path: %s\n"),
		port, socal_path);
	if (fc_stats->version > 1) {
		(void) fprintf(stdout, "\t");
		(void) fprintf(stdout, MSGSTR(32,
			"Information from %s"), fc_stats->drvr_name);
		(void) fprintf(stdout, "\n");
		if ((*fc_stats->node_wwn != NULL) &&
			(*fc_stats->port_wwn[port] != NULL)) {
			(void) fprintf(stdout, MSGSTR(104,
				"  Host Adapter WWN's: Node:%s"
				"  Port:%s\n"),
				fc_stats->node_wwn,
				fc_stats->port_wwn[port]);
		}
		if (*fc_stats->fw_revision != NULL) {
			(void) fprintf(stdout, MSGSTR(105,
				"  Host Adapter Firmware Revision: %s\n"),
				fc_stats->fw_revision);
		}
		if (fc_stats->parity_chk_enabled != 0) {
			(void) fprintf(stdout, MSGSTR(2069,
			"  This Host Adapter checks S-Bus parity.\n"));
		}
	}

	(void) fprintf(stdout, MSGSTR(2070,
		"  Version Resets  Req_Q_Intrpts  Qfulls"
		" Unsol_Resps Lips\n"));

	(void) fprintf(stdout,  "  %4d%8d%11d%13d%10d%7d\n",
			fc_stats->version,
			fc_stats->resets,
			fc_stats->reqq_intrs,
			fc_stats->qfulls,
			fc_stats->pstats[port].unsol_resps,
			fc_stats->pstats[port].lips);

	(void) fprintf(stdout, MSGSTR(2071,
		"  Els_rcvd  Abts"
		"     Abts_ok Offlines Loop_onlines Onlines\n"));

	(void) fprintf(stdout, "  %4d%9d%10d%9d%13d%10d\n",
			fc_stats->pstats[port].els_rcvd,
			fc_stats->pstats[port].abts,
			fc_stats->pstats[port].abts_ok,
			fc_stats->pstats[port].offlines,
			fc_stats->pstats[port].online_loops,
			fc_stats->pstats[port].onlines);

	/* If any status conditions exist then display */
	if (fc_stats->version > 1) {
		num_status_entries = FC_STATUS_ENTRIES;
	} else {
		num_status_entries = 64;
	}

	for (i = 0; i < num_status_entries; i++) {
		if (fc_stats->pstats[port].resp_status[i] != 0) {
			if (header_flag++ == 0) {
				(void) fprintf(stdout, MSGSTR(2072,
				"  Fibre Channel Transport status:\n        "
				"Status                       Value"
				"           Count\n"));
			}
			(void) l_format_fc_status_msg(status_msg_buf,
			fc_stats->pstats[port].resp_status[i], i);
			(void) fprintf(stdout, "        %s\n",
				status_msg_buf);
		}
	}
}

/*
 * display_usoc_stats() Display socal driver kstat information.
 *
 * RETURNS:
 *	none.
 */
static void
display_usoc_stats(int port, char *socal_path, struct usoc_stats *stats)
{
	struct socal_stats socal_stats;
	bzero(&socal_stats, sizeof (struct socal_stats));
	memcpy(&socal_stats, stats, sizeof (struct usoc_stats));
	/*
	 * XXX -- This should be fixed at the driver level.
	 *
	 * Once RFE 4245408 is implemented, this should
	 * just be a simple cast.
	 */

	/*
	 * Now we have to recopy the second usoc_pstats
	 * as it's shorter so things didn't line up
	 * in the last memcpy. (If it was larger
	 * this wouldn't work)
	 */
	memcpy(&socal_stats.pstats[1], &stats->pstats[1],
		sizeof (struct usoc_pstats));
	/*
	 * NOTE:  The tail end of socal_stats is now garbage
	 * as usoc_stats is smaller.  We set the version
	 * number back to 1 so that display_socal_stats
	 * will use the smaller version.
	 */
	socal_stats.version = 1; /* Force old behavior */

	/*
	 * So now we can just call the old code.
	 */
	display_socal_stats(port, socal_path, &socal_stats);
}


/*
 * display_sf_stats() Display sf driver kstat information.
 *
 * RETURNS:
 *	none.
 */
static void
display_sf_stats(char *path_phys, int dtype, struct sf_stats *sf_stats)
{
int		i, al_pa, err = 0;
sf_al_map_t	map;
uchar_t		node_wwn[WWN_SIZE];
uchar_t		port_wwn[WWN_SIZE];
	if (sf_stats->version > 1) {
		(void) fprintf(stdout, "\n\t");
		(void) fprintf(stdout, MSGSTR(32,
			"Information from %s"),
			sf_stats->drvr_name);
		(void) fprintf(stdout, "\n");
	} else {
		(void) fprintf(stdout,
			MSGSTR(2073, "\n\t\tInformation from sf driver:\n"));
	}

	(void) fprintf(stdout, MSGSTR(2074,
		"  Version  Lip_count  Lip_fail"
		" Alloc_fail  #_cmds "
		"Throttle_limit  Pool_size\n"));

	(void) fprintf(stdout, "  %4d%9d%12d%11d%10d%11d%12d\n",
			sf_stats->version,
			sf_stats->lip_count,
			sf_stats->lip_failures,
			sf_stats->cralloc_failures,
			sf_stats->ncmds,
			sf_stats->throttle_limit,
			sf_stats->cr_pool_size);

	(void) fprintf(stdout, MSGSTR(2075,
		"\n\t\tTARGET ERROR INFORMATION:\n"));
	(void) fprintf(stdout, MSGSTR(2076,
		"AL_PA  Els_fail Timouts Abts_fail"
		" Tsk_m_fail "
		" Data_ro_mis Dl_len_mis Logouts\n"));

	if (err = g_get_dev_map(path_phys, &map, (Options & PVERBOSE))) {
		(void) print_errString(err, path_phys);
		exit(-1);
	}

	if (dtype == DTYPE_DIRECT) {
		if (err = g_get_wwn(path_phys, port_wwn, node_wwn, &al_pa,
			Options & PVERBOSE)) {
			(void) print_errString(err, path_phys);
			exit(-1);
		}
		for (i = 0; i < map.sf_count; i++) {
			if (map.sf_addr_pair[i].sf_al_pa ==
					al_pa) {
		(void) fprintf(stdout,
			"%3x%10d%8d%10d%11d%13d%11d%9d\n",
				map.sf_addr_pair[i].sf_al_pa,
				sf_stats->tstats[i].els_failures,
				sf_stats->tstats[i].timeouts,
				sf_stats->tstats[i].abts_failures,
				sf_stats->tstats[i].task_mgmt_failures,
				sf_stats->tstats[i].data_ro_mismatches,
				sf_stats->tstats[i].dl_len_mismatches,
				sf_stats->tstats[i].logouts_recvd);
				break;
			}
		}
		if (i >= map.sf_count) {
			(void) print_errString(L_INVALID_LOOP_MAP, path_phys);
			exit(-1);
		}
	} else {
		for (i = 0; i < map.sf_count; i++) {
			(void) fprintf(stdout,
				"%3x%10d%8d%10d%11d%13d%11d%9d\n",
				map.sf_addr_pair[i].sf_al_pa,
				sf_stats->tstats[i].els_failures,
				sf_stats->tstats[i].timeouts,
				sf_stats->tstats[i].abts_failures,
				sf_stats->tstats[i].task_mgmt_failures,
				sf_stats->tstats[i].data_ro_mismatches,
				sf_stats->tstats[i].dl_len_mismatches,
				sf_stats->tstats[i].logouts_recvd);
		}
	}
}



/*
 * adm_display_err() Displays enclosure specific
 * error information.
 *
 * RETURNS:
 *	none.
 */
static void
adm_display_err(char *path_phys, int dtype, int verbose_flag)
{
int		i, drvr_inst, socal_inst, port, al_pa, err = 0;
char		*char_ptr, socal_path[MAXPATHLEN], drvr_path[MAXPATHLEN];
struct		stat sbuf;
kstat_ctl_t	*kc;
kstat_t		*ifp_ks, *sf_ks, *fc_ks;
sf_stats_t	sf_stats;
socal_stats_t	fc_stats;
struct usoc_stats	usoc_stats;
ifp_stats_t	ifp_stats;
int		header_flag = 0;
char		status_msg_buf[MAXNAMELEN];
sf_al_map_t	map;
uchar_t		node_wwn[WWN_SIZE], port_wwn[WWN_SIZE];
/* XXX -- Leadville stuff */
uint_t		path_type;

	if ((kc = kstat_open()) == (kstat_ctl_t *)NULL) {
		(void) fprintf(stdout,
			MSGSTR(2077, " Error: can't open kstat\n"));
		exit(-1);
	}

	(void) strcpy(drvr_path, path_phys);

	if ((char_ptr = strrchr(drvr_path, '/')) == NULL) {
		(void) print_errString(L_INVLD_PATH_NO_SLASH_FND, path_phys);
		exit(-1);
	}
	*char_ptr = '\0';   /* Make into nexus or HBA driver path. */

	/*
	 * Each HBA and driver stack has its own structures
	 * for this, so we have to handle each one individually.
	 */
	path_type = g_get_path_type(drvr_path);

	if (path_type) { /* Quick sanity check for valid path */
		if ((err = g_get_nexus_path(drvr_path, &char_ptr)) != 0) {
			(void) print_errString(err, path_phys);
			exit(-1);
		}
		(void) strcpy(socal_path, char_ptr);

	}

	/* attach :devctl to get node stat instead of dir stat. */
	(void) strcat(drvr_path, FC_CTLR);

	if (stat(drvr_path, &sbuf) < 0) {
		(void) print_errString(L_LSTAT_ERROR, path_phys);
		exit(-1);
	}

	drvr_inst = minor(sbuf.st_rdev);


	/*
	 * first take care of the QLogic cards as they are
	 * the most unique
	 */
	if (path_type & FC4_PCI_FCA || path_type & FC_PCI_FCA) {
	    if ((ifp_ks = kstat_lookup(kc, "ifp",
			drvr_inst, "statistics")) != NULL) {

		if (kstat_read(kc, ifp_ks, &ifp_stats) < 0) {
			(void) fprintf(stdout,
				MSGSTR(2082,
				"Error: could not read ifp%d\n"), drvr_inst);
			exit(-1);
		}
		(void) fprintf(stdout, MSGSTR(2083,
			"\tInformation for FC Loop of"
			" FC100/P Host Adapter\n\tat path: %s\n"),
			drvr_path);
		if (ifp_stats.version > 1) {
			(void) fprintf(stdout, "\t");
			(void) fprintf(stdout, MSGSTR(32,
				"Information from %s"),
				ifp_stats.drvr_name);
			(void) fprintf(stdout, "\n");
			if ((*ifp_stats.node_wwn != NULL) &&
				(*ifp_stats.port_wwn != NULL)) {
				(void) fprintf(stdout, MSGSTR(104,
					"  Host Adapter WWN's: Node:%s"
					"  Port:%s\n"),
					ifp_stats.node_wwn,
					ifp_stats.port_wwn);
			}
			if (*ifp_stats.fw_revision != NULL) {
				(void) fprintf(stdout, MSGSTR(105,
				"  Host Adapter Firmware Revision: %s\n"),
				ifp_stats.fw_revision);
			}
			if (ifp_stats.parity_chk_enabled != 0) {
				(void) fprintf(stdout, MSGSTR(2084,
				"  This Host Adapter checks "
				"PCI-Bus parity.\n"));
			}
		}

		(void) fprintf(stdout, MSGSTR(2085,
			"        Version Lips\n"));
		(void) fprintf(stdout, "  %10d%7d\n",
				ifp_stats.version,
				ifp_stats.lip_count);
		/* If any status conditions exist then display */
		for (i = 0; i < FC_STATUS_ENTRIES; i++) {
			if (ifp_stats.resp_status[i] != 0) {
				if (header_flag++ == 0) {
					(void) fprintf(stdout, MSGSTR(2086,
					"  Fibre Channel Transport "
					"status:\n        "
					"Status           "
					"            Value"
					"           Count\n"));
				}
				(void) l_format_ifp_status_msg(
					status_msg_buf,
					ifp_stats.resp_status[i], i);
					(void) fprintf(stdout, "        %s\n",
					status_msg_buf);
			}
		}

		(void) fprintf(stdout, MSGSTR(2087,
			"\n\t\tTARGET ERROR INFORMATION:\n"));
		(void) fprintf(stdout, MSGSTR(2088,
			"AL_PA  logouts_recvd  task_mgmt_failures"
			"  data_ro_mismatches  data_len_mismatch\n"));

		if (err = g_get_dev_map(path_phys, &map,
					(Options & PVERBOSE))) {
			(void) print_errString(err, path_phys);
			exit(-1);
		}

		if (dtype == DTYPE_DIRECT) {
			if (err = g_get_wwn(path_phys, port_wwn,
				node_wwn, &al_pa,
				Options & PVERBOSE)) {
				(void) print_errString(err, path_phys);
				exit(-1);
			}
			for (i = 0; i < map.sf_count; i++) {
				if (map.sf_addr_pair[i].sf_al_pa ==
						al_pa) {
			(void) fprintf(stdout, "%3x%14d%18d%20d%20d\n",
					map.sf_addr_pair[i].sf_al_pa,
					ifp_stats.tstats[i].logouts_recvd,
					ifp_stats.tstats[i].task_mgmt_failures,
					ifp_stats.tstats[i].data_ro_mismatches,
					ifp_stats.tstats[i].dl_len_mismatches);
					break;
				}
			}
			if (i >= map.sf_count) {
			(void) print_errString(L_INVALID_LOOP_MAP, path_phys);
				exit(-1);
			}
		} else {
			for (i = 0; i < map.sf_count; i++) {
				(void) fprintf(stdout, "%3x%14d%18d%20d%20d\n",
					map.sf_addr_pair[i].sf_al_pa,
					ifp_stats.tstats[i].logouts_recvd,
					ifp_stats.tstats[i].task_mgmt_failures,
					ifp_stats.tstats[i].data_ro_mismatches,
					ifp_stats.tstats[i].dl_len_mismatches);
			}
		}
	    }
	/* End of the PCI section */

	} else {
	/*
	 * We must have one of the Sbus cards, so lets get
	 * the common stuff out of the way (driver stack independent)
	 */
	    if (stat(socal_path, &sbuf) < 0) {
		(void) print_errString(L_LSTAT_ERROR, path_phys);
		exit(-1);
	    }
	    socal_inst = minor(sbuf.st_rdev)/2;
	    port = socal_inst%2;

	/*
	 * Now we handle each driver stack independantly
	 */
	    if (path_type & FC4_SF_XPORT) {
		if (!(sf_ks = kstat_lookup(kc, "sf", drvr_inst,
			"statistics"))) {
			(void) fprintf(stdout,
				MSGSTR(2078,
			" Error: could not lookup driver stats for sf%d\n"),
				drvr_inst);
			exit(-1);
		}
		if (!(fc_ks = kstat_lookup(kc, "socal", socal_inst,
						"statistics"))) {
			(void) fprintf(stdout,
				MSGSTR(2079,
			" Error: could not lookup driver stats for socal%d\n"),
				socal_inst);
			exit(-1);
		}
		if (kstat_read(kc, sf_ks, &sf_stats) < 0) {
			(void) fprintf(stdout,
				MSGSTR(2080,
			" Error: could not read driver stats for sf%d\n"),
				drvr_inst);
			exit(-1);
		}
		if (kstat_read(kc, fc_ks, &fc_stats) < 0) {
			(void) fprintf(stdout,
				MSGSTR(2081,
			" Error: could not read driver stats for socal%d\n"),
				socal_inst);
			exit(-1);
		}
		(void) display_socal_stats(port, socal_path, &fc_stats);
		(void) display_sf_stats(path_phys, dtype, &sf_stats);

	    } else if (path_type & FC_FCA_MASK) {
		/*
		 * XXX -- If the driver folks ever implement
		 * an fp_stats structure and implement the kstat_lookup
		 * code then we could grab that information as well.
		 * For now, this feature doesn't exist so we just
		 * omit that output.  (The structure is somewhat
		 * private loop centric so this may never happen)
		 */
		if (!(fc_ks = kstat_lookup(kc, "usoc", socal_inst,
						"statistics"))) {
			(void) fprintf(stdout,
				MSGSTR(2226,
			" Error: could not lookup driver stats for usoc%d\n"),
				socal_inst);
			exit(-1);
		}
		if (kstat_read(kc, fc_ks, &usoc_stats) < 0) {
			(void) fprintf(stdout,
				MSGSTR(2227,
			" Error: could not read driver stats for usoc%d\n"),
				socal_inst);
			exit(-1);
		}
		(void) display_usoc_stats(port, socal_path, &usoc_stats);
	    }
	}
	(void) kstat_close(kc);

}



/*ARGSUSED*/
/*
 * adm_display_verbose_disk() Gets the mode page information
 * for a SENA disk and prints that information.
 *
 * RETURNS:
 *	none.
 */
static void
adm_display_verbose_disk(char *path, int verbose)
{
uchar_t		*pg_buf;
Mode_header_10	*mode_header_ptr;
Mp_01		*pg1_buf;
Mp_04		*pg4_buf;
struct mode_page *pg_hdr;
int		offset, hdr_printed = 0, err = 0;

	if ((err = l_get_mode_pg(path, &pg_buf, verbose)) == 0) {

		mode_header_ptr = (struct mode_header_10_struct *)(int)pg_buf;
		pg_hdr = ((struct mode_page *)((int)pg_buf +
		    (uchar_t)sizeof (struct mode_header_10_struct) +
		    (uchar_t *)(mode_header_ptr->bdesc_length)));
		offset = sizeof (struct mode_header_10_struct) +
		    mode_header_ptr->bdesc_length;
		while (offset < (mode_header_ptr->length +
			sizeof (mode_header_ptr->length))) {
			switch (pg_hdr->code) {
				case 0x01:
				pg1_buf = (struct mode_page_01_struct *)
					(int)pg_hdr;
				P_DPRINTF("  adm_display_verbose_disk:"
					"Mode Sense page 1 found.\n");
				if (hdr_printed++ == 0) {
					(void) fprintf(stdout,
						MSGSTR(2089,
						"  Mode Sense data:\n"));
				}
				(void) fprintf(stdout,
					MSGSTR(2090,
					"    AWRE:\t\t\t%d\n"
					"    ARRE:\t\t\t%d\n"
					"    Read Retry Count:\t\t"
					"%d\n"
					"    Write Retry Count:\t\t"
					"%d\n"),
					pg1_buf->awre,
					pg1_buf->arre,
					pg1_buf->read_retry_count,
					pg1_buf->write_retry_count);
				break;
				case MODEPAGE_GEOMETRY:
				pg4_buf = (struct mode_page_04_struct *)
					(int)pg_hdr;
				P_DPRINTF("  adm_display_verbose_disk:"
					"Mode Sense page 4 found.\n");
				if (hdr_printed++ == 0) {
					(void) fprintf(stdout,
						MSGSTR(2091,
						"  Mode Sense data:\n"));
				}
				if (pg4_buf->rpm) {
					(void) fprintf(stdout,
						MSGSTR(2092,
						"    Medium rotation rate:\t"
						"%d RPM\n"), pg4_buf->rpm);
				}
				break;
			}
			offset += pg_hdr->length + sizeof (struct mode_page);
			pg_hdr = ((struct mode_page *)((int)pg_buf +
				(uchar_t)offset));
		}





	} else if (getenv("_LUX_P_DEBUG") != NULL) {
			(void) print_errString(err, path);
	}
}



/*
 * For leadville specific HBAs only.
 * Gets the HBAs information and prints
 * to the stdout.
 *
 * RETURNS:
 *	0	 if OK
 *	non-zero otherwise
 */
int
prt_host_params(char *ppath, int num, int verbose)
{
int		err, fd;
struct stat	stbuf;
char		*ptr, drvr_path[MAXPATHLEN];
fc_port_dev_t	host_params;

	/*
	 * create path to the port driver and
	 * issue FCIO ioctl to the fp (port) driver.
	 */
	(void) strcpy(drvr_path, ppath);
	if (strstr(drvr_path, DRV_NAME_SSD) || strstr(drvr_path, SES_NAME)) {
		if ((ptr = strrchr(drvr_path, '/')) == NULL) {
			return (L_INVALID_PATH);
		}
		*ptr = '\0';   /* Terminate sting  */
		/* append controller */
		(void) strcat(drvr_path, FC_CTLR);
	} else {
		if (stat(drvr_path, &stbuf) < 0) {
			return (L_LSTAT_ERROR);
		}
		if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
			/* append controller */
			(void) strcat(drvr_path, FC_CTLR);
		}
	}
	/* open port driver path */
	if ((fd = open(drvr_path, O_RDONLY)) < 0) {
		return (L_OPEN_PATH_FAIL);
	}

	/* get the host parameters from the port driver */
	if ((err = g_get_host_params(fd, &host_params,
				(Options & PVERBOSE))) != 0) {
		(void) close(fd);
		return (err);
	}

	/* print the HBAs information here */
	(void) fprintf(stdout,
		"%-3d   %-2x  %-2x    %-2x     "
		"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x"
		" %1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x ",
		num, (uchar_t)host_params.dev_did.port_id,
	g_sf_alpa_to_switch[(uchar_t)host_params.dev_did.port_id],
		(uchar_t)host_params.dev_hard_addr.hard_addr,
		host_params.dev_pwwn.raw_wwn[0],
		host_params.dev_pwwn.raw_wwn[1],
		host_params.dev_pwwn.raw_wwn[2],
		host_params.dev_pwwn.raw_wwn[3],
		host_params.dev_pwwn.raw_wwn[4],
		host_params.dev_pwwn.raw_wwn[5],
		host_params.dev_pwwn.raw_wwn[6],
		host_params.dev_pwwn.raw_wwn[7],

		host_params.dev_nwwn.raw_wwn[0],
		host_params.dev_nwwn.raw_wwn[1],
		host_params.dev_nwwn.raw_wwn[2],
		host_params.dev_nwwn.raw_wwn[3],
		host_params.dev_nwwn.raw_wwn[4],
		host_params.dev_nwwn.raw_wwn[5],
		host_params.dev_nwwn.raw_wwn[6],
		host_params.dev_nwwn.raw_wwn[7]);
	(void) fprintf(stdout,
		"0x%-2x (Unknown Type)\n", host_params.dev_dtype);

	(void) close(fd);
	return (0);
}



/*
 * Get the device map from
 * fc nexus driver and prints the map.
 *
 * RETURNS:
 *	none.
 */
static void
dump_map(char **argv)
{
int		i = 0, path_index = 0;
int		limited_map_flag = 0, err = 0;
sf_al_map_t	map;
char		*path_phys = NULL;
Path_struct	*path_struct;
struct lilpmap	limited_map;
uint_t		dev_type;

	while (argv[path_index] != NULL) {
		if ((err = l_convert_name(argv[path_index], &path_phys,
			&path_struct, Options & PVERBOSE)) != 0) {
			(void) fprintf(stdout,
				MSGSTR(33,
					" Error: converting"
					" %s to physical path.\n"
					" Invalid pathname.\n"),
				argv[path_index]);
			if (err != -1) {
				(void) print_errString(err, argv[path_index]);
			}
			exit(-1);
		}
		if ((dev_type = g_get_path_type(path_phys)) == 0) {
			(void) print_errString(L_INVALID_PATH,
						argv[path_index]);
			exit(-1);
		}

		if (err = g_get_dev_map(path_phys, &map,
					(Options & PVERBOSE))) {
			if (dev_type & FC_FCA_MASK) {
				(void) print_errString(err, argv[path_index]);
				exit(-1);
			} else {
				/*
				 * This did not work so try the FCIO_GETMAP
				 * type ioctl.
				 */
				if (err = g_get_limited_map(path_phys,
					&limited_map, (Options & PVERBOSE))) {
					(void) print_errString(err,
							argv[path_index]);
					exit(-1);
				}
				limited_map_flag++;
			}

		}

		if (limited_map_flag) {
			(void) fprintf(stdout,
				MSGSTR(2093,
				"Host Adapter AL_PA: %x\n"),
				limited_map.lilp_myalpa);

			(void) fprintf(stdout,
				MSGSTR(2094,
				"Pos AL_PA\n"));
			for (i = 0; i < (uint_t)limited_map.lilp_length; i++) {
				(void) fprintf(stdout, "%-3d   %-2x\n",
					i, limited_map.lilp_list[i]);
			}
		} else {
			/*
			 * print the device list information here.
			 * For leadville devices, only the device list
			 * be printed except the HBAs information unlike
			 * for the non-leadville device map.
			 */
			(void) fprintf(stdout,
				MSGSTR(2095,
				"Pos AL_PA ID Hard_Addr "
				"Port WWN         Node WWN         Type\n"));
			for (i = 0; i < map.sf_count; i++) {
				(void) fprintf(stdout,
				"%-3d   %-2x  %-2x    %-2x     "
				"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x"
				" %1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x ",
				i, map.sf_addr_pair[i].sf_al_pa,
			g_sf_alpa_to_switch[map.sf_addr_pair[i].sf_al_pa],
				map.sf_addr_pair[i].sf_hard_address,
				map.sf_addr_pair[i].sf_port_wwn[0],
				map.sf_addr_pair[i].sf_port_wwn[1],
				map.sf_addr_pair[i].sf_port_wwn[2],
				map.sf_addr_pair[i].sf_port_wwn[3],
				map.sf_addr_pair[i].sf_port_wwn[4],
				map.sf_addr_pair[i].sf_port_wwn[5],
				map.sf_addr_pair[i].sf_port_wwn[6],
				map.sf_addr_pair[i].sf_port_wwn[7],

				map.sf_addr_pair[i].sf_node_wwn[0],
				map.sf_addr_pair[i].sf_node_wwn[1],
				map.sf_addr_pair[i].sf_node_wwn[2],
				map.sf_addr_pair[i].sf_node_wwn[3],
				map.sf_addr_pair[i].sf_node_wwn[4],
				map.sf_addr_pair[i].sf_node_wwn[5],
				map.sf_addr_pair[i].sf_node_wwn[6],
				map.sf_addr_pair[i].sf_node_wwn[7]);

				if (map.sf_addr_pair[i].sf_inq_dtype < 0x10) {
					(void) fprintf(stdout,
						"0x%-2x (%s)\n",
					map.sf_addr_pair[i].sf_inq_dtype,
				dtype[map.sf_addr_pair[i].sf_inq_dtype]);
				} else if (map.sf_addr_pair[i].sf_inq_dtype
								< 0x1f) {
					(void) fprintf(stdout, MSGSTR(2096,
						"0x%-2x (Reserved)\n"),
					map.sf_addr_pair[i].sf_inq_dtype);
				} else {
					(void) fprintf(stdout, MSGSTR(2097,
						"0x%-2x (Unknown Type)\n"),
					map.sf_addr_pair[i].sf_inq_dtype);
				}
			}
			if (dev_type & FC_FCA_MASK) {
				if ((err = prt_host_params(path_phys, i,
					(Options & PVERBOSE))) != 0) {
					(void) print_errString(err,
							argv[path_index]);
					exit(-1);
				}
			}
		}
		limited_map_flag = 0;
		path_index++;
	}

}



/*
 * Gets a list of non-SENA fcal devices
 * found on the system.
 *
 * OUTPUT:
 *	wwn_list pointer
 *			NULL: No non-enclosure devices found.
 *			!NULL: Devices found
 *                      wwn_list points to a linked list of wwn's.
 * RETURNS:
 *	0	O.K.
 */
int
n_get_non_encl_list(WWN_list **wwn_list_ptr, int verbose)
{
int		i, j, err, found_ib = 0;
WWN_list	*wwn_list;
Box_list	*b_list = NULL;
sf_al_map_t	map;
uchar_t		box_id;

	/* Get path to all the FC disk and tape devices. */
	if (err = g_get_wwn_list(&wwn_list, verbose)) {
		return (err);	/* Failure */
	}

	/*
	 * Only interested in devices that are not part of
	 * a Photon enclosure.
	 */
	if ((err = l_get_box_list(&b_list, verbose)) != 0) {
		(void) g_free_wwn_list(&wwn_list);
		return (err);	/* Failure */
	}

	while (b_list != NULL) {
		if ((err = g_get_dev_map(b_list->b_physical_path,
			&map, verbose)) != 0) {
			(void) g_free_wwn_list(&wwn_list);
			(void) l_free_box_list(&b_list);
			return (err);
		}
		for (i = 0; i < map.sf_count; i++) {
			for (found_ib = 1, j = 0; j < WWN_SIZE; j++) {
				if (b_list->b_node_wwn[j] !=
					map.sf_addr_pair[i].sf_node_wwn[j]) {
					found_ib = 0;
				}
			}
			if (found_ib) {
				box_id = g_sf_alpa_to_switch
				[map.sf_addr_pair[i].sf_al_pa] & BOX_ID_MASK;
				/*
				 * This function has been added
				 * here only to keep from having
				 * to tab over farther.
				 */
				(void) n_rem_list_entry(box_id, &map,
					&wwn_list);
				if (wwn_list == NULL) {
					/* Return the list */
					*wwn_list_ptr = NULL;
					break;
				}
			}
		}
		b_list = b_list->box_next;
	}
	/* Return the list */
	*wwn_list_ptr = wwn_list;
	(void) l_free_box_list(&b_list);
	return (0);
}



/*
 * n_rem_list_entry() We found an IB so remove disks that
 * are in the Photon from the individual device list.
 *
 * OUTPUT:
 *	wwn_list - removes the fcal disks that are in SENA enclosure
 *
 * RETURNS:
 *	none
 */
static void
n_rem_list_entry(uchar_t box_id,  struct sf_al_map *map,
	struct	wwn_list_struct **wwn_list)
{
int		k, l, found_dev;
WWN_list	*inner, *l1;

	N_DPRINTF("  n_rem_list_entry: Removing devices"
		" with box_id=0x%x from device list.\n", box_id);
	for (k = 0; k < map->sf_count; k++) {
		if ((g_sf_alpa_to_switch[map->sf_addr_pair[k].sf_hard_address] &
				BOX_ID_MASK) == box_id) {
			inner = *wwn_list;
			while (inner != NULL) {
				for (found_dev = 1, l = 0; l < WWN_SIZE; l++) {
					if (inner->w_node_wwn[l] !=
					map->sf_addr_pair[k].sf_node_wwn[l]) {
					found_dev = 0;
					}
				}
				if (found_dev) {
					/* Remove this entry from the list */
					if (inner->wwn_prev != NULL) {
						inner->wwn_prev->wwn_next =
							inner->wwn_next;
					} else {
						*wwn_list = inner->wwn_next;
					}
					if (inner->wwn_next != NULL) {
						inner->wwn_next->wwn_prev =
							inner->wwn_prev;
					}
					l1 = inner;
					N_DPRINTF("  n_rem_list_entry: "
						"Removing Logical=%s "
						"Current=0x%x, "
						"Prev=0x%x, Next=0x%x\n",
						l1->logical_path,
						l1,
						l1->wwn_prev,
						l1->wwn_next);
					inner = inner->wwn_next;
					if ((l1->wwn_prev == NULL) &&
						(l1->wwn_next) == NULL) {
						(void) free(l1);
						*wwn_list = NULL;
						N_DPRINTF("  n_rem_list_entry: "
						"No non-Photon "
						"devices left"
						" in the list.\n");
						return;
					}
					(void) free(l1);
				} else {
					inner = inner->wwn_next;
				}
			}
		}
	}

}



static void
ssa_probe()
{


}


/*
 * non_encl_probe() Finds and displays a list of
 * non-SENA fcal devices which is found on the
 * system.
 *
 * RETURNS:
 *	none.
 */
static void
non_encl_probe()
{
WWN_list	*wwn_list, *inner, *l1;
int		err = 0;

	if (err = n_get_non_encl_list(&wwn_list, (Options & PVERBOSE))) {
		(void) print_errString(err, NULL);
		exit(-1);
	}

	if (wwn_list != NULL) {
		if (wwn_list->wwn_next != NULL) {
			(void) fprintf(stdout,
			    MSGSTR(2098, "\nFound Fibre Channel device(s):\n"));
		} else {
			(void) fprintf(stdout,
			    MSGSTR(2099, "\nFound Fibre Channel device:\n"));
		}
	} else {
		return;
	}

	while (wwn_list != NULL) {
		(void) fprintf(stdout, "  ");
		(void) fprintf(stdout, MSGSTR(90, "Node WWN:"));
		(void) fprintf(stdout, "%s  ",
			wwn_list->node_wwn_s);
		if (wwn_list->device_type < 0x10) {
			(void) fprintf(stdout, MSGSTR(35, "Device Type:"));
			(void) fprintf(stdout, "%s",
			dtype[wwn_list->device_type]);
		} else if (wwn_list->device_type < 0x1f) {
			(void) fprintf(stdout, MSGSTR(2100,
			"Type:Reserved"));
		} else {
			(void) fprintf(stdout, MSGSTR(2101,
			"Type:Unknown"));
		}
		(void) fprintf(stdout, "\n    ");
		(void) fprintf(stdout, MSGSTR(31,
			"Logical Path:%s"),
			wwn_list->logical_path);
		(void) fprintf(stdout, "\n");
		if (Options & OPTION_P) {
			(void) fprintf(stdout, "    ");
			(void) fprintf(stdout,
			MSGSTR(5, "Physical Path:"));
			(void) fprintf(stdout, "\n     %s\n",
			wwn_list->physical_path);
		}
		inner = wwn_list->wwn_next;
		while (inner != NULL) {
			if (strcmp(inner->node_wwn_s,
				wwn_list->node_wwn_s) == 0) {
				(void) fprintf(stdout, "    ");
				(void) fprintf(stdout, MSGSTR(31,
					"Logical Path:%s"),
					inner->logical_path);
				(void) fprintf(stdout, "\n");
				if (Options & OPTION_P) {
					(void) fprintf(stdout, "    ");
					(void) fprintf(stdout,
					MSGSTR(5, "Physical Path:"));
					(void) fprintf(stdout, "\n     %s\n",
					inner->physical_path);
				}
				/* Remove this entry from the list */
				if (inner->wwn_prev != NULL) {
					inner->wwn_prev->wwn_next =
						inner->wwn_next;
				}
				if (inner->wwn_next != NULL) {
					inner->wwn_next->wwn_prev =
						inner->wwn_prev;
				}
				l1 = inner;
				inner = inner->wwn_next;
				(void) free(l1);
			} else {
				inner = inner->wwn_next;
			}
		}
		wwn_list = wwn_list->wwn_next;
	}
	(void) g_free_wwn_list(&wwn_list);
}



static void
pho_probe()
{

Box_list	*b_list, *o_list, *c_list;
int		multi_path_flag, multi_print_flag;
int		duplicate_names_found = 0, err = 0;

	b_list = o_list = c_list = NULL;
	if ((err = l_get_box_list(&b_list, Options & PVERBOSE)) != 0) {
		(void) print_errString(err, NULL);
		exit(-1);
	}
	if (b_list == NULL) {
		(void) fprintf(stdout,
			MSGSTR(93, "No %s enclosures found "
			"in /dev/es\n"), ENCLOSURE_PROD_NAME);
	} else {
		o_list = b_list;
		if (b_list->box_next != NULL) {
			(void) fprintf(stdout, MSGSTR(2102,
				"Found Enclosure(s)"));
		} else {
			(void) fprintf(stdout, MSGSTR(2103, "Found Enclosure"));
		}
		(void) fprintf(stdout, ":\n");
		while (b_list != NULL) {
			/* Don't re-print multiple paths */
			c_list = o_list;
			multi_print_flag = 0;
			while (c_list != b_list) {
				if (strcmp(c_list->b_node_wwn_s,
					b_list->b_node_wwn_s) == 0) {
					multi_print_flag = 1;
					break;
				}
				c_list = c_list->box_next;
			}
			if (multi_print_flag) {
				b_list = b_list->box_next;
				continue;
			}
			(void) fprintf(stdout,
			MSGSTR(2104, "%s   Name:%s   Node WWN:%s   "),
			b_list->prod_id_s, b_list->b_name,
				b_list->b_node_wwn_s);
			/*
			 * Print logical path on same line if not multipathed.
			 */
			multi_path_flag = 0;
			c_list = o_list;
			while (c_list != NULL) {
				if ((c_list != b_list) &&
					(strcmp(c_list->b_node_wwn_s,
					b_list->b_node_wwn_s) == 0)) {
					multi_path_flag = 1;
				}
				c_list = c_list->box_next;
			}
			if (multi_path_flag) {
				(void) fprintf(stdout, "\n  ");
			}
			(void) fprintf(stdout,
			MSGSTR(31, "Logical Path:%s"),
			b_list->logical_path);

			if (Options & OPTION_P) {
				(void) fprintf(stdout, "\n  ");
				(void) fprintf(stdout,
				MSGSTR(5, "Physical Path:"));
				(void) fprintf(stdout, "%s",
				b_list->b_physical_path);
			}
			c_list = o_list;
			while (c_list != NULL) {
				if ((c_list != b_list) &&
				(strcmp(c_list->b_node_wwn_s,
					b_list->b_node_wwn_s) == 0)) {
					(void) fprintf(stdout, "\n  ");
					(void) fprintf(stdout,
					MSGSTR(31, "Logical Path:%s"),
					c_list->logical_path);
					if (Options & OPTION_P) {
						(void) fprintf(stdout, "\n  ");
						(void) fprintf(stdout,
						MSGSTR(5, "Physical Path:"));
						(void) fprintf(stdout, "%s",
						c_list->b_physical_path);
					}
				}
				c_list = c_list->box_next;
			}
			(void) fprintf(stdout, "\n");
			/* Check for duplicate names */
			if (l_duplicate_names(o_list, b_list->b_node_wwn_s,
				(char *)b_list->b_name,
				Options & PVERBOSE)) {
				duplicate_names_found++;
			}
			b_list = b_list->box_next;
		}
	}
	if (duplicate_names_found) {
		(void) fprintf(stdout,
			MSGSTR(2105, "\nWARNING: There are enclosures with "
			"the same names.\n"
			"You can not use the \"enclosure\""
			" name to specify these subsystems.\n"
			"Please use the \"enclosure_name\""
			" subcommand to select unique names.\n\n"));
	}
	(void) l_free_box_list(&b_list);
}


/*
 * display_port_status() Prints the device's
 * port status.
 *
 * RETURNS:
 *	none.
 */
static	void
display_port_status(int d_state_flag)
{

	if (d_state_flag & L_OPEN_FAIL) {
		(void) fprintf(stdout, MSGSTR(28, "Open Failed"));
	} else if (d_state_flag & L_NOT_READY) {
		(void) fprintf(stdout, MSGSTR(20, "Not Ready"));
	} else if (d_state_flag & L_NOT_READABLE) {
		(void) fprintf(stdout, MSGSTR(88, "Not Readable"));
	} else if (d_state_flag & L_SPUN_DWN_D) {
		(void) fprintf(stdout, MSGSTR(68, "Spun Down"));
	} else if (d_state_flag & L_SCSI_ERR) {
		(void) fprintf(stdout, MSGSTR(70, "SCSI Error"));
	} else if (d_state_flag & L_RESERVED) {
		(void) fprintf(stdout, MSGSTR(73, "Reservation conflict"));
	} else if (d_state_flag & L_NO_LABEL) {
		(void) fprintf(stdout, MSGSTR(92, "No UNIX Label"));
	} else {
		(void) fprintf(stdout, MSGSTR(29, "O.K."));
	}
	(void) fprintf(stdout, "\n");
}

/*
 * Displays individual SENA
 * FC disk information.
 *
 * RETURNS:
 *	none.
 */
static void
display_fc_disk(struct path_struct *path_struct, char *ses_path,
	sf_al_map_t *map, L_inquiry inq, int verbose)
{
static WWN_list		*wwn_list = NULL;
static char		path_phys[MAXPATHLEN];
static L_disk_state	l_disk_state;
static L_inquiry	local_inq;
static uchar_t		node_wwn[WWN_SIZE];
char			same_path_phys = B_FALSE; /* To chk for repeat args */
uchar_t			port_wwn[WWN_SIZE], *pg_buf;
char			logical_path[MAXPATHLEN];
int			al_pa, port_a_flag = 0;
int			offset, mode_data_avail = 0;
int			no_path_flag = 0, err = 0;
L_state			l_state;
Mode_header_10		*mode_header_ptr = NULL;
struct mode_page	*pg_hdr;

	/*
	 * Do a quick check to see if its the same path as in last call.
	 * path_phys is a static array and so dont worry about its
	 * initialization.
	 */
	if (strcmp(path_phys, path_struct->p_physical_path) == 0)
		same_path_phys = B_TRUE;

	(void) strcpy(path_phys, path_struct->p_physical_path);
	(void) memset((char *)logical_path, 0, sizeof (logical_path));

	/*
	 * slot_valid is 1 when argument is of the form 'enclosure,[f|r]<n>'.
	 * If slot_valid != 1, g_get_dev_map and l_get_ses_path would
	 * already have been called
	 */
	if (path_struct->slot_valid == 1) {
		/* Get the location information. */
		if (err = g_get_dev_map(path_phys, map, (Options & PVERBOSE))) {
			(void) print_errString(err, path_phys);
			exit(-1);
		}
		if (err = l_get_ses_path(path_phys, ses_path, map,
			(Options & PVERBOSE))) {
			(void) print_errString(err, path_phys);
			exit(-1);
		}
	}

	/*
	 * Get the WWN for our disk if we already haven't or if there was an
	 * error earlier
	 */
	if (same_path_phys == B_FALSE) {
		if (err = g_get_wwn(path_phys, port_wwn, node_wwn,
			&al_pa, (Options & PVERBOSE))) {
			(void) print_errString(err, path_phys);
			exit(-1);
		}

		if (err = g_get_inquiry(ses_path, &local_inq)) {
			(void) print_errString(err, ses_path);
			exit(-1);
		}
	}

	/*
	 * We are interested only a couple of ib_tbl fields and
	 * those get filled using l_get_ib_status.
	 * Note that NOT ALL of ib_tbl fields get filled here
	 */
	if ((err = l_get_ib_status(ses_path, &l_state,
				Options & PVERBOSE)) != 0) {
		(void) print_errString(err, ses_path);
		exit(-1);
	}

	/*
	 * Get path to all the FC disk and tape devices.
	 * if we haven't already done so in a previous pass
	 */
	if ((wwn_list == NULL) && (err = g_get_wwn_list(&wwn_list, verbose))) {
		(void) print_errString(err, ses_path);
		exit(-1);   /* Failure */
	}

	/*
	 * Get the disk status if it is a different path_phys from
	 * last time.
	 */
	if (same_path_phys == B_FALSE) {
		(void) memset(&l_disk_state, 0,
				sizeof (struct l_disk_state_struct));
		if (err = l_get_disk_status(path_phys, &l_disk_state,
				wwn_list, (Options & PVERBOSE))) {
			(void) print_errString(err, path_phys);
			exit(-1);
		}
	}

	if (l_disk_state.l_state_flag & L_NO_PATH_FOUND) {
		(void) fprintf(stderr, MSGSTR(2106,
			"\nWARNING: No path found "
			"in /dev/rdsk directory\n"
			"  Please check the logical links in /dev/rdsk\n"
			"  (It may be necessary to run the \"disks\" "
			"program.)\n\n"));

		/* Just call to get the status directly. */
		if (err = l_get_port(ses_path, &port_a_flag, verbose)) {
			(void) print_errString(err, ses_path);
			exit(-1);
		}
		if (err = l_get_disk_port_status(path_phys,
			&l_disk_state, port_a_flag,
			(Options & PVERBOSE))) {
			(void) print_errString(err, path_phys);
			exit(-1);
		}
		no_path_flag++;
	}

	if (strlen(l_disk_state.g_disk_state.node_wwn_s) == 0) {
		(void) sprintf(l_disk_state.g_disk_state.node_wwn_s,
			"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			node_wwn[0], node_wwn[1], node_wwn[2], node_wwn[3],
			node_wwn[4], node_wwn[5], node_wwn[6], node_wwn[7]);
	}

	/* get mode page information for FC device */
	if (l_get_mode_pg(path_phys, &pg_buf, Options & PVERBOSE) == 0) {
		mode_header_ptr = (struct mode_header_10_struct *)(int)pg_buf;
		pg_hdr = ((struct mode_page *)((int)pg_buf +
			(uchar_t)sizeof (struct mode_header_10_struct) +
			(uchar_t *)(mode_header_ptr->bdesc_length)));
		offset = sizeof (struct mode_header_10_struct) +
			mode_header_ptr->bdesc_length;
		while (offset < (mode_header_ptr->length +
			sizeof (mode_header_ptr->length)) &&
						!mode_data_avail) {
			if (pg_hdr->code == MODEPAGE_CACHING) {
				mode_data_avail++;
				break;
			}
			offset += pg_hdr->length + sizeof (struct mode_page);
			pg_hdr = ((struct mode_page *)((int)pg_buf +
				(uchar_t)offset));
		}
	}

	(void) fprintf(stdout,
		MSGSTR(121, "DEVICE PROPERTIES for disk: %s\n"),
			path_struct->argv);
	if (l_disk_state.g_disk_state.port_a_valid) {
		(void) fprintf(stdout, "  ");
		(void) fprintf(stdout, MSGSTR(141, "Status(Port A):"));
		(void) fprintf(stdout, "\t");
		display_port_status(
			l_disk_state.g_disk_state.d_state_flags[PORT_A]);
	} else {
		if (path_struct->f_flag) {
			if ((ib_present_chk(&l_state, 0) == 1) &&
		(l_state.drv_front[path_struct->slot].ib_status.bypass_a_en)) {
				(void) fprintf(stdout,
					MSGSTR(66,
					"  Status(Port A):\tBYPASSED\n"));
			}
		} else {
			if ((ib_present_chk(&l_state, 0) == 1) &&
		(l_state.drv_rear[path_struct->slot].ib_status.bypass_a_en)) {
				(void) fprintf(stdout,
					MSGSTR(66,
					"  Status(Port A):\tBYPASSED\n"));
			}
		}
	}

	if (l_disk_state.g_disk_state.port_b_valid) {
		(void) fprintf(stdout, "  ");
		(void) fprintf(stdout, MSGSTR(142, "Status(Port B):"));
		(void) fprintf(stdout, "\t");
	display_port_status(l_disk_state.g_disk_state.d_state_flags[PORT_B]);
	} else {
		if (path_struct->f_flag) {
			if ((ib_present_chk(&l_state, 1) == 1) &&
		(l_state.drv_front[path_struct->slot].ib_status.bypass_b_en)) {
				(void) fprintf(stdout,
					MSGSTR(65,
					"  Status(Port B):\tBYPASSED\n"));
			}
		} else {
			if ((ib_present_chk(&l_state, 1) == 1) &&
		(l_state.drv_rear[path_struct->slot].ib_status.bypass_b_en)) {
				(void) fprintf(stdout,
					MSGSTR(65,
					"  Status(Port B):\tBYPASSED\n"));
			}
		}
	}

	if (no_path_flag) {
		(void) fprintf(stdout, "  ");
		if (port_a_flag != NULL) {
			(void) fprintf(stdout, MSGSTR(142, "Status(Port B):"));
		} else {
			(void) fprintf(stdout, MSGSTR(141, "Status(Port A):"));
		}
		(void) fprintf(stdout, "\t");
		display_port_status(
		l_disk_state.g_disk_state.d_state_flags[port_a_flag]);
	} else if ((!l_disk_state.g_disk_state.port_a_valid) &&
			(!l_disk_state.g_disk_state.port_b_valid)) {
		(void) fprintf(stdout, MSGSTR(2107, "  Status:\t\t"
		"No state available.\n"));
	}

	(void) display_disk_info(inq, l_disk_state, path_struct, pg_hdr,
		mode_data_avail, (char *)local_inq.inq_box_name, verbose);
}


/*
 *	DISPLAY function
 *
 * RETURNS:
 *	0	O.K.
 */
static int
adm_display_config(char **argv, int option_t_input, int argc)
{
L_inquiry	inq;
int		i, slot, f_r, path_index = 0, err = 0;
sf_al_map_t	map;
Path_struct	*path_struct;
char		*path_phys = NULL, *ptr;
char		ses_path[MAXPATHLEN], inq_path[MAXNAMELEN];


	while (argv[path_index] != NULL) {
	    VERBPRINT(MSGSTR(2108, "  Displaying information for: %s\n"),
			argv[path_index]);
	    if ((err = l_convert_name(argv[path_index], &path_phys,
		&path_struct, Options & PVERBOSE)) != 0) {
		(void) strcpy(inq_path, argv[path_index]);
		if (((ptr = strstr(inq_path, ",")) != NULL) &&
			((*(ptr + 1) == 'f') || (*(ptr + 1) == 'r'))) {
			if (err != -1) {
				(void) print_errString(err, argv[path_index]);
				path_index++;
				continue;
			}
			*ptr = NULL;
			slot = path_struct->slot;
			f_r = path_struct->f_flag;
			if ((err = l_convert_name(inq_path, &path_phys,
				&path_struct, Options & PVERBOSE)) != 0) {
				(void) fprintf(stdout,
					MSGSTR(33,
					" Error: converting"
					" %s to physical path.\n"
					" Invalid pathname.\n"),
					argv[path_index]);
				if (err != -1) {
					(void) print_errString(err,
							argv[path_index]);
				}
				path_index++;
				continue;
			}

			if ((err = print_devState(argv[path_index],
				path_struct->p_physical_path,
				f_r, slot, Options & PVERBOSE)) != 0) {
				path_index++;
				continue;
			}
		} else {
			if (err != -1) {
				(void) print_errString(err, argv[path_index]);
			} else {
				(void) fprintf(stdout, "\n ");
				(void) fprintf(stdout,
					MSGSTR(112,
					"Error: Invalid pathname (%s)"),
					argv[path_index]);
				(void) fprintf(stdout, "\n");
			}
		}
		path_index++;
		continue;
	    }

	/*
	 * See what kind of device we are talking to.
	 */
	    if (g_get_inquiry(path_phys, &inq)) {
		/* INQUIRY failed - try ssa display */
		ssa_cli_display_config(argv, path_phys,
			option_t_input, 0, argc);
	    } else if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) != 0) ||
			(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) &&
			(inq.inq_dtype == DTYPE_ESI))) {

		/*
		 * Display SENA enclosure.
		 */
		(void) fprintf(stdout, "\n\t\t\t\t   ");
		for (i = 0; i < sizeof (inq.inq_pid); i++) {
			(void) fprintf(stdout, "%c", inq.inq_pid[i]);
		}

		(void) fprintf(stdout, "\n");
		if (Options & OPTION_R) {
			adm_display_err(path_phys,
				inq.inq_dtype, Options & PVERBOSE);
		} else {
			pho_display_config(path_phys);
		}

	    } else if (strstr((char *)inq.inq_pid, "SSA") != 0) {
		ssa_cli_display_config(argv, path_phys,
			option_t_input, 0, argc);

		/* if the device is in SSA. */
	    } else if ((inq.inq_dtype == DTYPE_DIRECT) &&
		(g_get_dev_map(path_phys, &map, (Options & PVERBOSE)) != 0)) {
			ssa_cli_display_config(argv, path_phys,
				option_t_input, 0, argc);

		/* if device is in SENA enclosure */
	    } else if ((inq.inq_dtype == DTYPE_DIRECT) &&
			((path_struct->slot_valid == 1) ||
			((g_get_dev_map(path_phys,
				&map, (Options & PVERBOSE)) == 0) &&
				(l_get_ses_path(path_phys, ses_path,
				&map, Options & PVERBOSE) == 0)))) {
		if (Options & OPTION_R) {
			adm_display_err(path_phys,
			inq.inq_dtype, Options & PVERBOSE);
		} else {
			display_fc_disk(path_struct, ses_path, &map, inq,
							Options & PVERBOSE);
		}

	    } else if (strstr((char *)inq.inq_pid, "SUN_SEN") != 0) {
			if (strcmp(argv[path_index], path_phys) != 0) {
				(void) fprintf(stdout, "  ");
				(void) fprintf(stdout,
				MSGSTR(5, "Physical Path:"));
				(void) fprintf(stdout, "\n  %s\n", path_phys);
			}
			(void) fprintf(stdout, MSGSTR(2109, "DEVICE is a "));
			for (i = 0; i < sizeof (inq.inq_vid); i++) {
				(void) fprintf(stdout, "%c", inq.inq_vid[i]);
			}
			(void) fprintf(stdout, " ");
			for (i = 0; i < sizeof (inq.inq_pid); i++) {
				(void) fprintf(stdout, "%c", inq.inq_pid[i]);
			}
			(void) fprintf(stdout, MSGSTR(2110, " card."));
			if (inq.inq_len > 31) {
				(void) fprintf(stdout, "   ");
				(void) fprintf(stdout, MSGSTR(26, "Revision:"));
				(void) fprintf(stdout, " ");
				for (i = 0; i < sizeof (inq.inq_revision);
					i++) {
					(void) fprintf(stdout, "%c",
					inq.inq_revision[i]);
				}
			}
			(void) fprintf(stdout, "\n");
		/* if device is not in SENA or SSA enclosures. */
	    } else if (inq.inq_dtype < 0x10) {
		switch (inq.inq_dtype) {
			case 0x00:
				if (Options & OPTION_R) {
					adm_display_err(path_phys,
					inq.inq_dtype, Options & PVERBOSE);
				} else if (non_encl_fc_disk_display(path_struct,
					inq, Options & PVERBOSE) != 0) {
					(void) fprintf(stderr,
						MSGSTR(2111,
						"Error: getting the device"
						" information.\n"));
				}
				break;
			/* case 0x01: same as default */
			default:
				(void) fprintf(stdout, "  ");
				(void) fprintf(stdout, MSGSTR(35,
						"Device Type:"));
				(void) fprintf(stdout, "%s\n",
					dtype[inq.inq_dtype]);
				break;
		}
	    } else if (inq.inq_dtype < 0x1f) {
			(void) fprintf(stdout,
				MSGSTR(2112, "  Device type: Reserved"));
	    } else {
			(void) fprintf(stdout,
				MSGSTR(2113, "  Device type: Unknown device"));
	    }
	    path_index++;
	    (void) free(path_struct);
	}
	return (err);
}



/*
 * non_encl_fc_disk_display() Prints the device specific
 * information for an individual fcal device.
 *
 * RETURNS:
 *	none.
 */
static int
non_encl_fc_disk_display(Path_struct *path_struct,
				L_inquiry inq_struct, int verbose)
{

char			phys_path[MAXPATHLEN];
uchar_t			node_wwn[WWN_SIZE], port_wwn[WWN_SIZE], *pg_buf;
L_disk_state		l_disk_state;
struct dlist		*mlist;
int			al_pa, offset, mode_data_avail = 0, err = 0;
int			path_a_found = 0, path_b_found = 0;
L_inquiry		local_inq;
Mode_header_10		*mode_header_ptr;
struct mode_page	*pg_hdr;
WWN_list		*wwn_list;

	(void) strcpy(phys_path, path_struct->p_physical_path);

	(void) memset(&l_disk_state, 0, sizeof (struct l_disk_state_struct));

	/* Get path to all the FC disk and tape devices. */
	if (err = g_get_wwn_list(&wwn_list, verbose)) {
		return (err);
	}

	if ((err = g_get_multipath(phys_path,
		&(l_disk_state.g_disk_state.multipath_list),
		wwn_list, verbose)) != 0) {
		return (err);
	}
	mlist = l_disk_state.g_disk_state.multipath_list;
	if (mlist == NULL) {
		l_disk_state.l_state_flag = L_NO_PATH_FOUND;
		N_DPRINTF(" non_encl_fc_disk_display: Error finding"
			" multiple paths to the disk.\n");
		(void) g_free_wwn_list(&wwn_list);
		return (-1);
	}

	/* get mode page information for FC device */
	if (l_get_mode_pg(phys_path, &pg_buf, verbose) == 0) {
		mode_header_ptr = (struct mode_header_10_struct *)(int)pg_buf;
		pg_hdr = ((struct mode_page *)((int)pg_buf +
			(uchar_t)sizeof (struct mode_header_10_struct) +
			(uchar_t *)(mode_header_ptr->bdesc_length)));
		offset = sizeof (struct mode_header_10_struct) +
			mode_header_ptr->bdesc_length;
		while (offset < (mode_header_ptr->length +
			sizeof (mode_header_ptr->length)) &&
						!mode_data_avail) {
			if (pg_hdr->code == MODEPAGE_CACHING) {
				mode_data_avail++;
				break;
			}
			offset += pg_hdr->length + sizeof (struct mode_page);
			pg_hdr = ((struct mode_page *)((int)pg_buf +
				(uchar_t)offset));
		}
	}

	(void) fprintf(stdout,
		MSGSTR(121, "DEVICE PROPERTIES for disk: %s\n"),
						path_struct->argv);
	while ((mlist != NULL) && (!(path_a_found && path_b_found))) {
		(void) strcpy(phys_path, mlist->dev_path);
		if (err = g_get_inquiry(phys_path, &local_inq)) {
			(void) fprintf(stderr,
				MSGSTR(2114,
				"non_encl_fc_disk_display: Inquiry failed\n"));
			(void) print_errString(err, phys_path);
			(void) g_free_multipath(
				l_disk_state.g_disk_state.multipath_list);
			(void) g_free_wwn_list(&wwn_list);
			return (-1);
		}
		if ((err = g_get_wwn(mlist->dev_path, port_wwn, node_wwn,
					&al_pa, verbose)) != 0) {
			(void) print_errString(err, mlist->dev_path);
			(void) g_free_multipath(
				l_disk_state.g_disk_state.multipath_list);
			(void) g_free_wwn_list(&wwn_list);
			return (-1);
		}
		if (strlen(l_disk_state.g_disk_state.node_wwn_s) == 0) {
			(void) sprintf(l_disk_state.g_disk_state.node_wwn_s,
			"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			node_wwn[0], node_wwn[1], node_wwn[2], node_wwn[3],
			node_wwn[4], node_wwn[5], node_wwn[6], node_wwn[7]);
		}
		if ((err = l_get_disk_port_status(phys_path, &l_disk_state,
				(local_inq.inq_port) ? FC_PORT_B : FC_PORT_A,
				verbose)) != 0) {
			(void) print_errString(err, phys_path);
		(void) g_free_multipath(
				l_disk_state.g_disk_state.multipath_list);
			exit(-1);
		}

		if ((!local_inq.inq_port) && (!path_a_found)) {
			(void) sprintf(l_disk_state.g_disk_state.port_a_wwn_s,
				"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			port_wwn[0], port_wwn[1], port_wwn[2], port_wwn[3],
			port_wwn[4], port_wwn[5], port_wwn[6], port_wwn[7]);
		path_a_found = l_disk_state.g_disk_state.port_a_valid = 1;
		}
		if ((local_inq.inq_port) && (!path_b_found)) {
		path_b_found = l_disk_state.g_disk_state.port_b_valid = 1;
			(void) sprintf(l_disk_state.g_disk_state.port_b_wwn_s,
				"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			port_wwn[0], port_wwn[1], port_wwn[2], port_wwn[3],
			port_wwn[4], port_wwn[5], port_wwn[6], port_wwn[7]);
		}
		mlist = mlist->next;
	}

	if (l_disk_state.g_disk_state.port_a_valid) {
		(void) fprintf(stdout, "  ");
		(void) fprintf(stdout, MSGSTR(141, "Status(Port A):"));
		(void) fprintf(stdout, "\t");
	display_port_status(l_disk_state.g_disk_state.d_state_flags[FC_PORT_A]);
	}

	if (l_disk_state.g_disk_state.port_b_valid) {
		(void) fprintf(stdout, "  ");
		(void) fprintf(stdout, MSGSTR(142, "Status(Port B):"));
		(void) fprintf(stdout, "\t");
	display_port_status(l_disk_state.g_disk_state.d_state_flags[FC_PORT_B]);
	}

	(void) display_disk_info(inq_struct, l_disk_state, path_struct,
				pg_hdr, mode_data_avail, NULL, verbose);
	(void) g_free_multipath(l_disk_state.g_disk_state.multipath_list);
	(void) g_free_wwn_list(&wwn_list);
	return (0);
}



/*
 * display_disk_info() Prints the device specific information
 * for any FC_AL disk device.
 *
 * RETURNS:
 *	none.
 */
static void
display_disk_info(L_inquiry inq, L_disk_state l_disk_state,
		Path_struct *path_struct, struct mode_page *pg_hdr,
		int mode_data_avail, char *name_buf, int options)
{
float		num_blks;
struct dlist	*mlist;
int		i, port_a, port_b;
struct	my_mode_caching	*pg8_buf;

	(void) fprintf(stdout, "  ");
	(void) fprintf(stdout, MSGSTR(3, "Vendor:"));
	(void) fprintf(stdout, "\t\t");
	for (i = 0; i < sizeof (inq.inq_vid); i++) {
		(void) fprintf(stdout, "%c", inq.inq_vid[i]);
	}

	(void) fprintf(stdout, MSGSTR(2115, "\n  Product ID:\t\t"));
	for (i = 0; i < sizeof (inq.inq_pid); i++) {
		(void) fprintf(stdout, "%c", inq.inq_pid[i]);
	}

	(void) fprintf(stdout, MSGSTR(2116, "\n  WWN(Node):\t\t%s"),
				l_disk_state.g_disk_state.node_wwn_s);

	if (l_disk_state.g_disk_state.port_a_valid) {
		(void) fprintf(stdout, MSGSTR(2117, "\n  WWN(Port A):\t\t%s"),
				l_disk_state.g_disk_state.port_a_wwn_s);
	}
	if (l_disk_state.g_disk_state.port_b_valid) {
		(void) fprintf(stdout, MSGSTR(2118, "\n  WWN(Port B):\t\t%s"),
				l_disk_state.g_disk_state.port_b_wwn_s);
	}
	(void) fprintf(stdout, "\n  ");
	(void) fprintf(stdout, MSGSTR(2119, "Revision:"));
	(void) fprintf(stdout, "\t\t");
	for (i = 0; i < sizeof (inq.inq_revision); i++) {
		(void) fprintf(stdout, "%c", inq.inq_revision[i]);
	}

	if ((strstr((char *)inq.inq_pid, "SUN") != NULL) ||
		(strncmp((char *)inq.inq_vid, "SUN     ",
		sizeof (inq.inq_vid)) == 0)) {
		/*
		 * Only print the Serial Number
		 * if vendor ID is SUN or product ID
		 * contains SUN as other drives may
		 * not have the Serial Number fields defined.
		 *
		 * NOTE: The Serial Number is stored in 2 fields??
		 *
		 */
		(void) fprintf(stdout, "\n  ");
		(void) fprintf(stdout, MSGSTR(17, "Serial Num:"));
		(void) fprintf(stdout, "\t\t");
		for (i = 0; i < sizeof (inq.inq_firmware_rev); i++) {
			(void) fprintf(stdout,
				"%c", inq.inq_firmware_rev[i]);
		}
		for (i = 0; i < sizeof (inq.inq_serial); i++) {
			(void) fprintf(stdout, "%c", inq.inq_serial[i]);
		}
	}
	num_blks = l_disk_state.g_disk_state.num_blocks;
	if (num_blks) {
		num_blks /= 2048;	/* get Mbytes */
		(void) fprintf(stdout, "\n  ");
		(void) fprintf(stdout,
			MSGSTR(60,
		"Unformatted capacity:\t%6.3f MBytes"), num_blks);
	}
	(void) fprintf(stdout, "\n");

	if (l_disk_state.g_disk_state.persistent_reserv_flag) {
		(void) fprintf(stdout,
			MSGSTR(2120, "  Persistent Reserve:\t"));
		if (l_disk_state.g_disk_state.persistent_active) {
			(void) fprintf(stdout,
				MSGSTR(39, "Active"));
				(void) fprintf(stdout, "\n");
		}
		if (l_disk_state.g_disk_state.persistent_registered) {
			(void) fprintf(stdout,
				MSGSTR(2121, "Found Registered Keys"));
		} else {
			(void) fprintf(stdout,
				MSGSTR(87, "Not being used"));
		}
		(void) fprintf(stdout, "\n");
	}

	if ((mode_data_avail) && (pg_hdr->code == MODEPAGE_CACHING)) {
		pg8_buf = (struct my_mode_caching *)(int)pg_hdr;
		if (pg8_buf->wce) {
			(void) fprintf(stdout,
				MSGSTR(2122,
				"  Write Cache:\t\t"
				"Enabled\n"));
		}
		if (pg8_buf->rcd == 0) {
			(void) fprintf(stdout,
				MSGSTR(2123,
				"  Read Cache:\t\t"
				"Enabled\n"));
			(void) fprintf(stdout,
				MSGSTR(2124,
				"    Minimum prefetch:"
				"\t0x%x\n"
				"    Maximum prefetch:"
				"\t0x%x\n"),
				pg8_buf->min_prefetch,
				pg8_buf->max_prefetch);
		}
	}

	if (path_struct->slot_valid) {
		(void) fprintf(stdout, MSGSTR(2125, "  Location:\t\t"));
		(void) fprintf(stdout,
			path_struct->f_flag ?
			MSGSTR(2126,
		"In slot %d in the Front of the enclosure named: %s\n")
			: MSGSTR(2127,
		"In slot %d in the Rear of the enclosure named: %s\n"),
			path_struct->slot,
			name_buf);
	}

	(void) fprintf(stdout, "  %s\t\t%s\n",
			MSGSTR(35, "Device Type:"), dtype[inq.inq_dtype]);

	mlist = l_disk_state.g_disk_state.multipath_list;
	(void) fprintf(stdout, MSGSTR(2128, "  Path(s):\n"));
	while (mlist) {
		(void) fprintf(stdout, "  %s\n  %s\n",
			mlist->logical_path, mlist->dev_path);
		mlist = mlist->next;
	}

	if (Options & OPTION_V) {
		if (path_struct->slot_valid) {
			port_a = PORT_A;
			port_b = PORT_B;
		} else {
			port_a = FC_PORT_A;
			port_b = FC_PORT_B;
		}
		/* Only bother if the state is O.K. */
		if ((l_disk_state.g_disk_state.port_a_valid) &&
			(l_disk_state.g_disk_state.d_state_flags[port_a] == 0))
		adm_display_verbose_disk(path_struct->p_physical_path, options);
		else if ((l_disk_state.g_disk_state.port_b_valid) &&
			(l_disk_state.g_disk_state.d_state_flags[port_b] == 0))
		adm_display_verbose_disk(path_struct->p_physical_path, options);
	}
	(void) fprintf(stdout, "\n");

}



/*
 * temp_decode() Display temperature bytes 1-3 state.
 *
 * RETURNS:
 *	none.
 */
static void
temp_decode(Temp_elem_st *temp)
{
	if (temp->ot_fail) {
		(void) fprintf(stdout, MSGSTR(2129,
			": FAILURE - Over Temperature"));
	}
	if (temp->ut_fail) {
		(void) fprintf(stdout, MSGSTR(2130,
			": FAILURE - Under Temperature"));
	}
	if (temp->ot_warn) {
		(void) fprintf(stdout, MSGSTR(2131,
			": WARNING - Over Temperature"));
	}
	if (temp->ut_warn) {
		(void) fprintf(stdout, MSGSTR(2132,
			": WARNING - Under Temperature"));
	}
}



/*
 * disp_degree() Display temperature in Degrees Celsius.
 *
 * RETURNS:
 *	none.
 */
static void
disp_degree(Temp_elem_st *temp)
{
int	t;

	t = temp->degrees;
	t -= 20;	/* re-adjust */
	/*
	 * NL_Comment
	 * The %c is the degree symbol.
	 */
	(void) fprintf(stdout, ":%1.2d%cC ", t, 186);
}



/*
 * trans_decode() Display tranceivers state.
 *
 * RETURNS:
 *	none.
 */
static void
trans_decode(Trans_elem_st *trans)
{
	if (trans->disabled) {
		(void) fprintf(stdout, ": ");
		(void) fprintf(stdout, MSGSTR(34,
			"Disabled"));
	}
	if (trans->lol) {
		(void) fprintf(stdout, MSGSTR(2133,
			": Not receiving a signal"));
	}
	if (trans->lsr_fail) {
		(void) fprintf(stdout, MSGSTR(2134,
			": Laser failed"));
	}
}



/*
 * trans_messages() Display tranceiver status.
 *
 * NOTE: The decoding of the status assumes that the elements
 * are in order with the first two elements are for the
 * "A" IB. It also assumes the tranceivers are numbered
 * 0 and 1.
 *
 * RETURNS:
 *	none.
 */
static void
trans_messages(struct l_state_struct *l_state, int ib_a_flag)
{
Trans_elem_st	trans;
int	i, j, k;
int	count = 0;
int	elem_index = 0;

	/* Get and print messages */
	for (i = 0; i < (int)l_state->ib_tbl.config.enc_num_elem; i++) {
	    elem_index++;
	    if (l_state->ib_tbl.config.type_hdr[i].type == ELM_TYP_FL) {

		if (l_state->ib_tbl.config.type_hdr[i].text_len != 0) {
			(void) fprintf(stdout, "\n\t\t%s\n",
			l_state->ib_tbl.config.text[i]);
		}
		count = k = 0;

		for (j = 0; j <
			(int)l_state->ib_tbl.config.type_hdr[i].num; j++) {
			/*
			 * Only display the status for the selected IB.
			 */
		    if ((count < 2 && ib_a_flag) ||
				(count >= 2 && !ib_a_flag)) {
			(void) bcopy((const void *)
				&l_state->ib_tbl.p2_s.element[elem_index + j],
				(void *)&trans, sizeof (trans));

			if (k == 0) {
				(void) fprintf(stdout, "\t\t%d ", k);
			} else {
				(void) fprintf(stdout, "\n\t\t%d ", k);
			}
			if (trans.code == S_OK) {
				(void) fprintf(stdout,
				MSGSTR(29, "O.K."));
				revision_msg(l_state, elem_index + j);
			} else if ((trans.code == S_CRITICAL) ||
				(trans.code == S_NONCRITICAL)) {
				(void) fprintf(stdout,
				MSGSTR(2135, "Failed"));
				revision_msg(l_state, elem_index + j);
				trans_decode(&trans);
			} else if (trans.code == S_NOT_INSTALLED) {
				(void) fprintf(stdout,
				MSGSTR(30, "Not Installed"));
			} else if (trans.code == S_NOT_AVAILABLE) {
				(void) fprintf(stdout,
				MSGSTR(34, "Disabled"));
				revision_msg(l_state, elem_index + j);
			} else {
				(void) fprintf(stdout,
				MSGSTR(4, "Unknown status"));
			}
			k++;
		    }
		    count++;
		}
	    }
		/*
		 * Calculate the index to each element.
		 */
		elem_index += l_state->ib_tbl.config.type_hdr[i].num;
	}
	(void) fprintf(stdout, "\n");
}



/*
 * temperature_messages() Display temperature status.
 *
 * RETURNS:
 *	none.
 */
static void
temperature_messages(struct l_state_struct *l_state, int rear_flag)
{
Temp_elem_st	temp;
int	i, j, last_ok = 0;
int	all_ok = 1;
int	elem_index = 0;

	/* Get and print messages */
	for (i = 0; i < (int)l_state->ib_tbl.config.enc_num_elem; i++) {
	    elem_index++;	/* skip global */
	    if (l_state->ib_tbl.config.type_hdr[i].type == ELM_TYP_TS) {
		if (!rear_flag) {
		rear_flag = 1;		/* only do front or rear backplane */
		if (l_state->ib_tbl.config.type_hdr[i].text_len != 0) {
			(void) fprintf(stdout, "\t  %s",
			l_state->ib_tbl.config.text[i]);
		}

		/*
		 * Check global status and if not all O.K.
		 * then print individually.
		 */
		(void) bcopy((const void *)&l_state->ib_tbl.p2_s.element[i],
			(void *)&temp, sizeof (temp));
		for (j = 0; j <
			(int)l_state->ib_tbl.config.type_hdr[i].num; j++) {
			(void) bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + j],
				(void *)&temp, sizeof (temp));

			if ((j == 0) && (temp.code == S_OK) &&
				(!(temp.ot_fail || temp.ot_warn ||
					temp.ut_fail || temp.ut_warn))) {
				(void) fprintf(stdout, "\n\t  %d", j);
			} else if ((j == 6) && (temp.code == S_OK) &&
				all_ok) {
				(void) fprintf(stdout, "\n\t  %d", j);
			} else if (last_ok && (temp.code == S_OK)) {
				(void) fprintf(stdout, "%d", j);
			} else {
				(void) fprintf(stdout, "\n\t\t%d", j);
			}
			if (temp.code == S_OK) {
				disp_degree(&temp);
				if (temp.ot_fail || temp.ot_warn ||
					temp.ut_fail || temp.ut_warn) {
					temp_decode(&temp);
					all_ok = 0;
					last_ok = 0;
				} else {
					last_ok++;
				}
			} else if (temp.code == S_CRITICAL) {
				(void) fprintf(stdout,
				MSGSTR(122, "Critical failure"));
				last_ok = 0;
				all_ok = 0;
			} else if (temp.code == S_NONCRITICAL) {
				(void) fprintf(stdout,
				MSGSTR(89, "Non-Critical Failure"));
				last_ok = 0;
				all_ok = 0;
			} else if (temp.code == S_NOT_INSTALLED) {
				(void) fprintf(stdout,
				MSGSTR(30, "Not Installed"));
				last_ok = 0;
				all_ok = 0;
			} else if (temp.code == S_NOT_AVAILABLE) {
				(void) fprintf(stdout,
				MSGSTR(34, "Disabled"));
				last_ok = 0;
				all_ok = 0;
			} else {
				(void) fprintf(stdout,
				MSGSTR(4, "Unknown status"));
				last_ok = 0;
				all_ok = 0;
			}
		}
		if (all_ok) {
			(void) fprintf(stdout,
			MSGSTR(2136, " (All temperatures are "
			"NORMAL.)"));
		}
		all_ok = 1;
		(void) fprintf(stdout, "\n");
	    } else {
		rear_flag = 0;
	    }
	    }
	    elem_index += l_state->ib_tbl.config.type_hdr[i].num;
	}
}



/*
 * ib_decode() Display IB byte 3 state.
 *
 * RETURNS:
 *	none.
 */
static void
ib_decode(Ctlr_elem_st *ctlr)
{
	if (ctlr->overtemp_alart) {
		(void) fprintf(stdout, MSGSTR(2137,
			" - IB Over Temperature Alert "));
	}
	if (ctlr->ib_loop_1_fail) {
		(void) fprintf(stdout, MSGSTR(2138,
			" - IB Loop 1 has failed "));
	}
	if (ctlr->ib_loop_0_fail) {
		(void) fprintf(stdout, MSGSTR(2139,
			" - IB Loop 0 has failed "));
	}
}



/*
 * mb_messages() Display motherboard
 * (interconnect assembly) messages.
 *
 * RETURNS:
 *	none.
 */
static void
mb_messages(struct l_state_struct *l_state, int index, int elem_index)
{
int		j;
Interconnect_st	interconnect;

	if (l_state->ib_tbl.config.type_hdr[index].text_len != 0) {
		(void) fprintf(stdout, "%s\n",
		l_state->ib_tbl.config.text[index]);
	}
	for (j = 0; j < (int)l_state->ib_tbl.config.type_hdr[index].num;
			j++) {
		(void) bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + j],
			(void *)&interconnect, sizeof (interconnect));
		(void) fprintf(stdout, "\t");

		if (interconnect.code == S_OK) {
			(void) fprintf(stdout,
			MSGSTR(29, "O.K."));
			revision_msg(l_state, elem_index + j);
		} else if (interconnect.code == S_NOT_INSTALLED) {
			(void) fprintf(stdout,
			MSGSTR(30, "Not Installed"));
		} else if (interconnect.code == S_CRITICAL) {
			if (interconnect.eprom_fail != NULL) {
				(void) fprintf(stdout, MSGSTR(2140,
					"Critical Failure: EEPROM failure"));
			} else {
				(void) fprintf(stdout, MSGSTR(2141,
					"Critical Failure: Unknown failure"));
			}
			revision_msg(l_state, elem_index + j);
		} else if (interconnect.code == S_NONCRITICAL) {
			if (interconnect.eprom_fail != NULL) {
				(void) fprintf(stdout, MSGSTR(2142,
				"Non-Critical Failure: EEPROM failure"));
			} else {
				(void) fprintf(stdout, MSGSTR(2143,
				"Non-Critical Failure: Unknown failure"));
			}
			revision_msg(l_state, elem_index + j);
		} else if (interconnect.code == S_NOT_AVAILABLE) {
			(void) fprintf(stdout,
			MSGSTR(34, "Disabled"));
			revision_msg(l_state, elem_index + j);
		} else {
			(void) fprintf(stdout,
			MSGSTR(4, "Unknown status"));
		}
		(void) fprintf(stdout, "\n");
	}


}



/*
 * back_plane_messages() Display back_plane messages
 * including the temperature's.
 *
 * RETURNS:
 *	none.
 */
static void
back_plane_messages(struct l_state_struct *l_state, int index, int elem_index)
{
Bp_elem_st	bp;
int		j;
char		status_string[MAXPATHLEN];

	if (l_state->ib_tbl.config.type_hdr[index].text_len != 0) {
		(void) fprintf(stdout, "%s\n",
		l_state->ib_tbl.config.text[index]);
	}
	for (j = 0; j < (int)l_state->ib_tbl.config.type_hdr[index].num;
			j++) {
		(void) bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + j],
			(void *)&bp, sizeof (bp));
		if (j == 0) {
			(void) fprintf(stdout,
				MSGSTR(2144, "\tFront Backplane: "));
		} else {
			(void) fprintf(stdout,
				MSGSTR(2145, "\tRear Backplane:  "));
		}

		(void) l_element_msg_string(bp.code, status_string);
		(void) fprintf(stdout, "%s", status_string);

		if (bp.code != S_NOT_INSTALLED) {
			revision_msg(l_state, elem_index + j);
			if ((bp.byp_a_enabled || bp.en_bypass_a) &&
				!(bp.byp_b_enabled || bp.en_bypass_b)) {
				(void) fprintf(stdout, " (");
				(void) fprintf(stdout,
				MSGSTR(130, "Bypass A enabled"));
				(void) fprintf(stdout, ")");
			} else if ((bp.byp_b_enabled || bp.en_bypass_b) &&
				!(bp.byp_a_enabled || bp.en_bypass_a)) {
				(void) fprintf(stdout, " (");
				(void) fprintf(stdout,
				MSGSTR(129, "Bypass B enabled"));
				(void) fprintf(stdout, ")");
			/* This case covers where a and b are bypassed */
			} else if (bp.byp_b_enabled || bp.en_bypass_b) {
				(void) fprintf(stdout,
				MSGSTR(2146, " (Bypass's A & B enabled)"));
			}
			(void) fprintf(stdout, "\n");
			temperature_messages(l_state, j);
		} else {
			(void) fprintf(stdout, "\n");
		}
	}
}



/*
 * loop_messages() Display loop messages.
 *
 * RETURNS:
 *	none.
 */
static void
loop_messages(struct l_state_struct *l_state, int index, int elem_index)
{
Loop_elem_st	loop;
int		j;

	if (l_state->ib_tbl.config.type_hdr[index].text_len != 0) {
		(void) fprintf(stdout, "%s\n",
		l_state->ib_tbl.config.text[index]);
	}
	for (j = 0; j < (int)l_state->ib_tbl.config.type_hdr[index].num;
			j++) {
		(void) bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + j],
			(void *)&loop, sizeof (loop));

		(void) fprintf(stdout, "\t");
		if (j == 0) {
			if (loop.code == S_NOT_INSTALLED) {
				(void) fprintf(stdout,
				MSGSTR(2147, "Loop A is not installed"));
			} else {
				if (loop.split) {
					(void) fprintf(stdout, MSGSTR(2148,
				"Loop A is configured as two separate loops."));
				} else {
					(void) fprintf(stdout, MSGSTR(2149,
				"Loop A is configured as a single loop."));
				}
			}
		} else {
			if (loop.code == S_NOT_INSTALLED) {
				(void) fprintf(stdout,
				MSGSTR(2150, "Loop B is not installed"));
			} else {
				if (loop.split) {
					(void) fprintf(stdout, MSGSTR(2151,
				"Loop B is configured as two separate loops."));
				} else {
					(void) fprintf(stdout, MSGSTR(2152,
				"Loop B is configured as a single loop."));
				}
			}
		}
		(void) fprintf(stdout, "\n");
	}
}



/*
 * ctlr_messages() Display ESI Controller status.
 *
 * RETURNS:
 *	none.
 */
static void
ctlr_messages(struct l_state_struct *l_state, int index, int elem_index)
{
Ctlr_elem_st	ctlr;
int		j;
int		ib_a_flag = 1;

	if (l_state->ib_tbl.config.type_hdr[index].text_len != 0) {
		(void) fprintf(stdout, "%s\n",
		l_state->ib_tbl.config.text[index]);
	}
	for (j = 0; j < (int)l_state->ib_tbl.config.type_hdr[index].num;
			j++) {
		(void) bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + j],
			(void *)&ctlr, sizeof (ctlr));
		if (j == 0) {
			(void) fprintf(stdout, MSGSTR(2153, "\tA: "));
		} else {
			(void) fprintf(stdout, MSGSTR(2154, "\tB: "));
			ib_a_flag = 0;
		}
		if (ctlr.code == S_OK) {
			(void) fprintf(stdout, MSGSTR(29, "O.K."));
			/* If any byte 3 bits set display */
			ib_decode(&ctlr);
			/* Display Version message */
			revision_msg(l_state, elem_index + j);
			/*
			 * Display the tranciver module state for this
			 * IB.
			 */
			trans_messages(l_state, ib_a_flag);
		} else if (ctlr.code == S_CRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(122, "Critical failure"));
			ib_decode(&ctlr);
			(void) fprintf(stdout, "\n");
		} else if (ctlr.code == S_NONCRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(89, "Non-Critical Failure"));
			ib_decode(&ctlr);
			(void) fprintf(stdout, "\n");
		} else if (ctlr.code == S_NOT_INSTALLED) {
			(void) fprintf(stdout,
			MSGSTR(30, "Not Installed"));
			(void) fprintf(stdout, "\n");
		} else if (ctlr.code == S_NOT_AVAILABLE) {
			(void) fprintf(stdout,
			MSGSTR(34, "Disabled"));
			(void) fprintf(stdout, "\n");
		} else {
			(void) fprintf(stdout,
			MSGSTR(4, "Unknown status"));
			(void) fprintf(stdout, "\n");
		}
	}
}



/*
 * fan_decode() Display Fans bytes 1-3 state.
 *
 * RETURNS:
 *	none.
 */
static void
fan_decode(Fan_elem_st *fan)
{
	if (fan->fail) {
		(void) fprintf(stdout, MSGSTR(2155,
			":Yellow LED is on"));
	}
	if (fan->speed == 0) {
		(void) fprintf(stdout, MSGSTR(2156,
			":Fan stopped"));
	} else if (fan->speed < S_HI_SPEED) {
		(void) fprintf(stdout, MSGSTR(2157,
			":Fan speed Low"));
	} else {
		(void) fprintf(stdout, MSGSTR(2158,
			":Fan speed Hi"));
	}
}

/*
 * fan_messages() Display Fan status.
 *
 * RETURNS:
 *	none.
 */
static void
fan_messages(struct l_state_struct *l_state, int hdr_index, int elem_index)
{
Fan_elem_st	fan;
int	j;

	/* Get and print messages */
	if (l_state->ib_tbl.config.type_hdr[hdr_index].text_len != 0) {
		(void) fprintf(stdout, "%s\n",
		l_state->ib_tbl.config.text[hdr_index]);
	}
	for (j = 0; j < (int)l_state->ib_tbl.config.type_hdr[hdr_index].num;
			j++) {
		(void) bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + j],
			(void *)&fan, sizeof (fan));
		(void) fprintf(stdout, "\t%d ", j);
		if (fan.code == S_OK) {
			(void) fprintf(stdout, MSGSTR(29, "O.K."));
			revision_msg(l_state, elem_index + j);
		} else if (fan.code == S_CRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(122, "Critical failure"));
			fan_decode(&fan);
			revision_msg(l_state, elem_index + j);
		} else if (fan.code == S_NONCRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(89, "Non-Critical Failure"));
			fan_decode(&fan);
			revision_msg(l_state, elem_index + j);
		} else if (fan.code == S_NOT_INSTALLED) {
			(void) fprintf(stdout,
			MSGSTR(30, "Not Installed"));
		} else if (fan.code == S_NOT_AVAILABLE) {
			(void) fprintf(stdout,
			MSGSTR(34, "Disabled"));
			revision_msg(l_state, elem_index + j);
		} else {
			(void) fprintf(stdout,
			MSGSTR(4, "Unknown status"));
		}
	}
	(void) fprintf(stdout, "\n");
}



/*
 * ps_decode() Display Power Supply bytes 1-3 state.
 *
 * RETURNS:
 *	none.
 */
static void
ps_decode(Ps_elem_st *ps)
{
	if (ps->dc_over) {
		(void) fprintf(stdout, MSGSTR(2159,
			": DC Voltage too high"));
	}
	if (ps->dc_under) {
		(void) fprintf(stdout, MSGSTR(2160,
			": DC Voltage too low"));
	}
	if (ps->dc_over_i) {
		(void) fprintf(stdout, MSGSTR(2161,
			": DC Current too high"));
	}
	if (ps->ovrtmp_fail || ps->temp_warn) {
		(void) fprintf(stdout, MSGSTR(2162,
			": Temperature too high"));
	}
	if (ps->ac_fail) {
		(void) fprintf(stdout, MSGSTR(2163,
			": AC Failed"));
	}
	if (ps->dc_fail) {
		(void) fprintf(stdout, MSGSTR(2164,
			": DC Failed"));
	}
}



/*
 * revision_msg() Print the revision message from page 7.
 *
 * RETURNS:
 *	none.
 */
static	void
revision_msg(struct l_state_struct *l_state, int index)
{
	if (strlen((const char *)
		l_state->ib_tbl.p7_s.element_desc[index].desc_string)) {
		(void) fprintf(stdout, "(%s)",
		l_state->ib_tbl.p7_s.element_desc[index].desc_string);
	}
}



/*
 * ps_messages() Display Power Supply status.
 *
 * RETURNS:
 *	none.
 */
static void
ps_messages(struct l_state_struct *l_state, int	index, int elem_index)
{
Ps_elem_st	ps;
int	j;

	/* Get and print Power Supply messages */

	if (l_state->ib_tbl.config.type_hdr[index].text_len != 0) {
		(void) fprintf(stdout, "%s\n",
		l_state->ib_tbl.config.text[index]);
	}

	for (j = 0; j < (int)l_state->ib_tbl.config.type_hdr[index].num;
		j++) {
		(void) bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + j],
			(void *)&ps, sizeof (ps));
		(void) fprintf(stdout, "\t%d ", j);
		if (ps.code == S_OK) {
			(void) fprintf(stdout, MSGSTR(29, "O.K."));
			revision_msg(l_state, elem_index + j);
		} else if (ps.code == S_CRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(122, "Critical failure"));
			ps_decode(&ps);
			revision_msg(l_state, elem_index + j);
		} else if (ps.code == S_NONCRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(89, "Non-Critical Failure"));
			ps_decode(&ps);
			revision_msg(l_state, elem_index + j);
		} else if (ps.code == S_NOT_INSTALLED) {
			(void) fprintf(stdout,
			MSGSTR(30, "Not Installed"));
		} else if (ps.code == S_NOT_AVAILABLE) {
			(void) fprintf(stdout,
			MSGSTR(34, "Disabled"));
			revision_msg(l_state, elem_index + j);
		} else {
			(void) fprintf(stdout,
			MSGSTR(4, "Unknown status"));
		}

	}
	(void) fprintf(stdout, "\n");
}



/*
 * abnormal_condition() Display any abnormal condition messages.
 *
 * RETURNS:
 *	none.
 */
static void
abnormal_condition_display(struct l_state_struct *l_state)
{

	(void) fprintf(stdout, "\n");
	if (l_state->ib_tbl.p2_s.ui.crit) {
		(void) fprintf(stdout,
			MSGSTR(2165, "                         "
			"CRITICAL CONDITION DETECTED\n"));
	}
	if (l_state->ib_tbl.p2_s.ui.non_crit) {
		(void) fprintf(stdout,
			MSGSTR(2166, "                   "
			"WARNING: NON-CRITICAL CONDITION DETECTED\n"));
	}
	if (l_state->ib_tbl.p2_s.ui.invop) {
		(void) fprintf(stdout,
			MSGSTR(2167, "                      "
			"WARNING: Invalid Operation bit set.\n"
			"\tThis means an Enclosure Control page"
			" or an Array Control page with an invalid\n"
			"\tformat has previously been transmitted to the"
			" Enclosure Services card by a\n\tSend Diagnostic"
			" SCSI command.\n"));
	}
	(void) fprintf(stdout, "\n");
}


/*
 *	FORCELIP expert function
 */
static void
adm_forcelip(char **argv)
{
int		slot, f_r, path_index = 0, err = 0;
Path_struct	*path_struct = NULL;
char		*path_phys = NULL, *ptr;
char		 err_path[MAXNAMELEN];

while (argv[path_index] != NULL) {
	if ((err = l_convert_name(argv[path_index], &path_phys,
	    &path_struct, Options & PVERBOSE)) != 0) {
		(void) strcpy(err_path, argv[path_index]);
		if (err != -1) {
			(void) print_errString(err, argv[path_index]);
			path_index++;
			continue;
		}
		if (((ptr = strstr(err_path, ", ")) != NULL) &&
		    ((*(ptr + 1) == 'f') || (*(ptr + 1) == 'r'))) {
			*ptr = NULL;
			slot = path_struct->slot;
			f_r = path_struct->f_flag;
			path_phys = NULL;
			if ((err = l_convert_name(err_path, &path_phys,
			    &path_struct, Options & PVERBOSE)) != 0) {
				(void) fprintf(stdout,
				    MSGSTR(33,
					" Error: converting"
					" %s to physical path.\n"
					" Invalid pathname.\n"),
				    argv[path_index]);
				if (err != -1) {
					(void) print_errString(err,
					    argv[path_index]);
				}
				path_index++;
				continue;
			}
			if ((err = print_devState(argv[path_index],
				path_struct->p_physical_path,
				f_r, slot, Options & PVERBOSE)) != 0) {
				path_index++;
				continue;
			}
		} else {
			(void) fprintf(stdout, "\n ");
			(void) fprintf(stdout,
			    MSGSTR(112, "Error: Invalid pathname (%s)"),
			    argv[path_index]);
			(void) fprintf(stdout, "\n");
		}
		path_index++;
		continue;
	    }
	    if (err = g_force_lip(path_phys, Options & PVERBOSE)) {
			(void) print_errString(err, argv[path_index]);
			path_index++;
			continue;
	    }
	    path_index++;
	    if (path_struct != NULL) {
		(void) free(path_struct);
	    }
	}
}


/*
 * adm_inquiry() Display the inquiry information for
 * a SENA enclosure(s) or disk(s).
 *
 * RETURNS:
 *	none.
 */
static void
adm_inquiry(char **argv)
{
L_inquiry	inq;
int		path_index = 0;
char		**p;
uchar_t		*v_parm;
int		i, scsi_3, length, slot, f_r, err = 0;
char		byte_number[MAXNAMELEN], inq_path[MAXNAMELEN];
char		*path_phys = NULL, *ptr;
Path_struct	*path_struct;

static	char *scsi_inquiry_labels_2[21];
static	char *scsi_inquiry_labels_3[22];
static	char	*ansi_version[4];
	/*
	 * Intialize scsi_inquiry_labels_2 with i18n strings
	 */
	scsi_inquiry_labels_2[0] = MSGSTR(138, "Vendor:                     ");
	scsi_inquiry_labels_2[1] = MSGSTR(149, "Product:                    ");
	scsi_inquiry_labels_2[2] = MSGSTR(139, "Revision:                   ");
	scsi_inquiry_labels_2[3] = MSGSTR(143, "Firmware Revision           ");
	scsi_inquiry_labels_2[4] = MSGSTR(144, "Serial Number               ");
	scsi_inquiry_labels_2[5] = MSGSTR(140, "Device type:                ");
	scsi_inquiry_labels_2[6] = MSGSTR(145, "Removable media:            ");
	scsi_inquiry_labels_2[7] = MSGSTR(146, "ISO version:                ");
	scsi_inquiry_labels_2[8] = MSGSTR(147, "ECMA version:               ");
	scsi_inquiry_labels_2[9] = MSGSTR(148, "ANSI version:               ");
	scsi_inquiry_labels_2[10] =
		MSGSTR(2168, "Async event notification:   ");
	scsi_inquiry_labels_2[11] =
		MSGSTR(2169, "Terminate i/o process msg:  ");
	scsi_inquiry_labels_2[12] = MSGSTR(150, "Response data format:       ");
	scsi_inquiry_labels_2[13] = MSGSTR(151, "Additional length:          ");
	scsi_inquiry_labels_2[14] = MSGSTR(152, "Relative addressing:        ");
	scsi_inquiry_labels_2[15] =
		MSGSTR(2170, "32 bit transfers:           ");
	scsi_inquiry_labels_2[16] =
		MSGSTR(2171, "16 bit transfers:           ");
	scsi_inquiry_labels_2[17] =
		MSGSTR(2172, "Synchronous transfers:      ");
	scsi_inquiry_labels_2[18] = MSGSTR(153, "Linked commands:            ");
	scsi_inquiry_labels_2[19] = MSGSTR(154, "Command queueing:           ");
	scsi_inquiry_labels_2[20] =
		MSGSTR(2173, "Soft reset option:          ");

	/*
	 * Intialize scsi_inquiry_labels_3 with i18n strings
	 */
	scsi_inquiry_labels_3[0] = MSGSTR(138, "Vendor:                     ");
	scsi_inquiry_labels_3[1] = MSGSTR(149, "Product:                    ");
	scsi_inquiry_labels_3[2] = MSGSTR(139, "Revision:                   ");
	scsi_inquiry_labels_3[3] = MSGSTR(143, "Firmware Revision           ");
	scsi_inquiry_labels_3[4] = MSGSTR(144, "Serial Number               ");
	scsi_inquiry_labels_3[5] = MSGSTR(140, "Device type:                ");
	scsi_inquiry_labels_3[6] = MSGSTR(145, "Removable media:            ");
	scsi_inquiry_labels_3[7] = MSGSTR(2174, "Medium Changer Element:     ");
	scsi_inquiry_labels_3[8] = MSGSTR(146, "ISO version:                ");
	scsi_inquiry_labels_3[9] = MSGSTR(147, "ECMA version:               ");
	scsi_inquiry_labels_3[10] = MSGSTR(148, "ANSI version:               ");
	scsi_inquiry_labels_3[11] =
		MSGSTR(2175, "Async event reporting:      ");
	scsi_inquiry_labels_3[12] =
		MSGSTR(2176, "Terminate task:             ");
	scsi_inquiry_labels_3[13] =
		MSGSTR(2177, "Normal ACA Supported:       ");
	scsi_inquiry_labels_3[14] = MSGSTR(150, "Response data format:       ");
	scsi_inquiry_labels_3[15] = MSGSTR(151, "Additional length:          ");
	scsi_inquiry_labels_3[16] =
		MSGSTR(2178, "Cmd received on port:       ");
	scsi_inquiry_labels_3[17] =
		MSGSTR(2179, "SIP Bits:                   ");
	scsi_inquiry_labels_3[18] = MSGSTR(152, "Relative addressing:        ");
	scsi_inquiry_labels_3[19] = MSGSTR(153, "Linked commands:            ");
	scsi_inquiry_labels_3[20] =
		MSGSTR(2180, "Transfer Disable:           ");
	scsi_inquiry_labels_3[21] = MSGSTR(154, "Command queueing:           ");

	/*
	 * Intialize scsi_inquiry_labels_3 with i18n strings
	 */
	ansi_version[0] = MSGSTR(2181,
		" (Device might or might not comply to an ANSI version)");
	ansi_version[1] = MSGSTR(2182,
		" (This code is reserved for historical uses)");
	ansi_version[2] = MSGSTR(2183,
		" (Device complies to ANSI X3.131-1994 (SCSI-2))");
	ansi_version[3] = MSGSTR(2184,
		" (Device complies to SCSI-3)");

	while (argv[path_index] != NULL) {
	    if ((err = l_convert_name(argv[path_index], &path_phys,
		&path_struct, Options & PVERBOSE)) != 0) {
		(void) strcpy(inq_path, argv[path_index]);
		if (((ptr = strstr(inq_path, ",")) != NULL) &&
			((*(ptr + 1) == 'f') || (*(ptr + 1) == 'r'))) {
			if (err != -1) {
				(void) print_errString(err, argv[path_index]);
				path_index++;
				continue;
			}
			*ptr = NULL;
			slot = path_struct->slot;
			f_r = path_struct->f_flag;
			path_phys = NULL;
			if ((err = l_convert_name(inq_path, &path_phys,
				&path_struct, Options & PVERBOSE)) != 0) {
				(void) fprintf(stdout,
					MSGSTR(33,
					" Error: converting"
					" %s to physical path.\n"
					" Invalid pathname.\n"),
					argv[path_index]);
				if (err != -1) {
					(void) print_errString(err,
							argv[path_index]);
				}
				path_index++;
				continue;
			}
			if ((err = print_devState(argv[path_index],
					path_struct->p_physical_path,
					f_r, slot, Options & PVERBOSE)) != 0) {
				path_index++;
				continue;
			}
		} else {
			if (err != -1) {
				(void) print_errString(err, argv[path_index]);
			} else {
			    (void) fprintf(stdout, "\n ");
			    (void) fprintf(stdout,
				MSGSTR(112, "Error: Invalid pathname (%s)"),
				argv[path_index]);
			    (void) fprintf(stdout, "\n");
			}
		}
		path_index++;
		continue;
	    }
	    if (err = g_get_inquiry(path_phys, &inq)) {
		(void) fprintf(stderr, "\n");
		(void) print_errString(err, argv[path_index]);
		(void) fprintf(stderr, "\n");

	    /* Continue on in case of an error */
	    } else {

	    /* print inquiry information */

	    (void) fprintf(stdout, MSGSTR(2185, "\nINQUIRY:\n"));
	    if (strcmp(argv[path_index], path_phys) != 0) {
		(void) fprintf(stdout, "  ");
		(void) fprintf(stdout,
		MSGSTR(5, "Physical Path:"));
		(void) fprintf(stdout, "\n  %s\n", path_phys);
	    }
	    if (inq.inq_ansi < 3) {
		p = scsi_inquiry_labels_2;
		scsi_3 = 0;
	    } else {
		p = scsi_inquiry_labels_3;
		scsi_3 = 1;
	    }
	    if (inq.inq_len < 11) {
		p += 1;
	    } else {
		/* */
		(void) fprintf(stdout, "%s", *p++);
		for (i = 0; i < sizeof (inq.inq_vid); i++) {
			(void) fprintf(stdout, "%c", inq.inq_vid[i]);
		}
		(void) fprintf(stdout, "\n");
	    }
	    if (inq.inq_len < 27) {
		p += 1;
	    } else {
		(void) fprintf(stdout, "%s", *p++);
		for (i = 0; i < sizeof (inq.inq_pid); i++) {
			(void) fprintf(stdout, "%c", inq.inq_pid[i]);
		}
		(void) fprintf(stdout, "\n");
	    }
	    if (inq.inq_len < 31) {
		p += 1;
	    } else {
		(void) fprintf(stdout, "%s", *p++);
		for (i = 0; i < sizeof (inq.inq_revision); i++) {
		    (void) fprintf(stdout, "%c", inq.inq_revision[i]);
		}
		(void) fprintf(stdout, "\n");
	    }
	    if (inq.inq_len < 39) {
		p += 2;
	    } else {
		/*
		 * If Pluto then print
		 * firmware rev & serial #.
		 */
		if (strstr((char *)inq.inq_pid, "SSA") != 0) {
			(void) fprintf(stdout, "%s", *p++);
			for (i = 0; i < sizeof (inq.inq_firmware_rev); i++) {
				(void) fprintf(stdout,
				"%c", inq.inq_firmware_rev[i]);
			}
			(void) fprintf(stdout, "\n");
			(void) fprintf(stdout, "%s", *p++);
			for (i = 0; i < sizeof (inq.inq_serial); i++) {
				(void) fprintf(stdout, "%c", inq.inq_serial[i]);
			}
			(void) fprintf(stdout, "\n");
		} else if (((strstr((char *)inq.inq_pid, "SUN") != 0) ||
			(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) == 0)) &&
			(strstr((char *)inq.inq_pid,
			ENCLOSURE_PROD_ID) == 0)) {
			/*
			 * Only print the Serial Number
			 * if vendor ID is SUN or product ID
			 * contains SUN as other drives may
			 * not have the Serial Number fields defined
			 * and it is not a Photon SES card.
			 *
			 * NOTE: The Serial Number is stored in 2 fields??
			 *
			 */
			p++;
			(void) fprintf(stdout, "%s", *p++);
			for (i = 0; i < sizeof (inq.inq_firmware_rev); i++) {
				(void) fprintf(stdout,
				"%c", inq.inq_firmware_rev[i]);
			}
			for (i = 0; i < sizeof (inq.inq_serial); i++) {
				(void) fprintf(stdout, "%c", inq.inq_serial[i]);
			}
			(void) fprintf(stdout, "\n");
		    } else {
			p += 2;
		    }
	    }

	    (void) fprintf(stdout, "%s0x%x (",
			*p++, inq.inq_dtype);
	    if (inq.inq_dtype < 0x10) {
		(void) fprintf(stdout, "%s", dtype[inq.inq_dtype]);
	    } else if (inq.inq_dtype < 0x1f) {
		(void) fprintf(stdout, MSGSTR(71, "Reserved"));
	    } else {
		(void) fprintf(stdout, MSGSTR(2186, "Unknown device"));
	    }
	    (void) fprintf(stdout, ")\n");

	    (void) fprintf(stdout, "%s", *p++);
	    if (inq.inq_rmb != NULL) {
		(void) fprintf(stdout, MSGSTR(40, "yes"));
	    } else {
		(void) fprintf(stdout, MSGSTR(45, "no"));
	    }
	    (void) fprintf(stdout, "\n");

	    if (scsi_3) {
		(void) fprintf(stdout, "%s", *p++);
		if (inq.inq_mchngr != NULL) {
			(void) fprintf(stdout, MSGSTR(40, "yes"));
		} else {
			(void) fprintf(stdout, MSGSTR(45, "no"));
		}
		(void) fprintf(stdout, "\n");
	    }
	    (void) fprintf(stdout, "%s%d\n", *p++, inq.inq_iso);
	    (void) fprintf(stdout, "%s%d\n", *p++, inq.inq_ecma);

	    (void) fprintf(stdout, "%s%d", *p++, inq.inq_ansi);
	    if (inq.inq_ansi < 0x4) {
		(void) fprintf(stdout, "%s", ansi_version[inq.inq_ansi]);
	    } else
		(void) fprintf(stdout, MSGSTR(71, "Reserved"));
	    (void) fprintf(stdout, "\n");

	    if (inq.inq_aenc) {
		(void) fprintf(stdout, "%s", *p++);
		(void) fprintf(stdout, MSGSTR(40, "yes"));
		(void) fprintf(stdout, "\n");
	    } else {
		p++;
	    }
	    if (scsi_3) {
		(void) fprintf(stdout, "%s", *p++);
		if (inq.inq_normaca != NULL) {
			(void) fprintf(stdout, MSGSTR(40, "yes"));
		} else {
			(void) fprintf(stdout, MSGSTR(45, "no"));
		}
		(void) fprintf(stdout, "\n");
	    }
	    if (inq.inq_trmiop) {
		(void) fprintf(stdout, "%s", *p++);
		(void) fprintf(stdout, MSGSTR(40, "yes"));
		(void) fprintf(stdout, "\n");
	    } else {
		p++;
	    }
	    (void) fprintf(stdout, "%s%d\n", *p++, inq.inq_rdf);
	    (void) fprintf(stdout, "%s0x%x\n", *p++, inq.inq_len);
	    if (scsi_3) {
		if (inq.inq_dual_p) {
			if (inq.inq_port != NULL) {
				(void) fprintf(stdout, MSGSTR(2187,
					"%sa\n"), *p++);
			} else {
				(void) fprintf(stdout, MSGSTR(2188,
					"%sb\n"), *p++);
			}
		} else {
		    p++;
		}
	    }
	    if (scsi_3) {
		if (inq.inq_SIP_1 || inq.ui.inq_SIP_2 ||
			inq.ui.inq_SIP_3) {
		    (void) fprintf(stdout, "%s%d, %d, %d\n", *p,
			inq.inq_SIP_1, inq.ui.inq_SIP_2, inq.ui.inq_SIP_3);
		}
		p++;

	    }

	    if (inq.ui.inq_2_reladdr) {
		(void) fprintf(stdout, "%s", *p);
		(void) fprintf(stdout, MSGSTR(40, "yes"));
		(void) fprintf(stdout, "\n");
	    }
	    p++;

	    if (!scsi_3) {

		    if (inq.ui.inq_wbus32) {
			(void) fprintf(stdout, "%s", *p);
			(void) fprintf(stdout, MSGSTR(40, "yes"));
			(void) fprintf(stdout, "\n");
		    }
		    p++;

		    if (inq.ui.inq_wbus16) {
			(void) fprintf(stdout, "%s", *p);
			(void) fprintf(stdout, MSGSTR(40, "yes"));
			(void) fprintf(stdout, "\n");
		    }
		    p++;

		    if (inq.ui.inq_sync) {
			(void) fprintf(stdout, "%s", *p);
			(void) fprintf(stdout, MSGSTR(40, "yes"));
			(void) fprintf(stdout, "\n");
		    }
		    p++;

	    }
	    if (inq.ui.inq_linked) {
		(void) fprintf(stdout, "%s", *p);
		(void) fprintf(stdout, MSGSTR(40, "yes"));
		(void) fprintf(stdout, "\n");
	    }
	    p++;

	    if (inq.ui.inq_cmdque) {
		(void) fprintf(stdout, "%s", *p);
		(void) fprintf(stdout, MSGSTR(40, "yes"));
		(void) fprintf(stdout, "\n");
	    }
	    p++;

	    if (scsi_3) {
		(void) fprintf(stdout, "%s", *p++);
		if (inq.ui.inq_trandis != NULL) {
			(void) fprintf(stdout, MSGSTR(40, "yes"));
		} else {
			(void) fprintf(stdout, MSGSTR(45, "no"));
		}
		(void) fprintf(stdout, "\n");
	    }
	    if (!scsi_3) {
		    if (inq.ui.inq_sftre) {
			(void) fprintf(stdout, "%s", *p);
			(void) fprintf(stdout, MSGSTR(40, "yes"));
			(void) fprintf(stdout, "\n");
		    }
		    p++;

	    }

		/*
		 * Now print the vendor-specific data.
		 */
	    v_parm = inq.inq_ven_specific_1;
	    if (inq.inq_len >= 32) {
		length = inq.inq_len - 31;
		if (strstr((char *)inq.inq_pid, "SSA") != 0) {
		(void) fprintf(stdout, MSGSTR(2189,
					"Number of Ports, Targets:   %d,%d\n"),
			inq.inq_ssa_ports, inq.inq_ssa_tgts);
			v_parm += 20;
			length -= 20;
		} else if ((strstr((char *)inq.inq_pid, "SUN") != 0) ||
			(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) == 0)) {
			v_parm += 16;
			length -= 16;
		}
		/*
		 * Do hex Dump of rest of the data.
		 */
		if (length > 0) {
			(void) fprintf(stdout,
				MSGSTR(2190,
			"              VENDOR-SPECIFIC PARAMETERS\n"));
			(void) fprintf(stdout,
				MSGSTR(2191,
				"Byte#                  Hex Value            "
				"                 ASCII\n"));
			(void) sprintf(byte_number,
			"%d    ", inq.inq_len - length + 5);
			(void) g_dump(byte_number, v_parm,
				min(length, inq.inq_res3 - v_parm),
				HEX_ASCII);
		}
		/*
		 * Skip reserved bytes 56-95.
		 */
		length -= (inq.inq_box_name - v_parm);
		if (length > 0) {
			(void) sprintf(byte_number, "%d    ",
				inq.inq_len - length + 5);
			(void) g_dump(byte_number, inq.inq_box_name,
				min(length, sizeof (inq.inq_box_name) +
				sizeof (inq.inq_avu)),
				HEX_ASCII);
		}
	    }
	    if (getenv("_LUX_D_DEBUG") != NULL) {
		(void) g_dump("\nComplete Inquiry: ",
			(uchar_t *)&inq,
			min(inq.inq_len + 5, sizeof (inq)),
			HEX_ASCII);
	    }

	    }
	    path_index++;
	}
}


/*
 * adm_start() Spin up the given list
 * of SENA devices.
 *
 * RETURNS:
 *	none.
 */
static void
adm_start(char **argv, int option_t_input)
{
char		*path_phys = NULL;
Path_struct	*path_struct;
int		err = 0;

	if (Options & OPTION_T) {
		ssa_cli_start(&(*argv), option_t_input);
	} else {
	    while (*argv != NULL) {
		if ((err = l_convert_name(*argv, &path_phys,
			&path_struct, Options & PVERBOSE)) != 0) {
			(void) fprintf(stdout,
				MSGSTR(33,
					" Error: converting"
					" %s to physical path.\n"
					" Invalid pathname.\n"),
				*argv);
			if (err != -1) {
				(void) print_errString(err, *argv);
			}
			(argv)++;
			continue;
		}
		VERBPRINT(MSGSTR(101, "Issuing start to:\n %s\n"), *argv);
		if (err = g_start(path_phys))  {
			(void) print_errString(err, *argv);
			(argv)++;
			continue;
		}
		(argv)++;
	    }
	}
}



/*
 * adm_stop() Spin down a
 * given list of SENA devices.
 *
 * RETURNS:
 *	none.
 */
static void
adm_stop(char **argv, int option_t_input)
{
char		*path_phys = NULL;
Path_struct	*path_struct;
int		err = 0;

	if (Options & OPTION_T) {
		ssa_cli_stop(&(*argv), option_t_input);
	} else {
	    while (*argv != NULL) {
		if ((err = l_convert_name(*argv, &path_phys,
			&path_struct, Options & PVERBOSE)) != 0) {
			(void) fprintf(stdout,
				MSGSTR(33,
					" Error: converting"
					" %s to physical path.\n"
					" Invalid pathname.\n"),
				*argv);
			if (err != -1) {
				(void) print_errString(err, *argv);
			}
			(argv)++;
			continue;
		}
		VERBPRINT(MSGSTR(100, "Issuing stop to:\n %s\n"), *argv);
		if (err = g_stop(path_phys, 1))  {
			(void) print_errString(err, *argv);
			(argv)++;
			continue;
		}
		(argv)++;
	    }
	}
}


/*
 * Offline or Online an FCA port depending on the flag passed.
 * The laser is turned off/on on a USOC chip.
 * On a SOC+ chip, the port is either put into (offline) or pulled out
 * of (online) a loopback mode since the laser cannot be turned on or off.
 * As of this writing, this feature is yet to be supported by the ifp
 * driver on a QLogic card.
 * INPUT :
 *	Command line args and flag - P_ONLINE or P_OFFLINE
 *	The path that is passed has to be the physical path to the port.
 *	For example :
 *	/devices/sbus@2,0/SUNW,socal@2,0:0
 *	/devices/sbus@2,0/SUNW,usoc@2,0:0
 *	/devices/io-unit@f,e0200000/sbi@0,0/SUNW,socal@2,0:0
 *	/devices/pci@1f,4000/SUNW,ifp@2:devctl
 * RETURNS :
 *	Nothing
 */
static void
adm_port_offline_online(char *argv[], int flag)
{
	int		err;
	char		*path_phys = NULL;
	char		*nexus_path_ptr = NULL;
	Path_struct	*path_struct = NULL;

	while (*argv != NULL) {
		if ((err = l_convert_name(*argv, &path_phys,
			&path_struct, Options & PVERBOSE)) != 0) {
			(void) fprintf(stdout,
				MSGSTR(33,
					" Error: converting"
					" %s to physical path.\n"
					" Invalid pathname.\n"),
				*argv);
			if (err != -1) {
				(void) print_errString(err, *argv);
			}
			argv++;
			continue;
		}

		/* Get the nexus path - need this to print messages */
		if ((err = g_get_nexus_path(path_phys, &nexus_path_ptr)) != 0) {
			(void) print_errString(err, *argv);
			goto cleanup_and_go;
		}

		if (flag == P_OFFLINE) {
			if ((err = g_port_offline(nexus_path_ptr))) {
				(void) print_errString(err, nexus_path_ptr);
				goto cleanup_and_go;
			}
			fprintf(stdout,
				MSGSTR(2223, "Port %s has been disabled\n"),
					nexus_path_ptr);
		} else if (flag == P_ONLINE) {
			if ((err = g_port_online(nexus_path_ptr))) {
				(void) print_errString(err, nexus_path_ptr);
				goto cleanup_and_go;
			}
			fprintf(stdout,
				MSGSTR(2224, "Port %s has been enabled\n"),
					nexus_path_ptr);
		} else {
			(void) fprintf(stderr,
					MSGSTR(2225,
					"Unknown action requested "
					"on port - %d\nIgnoring."),
					flag);
		}
cleanup_and_go:
		free(path_phys);
		free(path_struct);
		free(nexus_path_ptr);
		argv++;
	}
}

/*
 * Definition of getaction() routine which does keyword parsing
 *
 * Operation: A character string containing the ascii cmd to be
 * parsed is passed in along with an array of structures.
 * The 1st struct element is a recognizable cmd string, the second
 * is the minimum number of characters from the start of this string
 * to succeed on a match. For example, { "offline", 3, ONLINE }
 * will match "off", "offli", "offline", but not "of" nor "offlinebarf"
 * The third element is the {usually but not necessarily unique}
 * integer to return on a successful match. Note: compares are cAsE insensitive.
 *
 * To change, extend or use this utility, just add or remove appropriate
 * lines in the structure initializer below and in the #define	s for the
 * return values.
 *
 *                              N O T E
 * Do not change the minimum number of characters to produce
 * a match as someone may be building scripts that use this
 * feature.
 */
struct keyword {
	char *match;		/* Character String to match against */
	int  num_match;		/* Minimum chars to produce a match */
	int  ret_code;		/* Value to return on a match */
};

static  struct keyword Keywords[] = {
	{"display",		2, DISPLAY},
	{"download",		3, DOWNLOAD},
	{"enclosure_names",	2, ENCLOSURE_NAMES},
	{"fast_write",		3, FAST_WRITE},
	{"fcal_s_download",	4, FCAL_UPDATE},
	{"fc_s_download",	4, FC_UPDATE},
	{"fcode_download",	4, FCODE_UPDATE},
	{"inquiry",		2, INQUIRY},
	{"insert_device",	3, INSERT_DEVICE},
	{"led",			3, LED},
	{"led_on",		5, LED_ON},
	{"led_off",		5, LED_OFF},
	{"led_blink",		5, LED_BLINK},
	{"nvram_data",		2, NVRAM_DATA},
	{"password",		2, PASSWORD},
	{"perf_statistics",	2, PERF_STATISTICS},
	{"power_on",		8, POWER_ON},
	{"power_off",		9, POWER_OFF},
	{"probe",		2, PROBE},
	{"purge",		2, PURGE},
	{"qlgc_s_download",	4, QLGC_UPDATE},
	{"remove_device",	3, REMOVE_DEVICE},
	{"reserve",		5, RESERVE},
	{"release",		3, RELEASE},
	{"set_boot_dev",	5, SET_BOOT_DEV},
	{"start",		3, START},
	{"stop",		3, STOP},
	{"sync_cache",		2, SYNC_CACHE},
	{"env_display",		5, ENV_DISPLAY},
	{"alarm",		5, ALARM},
	{"alarm_off",		8, ALARM_OFF},
	{"alarm_on",		8, ALARM_ON},
	{"alarm_set",		9, ALARM_SET},
	{"rdls",		2, RDLS},
	{"bypass",		3, BYPASS},
	{"enable",		3, ENABLE},
	{"p_bypass",		3, P_BYPASS},
	{"p_enable",		3, P_ENABLE},
	{"p_offline",		4, P_OFFLINE},
	{"p_online",		4, P_ONLINE},
	{"forcelip",		2, FORCELIP},
	{"dump",		2, DUMP},
	{"check_file",		2, CHECK_FILE},
	{"dump_map",		2, DUMP_MAP},
	{"sysdump",		5, SYSDUMP},
	{"version",		2, VERSION},
	/* hotplugging device operations */
	{"online",		2, DEV_ONLINE},
	{"offline",		2, DEV_OFFLINE},
	{"dev_getstate",	5, DEV_GETSTATE},
	{"dev_reset",		5, DEV_RESET},
	/* hotplugging bus operations */
	{"bus_quiesce",		5, BUS_QUIESCE},
	{"bus_unquiesce",	5, BUS_UNQUIESCE},
	{"bus_getstate",	5, BUS_GETSTATE},
	{"bus_reset",		9, BUS_RESET},
	{"bus_resetall",	12, BUS_RESETALL},
	/* hotplugging "helper" subcommands */
	{"replace_device",	3, REPLACE_DEVICE},
	{ NULL,			0, 0}
};

#ifndef	EOK
static	const	EOK	= 0;	/* errno.h type success return code */
#endif

/*
 * function getaction() takes a character string, cmd, and
 * tries to match it against a passed structure of known cmd
 * character strings. If a match is found, corresponding code
 * is returned in retval. Status returns as follows:
 *   EOK	= Match found, look for cmd's code in retval
 *   EFAULT = One of passed parameters was bad
 *   EINVAL = cmd did not match any in list
 */
static int
getaction(char *cmd, struct keyword *matches, int  *retval)
{
int actlen;

	/* Idiot checking of pointers */
	if (! cmd || ! matches || ! retval ||
	    ! (actlen = strlen(cmd)))	/* Is there an cmd ? */
	    return (EFAULT);

	    /* Keep looping until NULL match string (end of list) */
	    while (matches->match) {
		/*
		 * Precedence: Make sure target is no longer than
		 * current match string
		 * and target is at least as long as
		 * minimum # match chars,
		 * then do case insensitive match
		 * based on actual target size
		 */
		if ((((int)strlen(matches->match)) >= actlen) &&
		    (actlen >= matches->num_match) &&
		    /* can't get strncasecmp to work on SCR4 */
		    /* (strncasecmp(matches->match, cmd, actlen) == 0) */
		    (strncmp(matches->match, cmd, actlen) == 0)) {
		    *retval = matches->ret_code;	/* Found our match */
		    return (EOK);
		} else {
		    matches++;		/* Next match string/struct */
		}
	    }	/* End of matches loop */
	return (EINVAL);

}	/* End of getaction() */


/* main functions. */
main(int argc, char **argv)
{
register	    c;
/* getopt varbs */
extern char *optarg;
char		*optstring = NULL;
int		path_index, err = 0;
int		cmd = 0;		/* Cmd verb from cmd line */
int		exit_code = 0;		/* exit code for program */
int		temp_fd;		/* For -f option */
char		*wwn = NULL;
char		*file_name = NULL;
int		option_t_input;
char		*path_phys = NULL;
Path_struct	*path_struct;
char		*get_phys_path;

	whoami = argv[0];
	p_error_msg_ptr = NULL;


	/*
	 * Enable locale announcement
	 */
	g_i18n_catopen();

	while ((c = getopt(argc, argv, "ve"))
	    != EOF) {
	    switch (c) {
		case 'v':
		    Options |= PVERBOSE;
		    break;
		case 'e':
		    Options |= EXPERT;
		    break;
		default:
		    /* Note: getopt prints an error if invalid option */
		    USEAGE()
		    exit(-1);
	    } /* End of switch(c) */
	}
	setbuf(stdout, NULL);	/* set stdout unbuffered. */

	/*
	 * Build any i18n global variables
	 */
	dtype[0] = MSGSTR(2192, "Disk device");
	dtype[1] = MSGSTR(2193, "Tape device");
	dtype[2] = MSGSTR(2194, "Printer device");
	dtype[3] = MSGSTR(2195, "Processor device");
	dtype[4] = MSGSTR(2196, "WORM device");
	dtype[5] = MSGSTR(2197, "CD-ROM device");
	dtype[6] = MSGSTR(2198, "Scanner device");
	dtype[7] = MSGSTR(2199, "Optical memory device");
	dtype[8] = MSGSTR(2200, "Medium changer device");
	dtype[9] = MSGSTR(2201, "Communications device");
	dtype[10] = MSGSTR(107, "Graphic arts device");
	dtype[11] = MSGSTR(107, "Graphic arts device");
	dtype[12] = MSGSTR(2202, "Array controller device");
	dtype[13] = MSGSTR(2203, "SES device");
	dtype[14] = MSGSTR(71, "Reserved");
	dtype[15] = MSGSTR(71, "Reserved");



	/*
	 * Get subcommand.
	 */
	if ((getaction(argv[optind], Keywords, &cmd)) == EOK) {
		optind++;
		if ((cmd != PROBE) && (cmd != FCAL_UPDATE) &&
		(cmd != QLGC_UPDATE) && (cmd != FCODE_UPDATE) &&
		(cmd != FC_UPDATE) && (cmd != INSERT_DEVICE) &&
		(cmd != SYSDUMP) && (cmd != AU) &&
		(optind >= argc)) {
			(void) fprintf(stderr,
			MSGSTR(2204,
			"Error: enclosure or pathname not specified.\n"));
			USEAGE();
			exit(-1);
		}
	} else {
		(void) fprintf(stderr,
		MSGSTR(2205, "%s: subcommand not specified.\n"),
		whoami);
		USEAGE();
		exit(-1);
	}

	/* Extract & Save subcommand options */
	if ((cmd == ENABLE) || (cmd == BYPASS)) {
		optstring = "Ffrab";
	} else if (cmd == FCODE_UPDATE) {
		optstring = "pd:";
	} else if (cmd == REMOVE_DEVICE) {
		optstring = "F";
	} else {
		optstring = "Fryszabepcdlvt:f:w:";
	}
	while ((c = getopt(argc, argv, optstring)) != EOF) {
	    switch (c) {
		case 'a':
			Options |= OPTION_A;
			break;
	    case 'b':
			Options |= OPTION_B;
			break;
		case 'c':
			Options |= OPTION_C;
			break;
		case 'd':
			Options |= OPTION_D;
			if (cmd == FCODE_UPDATE) {
			    file_name = optarg;
			}
			break;
		case 'e':
			Options |= OPTION_E;
			break;
		case 'f':
			Options |= OPTION_F;
			if (!((cmd == ENABLE) || (cmd == BYPASS))) {
				file_name = optarg;
			}
			break;
		case 'F':
			Options |= OPTION_CAPF;
			break;
		case 'l':
		    Options |= OPTION_L;
		    break;
		case 'p':
		    Options |= OPTION_P;
		    break;
		case 'r':
		    Options |= OPTION_R;
		    break;
		case 's':
		    Options |= SAVE;
		    break;
		case 't':
		    Options |= OPTION_T;
		    option_t_input = atoi(optarg);
		    break;
		case 'v':
		    Options |= OPTION_V;
		    break;
		case 'w':
		    Options |= OPTION_W;
		    wwn = optarg;
		    break;
		case 'z':
		    Options |= OPTION_Z;
		    break;
		case 'y':
		    Options |= OPTION_Y;
		    break;
		default:
		    /* Note: getopt prints an error if invalid option */
		    USEAGE()
		    exit(-1);
	    } /* End of switch(c) */
	}
	if ((cmd != PROBE) && (cmd != FCAL_UPDATE) &&
	    (cmd != QLGC_UPDATE) && (cmd != FCODE_UPDATE) &&
	    (cmd != FC_UPDATE) && (cmd != INSERT_DEVICE) &&
	    (cmd != SYSDUMP) && (cmd != AU) &&
	    (optind >= argc)) {
	    (void) fprintf(stderr,
		MSGSTR(2206,
		"Error: enclosure or pathname not specified.\n"));
	    USEAGE();
	    exit(-1);
	}
	path_index = optind;

	/*
	 * Check if the file supplied with the -f option is valid
	 * Some sub commands (bypass for example) use the -f option
	 * for other reasons. In such cases, "file_name" should be
	 * NULL.
	 */
	if ((file_name != NULL) && (Options & OPTION_F)) {
		if ((temp_fd = open(file_name, O_RDONLY)) == -1) {
			perror(file_name);
			exit(-1);
		} else {
			close(temp_fd);
		}
	}

	switch (cmd)	{
	    case	DISPLAY:
		if (Options &
		    ~(PVERBOSE | OPTION_A | OPTION_Z | OPTION_R |
		    OPTION_P | OPTION_V | OPTION_L | OPTION_E | OPTION_T)) {
		    USEAGE();
		    exit(-1);
		}
		/* Display object(s) */
		exit_code = adm_display_config(&argv[path_index],
			option_t_input, argc - path_index);
		break;

	    case	DOWNLOAD:
		    if (Options &
			~(PVERBOSE | OPTION_F | OPTION_W | SAVE)) {
			USEAGE();
			exit(-1);
		    }
		    adm_download(&argv[path_index], file_name, wwn);
		    break;

	    case	ENCLOSURE_NAMES:
		    if (Options & ~PVERBOSE) {
			USEAGE();
			exit(-1);
		    }
		    up_encl_name(&argv[path_index], argc);
		    break;

	    case	FAST_WRITE:
		    if (Options &
			~(PVERBOSE | OPTION_E | OPTION_D | OPTION_C | SAVE)) {
		    USEAGE();
		    exit(-1);
		}
		if (!((Options & (OPTION_E | OPTION_D | OPTION_C)) &&
		    !((Options & OPTION_E) &&
		    (Options & (OPTION_D | OPTION_C))) &&
		    !((Options & OPTION_D) && (Options & OPTION_C)))) {
		    USEAGE();
		    exit(-1);
		}
		ssa_fast_write(argv[path_index]);
		break;

	    case	INQUIRY:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		adm_inquiry(&argv[path_index]);
		break;

	    case	NVRAM_DATA:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		while (argv[path_index] != NULL) {
			if ((get_phys_path =
			    g_get_physical_name(argv[path_index])) == NULL) {

				(void) fprintf(stderr, "%s: ", whoami);
				(void) fprintf(stdout,
				MSGSTR(112, "Error: Invalid pathname (%s)"),
					argv[path_index]);
				(void) fprintf(stdout, "\n");
				exit(-1);
			}
			ssa_cli_display_config(&argv[path_index], get_phys_path,
				option_t_input, 1, argc - path_index);
			path_index++;
		}
		break;

	    case	PERF_STATISTICS:
		if (Options & ~(PVERBOSE | OPTION_D | OPTION_E)) {
		    USEAGE();
		    exit(-1);
		}
		if (!((Options & (OPTION_E | OPTION_D)) &&
		    !((Options & OPTION_E) && (Options & OPTION_D)))) {
		    USEAGE();
		    exit(-1);
		}
		(void) ssa_perf_statistics(argv[path_index]);
		break;

	    case	PURGE:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		VERBPRINT(MSGSTR(2207,
			"Throwing away all data in the NV_RAM for:\n %s\n"),
		    argv[path_index]);
		if ((get_phys_path =
		    g_get_physical_name(argv[path_index])) == NULL) {

			(void) fprintf(stderr, "%s: ", whoami);
			(void) fprintf(stdout,
				MSGSTR(112, "Error: Invalid pathname (%s)"),
				argv[path_index]);
			(void) fprintf(stdout, "\n");
			exit(-1);
		}
		if (p_purge(get_phys_path)) {
		    L_ERR_PRINT;
		    exit(-1);
		}
		break;

	    case	PROBE:
		if (Options & ~(PVERBOSE | OPTION_P)) {
			USEAGE();
			exit(-1);
		}
		/*
		 * A special check just in case someone entered
		 * any characters after the -p or the probe.
		 *
		 * (I know, a nit.)
		 */
		if (((Options & PVERBOSE) && (Options & OPTION_P) &&
			(argc != 4)) ||
			(!(Options & PVERBOSE) && (Options & OPTION_P) &&
			(argc != 3)) ||
			((Options & PVERBOSE) && (!(Options & OPTION_P)) &&
			(argc != 3)) ||
			(!(Options & PVERBOSE) && (!(Options & OPTION_P)) &&
			(argc != 2))) {
			(void) fprintf(stderr,
			MSGSTR(114, "Error: Incorrect number of arguments.\n"));
			(void) fprintf(stderr,  MSGSTR(2208,
			"Usage: %s [-v] subcommand [option]\n"), whoami);
			exit(-1);
		}
		pho_probe();
		ssa_probe();
		non_encl_probe();
		break;

	    case	FCODE_UPDATE:	/* Update Fcode in all cards */
			if ((Options & ~(PVERBOSE)) &
			    ~(OPTION_P | OPTION_D) || argv[path_index]) {
				USEAGE();
				exit(-1);
			}
			if (adm_fcode(Options & PVERBOSE, file_name) != 0) {
				exit(-1);
			}
			break;

	    case	QLGC_UPDATE:	/* Update Fcode in PCI HBA card(s) */
			if ((Options & ~(PVERBOSE)) & ~(OPTION_F) ||
			    argv[path_index]) {
				USEAGE();
				exit(-1);
			}
			if (q_qlgc_update(Options & PVERBOSE,
			    file_name) != 0) {
				exit(-1);
			}
			break;

	    case	FCAL_UPDATE:	/* Update Fcode in Sbus soc+ card */
			if ((Options & ~(PVERBOSE)) & ~(OPTION_F) ||
			    argv[path_index]) {
				USEAGE();
				exit(-1);
			}
			(void) fcal_update(Options & PVERBOSE, file_name);
			break;

	    case	FC_UPDATE:	/* Update Fcode in Sbus soc card */
			if (((Options & ~(PVERBOSE)) & ~(OPTION_F) &
				~(OPTION_CAPF)) || argv[path_index]) {
				USEAGE();
				exit(-1);
			}
			(void) fc_update(Options & PVERBOSE,
				Options & OPTION_CAPF, file_name);
		break;

	    case	SET_BOOT_DEV:   /* Set boot-device variable in nvram */
			exit_code = setboot(Options & OPTION_Y,
				Options & PVERBOSE, argv[path_index]);
		break;

	    case	LED:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		adm_led(&argv[path_index], L_LED_STATUS);
		break;
	    case	LED_ON:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		adm_led(&argv[path_index], L_LED_ON);
		break;
	    case	LED_OFF:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		adm_led(&argv[path_index], L_LED_OFF);
		break;
	    case	LED_BLINK:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		adm_led(&argv[path_index], L_LED_RQST_IDENTIFY);
		break;
	    case	PASSWORD:
		if (Options & ~(PVERBOSE))  {
			USEAGE();
			exit(-1);
		}
		up_password(&argv[path_index]);
		break;

	    case	RESERVE:
		if (Options & (~PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		VERBPRINT(MSGSTR(2209,
			"  Reserving: \n %s\n"), argv[path_index]);
		if ((path_phys =
		    g_get_physical_name(argv[path_index])) == NULL) {

			(void) fprintf(stderr, "%s: ", whoami);
			(void) fprintf(stdout,
				MSGSTR(112, "Error: Invalid pathname (%s)"),
				argv[path_index]);
			(void) fprintf(stdout, "\n");
			exit(-1);
		}
		if (err = g_reserve(path_phys)) {
		    (void) print_errString(err, argv[path_index]);
		    exit(-1);
		}
		break;

	    case	RELEASE:
		if (Options & (~PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		VERBPRINT(MSGSTR(2210, "  Canceling Reservation for:\n %s\n"),
		    argv[path_index]);
		if ((path_phys =
		    g_get_physical_name(argv[path_index])) == NULL) {

			(void) fprintf(stderr, "%s: ", whoami);
			(void) fprintf(stdout,
				MSGSTR(112, "Error: Invalid pathname (%s)"),
				argv[path_index]);
			(void) fprintf(stdout, "\n");
			exit(-1);

		}
		if (err = g_release(path_phys)) {
		    (void) print_errString(err, argv[path_index]);
		    exit(-1);
		}
		break;

	    case	START:
		if (Options & ~(PVERBOSE | OPTION_T)) {
			USEAGE();
			exit(-1);
		}
		adm_start(&argv[path_index], option_t_input);
		break;

	    case	STOP:
		if (Options & ~(PVERBOSE | OPTION_T)) {
			USEAGE();
			exit(-1);
		}
		adm_stop(&argv[path_index], option_t_input);
		break;
	    case	SYNC_CACHE:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		VERBPRINT(MSGSTR(2211, "Flushing the NV_RAM buffer of "
		    "all writes for:\n %s\n"),
		    argv[path_index]);
		if ((get_phys_path =
		    g_get_physical_name(argv[path_index])) == NULL) {
			(void) fprintf(stderr, "%s: ", whoami);
			(void) fprintf(stdout,
				MSGSTR(112, "Error: Invalid pathname (%s)"),
				argv[path_index]);
			(void) fprintf(stdout, "\n");
			exit(-1);

		}
		if (p_sync_cache(get_phys_path)) {
		    L_ERR_PRINT;
		    exit(-1);
		}
		break;
	    case	ENV_DISPLAY:
		cli_display_envsen_data(&argv[path_index], argc - path_index);
		break;
	    case	ALARM:
		alarm_enable(&argv[path_index], 0, argc - path_index);
		break;
	    case	ALARM_OFF:
		alarm_enable(&argv[path_index], OPTION_D, argc - path_index);
		break;
	    case	ALARM_ON:
		alarm_enable(&argv[path_index], OPTION_E, argc - path_index);
		break;
	    case	ALARM_SET:
		alarm_set(&argv[path_index], argc - path_index);
		break;
	    case	POWER_OFF:
		if (Options & ~(PVERBOSE | OPTION_CAPF)) {
			USEAGE();
			exit(-1);
		}
		adm_power_off(&argv[path_index], argc - path_index, 1);
		break;

	    case	POWER_ON:
		if (Options & (~PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		adm_power_off(&argv[path_index], argc - path_index, 0);
		break;

	/*
	 * EXPERT commands.
	 */

	    case	FORCELIP:
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT))) {
			E_USEAGE();
			exit(-1);
		}
		adm_forcelip(&argv[path_index]);
		break;

	    case	BYPASS:
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT |
			OPTION_CAPF | OPTION_A | OPTION_B | OPTION_F |
			OPTION_R)) || !(Options & (OPTION_A | OPTION_B)) ||
			((Options & OPTION_A) && (Options & OPTION_B))) {
			E_USEAGE();
			exit(-1);
		}
		adm_bypass_enable(&argv[path_index], argc - path_index, 1);
		break;

	    case	ENABLE:
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT |
			OPTION_CAPF | OPTION_A | OPTION_B | OPTION_F |
			OPTION_R)) || !(Options & (OPTION_A | OPTION_B)) ||
			((Options & OPTION_A) && (Options & OPTION_B))) {
			E_USEAGE();
			exit(-1);
		}
		adm_bypass_enable(&argv[path_index], argc - path_index, 0);
		break;
	    case	P_BYPASS:
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT))) {
			E_USEAGE();
			exit(-1);
		}
		if ((err = l_convert_name(argv[path_index], &path_phys,
			&path_struct, Options & PVERBOSE)) != 0) {
			(void) fprintf(stdout,
				MSGSTR(33,
					" Error: converting"
					" %s to physical path.\n"
					" Invalid pathname.\n"),
				argv[path_index]);
			if (err != -1) {
				(void) print_errString(err, argv[path_index]);
			}
			exit(-1);
		}
		if (err = d_p_bypass(path_phys, Options & PVERBOSE)) {
		    (void) print_errString(err, argv[path_index]);
		    exit(-1);
		}
		break;

	    case	P_ENABLE:
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT))) {
			E_USEAGE();
			exit(-1);
		}
		if ((err = l_convert_name(argv[path_index], &path_phys,
			&path_struct, Options & PVERBOSE)) != 0) {
			(void) fprintf(stdout,
					MSGSTR(33,
					" Error: converting"
					" %s to physical path.\n"
					" Invalid pathname.\n"),
					argv[path_index]);
			if (err != -1) {
				(void) print_errString(err, argv[path_index]);
			}
			exit(-1);
		}
		if (err = d_p_enable(path_phys, Options & PVERBOSE)) {
		    (void) print_errString(err, argv[path_index]);
		    exit(-1);
		}
		break;

	    case	P_OFFLINE:	/* Offline a port */
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT))) {
			E_USEAGE();
			exit(-1);
		}
		adm_port_offline_online(&argv[path_index], P_OFFLINE);
		break;

	    case	P_ONLINE:	/* Online a port */
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT))) {
			E_USEAGE();
			exit(-1);
		}
		adm_port_offline_online(&argv[path_index], P_ONLINE);
		break;

	    case	RDLS:
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT))) {
			E_USEAGE();
			exit(-1);
		}
		display_link_status(&argv[path_index]);
		break;

	/*
	 * Undocumented commands.
	 */

	    case	CHECK_FILE:	/* Undocumented Cmd */
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		/* check & display download file parameters */
		if (err = l_check_file(argv[path_index],
		    (Options & PVERBOSE))) {
		    (void) print_errString(err, argv[path_index]);
		    exit(-1);
		}
		(void) fprintf(stdout, MSGSTR(2212, "Download file O.K. \n\n"));
		break;

	    case	DUMP:		/* Undocumented Cmd */
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT))) {
			USEAGE();
			exit(-1);
		}
		dump(&argv[path_index]);
		break;

	    case	DUMP_MAP:	/* Undocumented Cmd */
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT))) {
			USEAGE();
			exit(-1);
		}
		dump_map(&argv[path_index]);
		break;

	    case	SYSDUMP:
			if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		if (err = sysdump(Options & PVERBOSE)) {
		    (void) print_errString(err, NULL);
		    exit(-1);
		}
		break;

	    case	VERSION:
		break;


	    case	INSERT_DEVICE:
			if (argv[path_index] == NULL) {
				if ((err = h_insertSena_fcdev()) != 0) {
					(void) print_errString(err, NULL);
					exit(-1);
				}
			} else if ((err = hotplug(INSERT_DEVICE,
					&argv[path_index],
					Options & PVERBOSE,
					Options & OPTION_CAPF)) != 0) {
				(void) print_errString(err, argv[path_index]);
				exit(-1);
			}
			break;
	    case	REMOVE_DEVICE:
			if (err = hotplug(REMOVE_DEVICE, &argv[path_index],
			    Options & PVERBOSE, Options & OPTION_CAPF)) {
			    (void) print_errString(err, argv[path_index]);
			    exit(-1);
			}
			break;

	    case	REPLACE_DEVICE:
		if (err = hotplug(REPLACE_DEVICE, &argv[path_index],
		    Options & PVERBOSE, Options & OPTION_CAPF)) {
			(void) print_errString(err, argv[path_index]);
			exit(-1);
		}
		break;

	/* for hotplug device operations */
	    case	DEV_ONLINE:
	    case	DEV_OFFLINE:
	    case	DEV_GETSTATE:
	    case	DEV_RESET:
	    case	BUS_QUIESCE:
	    case	BUS_UNQUIESCE:
	    case	BUS_GETSTATE:
	    case	BUS_RESET:
	    case	BUS_RESETALL:
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT))) {
			E_USEAGE();
			exit(-1);
		}
		if (hotplug_e(cmd, &argv[path_index],
		    Options & PVERBOSE, Options & OPTION_CAPF) != 0) {
			return (-1);
		}
		break;

	    default:
		(void) fprintf(stderr,
		    MSGSTR(2213, "%s: subcommand decode failed.\n"),
		    whoami);
		USEAGE();
		exit(-1);
	}
	return (exit_code);
}
