/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lhot.c	1.14	99/07/22 SMI"

/*LINTLIBRARY*/


/*
 *	This module is part of the photon library
 */

/*
 * I18N message number ranges
 *  This file: 8500 - 8999
 *  Shared common messages: 1 - 1999
 */

/* #define		_POSIX_SOURCE 1 */

/*	Includes	*/
#include	<stdlib.h>
#include	<stdio.h>
#include	<sys/file.h>
#include	<sys/types.h>
#include	<sys/param.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<string.h>
#include	<time.h>
#include	<sys/scsi/scsi.h>
#include	<sys/vtoc.h>
#include	<nl_types.h>
#include	<strings.h>
#include	<sys/ddi.h>		/* for max */
#include	<l_common.h>
#include	<stgcom.h>
#include	<l_error.h>
#include	<rom.h>
#include	<a_state.h>
#include	<a5k.h>


/*	Global variables	*/
extern	uchar_t		g_switch_to_alpa[];
extern	uchar_t		g_sf_alpa_to_switch[];


int
l_make_node(char *ses_path, int tid, char *dev_path,
			sf_al_map_t *map, int dtype)
{
int	len, i, j, name_id, err;
char	ssd[40], wwn[20];


	/* get the architecture of the machine */
	if ((err = g_get_machineArch(&name_id)) != 0) {
		return (err);
	}
	if (name_id) {
		len = strlen(ses_path) - strlen(strrchr(ses_path, '/'));

		/* TBD: Must find path, not just use :c */
		if (dtype != DTYPE_ESI) {
			(void) sprintf(ssd, "/ssd@%x,0:c", tid);
		} else {
			(void) sprintf(ssd, "/ses@%x,0:c", tid);
		}

		(void) strncpy(dev_path, ses_path, len);
		dev_path[len] = '\0';
		(void) strcat(dev_path, ssd);

	} else {
		for (i = 0; i < map->sf_count; i++) {
			if (map->sf_addr_pair[i].sf_al_pa ==
				g_switch_to_alpa[tid])
				break;
		}
		if (i >= map->sf_count) {
			*dev_path = '\0';
			return (L_INVALID_LOOP_MAP);
		} else {
			/* Make sure that the port WWN is valid */
			for (j = 0; j < 8; j++) {
				if (map->sf_addr_pair[i].sf_port_wwn[j] != '\0')
					break;
			}
			if (j >= 8) {
				*dev_path = '\0';
				return (L_INVLD_WWN_FORMAT);
			}
			(void) g_ll_to_str(map->sf_addr_pair[i].sf_port_wwn,
								wwn);
			len = strlen(ses_path) - strlen(strrchr(ses_path, '/'));

			if (dtype != DTYPE_ESI) {
				(void) sprintf(ssd, "/ssd@w%s,0:c", wwn);
			} else {
				(void) sprintf(ssd, "/ses@w%s,0:c", wwn);
			}

			/* TBD: Must find path, not just use :c */
			(void) strncpy(dev_path, ses_path, len);
			dev_path[len] = '\0';
			(void) strcat(dev_path, ssd);
		}
	}
	return (0);

}



/*
 * checks for null wwn to a disk.
 * and returns -1 if found, 0
 * otherwise.
 *
 * OUTPUT:
 *	char	*ses_path
 *
 * RETURNS:
 *	0	 if OK
 *	non-zero otherwise
 */
