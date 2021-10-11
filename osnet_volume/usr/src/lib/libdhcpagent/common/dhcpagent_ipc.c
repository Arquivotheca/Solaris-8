/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dhcpagent_ipc.c	1.6	99/09/01 SMI"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/sockio.h>
#include <sys/fcntl.h>
#include <stdio.h>		/* snprintf */
#include <arpa/inet.h>		/* ntohl, ntohs, etc */

#include "dhcpagent_ipc.h"
#include "dhcpagent_util.h"

/*
 * the protocol used here is a simple request/reply scheme: a client
 * sends a dhcp_ipc_request_t message to the agent, and the agent
 * sends a dhcp_ipc_reply_t back to the client.  since the requests
 * and replies can be variable-length, they are prefixed on "the wire"
 * by a 32-bit number that tells the other end how many bytes to
 * expect.
 *
 * the format of a request consists of a single dhcp_ipc_request_t;
 * note that the length of this dhcp_ipc_request_t is variable (using
 * the standard c array-of-size-1 trick).  the type of the payload is
 * given by `data_type', which is guaranteed to be `data_length' bytes
 * long starting at `buffer'.  note that `buffer' is guaranteed to be
 * 32-bit aligned but it is poor taste to rely on this.
 *
 * the format of a reply is much the same: a single dhcp_ipc_reply_t;
 * note again that the length of the dhcp_ipc_reply_t is variable.
 * the type of the payload is given by `data_type', which is
 * guaranteed to be `data_length' bytes long starting at `buffer'.
 * once again, note that `buffer' is guaranteed to be 32-bit aligned
 * but it is poor taste to rely on this.
 *
 * requests and replies can be paired up by comparing `ipc_id' fields.
 */

#define	BUFMAX	256

static int	dhcp_ipc_rresvport(in_port_t *);
static int	dhcp_ipc_timed_read(int, void *, unsigned int, int *);
static int	getinfo_ifnames(const char *, dhcp_optnum_t *, DHCP_OPT **);
static char	*get_ifnames(int, int);

/* LINTLIBRARY */

/*
 * dhcp_ipc_alloc_request(): allocates a dhcp_ipc_request_t of the given type
 *			     and interface, with a timeout of 0.
 *
 *   input: dhcp_ipc_type_t: the type of ipc request to allocate
 *	    const char *: the interface to associate the request with
 *	    void *: the payload to send with the message (NULL if none)
 *	    uint32_t: the payload size (0 if none)
 *	    dhcp_data_type_t: the description of the type of payload
 *  output: dhcp_ipc_request_t *: the request on success, NULL on failure
 */

dhcp_ipc_request_t *
dhcp_ipc_alloc_request(dhcp_ipc_type_t type, const char *ifname, void *buffer,
    uint32_t buffer_size, dhcp_data_type_t data_type)
{
	dhcp_ipc_request_t *request = calloc(1, DHCP_IPC_REQUEST_SIZE +
	    buffer_size);

	if (request == NULL)
		return (NULL);

	request->message_type   = type;
	request->data_length    = buffer_size;
	request->data_type	= data_type;

	if (ifname != NULL)
		(void) strlcpy(request->ifname, ifname, IFNAMSIZ);

	if (buffer != NULL)
		(void) memcpy(request->buffer, buffer, buffer_size);

	return (request);
}

/*
 * dhcp_ipc_alloc_reply(): allocates a dhcp_ipc_reply_t
 *
 *   input: dhcp_ipc_request_t *: the request the reply is for
 *	    int: the return code (0 for success, DHCP_IPC_E_* otherwise)
 *	    void *: the payload to send with the message (NULL if none)
 *	    uint32_t: the payload size (0 if none)
 *	    dhcp_data_type_t: the description of the type of payload
 *  output: dhcp_ipc_reply_t *: the reply on success, NULL on failure
 */

