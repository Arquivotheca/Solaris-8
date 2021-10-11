#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)dhcputil.spec	1.1	99/01/29 SMI"
#
# lib/libdhcpagent/spec/dhcputil.spec

function        dhcpmsg
include         <dhcpmsg.h>
declaration     void dhcpmsg(int level, const char *format, ...)
version         SUNWprivate_1.1
end

function        dhcpmsg_init
include         <dhcpmsg.h>
declaration     void dhcpmsg_init(const char *program_name, boolean_t	\
		    is_daemon, boolean_t is_verbose, int debugging_level)
version         SUNWprivate_1.1
end

function        dhcpmsg_fini
include         <dhcpmsg.h>
declaration     void dhcpmsg_fini(void)
version         SUNWprivate_1.1
end

function        inittab_load
include         <dhcp_inittab.h>
declaration     inittab_entry_t *inittab_load(uchar_t categories, char	\
		   consumer, size_t *n_entries)
version         SUNWprivate_1.1
end

function        inittab_getbyname
include         <dhcp_inittab.h>
declaration     inittab_entry_t *inittab_getbyname(uchar_t categories,	\
		    char consumer, const char *name)
version         SUNWprivate_1.1
end

function        inittab_getbycode
include         <dhcp_inittab.h>
declaration     inittab_entry_t *inittab_getbycode(uchar_t categories,	\
		    char consumer, uint16_t code)
version         SUNWprivate_1.1
end

function        inittab_verify
include         <dhcp_inittab.h>
declaration     int inittab_verify(inittab_entry_t *inittab_entry,	\
		    inittab_entry_t *internal_entry)
version         SUNWprivate_1.1
end

function        inittab_encode
include         <dhcp_inittab.h>
declaration     uchar_t *inittab_encode(inittab_entry_t *inittab_entry,	\
		    const char *value, uint16_t *lengthp, boolean_t	\
		    just_payload)
version         SUNWprivate_1.1
end

function        inittab_decode
include         <dhcp_inittab.h>
declaration     char *inittab_decode(inittab_entry_t *inittab_entry,	\
		    uchar_t *payload, uint16_t length, boolean_t	\
		    just_payload)
version         SUNWprivate_1.1
end