int
l_chk_null_wwn(Path_struct *path_struct, char *ses_path,
				L_state *l_state, int verbose)
{
char		*ptr, boxname[MAXPATHLEN];
char		node_wwn_s[17];
Box_list	*boxlist;
int		i;


	/*
	 * verify and continue only if the argv
	 * has a format like box,{f/r}<slot #>.
	 * Otherwise, return to the caller.
	 * The only way to address null wwn disk
	 * is using the box,{f/r}<slot#> format.
	 */

	(void) strcpy(boxname, path_struct->argv);
	if (((ptr = strstr(boxname, ",")) != NULL) &&
			((*(ptr + 1) == 'f') || (*(ptr + 1) == 'r'))) {
		*ptr = NULL;
	} else {
		return (0);
	}


	/*
	 * Get the list of enclosures
	 * connected to the system.
	 */
	if (l_get_box_list(&boxlist, verbose) != 0) {
		return (L_NO_ENCL_LIST_FOUND);
	}

	*ses_path = NULL;

	/*
	 * The following method is safer to get an ses path
	 * to the enclosure than calling l_get_ses_path(),
	 * with physical path to null WWN disk.
	 * Because, l_get_ses_path uses the disk's
	 * al_pa to get the box id and then ses path
	 * to the box. When a disk has null wwn, it may
	 * not have a valid al_pa, and hard address.
	 * There is a possibility that l_get_ses_path()
	 * not returning ses path to the correct enclosure.
	 */
	while (boxlist != NULL) {
		if ((strcmp(boxname, (char *)boxlist->b_name) == 0)) {
			(void) strcpy(ses_path, boxlist->b_physical_path);
			break;
		}
		boxlist = boxlist->box_next;
	}

	/* free the box list */
	(void) l_free_box_list(&boxlist);

	if ((ses_path != NULL) && (strstr(ses_path, "ses") != NULL)) {
		if (l_get_status(ses_path, l_state,
				verbose) != 0) {
			return (L_GET_STATUS_FAILED);
		}
		if (path_struct->f_flag) {
			(void) strcpy(node_wwn_s,
		l_state->drv_front[path_struct->slot].g_disk_state.node_wwn_s);
		} else {
			(void) strcpy(node_wwn_s,
		l_state->drv_rear[path_struct->slot].g_disk_state.node_wwn_s);
		}

		W_DPRINTF("Found ses path: %s\n"
			"and Node WWN: %s\n", ses_path, node_wwn_s);

		/* check for null WWN */
		for (i = 0; i < WWN_SIZE; i++) {
			if (node_wwn_s[i] != '0') {
				return (0);
			}
		}
		W_DPRINTF("Found NULL WWN: %s\n", node_wwn_s);
		return (1);
	}

	return (0);

}



/*
 * If OVERALL_STATUS is sent as the "func",
 * the code pointer must be valid (non NULL).
 *
 * RETURNS:
 *	0	 if OK
 *	non-zero otherwise
 */
int
l_encl_status_page_funcs(int func, char *code, int todo, char *ses_path,
					struct l_state_struct  *l_state,
				int f_flag, int slot, int verbose_flag)
{
uchar_t	*page_buf;
int 	fd, front_index, rear_index, offset, err;
unsigned short	page_len;
struct	device_element *elem;

	if ((page_buf = (uchar_t *)g_zalloc(MAX_REC_DIAG_LENGTH)) == NULL) {
		return (L_MALLOC_FAILED);
	}

	if ((fd = g_object_open(ses_path, O_NDELAY | O_RDWR)) == -1) {
		(void) g_destroy_data(page_buf);
		return (L_OPEN_PATH_FAIL);
	}

	if ((err = l_get_envsen_page(fd, page_buf, MAX_REC_DIAG_LENGTH,
					L_PAGE_2, verbose_flag)) != 0) {
		(void) g_destroy_data(page_buf);
		(void) close(fd);
		return (err);
	}

	page_len = (page_buf[2] << 8 | page_buf[3]) + HEADER_LEN;

	if ((err = l_get_disk_element_index(l_state, &front_index,
							&rear_index)) != 0) {
		(void) g_destroy_data(page_buf);
		(void) close(fd);
		return (err);
	}
	/* Skip global element */
	front_index++, rear_index++;

	if (f_flag) {
		offset = (8 + (front_index + slot)*4);
	} else {
		offset = (8 + (rear_index  + slot)*4);
	}

	elem = (struct device_element *)((int)page_buf + offset);

	switch (func) {
		case OVERALL_STATUS:
		    switch (todo) {
			case INSERT_DEVICE:
				*code = (elem->code != S_OK) ? elem->code : 0;
				(void) g_destroy_data(page_buf);
				(void) close(fd);
				return (0);
			case REMOVE_DEVICE:
				*code = (elem->code != S_NOT_INSTALLED) ?
					elem->code : 0;
				(void) g_destroy_data(page_buf);
				(void) close(fd);
				return (0);
		    }
		    /* NOTREACHED */
		case SET_RQST_INSRT:
			bzero(elem, sizeof (struct device_element));
			elem->select = 1;
			elem->rdy_to_ins = 1;
			break;
		case SET_RQST_RMV:
			bzero(elem, sizeof (struct device_element));
			elem->select = 1;
			elem->rmv = 1;
			elem->dev_off = 1;
			elem->en_bypass_a = 1;
			elem->en_bypass_b = 1;
			break;
		case SET_FAULT:
			bzero(elem, sizeof (struct device_element));
			elem->select = 1;
			elem->fault_req = 1;
			elem->dev_off = 1;
			elem->en_bypass_a = 1;
			elem->en_bypass_b = 1;
			break;
		case SET_DRV_ON:
			bzero(elem, sizeof (struct device_element));
			elem->select = 1;
			break;
	}

	err = g_scsi_send_diag_cmd(fd, (uchar_t *)page_buf, page_len);
	(void) g_destroy_data(page_buf);
	(void) close(fd);
	return (err);
}



