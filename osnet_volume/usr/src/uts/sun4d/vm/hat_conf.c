/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)hat_conf.c	1.13	96/04/26 SMI"

#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/systm.h>
#include <vm/hat_srmmu.h>

extern struct hatops srmmu_hatops;

struct hatsw hattab[] = {
	"srmmu", &srmmu_hatops,
	0, 0
};

#define	NHATTAB	(sizeof (hattab) / sizeof (struct hatsw))

/*
 * hat_getops - for a hat identified by the given name string,
 *	return a hat ops vector if one exists, else return (NULL);
 */
struct hatops *
hat_getops(namestr)
	char *namestr;
{
	int i;

	for (i = 0; i < NHATTAB; i++) {
		if (hattab[i].hsw_name == NULL)
			break;
		if (strcmp(namestr, hattab[i].hsw_name) == 0)
			return (hattab[i].hsw_ops);
	}

	return (NULL);
}
