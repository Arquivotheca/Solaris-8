/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mon.c	1.30	99/08/18 SMI"

/*LINTLIBRARY*/

/*
 * I18N message number ranges
 *  This file: 9000 - 9499
 *  Shared common messages: 1 - 1999
 */

/*
 *	This module is part of the photon library
 */
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
#include	<assert.h>
#include	<sys/scsi/scsi.h>
#include	<dirent.h>		/* for DIR */
#include	<sys/vtoc.h>
#include	<sys/dkio.h>
#include	<nl_types.h>
#include	<strings.h>
#include	<sys/ddi.h>		/* for max */
#include	<l_common.h>
#include	<stgcom.h>
#include	<l_error.h>
#include	<rom.h>
#include	<exec.h>
#include	<a_state.h>
#include	<a5k.h>


/*	Defines 	*/
#define	PLNDEF		"SUNW,pln"	/* check if box name starts with 'c' */
#define	DOWNLOAD_RETRIES	60*5	/* 5 minutes */
#define	IBFIRMWARE_FILE		"/usr/lib/locale/C/LC_MESSAGES/ibfirmware"

/*	Global variables	*/
extern	uchar_t		g_switch_to_alpa[];
extern	uchar_t		g_sf_alpa_to_switch[];

/*	Forward declarations	*/
static int
pwr_up_down(char *, L_state *, int, int, int, int);

/*
 * l_get_mode_pg() - Read all mode pages.
 *
 * RETURNS:
 *	0        O.K.
 *	non-zero otherwise
 *
 * INPUTS:
 *	path     pointer to device path
 *	pg_buf   ptr to mode pages
 *
 */
/*ARGSUSED*/
int
l_get_mode_pg(char *path, uchar_t **pg_buf, int verbose)
{
Mode_header_10	*mode_header_ptr;
int		status, size, fd;

	P_DPRINTF("  l_get_mode_pg: Reading Mode Sense pages.\n");

	/* open controller */
	if ((fd = g_object_open(path, O_NDELAY | O_RDWR)) == -1)
		return (L_OPEN_PATH_FAIL);

	/*
	 * Read the first part of the page to get the page size
	 */
	size = 20;
	if ((*pg_buf = (uchar_t *)g_zalloc(size)) == NULL) {
	    (void) close(fd);
	    return (L_MALLOC_FAILED);
	}
	/* read page */
	if (status = g_scsi_mode_sense_cmd(fd, *pg_buf, size,
	    0, MODEPAGE_ALLPAGES)) {
	    (void) close(fd);
	    (void) g_destroy_data((char *)*pg_buf);
	    return (status);
	}
	/* Now get the size for all pages */
	mode_header_ptr = (struct mode_header_10_struct *)(int)*pg_buf;
	size = mode_header_ptr->length + sizeof (mode_header_ptr->length);
	(void) g_destroy_data((char *)*pg_buf);
	if ((*pg_buf = (uchar_t *)g_zalloc(size)) == NULL) {
	    (void) close(fd);
	    return (L_MALLOC_FAILED);
	}
	/* read all pages */
	if (status = g_scsi_mode_sense_cmd(fd, *pg_buf, size,
					0, MODEPAGE_ALLPAGES)) {
	    (void) close(fd);
	    (void) g_destroy_data((char *)*pg_buf);
	    return (status);
	}
	(void) close(fd);
	return (0);
}



/*
 * Format QLA21xx status
 *
 * INPUTS: message buffer
 *         Count
 *         status
 *
 * OUTPUT: Message of this format in message buffer
 *         "status type:            0xstatus        count"
 */
int
l_format_ifp_status_msg(char *status_msg_buf, int count, int status)
{
	switch (status) {
	case IFP_CMD_CMPLT:
		(void) sprintf(status_msg_buf,
			MSGSTR(9000, "O.K.                          0x%-2x"
			"            %d"), status, count);
		break;
	case IFP_CMD_INCOMPLETE:
		(void) sprintf(status_msg_buf,
			MSGSTR(9001, "Cmd incomplete                0x%-2x"
			"            %d"), status, count);
		break;
	case IFP_CMD_DMA_DERR:
		(void) sprintf(status_msg_buf,
			MSGSTR(9002, "DMA direction error           0x%-2x"
			"            %d"), status, count);
		break;
	case IFP_CMD_TRAN_ERR:
		(void) sprintf(status_msg_buf,
			MSGSTR(9003, "Unspecified transport error   0x%-2x"
			"            %d"), status, count);
		break;
	case IFP_CMD_RESET:
		(void) sprintf(status_msg_buf,
			MSGSTR(9004, "Reset aborted transport       0x%-2x"
			"            %d"), status, count);
		break;
	case IFP_CMD_ABORTED:
		(void) sprintf(status_msg_buf,
			MSGSTR(9005, "Cmd aborted                   0x%-2x"
			"            %d"), status, count);
		break;
	case IFP_CMD_TIMEOUT:
		(void) sprintf(status_msg_buf,
			MSGSTR(9006, "Cmd Timeout                   0x%-2x"
			"            %d"), status, count);
		break;
	case IFP_CMD_DATA_OVR:
		(void) sprintf(status_msg_buf,
			MSGSTR(9007, "Data Overrun                  0x%-2x"
			"            %d"), status, count);
		break;
	case IFP_CMD_ABORT_REJECTED:
		(void) sprintf(status_msg_buf,
			MSGSTR(9008, "Target rejected abort msg     0x%-2x"
			"            %d"), status, count);
		break;
	case IFP_CMD_RESET_REJECTED:
		(void) sprintf(status_msg_buf,
			MSGSTR(9009, "Target rejected reset msg     0x%-2x"
			"            %d"), status, count);
		break;
	case IFP_CMD_DATA_UNDER:
		(void) sprintf(status_msg_buf,
			MSGSTR(9010, "Data underrun                 0x%-2x"
			"            %d"), status, count);
		break;
	case IFP_CMD_QUEUE_FULL:
		(void) sprintf(status_msg_buf,
			MSGSTR(9011, "Queue full SCSI status        0x%-2x"
			"            %d"), status, count);
		break;
	case IFP_CMD_PORT_UNAVAIL:
		(void) sprintf(status_msg_buf,
			MSGSTR(9012, "Port unavailable              0x%-2x"
			"            %d"), status, count);
		break;
	case IFP_CMD_PORT_LOGGED_OUT:
		(void) sprintf(status_msg_buf,
			MSGSTR(9013, "Port loged out                0x%-2x"
			"            %d"), status, count);
		break;
	case IFP_CMD_PORT_CONFIG_CHANGED:
		/* Not enough packets for given request */
		(void) sprintf(status_msg_buf,
			MSGSTR(9014, "Port name changed             0x%-2x"
			"            %d"), status, count);
		break;
	default:
		(void) sprintf(status_msg_buf,
			"%s                0x%-2x"
			"            %d", MSGSTR(4, "Unknown status"),
			status, count);

	} /* End of switch() */

	return (0);

}



/*
 * Format Fibre Channel status
 *
 * INPUTS: message buffer
 *         Count
 *         status
 *
 * OUTPUT: Message of this format in message buffer
 *         "status type:            0xstatus        count"
 */
