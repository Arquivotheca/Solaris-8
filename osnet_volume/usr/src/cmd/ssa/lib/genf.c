/*
 * Copyright (c) 1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma	ident	"@(#)genf.c 1.10     99/06/04 SMI"


/*LINTLIBRARY*/

/*
 *  This module is part of the Command Interface Library
 *  for the Pluto User Interface.
 *
 */



#include	<stdlib.h>
#include 	<stdio.h>
#include	<sys/file.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/param.h>
#include	<sys/ioctl.h>
#include	<fcntl.h>
#include	<string.h>
#include	<errno.h>
#include	<assert.h>
#include	<memory.h>
#include	<unistd.h>
#include	<sys/types.h>
#include	<sys/param.h>
#include	<sys/dklabel.h>
#include	<sys/vtoc.h>
#include	<sys/dkio.h>
#include	<sys/mnttab.h>
#include	<sys/mntent.h>
#include	<sys/autoconf.h>
#include	<unistd.h>
#include 	<sys/ddi.h>	/* for min */
#include	<ctype.h>	/* for isprint */
#include	<sys/scsi/scsi.h>
#include	<libintl.h>	/* gettext */
#include	<dirent.h>	/* for DIR */

#include	"common.h"
#include	"scsi.h"
#include	"error.h"

/* function references */
static	void	string_dump(char *, u_char *, int, int, char msg_string[]);
		void	*zalloc(int);


/*	Global variables */
extern	char	p_error_msg[];	    /* global error message */
extern	char	*p_error_msg_ptr;   /* global pointer to the error message */

/*
 * Allocate space for and return a pointer to a string
 * on the stack.  If the string is null, create
 * an empty string.
 * Use destroy_data() to free when no longer used.
 */
char *
alloc_string(char *s)
{
	char	*ns;

	if (s == (char *)NULL) {
		ns = (char *)zalloc(1);
	} else {
		ns = (char *)zalloc(strlen(s) + 1);
		(void) strcpy(ns, s);
	}
	return (ns);
}


/*
 * Return true if we find an occurance of s2 at the
 * beginning of s1.  We don't have to match all of
 * s1, but we do have to match all of s2
 */
static
match_substr(char *s1, char *s2)
{
	while (*s2 != 0) {
		if (*s1++ != *s2++)
		return (0);
	}

	return (1);
}

/*
 * Create path name of the form:
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
	 * Create path name to individual drives
	 */
	/* get copy of controller path */
	(void) strcpy(drv_path, ctlr_path);
	/* point to the place to add */
	if ((char_ptr = strstr(drv_path, CTLR_POSTFIX)) == NULL) {
	    (void) sprintf(p_error_msg,
	    MSG("Invalid controller physical path name: %s\n"),
		ctlr_path);
	    /* set error ptr */
	    p_error_msg_ptr = (char *)&p_error_msg;
	    return (P_INVALID_PATH);
	}
	*char_ptr = '\0';	/* Terminate sting  */
	if ((dirp = opendir(drv_path)) == NULL) {
	    (void) sprintf(p_error_msg,
	    MSG("Error opening directory: %s\n"),
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
		MSG("Invalid device name: %s\n"),
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
 * This routine is a wrapper for free.
 */
void
destroy_data(char *data)
{
	free((char *)data);
}


char *
p_decode_sense(
	u_char	status)
{
	switch (status & STATUS_MASK) {
	case STATUS_GOOD:
		return ("good status");

	case STATUS_CHECK:
		return ("check condition");

	case STATUS_MET:
		return ("condition met");

	case STATUS_BUSY:
		return ("busy");

	case STATUS_INTERMEDIATE:
		return ("intermediate");

	case STATUS_INTERMEDIATE_MET:
		return ("intermediate - condition met");

	case STATUS_RESERVATION_CONFLICT:
		return ("reservation_conflict");

	case STATUS_TERMINATED:
		return ("command terminated");

	case STATUS_QFULL:
		return ("queue full");

	default:
		return ("<unknown status>");
	}
}

/*
 * Dump a structure in hexadecimal, for diagnostic purposes
 */
#define	BYTES_PER_LINE	16
void
p_dump(char *hdr, u_char *src, int nbytes, int format)
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
		(void) fprintf(stderr, "%s", p);
		p = s;
		n = min(nbytes, BYTES_PER_LINE);
		for (i = 0; i < n; i++) {
			(void) fprintf(stderr, "%02x ", src[i] & 0xff);
		}
		if (format == HEX_ASCII) {
			for (i = BYTES_PER_LINE-n; i > 0; i--) {
				(void) fprintf(stderr, "   ");
			}
			(void) fprintf(stderr, "    ");
			for (i = 0; i < n; i++) {
				(void) fprintf(stderr, "%c",
					isprint(src[i]) ? src[i] : '.');
			}
		}
		(void) fprintf(stderr, "\n");
		nbytes -= n;
		src += n;
	}
}


/*
 * Base pathname for devfs names to be stripped from physical name.
 */
#define	DEVFS_PREFIX	"/devices"


