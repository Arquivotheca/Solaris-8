/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)funct.c	1.10	99/08/13 SMI"

/*LINTLIBRARY*/


/*
 *  This module is part of the Command Interface Library
 *  for the Pluto User Interface.
 */

/*
 * I18N message number ranges
 *  This file: 7000 - 7499
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
#include	<libintl.h>	    /* gettext */
/* SVR4 */
#include	<sys/sunddi.h>
#include	<sys/systeminfo.h>
#include	<sys/scsi/scsi.h>
#include	<dirent.h>	/* for DIR */
#include	<sys/vtoc.h>
#include	<sys/dkio.h>
#include	<sys/utsname.h>
#include	<strings.h> /* for bcopy */
#include	<sys/exechdr.h>

#include	<g_state.h>
#include	"ssadef.h"
#include	"hdrs/rom.h"
#include	"state.h"
#include	"scsi.h"
#include	"stgcom.h"

#define	VERBPRINT	if (verbose_flag) (void) printf
#define	CTLR_POSTFIX	":ctlr"


/* global variables */
char 	*p_error_msg_ptr;	/* pointer to error message */
char	p_error_msg[80*100];	/* global error messsage - 100 lines long */

/* local variables */
static	P_state	pg_state, *state_ptr;		/* state of a Pluto */
static	P_perf_statistics	perf_st, *perf_ptr;	/* performance stat */
static  Wb_log_entry	wle_st;

/*	Index to status_strings */
#define	P_NDF 		0
#define	P_NS  		1
#define	P_NR  		2
#define	P_NRD 		3
#define	P_SPUN_DWN_D	4
#define	P_RESERVEDF	5
#define	P_NL  		6
#define	P_OPNF		7
#define	P_INQF		8


/*	external functions */
extern	void	init_pluto_status(int);
extern	int	p_get_drv_name(char *, char *);

/*	internal functions */
int	p_sync_cache(char *);
static	int	get_status(char *, int, int);
static	int	scsi_sync_cache_cmd(int);
static	int	uscsi_cmd(int, struct uscsi_cmd *, int);
static	int	scsi_download_code_cmd(int, uchar_t *, int, uchar_t);
static	int	scsi_upload_code_cmd(int, uchar_t *, int, int);
static	void	scsi_printerr(struct uscsi_cmd *, struct scsi_extended_sense *,
		int, char *);
static	char 	*p_decode_sense(uchar_t);
static	char 	*scsi_find_command_name(int);
static	void	string_dump(char *, uchar_t *, int, int, char *);
static	int	match_substr(char *, char *);
static	int	p_check_file(int, int, uchar_t **);


/*
 * Read Mode Sense page 21 from controller fd
 *
 * NOTE: Does NOT release the space allocated
 * if successfull so the calling routine must release
 *
 *
 */
static int
get_pg21(int fd,  uchar_t **pg21_read_buf)
{
Mode_header_10	*mode_header_ptr;
Ms_pg21	*pg21_ptr;
int	status, size;

	/*
	 * read page 21 from controller
	 */
	/*
	 * Read the first part of the page to get the page size
	 */
	size = 20;
	if ((*pg21_read_buf = (uchar_t *)g_zalloc(size)) == NULL) {
	    (void) close(fd);
	    return (errno);
	}
	/* read page */
	if (status = g_scsi_mode_sense_cmd(fd, *pg21_read_buf, size,
	    0, 0x21)) {
	    (void) close(fd);
	    g_destroy_data((char *)*pg21_read_buf);
	    return (status);
	}
	/* check */
	mode_header_ptr = (struct mode_header_10_struct *)(int)*pg21_read_buf;
	pg21_ptr = ((struct ms_pg21_struct *)((int)*pg21_read_buf +
	    (uchar_t)sizeof (struct mode_header_10_struct) +
	    (mode_header_ptr->bdesc_length)));
	/* check parameters for sanity */
	if ((pg21_ptr->pg_code != 0x21) ||
	    (pg21_ptr->pg_len != 8)) {
	    (void) sprintf(&p_error_msg[0],
		MSGSTR(7000, "Error reading page 21 from controller\n"));
	    p_error_msg_ptr = (char *)&p_error_msg;	/* set error pointer */
	    g_destroy_data((char *)*pg21_read_buf);
	    (void) close(fd);
	    return (RD_PG21);
	}

	size = mode_header_ptr->length + 2;
	g_destroy_data((char *)*pg21_read_buf);

	/*
	 * Now get the whole page
	 */
	if ((*pg21_read_buf = (uchar_t *)g_zalloc(size)) == NULL) {
	    (void) close(fd);
	    return (errno);
	}
	/* read page */
	if (status = g_scsi_mode_sense_cmd(fd, *pg21_read_buf, size,
	    0, 0x21)) {
	    (void) close(fd);
	    g_destroy_data((char *)*pg21_read_buf);
	    return (status);
	}
	P_DPRINTF("get_pg21: Read mode sense page 21 size=0x%x"
		"\n", size);

	return (0);
}


/*
 *	Generate status strings for each drive
 */
static void
build_labels()
{
uchar_t	p, t;

	char *status_strings[9];
	status_strings[0] = MSGSTR(7001, " -                ");
	status_strings[1] = MSGSTR(7002, "NO SELECT         ");
	status_strings[2] = MSGSTR(7003, "NOT READY         ");
	status_strings[3] = MSGSTR(7004, "NOT READABLE      ");
	status_strings[4] = MSGSTR(7005, "SPUN DOWN   D: X,X");
	status_strings[5] = MSGSTR(7006, "RESERVED          ");
	status_strings[6] = MSGSTR(7007, "NO UNIX LABEL     ");
	status_strings[7] = MSGSTR(7008, "OPEN FAILED       ");
	status_strings[8] = MSGSTR(7009, "INQUIRY FAILED    ");

	for (p = 0; p < state_ptr->c_tbl.num_ports; p++)
	    for (t = 0; t < state_ptr->c_tbl.num_tgts; t++) {
		/* Default string */
		(void) strcpy(state_ptr->drv[p][t].id1, "Drive: X,X ");
		state_ptr->drv[p][t].id1[7] = p + '0';
		state_ptr->drv[p][t].id1[9] = t + '0';
		if (state_ptr->drv[p][t].state_flags & DS_FWE) {
			(void) strcat(state_ptr->drv[p][t].id1, "(FW)   ");
		} else if (state_ptr->drv[p][t].state_flags & DS_PCFW) {
			(void) strcat(state_ptr->drv[p][t].id1, "(PCFW) ");
		} else
			(void) strcat(state_ptr->drv[p][t].id1, "       ");

		/*
		 *	Note: DS_NDF & DS_NDS & DS_CNR & DS_DNR are
		 *	assumed to be exclusive
		 *
		 *	The other states are not, for example a grouped
		 *	drive can be spun down and not have a UNIX label.
		 *
		 */
		if (state_ptr->drv[p][t].state_flags & DS_NDF) {
		    (void) strcpy(state_ptr->drv[p][t].id1,
			status_strings[P_NDF]);
		} else if (state_ptr->drv[p][t].state_flags & DS_NDS) {
		    (void) strcpy(state_ptr->drv[p][t].id1,
			status_strings[P_NS]);
		} else if (state_ptr->drv[p][t].state_flags & DS_DNR) {
		    (void) strcpy(state_ptr->drv[p][t].id1,
			status_strings[P_NR]);
		} else if (state_ptr->drv[p][t].state_flags & DS_CNR) {
		    (void) strcpy(state_ptr->drv[p][t].id1,
			status_strings[P_NRD]);
		} else if (state_ptr->drv[p][t].state_flags & DS_SPUN_DWN) {
		    (void) strcpy(state_ptr->drv[p][t].id1,
			status_strings[P_SPUN_DWN_D]);
		    state_ptr->drv[p][t].id1[15] = p + '0';
		    state_ptr->drv[p][t].id1[17] = t + '0';
		} else if (state_ptr->drv[p][t].no_label_flag) {
		    (void) strcpy(state_ptr->drv[p][t].id1,
			status_strings[P_NL]);
		} else if (state_ptr->drv[p][t].reserved_flag) {
		    (void) strcpy(state_ptr->drv[p][t].id1,
			status_strings[P_RESERVEDF]);
		} else if (state_ptr->drv[p][t].state_flags & DS_OPNF) {
		    (void) strcpy(state_ptr->drv[p][t].id1,
			status_strings[P_OPNF]);
		} else if (state_ptr->drv[p][t].state_flags & DS_INQF) {
		    (void) strcpy(state_ptr->drv[p][t].id1,
			status_strings[P_INQF]);
		}
	    }
}

char
ctoi(char c)
{
	if ((c >= '0') && (c <= '9'))
		c -= '0';
	else if ((c >= 'A') && (c <= 'F'))
		c = c - 'A' + 10;
	else if ((c >= 'a') && (c <= 'f'))
		c = c - 'a' + 10;
	else
		c = 0;
	return (c);
}

/*
 * This is the routine to check the SSA firmware.
 * Check the file for validity:
 *	- verify the size is that of 3 proms worth of text.
 *	- verify PROM_MAGIC.
 *	- verify (and print) the date.
 *	- verify the checksum.
 *	- verify the WWN == 0.
 * Since this requires reading the entire file, do it now and pass a pointer
 * to the allocated buffer back to the calling routine (which is responsible
 * for freeing it). If the buffer is not allocated, it will be NULL.
 */
