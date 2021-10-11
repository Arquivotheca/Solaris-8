/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)diag.c	1.9	99/05/14 SMI"

/*LINTLIBRARY*/


/*
 *	This module is part of the photon library
 */

/*
 * I18N message number ranges
 *  This file: 8000 - 8499
 *  Shared common messages: 1 - 1999
 */

/*	Includes	*/
#include	<stdlib.h>
#include	<stdio.h>
#include	<sys/file.h>
#include	<sys/errno.h>
#include	<sys/types.h>
#include	<sys/param.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<errno.h>
#include	<string.h>
#include	<sys/scsi/scsi.h>
#include	<nl_types.h>
#include	<strings.h>
#include	<sys/ddi.h>	/* for max */
#include	<l_common.h>
#include	<stgcom.h>
#include	<l_error.h>
#include	<a_state.h>
#include	<a5k.h>



/*	Defines		*/
#define	VERBPRINT	if (verbose) (void) printf


/*
 * take all paths supplied by dl offline.
 *
 * RETURNS:
 *	0 = No error.
 *	*bsy_res_flag_p: 1 = The device is "busy".
 *
 * In pre-2.6 we just return success
 */
static int
d_offline_drive(struct dlist *dl, int *bsy_res_flag_p, int verbose)
{
char			dev_path1[MAXPATHLEN];
devctl_hdl_t		devhdl;


	/* for each path attempt to take it offline */
	for (; dl != NULL; dl = dl->next) {

		/* save a copy of the pathname */
		(void) strcpy(dev_path1, dl->dev_path);

		/* attempt to acquire the device */
#ifdef	TWO_SIX
		if (devctl_acquire(dev_path1, DC_EXCL, &devhdl) != 0) {
#else
		if ((devhdl = devctl_device_acquire(dev_path1,
			DC_EXCL)) == NULL) {
#endif
			if (errno != EBUSY) {
				return (L_ACQUIRE_FAIL);
			}
		}

		/* attempt to offline the drive */
		if (devctl_device_offline(devhdl) != 0) {
			*bsy_res_flag_p = 1;
			(void) devctl_release(devhdl);
			return (0);
		}

		E_DPRINTF("  d_offline_drive: Offline succeeded:/n    "
			"%s\n", dev_path1);
		/* offline succeeded -- release handle acquired above */
		(void) devctl_release(devhdl);
	}
	return (0);
}




/*
 * Check to see if any of the disks that are attached
 * to the selected port on this backplane are reserved or busy.
 *
 * INPUTS:
 * RETURNS:
 *	0 = No error.
 *	*bsy_res_flag_p: 1 = The device is "busy".
 */

int
l_check_busy_reserv_bp(char *ses_path, int front_flag,
	int port_a_flag, int *bsy_res_flag_p, int verbose)
{
int	err, i;
L_state		*l_state = NULL;
struct dlist	*p_list;

	if ((l_state = (L_state *)calloc(1, sizeof (L_state))) == NULL) {
		return (L_MALLOC_FAILED);
	}

	if (err = l_get_status(ses_path, l_state, verbose)) {
		(void) l_free_lstate(&l_state);
		return (err);
	}
	for (i = 0; i < (int)l_state->total_num_drv/2; i++) {
		if ((front_flag &&
	(l_state->drv_front[i].g_disk_state.d_state_flags[port_a_flag] &
			L_RESERVED)) || (!front_flag &&
	(l_state->drv_rear[i].g_disk_state.d_state_flags[port_a_flag] &
			L_RESERVED))) {
			*bsy_res_flag_p = 1;
			(void) l_free_lstate(&l_state);
			return (0);
		}
	}

	for (i = 0; i < (int)l_state->total_num_drv/2; i++) {
		/* Get list of all paths to the requested port. */
		if (front_flag) {
			if (port_a_flag) {
				if ((err = g_get_port_multipath(
				l_state->drv_front[i].g_disk_state.port_a_wwn_s,
					&p_list, verbose)) != 0) {
					(void) l_free_lstate(&l_state);
					return (err);
				}
			} else {
				if ((err = g_get_port_multipath(
				l_state->drv_front[i].g_disk_state.port_b_wwn_s,
					&p_list, verbose)) != 0) {
					(void) l_free_lstate(&l_state);
					return (err);
				}
			}
		} else {
			if (port_a_flag) {
				if ((err = g_get_port_multipath(
				l_state->drv_rear[i].g_disk_state.port_a_wwn_s,
					&p_list, verbose)) != 0) {
					(void) l_free_lstate(&l_state);
					return (err);
				}
			} else {
				if ((err = g_get_port_multipath(
				l_state->drv_rear[i].g_disk_state.port_b_wwn_s,
					&p_list, verbose)) != 0) {
					(void) l_free_lstate(&l_state);
					return (err);
				}
			}
		}
		if (err = d_offline_drive(p_list,
			bsy_res_flag_p, verbose)) {
			(void) g_free_multipath(p_list);
			(void) l_free_lstate(&l_state);
			return (err);
		}
		(void) g_free_multipath(p_list);
	}
	(void) l_free_lstate(&l_state);
	return (0);
}



/*
 * Request the enclosure services controller (IB)
 * to set the LRC (Loop Redundancy Circuit) to the
 * bypassed/enabled state for the backplane specified by
 * the a and f flag and the enclosure or pathname.
 */
int
d_bp_bypass_enable(char *ses_path, int bypass_flag, int port_a_flag,
	int front_flag, int force_flag, int verbose)
{

int		fd, i;
int		nobj = 0;
ses_objarg	obj;
ses_object	*all_objp = NULL;
int		found = 0;
Bp_elem_st	*bp;
char		msg[MAXPATHLEN];
int		bsy_res_flag = 0;
int		err;

	/*
	 * Check for reservation and busy for all disks on this
	 * backplane.
	 */

	if (!force_flag && bypass_flag) {
		if (err = l_check_busy_reserv_bp(ses_path,
			front_flag, port_a_flag, &bsy_res_flag, verbose)) {
			return (err);
		}
		if (bsy_res_flag) {
				return (L_BP_BUSY_RESERVED);
		}
	}


	if ((fd = g_object_open(ses_path, O_NDELAY | O_RDWR)) == -1) {
		return (errno);
	}

	if (ioctl(fd, SESIOC_GETNOBJ, (caddr_t)&nobj) < 0) {
		(void) close(fd);
		return (errno);
	}
	if (nobj == 0) {
		(void) close(fd);
		return (L_IB_NO_ELEM_FOUND);
	}

	E_DPRINTF("  l_ib_bypass_bp: Number of SES objects: 0x%x\n",
		nobj);

	/* alloc some memory for the objmap */
	if ((all_objp = g_zalloc((nobj + 1) * sizeof (ses_object))) == NULL) {
		(void) close(fd);
		return (errno);
	}

	if (ioctl(fd, SESIOC_GETOBJMAP, (caddr_t)all_objp) < 0) {
		(void) close(fd);
		(void) g_destroy_data(all_objp);
		return (errno);
	}

	for (i = 0; i < nobj; i++, all_objp++) {
			E_DPRINTF("  ID 0x%x\t Element type 0x%x\n",
			all_objp->obj_id, all_objp->elem_type);
		if (all_objp->elem_type == ELM_TYP_BP) {
			found++;
			break;
		}
	}

	if (found == 0) {
		(void) close(fd);
		(void) g_destroy_data(all_objp);
		return (L_NO_BP_ELEM_FOUND);
	}

	/*
	 * We found the backplane element.
	 */


	if (verbose) {
		/* Get the status for backplane #0 */
		obj.obj_id = all_objp->obj_id;
		if (ioctl(fd, SESIOC_GETOBJSTAT, (caddr_t)&obj) < 0) {
			(void) close(fd);
			(void) g_destroy_data(all_objp);
			return (errno);
		}
		(void) fprintf(stdout, MSGSTR(8000,
			"  Front backplane status: "));
		bp = (struct  bp_element_status *)&obj.cstat[0];
		l_element_msg_string(bp->code, msg);
		(void) fprintf(stdout, "%s\n", msg);
		if (bp->byp_a_enabled || bp->en_bypass_a) {
			(void) fprintf(stdout, "    ");
			(void) fprintf(stdout,
			MSGSTR(130, "Bypass A enabled"));
			(void) fprintf(stdout, ".\n");
		}
		if (bp->byp_b_enabled || bp->en_bypass_b) {
			(void) fprintf(stdout, "    ");
			(void) fprintf(stdout,
			MSGSTR(129, "Bypass B enabled"));
			(void) fprintf(stdout, ".\n");
		}

		all_objp++;
		obj.obj_id = all_objp->obj_id;
		all_objp--;
		if (ioctl(fd, SESIOC_GETOBJSTAT, (caddr_t)&obj) < 0) {
			(void) close(fd);
			(void) g_destroy_data(all_objp);
			return (errno);
		}
		(void) fprintf(stdout, MSGSTR(8001,
			"  Rear backplane status: "));
		bp = (struct  bp_element_status *)&obj.cstat[0];
		l_element_msg_string(bp->code, msg);
		(void) fprintf(stdout, "%s\n", msg);
		if (bp->byp_a_enabled || bp->en_bypass_a) {
			(void) fprintf(stdout, "    ");
			(void) fprintf(stdout,
			MSGSTR(130, "Bypass A enabled"));
			(void) fprintf(stdout, ".\n");
		}
		if (bp->byp_b_enabled || bp->en_bypass_b) {
			(void) fprintf(stdout, "    ");
			(void) fprintf(stdout,
			MSGSTR(129, "Bypass B enabled"));
			(void) fprintf(stdout, ".\n");
		}
	}

	/* Get the current status */
	if (!front_flag) {
		all_objp++;
	}
	obj.obj_id = all_objp->obj_id;
	if (ioctl(fd, SESIOC_GETOBJSTAT, (caddr_t)&obj) < 0) {
		(void) close(fd);
		(void) g_destroy_data(all_objp);
		return (errno);
	}
	/* Do the requested action. */
	bp = (struct  bp_element_status *)&obj.cstat[0];
	bp->select = 1;
	bp->code = 0;
	if (port_a_flag) {
		bp->en_bypass_a = bypass_flag;
	} else {
		bp->en_bypass_b = bypass_flag;
	}
	if (getenv("_LUX_E_DEBUG") != NULL) {
		(void) printf("  Sending this structure to ID 0x%x"
			" of type 0x%x\n",
			obj.obj_id, all_objp->elem_type);
		for (i = 0; i < 4; i++) {
			(void) printf("    Byte %d  0x%x\n", i,
			obj.cstat[i]);
		}
	}

	if (ioctl(fd, SESIOC_SETOBJSTAT, (caddr_t)&obj) < 0) {
		(void) close(fd);
		(void) g_destroy_data(all_objp);
		return (errno);
	}

	(void) g_destroy_data(all_objp);
	(void) close(fd);

	return (0);
}




/*
 * This function will request the enclosure services
 * controller (IB) to set the LRC (Loop Redundancy Circuit) to the
 * bypassed/enabled state for the device specified by the
 * enclosure,dev or pathname and the port specified by the a
 * flag.
 */

int
d_dev_bypass_enable(struct path_struct *path_struct, int bypass_flag,
	int force_flag, int port_a_flag, int verbose)
{
sf_al_map_t		map;
char			ses_path[MAXPATHLEN];
uchar_t			*page_buf;
int 			err, fd, front_index, rear_index, offset;
unsigned short		page_len;
struct	device_element 	*elem;
L_state			*l_state = NULL;
struct device_element 	status;
int			bsy_flag = 0, i, f_flag;
struct dlist		*p_list;

	if ((l_state = (L_state *)calloc(1, sizeof (L_state))) == NULL) {
		return (L_MALLOC_FAILED);
	}

	/*
	 * Need to get a valid location, front/rear & slot.
	 *
	 * The path_struct will return a valid slot
	 * and the IB path or a disk path.
	 */

	if (!path_struct->ib_path_flag) {
		if (err = g_get_dev_map(path_struct->p_physical_path,
			&map, verbose)) {
			(void) l_free_lstate(&l_state);
			return (err);
		}
		if (err = l_get_ses_path(path_struct->p_physical_path,
			ses_path, &map, verbose)) {
			(void) l_free_lstate(&l_state);
			return (err);
		}
	} else {
		(void) strcpy(ses_path, path_struct->p_physical_path);
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
		if ((err = l_get_slot(path_struct, l_state, verbose)) != 0) {
			(void) l_free_lstate(&l_state);
			return (err);
		}
	}

	if ((page_buf = (uchar_t *)malloc(MAX_REC_DIAG_LENGTH)) == NULL) {
		(void) l_free_lstate(&l_state);
		return (errno);
	}

	if ((fd = g_object_open(ses_path, O_NDELAY | O_RDWR)) == -1) {
		(void) g_destroy_data(page_buf);
		(void) l_free_lstate(&l_state);
		return (errno);
	}

	if (err = l_get_envsen_page(fd, page_buf, MAX_REC_DIAG_LENGTH,
				L_PAGE_2, verbose)) {
		(void) close(fd);
		(void) g_destroy_data(page_buf);
		(void) l_free_lstate(&l_state);
		return (err);
	}

	page_len = (page_buf[2] << 8 | page_buf[3]) + HEADER_LEN;

	/* Get index to the disk we are interested in */
	if (err = l_get_status(ses_path, l_state, verbose)) {
		(void) close(fd);
		(void) g_destroy_data(page_buf);
		(void) l_free_lstate(&l_state);
		return (err);
	}
	/*
	 * Now that we have the status check to see if
	 * busy or reserved, if bypassing.
	 */
	if ((!(force_flag | path_struct->ib_path_flag)) &&
						bypass_flag) {
		i = path_struct->slot;
		f_flag = path_struct->f_flag;

		/*
		 * Check for reservation and busy
		 */
		if ((f_flag &&
		(l_state->drv_front[i].g_disk_state.d_state_flags[port_a_flag] &
			L_RESERVED)) || (!f_flag &&
		(l_state->drv_rear[i].g_disk_state.d_state_flags[port_a_flag] &
			L_RESERVED))) {
			(void) close(fd);
			(void) g_destroy_data(page_buf);
			(void) l_free_lstate(&l_state);
			return (L_BP_RESERVED);
		}
		if (f_flag) {
			if (port_a_flag) {
				if ((err = g_get_port_multipath(
				l_state->drv_front[i].g_disk_state.port_a_wwn_s,
					&p_list, verbose)) != 0) {
					(void) close(fd);
					(void) g_destroy_data(page_buf);
					(void) l_free_lstate(&l_state);
					return (err);
				}
			} else {
				if ((err = g_get_port_multipath(
				l_state->drv_front[i].g_disk_state.port_b_wwn_s,
					&p_list, verbose)) != 0) {
					(void) close(fd);
					(void) g_destroy_data(page_buf);
					(void) l_free_lstate(&l_state);
					return (err);
				}
			}
		} else {
			if (port_a_flag) {
				if ((err = g_get_port_multipath(
				l_state->drv_rear[i].g_disk_state.port_a_wwn_s,
					&p_list, verbose)) != 0) {
					(void) close(fd);
					(void) g_destroy_data(page_buf);
					(void) l_free_lstate(&l_state);
					return (err);
				}
			} else {
				if ((err = g_get_port_multipath(
				l_state->drv_rear[i].g_disk_state.port_b_wwn_s,
					&p_list, verbose)) != 0) {
					(void) close(fd);
					(void) g_destroy_data(page_buf);
					(void) l_free_lstate(&l_state);
					return (err);
				}
			}
		}
		if (err = d_offline_drive(p_list,
			&bsy_flag, verbose)) {
			(void) g_free_multipath(p_list);
			(void) close(fd);
			(void) g_destroy_data(page_buf);
			(void) l_free_lstate(&l_state);
			return (err);
		}
		(void) g_free_multipath(p_list);
		if (bsy_flag) {
			(void) close(fd);
			(void) g_destroy_data(page_buf);
			(void) l_free_lstate(&l_state);
			return (L_BP_BUSY);
		}
	}

	if (err = l_get_disk_element_index(l_state, &front_index,
		&rear_index)) {
		(void) close(fd);
		(void) g_destroy_data(page_buf);
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
	bcopy((const void *)elem, (void *)&status,
		sizeof (struct device_element));	/* save status */
	bzero(elem, sizeof (struct device_element));
	elem->select = 1;
	elem->dev_off = status.dev_off;
	elem->en_bypass_a = status.en_bypass_a;
	elem->en_bypass_b = status.en_bypass_b;

	/* Do requested action */
	if (port_a_flag) {
		elem->en_bypass_a = bypass_flag;
	} else {
		elem->en_bypass_b = bypass_flag;
	}

	if (getenv("_LUX_E_DEBUG") != NULL) {
		g_dump("  d_dev_bypass_enable: Updating LRC circuit state:\n"
		"    Device Status Element ",
		(uchar_t *)elem, sizeof (struct device_element),
		HEX_ONLY);
		(void) fprintf(stdout, "    for device at location:"
			" enclosure:%s slot:%d %s\n",
			l_state->ib_tbl.enclosure_name,
			path_struct->slot,
			path_struct->f_flag ? "front" : "rear");
	}
	if (err = g_scsi_send_diag_cmd(fd,
		(uchar_t *)page_buf, page_len)) {
		(void) close(fd);
		(void) g_destroy_data(page_buf);
		(void) l_free_lstate(&l_state);
		return (err);
	}

	(void) close(fd);
	(void) g_destroy_data(page_buf);
	(void) l_free_lstate(&l_state);
	return (0);
}



/*
 * Issue a Loop Port enable Primitive sequence
 * to the device specified by the pathname.
 */
int
d_p_enable(char *path, int verbose)
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
d_p_bypass(char *path, int verbose)
/*ARGSUSED*/
{

	return (0);
}