/*
 * finds whether device id (tid) returned by
 * sf, exists in the Arbitrated loop map or not.
 *
 * RETURNS:
 *	1	 if device present
 *	0	 otherwise
 */
int
l_device_present(char *ses_path, int tid, sf_al_map_t *map,
				int verbose_flag, char **dev_path)
{
char		sf_path[MAXPATHLEN];
uchar_t		wwn[40], c;
int		len, i, j, k, fnib, snib;
int		fd, al_pa, name_id, err;
char		ssd[30];


	/* get the architecture of the machine */
	if ((err = g_get_machineArch(&name_id)) != 0) {
		return (err);
	}

	if (name_id) {
		len = strlen(ses_path) - strlen(strrchr(ses_path, '/'));

		(void) sprintf(ssd, "ssd@%x,0", tid);

		(void) strncpy(sf_path, ses_path, len);
		sf_path[len] = '\0';
		P_DPRINTF(" l_device_present: tid=%x, sf_path=%s\n",
			tid, sf_path);
		if ((*dev_path = g_zalloc(MAXPATHLEN)) == NULL) {
			return (L_MALLOC_FAILED);
		}
		(void) sprintf(*dev_path, "%s/%s", sf_path, ssd);
		P_DPRINTF(" l_device_present: dev_path=%s\n", *dev_path);

		(void) strcat(*dev_path, ":c");
		if ((fd = open(*dev_path, O_RDONLY)) == -1) {
			return (0);
		}
		(void) close(fd);
		return (1);
	} else {
		for (i = 0; i < map->sf_count; i++) {
			if (map->sf_addr_pair[i].sf_inq_dtype != DTYPE_ESI) {
				al_pa = map->sf_addr_pair[i].sf_al_pa;
				if (tid == g_sf_alpa_to_switch[al_pa]) {
					break;
				}
			}
		}
		if (i >= map->sf_count)
			return (0);
		/*
		 * Make sure that the port WWN is valid
		 */
		for (j = 0; j < 8; j++)
			if (map->sf_addr_pair[i].sf_port_wwn[j] != '\0')
				break;
		if (j >= 8)
			return (0);
		for (j = 0, k = 0; j < 8; j++) {
			c = map->sf_addr_pair[i].sf_port_wwn[j];
			fnib = (((int)(c & 0xf0)) >> 4);
			snib = (c & 0x0f);
			if (fnib >= 0 && fnib <= 9)
				wwn[k++] = '0' + fnib;
			else if (fnib >= 10 && fnib <= 15)
				wwn[k++] = 'a' + fnib - 10;
			if (snib >= 0 && snib <= 9)
				wwn[k++] = '0' + snib;
			else if (snib >= 10 && snib <= 15)
				wwn[k++] = 'a' + snib - 10;
		}
		wwn[k] = '\0';
		len = strlen(ses_path) - strlen(strrchr(ses_path, '/'));

		(void) sprintf(ssd, "ssd@w%s,0", wwn);

		(void) strncpy(sf_path, ses_path, len);
		sf_path[len] = '\0';
		P_DPRINTF("  l_device_present: wwn=%s, sf_path=%s\n",
			wwn, sf_path);

		if ((*dev_path = g_zalloc(MAXPATHLEN)) == NULL) {
			return (L_MALLOC_FAILED);
		}
		(void) sprintf(*dev_path, "%s/%s", sf_path, ssd);
		P_DPRINTF("  l_device_present: dev_path=%s\n", *dev_path);

		(void) strcat(*dev_path, ":c");
		if ((fd = open(*dev_path, O_RDONLY)) == -1) {
			return (0);
		}
		(void) close(fd);
		return (1);
	}
}



/*
 * onlines the given list of devices
 * and free up the allocated memory.
 *
 * RETURNS:
 *	N/A
 */
static void
online_dev(struct dlist *dl_head, int force_flag)
{
struct dlist	*dl, *dl1;

	for (dl = dl_head; dl != NULL; ) {
		(void) g_online_drive(dl->multipath, force_flag);
		(void) g_free_multipath(dl->multipath);
		dl1 = dl;
		dl = dl->next;
		(void) g_destroy_data(dl1);
	}
}



/*
 * offlines all the disks in a
 * SENA enclosure.
 *
 * RETURNS:
 *	0	 if O.K.
 *	non-zero otherwise
 */