static int
p_check_file(int fd, int verbose_flag, uchar_t **buf_ptr)
{
	struct	exec	the_exec;
	int		temp;
	int		i, j, offset;
	uchar_t		*buf;
	int		found = 0;
	uchar_t		date_str[26];
	int		*p;

	*buf_ptr = NULL;

	/* read exec header */
	if (lseek(fd, 0, SEEK_SET) == -1)
		return (errno);
	if ((temp = read(fd, (char *)&the_exec, sizeof (the_exec))) == -1) {
		(void) sprintf(p_error_msg,
		MSGSTR(7036, "Error reading download file exec header: %s\n"),
		strerror(errno));
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
		return (errno);
	}
	if (temp != sizeof (the_exec)) {
		(void) sprintf(p_error_msg,
		MSGSTR(7037, "Error reading exec header: incorrect number"
			" of bytes read\n"));
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
		return (P_DOWNLOAD_FILE);
	}

	if (the_exec.a_text != (PROMSIZE * 3)) {
		(void) sprintf(p_error_msg,
		MSGSTR(7038, "Error: Text segment wrong size: 0x%x "
			"(expecting 0x%x)\n"), the_exec.a_text, (PROMSIZE * 3));
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
		return (P_DOWNLOAD_FILE);
	}

	if (!(buf = (uchar_t *)calloc(1, (PROMSIZE * 3))))
		return (errno);
	if ((temp = read(fd, buf, (PROMSIZE * 3))) == -1) {
		(void) sprintf(p_error_msg,
			MSGSTR(7039, "Error reading download file: %s\n"),
						strerror(errno));
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
		(void) free(buf);
		return (errno);
	}
	if (temp != (PROMSIZE * 3)) {
		(void) sprintf(p_error_msg, MSGSTR(7040, "Error reading: "
			"incorrect number of bytes read\n"));
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
		(void) free(buf);
		return (P_DOWNLOAD_FILE);
	}

	/* check the SPARCstorage Array MAGIC */
	offset = ((PROM_MAGIC_PROM -1) * PROMSIZE) + PROM_MAGIC_OFF;
	j = * ((int *)((int)buf + offset));
	if (j != PROM_MAGIC) {
		(void) sprintf(p_error_msg,
			MSGSTR(7041, "Error: Bad SPARCstorage Array MAGIC\n"));
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
		(void) free(buf);
		return (P_DOWNLOAD_FILE);
	}

	/*
	* Make sure the date is a reasonable string before printing
	*
	* Must be NULL terminated
	* Must have ASCII characters ?Internationalization?
	*
	*/

	offset = ((DATE_PROM - 1) * PROMSIZE) + DATE_OFF + sizeof (long);
	bcopy((void *)(buf + offset), (void *)date_str, 26);
	for (found = 1, i = 0; i < 24; i++) {
		if ((date_str[i] == '\0') || (date_str[i] == '\n'))
			found = 0;
	}
	if ((date_str[24] != '\n') || (date_str[25] != '\0'))
		found = 0;
	if (!found) {
		(void) sprintf(p_error_msg,
			MSGSTR(7042, "Error: Bad SPARCstorage Array DATE\n"));
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
		(void) free(buf);
		return (P_DOWNLOAD_FILE);
	}

	VERBPRINT("SPARCstorage Array Prom set Date: %s\n", date_str);

	/* check the WWN */
	offset = ((WWN_PROM - 1) * PROMSIZE) + WWN_OFF;
	j = * ((int *)((int)buf + offset));
	if (j != 0) {
		(void) sprintf(p_error_msg,
			MSGSTR(7043, "Error: Non-zero WWN in file\n"));
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
		(void) free(buf);
		return (P_DOWNLOAD_FILE);
	}

	/*
	 * verify checksum
	 */
	for (j = 0, p = (int *)(int)buf, i = 0;
		i < (PROMSIZE * 3) / 4;
		i++, j ^= *p++);

	if (j != 0) {
		(void) sprintf(p_error_msg,
			MSGSTR(7044, "Download file checksum failed\n"));
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
		(void) free(buf);
		return (P_DOWNLOAD_CHKSUM);
	}

	/* file verified */
	*buf_ptr = buf;
	return (0);
}

/*
 * path		- physical path of SPARCstorage Array controller
 * file		- input file for new code (may be NULL)
 * wwn		- string of 12 hex digits representing new WWN (may be NULL).
 */
int
p_download(char *path, char *file, int ps, int verbose_flag, uchar_t *wwn)
{
int		controller_fd, fd;
int		err, status;
uchar_t		*buf_ptr;
int		offset;
uchar_t		*wwnp;
int		i, sum, *p;

	if (!ps) {
	    (void) sprintf(p_error_msg,
	MSGSTR(7010, "Download without save is not supported\n"));
	    p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
	    return (P_DOWNLOAD_FILE);
	}
	if (!file && !wwn)
		return (0);
	VERBPRINT(MSGSTR(7011, "Opening the SPARCstorage Array for I/O\n"));
	P_DPRINTF("Opening the SPARCstorage Array for I/O\n");
	if ((controller_fd = g_object_open(path, O_NDELAY | O_RDWR)) == -1)
		return (errno);
	if (file) {
		VERBPRINT(MSGSTR(7012,
			"Doing download (and saving firmware) to:"
				"\n\t%s\nFrom file: %s\n"), path, file);
		P_DPRINTF("Doing download (and saving firmware) to:"
				"\n\t%s\nFrom file: %s\n", path, file);

		if ((fd = open(file, O_NDELAY|O_RDONLY)) == -1) {
			close(controller_fd);
			return (errno);
		}
		if (err = p_check_file(fd, verbose_flag, &buf_ptr)) {
			close(controller_fd);
			close(fd);
			return (err);
		}
		close(fd);

		VERBPRINT(MSGSTR(127, "Checkfile O.K."));
		VERBPRINT("\n");
		P_DPRINTF("Checkfile OK.\n");
	} else { /* No file to open; read the old image */
		if ((buf_ptr = g_zalloc(PROMSIZE * 3)) == NULL) {
			close(controller_fd);
			return (errno);
		}
		VERBPRINT(MSGSTR(7013,
			"Reading old firmware from SPARCstorage Array\n"));
		P_DPRINTF("Reading old firmware from SPARCstorage Array\n");
		if (status = scsi_upload_code_cmd
			(controller_fd,
			buf_ptr, PROMSIZE * 3, 0)) {
			(void) g_destroy_data((char *)buf_ptr);
			close(controller_fd);
			return (status);
		}
	}

	offset = ((WWN_PROM - 1) * PROMSIZE) + WWN_OFF;
	wwnp = (uchar_t *)(buf_ptr + offset);
	if (!wwn) { /* get WWN from controller */
		VERBPRINT(MSGSTR(7014,
			"Reading old WWN from SPARCstorage Array\n"));
		P_DPRINTF("Reading old WWN from SPARCstorage Array\n");
		if (status = scsi_upload_code_cmd
			(controller_fd, wwnp, WWN_SIZE, offset)) {
			(void) g_destroy_data((char *)buf_ptr);
			close(controller_fd);
			return (status);
		}
	} else { /* use the new WWN */
		g_string_to_wwn(wwn, wwnp);
	}

	/* compute the new checksum */
	offset = ((CHECKSUM_PROM - 1) * PROMSIZE) + CHECKSUM_OFF;
	*((int *)((int)buf_ptr + offset)) = 0;
	for (sum = 0, p = (int *)(int)buf_ptr, i = 0;
		i < (PROMSIZE * 3) / 4;
		i++, sum ^= *p++);
	VERBPRINT(MSGSTR(7015, "New checksum = 0x%x \n"), sum);
	P_DPRINTF("New checksum = 0x%x \n", sum);
	*((int *)((int)buf_ptr + offset)) = sum;


	VERBPRINT(MSGSTR(7016, "Writing new image to SPARCstorage Array\n"));
	P_DPRINTF("Writing new image to SPARCstorage Array\n");
	status = scsi_download_code_cmd(controller_fd,
		buf_ptr, (PROMSIZE * 3), 1);
	(void) close(controller_fd);
	(void) g_destroy_data((char *)buf_ptr);
	return (status);
}

