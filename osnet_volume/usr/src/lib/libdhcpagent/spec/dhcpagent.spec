#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)dhcpagent.spec	1.2	99/07/08 SMI"
#
# lib/libdhcpagent/spec/dhcpagent.spec

function        ifname_to_hostconf
include         <dhcp_hostconf.h>
declaration     char *ifname_to_hostconf(const char *ifname)
version         SUNWprivate_1.1
end

function        remove_hostconf
include         <dhcp_hostconf.h>
declaration     int remove_hostconf(const char *ifname)
version         SUNWprivate_1.1
end

function        read_hostconf
include         <dhcp_hostconf.h>
declaration     int read_hostconf(const char *ifname, PKT_LIST **read_pkt)
version         SUNWprivate_1.1
end

function        write_hostconf
include         <dhcp_hostconf.h>
declaration     int write_hostconf(const char *ifname, PKT_LIST *pkt, \
		    time_t relative_to)
version         SUNWprivate_1.1
end

function        dhcp_ipc_init
include         <dhcpagent_ipc.h>
declaration     int dhcp_ipc_init(int *listen_fd)
version         SUNWprivate_1.1
end

function        dhcp_ipc_accept
include         <dhcpagent_ipc.h>
declaration     int dhcp_ipc_accept(int listen_fd, int *fd, int *is_priv)
version         SUNWprivate_1.1
end

function        dhcp_ipc_recv_request
include         <dhcpagent_ipc.h>
declaration     int dhcp_ipc_recv_request(int fd, 			\
		    dhcp_ipc_request_t **request, int timeout_msec)
version         SUNWprivate_1.1
end

function        dhcp_ipc_send_reply
include         <dhcpagent_ipc.h>
declaration     int dhcp_ipc_send_reply(int fd, dhcp_ipc_reply_t *reply)
version         SUNWprivate_1.1
end

function        dhcp_ipc_close
include         <dhcpagent_ipc.h>
declaration     int dhcp_ipc_close(int fd)
version         SUNWprivate_1.1
end

function        dhcp_ipc_strerror
include         <dhcpagent_ipc.h>
declaration     const char *dhcp_ipc_strerror(int errno)
version         SUNWprivate_1.1
end

function        dhcp_ipc_alloc_request
include         <dhcpagent_ipc.h>
declaration     dhcp_ipc_request_t *dhcp_ipc_alloc_request(		\
		    dhcp_ipc_type_t type, const char *ifname,		\
		    void *buffer, uint32_t buffer_size,			\
		    dhcp_data_type_t data_type)
version         SUNWprivate_1.1
end

function        dhcp_ipc_make_request
include         <dhcpagent_ipc.h>
declaration	int dhcp_ipc_make_request(dhcp_ipc_request_t * request,	\
		    dhcp_ipc_reply_t **reply, int32_t timeout)
version         SUNWprivate_1.1
end

function        dhcp_ipc_get_data
include         <dhcpagent_ipc.h>
declaration	void *dhcp_ipc_get_data(dhcp_ipc_reply_t *reply,	\
		    size_t *size, dhcp_data_type_t *type)
version         SUNWprivate_1.1
end

function        dhcp_ipc_getinfo
include         <dhcpagent_ipc.h>
declaration	int dhcp_ipc_getinfo(dhcp_optnum_t *optnum,		\
		    DHCP_OPT **result, int32_t timeout)
version         SUNWprivate_1.1
end

function        dhcp_ipc_alloc_reply
include         <dhcpagent_ipc.h>
declaration	dhcp_ipc_reply_t *dhcp_ipc_alloc_reply(			\
		    dhcp_ipc_request_t *request, int return_code,	\
		    void *buffer, uint32_t buffer_size,			\
		    dhcp_data_type_t data_type)
version         SUNWprivate_1.1
end

function        dhcp_state_to_string
include         <dhcpagent_util.h>
declaration	const char *dhcp_state_to_string(DHCPSTATE state)
version         SUNWprivate_1.1
end

function        dhcp_string_to_request
include         <dhcpagent_util.h>
declaration	dhcp_ipc_type_t dhcp_string_to_request(const char *string)
version         SUNWprivate_1.1
end

function	dhcp_start_agent
include		<dhcpagent_util.h>
declaration	int dhcp_start_agent(int)
version		SUNWprivate_1.1
end