/*
 * Follow symbolic links from the logical device name to
 * the /devfs physical device name.  To be complete, we
 * handle the case of multiple links.  This function
 * either returns NULL (no links, or some other error),
 * or the physical device name, alloc'ed on the heap.
 *
 */
char *
get_physical_name_from_link(char *path)
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
		/*
		 * fprintf(stderr,
		 * "getcwd() failed - %s\n", sys_errlist[errno]);
		 */
		(void) fprintf(stderr,
			"getcwd() failed - %s\n", strerror(errno));
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
			(void) fprintf(stderr, "%s: lstat() failed - %s\n",
				s, strerror(errno));
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
					(void) fprintf(stderr,
					"getcwd() failed - %s\n",
					strerror(errno));
					goto exit;
				}
				(void) strcat(dir, "/");
				(void) strcat(dir, s);
				result = alloc_string(dir);
			}
			goto exit;
		}
		i = readlink(s, buf, sizeof (buf));
		if (i == -1) {
			(void) fprintf(stderr, "%s: readlink() failed - %s\n",
				s, strerror(errno));
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
			 * did not work here because path name
			 * I got was ../../devices
			 * so I am adding / to front
			 */
			(void) strcpy(s, "/");
			(void) strcat(s, dir);
			if (chdir(s) == -1) {
				(void) fprintf(stderr,
					"cannot chdir() to %s - %s\n",
					s, strerror(errno));
				goto exit;
			}
			(void) strcpy(s, p+1);
		} else {
			(void) strcpy(s, buf);
		}
	}

exit:
	if (chdir(savedir) == -1) {
		(void) fprintf(stderr, "cannot chdir() to %s - %s\n",
			savedir, strerror(errno));
	}

	return (result);
}

/*
 * Function for getting physical path names
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
 *	The physical path name is returned.
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
get_physical_name(char *path)
{
	struct stat	stbuf;
	char		s[MAXPATHLEN];
	char		namebuf[MAXPATHLEN];
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
		P_DPRINTF("get_physical_name: "
			"Entry of the form cN n=%s len=%d\n",
			&s[1], strlen(s));

		dev_name = zalloc(sizeof ("/dev/rdsk"));
		sprintf((char *)dev_name, "/dev/rdsk");

		if ((dirp = opendir(dev_name)) == NULL) {
			(void) fprintf(stderr,
			"Unable to open /dev/rdsk - %s\n",
			strerror(errno));
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
			fprintf(stderr,
			    "Warning: Cannot stat %s\n", namebuf);
			continue;
		    }

		    if (!S_ISLNK(sb.st_mode)) {
			fprintf(stderr,
			"Warning: %s is not a symbolic link\n",
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
			    P_DPRINTF("get_physical_name: "
			    "Found entry in /dev/rdsk matching %s: %s\n",
				s, entp->d_name);
				found_flag = 1;
				break;
			}
		    }
		}
		closedir(dirp);

		if (found_flag) {
		    result = get_physical_name_from_link(namebuf);
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
		(void) fprintf(stderr, "%s: lstat() failed - %s\n",
			s, strerror(errno));
		goto exit;
	}
	/*
	 */
	if (!S_ISLNK(stbuf.st_mode)) {
		/*
		 * Path is not a linked file so must be
		 * a physical path
		 *
		 * Check to be sure it starts with a /
		 */
		if (s[0] == '/') {
			result = alloc_string(s);
		}
	} else {
		/*
		 * Entry is linked file
		 * so follow link to physical name
		 */
		result = get_physical_name_from_link(path);
	}

exit:
	return (result);
}


/*
 *	Function to open a USCSI device
 */

int
object_open(char *path, int flag)
{
int fd;
	if ((fd = open(path, flag)) == -1) {
		(void) sprintf(p_error_msg,
			MSG("Error opening %s\n%s\n"),
			path, strerror(errno));
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
	}
	O_DPRINTF("Object_open: Opening fd %d, file %s\n", fd, path);
	return (fd);
}


/*
 * Return a pointer to a string telling us the name of the command.
 */
