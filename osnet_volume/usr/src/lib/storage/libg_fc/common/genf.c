/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)genf.c	1.20	99/10/14 SMI"


/*LINTLIBRARY*/

/*
 *	This module is part of the Fibre Channel Interface library.
 *
 */

/*
 * I18N message number ranges
 *  This file: 10500 - 10999
 *  Shared common messages: 1 - 1999
 */

/*	Includes	*/
#include	<stdlib.h>
#include 	<stdio.h>
#include	<sys/file.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/param.h>
#include	<fcntl.h>
#include	<string.h>
#include	<errno.h>
#include	<assert.h>
#include	<unistd.h>
#include	<sys/types.h>
#include	<sys/param.h>
#include	<sys/dklabel.h>
#include	<sys/autoconf.h>
#include	<sys/utsname.h>
#include 	<sys/ddi.h>		/* for min */
#include	<ctype.h>		/* for isprint */
#include	<sys/scsi/scsi.h>
#include	<dirent.h>		/* for DIR */
#include	<nl_types.h>
#include	<locale.h>
#include	<thread.h>
#include	<synch.h>
#include	<l_common.h>
#include	<stgcom.h>
#include	<l_error.h>
#include	<g_state.h>


/*	Defines		*/
#define	FAKE_FILE_NAME		"/xxx"
#define	FAKE_FILE_NAME_LEN	4
#define	BYTES_PER_LINE		16	/* # of bytes to dump per line */
#define	SCMD_UNKNOWN		0xff

/* Constants and macros used by the g_get_path_type() function */
#define	SLASH		"/"
#define	DEV_PREFIX	"/devices/"	/* base pathname for devfs names */
#define	DEV_PREFIX_LEN	9		/* Length of DEV_PREFIX string */
					/* Can do a strlen and generalize */
					/* but this is is easier */

/* Bus strings - for internal use by g_get_path_type() only */
#define	PCI_BUS			1
#define	SBUS			2
#define	SUN4D_BUS		3

/* Strings specific to Sun4d systems */
#define	SUN4D_STRING1		"sbi@"
#define	SUN4D_STRING1_LEN	4

/* Strings specific to PCI systems */
#define	PCI_STRING1		"pci@"
#define	PCI_STRING1_LEN		4
#define	PCI_STRING2		"scsi@"
#define	PCI_STRING2_LEN		5
#define	PCI_STRING3		"fibre-channel@"
#define	PCI_STRING3_LEN		14

static struct str_type {
	char *string;
	uint_t type;
};

static struct str_type ValidBusStrings[] = {
	{"pci@", PCI_BUS},
	{"sbus@", SBUS},
	{"io-unit@", SUN4D_BUS},
	{NULL, 0}
};

static struct str_type ValidFCAstrings[] = {
	{"SUNW,ifp@", FC4_PCI_FCA},
	{"SUNW,socal@", FC4_SOCAL_FCA},
	{"SUNW,usoc@", FC_USOC_FCA},
	{"scsi/", FC_PCI_FCA},
	{"fibre-channel/", FC_PCI_FCA},
	{NULL, 0}
};

static struct str_type ValidXportStrings[] = {
	{"/sf@", FC4_SF_XPORT},
	{"/fp@", FC_GEN_XPORT},
	{NULL, 0}
};

/* i18n */
nl_catd		l_catd;


/*	External functions	*/

/*	Internal Functions	*/
static	void	string_dump(char *, uchar_t *, int, int, char msg_string[]);

/*
 * This function finds the architecture
 * of a host machine and sets the name_id
 * variable to either 1 (in case of 4m/4d) or
 * 0 (if arch. is other than 4m/4d).
 *
 * RETURNS:
 *	0	 if O.K.
 *	non-zero otherwise.
 */
int
g_get_machineArch(int *name_id)
{
struct utsname	name;


	if (uname(&name) == -1) {
		return (L_UNAME_FAILED);
	}

	if ((strcmp(name.machine, L_ARCH_4D) == 0) ||
		(strcmp(name.machine, L_ARCH_4M) == 0)) {
		*name_id = 1;
	} else {
		*name_id = 0;
	}

	return (0);
}