dhcp_ipc_reply_t *
dhcp_ipc_alloc_reply(dhcp_ipc_request_t *request, int return_code, void *buffer,
    uint32_t buffer_size, dhcp_data_type_t data_type)
{
	dhcp_ipc_reply_t *reply = calloc(1, DHCP_IPC_REPLY_SIZE + buffer_size);

	if (reply == NULL)
		return (NULL);

	reply->message_type	= request->message_type;
	reply->ipc_id		= request->ipc_id;
	reply->return_code	= return_code;
	reply->data_length	= buffer_size;
	reply->data_type	= data_type;

	if (buffer != NULL)
		(void) memcpy(reply->buffer, buffer, buffer_size);

	return (reply);
}

/*
 * dhcp_ipc_get_data(): gets the data and data type from a dhcp_ipc_reply_t
 *
 *   input: dhcp_ipc_reply_t *: the reply to get data from
 *	    size_t *: the size of the resulting data
 *	    dhcp_data_type_t *: the type of the message (returned)
 *  output: void *: a pointer to the data, if there is any.
 */

void *
dhcp_ipc_get_data(dhcp_ipc_reply_t *reply, size_t *size, dhcp_data_type_t *type)
{
	if (reply == NULL || reply->data_length == 0) {
		*size = 0;
		return (NULL);
	}

	if (type != NULL)
		*type = reply->data_type;

	*size = reply->data_length;
	return (reply->buffer);
}

/*
 * dhcp_ipc_recv_msg(): gets a message using the agent's ipc protocol
 *
 *   input: int: the file descriptor to get the message from
 *	    void **: the address of a pointer to store the message
 *		     (dynamically allocated)
 *	    uint32_t: the minimum length of the packet
 *	    int: the # of milliseconds to wait for the message (-1 is forever)
 *  output: int: 0 on success, DHCP_IPC_E_* otherwise
 */

static int
dhcp_ipc_recv_msg(int fd, void **msg, uint32_t base_length, int msec)
{
	ssize_t			retval;
	dhcp_ipc_reply_t	*ipc_msg;
	uint32_t		length;

	retval = dhcp_ipc_timed_read(fd, &length, sizeof (uint32_t), &msec);
	if (retval != sizeof (uint32_t))
		return (DHCP_IPC_E_READ);

	*msg = malloc(length);
	if (*msg == NULL)
		return (DHCP_IPC_E_MEMORY);

	retval = dhcp_ipc_timed_read(fd, *msg, length, &msec);
	if (retval != length) {
		free(*msg);
		return (DHCP_IPC_E_READ);
	}

	if (length < base_length) {
		free(*msg);
		return (DHCP_IPC_E_READ);
	}

	/*
	 * the data_length field is in the same place in either ipc message.
	 */

	ipc_msg = (dhcp_ipc_reply_t *)(*msg);
	if (ipc_msg->data_length + base_length != length) {
		free(*msg);
		return (DHCP_IPC_E_READ);
	}

	return (0);
}

/*
 * dhcp_ipc_recv_request(): gets a request using the agent's ipc protocol
 *
 *   input: int: the file descriptor to get the message from
 *	    dhcp_ipc_request_t **: address of a pointer to store the request
 *				 (dynamically allocated)
 *	    int: the # of milliseconds to wait for the message (-1 is forever)
 *  output: int: 0 on success, DHCP_IPC_E_* otherwise
 */

int
dhcp_ipc_recv_request(int fd, dhcp_ipc_request_t **request, int msec)
{
	int	retval;

	retval = dhcp_ipc_recv_msg(fd, (void **)request, DHCP_IPC_REQUEST_SIZE,
	    msec);

	/* guarantee that ifname will be NUL-terminated */
	if (retval == 0)
		(*request)->ifname[IFNAMSIZ - 1] = '\0';

	return (retval);
}