char *
scsi_find_command_name(int cmd)
{
#define	SCMD_UNKNOWN	0xff
/*
 * Names of commands.  Must have SCMD_UNKNOWN at end of list.
 */
struct scsi_command_name {
	int command;
	char	*name;
} scsi_command_names[] = {
	SCMD_TEST_UNIT_READY,	"Test Unit Ready",
	SCMD_FORMAT,		"Format",
	SCMD_REASSIGN_BLOCK,	"Reassign Block",
	SCMD_READ,		"Read",
	SCMD_WRITE,		"Write",
	SCMD_READ_G1,		"Read(10 Byte)",
	SCMD_WRITE_G1, 		"Write(10 Byte)",
	SCMD_MODE_SELECT,	"Mode Select",
	SCMD_MODE_SENSE,	"Mode Sense",
	SCMD_REASSIGN_BLOCK,	"Reassign Block",
	SCMD_REQUEST_SENSE,	"Request Sense",
	SCMD_READ_DEFECT_LIST,  "Read Defect List",
	SCMD_INQUIRY,		"Inquiry",
	SCMD_WRITE_BUFFER,	"Write Buffer",
	SCMD_READ_BUFFER,	"Read Buffer",
	SCMD_START_STOP,	"Start/Stop",
	SCMD_RESERVE,		"Reserve",
	SCMD_RELEASE,		"Release",
	SCMD_MODE_SENSE_G1,	"Mode Sense(10 Byte)",
	SCMD_MODE_SELECT_G1,	"Mode Select(10 Byte)",
	SCMD_READ_CAPACITY,	"Read Capacity",
	SCMD_SYNC_CACHE,	"Synchronize Cache",
	SCMD_READ_DEFECT_LIST,	"Read Defect List",
	SCMD_GDIAG,		"Get Diagnostic",
	SCMD_SDIAG,		"Set Diagnostic",
	SCMD_UNKNOWN,		"unknown"
};
register struct scsi_command_name *c;

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
scsi_printerr(struct uscsi_cmd *ucmd, struct scsi_extended_sense *rq, int rqlen,
		char msg_string[])
{
	int		blkno;

	switch (rq->es_key) {
	case KEY_NO_SENSE:
		(void) sprintf(msg_string, MSG("No sense error"));
		break;
	case KEY_RECOVERABLE_ERROR:
		(void) sprintf(msg_string, MSG("Recoverable error"));
		break;
	case KEY_NOT_READY:
		(void) sprintf(msg_string, MSG("Not ready error"));
		break;
	case KEY_MEDIUM_ERROR:
		(void) sprintf(msg_string, MSG("Medium error"));
		break;
	case KEY_HARDWARE_ERROR:
		(void) sprintf(msg_string, MSG("Hardware error"));
		break;
	case KEY_ILLEGAL_REQUEST:
		(void) sprintf(msg_string, MSG("Illegal request"));
		break;
	case KEY_UNIT_ATTENTION:
		(void) sprintf(msg_string, MSG("Unit attention error"));
		break;
	case KEY_WRITE_PROTECT:
		(void) sprintf(msg_string, MSG("Write protect error"));
		break;
	case KEY_BLANK_CHECK:
		(void) sprintf(msg_string, MSG("Blank check error"));
		break;
	case KEY_VENDOR_UNIQUE:
		(void) sprintf(msg_string, MSG("Vendor unique error"));
		break;
	case KEY_COPY_ABORTED:
		(void) sprintf(msg_string, MSG("Copy aborted error"));
		break;
	case KEY_ABORTED_COMMAND:
		(void) sprintf(msg_string, MSG("Aborted command"));
		break;
	case KEY_EQUAL:
		(void) sprintf(msg_string, MSG("Equal error"));
		break;
	case KEY_VOLUME_OVERFLOW:
		(void) sprintf(msg_string, MSG("Volume overflow"));
		break;
	case KEY_MISCOMPARE:
		(void) sprintf(msg_string, MSG("Miscompare error"));
		break;
	case KEY_RESERVED:
		(void) sprintf(msg_string, MSG("Reserved error"));
		break;
	default:
		(void) sprintf(msg_string, MSG("Unknown error"));
		break;
	}

	(void) sprintf(&msg_string[strlen(msg_string)],
		MSG(" during %s"), scsi_find_command_name(ucmd->uscsi_cdb[0]));

	if (rq->es_valid) {
		blkno = (rq->es_info_1 << 24) | (rq->es_info_2 << 16) |
			(rq->es_info_3 << 8) | rq->es_info_4;
		(void) sprintf(&msg_string[strlen(msg_string)],
			MSG(": block %d (0x%x)"), blkno, blkno);
	}

	(void) sprintf(&msg_string[strlen(msg_string)], "\n");

	if (rq->es_add_len >= 6) {
		(void) sprintf(&msg_string[strlen(msg_string)],
		MSG("Additional sense: 0x%x   ASC Qualifier: 0x%x\n"),
			rq->es_add_code, rq->es_qual_code);
			/*
			 * rq->es_add_info[ADD_SENSE_CODE],
			 * rq->es_add_info[ADD_SENSE_QUAL_CODE]);
			 */
	}
	if (rq->es_key == KEY_ILLEGAL_REQUEST) {
		string_dump(MSG("cmd:    "), (u_char *) ucmd,
			sizeof (struct uscsi_cmd), HEX_ONLY, msg_string);
		string_dump(MSG("cdb:    "), (u_char *) ucmd->uscsi_cdb,
			ucmd->uscsi_cdblen, HEX_ONLY, msg_string);
	}
	string_dump(MSG("sense:  "),
		(u_char *) rq, 8 + rq->es_add_len, HEX_ONLY,
		msg_string);
	rqlen = rqlen;	/* not used */
}


/*
 *		Special string dump for error message
 */
static void
string_dump(char *hdr, u_char *src, int nbytes, int format, char msg_string[])
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
zalloc(int count)
{
	void	*ptr;

	if ((ptr = (void *) calloc(1, (unsigned)count)) == NULL) {
		(void) sprintf(p_error_msg,
			"Error: unable to calloc more space.\n");
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
	}
	return (ptr);
}
