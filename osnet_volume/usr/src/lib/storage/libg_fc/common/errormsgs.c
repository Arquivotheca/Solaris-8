/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)errormsgs.c	1.25	99/08/20 SMI"

/*LINTLIBRARY*/

/*
 *	This module provides error messages to the
 *	a5k and g_fc libraries.
 */

/*
 * I18N message number ranges
 *  This file: 10000 - 10499
 *  Shared common messages: 1 - 1999
 */

/* #define	_POSIX_SOURCE 1 */


/*	Includes	*/
#include	<stdlib.h>
#include	<stdio.h>
#include	<fcntl.h>
#include	<nl_types.h>
#include	<sys/scsi/scsi.h>
#include	<stgcom.h>
#include	<l_error.h>	/* error msg defines. */
#include	<l_common.h>	/* I18N	message define */
#include	<g_state.h>

#include	<string.h> /* For strerror */
#include	<errno.h>


/*	Defines		*/
#define	MAXLEN	1000



/*
 * Decodes the SCSI sense byte to a string.
 *
 * RETURNS:
 *	character string
 */
static char *
decode_sense_byte(uchar_t status)
{
	switch (status & STATUS_MASK) {
		case STATUS_GOOD:
			return (MSGSTR(10000, "Good status"));

		case STATUS_CHECK:
			return (MSGSTR(128, "Check condition"));

		case STATUS_MET:
			return (MSGSTR(124, "Condition met"));

		case STATUS_BUSY:
			return (MSGSTR(37, "Busy"));

		case STATUS_INTERMEDIATE:
			return (MSGSTR(10001, "Intermediate"));

		case STATUS_INTERMEDIATE_MET:
			return (MSGSTR(10002, "Intermediate - condition met"));

		case STATUS_RESERVATION_CONFLICT:
			return (MSGSTR(10003, "Reservation_conflict"));

		case STATUS_TERMINATED:
			return (MSGSTR(126, "Command terminated"));

		case STATUS_QFULL:
			return (MSGSTR(83, "Queue full"));

		default:
			return (MSGSTR(4, "Unknown status"));
	}
}


/*
 * This function finds a predefined error string to a given
 * error number (errornum), allocates memory for the string
 * and returns the corresponding error message to the caller.
 *
 * RETURNS
 *	error string	if O.K.
 *	NULL		otherwise
 */