/*
 * dhcp_ipc_recv_reply(): gets a reply using the agent's ipc protocol
 *
 *   input: int: the file descriptor to get the message from
 *	    dhcp_ipc_reply_t **: address of a pointer to store the reply
 *				 (dynamically allocated)
 *  output: int: 0 on success, DHCP_IPC_E_* otherwise
 */

static int
dhcp_ipc_recv_reply(int fd, dhcp_ipc_reply_t **reply)
{
	return (dhcp_ipc_recv_msg(fd, (void **)reply, DHCP_IPC_REPLY_SIZE, -1));
}

/*
 * dhcp_ipc_send_msg(): transmits a message using the agent's ipc protocol
 *
 *   input: int: the file descriptor to transmit on
 *	    void *: the message to send
 *	    uint32_t: the message length
 *  output: int: 0 on success, DHCP_IPC_E_* otherwise
 */

static int
dhcp_ipc_send_msg(int fd, void *msg, uint32_t message_length)
{
	struct iovec	iovec[2];

	iovec[0].iov_base = (caddr_t)&message_length;
	iovec[0].iov_len  = sizeof (uint32_t);
	iovec[1].iov_base = msg;
	iovec[1].iov_len  = message_length;

	if (writev(fd, iovec, sizeof (iovec) / sizeof (*iovec)) == -1)
		return (DHCP_IPC_E_WRITEV);

	return (0);
}

/*
 * dhcp_ipc_send_reply(): transmits a reply using the agent's ipc protocol
 *
 *   input: int: the file descriptor to transmit on
 *	    dhcp_ipc_reply_t *: the reply to send
 *  output: int: 0 on success, DHCP_IPC_E_* otherwise
 */

int
dhcp_ipc_send_reply(int fd, dhcp_ipc_reply_t *reply)
{
	return (dhcp_ipc_send_msg(fd, reply, DHCP_IPC_REPLY_SIZE +
	    reply->data_length));
}

/*
 * dhcp_ipc_send_request(): transmits a request using the agent's ipc protocol
 *
 *   input: int: the file descriptor to transmit on
 *	    dhcp_ipc_request_t *: the request to send
 *  output: int: 0 on success, DHCP_IPC_E_* otherwise
 */

static int
dhcp_ipc_send_request(int fd, dhcp_ipc_request_t *request)
{
	/*
	 * for now, ipc_ids aren't really used, but they're intended
	 * to make it easy to send several requests and then collect
	 * all of the replies (and pair them with the requests).
	 */

	request->ipc_id = gethrtime();

	return (dhcp_ipc_send_msg(fd, request, DHCP_IPC_REQUEST_SIZE +
	    request->data_length));
}

/*
 * dhcp_ipc_make_request(): sends the provided request to the agent and reaps
 *			    the reply
 *
 *   input: dhcp_ipc_request_t *: the request to make
 *	    dhcp_ipc_reply_t **: the reply (dynamically allocated)
 *	    int32_t: timeout (in seconds), or DHCP_IPC_WAIT_FOREVER,
 *		     or DHCP_IPC_WAIT_DEFAULT
 *  output: int: 0 on success, DHCP_IPC_E_* otherwise
 */

int
dhcp_ipc_make_request(dhcp_ipc_request_t *request, dhcp_ipc_reply_t **reply,
    int32_t timeout)
{
	int			fd, retval;
	struct sockaddr_in	sin_peer;
	in_port_t		source_port = IPPORT_RESERVED - 1;

	(void) memset(&sin_peer, 0, sizeof (sin_peer));

	sin_peer.sin_family	 = AF_INET;
	sin_peer.sin_port	 = htons(IPPORT_DHCPAGENT);
	sin_peer.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if ((fd = dhcp_ipc_rresvport(&source_port)) == -1) {

		/*
		 * user isn't privileged.  just make a socket.
		 */

		fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd == -1)
			return (DHCP_IPC_E_SOCKET);
	}

	retval = connect(fd, (struct sockaddr *)&sin_peer, sizeof (sin_peer));
	if (retval == -1) {
		(void) dhcp_ipc_close(fd);
		return (DHCP_IPC_E_CONNECT);
	}

	request->timeout = timeout;

	retval = dhcp_ipc_send_request(fd, request);
	if (retval == 0)
		retval = dhcp_ipc_recv_reply(fd, reply);

	(void) dhcp_ipc_close(fd);

	return (retval);
}