int
l_format_fc_status_msg(char *status_msg_buf, int count, int status)
{
	switch (status) {
	case FCAL_STATUS_OK:
		(void) sprintf(status_msg_buf,
			MSGSTR(9015, "O.K.                          0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_P_RJT:
		(void) sprintf(status_msg_buf,
			MSGSTR(9016, "P_RJT (Frame Rejected)        0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_F_RJT:
		(void) sprintf(status_msg_buf,
			MSGSTR(9017, "F_RJT (Frame Rejected)        0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_P_BSY:
		(void) sprintf(status_msg_buf,
			MSGSTR(9018, "P_BSY (Port Busy)             0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_F_BSY:
		(void) sprintf(status_msg_buf,
			MSGSTR(9019, "F_BSY (Port Busy)             0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_OLDPORT_ONLINE:
		/* Should not happen. */
		(void) sprintf(status_msg_buf,
			MSGSTR(9020, "Old port Online               0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_ERR_OFFLINE:
		(void) sprintf(status_msg_buf,
			MSGSTR(9021, "Link Offline                  0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_TIMEOUT:
		/* Should not happen. */
		(void) sprintf(status_msg_buf,
			MSGSTR(9022, "Sequence Timeout              0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_ERR_OVERRUN:
		(void) sprintf(status_msg_buf,
			MSGSTR(9023, "Sequence Payload Overrun      0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_LOOP_ONLINE:
		(void) sprintf(status_msg_buf,
			MSGSTR(9060, "Loop Online                   0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_OLD_PORT:
		(void) sprintf(status_msg_buf,
			MSGSTR(9061, "Old port                      0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_AL_PORT:
		(void) sprintf(status_msg_buf,
			MSGSTR(9062, "AL port                       0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_UNKNOWN_CQ_TYPE:
		(void) sprintf(status_msg_buf,
			MSGSTR(9024, "Unknown request type          0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_BAD_SEG_CNT:
		(void) sprintf(status_msg_buf,
			MSGSTR(9025, "Bad segment count             0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_MAX_XCHG_EXCEEDED:
		(void) sprintf(status_msg_buf,
			MSGSTR(9026, "Maximum exchanges exceeded    0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_BAD_XID:
		(void) sprintf(status_msg_buf,
			MSGSTR(9027, "Bad exchange identifier       0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_XCHG_BUSY:
		(void) sprintf(status_msg_buf,
			MSGSTR(9028, "Duplicate exchange request    0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_BAD_POOL_ID:
		(void) sprintf(status_msg_buf,
			MSGSTR(9029, "Bad memory pool ID            0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_INSUFFICIENT_CQES:
		/* Not enough packets for given request */
		(void) sprintf(status_msg_buf,
			MSGSTR(9030, "Invalid # of segments for req 0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_ALLOC_FAIL:
		(void) sprintf(status_msg_buf,
			MSGSTR(9031, "Resource allocation failure   0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_BAD_SID:
		(void) sprintf(status_msg_buf,
			MSGSTR(9032, "Bad Source Identifier(S_ID)   0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_NO_SEQ_INIT:
		(void) sprintf(status_msg_buf,
			MSGSTR(9033, "No sequence initiative        0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_BAD_DID:
		(void) sprintf(status_msg_buf,
			MSGSTR(9034, "Bad Destination ID(D_ID)      0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_ABORTED:
		(void) sprintf(status_msg_buf,
			MSGSTR(9035, "Received BA_ACC from abort    0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_ABORT_FAILED:
		(void) sprintf(status_msg_buf,
			MSGSTR(9036, "Received BA_RJT from abort    0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_DIAG_BUSY:
		(void) sprintf(status_msg_buf,
			MSGSTR(9037, "Diagnostics currently busy    0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_DIAG_INVALID:
		(void) sprintf(status_msg_buf,
			MSGSTR(9038, "Diagnostics illegal request   0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_INCOMPLETE_DMA_ERR:
		(void) sprintf(status_msg_buf,
			MSGSTR(9039, "SBus DMA did not complete     0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_CRC_ERR:
		(void) sprintf(status_msg_buf,
			MSGSTR(9040, "CRC error detected            0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_OPEN_FAIL:
		(void) sprintf(status_msg_buf,
			MSGSTR(9063, "Open failure                  0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_ERROR:
		(void) sprintf(status_msg_buf,
			MSGSTR(9041, "Invalid status error          0x%-2x"
			"            %d"), status, count);
		break;
	case FCAL_STATUS_ONLINE_TIMEOUT:
		(void) sprintf(status_msg_buf,
			MSGSTR(9042, "Timed out before ONLINE       0x%-2x"
			"            %d"), status, count);
		break;
	default:
		(void) sprintf(status_msg_buf,
			"%s                0x%-2x"
			"            %d", MSGSTR(4, "Unknown status"),
			status, count);

	} /* End of switch() */

	return (0);

}



/*
 * Get the indexes to the disk device elements in page 2,
 * based on the locations found in page 1.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
l_get_disk_element_index(struct l_state_struct *l_state, int *front_index,
						int *rear_index)
{
int	index = 0, front_flag = 0, local_front = 0, local_rear = 0;
int	i, rear_flag = 0;

	*front_index = *rear_index = 0;
	/* Get the indexes to the disk device elements */
	for (i = 0; i < (int)l_state->ib_tbl.config.enc_num_elem; i++) {
		if (l_state->ib_tbl.config.type_hdr[i].type == ELM_TYP_DD) {
			if (front_flag) {
				local_rear = index;
				rear_flag = 1;
				break;
			} else {
				local_front = index;
				front_flag = 1;
			}
		}
		index += l_state->ib_tbl.config.type_hdr[i].num;
		index++;		/* for global element */
	}

	D_DPRINTF("  l_get_disk_element_index:"
		" Index to front disk elements 0x%x\n"
		"  l_get_disk_element_index:"
		" Index to rear disk elements 0x%x\n",
		local_front, local_rear);

	if (!front_flag || !rear_flag) {
		return (L_RD_NO_DISK_ELEM);
	}
	*front_index = local_front;
	*rear_index = local_rear;
	return (0);
}



/*
 * l_led() manage the device led's
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
l_led(struct path_struct *path_struct, int led_action,
	struct device_element *status,
	int verbose)
{
sf_al_map_t		map;
char			ses_path[MAXPATHLEN];
uchar_t			*page_buf;
int 			err, write, fd, front_index, rear_index, offset;
unsigned short		page_len;
struct	device_element 	*elem;
L_state			*l_state;

	/*
	 * Need to get a valid location, front/rear & slot.
	 *
	 * The path_struct will return a valid slot
	 * and the IB path or a disk path.
	 */

	if (!path_struct->ib_path_flag) {
		if ((err = g_get_dev_map(path_struct->p_physical_path,
							&map, verbose)) != 0)
			return (err);
		if ((err = l_get_ses_path(path_struct->p_physical_path,
						ses_path, &map, verbose)) != 0)
			return (err);
	} else {
		(void) strcpy(ses_path, path_struct->p_physical_path);
	}

	if ((l_state = (L_state *)calloc(1, sizeof (L_state))) == NULL) {
		return (L_MALLOC_FAILED);
	}

	if (!path_struct->slot_valid) {
		if ((err = g_get_dev_map(path_struct->p_physical_path,
							&map, verbose)) != 0) {
			(void) l_free_lstate(&l_state);
			return (err);
		}
		if ((err = l_get_ses_path(path_struct->p_physical_path,
			ses_path, &map, verbose)) != 0) {
			(void) l_free_lstate(&l_state);
			return (err);
		}
		if ((err = l_get_status(ses_path, l_state, verbose)) != 0) {
			(void) l_free_lstate(&l_state);
			return (err);
		}

		/* We are passing the disks path */
		if (err = l_get_slot(path_struct, l_state, verbose)) {
			(void) l_free_lstate(&l_state);
			return (err);
		}
	}

	if ((page_buf = (uchar_t *)calloc(1,
				MAX_REC_DIAG_LENGTH)) == NULL) {
		(void) l_free_lstate(&l_state);
		return (L_MALLOC_FAILED);
	}

	if ((fd = g_object_open(ses_path, O_NDELAY | O_RDWR)) == -1) {
		(void) l_free_lstate(&l_state);
		(void) g_destroy_data(page_buf);
		return (L_OPEN_PATH_FAIL);
	}

	if (err = l_get_envsen_page(fd, page_buf, MAX_REC_DIAG_LENGTH,
						L_PAGE_2, verbose)) {
		(void) l_free_lstate(&l_state);
		(void) close(fd);
		(void) g_destroy_data(page_buf);
		return (err);
	}

	page_len = (page_buf[2] << 8 | page_buf[3]) + HEADER_LEN;

	/* Get index to the disk we are interested in */
	if (err = l_get_status(ses_path, l_state, verbose)) {
		(void) l_free_lstate(&l_state);
		(void) close(fd);
		(void) g_destroy_data(page_buf);
		return (err);
	}
	/* Double check slot. */
	if (path_struct->slot >= l_state->total_num_drv/2) {
		(void) l_free_lstate(&l_state);
		return (L_INVALID_SLOT);
	}

	if (err = l_get_disk_element_index(l_state, &front_index,
	    &rear_index)) {
		(void) l_free_lstate(&l_state);
		return (err);
	}

	/* Skip global element */
	front_index++;
	rear_index++;

	if (path_struct->f_flag) {
		offset = (8 + (front_index + path_struct->slot)*4);
	} else {
		offset = (8 + (rear_index + path_struct->slot)*4);
	}

	elem = (struct device_element *)((int)page_buf + offset);
	/*
	 * now do requested action.
	 */
	bcopy((const void *)elem, (void *)status,
		sizeof (struct device_element));	/* save status */
	bzero(elem, sizeof (struct device_element));
	elem->select = 1;
	elem->dev_off = status->dev_off;
	elem->en_bypass_a = status->en_bypass_a;
	elem->en_bypass_b = status->en_bypass_b;
	write = 1;

	switch (led_action) {
	case	L_LED_STATUS:
		write = 0;
		break;
	case	L_LED_RQST_IDENTIFY:
		elem->ident = 1;
		if (verbose) {
			(void) fprintf(stdout,
			MSGSTR(9043, "  Blinking LED for slot %d in enclosure"
			" %s\n"), path_struct->slot,
			l_state->ib_tbl.enclosure_name);
		}
		break;
	case	L_LED_OFF:
		if (verbose) {
			(void) fprintf(stdout,
			MSGSTR(9044,
			"  Turning off LED for slot %d in enclosure"
			" %s\n"), path_struct->slot,
			l_state->ib_tbl.enclosure_name);
		}
		break;
	default:
		(void) l_free_lstate(&l_state);
		return (L_INVALID_LED_RQST);
	} /* End of switch */

	if (write) {
		if (getenv("_LUX_D_DEBUG") != NULL) {
			g_dump("  l_led: Updating led state: "
			"Device Status Element ",
			(uchar_t *)elem, sizeof (struct device_element),
			HEX_ONLY);
		}
		if (err = g_scsi_send_diag_cmd(fd,
			(uchar_t *)page_buf, page_len)) {
			(void) close(fd);
			(void) g_destroy_data(page_buf);
			(void) l_free_lstate(&l_state);
			return (err);
		}

		bzero(page_buf, MAX_REC_DIAG_LENGTH);
		if (err = l_get_envsen_page(fd, page_buf, MAX_REC_DIAG_LENGTH,
					L_PAGE_2, verbose)) {
			(void) g_destroy_data(page_buf);
			(void) close(fd);
			(void) l_free_lstate(&l_state);
			return (err);
		}
		elem = (struct device_element *)((int)page_buf + offset);
		bcopy((const void *)elem, (void *)status,
			sizeof (struct device_element));
	}
	if (getenv("_LUX_D_DEBUG") != NULL) {
		g_dump("  l_led: Device Status Element ",
		(uchar_t *)status, sizeof (struct device_element),
		HEX_ONLY);
	}

	(void) l_free_lstate(&l_state);
	(void) close(fd);
	(void) g_destroy_data(page_buf);
	return (0);
}


/*
 * frees the previously alloced l_state
 * structure.
 *
 * RETURNS:
 *	0	O.K.
 *	non-zero otherwise
 */
int
l_free_lstate(L_state **l_state)
{
int	i;

	if (*l_state == NULL)
		return (0);

	for (i = 0; i < (int)(*l_state)->total_num_drv/2; i++) {
	if ((*l_state)->drv_front[i].g_disk_state.multipath_list != NULL)
		(void) g_free_multipath(
		(*l_state)->drv_front[i].g_disk_state.multipath_list);
	if ((*l_state)->drv_rear[i].g_disk_state.multipath_list != NULL)
		(void) g_free_multipath(
		(*l_state)->drv_rear[i].g_disk_state.multipath_list);
	}
	(void) g_destroy_data (*l_state);
	l_state = NULL;

	return (0);
}



/*
 * Set the state of an individual disk
 * in the Photon enclosure the powered
 * up/down mode. The path must point to
 * a disk or the ib_path_flag must be set.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
l_dev_pwr_up_down(char *path_phys, struct path_struct *path_struct,
		int power_off_flag, int verbose, int force_flag)
/*ARGSUSED*/
{
sf_al_map_t		map;
char			ses_path[MAXPATHLEN], dev_path[MAXPATHLEN];
int			slot, err = 0;
L_state			*l_state = NULL;
struct l_disk_state_struct	*drive;
struct dlist		*dl, *dl1;
devctl_hdl_t		devhdl;
WWN_list		*wwn_list = NULL;

	dl = (struct dlist *)NULL;

	if (err = g_get_dev_map(path_struct->p_physical_path,
					&map, verbose))
		return (err);

	if (err = l_get_ses_path(path_struct->p_physical_path,
				ses_path, &map, verbose))
		return (err);

	if ((l_state = (L_state *)calloc(1, sizeof (L_state))) == NULL) {
		return (L_MALLOC_FAILED);
	}

	if (err = l_get_status(ses_path, l_state, verbose)) {
		(void) l_free_lstate(&l_state);
		return (err);
	}

	if (!path_struct->slot_valid) {
		/* We are passing the disks path */
		if (err = l_get_slot(path_struct, l_state, verbose)) {
			(void) l_free_lstate(&l_state);
			return (err);
		}
	}

	slot = path_struct->slot;
	(void) strcpy(dev_path, path_struct->p_physical_path);

	/*
	 * Either front or rear drive
	 */
	if (path_struct->f_flag) {
		drive = &l_state->drv_front[slot];
	} else {
		drive = &l_state->drv_rear[slot];
	}

	/*
	 * Check for drive presence always
	 */
	if (drive->ib_status.code == S_NOT_INSTALLED) {
		(void) l_free_lstate(&l_state);
		return (L_SLOT_EMPTY);
	}

	/*
	 * Check disk state
	 * before the power off.
	 *
	 */
	if (power_off_flag && !force_flag) {
		goto pre_pwr_dwn;
	} else {
		goto pwr_up_dwn;
	}

pre_pwr_dwn:

	/*
	 * Check whether disk
	 * is reserved by another
	 * host
	 */
	if ((drive->g_disk_state.d_state_flags[PORT_A] & L_RESERVED) ||
		(drive->g_disk_state.d_state_flags[PORT_B] &
		L_RESERVED)) {
		(void) l_free_lstate(&l_state);
		return (L_DEVICE_RESERVED);
	}


	if ((dl = (struct dlist *)g_zalloc(sizeof (struct dlist))) == NULL) {
		(void) l_free_lstate(&l_state);
		return (L_MALLOC_FAILED);
	}

	/*
	 * NOTE: It is not necessary to get the multipath list here as ------
	 * we alread have it after getting the status earlier.
	 * - REWRITE -
	 */

	/*
	 * Get path to all the FC disk and tape devices.
	 *
	 * I get this now and pass down for performance
	 * reasons.
	 * If for some reason the list can become invalid,
	 * i.e. device being offlined, then the list
	 * must be re-gotten.
	 */
	if (err = g_get_wwn_list(&wwn_list, verbose)) {
		(void) g_destroy_data(dl);
		(void) l_free_lstate(&l_state);
		return (err);   /* Failure */
	}

	dl->dev_path = dev_path;
	if ((err = g_get_multipath(dev_path,
			&(dl->multipath), wwn_list, verbose)) != 0) {
		(void) g_destroy_data(dl);
		(void) g_free_wwn_list(&wwn_list);
		(void) l_free_lstate(&l_state);
		return (err);
	}

	for (dl1 = dl->multipath; dl1 != NULL; dl1 = dl1->next) {

#ifdef	TWO_SIX
		if (devctl_acquire(dl1->dev_path, DC_EXCL, &devhdl) != 0) {
#else
		if ((devhdl = devctl_device_acquire(dl1->dev_path,
						DC_EXCL)) == NULL) {
#endif
			if (errno != EBUSY) {
				ER_DPRINTF("%s could not acquire"
				" the device: %s\n\n",
				strerror(errno), dl1->dev_path);
				continue;
			}
		}
		if (devctl_device_offline(devhdl) != 0) {
			(void) devctl_release(devhdl);
			(void) g_free_multipath(dl->multipath);
			(void) g_destroy_data(dl);
			(void) g_free_wwn_list(&wwn_list);
			(void) l_free_lstate(&l_state);
			return (L_POWER_OFF_FAIL_BUSY);
		}
		(void) devctl_release(devhdl);
	}

pwr_up_dwn:
	err = pwr_up_down(ses_path, l_state, path_struct->f_flag,
			path_struct->slot, power_off_flag, verbose);

	if (dl != NULL) {
		(void) g_free_multipath(dl->multipath);
		(void) g_destroy_data(dl);
	}
	(void) g_free_wwn_list(&wwn_list);
	(void) l_free_lstate(&l_state);
	if (err) {
		return (err);
	}
	return (0);
}



/*
 * l_pho_pwr_up_down() Set the state of the Photon enclosure
 * the powered up/down mode.
 * The path must point to an IB.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
l_pho_pwr_up_down(char *dev_name, char *path_phys, int power_off_flag,
    int verbose, int force_flag)
{
L_state		*l_state = NULL;
int		i, err = 0;
struct dlist	*dl, *dl1;
char		dev_path[MAXPATHLEN];
devctl_hdl_t	devhdl;
WWN_list	*wwn_list = NULL;

	dl = (struct dlist *)NULL;
	if ((l_state = (L_state *)calloc(1, sizeof (L_state))) == NULL) {
		return (L_MALLOC_FAILED);
	}
	if (err = l_get_status(path_phys, l_state, verbose)) {
		(void) l_free_lstate(&l_state);
		return (err);
	}
	if (power_off_flag && !force_flag) {
		goto pre_pwr_dwn;
	} else {
		goto pwr_up_dwn;
	}

pre_pwr_dwn:

	/*
	 * Check if any disk in this enclosure
	 * is reserved by another host before
	 * the power off.
	 */
	for (i = 0; i < l_state->total_num_drv/2; i++) {
		if ((l_state->drv_front[i].g_disk_state.d_state_flags[PORT_A] &
						L_RESERVED) ||
		(l_state->drv_front[i].g_disk_state.d_state_flags[PORT_B] &
						L_RESERVED) ||
		(l_state->drv_rear[i].g_disk_state.d_state_flags[PORT_A] &
						L_RESERVED) ||
		(l_state->drv_rear[i].g_disk_state.d_state_flags[PORT_B] &
						L_RESERVED)) {
				return (L_DISKS_RESERVED);
		}
	}

	/*
	 * Check if any disk in this enclosure
	 * Get path to all the FC disk and tape devices.
	 *
	 * I get this now and pass down for performance
	 * reasons.
	 * If for some reason the list can become invalid,
	 * i.e. device being offlined, then the list
	 * must be re-gotten.
	 */
	if (err = g_get_wwn_list(&wwn_list, verbose)) {
		(void) l_free_lstate(&l_state);
		return (err);   /* Failure */
	}
	for (i = 0; i < l_state->total_num_drv/2; i++) {
		if (*l_state->drv_front[i].g_disk_state.physical_path) {
			(void) memset(dev_path, 0, MAXPATHLEN);
			(void) strcpy(dev_path,
		(char *)&l_state->drv_front[i].g_disk_state.physical_path);

			if ((dl = (struct dlist *)
				g_zalloc(sizeof (struct dlist))) == NULL) {
				(void) g_free_wwn_list(&wwn_list);
				(void) l_free_lstate(&l_state);
				return (L_MALLOC_FAILED);
			}
			dl->dev_path = dev_path;
			if (g_get_multipath(dev_path, &(dl->multipath),
				wwn_list, verbose) != 0) {
				(void) g_destroy_data(dl);
				continue;
			}

			for (dl1 = dl->multipath;
			    dl1 != NULL;
			    dl1 = dl1->next) {

				/* attempt to acquire the device */
#ifdef	TWO_SIX
				if (devctl_acquire(dl1->dev_path, DC_EXCL,
							&devhdl) != 0) {
#else
				if ((devhdl = devctl_device_acquire(
					dl1->dev_path, DC_EXCL)) == NULL) {
#endif
					if (errno != EBUSY) {
						ER_DPRINTF("%s: Could not "
						"acquire the device: %s\n\n",
						strerror(errno),
						dl1->dev_path);
						continue;
					}
				}

				/* attempt to offline the device */
				if (devctl_device_offline(devhdl) != 0) {
					(void) devctl_release(devhdl);
					(void) g_free_multipath(
						dl->multipath);
					(void) g_destroy_data(dl);
					(void) g_free_wwn_list(&wwn_list);
					(void) l_free_lstate(&l_state);
					return (L_POWER_OFF_FAIL_BUSY);
				}

				/* release handle acquired above */
				(void) devctl_release(devhdl);
			}
			(void) g_free_multipath(dl->multipath);
			(void) g_destroy_data(dl);

		}
		if (*l_state->drv_rear[i].g_disk_state.physical_path) {
			(void) memset(dev_path, 0, MAXPATHLEN);
			(void) strcpy(dev_path,
		(char *)&l_state->drv_rear[i].g_disk_state.physical_path);

			if ((dl = (struct dlist *)
				g_zalloc(sizeof (struct dlist))) == NULL) {
				(void) g_free_wwn_list(&wwn_list);
				(void) l_free_lstate(&l_state);
				return (L_MALLOC_FAILED);
			}
			dl->dev_path = dev_path;
			if (g_get_multipath(dev_path, &(dl->multipath),
				wwn_list, verbose) != 0) {
				(void) g_destroy_data(dl);
				continue;
			}


			for (dl1 = dl->multipath;
			    dl1 != NULL;
			    dl1 = dl1->next) {

				/* attempt to acquire the device */
#ifdef	TWO_SIX
				if (devctl_acquire(dl1->dev_path, DC_EXCL,
								&devhdl) != 0) {
#else
				if ((devhdl = devctl_device_acquire(
					dl1->dev_path, DC_EXCL)) == NULL) {
#endif
					if (errno != EBUSY) {
						ER_DPRINTF("%s: Could not "
						"acquire the device: %s\n\n",
						strerror(errno),
						dl1->dev_path);
						continue;
					}
				}
				/* attempt to offline the device */
				if (devctl_device_offline(devhdl) != 0) {
					(void) devctl_release(devhdl);
					(void) g_free_multipath(
							dl->multipath);
					(void) g_destroy_data(dl);
					(void) g_free_wwn_list(&wwn_list);
					(void) l_free_lstate(&l_state);
					return (L_POWER_OFF_FAIL_BUSY);
				}

				/* release handle acquired above */
				(void) devctl_release(devhdl);
			}
			(void) g_free_multipath(dl->multipath);
			(void) g_destroy_data(dl);

		}
	}

pwr_up_dwn:

	(void) g_free_wwn_list(&wwn_list);
	if ((err = pwr_up_down(path_phys, l_state, 0, -1,
		power_off_flag, verbose)) != 0) {
		(void) l_free_lstate(&l_state);
		return (err);
	}
	(void) l_free_lstate(&l_state);
	return (0);
}


/*
 * Set the state of the Photon enclosure or disk
 * powered up/down mode.
 * The path must point to an IB.
 * slot == -1 implies entire enclosure.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
static int
pwr_up_down(char *path_phys, L_state *l_state, int front, int slot,
		int power_off_flag, int verbose)
{
L_inquiry		inq;
int			fd, status, err;
uchar_t			*page_buf;
int 			front_index, rear_index, front_offset, rear_offset;
unsigned short		page_len;
struct	device_element	*front_elem, *rear_elem;

	(void) memset(&inq, 0, sizeof (inq));
	if ((fd = g_object_open(path_phys, O_NDELAY | O_RDONLY)) == -1) {
		return (L_OPEN_PATH_FAIL);
	}
	/* Verify it is a Photon */
	if (status = g_scsi_inquiry_cmd(fd,
		(uchar_t *)&inq, sizeof (struct l_inquiry_struct))) {
		(void) close(fd);
		return (status);
	}
	if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) == 0) &&
		(!(strncmp((char *)inq.inq_vid, "SUN     ",
		sizeof (inq.inq_vid)) &&
		(inq.inq_dtype == DTYPE_ESI)))) {
		(void) close(fd);
		return (L_ENCL_INVALID_PATH);
	}

	/*
	 * To power up/down a Photon we use the Driver Off
	 * bit in the global device control element.
	 */
	if ((page_buf = (uchar_t *)malloc(MAX_REC_DIAG_LENGTH)) == NULL) {
		return (L_MALLOC_FAILED);
	}
	if (err = l_get_envsen_page(fd, page_buf, MAX_REC_DIAG_LENGTH,
				L_PAGE_2, verbose)) {
		(void) close(fd);
		(void) g_destroy_data(page_buf);
		return (err);
	}

	page_len = (page_buf[2] << 8 | page_buf[3]) + HEADER_LEN;

	/* Double check slot as convert_name only does gross check */
	if (slot >= l_state->total_num_drv/2) {
		(void) close(fd);
		(void) g_destroy_data(page_buf);
		return (L_INVALID_SLOT);
	}

	if (err = l_get_disk_element_index(l_state, &front_index,
		&rear_index)) {
		(void) close(fd);
		(void) g_destroy_data(page_buf);
		return (err);
	}
	/* Skip global element */
	front_index++;
	rear_index++;

	front_offset = (8 + (front_index + slot)*4);
	rear_offset = (8 + (rear_index + slot)*4);

	front_elem = (struct device_element *)((int)page_buf + front_offset);
	rear_elem = (struct device_element *)((int)page_buf + rear_offset);

	if (front || slot == -1) {
		/*
		 * now do requested action.
		 */
		bzero(front_elem, sizeof (struct device_element));
		/* Set/reset power off bit */
		front_elem->dev_off = power_off_flag;
		front_elem->select = 1;
	}
	if (!front || slot == -1) {
		/* Now do rear */
		bzero(rear_elem, sizeof (struct device_element));
		/* Set/reset power off bit */
		rear_elem->dev_off = power_off_flag;
		rear_elem->select = 1;
	}

	if (getenv("_LUX_D_DEBUG") != NULL) {
		if (front || slot == -1) {
			g_dump("  pwr_up_down: "
				"Front Device Status Element ",
				(uchar_t *)front_elem,
				sizeof (struct device_element),
				HEX_ONLY);
		}
		if (!front || slot == -1) {
			g_dump("  pwr_up_down: "
				"Rear Device Status Element ",
				(uchar_t *)rear_elem,
				sizeof (struct device_element),
				HEX_ONLY);
		}
	}
	if (err = g_scsi_send_diag_cmd(fd,
		(uchar_t *)page_buf, page_len)) {
		(void) close(fd);
		(void) g_destroy_data(page_buf);
		return (err);
	}
	(void) close(fd);
	(void) g_destroy_data(page_buf);
	return (0);
}

/*
 * Set the password of the FPM by sending the password
 * in page 4 of the Send Diagnostic command.
 *
 * The path must point to an IB.
 *
 * The size of the password string must be <= 8 bytes.
 * The string can also be NULL. This is the way the user
 * chooses to not have a password.
 *
 * I then tell the photon by giving him 4 NULL bytes.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
l_new_password(char *path_phys, char *password)
{
Page4_name	page4;
L_inquiry	inq;
int		fd, status;

	(void) memset(&inq, 0, sizeof (inq));
	(void) memset(&page4, 0, sizeof (page4));

	if ((fd = g_object_open(path_phys, O_NDELAY | O_RDONLY)) == -1) {
		return (L_OPEN_PATH_FAIL);
	}
	/* Verify it is a Photon */
	if (status = g_scsi_inquiry_cmd(fd,
		(uchar_t *)&inq, sizeof (struct l_inquiry_struct))) {
		(void) close(fd);
		return (status);
	}
	if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) == 0) &&
		(!(strncmp((char *)inq.inq_vid, "SUN     ",
		sizeof (inq.inq_vid)) &&
		(inq.inq_dtype == DTYPE_ESI)))) {
		(void) close(fd);
		return (L_ENCL_INVALID_PATH);
	}

	page4.page_code = L_PAGE_4;
	page4.page_len = (ushort_t)max((strlen(password) + 4), 8);
	/* Double check */
	if (strlen(password) > 8) {
		return (L_INVALID_PASSWORD_LEN);
	}
	page4.string_code = L_PASSWORD;
	page4.enable = 1;
	(void) strcpy((char *)page4.name, password);

	if (status = g_scsi_send_diag_cmd(fd, (uchar_t *)&page4,
		page4.page_len + HEADER_LEN)) {
		(void) close(fd);
		return (status);
	}

	(void) close(fd);
	return (0);
}



/*
 * Set the name of the enclosure by sending the name
 * in page 4 of the Send Diagnostic command.
 *
 * The path must point to an IB.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
l_new_name(char *path_phys, char *name)
{
Page4_name	page4;
L_inquiry	inq;
int		fd, status;

	(void) memset(&inq, 0, sizeof (inq));
	(void) memset(&page4, 0, sizeof (page4));

	if ((fd = g_object_open(path_phys, O_NDELAY | O_RDONLY)) == -1) {
		return (L_OPEN_PATH_FAIL);
	}
	/* Verify it is a Photon */
	if (status = g_scsi_inquiry_cmd(fd,
		(uchar_t *)&inq, sizeof (struct l_inquiry_struct))) {
		(void) close(fd);
		return (status);
	}
	if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) == 0) &&
		(!(strncmp((char *)inq.inq_vid, "SUN     ",
		sizeof (inq.inq_vid)) &&
		(inq.inq_dtype == DTYPE_ESI)))) {
		(void) close(fd);
		return (L_ENCL_INVALID_PATH);
	}

	page4.page_code = L_PAGE_4;
	page4.page_len = (ushort_t)((sizeof (struct page4_name) - 4));
	page4.string_code = L_ENCL_NAME;
	page4.enable = 1;
	strncpy((char *)page4.name, name, sizeof (page4.name));

	if (status = g_scsi_send_diag_cmd(fd, (uchar_t *)&page4,
		sizeof (page4))) {
		(void) close(fd);
		return (status);
	}

	/*
	 * Check the name really changed.
	 */
	if (status = g_scsi_inquiry_cmd(fd,
		(uchar_t *)&inq, sizeof (struct l_inquiry_struct))) {
		(void) close(fd);
		return (status);
	}
	if (strncmp((char *)inq.inq_box_name, name, sizeof (page4.name)) != 0) {
		char	name_buf[MAXNAMELEN];
		(void) close(fd);
		strncpy((char *)name_buf, (char *)inq.inq_box_name,
			sizeof (inq.inq_box_name));
		return (L_ENCL_NAME_CHANGE_FAIL);
	}

	(void) close(fd);
	return (0);
}



/*
 * Issue a Loop Port enable Primitive sequence
 * to the device specified by the pathname.
 */
int
l_enable(char *path, int verbose)
/*ARGSUSED*/
{

	return (0);
}

/*
 * Issue a Loop Port Bypass Primitive sequence
 * to the device specified by the pathname. This requests the
 * device to set its L_Port into the bypass mode.
 */
int
l_bypass(char *path, int verbose)
/*ARGSUSED*/
{

	return (0);
}



/*
 * Create a linked list of all the Photon enclosures that
 * are attached to this host.
 *
 * RETURN VALUES: 0 O.K.
 *
 * box_list pointer:
 *			NULL: No enclosures found.
 *			!NULL: Enclosures found
 *                      box_list points to a linked list of boxes.
 */
int
l_get_box_list(struct box_list_struct **box_list_ptr, int verbose)
{
char		*dev_name;
DIR		*dirp;
struct dirent	*entp;
char		namebuf[MAXPATHLEN];
struct stat	sb;
char		*result = NULL;
int		fd, status;
L_inquiry	inq;
Box_list	*box_list, *l1, *l2;
IB_page_config	page1;
uchar_t		node_wwn[WWN_SIZE], port_wwn[WWN_SIZE];
int		al_pa;

	box_list = *box_list_ptr = NULL;
	if ((dev_name = (char *)g_zalloc(sizeof ("/dev/es"))) == NULL) {
		return (L_MALLOC_FAILED);
	}
	(void) sprintf((char *)dev_name, "/dev/es");

	if (verbose) {
		(void) fprintf(stdout,
		MSGSTR(9045,
			"  Searching directory %s for links to enclosures\n"),
			dev_name);
	}

	if ((dirp = opendir(dev_name)) == NULL) {
		(void) g_destroy_data(dev_name);
		/* No Photons found */
		B_DPRINTF("  l_get_box_list: No Photons found\n");
		return (0);
	}


	while ((entp = readdir(dirp)) != NULL) {
		if (strcmp(entp->d_name, ".") == 0 ||
			strcmp(entp->d_name, "..") == 0)
			continue;

		(void) sprintf(namebuf, "%s/%s", dev_name, entp->d_name);

		if ((lstat(namebuf, &sb)) < 0) {
			ER_DPRINTF("Warning: Cannot stat %s\n",
							namebuf);
			continue;
		}

		if (!S_ISLNK(sb.st_mode)) {
			ER_DPRINTF("Warning: %s is not a symbolic link\n",
								namebuf);
			continue;
		}
		if ((result = g_get_physical_name_from_link(namebuf)) == NULL) {
			ER_DPRINTF("  Warning: Get physical name from"
			" link failed. Link=%s\n", namebuf);
			continue;
		}

		/* Found a SES card. */
		B_DPRINTF("  l_get_box_list: Link to SES Card found: %s/%s\n",
			dev_name, entp->d_name);
		if ((fd = g_object_open(result, O_NDELAY | O_RDONLY)) == -1) {
			g_destroy_data(result);
			continue;	/* Ignore errors */
		}
		/* Get the box name */
		if (status = g_scsi_inquiry_cmd(fd,
			(uchar_t *)&inq, sizeof (struct l_inquiry_struct))) {
			(void) close(fd);
			g_destroy_data(result);
			continue;	/* Ignore errors */
		}


		if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) != 0) ||
			(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) &&
			(inq.inq_dtype == DTYPE_ESI))) {
			/*
			 * Found Photon
			 */

			/* Get the port WWN from the IB, page 1 */
			if ((status = l_get_envsen_page(fd, (uchar_t *)&page1,
				sizeof (page1), 1, 0)) != NULL) {
				(void) close(fd);
				g_destroy_data(result);
				(void) g_destroy_data(dev_name);
				closedir(dirp);
				return (status);
			}

			/*
			 * Build list of names.
			 */
			if ((l2 = (struct  box_list_struct *)
				g_zalloc(sizeof (struct  box_list_struct)))
				== NULL) {
				(void) close(fd);
				g_destroy_data(result);
				g_destroy_data(dev_name);
				closedir(dirp);
				return (L_MALLOC_FAILED);
			}

			/* Fill in structure */
			(void) strcpy((char *)l2->b_physical_path,
				(char *)result);
			(void) strcpy((char *)l2->logical_path,
				(char *)namebuf);
			bcopy((void *)page1.enc_node_wwn,
				(void *)l2->b_node_wwn, WWN_SIZE);
			(void) sprintf(l2->b_node_wwn_s,
			"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
				page1.enc_node_wwn[0],
				page1.enc_node_wwn[1],
				page1.enc_node_wwn[2],
				page1.enc_node_wwn[3],
				page1.enc_node_wwn[4],
				page1.enc_node_wwn[5],
				page1.enc_node_wwn[6],
				page1.enc_node_wwn[7]);
			strncpy((char *)l2->prod_id_s,
				(char *)inq.inq_pid,
				sizeof (inq.inq_pid));
			strncpy((char *)l2->b_name,
				(char *)inq.inq_box_name,
				sizeof (inq.inq_box_name));
			/* make sure null terminated */
			l2->b_name[sizeof (l2->b_name) - 1] = NULL;
			/*
			 * Now get the port WWN for the port
			 * we are connected to.
			 */
			if (status = g_get_wwn(result, port_wwn, node_wwn,
				&al_pa, verbose)) {
				(void) close(fd);
				g_destroy_data(result);
				(void) g_destroy_data(dev_name);
				(void) g_destroy_data(l2);
				closedir(dirp);
				return (status);
			}
			(void) sprintf(l2->b_port_wwn_s,
			"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			port_wwn[0], port_wwn[1], port_wwn[2], port_wwn[3],
			port_wwn[4], port_wwn[5], port_wwn[6], port_wwn[7]);
			bcopy((void *)port_wwn,
				(void *)l2->b_port_wwn, WWN_SIZE);

			B_DPRINTF("  l_get_box_list:"
				" Found enclosure named:%s\n", l2->b_name);

			if (box_list == NULL) {
				l1 = box_list = l2;
			} else {
				l2->box_prev = l1;
				l1 = l1->box_next = l2;
			}

		}
		g_destroy_data(result);
		(void) close(fd);
		*box_list_ptr = box_list; /* pass back ptr to list */
	}
	(void) g_destroy_data(dev_name);
	closedir(dirp);
	return (0);
}



void
l_free_box_list(struct box_list_struct **box_list)
{
Box_list	*next = NULL;

	for (; *box_list != NULL; *box_list = next) {
		next = (*box_list)->box_next;
		(void) g_destroy_data(*box_list);
	}

	*box_list = NULL;
}



/*
 * Finds out if there are any other boxes
 * with the same name as "name".
 *
 * RETURNS:
 *	0   There are no other boxes with the same name.
 *	>0  if duplicate names found
 */
/*ARGSUSED*/
int
l_duplicate_names(Box_list *b_list, char wwn[], char *name, int verbose)
{
int		dup_flag = 0;
Box_list	*box_list_ptr = NULL;

	box_list_ptr = b_list;
	while (box_list_ptr != NULL) {
		if ((strcmp(name, (const char *)box_list_ptr->b_name) == 0) &&
			(strcmp(box_list_ptr->b_node_wwn_s, wwn) != 0)) {
			dup_flag++;
			break;
		}
		box_list_ptr = box_list_ptr->box_next;
	}
	return (dup_flag);
}



/*
 * Checks for a name conflict with an SSA cN type name.
 */
int
l_get_conflict(char *name, char **result, int verbose)
{
char		s[MAXPATHLEN];
char		*p = NULL;
char		*pp = NULL;
Box_list	*box_list = NULL;
int		found_box = 0, err = 0;

	(void) strcpy(s, name);
	if ((*result = g_get_physical_name(s)) == NULL) {
		return (0);
	}
	if ((strstr((const char *)*result, PLNDEF)) == NULL) {
		(void) g_destroy_data(*result);
		*result = NULL;
		return (0);
	}
	P_DPRINTF("  l_get_conflict: Found "
		"SSA path using %s\n", s);
	/* Find path to IB */
	if ((err = l_get_box_list(&box_list, verbose)) != 0) {
		return (err);	/* Failure */
	}
	/*
	 * Valid cN type name found.
	 */
	while (box_list != NULL) {
		if ((strcmp((char *)s,
			(char *)box_list->b_name)) == 0) {
			found_box = 1;
			if (p == NULL) {
				if ((p = g_zalloc(strlen(
				box_list->b_physical_path)
				+ 2)) == NULL) {
				(void) l_free_box_list(&box_list);
				return (errno);
				}
			} else {
				if ((pp = g_zalloc(strlen(
				box_list->b_physical_path)
				+ strlen(p)
				+ 2)) == NULL) {
				(void) l_free_box_list(&box_list);
				return (errno);
				}
				(void) strcpy(pp, p);
				(void) g_destroy_data(p);
				p = pp;
			}
			(void) strcat(p, box_list->b_physical_path);
			(void) strcat(p, "\n");
		}
		box_list = box_list->box_next;
	}
	if (found_box) {
		D_DPRINTF("There is a conflict between the "
			"enclosure\nwith this name, %s, "
			"and a SSA name of the same form.\n"
			"Please use one of the following physical "
			"pathnames:\n%s\n%s\n",
			s, *result, p);

		(void) l_free_box_list(&box_list);
		(void) g_destroy_data(p);
		return (L_SSA_CONFLICT);	/* failure */
	}
	(void) l_free_box_list(&box_list);
	return (0);
}



/*
 * convert box name or WWN or logical path to physical path.
 *
 *	OUTPUT:
 *		path_struct:
 *		- This structure is used to return more detailed
 *		  information about the path.
 *		- *p_physical_path
 *		  Normally this is the requested physical path.
 *		  If the requested path is not found then iff the
 *		  ib_path_flag is set this is the IB path.
 *		- *argv
 *		This is the argument variable input. e.g. Bob,f1
 *              - slot_valid
 *              - slot
 *		This is the slot number that was entered when using
 *		  the box,[fr]slot format. It is only valid if the
 *		  slot_valid flag is set.
 *		- f_flag
 *		  Front flag - If set, the requested device is located in the
 *		  front of the enclosure.
 *		- ib_path_flag
 *		  If this flag is set it means a devices path was requested
 *		  but could not be found but an IB's path was found and
 *		  the p_physical_path points to that path.
 *		- **phys_path
 *		  physical path to the device.
 *	RETURNS:
 *		- 0  if O.K.
 *		- error otherwise.
 */
int
l_convert_name(char *name, char **phys_path,
		struct path_struct **path_struct, int verbose)
{
char		s[MAXPATHLEN], ses_path[MAXPATHLEN];
Box_list	*box_list = NULL, *box_list_ptr = NULL;
WWN_list	*wwn_list, *wwn_list_ptr;
char		*char_ptr, *ptr = NULL;
Path_struct	*path_ptr = NULL;
int		slot, slot_flag = 0, found_box = 0, found_comma = 0, err = 0;
L_state		*l_state = NULL;
char		*result = NULL;
hrtime_t	start_time, end_time;

	start_time = gethrtime();

	if ((*path_struct = path_ptr = (struct path_struct *)
		g_zalloc(sizeof (struct path_struct))) == NULL) {
		return (L_MALLOC_FAILED);
	}

	*phys_path = NULL;
	/*
	 * If the path contains a "/" then assume
	 * it is a logical or physical path as the
	 * box name or wwn can not contain "/"s.
	 */
	if (strchr(name, '/') != NULL) {
		if ((result = g_get_physical_name(name)) == NULL) {
			return (L_NO_PHYS_PATH);
		}
		goto done;
	}

	(void) strcpy(s, name);
	if ((s[0] == 'c') &&
		((int)strlen(s) > 1) && ((int)strlen(s) < 5)) {
		if ((err = l_get_conflict(s, &result, verbose)) != 0) {
			if (result != NULL) {
				(void) g_destroy_data(result);
			}
			return (err);
		}
		if (result != NULL)
			goto done;
	}

	/*
	 * Check to see if we have a box or WWN name.
	 *
	 * If it contains a , then the format must be
	 *    box_name,f1 where f is front and 1 is the slot number
	 * or it is a format like
	 * ssd@w2200002037049adf,0:h,raw
	 * or
	 * SUNW,pln@a0000000,77791d:ctlr
	 */
	if (((char_ptr = strstr(s, ",")) != NULL) &&
		((*(char_ptr + 1) == 'f') || (*(char_ptr + 1) == 'r'))) {
		char_ptr++;	/* point to f/r */
		if (*char_ptr == 'f') {
			path_ptr->f_flag = 1;
		} else if (*char_ptr != 'r') {
			return (L_INVALID_PATH_FORMAT);
		}
		char_ptr++;
		slot = strtol(char_ptr, &ptr, 10);
		/*
		 * NOTE: Need to double check the slot when we get
		 * the number of the devices actually in the box.
		 */
		if ((slot < 0) || (ptr == char_ptr) ||
			(slot >= (MAX_DRIVES_PER_BOX/2))) {
			return (L_INVALID_SLOT);
		}
		/* Say slot valid. */
		slot_flag = path_ptr->slot_valid = 1;
		path_ptr->slot = slot;
	}

	if (((char_ptr = strstr(s, ",")) != NULL) &&
		((*(char_ptr + 1) == 'f') || (*(char_ptr + 1) == 'r'))) {
		*char_ptr = NULL; /* make just box name */
		found_comma = 1;
	}
	/* Find path to IB */
	if ((err = l_get_box_list(&box_list, verbose)) != 0) {
		(void) l_free_box_list(&box_list);
		return (err);
	}
	box_list_ptr = box_list;
	/* Look for box name. */
	while (box_list != NULL) {
	    if ((strcmp((char *)s, (char *)box_list->b_name)) == 0) {
			result =
				g_alloc_string(box_list->b_physical_path);
			L_DPRINTF("  l_convert_name:"
			" Found subsystem: name %s  WWN %s\n",
			box_list->b_name, box_list->b_node_wwn_s);
			/*
			 * Check for another box with this name.
			 */
			if (l_duplicate_names(box_list_ptr,
				box_list->b_node_wwn_s,
				(char *)box_list->b_name,
				verbose)) {
				(void) l_free_box_list(&box_list_ptr);
				(void) g_destroy_data(result);
				return (L_DUPLICATE_ENCLOSURES);
			}
			found_box = 1;
			break;
		}
		box_list = box_list->box_next;
	}
	/*
	 * Check to see if we must get individual disks path.
	 */

	if (found_box && slot_flag) {
		if ((l_state = (L_state *)g_zalloc(sizeof (L_state))) == NULL) {
			(void) g_destroy_data(result);
			(void) l_free_box_list(&box_list_ptr);
			return (L_MALLOC_FAILED);
		}
		(void) strcpy(ses_path, result);
		if ((err = l_get_status(ses_path, l_state,
			verbose)) != 0) {
			(void) g_destroy_data(result);
			(void) g_destroy_data(l_state);
			(void) l_free_box_list(&box_list_ptr);
			return (err);
		}
		/*
		 * Now double check the slot number.
		 */
		if (slot >= l_state->total_num_drv/2) {
			path_ptr->slot_valid = 0;
			(void) g_destroy_data(result);
			(void) l_free_box_list(&box_list_ptr);
			(void) l_free_lstate(&l_state);
			return (L_INVALID_SLOT);
		}
		if (path_ptr->f_flag) {
		if (*l_state->drv_front[slot].g_disk_state.physical_path) {
				result =
	g_alloc_string(l_state->drv_front[slot].g_disk_state.physical_path);
			} else {
				/* Result is the IB path */
				path_ptr->ib_path_flag = 1;
				path_ptr->p_physical_path =
					g_alloc_string(result);
				(void) g_destroy_data(result);
				result = NULL;
			}
		} else {
		if (*l_state->drv_rear[slot].g_disk_state.physical_path) {
				result =
	g_alloc_string(l_state->drv_rear[slot].g_disk_state.physical_path);
			} else {
				/* Result is the IB path */
				path_ptr->ib_path_flag = 1;
				path_ptr->p_physical_path =
					g_alloc_string(result);
				(void) g_destroy_data(result);
				result = NULL;
			}
		}
		(void) l_free_lstate(&l_state);
		goto done;
	}
	if (found_box || found_comma) {
		goto done;
	}
	/*
	 * No luck with the box name.
	 *
	 * Try WWN's
	 */
	/* Look for the SES's WWN */
	box_list = box_list_ptr;
	while (box_list != NULL) {
		if (((strcmp((char *)s,
			(char *)box_list->b_port_wwn_s)) == 0) ||
			((strcmp((char *)s,
			(char *)box_list->b_node_wwn_s)) == 0)) {
				result =
				g_alloc_string(box_list->b_physical_path);
				L_DPRINTF("  l_convert_name:"
				" Found subsystem using the WWN"
				": name %s  WWN %s\n",
				box_list->b_name, box_list->b_node_wwn_s);
				goto done;
		}
		box_list = box_list->box_next;
	}
	/* Look for a devices WWN */
	if (strlen(s) <= L_WWN_LENGTH) {
		if ((err = g_get_wwn_list(&wwn_list, verbose)) != 0) {
			return (err);
		}
		for (wwn_list_ptr = wwn_list; wwn_list_ptr != NULL;
				wwn_list_ptr = wwn_list_ptr->wwn_next) {
		    if (((strcmp((char *)s,
			(char *)wwn_list_ptr->node_wwn_s)) == 0) ||
			((strcmp((char *)s,
			(char *)wwn_list_ptr->port_wwn_s)) == 0)) {
			result =
				g_alloc_string(wwn_list_ptr->physical_path);
			L_DPRINTF("  l_convert_name:"
			"  Found device: WWN %s Path %s\n",
			s, wwn_list_ptr->logical_path);
			(void) g_free_wwn_list(&wwn_list);
			goto done;
		    }
		}
	}

	/*
	 * Try again in case we were in the /dev
	 * or /devices directory.
	 */
	result = g_get_physical_name(name);

done:
	(void) l_free_box_list(&box_list_ptr);
	path_ptr->argv = name;
	if (result == NULL) {
		if (!path_ptr->ib_path_flag)
			return (-1);
	} else {
		path_ptr->p_physical_path = result;
	}

	L_DPRINTF("  l_convert_name: path_struct:\n\tphysical_path:\n\t %s\n"
		"\targv:\t\t%s"
		"\n\tslot_valid\t%d"
		"\n\tslot\t\t%d"
		"\n\tf_flag\t\t%d"
		"\n\tib_path_flag\t%d\n",
		path_ptr->p_physical_path,
		path_ptr->argv,
		path_ptr->slot_valid,
		path_ptr->slot,
		path_ptr->f_flag,
		path_ptr->ib_path_flag);
	if (getenv("_LUX_T_DEBUG") != NULL) {
		end_time = gethrtime();
		(void) fprintf(stdout, "  l_convert_name: "
		"Time = %lld millisec\n",
		(end_time - start_time)/1000000);
	}

	if (path_ptr->ib_path_flag)
		return (-1);
	*phys_path = result;
	return (0);
}


/*
 * Gets envsen information of an enclosure from IB
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
l_get_envsen_page(int fd, uchar_t *buf, int buf_size, uchar_t page_code,
	int verbose)
{
Rec_diag_hdr	hdr;
uchar_t	*pg;
int	size, new_size, status;

	if (verbose) {
		(void) fprintf(stdout,
		MSGSTR(9046, "  Reading SES page %x\n"), page_code);
	}

	(void) memset(&hdr, 0, sizeof (struct rec_diag_hdr));
	if (status = g_scsi_rec_diag_cmd(fd, (uchar_t *)&hdr,
		sizeof (struct rec_diag_hdr), page_code)) {
		return (status);
	}

	/* Check */
	if ((hdr.page_code != page_code) || (hdr.page_len == 0)) {
		return (L_RD_PG_INVLD_CODE);
	}
	size = HEADER_LEN + hdr.page_len;
	/*
	 * Because of a hardware restriction in the soc+ chip
	 * the transfers must be word aligned.
	 */
	while (size & 0x03) {
		size++;
		if (size > buf_size) {
			return (L_RD_PG_MIN_BUFF);
		}
		P_DPRINTF("  l_get_envsen_page: Adjusting size of the "
			"g_scsi_rec_diag_cmd buffer.\n");
	}

	if ((pg = (uchar_t *)g_zalloc(size)) == NULL) {
		return (L_MALLOC_FAILED);
	}

	P_DPRINTF("  l_get_envsen_page: Reading page %x of size 0x%x\n",
		page_code, size);
	if (status = g_scsi_rec_diag_cmd(fd, pg, size, page_code)) {
		(void) g_destroy_data((char *)pg);
		return (status);
	}

	new_size = MIN(size, buf_size);
	bcopy((const void *)pg, (void *)buf, (size_t)new_size);

	(void) g_destroy_data(pg);
	return (0);
}



/*
 * Get consolidated copy of all environmental information
 * into buf structure.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */

int
l_get_envsen(char *path_phys, uchar_t *buf, int size, int verbose)
{
int		fd, rval;
uchar_t		*page_list_ptr, page_code, *local_buf_ptr = buf;
Rec_diag_hdr	*hdr = (struct rec_diag_hdr *)(int)buf;
ushort_t	num_pages;

	page_code = L_PAGE_PAGE_LIST;

	/* open IB */
	if ((fd = g_object_open(path_phys, O_NDELAY | O_RDONLY)) == -1)
		return (L_OPEN_PATH_FAIL);

	P_DPRINTF("  l_get_envsen: Getting list of supported"
		" pages from IB\n");
	if (verbose) {
		(void) fprintf(stdout,
		MSGSTR(9047, "  Getting list of supported pages from IB\n"));
	}

	/* Get page 0 */
	if ((rval = l_get_envsen_page(fd, local_buf_ptr,
		size, page_code, verbose)) != NULL) {
		(void) close(fd);
		return (rval);
	}

	page_list_ptr = buf + HEADER_LEN + 1; /* +1 to skip page 0 */

	num_pages = hdr->page_len - 1;

	/*
	 * check whether the number of pages received
	 * from IB are valid. SENA enclosure
	 * supports only 8 pages of sense information.
	 * According to SES specification dpANS X3.xxx-1997
	 * X3T10/Project 1212-D/Rev 8a, the enclosure supported
	 * pages can go upto L_MAX_POSSIBLE_PAGES (0xFF).
	 * Return an error if no. of pages exceeds L_MAX_POSSIBLE_PAGES.
	 * See if (num_pages >= L_MAX_POSSIBLE_PAGES) since 1 page (page 0)
	 * was already subtracted from the total number of pages before.
	 */
	if (num_pages < 1 || num_pages >= L_MAX_POSSIBLE_PAGES) {
		return (L_INVALID_NO_OF_ENVSEN_PAGES);
	}
	/*
	 * Buffer size of MAX_REC_DIAG_LENGTH can be small if the
	 * number of pages exceed more than L_MAX_SENAIB_PAGES
	 * but less than L_MAX_POSSIBLE_PAGES.
	 */
	if (size == MAX_REC_DIAG_LENGTH &&
			num_pages >= L_MAX_SENAIB_PAGES) {
		return (L_INVALID_BUF_LEN);
	}
	/* Align buffer */
	while (hdr->page_len & 0x03) {
		hdr->page_len++;
	}
	local_buf_ptr += HEADER_LEN + hdr->page_len;

	/*
	 * Getting all pages and appending to buf
	 */
	for (; num_pages--; page_list_ptr++) {
		/*
		 * The fifth byte of page 0 is the start
		 * of the list of pages not including page 0.
		 */
		page_code = *page_list_ptr;

		if ((rval = l_get_envsen_page(fd, local_buf_ptr,
			size, page_code, verbose)) != NULL) {
			(void) close(fd);
			return (rval);
		}
		hdr = (struct rec_diag_hdr *)(int)local_buf_ptr;
		local_buf_ptr += HEADER_LEN + hdr->page_len;
	}

	(void) close(fd);
	return (0);
}



/*
 * Get the individual disk status.
 * Path must be physical and point to a disk.
 *
 * This function updates the d_state_flags, port WWN's
 * and num_blocks for all accessiable ports
 * in l_disk_state->g_disk_state structure.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
l_get_disk_status(char *path, struct l_disk_state_struct *l_disk_state,
	WWN_list *wwn_list, int verbose)
{
struct dlist	*ml;
char		path_a[MAXPATHLEN], path_b[MAXPATHLEN], ses_path[MAXPATHLEN];
sf_al_map_t	map;
int		path_a_found = 0, path_b_found = 0, local_port_a_flag;
uchar_t		node_wwn[WWN_SIZE], port_wwn[WWN_SIZE];
int		al_pa, err;

	*path_a = *path_b = NULL;
	l_disk_state->g_disk_state.num_blocks = 0;	/* initialize */

	/* Get paths. */
	g_get_multipath(path,
		&(l_disk_state->g_disk_state.multipath_list),
		wwn_list, verbose);
	ml = l_disk_state->g_disk_state.multipath_list;
	if (ml == NULL) {
		l_disk_state->l_state_flag = L_NO_PATH_FOUND;
		G_DPRINTF("  l_get_disk_status: Error finding a "
			"multipath to the disk.\n");
		return (0);
	}

	while (ml && (!(path_a_found && path_b_found))) {
		if (err = g_get_dev_map(ml->dev_path, &map, verbose)) {
			(void) g_free_multipath(ml);
			return (err);
		}
		if ((err = l_get_ses_path(ml->dev_path, ses_path,
			&map, verbose)) != 0) {
			(void) g_free_multipath(ml);
			return (err);
		}
		/*
		 * Get the port, A or B, of the disk,
		 * by passing the IB path.
		 */
		if (err = l_get_port(ses_path, &local_port_a_flag, verbose)) {
			(void) g_free_multipath(ml);
			return (err);
		}
		if (local_port_a_flag && (!path_a_found)) {
			G_DPRINTF("  l_get_disk_status: Path to Port A "
				"found: %s\n", ml->dev_path);
			if (err = l_get_disk_port_status(ml->dev_path,
				l_disk_state, local_port_a_flag, verbose)) {
				(void) g_free_multipath(ml);
				return (err);
			}
			if (err = g_get_wwn(ml->dev_path,
				port_wwn, node_wwn,
				&al_pa, verbose)) {
				(void) g_free_multipath(ml);
				return (err);
			}
			(void) sprintf(l_disk_state->g_disk_state.port_a_wwn_s,
			"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			port_wwn[0], port_wwn[1], port_wwn[2], port_wwn[3],
			port_wwn[4], port_wwn[5], port_wwn[6], port_wwn[7]);

			l_disk_state->g_disk_state.port_a_valid++;
			path_a_found++;
		}
		if ((!local_port_a_flag) && (!path_b_found)) {
			G_DPRINTF("  l_get_disk_status: Path to Port B "
				"found: %s\n", ml->dev_path);
			if (err = l_get_disk_port_status(ml->dev_path,
				l_disk_state, local_port_a_flag, verbose)) {
				return (err);
			}
			if (err = g_get_wwn(ml->dev_path,
				port_wwn, node_wwn,
				&al_pa, verbose)) {
				(void) g_free_multipath(ml);
				return (err);
			}
			(void) sprintf(l_disk_state->g_disk_state.port_b_wwn_s,
			"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			port_wwn[0], port_wwn[1], port_wwn[2], port_wwn[3],
			port_wwn[4], port_wwn[5], port_wwn[6], port_wwn[7]);

			l_disk_state->g_disk_state.port_b_valid++;
			path_b_found++;
		}
		ml = ml->next;
	}
	return (0);
}



/*
 * Check for Persistent Reservations.
 */
int
l_persistent_check(int fd, struct l_disk_state_struct *l_disk_state,
	int verbose)
{
int	status;
Read_keys	read_key_buf;
Read_reserv	read_reserv_buf;

	(void) memset(&read_key_buf, 0, sizeof (struct  read_keys_struct));
	if ((status = g_scsi_persistent_reserve_in_cmd(fd,
		(uchar_t *)&read_key_buf, sizeof (struct  read_keys_struct),
		ACTION_READ_KEYS))) {
		return (status);
	}
	/* This means persistent reservations are supported by the disk. */
	l_disk_state->g_disk_state.persistent_reserv_flag = 1;

	if (read_key_buf.rk_length) {
		l_disk_state->g_disk_state.persistent_registered = 1;
	}

	(void) memset(&read_reserv_buf, 0,
			sizeof (struct  read_reserv_struct));
	if ((status = g_scsi_persistent_reserve_in_cmd(fd,
		(uchar_t *)&read_reserv_buf,
		sizeof (struct  read_reserv_struct),
		ACTION_READ_RESERV))) {
		return (status);
	}
	if (read_reserv_buf.rr_length) {
		l_disk_state->g_disk_state.persistent_active = 1;
	}
	if (verbose) {
		(void) fprintf(stdout,
		MSGSTR(9048, "  Checking for Persistent "
			"Reservations:"));
		if (l_disk_state->g_disk_state.persistent_reserv_flag) {
		    if (l_disk_state->g_disk_state.persistent_active != NULL) {
			(void) fprintf(stdout, MSGSTR(39, "Active"));
		    } else {
			(void) fprintf(stdout, MSGSTR(9049, "Registered"));
		    }
		} else {
			(void) fprintf(stdout,
			MSGSTR(87,
			"Not being used"));
		}
		(void) fprintf(stdout, "\n");
	}
	return (0);
}



/*
 * Gets the disk status and
 * updates the l_disk_state_struct structure.
 * Checks for open fail, Reservation Conflicts,
 * Not Ready and so on.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
l_get_disk_port_status(char *path, struct l_disk_state_struct *l_disk_state,
	int port_a_flag, int verbose)
{
int		fd, status = 0, local_state = 0;
Read_capacity_data	capacity;	/* local read capacity buffer */
struct vtoc	vtoc;

	/*
	 * Try to open drive.
	 */
	if ((fd = g_object_open(path, O_RDONLY)) == -1) {
	    if ((fd = g_object_open(path,
		O_RDONLY | O_NDELAY)) == -1) {
		G_DPRINTF("  l_get_disk_port_status: Error "
			"opening drive %s\n", path);
		local_state = L_OPEN_FAIL;
	    } else {
		/* See if drive ready */
		if (status = g_scsi_tur(fd)) {
			if ((status & L_SCSI_ERROR) &&
				((status & ~L_SCSI_ERROR) == STATUS_CHECK)) {
				/*
				 * TBD
				 * This is where I should figure out
				 * if the device is Not Ready or whatever.
				 */
				local_state = L_NOT_READY;
			} else if ((status & L_SCSI_ERROR) &&
			    ((status & ~L_SCSI_ERROR) ==
			    STATUS_RESERVATION_CONFLICT)) {
			    /* mark reserved */
			    local_state = L_RESERVED;
			} else {
				local_state = L_SCSI_ERR;
			}

		/*
		 * There may not be a label on the drive - check
		 */
		} else if (ioctl(fd, DKIOCGVTOC, &vtoc) == -1) {
		    I_DPRINTF("\t- DKIOCGVTOC ioctl failed: "
		    " invalid geometry\n");
		    local_state = L_NO_LABEL;
		} else {
			/*
			 * Sanity-check the vtoc
			 */
		    if (vtoc.v_sanity != VTOC_SANE ||
			vtoc.v_sectorsz != DEV_BSIZE) {
			local_state = L_NO_LABEL;
			G_DPRINTF("  l_get_disk_port_status: "
				"Checking vtoc - No Label found.\n");
		    }
		}
	    }
	}

	if ((local_state == 0) || (local_state == L_NO_LABEL)) {

	    if (status = g_scsi_read_capacity_cmd(fd, (uchar_t *)&capacity,
		sizeof (capacity))) {
			G_DPRINTF("  l_get_disk_port_status: "
				"Read Capacity failed.\n");
		if (status & L_SCSI_ERROR) {
		    if ((status & ~L_SCSI_ERROR) ==
			STATUS_RESERVATION_CONFLICT) {
			/* mark reserved */
			local_state |= L_RESERVED;
		    } else
			/* mark bad */
			local_state |= L_NOT_READABLE;
		} else {
			/*
			 * TBD
			 * Need a more complete state definition here.
			 */
			l_disk_state->g_disk_state.d_state_flags[port_a_flag] =
								L_SCSI_ERR;
			(void) close(fd);
			return (0);
		}
	    } else {
		/* save capacity */
		l_disk_state->g_disk_state.num_blocks =
					capacity.last_block_addr + 1;
	    }

	}
	(void) close(fd);

	l_disk_state->g_disk_state.d_state_flags[port_a_flag] = local_state;
	G_DPRINTF("  l_get_disk_port_status: Individual Disk"
		" Status: 0x%x for"
		" port %s for path:"
		" %s\n", local_state,
		port_a_flag ? "A" : "B", path);

	return (0);
}



/*
 * Copy and format page 1 from big buffer to state structure.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */

static int
copy_config_page(struct l_state_struct *l_state, uchar_t *from_ptr)
{
IB_page_config	*encl_ptr;
int		size, i;


	encl_ptr = (struct ib_page_config *)(int)from_ptr;

	/* Sanity check. */
	if ((encl_ptr->enc_len > MAX_VEND_SPECIFIC_ENC) ||
		(encl_ptr->enc_len == 0)) {
		return (L_REC_DIAG_PG1);
	}
	if ((encl_ptr->enc_num_elem > MAX_IB_ELEMENTS) ||
		(encl_ptr->enc_num_elem == 0)) {
		return (L_REC_DIAG_PG1);
	}

	size = HEADER_LEN + 4 + HEADER_LEN + encl_ptr->enc_len;
	bcopy((void *)(from_ptr),
		(void *)&l_state->ib_tbl.config, (size_t)size);
	/*
	 * Copy Type Descriptors seperately to get aligned.
	 */
	from_ptr += size;
	size = (sizeof (struct	type_desc_hdr))*encl_ptr->enc_num_elem;
	bcopy((void *)(from_ptr),
		(void *)&l_state->ib_tbl.config.type_hdr, (size_t)size);

	/*
	 * Copy Text Descriptors seperately to get aligned.
	 *
	 * Must use the text size from the Type Descriptors.
	 */
	from_ptr += size;
	for (i = 0; i < (int)l_state->ib_tbl.config.enc_num_elem; i++) {
		size = l_state->ib_tbl.config.type_hdr[i].text_len;
		bcopy((void *)(from_ptr),
			(void *)&l_state->ib_tbl.config.text[i], (size_t)size);
		from_ptr += size;
	}
	return (0);
}



/*
 * Copy page 7 (Element Descriptor page) to state structure.
 * Copy header then copy each element descriptor
 * seperately.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
static void
copy_page_7(struct l_state_struct *l_state, uchar_t *from_ptr)
{
uchar_t	*my_from_ptr;
int	size, j, k, p7_index;

	size = HEADER_LEN +
		sizeof (l_state->ib_tbl.p7_s.gen_code);
	bcopy((void *)(from_ptr),
		(void *)&l_state->ib_tbl.p7_s, (size_t)size);
	my_from_ptr = from_ptr + size;
	if (getenv("_LUX_D_DEBUG") != NULL) {
		g_dump("  copy_page_7: Page 7 header:  ",
		(uchar_t *)&l_state->ib_tbl.p7_s, size,
		HEX_ASCII);
		(void) fprintf(stdout,
			"  copy_page_7: Elements being stored "
			"in state table\n"
			"              ");
	}
	/* I am assuming page 1 has been read. */
	for (j = 0, p7_index = 0;
		j < (int)l_state->ib_tbl.config.enc_num_elem; j++) {
		/* Copy global element */
		size = HEADER_LEN +
			((*(my_from_ptr + 2) << 8) | *(my_from_ptr + 3));
		bcopy((void *)(my_from_ptr),
		(void *)&l_state->ib_tbl.p7_s.element_desc[p7_index++],
			(size_t)size);
		my_from_ptr += size;
		for (k = 0; k < (int)l_state->ib_tbl.config.type_hdr[j].num;
			k++) {
			/* Copy individual elements */
			size = HEADER_LEN +
				((*(my_from_ptr + 2) << 8) |
					*(my_from_ptr + 3));
			bcopy((void *)(my_from_ptr),
			(void *)&l_state->ib_tbl.p7_s.element_desc[p7_index++],
				(size_t)size);
			my_from_ptr += size;
			D_DPRINTF(".");
		}
	}
	D_DPRINTF("\n");
}


/*
 * Gets the status of an IB using a given pathname
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
l_get_ib_status(char *path, struct l_state_struct *l_state,
	int verbose)
{
uchar_t		*ib_buf, *from_ptr;
int		num_pages, i, size, err;
IB_page_2	*encl_ptr;

	/*
	* get big buffer
	*/
	if ((ib_buf = (uchar_t *)calloc(1,
				MAX_REC_DIAG_LENGTH)) == NULL) {
		return (L_MALLOC_FAILED);
	}

	/*
	 * Get IB information
	 * Even if there are 2 IB's in this box on this loop don't bother
	 * talking to the other one as both IB's in a box
	 * are supposed to report the same information.
	 */
	if (err = l_get_envsen(path, ib_buf, MAX_REC_DIAG_LENGTH,
		verbose)) {
		(void) g_destroy_data(ib_buf);
		return (err);
	}

	/*
	 * Set up state structure
	 */
	bcopy((void *)ib_buf, (void *)&l_state->ib_tbl.p0,
		(size_t)sizeof (struct  ib_page_0));

	num_pages = l_state->ib_tbl.p0.page_len;
	from_ptr = ib_buf + HEADER_LEN + l_state->ib_tbl.p0.page_len;

	for (i = 1; i < num_pages; i++) {
		if (l_state->ib_tbl.p0.sup_page_codes[i] == L_PAGE_1) {
			if (err = copy_config_page(l_state, from_ptr)) {
				return (err);
			}
		} else if (l_state->ib_tbl.p0.sup_page_codes[i] ==
								L_PAGE_2) {
			encl_ptr = (struct ib_page_2 *)(int)from_ptr;
			size = HEADER_LEN + encl_ptr->page_len;
			bcopy((void *)(from_ptr),
				(void *)&l_state->ib_tbl.p2_s, (size_t)size);
			if (getenv("_LUX_D_DEBUG") != NULL) {
				g_dump("  l_get_ib_status: Page 2:  ",
				(uchar_t *)&l_state->ib_tbl.p2_s, size,
				HEX_ONLY);
			}

		} else if (l_state->ib_tbl.p0.sup_page_codes[i] ==
								L_PAGE_7) {
			(void) copy_page_7(l_state, from_ptr);
		}
		from_ptr += ((*(from_ptr + 2) << 8) | *(from_ptr + 3));
		from_ptr += HEADER_LEN;
	}
	(void) g_destroy_data(ib_buf);
	G_DPRINTF("  l_get_ib_status: Read %d Receive Diagnostic pages "
		"from the IB.\n", num_pages);
	return (0);
}



/*
 * Given an IB path get the port, A or B.
 *
 * OUTPUT:
 *	port_a:	sets to 1 for port A
 *		and 0 for port B.
 * RETURNS:
 *	err:	0 O.k.
 *		non-zero otherwise
 */
int
l_get_port(char *ses_path, int *port_a, int verbose)
{
L_state	*ib_state = NULL;
Ctlr_elem_st	ctlr;
int	i, err, elem_index = 0;

	if ((ib_state = (L_state *)calloc(1, sizeof (L_state))) == NULL) {
		return (L_MALLOC_FAILED);
	}
	if (err = l_get_ib_status(ses_path, ib_state, verbose)) {
		(void) l_free_lstate(&ib_state);
		return (err);
	}

	for (i = 0; i < (int)ib_state->ib_tbl.config.enc_num_elem; i++) {
	    elem_index++;		/* skip global */
	    if (ib_state->ib_tbl.config.type_hdr[i].type == ELM_TYP_IB) {
		bcopy((const void *)
			&ib_state->ib_tbl.p2_s.element[elem_index],
			(void *)&ctlr, sizeof (ctlr));
		break;
	    }
	    elem_index += ib_state->ib_tbl.config.type_hdr[i].num;
	}
	*port_a = ctlr.report;
	G_DPRINTF("  l_get_port: Found ses is the %s card.\n",
		ctlr.report ? "A" : "B");
	(void) l_free_lstate(&ib_state);
	return (0);
}



/*
 * Finds the disk's node wwn string, and
 * port A and B's WWNs and their port status.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise.
 */
static int
l_get_node_status(char *path, struct l_disk_state_struct *state,
	int *found_flag, WWN_list *wwn_list, int verbose)
{
int		j, select_id, err, i, not_null, name_id;
char		temp_path[MAXPATHLEN];
char		sbuf[MAXPATHLEN], *char_ptr;
sf_al_map_t	map;

	/*
	 * Get a new map.
	 */
	if (err = g_get_dev_map(path, &map, verbose))
		return (err);

	for (j = 0; j < map.sf_count; j++) {

		/*
		 * Get a generic path to a device
		 *
		 * This assumes the path looks something like this
		 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0/ses@x,0:0
		 * then creates a path that looks like
		 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0/ssd@
		 */
		(void) strcpy(temp_path, path);
		if ((char_ptr = strrchr(temp_path, '/')) == NULL) {
			return (L_INVALID_PATH);
		}
		*char_ptr = '\0';   /* Terminate sting  */
		(void) strcat(temp_path, SLSH_DRV_NAME_SSD);
		/*
		 * Create complete path.
		 *
		 * Build entry ssd@xx,0:c,raw
		 * where xx is the AL_PA for sun4d or the WWN for
		 * all other architectures.
		 */
		select_id =
			g_sf_alpa_to_switch[map.sf_addr_pair[j].sf_al_pa];
		G_DPRINTF("  l_get_node_status: Searching loop map "
			"to find disk: ID:0x%x"
			" AL_PA:0x%x\n", select_id,
			state->ib_status.sel_id);

		/* Get the machine architecture. */
		if ((err = g_get_machineArch(&name_id)) != 0) {
			return (err);
		}
		if (name_id) {
			(void) sprintf(sbuf, "%x,0:c,raw", select_id);
		} else {
			(void) sprintf(sbuf,
			"w%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x"
			",0:c,raw",
			map.sf_addr_pair[j].sf_port_wwn[0],
			map.sf_addr_pair[j].sf_port_wwn[1],
			map.sf_addr_pair[j].sf_port_wwn[2],
			map.sf_addr_pair[j].sf_port_wwn[3],
			map.sf_addr_pair[j].sf_port_wwn[4],
			map.sf_addr_pair[j].sf_port_wwn[5],
			map.sf_addr_pair[j].sf_port_wwn[6],
			map.sf_addr_pair[j].sf_port_wwn[7]);
		}
		(void) strcat(temp_path, sbuf);

		/*
		 * If we find a device on this loop in this box
		 * update its status.
		 */
		if (state->ib_status.sel_id == select_id) {
			/*
			 * Found a device on this loop in this box.
			 *
			 * Update state.
			 */
			(void) sprintf(state->g_disk_state.node_wwn_s,
			"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			map.sf_addr_pair[j].sf_node_wwn[0],
			map.sf_addr_pair[j].sf_node_wwn[1],
			map.sf_addr_pair[j].sf_node_wwn[2],
			map.sf_addr_pair[j].sf_node_wwn[3],
			map.sf_addr_pair[j].sf_node_wwn[4],
			map.sf_addr_pair[j].sf_node_wwn[5],
			map.sf_addr_pair[j].sf_node_wwn[6],
			map.sf_addr_pair[j].sf_node_wwn[7]);

			(void) strcpy(state->g_disk_state.physical_path,
								temp_path);

			/* Bad if WWN is all zeros. */
			for (i = 0, not_null = 0; i < WWN_SIZE; i++) {
				if (map.sf_addr_pair[j].sf_node_wwn[i]) {
					not_null++;
					break;
				}
			}
			if (not_null == 0) {
				state->l_state_flag = L_INVALID_WWN;
				G_DPRINTF("  l_get_node_status: "
					"Disk state was "
					" Invalid WWN.\n");
				(*found_flag)++;
				return (0);
			}

			/* get device status */
			if (err = l_get_disk_status(temp_path, state,
				wwn_list, verbose))
				return (err);

			(*found_flag)++;
			break;
		}
	}

	return (0);

}


/*
 * Get the individual drives status for the device specified by the index.
 * device at the path where the path is of the IB and updates the
 * g_disk_state_struct structure.
 *
 * If the disk's port is bypassed,  it gets the
 * drive status such as node WWN from the second port.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
l_get_individual_state(char *path,
	struct l_disk_state_struct *state, Ib_state *ib_state,
	int front_flag, struct box_list_struct *box_list,
	struct wwn_list_struct *wwn_list, int verbose)
{
int		found_flag = 0, elem_index = 0;
int		port_a_flag, err, j;
struct dlist	*seslist = NULL;
Bp_elem_st	bpf, bpr;
hrtime_t	start_time, end_time;

	start_time = gethrtime();


	if ((state->ib_status.code != S_NOT_INSTALLED) &&
		(state->ib_status.code != S_NOT_AVAILABLE)) {

		/*
		 * Disk could have been bypassed on this loop.
		 * Check the port state before l_state_flag
		 * is set to L_INVALID_MAP.
		 */
		for (j = 0;
		j < (int)ib_state->config.enc_num_elem;
		j++) {
			elem_index++;
			if (ib_state->config.type_hdr[j].type ==
							ELM_TYP_BP)
				break;
			elem_index +=
				ib_state->config.type_hdr[j].num;
		}

		/*
		 * check if port A & B of backplane are bypassed.
		 * If so, do not bother.
		 */
		if (front_flag) {
			bcopy((const void *)
			&(ib_state->p2_s.element[elem_index]),
			(void *)&bpf, sizeof (bpf));

			if ((bpf.byp_a_enabled || bpf.en_bypass_a) &&
				(bpf.byp_b_enabled || bpf.en_bypass_b))
				return (0);
		} else {
			/* if disk is in rear slot */
			bcopy((const void *)
			&(ib_state->p2_s.element[elem_index+1]),
			(void *)&bpr, sizeof (bpr));

			if ((bpr.byp_b_enabled || bpr.en_bypass_b) &&
				(bpr.byp_a_enabled || bpr.en_bypass_a))
				return (0);
		}

		if ((err = l_get_node_status(path, state,
				&found_flag, wwn_list, verbose)) != 0)
			return (err);

		if (!found_flag) {
			if ((err = l_get_allses(path, box_list,
						&seslist, 0)) != 0) {
				return (err);
			}

			if (err = l_get_port(path, &port_a_flag, verbose))
				goto done;

			if (port_a_flag) {
				if ((state->ib_status.bypass_a_en &&
					!(state->ib_status.bypass_b_en)) ||
					!(state->ib_status.bypass_b_en)) {
					while (seslist != NULL && !found_flag) {
						if (err = l_get_port(
							seslist->dev_path,
						&port_a_flag, verbose)) {
							goto done;
						}
						if ((strcmp(seslist->dev_path,
							path) != 0) &&
							!port_a_flag) {
							*path = NULL;
							(void) strcpy(path,
							seslist->dev_path);
							if (err =
							l_get_node_status(path,
							state, &found_flag,
							wwn_list, verbose)) {
								goto done;
							}
						}
						seslist = seslist->next;
					}
				}
			} else {
				if ((state->ib_status.bypass_b_en &&
					!(state->ib_status.bypass_a_en)) ||
					!(state->ib_status.bypass_a_en)) {
					while (seslist != NULL && !found_flag) {
						if (err = l_get_port(
							seslist->dev_path,
						&port_a_flag, verbose)) {
							goto done;
						}
						if ((strcmp(seslist->dev_path,
						path) != 0) && port_a_flag) {
							*path = NULL;
							(void) strcpy(path,
							seslist->dev_path);
							if (err =
							l_get_node_status(path,
							state, &found_flag,
							wwn_list, verbose)) {
								goto done;
							}
						}
						seslist = seslist->next;
					}
				}
			}
			if (!found_flag) {
				state->l_state_flag = L_INVALID_MAP;
				G_DPRINTF("  l_get_individual_state: "
					"Disk state was "
					"Not in map.\n");
			} else {
				G_DPRINTF("  l_get_individual_state: "
					"Disk was found in the map.\n");
			}

			if (seslist != NULL)
				(void) g_free_multipath(seslist);

		}

	} else {
		G_DPRINTF("  l_get_individual_state: Disk state was %s.\n",
			(state->ib_status.code == S_NOT_INSTALLED) ?
			"Not Installed" : "Not Available");
	}

	if (getenv("_LUX_T_DEBUG") != NULL) {
		end_time = gethrtime();
		(void) fprintf(stdout, "    l_get_individual_state:"
		"\tTime = %lld millisec\n",
		(end_time - start_time)/1000000);
	}

	return (0);
done:
	(void) g_free_multipath(seslist);
	return (err);
}



/*
 * Get the global state of the photon.
 *
 * The path must be of the ses driver.
 * e.g.
 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0/ses@e,0:0
 * or
 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0/ses@WWN,0:0
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
l_get_status(char *path, struct l_state_struct *l_state, int verbose)
{
int		err = 0, i;
int		initial_update_flag = 1;
int		front_index, rear_index;
L_inquiry	inq;
uchar_t		node_wwn[WWN_SIZE], port_wwn[WWN_SIZE];
int		al_pa, found_front, found_rear, front_flag;
char		ses_path_front[MAXPATHLEN];
char		ses_path_rear[MAXPATHLEN];
Box_list	*b_list = NULL;
Box_list	*o_list = NULL;
char		node_wwn_s[(WWN_SIZE*2)+1];
uint_t		select_id;
hrtime_t	start_time, end_time;
WWN_list		*wwn_list = NULL;

	start_time = gethrtime();

	G_DPRINTF("  l_get_status: Get Status for enclosure at: "
		" %s\n", path);

	if (initial_update_flag) {
		/* initialization */
		(void) memset(l_state, 0, sizeof (struct l_state_struct));
	}

	if (err = g_get_inquiry(path, &inq)) {
		return (err);
	}
	if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) == 0) &&
		(!(strncmp((char *)inq.inq_vid, "SUN     ",
		sizeof (inq.inq_vid)) &&
		(inq.inq_dtype == DTYPE_ESI)))) {
		return (L_ENCL_INVALID_PATH);
	}

	(void) strncpy((char *)l_state->ib_tbl.enclosure_name,
		(char *)inq.inq_box_name, sizeof (inq.inq_box_name));

	/*
	 * Get all of the IB Receive Diagnostic pages.
	 */
	if (err = l_get_ib_status(path, l_state, verbose)) {
		return (err);
	}

	/*
	 * Get the total number of drives per box.
	 * This assumes front & rear are the same.
	 */
	l_state->total_num_drv = 0; /* default to use as a flag */
	for (i = 0; i < (int)l_state->ib_tbl.config.enc_num_elem; i++) {
		if (l_state->ib_tbl.config.type_hdr[i].type == ELM_TYP_DD) {
			if (l_state->total_num_drv) {
				if (l_state->total_num_drv !=
				(l_state->ib_tbl.config.type_hdr[i].num * 2)) {
					return (L_INVALID_NUM_DISKS_ENCL);
				}
			} else {
				l_state->total_num_drv =
				l_state->ib_tbl.config.type_hdr[i].num * 2;
			}
		}
	}

	/*
	 * transfer the individual drive Device Element information
	 * from IB state to drive state.
	 */
	if (err = l_get_disk_element_index(l_state, &front_index,
		&rear_index)) {
		return (err);
	}
	/* Skip global element */
	front_index++;
	rear_index++;
	for (i = 0; i < l_state->total_num_drv/2; i++) {
		bcopy((void *)&l_state->ib_tbl.p2_s.element[front_index + i],
		(void *)&l_state->drv_front[i].ib_status,
		(size_t)sizeof (struct device_element));
		bcopy((void *)&l_state->ib_tbl.p2_s.element[rear_index + i],
		(void *)&l_state->drv_rear[i].ib_status,
		(size_t)sizeof (struct device_element));
	}
	if (getenv("_LUX_G_DEBUG") != NULL) {
		g_dump("  l_get_status: disk elements:  ",
		(uchar_t *)&l_state->ib_tbl.p2_s.element[front_index],
		((sizeof (struct device_element)) * (l_state->total_num_drv)),
		HEX_ONLY);
	}

	/*
	 * Now get the individual devices information from
	 * the device itself.
	 *
	 * May need to use multiple paths to get to the
	 * front and rear drives in the box.
	 * If the loop is split some drives may not even be available
	 * from this host.
	 *
	 * The way this works is in the select ID the front disks
	 * are accessed via the IB with the bit 4 = 0
	 * and the rear disks by the IB with bit 4 = 1.
	 *
	 * First get device map from fc nexus driver for this loop.
	 */
	/*
	 * Get the boxes node WWN & al_pa for this path.
	 */
	if (err = g_get_wwn(path, port_wwn, node_wwn, &al_pa, verbose)) {
		return (err);
	}
	if (err = l_get_box_list(&o_list, verbose)) {
		(void) l_free_box_list(&o_list);
		return (err);	/* Failure */
	}

	found_front = found_rear = 0;
	for (i = 0; i < WWN_SIZE; i++) {
		(void) sprintf(&node_wwn_s[i << 1], "%02x", node_wwn[i]);
	}
	select_id = g_sf_alpa_to_switch[al_pa];
	l_state->ib_tbl.box_id = (select_id & BOX_ID_MASK) >> 5;

	G_DPRINTF("  l_get_status: Using this select_id 0x%x "
		"and node WWN %s\n",
		select_id, node_wwn_s);

	if (select_id & ALT_BOX_ID) {
		found_rear = 1;
		(void) strcpy(ses_path_rear, path);
		b_list = o_list;
		while (b_list) {
			if (strcmp(b_list->b_node_wwn_s, node_wwn_s) == 0) {
				if (err = g_get_wwn(b_list->b_physical_path,
					port_wwn, node_wwn,
					&al_pa, verbose)) {
					(void) l_free_box_list(&o_list);
					return (err);
				}
				select_id = g_sf_alpa_to_switch[al_pa];
				if (!(select_id & ALT_BOX_ID)) {
					(void) strcpy(ses_path_front,
					b_list->b_physical_path);
					found_front = 1;
					break;
				}
			}
			b_list = b_list->box_next;
		}
	} else {
		(void) strcpy(ses_path_front, path);
		found_front = 1;
		b_list = o_list;
		while (b_list) {
			if (strcmp(b_list->b_node_wwn_s, node_wwn_s) == 0) {
				if (err = g_get_wwn(b_list->b_physical_path,
					port_wwn, node_wwn,
					&al_pa, verbose)) {
					(void) l_free_box_list(&o_list);
					return (err);
				}
				select_id = g_sf_alpa_to_switch[al_pa];
				if (select_id & ALT_BOX_ID) {
					(void) strcpy(ses_path_rear,
					b_list->b_physical_path);
					found_rear = 1;
					break;
				}
			}
			b_list = b_list->box_next;
		}
	}

	if (getenv("_LUX_G_DEBUG") != NULL) {
		if (!found_front) {
		(void) printf("l_get_status: Loop to front disks not found.\n");
		}
		if (!found_rear) {
		(void) printf("l_get_status: Loop to rear disks not found.\n");
		}
	}

	/*
	 * Get path to all the FC disk and tape devices.
	 *
	 * I get this now and pass down for performance
	 * reasons.
	 * If for some reason the list can become invalid,
	 * i.e. device being offlined, then the list
	 * must be re-gotten.
	 */
	if (err = g_get_wwn_list(&wwn_list, verbose)) {
		return (err);   /* Failure */
	}

	if (found_front) {
		front_flag = 1;
		for (i = 0; i < l_state->total_num_drv/2; i++) {
			G_DPRINTF("  l_get_status: Getting individual"
				" State for front disk in slot %d\n", i);
			if (err = l_get_individual_state(ses_path_front,
			(struct l_disk_state_struct *)&l_state->drv_front[i],
			&l_state->ib_tbl, front_flag, o_list,
			wwn_list, verbose)) {
				(void) l_free_box_list(&o_list);
				(void) g_free_wwn_list(&wwn_list);
				return (err);
			}
		}
	} else {
		/* Set to loop not accessable. */
		for (i = 0; i < l_state->total_num_drv/2; i++) {
			l_state->drv_front[i].l_state_flag = L_NO_LOOP;
		}
	}
	if (found_rear) {
		front_flag = 0;
		for (i = 0; i < l_state->total_num_drv/2; i++) {
			G_DPRINTF("  l_get_status: Getting individual"
				" State for rear disk in slot %d\n", i);
			if (err = l_get_individual_state(ses_path_rear,
			(struct l_disk_state_struct *)&l_state->drv_rear[i],
			&l_state->ib_tbl, front_flag, o_list,
			wwn_list, verbose)) {
				(void) l_free_box_list(&o_list);
				(void) g_free_wwn_list(&wwn_list);
				return (err);
			}
		}
	} else {
		/* Set to loop not accessable. */
		for (i = 0; i < l_state->total_num_drv/2; i++) {
			l_state->drv_rear[i].l_state_flag = L_NO_LOOP;
		}
	}
	(void) l_free_box_list(&o_list);
	(void) g_free_wwn_list(&wwn_list);
	if (getenv("_LUX_T_DEBUG") != NULL) {
		end_time = gethrtime();
		(void) fprintf(stdout, "  l_get_status:   "
		"Time = %lld millisec\n",
		(end_time - start_time)/1000000);
	}

	return (0);
}



/*
 * Check the file for validity:
 *	- verify the size is that of 3 proms worth of text.
 *	- verify PROM_MAGIC.
 *	- verify (and print) the date.
 *	- verify the checksum.
 *	- verify the WWN == 0.
 * Since this requires reading the entire file, do it now and pass a pointer
 * to the allocated buffer back to the calling routine (which is responsible
 * for freeing it).  If the buffer is not allocated it will be NULL.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */

static int
check_file(int fd, int verbose, uchar_t **buf_ptr, int dl_info_offset)
{
struct	exec	the_exec;
int		temp, i, j, *p, size, *start;
uchar_t		*buf;
char		*date_str;
struct	dl_info	*dl_info;

	*buf_ptr = NULL;

	/* read exec header */
	if (lseek(fd, 0, SEEK_SET) == -1)
		return (errno);
	if ((temp = read(fd, (char *)&the_exec, sizeof (the_exec))) == -1) {
	    return (L_DWNLD_READ_HEADER_FAIL);
	}
	if (temp != sizeof (the_exec)) {
	    return (L_DWNLD_READ_INCORRECT_BYTES);
	}

	if (the_exec.a_text != PROMSIZE) {
	    return (L_DWNLD_INVALID_TEXT_SIZE);
	}

	if (!(buf = (uchar_t *)g_zalloc(PROMSIZE)))
	    return (L_MALLOC_FAILED);

	if ((temp = read(fd, buf, PROMSIZE)) == -1) {
	    return (L_DWNLD_READ_ERROR);
	}

	if (temp != PROMSIZE) {
	    return (L_DWNLD_READ_INCORRECT_BYTES);
	}



	/* check the IB firmware MAGIC */
	dl_info = (struct dl_info *)((int)buf + dl_info_offset);
	if (dl_info->magic != PROM_MAGIC) {
		return (L_DWNLD_BAD_FRMWARE);
	}

	/*
	* Get the date
	*/

	date_str = ctime((const long *)&dl_info->datecode);

	if (verbose) {
		(void) fprintf(stdout,
		MSGSTR(9050, "  IB Prom Date: %s"),
		date_str);
	}

	/*
	 * verify checksum
	*/

	if (dl_info_offset == FPM_DL_INFO) {
		start = (int *)((int)buf + FPM_OFFSET);
		size = FPM_SZ;
	} else {
		start = (int *)(int)buf;
		size = TEXT_SZ + IDATA_SZ;
	}

	for (j = 0, p = start, i = 0; i < (size/ 4); i++, j ^= *p++);

	if (j != 0) {
		return (L_DWNLD_CHKSUM_FAILED);
	}

	/* file verified */
	*buf_ptr = buf;

	return (0);
}



int
l_check_file(char *file, int verbose)
{
int	file_fd;
int	err;
uchar_t	*buf;

	if ((file_fd = g_object_open(file, O_RDONLY)) == -1) {
	    return (L_OPEN_PATH_FAIL);
	}
	err = check_file(file_fd, verbose, &buf, FW_DL_INFO);
	if (buf)
		(void) g_destroy_data((char *)buf);
	return (err);
}



/*
 * Write buffer command set up to download
 * firmware to the Photin IB.
 *
 * RETURNS:
 *	status
 */
static int
ib_download_code_cmd(int fd, int promid, int off, uchar_t *buf_ptr,
						int buf_len, int sp)
{
int	status, sz;

	while (buf_len) {
		sz = MIN(256, buf_len);
		buf_len -= sz;
		status = g_scsi_writebuffer_cmd(fd, off, buf_ptr, sz,
						(sp) ? 3 : 2, promid);
		if (status)
			return (status);
		buf_ptr += sz;
		off += sz;
	}

	return (status);
}



/*
 * Downloads the new prom image to IB.
 *
 * INPUTS:
 * 	path		- physical path of Photon SES card
 * 	file		- input file for new code (may be NULL)
 * 	ps		- whether the "save" bit should be set
 * 	verbose		- to be verbose or not
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
l_download(char *path_phys, char *file, int ps, int verbose)
{
int		file_fd, controller_fd;
int		err, status;
uchar_t		*buf_ptr;
char		printbuf[MAXPATHLEN];
int		retry;
char		file_path[MAXPATHLEN];

	if (!file) {
		(void) strcpy(file_path, IBFIRMWARE_FILE);
	} else {
		(void) strncpy(file_path, file, sizeof (file_path));
	}
	if (verbose)
		(void) fprintf(stdout, "%s\n",
			MSGSTR(9051, "  Opening the IB for I/O."));

	if ((controller_fd = g_object_open(path_phys, O_NDELAY | O_RDWR)) == -1)
		return (L_OPEN_PATH_FAIL);

	(void) sprintf(printbuf, MSGSTR(9052, "  Doing download to:"
			"\n\t%s.\n  From file: %s."), path_phys, file_path);

	if (verbose)
		(void) fprintf(stdout, "%s\n", printbuf);
	P_DPRINTF("  Doing download to:"
			"\n\t%s\n  From file: %s\n", path_phys, file_path);

	if ((file_fd = g_object_open(file_path, O_NDELAY | O_RDONLY)) == -1) {
		/*
		 * Return a different error code here to differentiate between
		 * this failure in g_object_open() and the one above.
		 */
		return (L_INVALID_PATH);
	}
	if (err = check_file(file_fd, verbose, &buf_ptr, FW_DL_INFO)) {
		if (buf_ptr)
			(void) g_destroy_data((char *)buf_ptr);
		return (err);
	}
	if (verbose) {
		(void) fprintf(stdout, "  ");
		(void) fprintf(stdout, MSGSTR(127, "Checkfile O.K."));
		(void) fprintf(stdout, "\n");
	}
	P_DPRINTF("  Checkfile OK.\n");
	(void) close(file_fd);

	if (verbose) {
		(void) fprintf(stdout, MSGSTR(9053,
			"  Verifying the IB is available.\n"));
	}

	retry = DOWNLOAD_RETRIES;
	while (retry) {
		if ((status = g_scsi_tur(controller_fd)) == 0) {
			break;
		} else {
			if ((retry % 30) == 0) {
				ER_DPRINTF(" Waiting for the IB to be"
						" available.\n");
			}
			(void) sleep(1);
		}
	}
	if (!retry) {
		if (buf_ptr)
			(void) g_destroy_data((char *)buf_ptr);
		(void) close(controller_fd);
		return (status);
	}

	if (verbose)
		(void) fprintf(stdout, "%s\n",
			MSGSTR(9054, "  Writing new text image to IB."));
	P_DPRINTF("  Writing new image to IB\n");
	status = ib_download_code_cmd(controller_fd, IBEEPROM, TEXT_OFFSET,
		(uchar_t *)((int)buf_ptr + (int)TEXT_OFFSET), TEXT_SZ, ps);
	if (status) {
		(void) close(controller_fd);
		(void) g_destroy_data((char *)buf_ptr);
		return (status);
	}
	if (verbose)
		(void) fprintf(stdout, "%s\n",
			MSGSTR(9055, "  Writing new data image to IB."));
	status = ib_download_code_cmd(controller_fd,
		IBEEPROM, IDATA_OFFSET,
		(uchar_t *)((int)buf_ptr + (int)IDATA_OFFSET), IDATA_SZ, ps);
	if (status) {
		(void) close(controller_fd);
		(void) g_destroy_data((char *)buf_ptr);
		return (status);
	}

	if (verbose) {
		(void) fprintf(stdout, MSGSTR(9056,
			"  Re-verifying the IB is available.\n"));
	}

	retry = DOWNLOAD_RETRIES;
	while (retry) {
		if ((status = g_scsi_tur(controller_fd)) == 0) {
			break;
		} else {
			if ((retry % 30) == 0) {
				ER_DPRINTF("  Waiting for the IB to be"
					" available.\n");
			}
			(void) sleep(1);
		}
		retry--;
	}
	if (!retry) {
		(void) close(controller_fd);
		(void) g_destroy_data((char *)buf_ptr);
		return (L_DWNLD_TIMED_OUT);
	}

	if (verbose) {
		(void) fprintf(stdout, "%s\n",
			MSGSTR(9057, "  Writing new image to FPM."));
	}
	status = ib_download_code_cmd(controller_fd, MBEEPROM, FPM_OFFSET,
		(uchar_t *)((int)buf_ptr + FPM_OFFSET), FPM_SZ, ps);
	(void) g_destroy_data((char *)buf_ptr);

	if ((!status) && ps) {
		/*
		 * Reset the IB
		 */
		status = g_scsi_reset(controller_fd);
	}

	(void) close(controller_fd);
	return (status);
}

/*
 * Set the World Wide Name
 * in page 4 of the Send Diagnostic command.
 *
 * Is it allowed to change the wwn ???
 * The path must point to an IB.
 *
 */
int
l_set_wwn(char *path_phys, char *wwn)
{
Page4_name	page4;
L_inquiry	inq;
int		fd, status;
char		wwnp[WWN_SIZE];

	(void) memset(&inq, 0, sizeof (inq));
	(void) memset(&page4, 0, sizeof (page4));

	if ((fd = g_object_open(path_phys, O_NDELAY | O_RDONLY)) == -1) {
		return (L_OPEN_PATH_FAIL);
	}
	/* Verify it is a Photon */
	if (status = g_scsi_inquiry_cmd(fd,
		(uchar_t *)&inq, sizeof (struct l_inquiry_struct))) {
		(void) close(fd);
		return (status);
	}
	if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) == 0) &&
		(!(strncmp((char *)inq.inq_vid, "SUN     ",
		sizeof (inq.inq_vid)) &&
		(inq.inq_dtype == DTYPE_ESI)))) {
		(void) close(fd);
		return (L_ENCL_INVALID_PATH);
	}

	page4.page_code = L_PAGE_4;
	page4.page_len = (ushort_t)((sizeof (struct page4_name) - 4));
	page4.string_code = L_WWN;
	page4.enable = 1;
	if (g_string_to_wwn((uchar_t *)wwn, (uchar_t *)&page4.name)) {
		close(fd);
		return (EINVAL);
	}
	bcopy((void *)wwnp, (void *)page4.name, (size_t)WWN_SIZE);

	if (status = g_scsi_send_diag_cmd(fd, (uchar_t *)&page4,
		sizeof (page4))) {
		(void) close(fd);
		return (status);
	}

	/*
	 * Check the wwn really changed.
	 */
	bzero((char *)page4.name, 32);
	if (status = g_scsi_rec_diag_cmd(fd, (uchar_t *)&page4,
				sizeof (page4), L_PAGE_4)) {
		(void) close(fd);
		return (status);
	}
	if (bcmp((char *)page4.name, wwnp, WWN_SIZE)) {
		(void) close(fd);
		return (L_WARNING);
	}

	(void) close(fd);
	return (0);
}



/*
 * Use a physical path to a disk in a Photon box
 * as the base to genererate a path to a SES
 * card in this box.
 *
 * path_phys: Physical path to a Photon disk.
 * ses_path:  This must be a pointer to an already allocated path string.
 *
 * RETURNS:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
l_get_ses_path(char *path_phys, char *ses_path, sf_al_map_t *map,
	int verbose)
{
char	*char_ptr, *ptr, id_buf[MAXPATHLEN], wwn[20];
uchar_t	t_wwn[20], *ses_wwn, *ses_wwn1;
int	j, al_pa, al_pa1, box_id, nu, fd, disk_flag = 0;
int	err, found = 0, name_id;

	(void) strcpy(ses_path, path_phys);
	if ((char_ptr = strrchr(ses_path, '/')) == NULL) {
			return (L_INVLD_PATH_NO_SLASH_FND);
	}
	disk_flag++;
	*char_ptr = '\0';   /* Terminate sting  */
	(void) strcat(ses_path, SLSH_SES_NAME);

	/*
	 * Figure out and create the boxes pathname.
	 *
	 * NOTE: This uses the fact that the disks's
	 * AL_PA and the boxes AL_PA must match
	 * the assigned hard address in the current
	 * implementations. This may not be true in the
	 * future.
	 */
	if ((char_ptr = strrchr(path_phys, '@')) == NULL) {
		return (L_INVLD_PATH_NO_ATSIGN_FND);
	}
	char_ptr++;	/* point to the loop identifier */

	if ((err = g_get_machineArch(&name_id)) != 0) {
		return (err);
	}
	if (name_id) {
		nu = strtol(char_ptr, &ptr, 16);
		if (ptr == char_ptr) {
			return (L_INVALID_PATH);
		}
		if ((nu > 0x7e) || (nu < 0)) {
			return (L_INVLD_ID_FOUND);
		}
		/*
		 * Mask out all but the box ID.
		 */
		nu &= BOX_ID_MASK;
		/*
		 * Or in the IB address.
		 */
		nu |= BOX_ID;

		(void) sprintf(id_buf, "%x,0:0", nu);
	} else {
		if ((err = g_get_wwn(path_phys, t_wwn, t_wwn,
						&al_pa, verbose)) != 0) {
			return (err);
		}
		box_id = g_sf_alpa_to_switch[al_pa] & BOX_ID_MASK;
		for (j = 0; j < map->sf_count; j++) {
			if (map->sf_addr_pair[j].sf_inq_dtype == DTYPE_ESI) {
				al_pa1 = map->sf_addr_pair[j].sf_al_pa;
				if (box_id == (g_sf_alpa_to_switch[al_pa1] &
						BOX_ID_MASK)) {
					if (!found) {
						ses_wwn =
					map->sf_addr_pair[j].sf_port_wwn;
						if (getenv("_LUX_P_DEBUG")) {
						(void) g_ll_to_str(ses_wwn,
							(char *)t_wwn);
							(void) printf(
							"  l_get_ses_path: "
							"Found ses wwn = %s "
							"al_pa 0x%x\n",
							t_wwn, al_pa1);
						}
					} else {
						ses_wwn1 =
					map->sf_addr_pair[j].sf_port_wwn;
						if (getenv("_LUX_P_DEBUG")) {
						(void) g_ll_to_str(ses_wwn1,
							(char *)t_wwn);
							(void) printf(
							"  l_get_ses_path: "
							"Found second ses "
							"wwn = %s "
							"al_pa 0x%x\n",
							t_wwn, al_pa1);
						}
					}
					found++;
				}
			}
		}
		if (!found) {
			return (L_INVALID_PATH);
		}
		(void) g_ll_to_str(ses_wwn, wwn);
		(void) sprintf(id_buf, "w%s,0:0", wwn);
	}
	(void) strcat(ses_path, id_buf);
	if (verbose) {
		(void) fprintf(stdout,
			MSGSTR(9058, "  Creating enclosure path:\n    %s\n"),
			ses_path);
	}

	/*
	 * see if these paths exist.
	 */
	if ((fd = g_object_open(ses_path, O_NDELAY | O_RDONLY)) == -1) {
		char_ptr = strrchr(ses_path, '/');
		*char_ptr = '\0';
		(void) strcat(ses_path, SLSH_SES_NAME);
		if (name_id) {
			nu |= 0x10;	/* add alternate IB address bit */
			(void) sprintf(id_buf, "%x,0:0", nu);
			(void) strcat(ses_path, id_buf);
			return (0);
		} else {
			if (found > 1) {
				(void) g_ll_to_str(ses_wwn1, wwn);
				P_DPRINTF("  l_get_ses_path: "
					"Using second path, ses wwn1 = %s\n",
					wwn);
				(void) sprintf(id_buf, "w%s,0:0", wwn);
				strcat(ses_path, id_buf);
				return (0);
			} else {
				return (L_INVALID_PATH);
			}
		}
	}
	close(fd);
	return (0);
}



/*
 * Get a valid location, front/rear & slot.
 *
 * path_struct->p_physical_path must be of a disk.
 *
 * OUTPUT: path_struct->slot_valid
 *	path_struct->slot
 *	path_struct->f_flag
 *
 * RETURN:
 *	0	 O.K.
 *	non-zero otherwise
 */
int
l_get_slot(struct path_struct *path_struct, L_state *l_state, int verbose)
{
int		err, al_pa, slot, found = 0;
uchar_t		node_wwn[8], port_wwn[8];
uint_t		select_id;

	/* Double check to see if we need to calculate. */
	if (path_struct->slot_valid)
		return (0);

	/* Programming error if this occures */
	assert(path_struct->ib_path_flag == NULL);

	if ((strstr(path_struct->p_physical_path, "ssd")) == NULL) {
		return (L_INVLD_PHYS_PATH_TO_DISK);
	}
	if (err = g_get_wwn(path_struct->p_physical_path, port_wwn, node_wwn,
		&al_pa, verbose)) {
		return (err);
	}

	/*
	 * Find the slot by searching for the matching hard address.
	 */
	select_id = g_sf_alpa_to_switch[al_pa];
	P_DPRINTF("  l_get_slot: Searching Receive Diagnostic page 2, "
		"to find the slot number with this ID:0x%x\n",
		select_id);

	for (slot = 0; slot < l_state->total_num_drv/2; slot++) {
		if (l_state->drv_front[slot].ib_status.sel_id ==
			select_id) {
			path_struct->f_flag = 1;
			found = 1;
			break;
		} else if (l_state->drv_rear[slot].ib_status.sel_id ==
			select_id) {
			path_struct->f_flag = 0;
			found = 1;
			break;
		}
	}
	if (!found) {
		return (L_INVALID_SLOT);	/* Failure */
	}
	P_DPRINTF("  l_get_slot: Found slot %d %s.\n", slot,
		path_struct->f_flag ? "Front" : "Rear");
	path_struct->slot = slot;
	path_struct->slot_valid = 1;
	return (0);
}


void
l_element_msg_string(uchar_t code, char *es)
{
	if (code == S_OK) {
		(void) sprintf(es, MSGSTR(29, "O.K."));
	} else if (code == S_NOT_AVAILABLE) {
		(void) sprintf(es, MSGSTR(34, "Disabled"));
	} else if (code == S_NOT_INSTALLED) {
		(void) sprintf(es, MSGSTR(30, "Not Installed"));
	} else if (code == S_NONCRITICAL) {
		(void) sprintf(es, MSGSTR(9059, "Noncritical failure"));
	} else if (code == S_CRITICAL) {
		(void) sprintf(es, MSGSTR(122, "Critical failure"));
	} else {
		(void) sprintf(es, MSGSTR(4, "Unknown status"));
	}
}


/*
 * Get all ses paths paths to a given box.
 * The arg should be the physical path to one of the box's IB.
 * NOTE: The caller must free the allocated lists.
 *
 * OUTPUT:
 *	a pointer to a list of ses paths if found
 *	NULL on error.
 *
 * RETURNS:
 *	0	 if O.K.
 *	non-zero otherwise
 */
int
l_get_allses(char *path, struct box_list_struct *box_list,
			struct dlist **ses_list, int verbose)
{
struct box_list_struct 	*box_list_ptr;
char			node_wwn_s[WWN_S_LEN];
struct dlist		*dlt, *dl;

	/* Initialize lists/arrays */
	*ses_list = dlt = dl = (struct dlist *)NULL;
	node_wwn_s[0] = '\0';

	H_DPRINTF("  l_get_allses: Looking for all ses paths for"
		" box at path: %s\n", path);

	for (box_list_ptr = box_list; box_list_ptr != NULL;
				box_list_ptr = box_list_ptr->box_next) {
		H_DPRINTF("  l_get_allses: physical_path= %s\n",
				box_list_ptr->b_physical_path);
		if (strcmp(path, box_list_ptr->b_physical_path) == 0) {
			(void) strcpy(node_wwn_s, box_list_ptr->b_node_wwn_s);
			break;
		}
	}
	if (node_wwn_s[0] == '\0') {
		H_DPRINTF("node_wwn_s is NULL!\n");
		return (L_NO_NODE_WWN_IN_BOXLIST);
	}
	H_DPRINTF("  l_get_allses: node_wwn=%s\n", node_wwn_s);
	for (box_list_ptr = box_list; box_list_ptr != NULL;
				box_list_ptr = box_list_ptr->box_next) {
		if (strcmp(node_wwn_s, box_list_ptr->b_node_wwn_s) == 0) {
			if ((dl = (struct dlist *)
				g_zalloc(sizeof (struct dlist))) == NULL) {
				while (*ses_list != NULL) {
					dl = dlt->next;
					(void) g_destroy_data(dlt);
					dlt = dl;
				}
				return (L_MALLOC_FAILED);
			}
			H_DPRINTF("  l_get_allses: Found ses=%s\n",
					box_list_ptr->b_physical_path);
			dl->dev_path = strdup(box_list_ptr->b_physical_path);
			dl->logical_path = strdup(box_list_ptr->logical_path);
			if (*ses_list == NULL) {
				*ses_list = dlt = dl;
			} else {
				dlt->next = dl;
				dl->prev = dlt;
				dlt = dl;
			}
		}
	}

	return (0);
}
