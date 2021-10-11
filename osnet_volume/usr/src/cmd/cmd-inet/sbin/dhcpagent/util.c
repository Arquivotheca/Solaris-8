/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)util.c	1.10	99/09/21 SMI"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <netinet/in.h>		/* struct in_addr */
#include <netinet/dhcp.h>
#include <signal.h>
#include <sys/dlpi.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <errno.h>
#include <net/route.h>
#include <string.h>
#include <dhcpmsg.h>

#include "states.h"
#include "agent.h"
#include "interface.h"
#include "util.h"
#include "packet.h"

/*
 * this file contains utility functions that have no real better home
 * of their own.  they can largely be broken into six categories:
 *
 *  o  conversion functions -- functions to turn integers into strings,
 *     or to convert between units of a similar measure.
 *
 *  o  ipc-related functions -- functions to simplify the generation of
 *     ipc messages to the agent's clients.
 *
 *  o  signal-related functions -- functions to clean up the agent when
 *     it receives a signal.
 *
 *  o  routing table manipulation functions
 *
 *  o  acknak handler functions
 *
 *  o  true miscellany -- anything else
 */

/*
 * pkt_type_to_string(): stringifies a packet type
 *
 *   input: uchar_t: a DHCP packet type value, as defined in RFC2131
 *  output: const char *: the stringified packet type
 */

const char *
pkt_type_to_string(uchar_t type)
{
	/*
	 * note: the ordering here allows direct indexing of the table
	 *	 based on the RFC2131 packet type value passed in.
	 */

	static const char *types[] = {
		"BOOTP",  "DISCOVER", "OFFER",   "REQUEST", "DECLINE",
		"ACK",    "NAK",      "RELEASE", "INFORM"
	};

	if (type > (sizeof (types) / sizeof (*types)) || types[type] == NULL)
		return ("<unknown>");

	return (types[type]);
}

/*
 * dlpi_to_arp(): converts DLPI datalink types into ARP datalink types
 *
 *   input: uchar_t: the DLPI datalink type
 *  output: uchar_t: the ARP datalink type (0 if no corresponding code)
 */

uchar_t
dlpi_to_arp(uchar_t dlpi_type)
{
	switch (dlpi_type) {

	case DL_ETHER:
		return (1);

	case DL_FRAME:
		return (15);

	case DL_ATM:
		return (16);

	case DL_HDLC:
		return (17);

	case DL_FC:
		return (18);

	case DL_CSMACD:				/* ieee 802 networks */
	case DL_TPB:
	case DL_TPR:
	case DL_METRO:
	case DL_FDDI:
		return (6);
	}

	return (0);
}

/*
 * monosec_to_string(): converts a monosec_t into a date string
 *
 *   input: monosec_t: the monosec_t to convert
 *  output: const char *: the corresponding date string
 */

const char *
monosec_to_string(monosec_t monosec)
{
	time_t	time = monosec_to_time(monosec);
	char	*time_string = ctime(&time);

	/* strip off the newline -- ugh, why, why, why.. */
	time_string[strlen(time_string) - 1] = '\0';
	return (time_string);
}

/*
 * monosec(): returns a monotonically increasing time in seconds that
 *            is not affected by stime(2) or adjtime(2).
 *
 *   input: void
 *  output: monosec_t: the number of seconds since some time in the past
 */

monosec_t
monosec(void)
{
	return (gethrtime() / NANOSEC);
}

/*
 * monosec_to_time(): converts a monosec_t into real wall time
 *
 *    input: monosec_t: the absolute monosec_t to convert
 *   output: time_t: the absolute time that monosec_t represents in wall time
 */

time_t
monosec_to_time(monosec_t abs_monosec)
{
	return (abs_monosec - monosec()) + time(NULL);
}

/*
 * send_ok_reply(): sends an "ok" reply to a request and closes the ipc
 *		    connection
 *
 *   input: dhcp_ipc_request_t *: the request to reply to
 *	    int *: the ipc connection file descriptor (set to -1 on return)
 *  output: void
 *    note: the request is freed (thus the request must be on the heap).
 */

void
send_ok_reply(dhcp_ipc_request_t *request, int *control_fd)
{
	send_error_reply(request, 0, control_fd);
}

/*
 * send_error_reply(): sends an "error" reply to a request and closes the ipc
 *		       connection
 *
 *   input: dhcp_ipc_request_t *: the request to reply to
 *	    int: the error to send back on the ipc connection
 *	    int *: the ipc connection file descriptor (set to -1 on return)
 *  output: void
 *    note: the request is freed (thus the request must be on the heap).
 */

void
send_error_reply(dhcp_ipc_request_t *request, int error, int *control_fd)
{
	send_data_reply(request, control_fd, error, DHCP_TYPE_NONE, NULL, NULL);
}