/*
 * dhcp_ipc_init(): initializes the ipc channel for use by the agent
 *
 *   input: int *: the file descriptor to accept on (returned)
 *  output: int: 0 on success, DHCP_IPC_E_* otherwise
 */

int
dhcp_ipc_init(int *listen_fd)
{
	struct sockaddr_in	sin;
	int			on = 1;

	(void) memset(&sin, 0, sizeof (struct sockaddr_in));

	sin.sin_family		= AF_INET;
	sin.sin_port		= htons(IPPORT_DHCPAGENT);
	sin.sin_addr.s_addr	= htonl(INADDR_LOOPBACK);

	*listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (*listen_fd == -1)
		return (DHCP_IPC_E_SOCKET);

	/*
	 * we use SO_REUSEADDR here since in the case where there
	 * really is another daemon running that is using the agent's
	 * port, bind(3N) will fail.  so we can't lose.
	 */

	(void) setsockopt(*listen_fd, SOL_SOCKET, SO_REUSEADDR, &on,
	    sizeof (on));

	if (bind(*listen_fd, (struct sockaddr *)&sin, sizeof (sin)) == -1) {
		(void) close(*listen_fd);
		return (DHCP_IPC_E_BIND);
	}

	if (listen(*listen_fd, DHCP_IPC_LISTEN_BACKLOG) == -1) {
		(void) close(*listen_fd);
		return (DHCP_IPC_E_LISTEN);
	}

	return (0);
}

/*
 * dhcp_ipc_accept(): accepts an incoming connection for the agent
 *
 *   input: int: the file descriptor to accept on
 *	    int *: the accepted file descriptor (returned)
 *	    int *: nonzero if the client is privileged (returned)
 *  output: int: 0 on success, DHCP_IPC_E_* otherwise
 *    note: sets the socket into nonblocking mode
 */

int
dhcp_ipc_accept(int listen_fd, int *fd, int *is_priv)
{
	struct sockaddr_in	sin_peer;
	int			sin_len = sizeof (sin_peer);
	int			sockflags;

	/*
	 * if we were extremely concerned with portability, we would
	 * set the socket into nonblocking mode before doing the
	 * accept(3N), since on BSD-based networking stacks, there is
	 * a potential race that can occur if the socket which
	 * connected to us performs a TCP RST before we accept, since
	 * BSD handles this case entirely in the kernel and as a
	 * result even though select said we will not block, we can
	 * end up blocking since there is no longer a connection to
	 * accept.  on SVR4-based systems, this should be okay,
	 * and we will get EPROTO back, even though POSIX.1g says
	 * we should get ECONNABORTED.
	 */

	*fd = accept(listen_fd, (struct sockaddr *)&sin_peer, &sin_len);
	if (*fd == -1)
		return (DHCP_IPC_E_ACCEPT);

	/* get credentials */
	*is_priv = ntohs(sin_peer.sin_port) < IPPORT_RESERVED;

	/*
	 * kick the socket into non-blocking mode so that later
	 * operations on the socket don't block and hold up the whole
	 * application.  with the event demuxing approach, this may
	 * seem unnecessary, but in order to get partial reads/writes
	 * and to handle our internal protocol for passing data
	 * between the agent and its consumers, this is needed.
	 */

	if ((sockflags = fcntl(*fd, F_GETFL, 0)) == -1) {
		(void) close(*fd);
		return (DHCP_IPC_E_FCNTL);
	}

	if (fcntl(*fd, F_SETFL, sockflags | O_NONBLOCK) == -1) {
		(void) close(*fd);
		return (DHCP_IPC_E_FCNTL);
	}

	return (0);
}

