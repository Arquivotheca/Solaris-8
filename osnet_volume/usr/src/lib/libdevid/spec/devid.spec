#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)devid.spec	1.2	99/05/14 SMI"
#
# lib/libdevid/spec/devid.spec

function	devid_get
include		<sys/types.h>, <libdevid.h>
declaration	int devid_get(int fd, ddi_devid_t *devid)
version		SUNW_1.1
end		

function	devid_free
include		<sys/types.h>, <libdevid.h>
declaration	void devid_free(ddi_devid_t devid)
version		SUNW_1.1
end		

function	devid_get_minor_name
include		<sys/types.h>, <libdevid.h>
declaration	int devid_get_minor_name(int fd, char **minor_name)
version		SUNW_1.1
end		

function	devid_sizeof
include		<sys/types.h>, <libdevid.h>
declaration	size_t devid_sizeof(ddi_devid_t devid)
version		SUNW_1.1
end		

function	devid_compare
include		<sys/types.h>, <libdevid.h>
declaration	int devid_compare(ddi_devid_t id1, ddi_devid_t id2)
version		SUNW_1.1
end		

function	devid_deviceid_to_nmlist
include		<sys/types.h>, <libdevid.h>
declaration	int devid_deviceid_to_nmlist( char *search_path, \
			ddi_devid_t devid, char *minor_name, \
			devid_nmlist_t  **retlist)
version		SUNW_1.1
end		

function	devid_free_nmlist
include		<sys/types.h>, <libdevid.h>
declaration	void devid_free_nmlist(devid_nmlist_t *list)
version		SUNW_1.1
end		