int
p_fast_write(char *path, int pcfw_flag, int fwe_flag, uchar_t save)
{
int	fd;
int	error_code;
char	c_path_phys[MAXPATHLEN];
char	*char_ptr;
struct	{
	Mode_header_10 hdr;
	Ms_pg23	pg23;		/* local page 23  */
} select_page;
struct	utsname name;

	/* save path passed in local copy */
	(void) strcpy(c_path_phys, path);
	/*  If pathname not already :ctlr then convert */
	if (strstr(c_path_phys, CTLR_POSTFIX) == NULL) {
	    /* point to place to add */
	    char_ptr = strrchr(c_path_phys, '/');
	    *char_ptr = '\0';	/* Terminate sting  */
	    (void) strcat(c_path_phys, CTLR_POSTFIX);
	}

	/* open controller */
	if ((fd = g_object_open(c_path_phys, O_NDELAY | O_RDWR)) == -1)
		return (errno);

	/* set up general parameters for page */
	(void) memset((char *)&select_page, 0, sizeof (select_page));
	select_page.hdr.length = ((int)sizeof (select_page.pg23)
	    + (int)sizeof (select_page.hdr) -2);
	select_page.pg23.pg_code = 0x23;	/* page code */
	select_page.pg23.pg_len = sizeof (struct ms_pg23_struct) - 4;
	/* set addressing */
	if (strstr(path, CTLR_POSTFIX) == NULL) {
		/*
		 * pathname is not of the controller
		 * get address from the pathname
		 */
	    if ((char_ptr = strstr(path, SLSH_DRV_NAME_SD)) == NULL) {
		if ((char_ptr = strstr(path, SLSH_DRV_NAME_SSD)) == NULL) {
		    (void) sprintf(p_error_msg,
			MSGSTR(113, " Error: Invalid pathname"));
		    p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
		    (void) close(fd);
		    return (P_INVALID_PATH);
		}
	    }
		/*
		 * point to next character past the @ character
		 * port and target
		 */
	    char_ptr = 1 + strstr(char_ptr, "@");
		/*
		 * Make sure we don't get core dump
		 * in case port and target not included in path
		 * (@p,t:x,raw)
		 */
	    if ((int)strlen(char_ptr) < 3) {
		(void) sprintf(p_error_msg,
			MSGSTR(113, " Error: Invalid pathname"));
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
		(void) close(fd);
		return (P_INVALID_PATH);
	    }
	    select_page.pg23.port = (uchar_t)atoi(char_ptr);
	    char_ptr += 2;  /* point to target  */
	    select_page.pg23.tgt = (uchar_t)atoi(char_ptr);
	} else {
	    /* command is addressed to the controller */
	    select_page.pg23.all = 1;
	}
	/*
	 * set bits
	 */
	/*
	 * For Solaris 2.3 OS we don't support Fast Writes
	 *
	 * Only check if release is 5.3
	 * as all others should be able to support fast writes.
	 */
	if (pcfw_flag || fwe_flag) {
		if (uname(&name) == -1) {
			(void) close(fd);
			return (errno);
		}
		if (((name.release[0] == '5') &&
			(name.release[1] == '.') &&
			(name.release[2] == '3'))) {
			(void) sprintf(p_error_msg,
			MSGSTR(7017,
		"Fast Writes are not supported by Solaris 2.3\n"));
			p_error_msg_ptr = (char *)&p_error_msg;
			(void) close(fd);
			return (P_NOT_SUPPORTED);
		}
	}
	select_page.pg23.enable = 1;

	select_page.pg23.pcfw = pcfw_flag & 1;	/* The & 1 is for lint */
	select_page.pg23.fwe = fwe_flag & 1;  /* The & 1 is for lint */


	P_DPRINTF("p_funct: Fast Write: Writing page 23 with address"
	    "\n  group %d "
	    "\n  all   %d "
	    "\n  port  %d "
	    "\n  tgt   %d "
	    "\n  enabl %d "
	    "\n pcfw %d  fwe %d \n",
	    select_page.pg23.group,
	    select_page.pg23.all,
	    select_page.pg23.port,
	    select_page.pg23.tgt,
	    select_page.pg23.enable,
	    select_page.pg23.pcfw, select_page.pg23.fwe);

	/* */
	error_code = g_scsi_mode_select_cmd(fd,
		(uchar_t *)&select_page, sizeof (select_page), save);
	(void) close(fd);
	return (error_code);
}



int
p_get_wb_statistics(char *path, Wb_log_entry **wle_ptr)
{
int	fd;
Ls_pg3c	*pg3c;
int	status;
int	size, chk_size;
Wb_log_entry	*wle;

	P_DPRINTF("p_funct: Get WB Statistics: Path %s\n", path);

	/*
	 * Use calling functions structure if one is passed.
	 */
	if (*wle_ptr) {
		wle = *wle_ptr;
	} else {
		wle = *wle_ptr = &wle_st;
	}

	/* initialize tables */
	(void) memset(wle, 0, sizeof (struct wb_log_entry));

	P_DPRINTF("Opening: %s\n", path);
	/* open controller */
	if ((fd = g_object_open(path, O_NDELAY | O_RDONLY)) == -1)
		return (errno);
	P_DPRINTF("%s Opened\n", path);

	/*
	 * Read the first part of the page to get the page size
	 */
	size = (sizeof (struct ls_pg3c_struct) - sizeof (struct wb_log_entry));
	P_DPRINTF("Getting size of page 3c...\n");
	if ((pg3c = (struct ls_pg3c_struct *)g_zalloc(size)) == NULL) {
	    (void) close(fd);
	    return (errno);
	}
	/* read log data */
	if (status = g_scsi_log_sense_cmd(fd,
	    (uchar_t *)pg3c, size, 0x3c)) {
	    (void) close(fd);
	    g_destroy_data((char *)pg3c);
	    return (status);
	}
	P_DPRINTF("Size of page 3c = 0x%x\n", pg3c->pg_len);
	/* check */
	if ((pg3c->pg_code != 0x3c) ||
		(pg3c->pg_len == 0) ||
		(pg3c->ports > P_NPORTS) ||
		(pg3c->tgts > P_NTARGETS)) {
	    (void) sprintf(&p_error_msg[0],
		MSGSTR(7018, "Error reading Log page 0x3c from controller\n"));
	    p_error_msg_ptr = (char *)&p_error_msg; /* set error pointer */
	    g_destroy_data((char *)pg3c);
	    return (LOG_PG3c);
	    /* XXX */
	}
	size = pg3c->pg_len + 4;
	g_destroy_data((char *)pg3c);

	/*
	 * Now get the whole page
	 */
	P_DPRINTF("Getting Whole page...\n");
	if ((pg3c = (struct ls_pg3c_struct *)g_zalloc(size)) == NULL) {
	    (void) close(fd);
	    return (errno);
	}
	/* read log data */
	if (status = g_scsi_log_sense_cmd(fd,
	    (uchar_t *)pg3c, size, 0x3c)) {
	    (void) close(fd);
	    g_destroy_data((char *)pg3c);
	    return (status);
	}
	P_DPRINTF("p_get_wb_statistics: Read page 3c size=0x%x"
		" len=0x%x ports=%d tgts=%d\n", size, pg3c->pg_len,
		pg3c->ports, pg3c->tgts);

	/* check */
	chk_size =
		(sizeof (struct ls_pg3c_struct) -
			sizeof (struct wb_log_entry)) +
		(sizeof (struct wb_log_entry) -
			(P_NPORTS * P_NTARGETS *
				sizeof (struct wb_log_drive_entry))) +
		((pg3c->ports * pg3c->tgts) *
			sizeof (struct wb_log_drive_entry));

	if ((pg3c->pg_code != 0x3c) ||
		(pg3c->pg_len != chk_size - 4) ||
		(pg3c->ports > P_NPORTS) ||
		(pg3c->tgts > P_NTARGETS)) {
	    (void) sprintf(&p_error_msg[0],
		MSGSTR(7019, "Error reading Log page 0x3c from controller\n"));
	    p_error_msg_ptr = (char *)&p_error_msg; /* set error pointer */
	    g_destroy_data((char *)pg3c);
	    return (LOG_PG3c);
	}
	bcopy((char *)&(pg3c->wb_log), (char *)wle,
		(int)((sizeof (struct wb_log_entry) -
			(P_NPORTS * P_NTARGETS *
				sizeof (struct wb_log_drive_entry))) +
		((pg3c->ports * pg3c->tgts) *
			sizeof (struct wb_log_drive_entry))));

	g_destroy_data((char *)pg3c);
	(void) close(fd);

	return (0);
}

