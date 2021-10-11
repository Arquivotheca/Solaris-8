#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)rcm_event.spec	1.1	99/08/10 SMI"
#
# lib/librcm/spec/rcm_event.spec

function	get_event_service
include		<librcm_event.h>
declaration	int get_event_service(char *door_name, void *data, size_t datalen, void **result, size_t *rlen)
version		SUNWprivate_1.1
end

function	create_event_service
include		<librcm_event.h>
declaration	int create_event_service(char *door_name, void (*func)(void **data, size_t *datalen))
version		SUNWprivate_1.1
end

function	revoke_event_service
include		<librcm_event.h>
declaration	int revoke_event_service(int fd)
version		SUNWprivate_1.1
end

function	se_alloc
include		<librcm_event.h>
declaration	sys_event_t * se_alloc(int class, int type, int level)
version		SUNWprivate_1.1
end

function	se_append_ints
include		<librcm_event.h>
declaration	int se_append_ints(sys_event_t *ev, char *name, int *data, int nitems)
version		SUNWprivate_1.1
end

function	se_append_bytes
include		<librcm_event.h>
declaration	int se_append_bytes(sys_event_t *ev, char *name, uchar_t *data, int nitems)
version		SUNWprivate_1.1
end

function	se_append_strings
include		<librcm_event.h>
declaration	int se_append_strings(sys_event_t *ev, char *name, char *data, int nitems)
version		SUNWprivate_1.1
end

function	se_end_of_data
include		<librcm_event.h>
declaration	sys_event_t * se_end_of_data(sys_event_t *ev)
version		SUNWprivate_1.1
end

function	se_free
include		<librcm_event.h>
declaration	void se_free(sys_event_t *ev)
version		SUNWprivate_1.1
end

function	se_lookup_ints
include		<librcm_event.h>
declaration	int se_lookup_ints(sys_event_t *ev, char *name, int **data)
version		SUNWprivate_1.1
end

function	se_lookup_bytes
include		<librcm_event.h>
declaration	int se_lookup_bytes(sys_event_t *ev, char *name, uchar_t **data)
version		SUNWprivate_1.1
end

function	se_lookup_strings
include		<librcm_event.h>
declaration	int se_lookup_strings(sys_event_t *ev, char *name, char **data)
version		SUNWprivate_1.1
end

function	se_get_next_tuple
include		<librcm_event.h>
declaration	se_data_tuple_t se_get_next_tuple(sys_event_t *ev, se_data_tuple_t tuple)
version		SUNWprivate_1.1
end

function	se_tuple_name
include		<librcm_event.h>
declaration	char * se_tuple_name(se_data_tuple_t tuple)
version		SUNWprivate_1.1
end

function	se_tuple_type
include		<librcm_event.h>
declaration	int se_tuple_type(se_data_tuple_t tuple)
version		SUNWprivate_1.1
end

function	se_tuple_ints
include		<librcm_event.h>
declaration	int se_tuple_ints(se_data_tuple_t tuple, int **data)
version		SUNWprivate_1.1
end

function	se_tuple_bytes
include		<librcm_event.h>
declaration	int se_tuple_bytes(se_data_tuple_t tuple, uchar_t **data)
version		SUNWprivate_1.1
end

function	se_tuple_strings
include		<librcm_event.h>
declaration	int se_tuple_strings(se_data_tuple_t tuple, char **data)
version		SUNWprivate_1.1
end

function	se_print
include		<librcm_event.h>
declaration	void se_print(FILE *fp, sys_event_t *ev)
version		SUNWprivate_1.1
end