/*
 * send_data_reply(): sends a reply to a request and closes the ipc connection
 *
 *   input: dhcp_ipc_request_t *: the request to reply to
 *	    int *: the ipc connection file descriptor (set to -1 on return)
 *	    int: the status to send back on the ipc connection (zero for
 *		 success, DHCP_IPC_E_* otherwise).
 *	    dhcp_data_type_t: the type of the payload in the reply
 *	    void *: the payload for the reply, or NULL if there is no payload
 *	    size_t: the size of the payload
 *  output: void
 *    note: the request is freed (thus the request must be on the heap).
 */

void
send_data_reply(dhcp_ipc_request_t *request, int *control_fd,
    int error, dhcp_data_type_t type, void *buffer, size_t size)
{
	dhcp_ipc_reply_t	*reply;

	if (*control_fd == -1)
		return;

	reply = dhcp_ipc_alloc_reply(request, error, buffer, size, type);
	if (reply == NULL)
		dhcpmsg(MSG_ERR, "send_data_reply: cannot allocate reply");

	else if (dhcp_ipc_send_reply(*control_fd, reply) != 0)
		dhcpmsg(MSG_ERR, "send_data_reply: dhcp_ipc_send_reply");

	/*
	 * free the request since we've now used it to send our reply.
	 * we can also close the socket since the reply has been sent.
	 */

	free(reply);
	free(request);
	(void) dhcp_ipc_close(*control_fd);
	*control_fd = -1;
}

/*
 * print_server_msg(): prints a message from a DHCP server
 *
 *   input: struct ifslist *: the interface the message came in on
 *	    DHCP_OPT *: the option containing the string to display
 *  output: void
 */

void
print_server_msg(struct ifslist *ifsp, DHCP_OPT *p)
{
	dhcpmsg(MSG_INFO, "%s: message from server: %.*s", ifsp->if_name,
	    p->len, p->value);
}

/*
 * daemonize(): daemonizes the process
 *
 *   input: void
 *  output: int: 1 on success, 0 on failure
 */

int
daemonize(void)
{
	int	max_fd = sysconf(_SC_OPEN_MAX);
	int	i;

	if (max_fd == -1)
		return (0);

	switch (fork()) {

	case -1:
		return (0);

	case  0:

		/*
		 * setsid() makes us lose our controlling terminal,
		 * and become both a session leader and a process
		 * group leader.
		 */

		(void) setsid();

		/*
		 * under POSIX, a session leader can accidentally
		 * (through open(2)) acquire a controlling terminal if
		 * it does not have one.  just to be safe, fork again
		 * so we are not a session leader.
		 */

		switch (fork()) {

		case -1:
			return (0);

		case 0:
			(void) signal(SIGHUP, SIG_IGN);
			(void) chdir("/");
			(void) umask(022);

			for (i = 0; i < max_fd; i++)
				(void) close(i);
			break;

		default:
			_exit(EXIT_SUCCESS);
		}
		break;

	default:
		_exit(EXIT_SUCCESS);
	}

	return (1);
}

/*
 * init_default_route(): common code for initializing the struct rtentry
 *
 *   input: struct rtentry *: the rtentry to initialize
 *	    struct in_addr: the default gateway to initialize it with
 *  output: void
 */

static void
init_default_route(struct rtentry *route, struct in_addr *gateway_nbo)
{
	struct sockaddr_in	*sin;

	(void) memset(route, 0, sizeof (struct rtentry));
	route->rt_flags	= RTF_GATEWAY;

	/* LINTED [rt_dst is a sockaddr which will be aligned] */
	sin = (struct sockaddr_in *)&route->rt_dst;
	sin->sin_family		= AF_INET;
	sin->sin_addr.s_addr	= htonl(0);

	/* LINTED [rt_gateway is a sockaddr which will be aligned] */
	sin = (struct sockaddr_in *)&route->rt_gateway;
	sin->sin_family		= AF_INET;
	sin->sin_addr		= *gateway_nbo;
}

/*
 * add_default_route(): add the default route to the given gateway
 *
 *   input: int: an open socket
 *	    struct in_addr: the default gateway to add
 *  output: int: 1 on success, 0 on failure
 */

int
add_default_route(int fd, struct in_addr *gateway_nbo)
{
	struct rtentry		route;

	init_default_route(&route, gateway_nbo);
	route.rt_flags |= RTF_UP;
	return (ioctl(fd, SIOCADDRT, &route) == 0 || errno == EEXIST);
}

/*
 * del_default_route(): deletes the default route to the given gateway
 *
 *   input: int: an open socket
 *	    struct in_addr: if not INADDR_ANY, the default gateway to remove
 *  output: int: 1 on success, 0 on failure
 */

int
del_default_route(int fd, struct in_addr *gateway_nbo)
{
	struct rtentry		route;

	if (gateway_nbo->s_addr == htonl(INADDR_ANY)) /* no router */
		return (1);

	init_default_route(&route, gateway_nbo);
	return (ioctl(fd, SIOCDELRT, &route) == 0);
}

/*
 * inactivity_shutdown(): shuts down agent if there are no interfaces to manage
 *
 *   input: tq_t *: unused
 *	    void *: unused
 *  output: void
 */