/*
 * Allocate space for and return a pointer to a string
 * on the stack.  If the string is null, create
 * an empty string.
 * Use g_destroy_data() to free when no longer used.
 */
char *
g_alloc_string(char *s)
{
	char	*ns;

	if (s == (char *)NULL) {
		ns = (char *)g_zalloc(1);
	} else {
		ns = (char *)g_zalloc(strlen(s) + 1);
		if (ns != NULL) {
			(void) strncpy(ns, s, (strlen(s) + 1));
		}
	}
	return (ns);
}


/*
 * This routine is a wrapper for free.
 */
void
g_destroy_data(void *data)
{
	A_DPRINTF("  g_destroy_data: Free\'ed buffer at 0x%x\n",
		data);
	free((void *)data);
}


/*
 * Dump a structure in hexadecimal.
 */
void
g_dump(char *hdr, uchar_t *src, int nbytes, int format)
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
		(void) fprintf(stdout, "%s", p);
		p = s;
		n = min(nbytes, BYTES_PER_LINE);
		for (i = 0; i < n; i++) {
			(void) fprintf(stdout, "%02x ", src[i] & 0xff);
		}
		if (format == HEX_ASCII) {
			for (i = BYTES_PER_LINE-n; i > 0; i--) {
				(void) fprintf(stdout, "   ");
			}
			(void) fprintf(stdout, "    ");
			for (i = 0; i < n; i++) {
				(void) fprintf(stdout, "%c",
					isprint(src[i]) ? src[i] : '.');
			}
		}
		(void) fprintf(stdout, "\n");
		nbytes -= n;
		src += n;
	}
}




/*
 * Follow symbolic links from the logical device name to
 * the /devfs physical device name.  To be complete, we
 * handle the case of multiple links.  This function
 * either returns NULL (no links, or some other error),
 * or the physical device name, alloc'ed on the heap.
 *
 */
char *
g_get_physical_name_from_link(char *path)
{
	struct stat	stbuf;
	int		i;
	int		level;
	char		*p;
	char		s[MAXPATHLEN];
	char		buf[MAXPATHLEN];
	char		dir[MAXPATHLEN];
	char		savedir[MAXPATHLEN];
	char		*result = NULL;

	if (getcwd(savedir, sizeof (savedir)) == NULL) {
		O_DPRINTF("getcwd() failed - %s\n", strerror(errno));
		return (NULL);
	}

	(void) strcpy(s, path);
	level = 0;
	for (;;) {
		/*
		 * See if there's a real file out there.  If not,
		 * we have a dangling link and we ignore it.
		 */
		if (stat(s, &stbuf) == -1) {
			goto exit;
		}
		if (lstat(s, &stbuf) == -1) {
			goto exit;
		}
		/*
		 * If the file is not a link, we're done one
		 * way or the other.  If there were links,
		 * return the full pathname of the resulting
		 * file.
		 */
		if (!S_ISLNK(stbuf.st_mode)) {
			if (level > 0) {
				/*
				 * Get the current directory, and
				 * glue the pieces together.
				 */
				if (getcwd(dir, sizeof (dir)) == NULL) {
					goto exit;
				}
				(void) strcat(dir, "/");
				(void) strcat(dir, s);
				result = g_alloc_string(dir);
			}
			goto exit;
		}
		i = readlink(s, buf, sizeof (buf));
		if (i == -1) {
			goto exit;
		}
		level++;
		buf[i] = 0;

		/*
		* Break up the pathname into the directory
		* reference, if applicable and simple filename.
		* chdir()'ing to the directory allows us to
		* handle links with relative pathnames correctly.
		*/
		(void) strcpy(dir, buf);
		if ((p = strrchr(dir, '/')) != NULL) {
			*p = 0;
			/*
			 * did not work here because pathname
			 * I got was ../../devices
			 * so I am adding / to front
			 */
			(void) strcpy(s, "/");
			(void) strcat(s, dir);
			if (chdir(s) == -1) {
				goto exit;
			}
			(void) strcpy(s, p+1);
		} else {
			(void) strcpy(s, buf);
		}
	}

exit:
	chdir(savedir);

	return (result);
}