/*
 * dhcp_ipc_close(): closes an ipc descriptor
 *
 *   input: int: the file descriptor to close
 *  output: int: 0 on success, DHCP_IPC_E_* otherwise
 */

int
dhcp_ipc_close(int fd)
{
	return ((close(fd) == -1) ? DHCP_IPC_E_CLOSE : 0);
}

/*
 * dhcp_ipc_strerror(): maps an ipc error code into a human-readable string
 *
 *   input: int: the ipc error code to map
 *  output: const char *: the corresponding human-readable string
 */

const char *
dhcp_ipc_strerror(int error)
{
	/* note: this must be kept in sync with DHCP_IPC_E_* definitions */
	const char *syscalls[] = {
		"<unknown>", "socket", "fcntl", "read", "accept", "close",
		"bind", "listen", "malloc", "connect", "writev"
	};

	const char	*error_string;
	static char	buffer[BUFMAX];

	switch (error) {

	/*
	 * none of these errors actually go over the wire.
	 * hence, we assume that errno is still fresh.
	 */

	case DHCP_IPC_E_SOCKET:			/* FALLTHRU */
	case DHCP_IPC_E_FCNTL:			/* FALLTHRU */
	case DHCP_IPC_E_READ:			/* FALLTHRU */
	case DHCP_IPC_E_ACCEPT:			/* FALLTHRU */
	case DHCP_IPC_E_CLOSE:			/* FALLTHRU */
	case DHCP_IPC_E_BIND:			/* FALLTHRU */
	case DHCP_IPC_E_LISTEN:			/* FALLTHRU */
	case DHCP_IPC_E_CONNECT:		/* FALLTHRU */
	case DHCP_IPC_E_WRITEV:

		error_string = strerror(errno);
		if (error_string == NULL)
			error_string = "unknown error";

		(void) snprintf(buffer, sizeof (buffer), "%s: %s",
		    syscalls[error], error_string);

		error_string = buffer;
		break;

	case DHCP_IPC_E_MEMORY:
		error_string = "out of memory";
		break;

	case DHCP_IPC_E_TIMEOUT:
		error_string = "wait timed out, operation still pending...";
		break;

	case DHCP_IPC_E_INVIF:
		error_string = "interface does not exist or cannot be managed "
		    "using DHCP";
		break;

	case DHCP_IPC_E_INT:
		error_string = "internal error (might work later)";
		break;

	case DHCP_IPC_E_PERM:
		error_string = "permission denied";
		break;

	case DHCP_IPC_E_OUTSTATE:
		error_string = "interface not in appropriate state for command";
		break;

	case DHCP_IPC_E_PEND:
		error_string = "interface currently has a pending command "
		    "(try later)";
		break;

	case DHCP_IPC_E_BOOTP:
		error_string = "interface is administered with BOOTP, not DHCP";
		break;

	case DHCP_IPC_E_CMD_UNKNOWN:
		error_string = "unknown command";
		break;

	case DHCP_IPC_E_UNKIF:
		error_string = "interface is not under DHCP control";
		break;

	case DHCP_IPC_E_PROTO:
		error_string = "ipc protocol violation";
		break;

	case DHCP_IPC_E_FAILEDIF:
		error_string = "interface is in a FAILED state and must be "
		    "manually restarted";
		break;

	case DHCP_IPC_E_NOPRIMARY:
		error_string = "primary interface requested but no primary "
		    "interface is set";
		break;

	case DHCP_IPC_E_NOIPIF:
		error_string = "interface currently has no IP address";
		break;

	case DHCP_IPC_E_DOWNIF:
		error_string = "interface is currently down";
		break;

	case DHCP_IPC_E_NOVALUE:
		error_string = "no value was found for this option";
		break;

	default:
		error_string = "unknown error";
		break;
	}

	/*
	 * TODO: internationalize this error string
	 */

	return (error_string);
}