int
p_get_perf_statistics(char *path, P_perf_statistics **p_perf_ptr_ptr)
{
int	fd;
Ls_pg3d	*pg3d;
int	status;
int	port, target;
int	total = 0;
int	grand_total = 0;
int	period = 0;
Perf_drv_parms	*pg3d_parmsp;
int	size, chk_size;

	P_DPRINTF("p_funct: Get Performance Statistics: Path %s\n", path);

	/*
	 * Use calling functions structure if one is passed.
	 */
	if (*p_perf_ptr_ptr) {
		perf_ptr = *p_perf_ptr_ptr;
	} else {
		perf_ptr = *p_perf_ptr_ptr = &perf_st;
	}

	/* initialize tables */
	(void) memset(perf_ptr, 0, sizeof (struct p_perf_statistics_struct));

	/* open controller */
	if ((fd = g_object_open(path, O_NDELAY | O_RDONLY)) == -1)
		return (errno);

	/*
	 * Read the first part of the page to get the page size
	 */
	size = 0x20;
	if ((pg3d = (struct ls_pg3d_struct *)g_zalloc(size)) == NULL) {
	    (void) close(fd);
	    return (errno);
	}
	/* read log data */
	if (status = g_scsi_log_sense_cmd(fd,
	    (uchar_t *)pg3d, size, 0x3d)) {
	    (void) close(fd);
	    g_destroy_data((char *)pg3d);
	    return (status);
	}
	/* check */
	if ((pg3d->pg_code != 0x3d) ||
		(pg3d->pg_len == 0) ||
		(pg3d->ports > P_NPORTS) ||
		(pg3d->tgts > P_NTARGETS)) {
	    (void) sprintf(&p_error_msg[0],
		MSGSTR(7020, "Error reading Log page 0x3d from controller\n"));
	    p_error_msg_ptr = (char *)&p_error_msg; /* set error pointer */
	    g_destroy_data((char *)pg3d);
	    return (LOG_PG3d);
	}
	size = pg3d->pg_len + 4;
	g_destroy_data((char *)pg3d);

	/*
	 * Now get the whole page
	 */
	if ((pg3d = (struct ls_pg3d_struct *)g_zalloc(size)) == NULL) {
	    (void) close(fd);
	    return (errno);
	}
	/* read log data */
	if (status = g_scsi_log_sense_cmd(fd,
	    (uchar_t *)pg3d, size, 0x3d)) {
	    (void) close(fd);
	    g_destroy_data((char *)pg3d);
	    return (status);
	}
	P_DPRINTF("p_get_perf_statistics: Read page 3d size=0x%x"
		" len=0x%x ports=%d tgts=%d\n", size, pg3d->pg_len,
		pg3d->ports, pg3d->tgts);

	/* check */
	chk_size = 0x20 +
		(sizeof (struct perf_drv_parms_struct) *
		(pg3d->ports * pg3d->tgts));

	if ((pg3d->pg_code != 0x3d) ||
		(pg3d->pg_len != chk_size - 4) ||
		(pg3d->ports > P_NPORTS) ||
		(pg3d->tgts > P_NTARGETS)) {
	    (void) sprintf(&p_error_msg[0],
		MSGSTR(7021, "Error reading Log page 0x3d from controller\n"));
	    p_error_msg_ptr = (char *)&p_error_msg; /* set error pointer */
	    g_destroy_data((char *)pg3d);
	    return (LOG_PG3d);
	}

	/* update parameters in the performance table */

	/* get the period in seconds */
	period = (int)pg3d->period/10;
	/*
	 * This is in case two or more programs are running
	 * and we get a low number.
	 */
	if (period == 0)
		period = 1;
	perf_ptr->ctlr_percent_busy = 100 - (int)pg3d->idle;
	pg3d_parmsp = (struct perf_drv_parms_struct *)pg3d->drv_parms;

	for (port = 0; port < (int)pg3d->ports; port++) {
	    for (target = 0; target < (int)pg3d->tgts;
		target++, pg3d_parmsp++) {
		total = 0;
		total +=
		    perf_ptr->perf_details[port][target].num_lt_2k_reads =
		    pg3d_parmsp->num_lt_2k_reads /
		    period;
		total +=
		    perf_ptr->perf_details[port][target].num_lt_2k_writes =
		    pg3d_parmsp->num_lt_2k_writes /
		    period;
		total +=
		perf_ptr->perf_details[port][target].num_gt_2k_lt_8k_reads =
		    pg3d_parmsp->num_gt_2k_lt_8k_reads /
		    period;
		total +=
		perf_ptr->perf_details[port][target].num_gt_2k_lt_8k_writes =
		    pg3d_parmsp->num_gt_2k_lt_8k_writes /
		    period;
		total += perf_ptr->perf_details[port][target].num_8k_reads =
		    pg3d_parmsp->num_8k_reads /
		    period;
		total += perf_ptr->perf_details[port][target].num_8k_writes =
		    pg3d_parmsp->num_8k_writes /
		    period;
		total += perf_ptr->perf_details[port][target].num_gt_8k_reads =
		    pg3d_parmsp->num_gt_8k_reads /
		    period;
	    total += perf_ptr->perf_details[port][target].num_gt_8k_writes =
		    pg3d_parmsp->num_gt_8k_writes /
		    period;
		perf_ptr->drive_iops[port][target] = total;
		grand_total += total;
	    }
	}
	perf_ptr->ctlr_iops = grand_total;

	g_destroy_data((char *)pg3d);
	(void) close(fd);

	return (0);
}




/*
 *	General routine to get the status of all 30 devices from the
 *	controller and set up data structures
 *
 *	The argument are:
 *	Controller physical path
 *	verbose flag which if one, causes progress messages to be printed.
 *
 */