/*
 * Function for getting physical pathnames
 *
 * This function can handle 3 different inputs.
 *
 * 1) Inputs of the form cN
 *	This requires the program  to search the /dev/rdsk
 *	directory for a device that is conected to the
 *	controller with number 'N' and then getting
 *	the physical pathname of the controller.
 *	The format of the controller pathname is
 *	/devices/.../.../SUNW,soc@x,x/SUNW,pln@xxxx,xxxxxxxx:ctlr
 *	The physical pathname is returned.
 *
 * 2) Inputs of the form /dev/rdsk/cNtNdNsN
 *	These are identified by being a link
 *	The physical path they are linked to is returned.
 *
 * 3) Inputs of the form /devices/...
 *	These are actual physical names.
 *	They are not converted.
 */
char *
g_get_physical_name(char *path)
{
	struct stat	stbuf;
	char		s[MAXPATHLEN];
	char		namebuf[MAXPATHLEN];
	char		savedir[MAXPATHLEN];
	char		*result = NULL;
	DIR		*dirp;
	struct dirent	*entp;
	char		*dev_name, *char_ptr;
	struct stat	sb;
	int		found_flag = 0;
	int		status = 0;
	int		i;

	(void) strcpy(s, path);
	/*
	 * See if the form is cN
	 * Must handle scenaro where there is a file cN in this directory
	 * Bug ID: 1184633
	 *
	 * We could be in the /dev/rdsk directory and the file could be of
	 * the form cNdNsN (See man disks).
	 */
	status = stat(s, &stbuf);
	if (((status == -1) && (errno == ENOENT)) ||
	    ((s[0] == 'c') && ((int)strlen(s) > 1) && ((int)strlen(s) < 5))) {
		/*
		 * Further qualify cN entry
		 */
		if ((s[0] != 'c') || ((int)strlen(s) <= 1) ||
		((int)strlen(s) >= 5)) {
			goto exit;
		}
		for (i = 1; i < (int)strlen(s); i++) {
			if ((s[i] < '0') || (s[i] > '9')) {
				goto exit;
			}
		}
		/*
		 * path does not point to a file or file is of form cN
		 */
		P_DPRINTF("  g_get_physical_name: "
			"Found entry of the form cN n=%s len=%d\n",
			&s[1], strlen(s));

		dev_name = g_zalloc(sizeof ("/dev/rdsk"));
		sprintf((char *)dev_name, "/dev/rdsk");

		if ((dirp = opendir(dev_name)) == NULL) {
			g_destroy_data(dev_name);
			goto exit;
		}

		while ((entp = readdir(dirp)) != NULL) {
		    if (strcmp(entp->d_name, ".") == 0 ||
			strcmp(entp->d_name, "..") == 0)
			continue;

		    if (entp->d_name[0] != 'c')
			/*
			 * Silently Ignore for now any names
			 * not stating with c
			 */
			continue;

		    sprintf(namebuf, "%s/%s", dev_name, entp->d_name);

		    if ((lstat(namebuf, &sb)) < 0) {
				L_WARNINGS(MSGSTR(55,
					"Warning: Cannot stat %s\n"),
					namebuf);
			continue;
		    }

		    if (!S_ISLNK(sb.st_mode)) {
				L_WARNINGS(MSGSTR(56,
					"Warning: %s is not a symbolic link\n"),
					namebuf);
			continue;
		    }

		    if (strstr(entp->d_name, s) != NULL) {
			/*
			 * found link to device in /devices
			 *
			 * Further qualify to be sure I have
			 * not found entry of the form c10
			 * when I am searching for c1
			 */
			if (atoi(&s[1]) == atoi(&entp->d_name[1])) {
			    P_DPRINTF("  g_get_physical_name: "
			    "Found entry in /dev/rdsk matching %s: %s\n",
				s, entp->d_name);
				found_flag = 1;
				break;
			}
		    }
		}
		closedir(dirp);
		g_destroy_data(dev_name);

		if (found_flag) {
		    result = g_get_physical_name_from_link(namebuf);
		    if (result == NULL) {
			goto exit;
		    }
			/*
			 * Convert from device name to controller name
			 */
		    char_ptr = strrchr(result, '/');
		    *char_ptr = '\0';   /* Terminate sting  */
		    (void) strcat(result, CTLR_POSTFIX);
		}
		goto exit;
	}
	if (status == -1)
		goto exit;

	if (lstat(s, &stbuf) == -1) {
			L_WARNINGS(MSGSTR(134,
				"%s: lstat() failed - %s\n"),
				s, strerror(errno));
		goto exit;
	}
	/*
	 */
	if (!S_ISLNK(stbuf.st_mode)) {
		/*
		 * Path is not a linked file so must be
		 * a physical path
		 */
		if (S_ISCHR(stbuf.st_mode) || S_ISDIR(stbuf.st_mode)) {
			/* Make sure a full path as that is required. */
			if (strstr(s, "/devices")) {
				result = g_alloc_string(s);
			} else {
				if (getcwd(savedir,
					sizeof (savedir)) == NULL) {
					return (NULL);
				}
				/*
				 * Check for this format:
				 * ./ssd@0,1:g,raw
				 */
				if (s[0] == '.') {
					strcat(savedir, &s[1]);
				} else {
					strcat(savedir, "/");
					strcat(savedir, s);
				}
				result = g_alloc_string(savedir);
			}
		}
	} else {
		/*
		 * Entry is linked file
		 * so follow link to physical name
		 */
		result = g_get_physical_name_from_link(path);
	}

exit:
	return (result);
}