/*
 * getinfo_ifnames(): checks the value of a specified option on a list of
 *		      interface names.
 *   input: const char *: a list of interface names to query (in order) for
 *			  the option; "" queries the primary interface
 *	    dhcp_optnum_t *: a description of the desired option
 *	    DHCP_OPT **:  filled in with the (dynamically allocated) value of
 *			  the option upon success.
 *  output: int: DHCP_IPC_E_* on error, 0 on success or if no value was
 *	         found but no error occurred either (*result will be NULL)
 */

static int
getinfo_ifnames(const char *ifn, dhcp_optnum_t *optnum, DHCP_OPT **result)
{
	dhcp_ipc_request_t	*request;
	dhcp_ipc_reply_t	*reply;
	char			*ifnames, *ifnames_head;
	DHCP_OPT		*opt;
	size_t			opt_size;
	int			retval = 0;

	*result = NULL;
	ifnames_head = ifnames = strdup(ifn);
	if (ifnames == NULL)
		return (DHCP_IPC_E_MEMORY);

	request = dhcp_ipc_alloc_request(DHCP_GET_TAG, "", optnum,
	    sizeof (dhcp_optnum_t), DHCP_TYPE_OPTNUM);

	if (request == NULL) {
		free(ifnames_head);
		return (DHCP_IPC_E_MEMORY);
	}

	ifnames = strtok(ifnames, " ");
	if (ifnames == NULL)
		ifnames = "";

	for (; ifnames != NULL; ifnames = strtok(NULL, " ")) {

		(void) strlcpy(request->ifname, ifnames, IFNAMSIZ);
		retval = dhcp_ipc_make_request(request, &reply, 0);
		if (retval != 0)
			break;

		if (reply->return_code == 0) {
			opt = dhcp_ipc_get_data(reply, &opt_size, NULL);
			if (opt_size > 2 && (opt->len == opt_size - 2)) {
				*result = malloc(opt_size);
				if (*result == NULL)
					retval = DHCP_IPC_E_MEMORY;
				else
					(void) memcpy(*result, opt, opt_size);

				free(reply);
				break;
			}
		}

		free(reply);
		if (ifnames[0] == '\0')
			break;
	}

	free(request);
	free(ifnames_head);

	return (retval);
}

/*
 * get_ifnames(): returns a space-separated list of interface names that
 *		  match the specified flags
 *
 *   input: int: flags which must be on in each interface returned
 *	    int: flags which must be off in each interface returned
 *  output: char *: a dynamically-allocated list of interface names, or
 *		    NULL upon failure.
 */

static char *
get_ifnames(int flags_on, int flags_off)
{
	struct ifconf	ifc;
	int		n_ifs, i, sock_fd;
	char		*ifnames;


	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock_fd == -1)
		return (NULL);

	if ((ioctl(sock_fd, SIOCGIFNUM, &n_ifs) == -1) || (n_ifs <= 0)) {
		(void) close(sock_fd);
		return (NULL);
	}

	ifnames = calloc(1, n_ifs * (IFNAMSIZ + 1));
	ifc.ifc_len = n_ifs * sizeof (struct ifreq);
	ifc.ifc_req = calloc(n_ifs, sizeof (struct ifreq));
	if (ifc.ifc_req != NULL && ifnames != NULL) {

		if (ioctl(sock_fd, SIOCGIFCONF, &ifc) == -1) {
			(void) close(sock_fd);
			free(ifnames);
			free(ifc.ifc_req);
			return (NULL);
		}

		for (i = 0; i < n_ifs; i++) {

			if (ioctl(sock_fd, SIOCGIFFLAGS, &ifc.ifc_req[i]) == 0)
				if ((ifc.ifc_req[i].ifr_flags &
				    (flags_on | flags_off)) != flags_on)
					continue;

			(void) strcat(ifnames, ifc.ifc_req[i].ifr_name);
			(void) strcat(ifnames, " ");
		}

		if (strlen(ifnames) > 1)
			ifnames[strlen(ifnames) - 1] = '\0';
	}

	(void) close(sock_fd);
	free(ifc.ifc_req);
	return (ifnames);
}