char
*g_get_errString(int errornum)
{
char	err_msg[MAXLEN], *errStrg;

	err_msg[0] = '\0'; /* Just in case */
	if (errornum < L_BASE) {
			/* Some sort of random system error most likely */
			errStrg = strerror(errno);
			if (errStrg != NULL) {
				(void) strcpy(err_msg, errStrg);
			} else { /* Something's _really_ messed up */
				(void) sprintf(err_msg,
					MSGSTR(10081,
					" Error: could not decode the"
					" error message.\n"
					" The given error message is not"
					" defined in the library.\n"
					" Message number: %d.\n"), errornum);
			}

	/* Make sure ALL CASES set err_msg to something */
	} else switch (errornum) {
		case L_SCSI_ERROR:
			(void) sprintf(err_msg,
				MSGSTR(10096,
				" Error: SCSI failure."));
			break;

		case L_PR_INVLD_TRNSFR_LEN:
			(void) sprintf(err_msg,
				MSGSTR(10005,
				" Error: Persistant Reserve command"
				" transfer length not word aligned."));
			break;

		case L_RD_NO_DISK_ELEM:
			(void) sprintf(err_msg,
				MSGSTR(10006,
				" Error: Could not find the disk elements"
				" in the Receive Diagnostic pages."));
			break;

		case L_RD_INVLD_TRNSFR_LEN:
			(void) sprintf(err_msg,
				MSGSTR(10007,
				" Error: Receive Diagnostic command"
				" transfer length not word aligned."));
			break;

		case L_ILLEGAL_MODE_SENSE_PAGE:
			(void) sprintf(err_msg,
				MSGSTR(10008,
				" Error: Programming error - "
				"illegal Mode Sense parameter."));
			break;

		case L_INVALID_NO_OF_ENVSEN_PAGES:
			(void) sprintf(err_msg,
				MSGSTR(10009,
				" Error: Invalid no. of sense pages.\n"
				" Could not get valid sense page"
				" information from the device."));
			break;

		case L_INVALID_BUF_LEN:
			(void) sprintf(err_msg,
				MSGSTR(10010,
				" Error: Invalid buffer length.\n"
				" Could not get diagnostic "
				" information from the device."));
			break;

		case L_INVALID_PATH:
			(void) sprintf(err_msg,
				MSGSTR(113,
				" Error: Invalid pathname"));
			break;

		case L_NO_PHYS_PATH:
			(void) sprintf(err_msg,
				MSGSTR(10011,
				" Error: Could not get"
				" physical path to the device."));
			break;


		case L_INVLD_PATH_NO_SLASH_FND:
			(void) sprintf(err_msg,
				MSGSTR(10012,
				"Error in the device physical path."));
			break;

		case L_INVLD_PATH_NO_ATSIGN_FND:
			(void) sprintf(err_msg,
				MSGSTR(10013,
				" Error in the device physical path:"
				" no @ found."));
			break;

		case L_INVALID_SLOT:
			(void) sprintf(err_msg,
				MSGSTR(10014,
				" Error: Invalid path format."
				" Invalid slot."));
			break;

		case L_INVALID_LED_RQST:
			(void) sprintf(err_msg,
				MSGSTR(10015,
				" Error: Invalid LED request."));
			break;

		case L_INVALID_PATH_FORMAT:
			(void) sprintf(err_msg,
				MSGSTR(10016,
				" Error: Invalid path format."));
			break;

		case L_OPEN_PATH_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10017,
				" Error opening the path."));
			break;

		case L_INVALID_PASSWORD_LEN:
			(void) sprintf(err_msg,
				MSGSTR(10018,
				"Error: Invalid password length."));
			break;

		case L_INVLD_PHYS_PATH_TO_DISK:
			(void) sprintf(err_msg,
				MSGSTR(10019,
				" Error: Physical path not of a disk."));
			break;

		case L_INVLD_ID_FOUND:
			(void) sprintf(err_msg,
				MSGSTR(10020,
				" Error in the device physical path:"
				" Invalid ID found in the path."));
			break;

		case L_INVLD_WWN_FORMAT:
			(void) sprintf(err_msg,
				MSGSTR(10021,
				" Error in the device physical path:"
				" Invalid wwn format."));

			break;

		case L_NO_VALID_PATH:
			(void) sprintf(err_msg,
				MSGSTR(10022,
				" Error: Could not find valid path to"
				" the device."));
			break;

		case L_NO_WWN_FOUND_IN_PATH:
			(void) sprintf(err_msg,
				MSGSTR(10023,
				" Error in the device physical path:"
				" No WWN found."));

			break;

		case L_NO_NODE_WWN_IN_WWNLIST:
			(void) sprintf(err_msg,
				MSGSTR(10024,
				" Error: Device's Node WWN is not"
				" found in the WWN list.\n"));
			break;

		case L_NO_NODE_WWN_IN_BOXLIST:
			(void) sprintf(err_msg,
				MSGSTR(10025,
				" Error: Device's Node WWN is not"
				" found in the Box list.\n"));
			break;

		case L_NULL_WWN_LIST:
			(void) sprintf(err_msg,
				MSGSTR(10026,
				" Error: Null WWN list found."));
			break;

		case L_NO_LOOP_ADDRS_FOUND:
			(void) sprintf(err_msg,
				MSGSTR(10027,
				" Error: Could not find the loop address for "
				" the device at physical path."));

			break;

		case L_INVLD_PORT_IN_PATH:
			(void) sprintf(err_msg,
				MSGSTR(10028,
				"Error in the device physical path:"
				" Invalid port number found."
				" (Should be 0 or 1)."));

			break;

		case L_INVALID_LOOP_MAP:
			(void) sprintf(err_msg,
				MSGSTR(10029,
				"Error: Invalid loop map found."));
			break;

		case L_SFIOCGMAP_IOCTL_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10030,
				" Error: SFIOCGMAP ioctl failed."
				" Cannot read loop map."));
			break;

		case L_FCIO_GETMAP_IOCTL_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10031,
				" Error: FCIO_GETMAP ioctl failed."
				" Cannot read loop map."));
			break;

		case L_FCIO_LINKSTATUS_FAILED:
			(void) sprintf(err_msg,
				MSGSTR(10032,
				" Error: FCIO_LINKSTATUS ioctl failed."
				" Cannot read loop map."));
			break;

		case L_FCIOGETMAP_INVLD_LEN:
			(void) sprintf(err_msg,
				MSGSTR(10033,
				" Error: FCIO_GETMAP ioctl returned"
				" an invalid parameter:"
				" # entries to large."));
			break;

		case L_FCIO_FORCE_LIP_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10034,
				" Error: FCIO_FORCE_LIP ioctl failed."));
			break;

		case L_DWNLD_CHKSUM_FAILED:
			(void) sprintf(err_msg,
				MSGSTR(10035,
				"Error: Download file checksum failed."));

			break;

		case L_DWNLD_READ_HEADER_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10036,
				" Error: Reading download file exec"
				" header failed."));
			break;

		case L_DWNLD_READ_INCORRECT_BYTES:
			(void) sprintf(err_msg,
				MSGSTR(10037,
				" Error: Incorrect number of bytes read."));
			break;

		case L_DWNLD_INVALID_TEXT_SIZE:
			(void) sprintf(err_msg,
				MSGSTR(10038,
				" Error: Reading text segment: "
				" Found wrong size."));
			break;

		case L_DWNLD_READ_ERROR:
			(void) sprintf(err_msg,
				MSGSTR(10039,
				" Error: Failed to read download file."));
			break;

		case L_DWNLD_BAD_FRMWARE:
			(void) sprintf(err_msg,
				MSGSTR(10040,
				" Error: Bad Firmware MAGIC."));
			break;

		case L_DWNLD_TIMED_OUT:
			(void) sprintf(err_msg,
				MSGSTR(10041,
				" Error: Timed out in 5 minutes"
				" waiting for the"
				" IB to become available."));
			break;

		case L_REC_DIAG_PG1:
			(void) sprintf(err_msg,
				MSGSTR(10042,
				" Error parsing the Receive"
				" diagnostic page."));
			break;

		case L_TRANSFER_LEN:
			(void) sprintf(err_msg,
				MSGSTR(10043, "  "));
			break;

		case L_MALLOC_FAILED:
			(void) sprintf(err_msg,
				MSGSTR(10,
				" Error: Unable to allocate memory."));
			break;

		case L_LOCALTIME_ERROR:
			(void) sprintf(err_msg,
				MSGSTR(10044,
				" Error: Could not convert time"
				" to broken-down time: Hrs/Mins/Secs."));
			break;

		case L_SELECT_ERROR:
			(void) sprintf(err_msg,
				MSGSTR(10045,
				" select() error during retry:"
				" Could not wait for"
				" specified time."));
			break;

		case L_NO_DISK_DEV_FOUND:
			(void) sprintf(err_msg,
				MSGSTR(10046,
				" Error: No disk devices found"
				" in the /dev/rdsk"
				" directory."));
			break;

		case L_NO_TAPE_DEV_FOUND:
			(void) sprintf(err_msg,
				MSGSTR(10047,
				" Error: No tape devices found"
				" in the /dev/rmt"
				" directory."));
			break;

		case L_LSTAT_ERROR:
			(void) sprintf(err_msg,
				MSGSTR(10048,
				" lstat() error: Cannot obtain status"
				" for the device."));
			break;

		case L_SYMLINK_ERROR:
			(void) sprintf(err_msg,
				MSGSTR(10049,
				" Error: Could not read the symbolic link."));
			break;

		case L_UNAME_FAILED:
			(void) sprintf(err_msg,
				MSGSTR(10050,
				" uname() error: Could not obtain the"
				" architeture of the host machine."));
			break;

		case L_DRVCONFIG_ERROR:
			(void) sprintf(err_msg,
				MSGSTR(10051,
				" Error: Could not run drvconfig."));
			break;

		case L_DISKS_ERROR:
			(void) sprintf(err_msg,
				MSGSTR(10052,
				" Error: Could not run disks."));
			break;

		case L_DEVLINKS_ERROR:
			(void) sprintf(err_msg,
				MSGSTR(10053,
				" Error: Could not run devlinks."));
			break;

		case L_READ_DEV_DIR_ERROR:
			(void) sprintf(err_msg,
				MSGSTR(10054,
				" Error: Could not read /dev/rdsk"
				" directory."));
			break;

		case L_OPEN_ES_DIR_FAILED:
			(void) sprintf(err_msg,
				MSGSTR(10055,
				" Error: Could not open /dev/es"
				" directory."));
			break;

		case L_LSTAT_ES_DIR_ERROR:
			(void) sprintf(err_msg,
				MSGSTR(10056,
				" lstat() error: Could not get status"
				" for /dev/es directory."));
			break;

		case L_DEV_BUSY:
			(void) sprintf(err_msg,
				MSGSTR(10057,
				" Error: Could not offline the device\n"
				" May be Busy."));
			break;

		case L_EXCL_OPEN_FAILED:
			(void) sprintf(err_msg,
				MSGSTR(10058,
				" Error: Could not open device in"
				" exclusive mode."
				"  May already be open."));
			break;

		case L_DEVICE_RESERVED:
			(void) sprintf(err_msg,
				MSGSTR(10059,
				" Error: Disk is reserved."));
			break;

		case L_DISKS_RESERVED:
			(void) sprintf(err_msg,
				MSGSTR(10060,
				" Error: One or more disks in"
				" SENA are reserved."));
			break;

		case L_SLOT_EMPTY:
			(void) sprintf(err_msg,
				MSGSTR(10061,
				" Error: Slot is empty."));
			break;

		case L_ACQUIRE_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10062,
				" Error: Could not acquire"
				" the device."));
			break;

		case L_POWER_OFF_FAIL_BUSY:
			(void) sprintf(err_msg,
				MSGSTR(10063,
				" Error: Could not power off the device.\n"
				" May be Busy."));
			break;

		case L_ENCL_NAME_CHANGE_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10064,
				" Error: The Enclosure name change failed."));
			break;

		case L_DUPLICATE_ENCLOSURES:
			(void) sprintf(err_msg,
				MSGSTR(10065,
				" Error: There are two or more enclosures"
				" with the same name."
				" Please use a logical or physical"
				" pathname."));
			break;

		case L_INVALID_NUM_DISKS_ENCL:
			(void) sprintf(err_msg,
				MSGSTR(10066,
				" Error: The number of disks in the"
				" front & rear of the enclosure are"
				" different."
				" This is not a supported configuration."));
			break;

		case L_ENCL_INVALID_PATH:
			(void) sprintf(err_msg,
				MSGSTR(10067,
				" Error: Invalid path."
				" Device is not a SENA subsystem."));
			break;

		case L_NO_ENCL_LIST_FOUND:
			(void) sprintf(err_msg,
				MSGSTR(10068,
				" Error: Cannot get the Box list."));
			break;

		case L_IB_NO_ELEM_FOUND:
			(void) sprintf(err_msg,
				MSGSTR(10069,
				" Error: No elements returned from"
				" enclosure (IB)."));
			break;

		case L_GET_STATUS_FAILED:
			(void) sprintf(err_msg,
				MSGSTR(10070,
				" Error: Get status failed."));
			break;

		case L_RD_PG_MIN_BUFF:
			(void) sprintf(err_msg,
				MSGSTR(10071,
				" Error: Reading page from IB.\n"
				" Buffer size too small."));
			break;

		case L_RD_PG_INVLD_CODE:
			(void) sprintf(err_msg,
				MSGSTR(10072,
				" Error: Reading page from IB\n"
				" Invalid page code or page len found."));
			break;

		case L_BP_BUSY_RESERVED:
			(void) sprintf(err_msg,
				MSGSTR(10073,
				" Error: There is a busy or reserved disk"
				" attached to this backplane.\n"
				" You must close the disk,\n"
				" or release the disk,\n"
				" or resubmit the command using"
				" the Force option."));
			break;

		case L_BP_BUSY:
			(void) sprintf(err_msg,
				MSGSTR(10074,
				" Error: There is a busy disk"
				" attached to this backplane.\n"
				" You must close the disk,\n"
				" or resubmit the command using"
				" the Force option."));
			break;

		case L_BP_RESERVED:
			(void) sprintf(err_msg,
				MSGSTR(10075,
				" Error: There is a reserved disk"
				" attached to this backplane.\n"
				" You must release the disk,\n"
				" or resubmit the subcommand using"
				" the Force option."));
			break;

		case L_NO_BP_ELEM_FOUND:
			(void) sprintf(err_msg,
				MSGSTR(10076,
				" Error: No Back plane elements found"
				" in the enclosure."));
			break;

		case L_SSA_CONFLICT:
			(void) sprintf(err_msg,
				MSGSTR(10077,
				" There is a conflict between the "
				"enclosure name and an SSA name of "
				"same form, cN.\n"
				" Please use a logical or physical "
				"pathname."));
			break;

		case L_WARNING:
			(void) sprintf(err_msg,
				MSGSTR(10078, " Warning:"));

			break;

		case L_TH_JOIN:
			(void) sprintf(err_msg,
				MSGSTR(10079,
				" Error: Thread join failed."));
			break;

		case L_FCIO_RESET_LINK_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10082,
				" Error: FCIO_RESET_LINK ioctl failed.\n"
				" Could not reset the loop."));
			break;

		case L_FCIO_GET_FCODE_REV_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10083,
				" Error: FCIO_GET_FCODE_REV ioctl failed.\n"
				" Could not get the fcode version."));
			break;

		case L_FCIO_GET_FW_REV_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10084,
				" Error: FCIO_GET_FW_REV ioctl failed.\n"
				" Could not get the firmware revision."));
			break;

		case L_NO_DEVICES_FOUND:
			(void) sprintf(err_msg,
				MSGSTR(10085,
				" No FC devices found."));
			break;

		case L_INVALID_DEVICE_COUNT:
			(void) sprintf(err_msg,
				MSGSTR(10086,
				" Error: FCIO_GET_DEV_LIST ioctl returned"
				" an invalid device count."));
			break;

		case L_FCIO_GET_NUM_DEVS_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10087,
				" Error: FCIO_GET_NUM_DEVS ioctl failed.\n"
				" Could not get the number of devices."));
			break;

		case L_FCIO_GET_DEV_LIST_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10088,
				" Error: FCIO_GET_DEV_LIST ioctl failed.\n"
				" Could not get the device list."));
			break;

		case L_FCIO_GET_LINK_STATUS_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10089,
				" Error: FCIO_GET_LINK_STATUS ioctl failed.\n"
				" Could not get the link status."));
			break;

		case L_PORT_OFFLINE_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10090,
				" Error: ioctl to offline the port failed."));
			break;

		case L_PORT_OFFLINE_UNSUPPORTED:
			(void) sprintf(err_msg,
				MSGSTR(10091,
				" Error: The driver does not support ioctl to"
				" disable the FCA port."));
			break;

		case L_PORT_ONLINE_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10092,
				" Error: ioctl to online the port failed."));
			break;

		case L_PORT_ONLINE_UNSUPPORTED:
			(void) sprintf(err_msg,
				MSGSTR(10093,
				" Error: The driver does not support ioctl to"
				" enable the FCA port."));
			break;

		case L_FCP_TGT_INQUIRY_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10094,
				" Error: FCP_TGT_INQUIRY ioctl failed.\n"
				" Could not get the target inquiry data"
				" from FCP."));
			break;

		case L_FSTAT_ERROR:
			(void) sprintf(err_msg,
				MSGSTR(10095,
				" fstat() error: Cannot obtain status"
				" for the device."));
			break;

		case L_FCIO_GET_HOST_PARAMS_FAIL:
			(void) sprintf(err_msg,
				MSGSTR(10097,
				" Error: FCIO_GET_HOST_PARAMS ioctl failed.\n"
				" Could not get the host parameters."));
			break;

		default:

			if (((L_SCSI_ERROR ^ errornum) == STATUS_GOOD) ||
			((L_SCSI_ERROR ^ errornum) == STATUS_BUSY) ||
			((L_SCSI_ERROR ^ errornum) == STATUS_CHECK) ||
			((L_SCSI_ERROR ^ errornum) == STATUS_MET) ||
			((L_SCSI_ERROR ^ errornum) == STATUS_INTERMEDIATE) ||
		((L_SCSI_ERROR ^ errornum) == STATUS_INTERMEDIATE_MET) ||
		((L_SCSI_ERROR ^ errornum) == STATUS_RESERVATION_CONFLICT) ||
			((L_SCSI_ERROR ^ errornum) == STATUS_TERMINATED) ||
			((L_SCSI_ERROR ^ errornum) == STATUS_QFULL)) {
				(void) sprintf(err_msg,
					MSGSTR(10080,
					" SCSI Error - Sense Byte:(0x%x) %s \n"
					" Error: Retry failed."),
				(L_SCSI_ERROR ^ errornum) & STATUS_MASK,
			decode_sense_byte((uchar_t)L_SCSI_ERROR ^ errornum));
			} else {
				(void) sprintf(err_msg,
					MSGSTR(10081,
					" Error: could not decode the"
					" error message.\n"
					" The given error message is not"
					" defined in the library.\n"
					" Message number: %d.\n"), errornum);
			}

	} /* end of switch */

	errStrg = g_alloc_string(err_msg);

	return (errStrg);
}