#ifdef	TWO_SIX

/*
 * function to get device or bus physical name for hot plugging
 *
 * just return the name g_get_physical_name() gives us, unless it
 * ends in ":ctlr"
 *
 * in this case we convert the name to any old fake name in the
 * next directory down, so that libdevice can handle it, i.e.:
 *
 *	if the result from g_get_physical_name is like:
 *
 *		/devices/.../HBA_NAME:ctlr
 *
 *	then we convert it to something like:
 *
 *		/devices/.../HAB_NAME/some_fake_name
 *
 * XXX: note that the libdevice routine devctl_acquire() *should* handle
 * passing in the name of the ":devctl" node directly, but it doesn't
 * (see RFE 4038181)
 *
 * assume:
 *	* input path is non-null
 */
char *
g_get_dev_or_bus_phys_name(char *path)
{
	char	*res;
	char	*phys;
	char	*cp;


	/* use get_physcial_name() on supplied path */
	if ((phys = g_get_physical_name(path)) == NULL) {
		return (NULL);
	}

	/* check for the devctl or ctlr node name */
	cp = phys + strlen(phys);
	if ((strcmp(cp - strlen(CTLR_POSTFIX), CTLR_POSTFIX) != 0) &&
	    (strcmp(cp - strlen(DEVCTL_POSTFIX), DEVCTL_POSTFIX) != 0)) {
		/* just return the name we got from g_get_physical_name() */
		return (phys);
	}

	/*
	 * XXX: note that we assume that both CTLR_POSTFIX and DEVCTL_POSTFIX
	 * are delinated by a colon character here
	 */

	/* we have a nexus driver devctl or ctlr name */
	cp = strrchr(phys, ':');
	*cp = '\0';
	res = g_zalloc(strlen(phys) + FAKE_FILE_NAME_LEN + 1);
	(void) strcpy(res, phys);
	(void) strcat(res, FAKE_FILE_NAME);
	g_destroy_data(phys);

	P_DPRINTF("g_get_dev_or_bus_phys_name: "
	    "returning: %s\n", res);
	return (res);
}

#else	/* TWO_SIX */

/*
 * dummy function for Solaris releases other than 2.6.
 * XXX - Is there a better way of doing this?
 *
 */
char *
g_get_dev_or_bus_phys_name(char *path)
{
	return (NULL);
}

#endif	/* TWO_SIX */



/*
 *	Function to open a device
 */