/*
 * dhcp_ipc_getinfo(): attempts to retrieve a value for the specified DHCP
 *		       option; tries primary interface, then all DHCP-owned
 *		       interfaces, then INFORMs on the remaining interfaces
 *		       (these interfaces are dropped prior to returning).
 *   input: dhcp_optnum_t *: a description of the desired option
 *	    DHCP_OPT **:  filled in with the (dynamically allocated) value of
 *			  the option upon success.
 *	    int32_t: timeout (in seconds), or DHCP_IPC_WAIT_FOREVER,
 *		     or DHCP_IPC_WAIT_DEFAULT.
 *  output: int: DHCP_IPC_E_* on error, 0 upon success.
 */

int
dhcp_ipc_getinfo(dhcp_optnum_t *optnum, DHCP_OPT **result, int32_t timeout)
{
	dhcp_ipc_request_t	*request;
	dhcp_ipc_reply_t	*reply;
	char			*ifnames, *ifnames_copy, *ifnames_head;
	int			retval;
	time_t			start_time = time(NULL);

	if (timeout == DHCP_IPC_WAIT_DEFAULT)
		timeout = DHCP_IPC_DEFAULT_WAIT;

	/*
	 * wait at most 5 seconds for the agent to start.
	 */

	if (dhcp_start_agent((timeout > 5 || timeout < 0) ? 5 : timeout) == -1)
		return (DHCP_IPC_E_INT);

	/*
	 * check the primary interface for the option value first.
	 */

	retval = getinfo_ifnames("", optnum, result);
	if ((retval != 0) || (retval == 0 && *result != NULL))
		return (retval);

	/*
	 * no luck.  get a list of the interfaces under DHCP control
	 * and perform a GET_TAG on each one.
	 */

	ifnames = get_ifnames(IFF_DHCPRUNNING, 0);
	if (ifnames != NULL && strlen(ifnames) != 0) {
		retval = getinfo_ifnames(ifnames, optnum, result);
		if ((retval != 0) || (retval == 0 && *result != NULL)) {
			free(ifnames);
			return (retval);
		}
	}
	free(ifnames);

	/*
	 * still no luck.  retrieve a list of all interfaces on the
	 * system that could use DHCP but aren't.  send INFORMs out on
	 * each one. after that, sit in a loop for the next `timeout'
	 * seconds, trying every second to see if a response for the
	 * option we want has come in on one of the interfaces.
	 */

	ifnames = get_ifnames(IFF_UP|IFF_RUNNING, IFF_LOOPBACK|IFF_DHCPRUNNING);
	if (ifnames == NULL || strlen(ifnames) == 0) {
		free(ifnames);
		return (DHCP_IPC_E_NOVALUE);
	}

	ifnames_head = ifnames_copy = strdup(ifnames);
	if (ifnames_copy == NULL) {
		free(ifnames);
		return (DHCP_IPC_E_MEMORY);
	}

	request = dhcp_ipc_alloc_request(DHCP_INFORM, "", NULL, 0,
	    DHCP_TYPE_NONE);
	if (request == NULL) {
		free(ifnames);
		free(ifnames_head);
		return (DHCP_IPC_E_MEMORY);
	}

	ifnames_copy = strtok(ifnames_copy, " ");
	for (; ifnames_copy != NULL; ifnames_copy = strtok(NULL, " ")) {
		(void) strlcpy(request->ifname, ifnames_copy, IFNAMSIZ);
		if (dhcp_ipc_make_request(request, &reply, 0) == 0)
			free(reply);
	}

	for (;;) {
		if ((timeout != DHCP_IPC_WAIT_FOREVER) &&
		    (time(NULL) - start_time > timeout)) {
			retval = DHCP_IPC_E_TIMEOUT;
			break;
		}

		retval = getinfo_ifnames(ifnames, optnum, result);
		if (retval != 0 || (retval == 0 && *result != NULL))
			break;

		(void) sleep(1);
	}

	/*
	 * drop any interfaces that weren't under DHCP control before
	 * we got here; this keeps this function more of a black box
	 * and the behavior more consistent from call to call.
	 */

	request->message_type = DHCP_DROP;

	ifnames_copy = strcpy(ifnames_head, ifnames);
	ifnames_copy = strtok(ifnames_copy, " ");
	for (; ifnames_copy != NULL; ifnames_copy = strtok(NULL, " ")) {
		(void) strlcpy(request->ifname, ifnames_copy, IFNAMSIZ);
		if (dhcp_ipc_make_request(request, &reply, 0) == 0)
			free(reply);
	}

	free(request);
	free(ifnames_head);
	free(ifnames);
	return (retval);
}