int
get_status(char *path, int initial_update_flag, int verbose_flag)
{
int	controller_fd, fd;
P_inquiry  inq;	    /* Local Inquiry buffer */
int	i;
/*
 * NOTE: These are int only to make lint happy
 *	    They should really be uchar_t
 *	    Be carefull when doing sizeof in relation to these
 *	    In particular when adding
 *	    "(uchar_t)sizeof (struct mode_header_10_struct)"
 */
uchar_t	*pg21_buf, *pg22_buf;
Ms_pg21	*pg21_ptr;
Ms_pg22	*pg22_ptr;
Sd_rec	*pg22_drvp;
uchar_t	p, t;
Mode_header_10	*mode_header_ptr;
char	local_path[MAXNAMELEN];
char	drv_path[MAXNAMELEN];
char	temp_string[256];
Read_capacity_data	capacity;	/* local read capacity buffer */
int	status;
struct	vtoc	vtoc;

	if (initial_update_flag) {
	    /* initialization */
	    (void) memset(state_ptr, 0, sizeof (struct p_state_struct));
	}

	if (verbose_flag)  {
	    if (initial_update_flag) {
		(void) printf(MSGSTR(7022, "    - Getting state information "
			"from SPARCstorage Array\n        %s\n"),
		path);
	    } else {
		(void) printf(MSGSTR(7023, "    - Updating state information "
			"for SPARCstorage Array\n        %s\n"),
		path);
	    }
	}

	/* Verify pathname is of a controller */
	if ((strstr(path, CTLR_POSTFIX)) == NULL) {
	    (void) sprintf(p_error_msg,
		MSGSTR(7024,
	"Invalid controller physical pathname: %s\n"), path);
	    p_error_msg_ptr = (char *)&p_error_msg;	/* set error ptr */
	    return (P_INVALID_PATH);
	}


	if (verbose_flag)
		(void) printf(MSGSTR(7025,
		"        - Verifying Controller is healthy\n"));

	if ((controller_fd = g_object_open(path, O_NDELAY | O_RDONLY)) == -1) {
	    return (errno);
	}
	if (initial_update_flag) {
	    /* Initially get the controller status  */
	    if (status = g_scsi_tur(controller_fd)) {
		(void) close(controller_fd);
		return (status);
	    }
	    /* Verify controller inquiry info */
	    /* get inquiry from the controller */
	    if (status = g_scsi_inquiry_cmd(controller_fd,
		(uchar_t *)&inq, sizeof (inq))) {
		(void) close(controller_fd);
		return (status);
	    }


	    if (verbose_flag) {
		(void) printf(MSGSTR(7026, "\t\tController is a:"));
		for (i = 0; i < sizeof (inq.inq_vid); i++)
			(void) printf("%c", inq.inq_vid[i]);
		(void) printf("  ");
		for (i = 0; i < sizeof (inq.inq_pid); i++)
			(void) printf("%c", inq.inq_pid[i]);
		(void) printf("\n");
	    }
	    if (strncmp(inq.inq_vid, "SUN     ", sizeof (inq.inq_vid)) != 0) {
		/*
		 * Note: Use to check for SSA or SDA but
		 * not exactly sure what the name is going to be.
		 */
		(void) sprintf(&p_error_msg[0],
		    MSGSTR(7027, "Controller was NOT an SPARCstorage Array\n"));
		p_error_msg_ptr = (char *)&p_error_msg;	/* set error pointer */
		(void) close(controller_fd);
		return (P_NOT_SDA);
	    }
	    /*	save the inquiry info in controller state table */
	    (void) strncpy((char *)&state_ptr->c_tbl.c_id.vendor_id[0],
		(char *)&inq.inq_vid[0], sizeof (inq.inq_vid));
	    (void) strncpy((char *)&state_ptr->c_tbl.c_id.prod_id[0],
		(char *)&inq.inq_pid[0], sizeof (inq.inq_pid));
	    (void) strncpy((char *)&state_ptr->c_tbl.c_id.revision[0],
		(char *)&inq.inq_revision[0], sizeof (inq.inq_revision));
	    (void) strncpy((char *)&state_ptr->c_tbl.c_id.firmware_rev[0],
		(char *)&inq.inq_firmware_rev[0],
		sizeof (inq.inq_firmware_rev));
	    (void) strncpy((char *)&state_ptr->c_tbl.c_id.ser_num[0],
		(char *)&inq.inq_serial[0],  sizeof (inq.inq_serial));

	}

	/*
	*	Get MODE SENSE information
	*
	*	Use temporary buffers
	*
	*/

	/*
	*	PAGE 21
	*/
	if ((pg21_buf = g_zalloc(MAX_MODE_SENSE_LEN + 1)) == NULL) {
	    (void) close(controller_fd);
	    return (errno);
	}
	if (status = g_scsi_mode_sense_cmd(controller_fd, pg21_buf,
	    MAX_MODE_SENSE_LEN, 0, 0x21)) {
	    (void) close(controller_fd);
	    return (status);
	}

	/* get info & put in my state structure  */
	mode_header_ptr = (struct mode_header_10_struct *)(int)pg21_buf;
	/* <da> ANSI "C" */
	pg21_ptr = ((struct ms_pg21_struct *)((int)pg21_buf +
	    (uchar_t)sizeof (struct mode_header_10_struct) +
	    (mode_header_ptr->bdesc_length)));
	/* check parameters for sanity */
	if ((pg21_ptr->pg_code != 0x21) ||
	    (pg21_ptr->pg_len == 0)) {
	    (void) sprintf(&p_error_msg[0],
		MSGSTR(7028, "Error reading page 21 from controller\n"));
	    p_error_msg_ptr = (char *)&p_error_msg;	/* set error pointer */
	    (void) close(controller_fd);
	    return (RD_PG21);
	}
	/* update state structure */
	state_ptr->c_tbl.aps = pg21_ptr->aps;	/* set up bit by bit */
	state_ptr->c_tbl.aes = pg21_ptr->aes;	/* set up bit by bit */
	state_ptr->c_tbl.hvac_fc = pg21_ptr->hvac_fc;	/* set up bit by bit */
	state_ptr->c_tbl.hvac_lobt = pg21_ptr->hvac_lobt;
	/* */
	g_destroy_data((char *)pg21_buf);	/* free buffer */


	/*
	*	PAGE 22
	*/
	if (verbose_flag)
	    (void) printf(MSGSTR(7029,
		"        - Reading state of each drive\n"));
	if ((pg22_buf = g_zalloc(MAX_MODE_SENSE_LEN + 1)) == NULL) {
	    (void) close(controller_fd);
	    return (errno);
	}
	if (status = g_scsi_mode_sense_cmd(controller_fd, pg22_buf,
	    MAX_MODE_SENSE_LEN, 0, 0x22)) {
	    (void) close(controller_fd);
	    return (status);
	}
	/* get info & put in my state structure  */
	mode_header_ptr = (struct mode_header_10_struct *)(int)pg22_buf;
	/* <da> ANSI "C" */
	pg22_ptr = ((struct ms_pg22_struct *)((int)pg22_buf +
	    (uchar_t)sizeof (struct mode_header_10_struct) +
	    (mode_header_ptr->bdesc_length)));
	/* check parameters for sanity */
	if ((pg22_ptr->pg_code != 0x22) ||
		(pg22_ptr->pg_len == 0) ||
		(pg22_ptr->num_ports > P_NPORTS) ||
		(pg22_ptr->num_tgts > P_NTARGETS)) {
	    (void) sprintf(&p_error_msg[0],
		MSGSTR(7030, "Error reading page 22 from controller\n"));
	    p_error_msg_ptr = (char *)&p_error_msg;
	    (void) close(controller_fd);
	    return (RD_PG22);
	}
	/*
	 * Set up global state
	 *
	 * If state of any drive changed then get
	 * new info from drive by setting initial_update_flag
	 */
	state_ptr->c_tbl.num_ports = pg22_ptr->num_ports;
	/* Set up global */
	state_ptr->c_tbl.num_tgts = pg22_ptr->num_tgts;
	/* NOTE :: Page 22 is a variable length page */
	pg22_drvp = ((struct sd_rec_struct *)&pg22_ptr->drv[0][0]);
	for (p = 0; p < state_ptr->c_tbl.num_ports; p++) {
	    for (t = 0; t < state_ptr->c_tbl.num_tgts; t++) {
		if (state_ptr->drv[p][t].state_flags !=
			pg22_drvp->state_flags) {
			I_DPRINTF("\t- State changed - "
			    "cnt=0x%x new=0x%x  port=%d, tgt=%d\n",
			    state_ptr->drv[p][t].state_flags,
			    pg22_drvp->state_flags,
			    p, t);
			initial_update_flag++;
			state_ptr->drv[p][t].state_flags =
			    pg22_drvp->state_flags;
		}
		pg22_drvp++;
	    }
	}
	g_destroy_data((char *)pg22_buf);	/* free big buffer */


	/*
	 * if initial update
	 * 	get Inquiry and size information for each drive
	 *	that is not having a problem
	 *
	 * 	If cmd fails then mark this drive
	 *	bad in state table - Don't fail
	 *
	 *	If open fails then drive may not be labeled.
	 *	Try open with O_NDELAY flag and mark drive
	 *	as not labeled.
	 *
	 */
	if (initial_update_flag) {
	    if (verbose_flag)
		(void) printf(MSGSTR(7031, "        - Reading vendor's "
		"information and capacity from each drive\n"));
		I_DPRINTF("\t-  Reading vendor's info"
		    " for individual drives # ports=%d, #tgts=%d\n",
		    p, t);
		/*
		 * Create pathname to individual drives
		 * from controller path
		 */

		if (status = p_get_drv_name(path, drv_path)) {
			return (status);
		}

		for (p = 0; p < state_ptr->c_tbl.num_ports; p++) {
		for (t = 0; t < state_ptr->c_tbl.num_tgts; t++) {
		    if (!(state_ptr->drv[p][t].state_flags &
			(DS_NDF | DS_NDS | DS_DNR | DS_SPUN_DWN))) {

			/*
			 * add port and target to drive path
			 */
			(void) strcpy(local_path, drv_path);
			strcpy(temp_string, "p,t:c,raw");
			temp_string[0] = p + '0';
			temp_string[2] = t + '0';
			(void) strcat(local_path, temp_string);

			/*
			 * Try to open drive.
			 */
			if ((fd = g_object_open(local_path, O_RDONLY)) == -1) {
			    if ((fd = g_object_open(local_path,
				O_RDONLY | O_NDELAY)) == -1) {
				I_DPRINTF("\t- Error opening drive %s\n",
					local_path);
				state_ptr->drv[p][t].state_flags |= DS_OPNF;
			    } else {
				/*
				 * There may not be a label on the drive - check
				 */
				if (ioctl(fd, DKIOCGVTOC, &vtoc) == -1) {
				    I_DPRINTF("\t- DKIOCGVTOC ioctl failed: "
				    " invalid geometry\n");
				    state_ptr->drv[p][t].no_label_flag++;
				} else {
					/*
					 * Sanity-check the vtoc
					 */
				    I_DPRINTF("\t- Checking vtoc\n");
				    if (vtoc.v_sanity != VTOC_SANE ||
					vtoc.v_sectorsz != DEV_BSIZE) {
					state_ptr->drv[p][t].no_label_flag++;
				    }
				}
			    }
			}
			/* */
			if (state_ptr->drv[p][t].state_flags != DS_OPNF) {
			    I_DPRINTF("\t- Getting INQUIRY info and "
				    "capacity for individual drive %d,%d\n",
				    p, t);
			    if (status = g_scsi_inquiry_cmd(fd,
				(uchar_t *)&inq, sizeof (inq))) {
				if (status & P_SCSI_ERROR) {
				    /* mark bad */
				    state_ptr->drv[p][t].state_flags |= DS_INQF;
				} else {
				    (void) close(fd);
				    (void) close(controller_fd);
				    return (status);
				}
			    } else if (status =
				g_scsi_read_capacity_cmd(fd,
				(uchar_t *)&capacity,
				sizeof (capacity))) {
				if (status & P_SCSI_ERROR) {
				    if ((status & ~P_SCSI_ERROR) ==
					STATUS_RESERVATION_CONFLICT) {
					/* mark reserved */
					state_ptr->drv[p][t].reserved_flag++;
				    } else
					/* mark bad */
					state_ptr->drv[p][t].state_flags |=
						DS_CNR;
				} else {
				    (void) close(fd);
				    (void) close(controller_fd);
				    return (status);
				}
			    }
			    (void) close(fd);
#ifdef	DONTUSE
			    if (close(fd)) {
				(void) close(controller_fd);
				(void) sprintf(p_error_msg,
				    MSGSTR(7032, "Close: %s\n"),
					strerror(errno));
				/* set error ptr */
				p_error_msg_ptr = (char *)&p_error_msg;
				return (errno);
			    }
#endif	DONTUSE
			/*
			 * save the inquiry info in controller state table
			 */
			    (void)
			    strncpy(&state_ptr->drv[p][t].id.vendor_id[0],
				    &inq.inq_vid[0], sizeof (inq.inq_vid));
			    (void)
			    strncpy((char *)&state_ptr->drv[p][t].id.prod_id[0],
				    (char *)&inq.inq_pid[0],
				    sizeof (inq.inq_pid));
			    (void) strncpy((char *)
				&state_ptr->drv[p][t].id.revision[0],
				(char *)&inq.inq_revision[0],
				sizeof (inq.inq_revision));
			    (void) strncpy((char *)
				&state_ptr->drv[p][t].id.firmware_rev[0],
				(char *)&inq.inq_firmware_rev[0],
				sizeof (inq.inq_firmware_rev));
			    (void)
			    strncpy((char *)&state_ptr->drv[p][t].id.ser_num[0],
				    (char *)&inq.inq_serial[0],
				    sizeof (inq.inq_serial));
			    /* save capacity */
			    state_ptr->drv[p][t].num_blocks =
			    capacity.last_block_addr + 1;
			}
		    }
		}
	    }
	}

	/* use parameters read to update labels */
	build_labels();

	(void) close(controller_fd);
	return (0);
}

/*
 * Use my local state structure unless the calling
 * function supplies a structure.
 */