int
g_object_open(char *path, int flag)
{
int fd;
	if (getenv("_LUX_O_DEBUG") != NULL) {
		(void) printf("  Object_open:%s ", path);
		if (flag & O_WRONLY) {
			(void) printf("O_WRONLY,");
		} else if (flag & O_RDWR) {
			(void) printf("O_RDWR,");
		} else {
			(void) printf("O_RDONLY,");
		}
		if (flag & O_NDELAY) {
			(void) printf("O_NDELAY,");
		}
		if (flag & O_APPEND) {
			(void) printf("O_APPEND,");
		}
		if (flag & O_DSYNC) {
			(void) printf("O_DSYNC,");
		}
		if (flag & O_RSYNC) {
			(void) printf("O_RSYNC,");
		}
		if (flag & O_SYNC) {
			(void) printf("O_SYNC,");
		}
		if (flag & O_NOCTTY) {
			(void) printf("O_NOCTTY,");
		}
		if (flag & O_CREAT) {
			(void) printf("O_CREAT,");
		}
		if (flag & O_EXCL) {
			(void) printf("O_EXCL,");
		}
		if (flag & O_TRUNC) {
			(void) printf("O_TRUNC,");
		}
		(void) printf("\n");
	}
	if ((fd = open(path, flag)) == -1) {
		O_DPRINTF("  Object_open: Open failed:%s\n", path);
	}
	return (fd);
}


/*
 * Return a pointer to a string telling us the name of the command.
 */
char *
g_scsi_find_command_name(int cmd)
{
/*
 * Names of commands.  Must have SCMD_UNKNOWN at end of list.
 */
struct scsi_command_name {
	int command;
	char	*name;
} scsi_command_names[29];

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
	scsi_command_names[18].name = MSGSTR(94, "Mode Sense(10 Byte)");

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

	scsi_command_names[25].command = SCMD_PERS_RESERV_IN;
	scsi_command_names[25].name = MSGSTR(10500, "Persistent Reserve In");

	scsi_command_names[26].command = SCMD_PERS_RESERV_OUT;
	scsi_command_names[26].name = MSGSTR(10501, "Persistent Reserve out");

	scsi_command_names[27].command = SCMD_LOG_SENSE;
	scsi_command_names[27].name = MSGSTR(10502, "Log Sense");

	scsi_command_names[28].command = SCMD_UNKNOWN;
	scsi_command_names[28].name = MSGSTR(25, "Unknown");


	for (c = scsi_command_names; c->command != SCMD_UNKNOWN; c++)
		if (c->command == cmd)
			break;
	return (c->name);
}


/*
 *	Function to create error message containing
 *	scsi request sense information
 */

void
g_scsi_printerr(struct uscsi_cmd *ucmd, struct scsi_extended_sense *rq,
		int rqlen, char msg_string[], char *err_string)
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
		(void) sprintf(msg_string,
			MSGSTR(10503,
			"Device Not ready."
			" Error: Random Retry Failed: %s\n."),
			err_string);
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
		(void) sprintf(msg_string,
			MSGSTR(10504,
			"Unit attention."
			"Error: Random Retry Failed.\n"));
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
		(void) sprintf(msg_string,
			MSGSTR(10505,
			"Aborted command."
			" Error: Random Retry Failed.\n"));
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
		(void) sprintf(msg_string, MSGSTR(10506,
			"Reserved value found"));
		break;
	default:
		(void) sprintf(msg_string, MSGSTR(59, "Unknown error"));
		break;
	}

	(void) sprintf(&msg_string[strlen(msg_string)],
		MSGSTR(10507, " during: %s"),
		g_scsi_find_command_name(ucmd->uscsi_cdb[0]));

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
		string_dump(MSGSTR(48, " cdb:   "),
			(uchar_t *)ucmd->uscsi_cdb,
			ucmd->uscsi_cdblen, HEX_ONLY, msg_string);
	}
	string_dump(MSGSTR(43, " sense:  "),
		(uchar_t *)rq, 8 + rq->es_add_len, HEX_ONLY,
		msg_string);
	rqlen = rqlen;	/* not used */
}


/*
 *		Special string dump for error message
 */
static	void
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
		n = min(nbytes, BYTES_PER_LINE);
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



/*
 * This routine is a wrapper for malloc.  It allocates pre-zeroed space,
 * and checks the return value so the caller doesn't have to.
 */
void *
g_zalloc(int count)
{
	void	*ptr;

	ptr = (void *) calloc(1, (unsigned)count);
	A_DPRINTF("  g_zalloc: Allocated 0x%x bytes "
			"at 0x%x\n", count, ptr);

	return (ptr);
}

