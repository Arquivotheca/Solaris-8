/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dc_interface.c	1.2	98/08/28 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/dc_ki.h>
#include <sys/cladm.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/sunddi.h>

/*
 * Implementation of the function to change the dc_ops vector on the fly. This
 * is used in the clustering software to reset the vector to use the cluster
 * specific functions instead of just the stubs.
 */
int
dcops_install(struct dc_ops *new_dcops, enum dcop_state state)
{

	/* Check that this is a version of the dcops vector we understand. */
	if (new_dcops->version > DCOPS_VERSION) {

#ifdef DEBUG
		cmn_err(CE_WARN, "DCS: Version %d of dcops not supported.",
		    state);
#endif
		return (-1);
	}

	switch (state) {

	case DC_READY:
		dcops.dc_get_major = new_dcops->dc_get_major;
		dcops.dc_free_major = new_dcops->dc_free_major;
		dcops.dc_major_name = new_dcops->dc_major_name;
		dcops.dc_sync_instances = new_dcops->dc_sync_instances;
		dcops.dc_instance_path = new_dcops->dc_instance_path;
		dcops.dc_get_instance = new_dcops->dc_get_instance;
		dcops.dc_free_instance = new_dcops->dc_free_instance;
		dcops.dc_map_minor = new_dcops->dc_map_minor;
		dcops.dc_unmap_minor = new_dcops->dc_unmap_minor;

		/*
		 * This function gets called twice during initialization. The
		 * first time, we want to initialize everything but the
		 * dc_resolve_minor entry. So, if no argument is passed in,
		 * do not remap the function.
		 */
		if (new_dcops->dc_resolve_minor != NULL)
			dcops.dc_resolve_minor = new_dcops->dc_resolve_minor;
		dcops.dc_devconfig_lock = new_dcops->dc_devconfig_lock;
		dcops.dc_devconfig_unlock = new_dcops->dc_devconfig_unlock;
		dcops.dc_service_config = new_dcops->dc_service_config;
		break;
	case DC_READONLY:
		dcops.dc_resolve_minor = new_dcops->dc_resolve_minor;
		break;
	case DC_INSTALLING:

		/*
		 * Do not set the map_minor, unmap_minor, resolve_minor
		 * functions, however, the rest of the vector should be
		 * set.
		 */
		dcops.dc_get_major = new_dcops->dc_get_major;
		dcops.dc_free_major = new_dcops->dc_free_major;
		dcops.dc_major_name = new_dcops->dc_major_name;
		dcops.dc_sync_instances = new_dcops->dc_sync_instances;
		dcops.dc_instance_path = new_dcops->dc_instance_path;
		dcops.dc_get_instance = new_dcops->dc_get_instance;
		dcops.dc_free_instance = new_dcops->dc_free_instance;
		dcops.dc_devconfig_lock = new_dcops->dc_devconfig_lock;
		dcops.dc_devconfig_unlock = new_dcops->dc_devconfig_unlock;
		dcops.dc_service_config = new_dcops->dc_service_config;
		break;
	default:
		cmn_err(CE_WARN, "DCS: Unknown cluster status.");
		ASSERT(0);
		return (-1);

		/* Not reached */
	}

	return (0);
}


/*
 * DCS stubs operations vector implementation.
 * All cases the functionality of these operations, preserve
 * the existing semantics without clustering.
 * Make sure they do sane things. All functions should return a 0 on
 * success and a -1 on failure.
 */

/*ARGSUSED*/
int
dcstub_get_major(major_t *major, char *driver_name)
{
	return (0);
}

/*ARGSUSED*/
int
dcstub_free_major(major_t major, char *driver)
{
	return (0);
}

/*ARGSUSED*/
int
dcstub_major_name(major_t major, char *driver_name)
{
	return (0);
}

/*ARGSUSED*/
int
dcstub_sync_instances()
{
	return (0);
}

int
dcstub_instance_path(major_t major, char *path, uint_t inst_number)
{
	dev_info_t *dip;

	ASSERT(path);
	ASSERT(ddi_major_to_name(major));

	if (!(dip = ddi_find_devinfo(ddi_major_to_name(major), inst_number, 0)))
		return (-1);
	(void) strcpy(path, i_ddi_get_dpath_prefix());
	(void) ddi_pathname(dip, path + strlen(path));
	return (0);
}

static struct {
	char	*path;
	int	instance;
	int	major;
} *saved_linstances = 0;

static int saved_linstance_count = 0;
static int saved_linstance_space = 0;

int
dcstub_get_instance(major_t maj, const char *path, uint_t *inst_number)
{
	extern int in_next_local_instance(major_t);

	*inst_number = (uint_t)in_next_local_instance(maj);
	if (cluster_bootflags & CLUSTER_BOOTED) {
		void	*tptr;

		if (saved_linstance_count == saved_linstance_space) {
			tptr = kmem_zalloc(sizeof (*saved_linstances) *
			    (saved_linstance_space + 64), KM_SLEEP);
			if (saved_linstances) {
				bcopy(saved_linstances, tptr,
				    sizeof (*saved_linstances) *
				    saved_linstance_space);
			} else {
			    saved_linstances = tptr;
			}
			saved_linstance_space += 64;
		}
		tptr = kmem_alloc(strlen(path)+1, KM_SLEEP);
		(void) strcpy(tptr, path);
		saved_linstances[saved_linstance_count].path = tptr;
		saved_linstances[saved_linstance_count].instance =
		    *inst_number;
		saved_linstances[saved_linstance_count].major = maj;
		saved_linstance_count++;
	}
	return (0);
}

void
dc_get_linstance(int index, int *major, char **path, int *instance)
{
	ASSERT(index < saved_linstance_count);
	*path = saved_linstances[index].path;
	*instance = saved_linstances[index].instance;
	*major = saved_linstances[index].major;
}

dc_get_linstance_count()
{
	return (saved_linstance_count);
}

/*ARGSUSED*/
int
dcstub_free_instance(major_t major, const char *path, uint_t inst)
{
	return (0);
}

/*ARGSUSED*/
int
dcstub_map_minor(major_t major, minor_t lminor, minor_t *gminor,
    dev_type_t dev_type)
{
	(*gminor) = lminor;
	return (0);
}

/*ARGSUSED*/
int
dcstub_unmap_minor(major_t major, minor_t lminor, minor_t gminor,
    dev_type_t dev_type)
{
	return (0);
}

/*ARGSUSED*/
int
dcstub_resolve_minor(major_t major, minor_t gminor, minor_t *lminor,
	dev_type_t *dev_type)
{
	(*lminor) = gminor;
	return (0);
}

/*ARGSUSED*/
int
dcstub_devconfig_lock(int sleep)
{
	return (0);
}

int
dcstub_devconfig_unlock()
{
	return (0);
}

/*ARGSUSED*/
int
dcstub_service_config(int cmd, void *arg, int mode)
{
	return (-1);
}