int
l_offline_photon(struct hotplug_disk_list *hotplug_sena,
				struct wwn_list_struct *wwn_list,
				int force_flag, int verbose_flag)
{
int		i, err;
struct dlist	*dl_head, *dl_tail, *dl, *dl_ses;
char		*dev_path, ses_path[MAXPATHLEN];
L_state		*l_state = NULL;

	dl_head = dl_tail = NULL;
	if ((l_state = (L_state *)calloc(1, sizeof (L_state))) == NULL) {
		return (L_MALLOC_FAILED);
	}

	/* Get global status for this Photon */
	dl_ses = hotplug_sena->seslist;
	while (dl_ses) {
		(void) strcpy(ses_path, dl_ses->dev_path);
		if (l_get_status(ses_path, l_state, verbose_flag) == 0)
			break;
		dl_ses = dl_ses->next;
	}

	if (dl_ses == NULL) {
		(void) l_free_lstate(&l_state);
		return (L_ENCL_INVALID_PATH);
	}

	for (i = 0; i < l_state->total_num_drv/2; i++) {
		if (*l_state->drv_front[i].g_disk_state.physical_path) {
			if ((dev_path = g_zalloc(MAXPATHLEN)) == NULL) {
				(void) online_dev(dl_head, force_flag);
				(void) l_free_lstate(&l_state);
				return (L_MALLOC_FAILED);
			}
			(void) strcpy(dev_path,
		(char *)&l_state->drv_front[i].g_disk_state.physical_path);
			if ((dl = g_zalloc(sizeof (struct dlist))) == NULL) {
				(void) g_destroy_data(dev_path);
				(void) online_dev(dl_head, force_flag);
				(void) l_free_lstate(&l_state);
				return (L_MALLOC_FAILED);
			}
			dl->dev_path = dev_path;
			if ((err = g_get_multipath(dev_path,
					&(dl->multipath), wwn_list,  0)) != 0) {
				(void) g_destroy_data(dev_path);
				if (dl->multipath != NULL) {
					(void) g_free_multipath(dl->multipath);
				}
				(void) g_destroy_data(dl);
				(void) online_dev(dl_head, force_flag);
				(void) l_free_lstate(&l_state);
				return (err);
			}
			if ((err = g_offline_drive(dl->multipath,
					    force_flag)) != 0) {
				(void) g_destroy_data(dev_path);
				(void) g_free_multipath(dl->multipath);
				(void) g_destroy_data(dl);
				(void) online_dev(dl_head, force_flag);
				(void) l_free_lstate(&l_state);
				return (err);
			}
			if (dl_head == NULL) {
				dl_head = dl_tail = dl;
			} else {
				dl_tail->next = dl;
				dl->prev = dl_tail;
				dl_tail = dl;
			}
			(void) g_destroy_data(dev_path);
		}
		if (*l_state->drv_rear[i].g_disk_state.physical_path) {
			if ((dev_path = g_zalloc(MAXPATHLEN)) == NULL) {
				(void) online_dev(dl_head, force_flag);
				(void) l_free_lstate(&l_state);
				return (L_MALLOC_FAILED);
			}
			(void) strcpy(dev_path,
		(char *)&l_state->drv_rear[i].g_disk_state.physical_path);
			if ((dl = g_zalloc(sizeof (struct dlist))) == NULL) {
				(void) g_destroy_data(dev_path);
				(void) online_dev(dl_head, force_flag);
				(void) l_free_lstate(&l_state);
				return (L_MALLOC_FAILED);
			}
			dl->dev_path = dev_path;
			if ((err = g_get_multipath(dev_path,
					&(dl->multipath), wwn_list, 0)) != 0) {
				(void) g_destroy_data(dev_path);
				if (dl->multipath != NULL) {
					(void) g_free_multipath(dl->multipath);
				}
				(void) g_destroy_data(dl);
				(void) online_dev(dl_head, force_flag);
				(void) l_free_lstate(&l_state);
				return (err);
			}
			if ((err = g_offline_drive(dl->multipath,
				force_flag)) != 0) {
				(void) g_destroy_data(dev_path);
				(void) g_free_multipath(dl->multipath);
				(void) g_destroy_data(dl);
				(void) online_dev(dl_head, force_flag);
				(void) l_free_lstate(&l_state);
				return (err);
			}
			if (dl_head == NULL) {
				dl_head = dl_tail = dl;
			} else {
				dl_tail->next = dl;
				dl->prev = dl_tail;
				dl_tail = dl;
			}
			(void) g_destroy_data(dev_path);
		}
	}
	hotplug_sena->dlhead = dl_head;
	(void) l_free_lstate(&l_state);
	return (0);

}



/*
 * prepares a char string
 * containing the name of the
 * device which will be hotplugged.
 *
 * RETURNS:
 *	N/A
 */
void
l_get_drive_name(char *drive_name, int slot, int f_flag, char *box_name)
{
	if (f_flag != NULL) {
		(void) sprintf(drive_name, MSGSTR(8500,
			"Drive in \"%s\" front slot %d"), box_name, slot);
	} else {
		(void) sprintf(drive_name, MSGSTR(8501,
			"Drive in \"%s\" rear slot %d"), box_name, slot);
	}
}