/*
 * Open up the i18n catalog.
 * Returns:
 *  0 = O.K.
 * -1 = Failed (Will revert to default strings)
 */
int
g_i18n_catopen(void)
{
	static int fileopen = 0;
	static mutex_t mp;

	if (setlocale(LC_ALL, "") == NULL) {
	    (void) fprintf(stderr,
		"Cannot operate in the locale requested. "
		"Continuing in the default C locale\n");
	}
	if (mutex_lock(&mp) != 0) {
		return (-1);
	}
	if (!fileopen) {
		l_catd = catopen("a5k_g_fc_i18n_cat", NL_CAT_LOCALE);
		if (l_catd == (nl_catd)-1) {
			(void) mutex_unlock(&mp);
			return (-1);
		}
		fileopen = 1;
	}
	(void) mutex_unlock(&mp);
	return (0);
}

/* Macro used by g_get_path_type() */
#define	GetMatch(s_ptr)	\
	for (found = 0, search_arr_ptr = s_ptr; \
		search_arr_ptr->string != NULL; \
			search_arr_ptr++) {\
		if (strstr(path_ptr, search_arr_ptr->string) != NULL) {\
			found = 1;\
			break;\
		}\
	}

/*
 * Input  : A NULL terminated string
 *          This string is checked to be an absolute device path
 * Output :
 * 	The FCA type and Xport type if found in the path on success
 *	0 on Failure
 *
 * Examples of valid device strings :
 *
 * Non Fabric FC driver :
 * /devices/io-unit@f,e0200000/sbi@0,0/SUNW,socal@1,0/sf@1,0:ctlr
 * /devices/io-unit@f,e2200000/sbi@0,0/SUNW,socal@3,0/sf@0,0/ssd@20,0:c,raw
 * /devices/sbus@1f,0/SUNW,socal@0,0/sf@0,0:devctl
 * /devices/sbus@1f,0/SUNW,socal@2,0/sf@1,0/ssd@w2200002037110cbf,0:b,raw
 * /devices/pci@1f,4000/SUNW,ifp@4:devctl
 * /devices/pci@1f,4000/SUNW,ifp@2/ssd@w2100002037049ba0,0:c,raw
 * /devices/pci@6,4000/pci@2/SUNW,ifp@5/ssd@w210000203708b44f,0:c,raw
 *
 * Fabric FC driver (fp) :
 * /devices/sbus@2,0/SUNW,usoc@1,0/fp@0,0:devctl
 * /devices/sbus@2,0/SUNW,usoc@1,0/fp@0,0/ssd@w21A0002037C70588,0:c,raw
 * /devices/pci@1f,4000/pci@2/scsi@5/fp@0,0:devctl
 * /devices/pci@1f,4000/scsi/fp@0,0:devctl
 */
uint_t
g_get_path_type(char *path)
{
	uint_t path_type = 0;
	char *path_ptr = path;
	struct str_type *search_arr_ptr; /* updated by GetMatch macro */
	char found;			 /* Updated by GetMatch marco */

	/* Path passed must be an absolute device path */
	if (strncmp(path_ptr, DEV_PREFIX, DEV_PREFIX_LEN) ||
				(strlen(path_ptr) == DEV_PREFIX_LEN)) {
		return (0);	/* Invalid path */
	}

	GetMatch(ValidBusStrings);
	if (found == 0) {
		/* No valid bus string - so not a valid path */
		return (0);
	}

	GetMatch(ValidFCAstrings);	/* Check for a valid FCA string */
	if (found == 0) {
		return (path_type);
	}
	path_type |= search_arr_ptr->type;

	GetMatch(ValidXportStrings);	/* Check for a valid transport str */
	if (found == 0) {
		return (path_type);
	}
	path_type |= search_arr_ptr->type;

	/*
	 * A quick sanity check to make sure that we dont have
	 * a combination that is not possible
	 */
	if (((path_type & (FC4_FCA_MASK | FC_XPORT_MASK)) ==
			path_type) ||
		((path_type & (FC_FCA_MASK | FC4_XPORT_MASK)) ==
			path_type)) {
		path_type = 0;
	}

	return (path_type);
}