int
p_get_status(char *path, struct p_state_struct **p_state_ptr_ptr,
	int initial_update_flag, int verbose_flag)
{
int error_code;
	P_DPRINTF("p_funct: Get Status: initial flag=%d"
		"\n    path %s\n", initial_update_flag, path);
	if (*p_state_ptr_ptr) {
		state_ptr = *p_state_ptr_ptr;
	} else {
		state_ptr = *p_state_ptr_ptr = &pg_state;
	}
	error_code = get_status(path, initial_update_flag, verbose_flag);
	return (error_code);
}

int
p_purge(char *path)
{
int	fd;
int	error_code;
char	c_path_phys[MAXPATHLEN];
char	*char_ptr;
struct	{
	Mode_header_10 hdr;
	Ms_pg23	pg23;		/* local page 23  */
} select_page;

	P_DPRINTF("p_funct: Purge: Path %s\n", path);

	/* save path passed in local copy */
	(void) strcpy(c_path_phys, path);
	/*  If pathname not already :ctlr then convert */
	if (strstr(c_path_phys, CTLR_POSTFIX) == NULL) {
	    /* point to place to add */
	    char_ptr = strrchr(c_path_phys, '/');
	    *char_ptr = '\0';	/* Terminate sting  */
	    (void) strcat(c_path_phys, CTLR_POSTFIX);
	}

	/* open controller */
	if ((fd = g_object_open(c_path_phys, O_NDELAY | O_RDWR)) == -1)
		return (errno);

	/* set up general parameters for page */
	(void) memset((char *)&select_page, 0, sizeof (select_page));
	select_page.hdr.length = ((int)sizeof (select_page.pg23)
	    + (int)sizeof (select_page.hdr) -2);
	select_page.pg23.pg_code = 0x23;	/* page code */
	select_page.pg23.pg_len = sizeof (struct ms_pg23_struct) - 4;
	/* set addressing */
	if (strstr(path, CTLR_POSTFIX) == NULL) {
		/*
		 * pathname is not of the controller
		 * get address from the pathname
		 */
	    if ((char_ptr = strstr(path, SLSH_DRV_NAME_SD)) == NULL) {
		if ((char_ptr = strstr(path, SLSH_DRV_NAME_SSD)) == NULL) {
		    (void) sprintf(p_error_msg,
			MSGSTR(113, " Error: Invalid pathname"));
		    p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
		    return (P_INVALID_PATH);
		}
	    }
		/*
		 * point to next character past the @ character
		 * port and target
		 */
	    char_ptr = 1 + strstr(char_ptr, "@");
		/*
		 * Make sure we don't get core dump
		 * in case port and target not included in path
		 * (@p,t:x,raw)
		 */
	    if ((int)strlen(char_ptr) < 3) {
		(void) sprintf(p_error_msg,
			MSGSTR(113, " Error: Invalid pathname"));
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
		return (P_INVALID_PATH);
	    }
	    select_page.pg23.port = (uchar_t)atoi(char_ptr);
	    char_ptr += 2;  /* point to target  */
	    select_page.pg23.tgt = (uchar_t)atoi(char_ptr);
	} else {
	    /* command is addressed to the controller */
	    select_page.pg23.all = 1;
	}

	select_page.pg23.purge = 1;	    /* set purge flag */
	/* Note: fast write bits are don't care when purge set */

	/* */
	error_code = g_scsi_mode_select_cmd(fd,
		(uchar_t *)&select_page, sizeof (select_page), 1);
	(void) close(fd);
	return (error_code);
}

int
p_set_perf_statistics(char *path, int aps_flag)
{
int	fd;
int	error_code;
struct	{
	Mode_header_10 hdr;
	Ms_pg21	pg21;		/* local page 21  */
} select_page;
Ms_pg21 *pg21_ptr = NULL;
uchar_t *pg21_read_buf = NULL;
Mode_header_10	*mode_header_ptr;
uchar_t	ps = 0;

	P_DPRINTF("p_funct: Performance statistics gathering: aps_flag=%d"
	    "\n    Path %s\n",
	    aps_flag, path);

	/* open controller */
	if ((fd = g_object_open(path, O_NDELAY | O_RDWR)) == -1)
		return (errno);

	/* get page 21 from controller */
	if ((error_code = get_pg21(fd, &pg21_read_buf)) != 0) {
	    return (error_code);
	}
	mode_header_ptr = (struct mode_header_10_struct *)(int)pg21_read_buf;
	pg21_ptr = ((struct ms_pg21_struct *)((int)pg21_read_buf +
	    (uchar_t)sizeof (struct mode_header_10_struct) +
	    (mode_header_ptr->bdesc_length)));

	/* set up general parameters for page */
	(void) memset((char *)&select_page, 0, sizeof (select_page));
	select_page.hdr.length = ((int)sizeof (select_page.pg21)
	    + (int)sizeof (select_page.hdr) - 2);
	select_page.pg21.pg_code = 0x21;	/* page code */
	select_page.pg21.pg_len = sizeof (struct ms_pg21_struct) - 4;
	/* update information that will not change from state table */
	select_page.pg21.aes = pg21_ptr->aes;
	/* */
	select_page.pg21.aps = aps_flag & 1;  /* The & 1 is for lint */
	if ((error_code = g_scsi_mode_select_cmd(fd,
	    (uchar_t *)&select_page, sizeof (select_page), ps)) != 0) {
		return (error_code);
	}
	(void) close(fd);
	g_destroy_data((char *)pg21_read_buf);
	return (0);
}

/*
 *	STRIPE drives
 *
 *	    path is a pointer to the physical pathname of the Pluto controller
 *
 *	    The drive list is a list of the ports and targets for the
 *	    individual drives that are to be striped
 *	    The list is terminated by a entry with the port = 0xff
 *
 */
/*ARGSUSED*/
int
p_stripe(char *path,
	int stripe_size,
	Drv_list_entry drv_list[],
	int verbose_flag)
{

	P_DPRINTF("p_funct: Stripe: stripe_size %d\n Path %s\n",
		stripe_size, path);

	(void) sprintf(p_error_msg,
	    MSGSTR(7033, "This function is not currently supported!\n"));
	p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
	return (P_NOT_SDA);
}

int
p_sync_cache(char *path)
{
int	status;
int	fd;

	P_DPRINTF("p_funct: Sync-Cache: Path %s\n", path);
	if ((fd = g_object_open(path, (O_RDONLY | O_NDELAY))) == -1)
		return (errno);
	status = scsi_sync_cache_cmd(fd);
	(void) close(fd);
	return (status);
}


/*
 *	UN-STRIPE drives
 */
/*ARGSUSED*/
int
p_unstripe(char *path, uchar_t port, uchar_t target, int verbose_flag)
{

	VERBPRINT(MSGSTR(7034,
	"Unstriping grouped drive at group address: %d,%d\n"),
	    port, target);

	P_DPRINTF("p_funct: Unstriping grouped drive at group address: %d,%d\n",
	    port, target);

	(void) sprintf(p_error_msg,
	    MSGSTR(7035, "This function is not currently supported!\n"));
	p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
	return (P_NOT_SDA);
}


/*
 * Migrated from SSA genf.c as it didn't make it into libg_fc/common/genf.c
 * yet we need it here and in ssa.c
 */


/*
 * Create pathname of the form:
 * /devices/iommu@x/sbus@x/SUNW,soc@2,0/SUNW,pln@a0000125,52000023/s[s]d@
 * from the :ctlr path
 *
 * dev_path_ptr must point to a string large enough to hold the
 * complete path.
 *
 * RETURN: 0 = O.K.
 *	   non-zero = error code
 *
 */
int
p_get_drv_name(char *ctlr_path, char *drv_path_ptr)
{
char	drv_path[MAXNAMELEN];
char	*char_ptr;
DIR	*dirp;
struct dirent	*entp;

	/*
	 * Create pathname to individual drives
	 */
	/* get copy of controller path */
	(void) strcpy(drv_path, ctlr_path);
	/* point to the place to add */
	if ((char_ptr = strstr(drv_path, CTLR_POSTFIX)) == NULL) {
	    (void) sprintf(p_error_msg,
	    MSGSTR(7500, "Invalid controller physical pathname: %s\n"),
		ctlr_path);
	    /* set error ptr */
	    p_error_msg_ptr = (char *)&p_error_msg;
	    return (P_INVALID_PATH);
	}
	*char_ptr = '\0';	/* Terminate sting  */
	if ((dirp = opendir(drv_path)) == NULL) {
	    (void) sprintf(p_error_msg,
	    MSGSTR(7501, "Error opening directory: %s\n"),
		ctlr_path);
	    /* set error ptr */
	    p_error_msg_ptr = (char *)&p_error_msg;
	    return (P_INVALID_PATH);
	}
	while ((entp = readdir(dirp)) != NULL) {
	    if (strcmp(entp->d_name, ".") == 0 ||
	    strcmp(entp->d_name, "..") == 0)
	    continue;

	    if ((match_substr(entp->d_name, DRV_NAME_SD) != 0) &&
		(match_substr(entp->d_name, DRV_NAME_SSD) != 0)) {
		(void) sprintf(p_error_msg,
		MSGSTR(7502, "Invalid device name: %s\n"),
		    entp->d_name);
		/* set error ptr */
		p_error_msg_ptr = (char *)&p_error_msg;
		closedir(dirp);
		return (P_INVALID_PATH);
	    } else {
		break;
	    }
	}
	if (strncmp(entp->d_name, DRV_NAME_SD,
	    sizeof (DRV_NAME_SD) - 1) == 0) {
	    (void) strcat(drv_path, SLSH_DRV_NAME_SD);
	} else {
	    (void) strcat(drv_path, SLSH_DRV_NAME_SSD);
	}
	I_DPRINTF("Drive Path %s\n", drv_path);
	strcpy(drv_path_ptr, drv_path);
	closedir(dirp);
	return (0);
}
/*
 * Return true if we find an occurance of s2 at the
 * beginning of s1.  We don't have to match all of
 * s1, but we do have to match all of s2
 */