/*
 * NOTE: we provide our own version of this function because currently
 *       (sunos 5.7), if we link against the one in libnsl, we will
 *       increase the size of our binary by more than 482K due to
 *	 perversions in linking.  besides, this one is tighter :-)
 */

static int
dhcp_ipc_rresvport(in_port_t *start_port)
{
	struct sockaddr_in	sin;
	int			s, saved_errno;

	(void) memset(&sin, 0, sizeof (struct sockaddr_in));
	sin.sin_family		= AF_INET;
	sin.sin_addr.s_addr	= htonl(INADDR_ANY);

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		return (-1);

	errno = EAGAIN;
	while (*start_port > IPPORT_RESERVED / 2) {

		sin.sin_port = htons((*start_port)--);

		if (bind(s, (struct sockaddr *)&sin, sizeof (sin)) == 0)
			return (s);

		if (errno != EADDRINUSE) {
			saved_errno = errno;
			break;
		}
	}

	(void) close(s);
	errno = saved_errno;
	return (-1);
}

/*
 * dhcp_ipc_timed_read(): reads from a descriptor using a maximum timeout
 *
 *   input: int: the file descriptor to read from
 *	    void *: the buffer to read into
 *	    unsigned int: the total length of data to read
 *	    int *: the number of milliseconds to wait; the number of
 *		   milliseconds left are returned
 *  output: int: -1 on failure, otherwise the number of bytes read
 */

static int
dhcp_ipc_timed_read(int fd, void *buffer, unsigned int length, int *msec)
{
	unsigned int	n_total = 0;
	ssize_t		n_read;
	struct pollfd	pollfd;
	struct timeval	start, end, elapsed;

	/* make sure that any errors we return are ours */
	errno = 0;

	pollfd.fd	= fd;
	pollfd.events	= POLLIN;

	while (n_total < length) {

		if (gettimeofday(&start, NULL) == -1)
			return (-1);

		switch (poll(&pollfd, 1, *msec)) {

		case 0:
			*msec = 0;
			return (n_total);

		case -1:
			*msec = 0;
			return (-1);

		default:
			if ((pollfd.revents & POLLIN) == 0)
				return (-1);

			if (gettimeofday(&end, NULL) == -1)
				return (-1);

			elapsed.tv_sec  = end.tv_sec  - start.tv_sec;
			elapsed.tv_usec = end.tv_usec - start.tv_usec;
			if (elapsed.tv_usec < 0) {
				elapsed.tv_sec--;
				elapsed.tv_usec += 1000000;	/* one second */
			}

			n_read = read(fd, (caddr_t)buffer + n_total,
			    length - n_total);

			if (n_read == -1)
				return (-1);

			n_total += n_read;
			*msec -= elapsed.tv_sec * 1000 + elapsed.tv_usec / 1000;
			if (*msec <= 0 || n_read == 0)
				return (n_total);
			break;
		}
	}

	return (n_total);
}