/* ARGSUSED */
void
inactivity_shutdown(tq_t *tqp, void *arg)
{
	if (ifs_count() > 0)	/* shouldn't happen, but... */
		return;

	eh_stop_handling_events(eh, DHCP_REASON_INACTIVITY);
}

/*
 * graceful_shutdown(): shuts down the agent gracefully
 *
 *   input: int: the signal that caused graceful_shutdown to be called
 *  output: void
 */

void
graceful_shutdown(int sig)
{
	switch (sig) {

	case SIGTERM:
		eh_stop_handling_events(eh, DHCP_REASON_TERMINATE);
		break;

	default:
		eh_stop_handling_events(eh, DHCP_REASON_SIGNAL);
		break;
	}
}

/*
 * register_acknak(): registers dhcp_acknak() to be called back when ACK or
 *		      NAK packets are received on a given interface
 *
 *   input: struct ifslist *: the interface to register for
 *  output: int: 1 on success, 0 on failure
 */

int
register_acknak(struct ifslist *ifsp)
{
	eh_event_id_t	ack_id, ack_bcast_id = -1;

	/*
	 * having an acknak id already registered isn't impossible;
	 * handle the situation as gracefully as possible.
	 */

	if (ifsp->if_acknak_id != -1) {
		dhcpmsg(MSG_DEBUG, "register_acknak: acknak id pending, "
		    "attempting to cancel");
		if (unregister_acknak(ifsp) == 0)
			return (0);
	}

	switch (ifsp->if_state) {

	case BOUND:
	case REBINDING:
	case RENEWING:

		ack_bcast_id = eh_register_event(eh, ifsp->if_sock_fd, POLLIN,
		    dhcp_acknak, ifsp);

		if (ack_bcast_id == -1) {
			dhcpmsg(MSG_WARNING, "register_acknak: cannot "
			    "register to receive socket broadcasts");
			return (0);
		}

		ack_id = eh_register_event(eh, ifsp->if_sock_ip_fd, POLLIN,
		    dhcp_acknak, ifsp);
		break;

	default:
		ack_id = eh_register_event(eh, ifsp->if_dlpi_fd, POLLIN,
		    dhcp_acknak, ifsp);
		break;
	}

	if (ack_id == -1) {
		dhcpmsg(MSG_WARNING, "register_acknak: cannot register event");
		(void) eh_unregister_event(eh, ack_bcast_id, NULL);
		return (0);
	}

	ifsp->if_acknak_id = ack_id;
	hold_ifs(ifsp);

	ifsp->if_acknak_bcast_id = ack_bcast_id;
	if (ifsp->if_acknak_bcast_id != -1) {
		hold_ifs(ifsp);
		dhcpmsg(MSG_DEBUG, "register_acknak: registered broadcast id "
		    "%d", ack_bcast_id);
	}

	dhcpmsg(MSG_DEBUG, "register_acknak: registered acknak id %d", ack_id);
	return (1);
}

/*
 * unregister_acknak(): unregisters dhcp_acknak() to be called back
 *
 *   input: struct ifslist *: the interface to unregister for
 *  output: int: 1 on success, 0 on failure
 */

int
unregister_acknak(struct ifslist *ifsp)
{
	if (ifsp->if_acknak_id != -1) {

		if (eh_unregister_event(eh, ifsp->if_acknak_id, NULL) == 0) {
			dhcpmsg(MSG_DEBUG, "unregister_acknak: cannot "
			    "unregister acknak id %d on %s",
			    ifsp->if_acknak_id, ifsp->if_name);
			return (0);
		}

		dhcpmsg(MSG_DEBUG, "unregister_acknak: unregistered acknak id "
		    "%d", ifsp->if_acknak_id);

		ifsp->if_acknak_id = -1;
		(void) release_ifs(ifsp);
	}

	if (ifsp->if_acknak_bcast_id != -1) {

		if (eh_unregister_event(eh, ifsp->if_acknak_bcast_id, NULL)
		    == 0) {
			dhcpmsg(MSG_DEBUG, "unregister_acknak: cannot "
			    "unregister broadcast id %d on %s",
			    ifsp->if_acknak_id, ifsp->if_name);
			return (0);
		}

		dhcpmsg(MSG_DEBUG, "unregister_acknak: unregistered "
		    "broadcast id %d", ifsp->if_acknak_bcast_id);

		ifsp->if_acknak_bcast_id = -1;
		(void) release_ifs(ifsp);
	}

	return (1);
}

/*
 * bind_sock(): binds a socket to a given IP address and port number
 *
 *   input: int: the socket to bind
 *	    in_port_t: the port number to bind to, host byte order
 *	    in_addr_t: the address to bind to, host byte order
 *  output: int: 1 on success, 0 on failure
 */

int
bind_sock(int fd, in_port_t port_hbo, in_addr_t addr_hbo)
{
	struct sockaddr_in	sin;
	int			on = 1;

	(void) memset(&sin, 0, sizeof (struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port   = htons(port_hbo);
	sin.sin_addr.s_addr = htonl(addr_hbo);

	(void) setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (int));

	return (bind(fd, (struct sockaddr *)&sin, sizeof (sin)) == 0);
}
