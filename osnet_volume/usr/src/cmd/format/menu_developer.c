
/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)menu_developer.c	1.5	94/09/25 SMI"

/*
 * This file contains functions that implement the fdisk menu commands.
 */
#include "global.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#include <sys/dklabel.h>

#include "main.h"
#include "analyze.h"
#include "menu.h"
#include "menu_developer.h"
#include "param.h"
#include "misc.h"
#include "label.h"
#include "startup.h"
#include "partition.h"
#include "prompts.h"
#include "checkmount.h"
#include "io.h"
#include "ctlr_scsi.h"
#include "auto_sense.h"
#include "hardware_structs.h"

extern	struct menu_item menu_developer[];


int
c_developer()
{

	cur_menu++;
	last_menu = cur_menu;

	/*
	 * Run the menu.
	 */
	run_menu(menu_developer, "DEVELOPER", "developer", 0);
	cur_menu--;
	return (0);
}

int
dv_disk()
{
	struct disk_info *diskp;

	diskp = disk_list;
	while (diskp != NULL) {

		printf("\ndisk_name %s  ", diskp->disk_name);
		printf("disk_path %s\n", diskp->disk_path);
		printf("ctlr_cname = %s  ", diskp->disk_ctlr->ctlr_cname);
		printf("cltr_dname = %s  ", diskp->disk_ctlr->ctlr_dname);
		printf("ctype_name = %s\n",
		    diskp->disk_ctlr->ctlr_ctype->ctype_name);
		printf("ctype_ctype = %d\n",
		    diskp->disk_ctlr->ctlr_ctype->ctype_ctype);
		printf("devfsname = %s\n", diskp->devfs_name);
		diskp = diskp->disk_next;
	}
	return (0);
}

int
dv_cont()
{
	struct ctlr_info *contp;

	contp = ctlr_list;
	while (contp != NULL) {

		printf("\nctype_name = %s ", contp->ctlr_ctype->ctype_name);
		printf("cname = %s dname =  %s ",
		    contp->ctlr_cname, contp->ctlr_dname);
		printf("ctype_ctype = %d\n", contp->ctlr_ctype->ctype_ctype);
		contp = contp->ctlr_next;
	}
	return (0);
}

int
dv_cont_chain()
{
	struct mctlr_list *ctlrp;

	ctlrp = controlp;

	if (ctlrp == NULL)
		printf("ctlrp is NULL!!\n");

	while (ctlrp != NULL) {
		printf("ctlrp->ctlr_type->ctype_name = %s\n",
			ctlrp->ctlr_type->ctype_name);
		ctlrp = ctlrp->next;
	}
	return (0);
}

int
dv_params()
{
	printf("ncyl = %d\n", ncyl);
	printf("acyl = %d\n", acyl);
	printf("pcyl = %d\n", pcyl);
	printf("nhead = %d\n", nhead);
	printf("nsect = %d\n", nsect);

	return (0);
}