static int
match_substr(char *s1, char *s2)
{
	while (*s2 != 0) {
		if (*s1++ != *s2++)
		return (0);
	}

	return (1);
}

/*
 * Extracted from the old SSA io.c
 */
#define	MIN(a, b) (a < b ? a : b)
/*
 *		Read buffer command set up to upload firmware
 *	Reads from PLUTO code image (in 3 readable proms) starting at offset
 * "code_off" for "buf_len" bytes.
 */
static int
scsi_upload_code_cmd(int fd, uchar_t *buf_ptr, int buf_len, int code_off)
{
	int	sz, status;

	while (buf_len) {
		sz = MIN(32*1024, buf_len);

		status = g_scsi_readbuffer_cmd(fd, buf_ptr, sz, code_off);
		if (status)
			return (status);
		buf_len -= sz;
		buf_ptr += sz;
		code_off += sz;
	}
	return (status);
}
/*
 *		Write buffer command set up to download firmware
 */
static int
scsi_download_code_cmd(int fd, uchar_t *buf_ptr, int buf_len, uchar_t sp)
{
	int	status, sz;
	int bid;

/*
 * The old scsi_writebuffer_cmd did not take an offset as the
 * second argument and blindly used 0, so now we pass 0 as arg
 * two.
 */
	if (status = g_scsi_writebuffer_cmd(fd, 0, buf_ptr, 0, sp, 0xff))
		return (status);

	bid = 0;
	while (buf_len) {
		sz = MIN(32*1024, buf_len);
		status = g_scsi_writebuffer_cmd(fd, 0, buf_ptr, sz, sp, bid);
		if (status)
			return (status);
		buf_len -= sz;
		buf_ptr += sz;
		bid ++;
	}

	return (g_scsi_writebuffer_cmd(fd, 0, NULL, 0, sp, 0xfe));
}

/*
 * XXX -- This is legacy code from SSA that was not included in
 * libg_fc as it only pertains to Sparc Storage Arrays.
 */
static int
scsi_sync_cache_cmd(int fd)
{
struct uscsi_cmd	ucmd;
const my_cdb_g1	cdb = {SCMD_SYNC_CACHE, 0, 0, 0, 0, 0, 0, 0, 0, 0};
struct	scsi_extended_sense	sense;

	(void) memset((char *)&ucmd, 0, sizeof (ucmd));

	ucmd.uscsi_cdb = (caddr_t)&cdb;
	ucmd.uscsi_cdblen = CDB_GROUP1;
	ucmd.uscsi_bufaddr = NULL;
	ucmd.uscsi_buflen = 0;
	ucmd.uscsi_rqbuf = (caddr_t)&sense;
	ucmd.uscsi_rqlen = sizeof (struct  scsi_extended_sense);
	ucmd.uscsi_timeout = 90;
	return (uscsi_cmd(fd, &ucmd, 0));
}

/*
 * XXX -- This is legacy code from SSA required by scsi_sync_cache_cmd
 * There is a cmd() in libg_fc that is the virtually the same as
 * this procedure, but it is not exported and so to keep scsi_sync_cache_cmd
 * happy we must include this.
 *
 * Execute a command and determine the result.
 * Uses the "uscsi" ioctl interface
 */
static int
uscsi_cmd(int file, struct uscsi_cmd *command, int flag)
{
int	status, i;

	/*
	 * Set function flags for driver.
	 *
	 * Set don't retry flags
	 * Set Automatic request sense enable
	 *
	 */
	command->uscsi_flags = USCSI_ISOLATE | USCSI_DIAGNOSE |
		USCSI_RQENABLE;
	command->uscsi_flags |= flag;

	/* print command for debug */
	if (getenv("SSA_S_DEBUG") != NULL) {
		(void) printf("Issuing the following SCSI command: %s\n",
			scsi_find_command_name(command->uscsi_cdb[0]));
		(void) printf("	fd=0x%x cdb=", file);
		for (i = 0; i < (int)command->uscsi_cdblen; i++) {
			(void) printf("%x ", *(command->uscsi_cdb + i));
		}
		(void) printf("\n\tlen=0x%x bufaddr=0x%x buflen=0x%x"
			" flags=0x%x\n",
		command->uscsi_cdblen,
		(int)command->uscsi_bufaddr,
		command->uscsi_buflen, command->uscsi_flags);

		if ((command->uscsi_buflen < 0x40) &&
			(command->uscsi_buflen > 0) &&
			((flag & USCSI_READ) == 0)) {
			(void) g_dump("Buffer data: ",
			(uchar_t *)command->uscsi_bufaddr,
			command->uscsi_buflen, HEX_ONLY);
		}
	}


	/*
	 * Default command timeout in case command left it 0
	 */
	if (command->uscsi_timeout == 0) {
		command->uscsi_timeout = 60;
	}
	/*	Issue command - finally */
	status = ioctl(file, USCSICMD, command);
	if (status == 0 && command->uscsi_status == 0) {
		if (getenv("SSA_S_DEBUG") != NULL) {
			if ((command->uscsi_buflen < 0x40) &&
				(command->uscsi_buflen > 0) &&
				((flag & USCSI_READ))) {
				(void) g_dump("Data read: ",
				(uchar_t *)command->uscsi_bufaddr,
				command->uscsi_buflen, HEX_ONLY);
			}
		}
		return (status);
	}
	if ((status != 0) && (command->uscsi_status == 0)) {
		if (getenv("SSA_S_DEBUG") != NULL) {
			(void) printf("Unexpected USCSICMD ioctl error: %s\n",
				strerror(errno));
		}
		return (status);
	}

	/*
	 * Just a SCSI error, create error message
	 */
	if ((command->uscsi_rqbuf != NULL) &&
	    (((char)command->uscsi_rqlen - (char)command->uscsi_rqresid) > 0)) {
		scsi_printerr(command,
			(struct scsi_extended_sense *)command->uscsi_rqbuf,
			(int)(command->uscsi_rqlen - command->uscsi_rqresid),
			p_error_msg);
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
	} else {
		/* Print sense byte information */

		sprintf(p_error_msg, MSGSTR(20500,
			"SCSI Error - Sense Byte: %s\n"),
			p_decode_sense((uchar_t)command->uscsi_status));
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
	}
	return (P_SCSI_ERROR | command->uscsi_status);
}

/*
 * XXX -- More dependancies for uscsi_cmd that are present in libg_fc,
 * but not exported...
 */
/*
 *	Function to create error message containing
 *	scsi request sense information
 */
