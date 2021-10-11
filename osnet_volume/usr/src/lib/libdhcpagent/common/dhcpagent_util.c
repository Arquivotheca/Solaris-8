/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dhcpagent_util.c	1.4	99/09/01 SMI"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "dhcpagent_ipc.h"
#include "dhcpagent_util.h"

/*
 * dhcp_state_to_string(): given a state, provides the state's name
 *
 *    input: DHCPSTATE: the state to get the name of
 *   output: const char *: the state's name
 */

const char *
dhcp_state_to_string(DHCPSTATE state)
{
	static struct {

		int		state;
		const char	*string;

	} states[] = {

		{ INIT,		"INIT"		},
		{ SELECTING,	"SELECTING"	},
		{ REQUESTING,	"REQUESTING"	},
		{ BOUND,	"BOUND"		},
		{ RENEWING,	"RENEWING"	},
		{ REBINDING,	"REBINDING"	},
		{ INFORMATION,	"INFORMATION"	},
		{ INIT_REBOOT,	"INIT_REBOOT"	},
		{ ADOPTING,	"ADOPTING"	},
		{ INFORM_SENT,	"INFORM_SENT"	}
	};

	unsigned int	i;

	for (i = 0; i < (sizeof (states) / sizeof (*states)); i++)
		if (states[i].state == state)
			return (states[i].string);

	return ("<unknown>");
}

/*
 * dhcp_string_to_request(): maps a string into a request code
 *
 *    input: const char *: the string to map
 *   output: dhcp_ipc_type_t: the request code, or -1 if unknown
 */

dhcp_ipc_type_t
dhcp_string_to_request(const char *request)
{
	static struct {

		const char	*string;
		dhcp_ipc_type_t  type;

	} types[] = {

		{ "drop",	DHCP_DROP	},
		{ "extend",	DHCP_EXTEND	},
		{ "inform",	DHCP_INFORM	},
		{ "ping",	DHCP_PING	},
		{ "release",	DHCP_RELEASE	},
		{ "start",	DHCP_START	},
		{ "status",	DHCP_STATUS	}
	};

	unsigned int	i;

	for (i = 0; i < (sizeof (types) / sizeof (*types)); i++)
		if (strcmp(types[i].string, request) == 0)
			return (types[i].type);

	return (-1);
}

/*
 * dhcp_start_agent(): starts the agent if not already running
 *
 *   input: int: number of seconds to wait for agent to start (-1 is forever)
 *  output: int: 0 on success, -1 on failure
 */

int
dhcp_start_agent(int timeout)
{
	int			error;
	time_t			start_time = time(NULL);
	dhcp_ipc_request_t	*request;
	dhcp_ipc_reply_t	*reply;

	/*
	 * just send a dummy request to the agent to find out if it's
	 * up.  we do this instead of directly connecting to it since
	 * we want to make sure we follow its IPC conventions
	 * (otherwise, it will log warnings to syslog).
	 */

	request = dhcp_ipc_alloc_request(DHCP_PING, "", NULL, 0,
	    DHCP_TYPE_NONE);
	if (request == NULL)
		return (-1);

	error = dhcp_ipc_make_request(request, &reply, 0);
	if (error == 0) {
		free(reply);
		free(request);
		return (0);
	} if (error != DHCP_IPC_E_CONNECT) {
		free(request);
		return (-1);
	}

	switch (fork()) {

	case -1:
		free(request);
		return (-1);

	case  0:
		(void) execl(DHCP_AGENT_PATH, DHCP_AGENT_PATH, (char *)0);
		_exit(EXIT_FAILURE);

	default:
		break;
	}

	while ((timeout != -1) && (time(NULL) - start_time < timeout)) {
		error = dhcp_ipc_make_request(request, &reply, 0);
		if (error == 0) {
			free(reply);
			free(request);
			return (0);
		} else if (error != DHCP_IPC_E_CONNECT)
			break;
		(void) sleep(1);
	}

	free(request);
	return (-1);
}
