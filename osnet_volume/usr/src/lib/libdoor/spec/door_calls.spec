#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)door_calls.spec	1.2	99/05/14 SMI"
#
# lib/libdoor/spec/door_calls.spec

function	door_bind
include		<door.h>
declaration	int door_bind(int d)
version		SUNWprivate_1.1
end

function	door_call
include		<door.h>
declaration	int door_call(int did, door_arg_t *arg)
version		SUNWprivate_1.1
exception	$return == -1
end

function	door_create
include		<door.h>
declaration	int door_create( \
			void (*server_procedure)(void *cookie, char *argp, \
				size_t arg_size, door_desc_t *dp, \
				uint_t n_desc), \
			void *cookie, uint_t attributes)
version		SUNWprivate_1.1
exception	$return == -1
end

function	door_cred
include		<door.h>
declaration	int door_cred(door_cred_t *dc)
version		SUNWprivate_1.1
end

function	door_info
include		<door.h>
declaration	int door_info(int did, door_info_t *di)
version		SUNWprivate_1.1
exception	$return == -1
end

function	door_return
include		<door.h>
declaration	int door_return(char *data_ptr, size_t data_size, \
			door_desc_t *desc_ptr, uint_t desc_size)
version		SUNWprivate_1.1
exception	$return == -1
end

function	door_revoke
include		<door.h>
declaration	int door_revoke(int did)
version		SUNWprivate_1.1
exception	$return == -1
end

#
# Header uses door_server_func_t, spec2trace does not interpret
# typedefs, so we use an alternate binary equivalent for delaration
#   declaration	door_server_func_t *door_server_create(door_server_func_t *)
#
function	door_server_create
include		<door.h>
declaration	void (*door_server_create(void(*create_proc)(door_info_t*))) \
			(door_info_t *)
version		SUNWprivate_1.1
end

function	door_unbind
include		<door.h>
declaration	int door_unbind(void)
version		SUNWprivate_1.1
end