static void
scsi_printerr(struct uscsi_cmd *ucmd, struct scsi_extended_sense *rq, int rqlen,
		char msg_string[])
{
	int		blkno;

	switch (rq->es_key) {
	case KEY_NO_SENSE:
		(void) sprintf(msg_string, MSGSTR(91, "No sense error"));
		break;
	case KEY_RECOVERABLE_ERROR:
		(void) sprintf(msg_string, MSGSTR(76, "Recoverable error"));
		break;
	case KEY_NOT_READY:
		(void) sprintf(msg_string, MSGSTR(7514, "Not ready error"));
		break;
	case KEY_MEDIUM_ERROR:
		(void) sprintf(msg_string, MSGSTR(99, "Medium error"));
		break;
	case KEY_HARDWARE_ERROR:
		(void) sprintf(msg_string, MSGSTR(106, "Hardware error"));
		break;
	case KEY_ILLEGAL_REQUEST:
		(void) sprintf(msg_string, MSGSTR(103, "Illegal request"));
		break;
	case KEY_UNIT_ATTENTION:
		(void) sprintf(msg_string, MSGSTR(7515,
			"Unit attention error"));
		break;
	case KEY_WRITE_PROTECT:
		(void) sprintf(msg_string, MSGSTR(52, "Write protect error"));
		break;
	case KEY_BLANK_CHECK:
		(void) sprintf(msg_string, MSGSTR(131, "Blank check error"));
		break;
	case KEY_VENDOR_UNIQUE:
		(void) sprintf(msg_string, MSGSTR(58, "Vendor unique error"));
		break;
	case KEY_COPY_ABORTED:
		(void) sprintf(msg_string, MSGSTR(123, "Copy aborted error"));
		break;
	case KEY_ABORTED_COMMAND:
		(void) sprintf(msg_string, MSGSTR(7516, "Aborted command"));
		break;
	case KEY_EQUAL:
		(void) sprintf(msg_string, MSGSTR(117, "Equal error"));
		break;
	case KEY_VOLUME_OVERFLOW:
		(void) sprintf(msg_string, MSGSTR(57, "Volume overflow"));
		break;
	case KEY_MISCOMPARE:
		(void) sprintf(msg_string, MSGSTR(98, "Miscompare error"));
		break;
	case KEY_RESERVED:
		(void) sprintf(msg_string, MSGSTR(7517, "Reserved error"));
		break;
	default:
		(void) sprintf(msg_string, MSGSTR(59, "Unknown error"));
		break;
	}

	(void) sprintf(&msg_string[strlen(msg_string)],
		MSGSTR(7518, " during %s"),
			scsi_find_command_name(ucmd->uscsi_cdb[0]));

	if (rq->es_valid) {
		blkno = (rq->es_info_1 << 24) | (rq->es_info_2 << 16) |
			(rq->es_info_3 << 8) | rq->es_info_4;
		(void) sprintf(&msg_string[strlen(msg_string)],
			MSGSTR(49, ": block %d (0x%x)"), blkno, blkno);
	}

	(void) sprintf(&msg_string[strlen(msg_string)], "\n");

	if (rq->es_add_len >= 6) {
		(void) sprintf(&msg_string[strlen(msg_string)],
		MSGSTR(132, "  Additional sense: 0x%x   ASC Qualifier: 0x%x\n"),
			rq->es_add_code, rq->es_qual_code);
			/*
			 * rq->es_add_info[ADD_SENSE_CODE],
			 * rq->es_add_info[ADD_SENSE_QUAL_CODE]);
			 */
	}
	if (rq->es_key == KEY_ILLEGAL_REQUEST) {
		string_dump(MSGSTR(47, " cmd:   "), (uchar_t *)ucmd,
			sizeof (struct uscsi_cmd), HEX_ONLY, msg_string);
		string_dump(MSGSTR(48, " cdb:   "), (uchar_t *)ucmd->uscsi_cdb,
			ucmd->uscsi_cdblen, HEX_ONLY, msg_string);
	}
	string_dump(MSGSTR(43, " sense:  "),
		(uchar_t *)rq, 8 + rq->es_add_len, HEX_ONLY,
		msg_string);
	rqlen = rqlen;	/* not used */
}

/*
 * XXX -- More dependancies for uscsi_cmd that are present in libg_fc,
 * but not exported...
 */
static char *
p_decode_sense(
	uchar_t	status)
{
	switch (status & STATUS_MASK) {
	case STATUS_GOOD:
		return (MSGSTR(7503, "good status"));

	case STATUS_CHECK:
		return (MSGSTR(128, "Check condition"));

	case STATUS_MET:
		return (MSGSTR(124, "Condition met"));

	case STATUS_BUSY:
		return (MSGSTR(37, "Busy"));

	case STATUS_INTERMEDIATE:
		return (MSGSTR(7504, "intermediate"));

	case STATUS_INTERMEDIATE_MET:
		return (MSGSTR(7505, "intermediate - condition met"));

	case STATUS_RESERVATION_CONFLICT:
		return (MSGSTR(7506, "reservation_conflict"));

	case STATUS_TERMINATED:
		return (MSGSTR(126, "Command terminated"));

	case STATUS_QFULL:
		return (MSGSTR(83, "Queue full"));

	default:
		return (MSGSTR(4, "Unknown status"));
	}
}

/*
 * XXX -- More dependancies for uscsi_cmd that are present in libg_fc,
 * but not exported...
 */
/*
 * Return a pointer to a string telling us the name of the command.
 */
static char *
scsi_find_command_name(int cmd)
{
#define	SCMD_UNKNOWN	0xff
/*
 * Names of commands.  Must have SCMD_UNKNOWN at end of list.
 */
struct scsi_command_name {
	int command;
	char	*name;
} scsi_command_names[26];
register struct scsi_command_name *c;

	scsi_command_names[0].command = SCMD_TEST_UNIT_READY;
	scsi_command_names[0].name = MSGSTR(61, "Test Unit Ready");

	scsi_command_names[1].command = SCMD_FORMAT;
	scsi_command_names[1].name = MSGSTR(110, "Format");

	scsi_command_names[2].command = SCMD_REASSIGN_BLOCK;
	scsi_command_names[2].name = MSGSTR(77, "Reassign Block");

	scsi_command_names[3].command = SCMD_READ;
	scsi_command_names[3].name = MSGSTR(27, "Read");

	scsi_command_names[4].command = SCMD_WRITE;
	scsi_command_names[4].name = MSGSTR(54, "Write");

	scsi_command_names[5].command = SCMD_READ_G1;
	scsi_command_names[5].name = MSGSTR(79, "Read(10 Byte)");

	scsi_command_names[6].command = SCMD_WRITE_G1;
	scsi_command_names[6].name = MSGSTR(51, "Write(10 Byte)");

	scsi_command_names[7].command = SCMD_MODE_SELECT;
	scsi_command_names[7].name = MSGSTR(97, "Mode Select");

	scsi_command_names[8].command = SCMD_MODE_SENSE;
	scsi_command_names[8].name = MSGSTR(95, "Mode Sense");

	scsi_command_names[9].command = SCMD_REASSIGN_BLOCK;
	scsi_command_names[9].name = MSGSTR(77, "Reassign Block");

	scsi_command_names[10].command = SCMD_REQUEST_SENSE;
	scsi_command_names[10].name = MSGSTR(74, "Request Sense");

	scsi_command_names[11].command = SCMD_READ_DEFECT_LIST;
	scsi_command_names[11].name = MSGSTR(80, "Read Defect List");

	scsi_command_names[12].command = SCMD_INQUIRY;
	scsi_command_names[12].name = MSGSTR(102, "Inquiry");

	scsi_command_names[13].command = SCMD_WRITE_BUFFER;
	scsi_command_names[13].name = MSGSTR(53, "Write Buffer");

	scsi_command_names[14].command = SCMD_READ_BUFFER;
	scsi_command_names[14].name = MSGSTR(82, "Read Buffer");

	scsi_command_names[15].command = SCMD_START_STOP;
	scsi_command_names[15].name = MSGSTR(67, "Start/Stop");

	scsi_command_names[16].command = SCMD_RESERVE;
	scsi_command_names[16].name = MSGSTR(72, "Reserve");

	scsi_command_names[17].command = SCMD_RELEASE;
	scsi_command_names[17].name = MSGSTR(75, "Release");

	scsi_command_names[18].command = SCMD_MODE_SENSE_G1;
	scsi_command_names[18].name = MSGSTR(84, "Mode Sense(10 Byte)");

	scsi_command_names[19].command = SCMD_MODE_SELECT_G1;
	scsi_command_names[19].name = MSGSTR(96, "Mode Select(10 Byte)");

	scsi_command_names[20].command = SCMD_READ_CAPACITY;
	scsi_command_names[20].name = MSGSTR(81, "Read Capacity");

	scsi_command_names[21].command = SCMD_SYNC_CACHE;
	scsi_command_names[21].name = MSGSTR(64, "Synchronize Cache");

	scsi_command_names[22].command = SCMD_READ_DEFECT_LIST;
	scsi_command_names[22].name = MSGSTR(80, "Read Defect List");

	scsi_command_names[23].command = SCMD_GDIAG;
	scsi_command_names[23].name = MSGSTR(108, "Get Diagnostic");

	scsi_command_names[24].command = SCMD_SDIAG;
	scsi_command_names[24].name = MSGSTR(69, "Set Diagnostic");

	scsi_command_names[25].command = SCMD_UNKNOWN;
	scsi_command_names[25].name = MSGSTR(25, "Unknown");


	for (c = scsi_command_names; c->command != SCMD_UNKNOWN; c++)
		if (c->command == cmd)
			break;
	return (c->name);
}

/*
 * XXX -- More dependancies for uscsi_cmd that are present in libg_fc,
 * but not exported...
 */
/*
 *		Special string dump for error message
 */
#define	BYTES_PER_LINE	16
static void
string_dump(char *hdr, uchar_t *src, int nbytes, int format, char msg_string[])
{
	int i;
	int n;
	char	*p;
	char	s[256];

	assert(format == HEX_ONLY || format == HEX_ASCII);

	(void) strcpy(s, hdr);
	for (p = s; *p; p++) {
		*p = ' ';
	}

	p = hdr;
	while (nbytes > 0) {
		(void) sprintf(&msg_string[strlen(msg_string)],
			"%s", p);
		p = s;
		n = MIN(nbytes, BYTES_PER_LINE);
		for (i = 0; i < n; i++) {
			(void) sprintf(&msg_string[strlen(msg_string)],
				"%02x ",
				src[i] & 0xff);
		}
		if (format == HEX_ASCII) {
			for (i = BYTES_PER_LINE-n; i > 0; i--) {
				(void) sprintf(&msg_string[strlen(msg_string)],
					"   ");
			}
			(void) sprintf(&msg_string[strlen(msg_string)],
				"    ");
			for (i = 0; i < n; i++) {
				(void) sprintf(&msg_string[strlen(msg_string)],
					"%c",
					isprint(src[i]) ? src[i] : '.');
			}
		}
		(void) sprintf(&msg_string[strlen(msg_string)], "\n");
		nbytes -= n;
		src += n;
	}
}
