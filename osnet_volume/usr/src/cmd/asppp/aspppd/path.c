#ident	"@(#)path.c	1.10	94/12/30 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#include "ipd_ioctl.h"
#include "path.h"

struct path	*paths = NULL;

struct path
*get_path_by_addr(ipd_con_dis_t addr)
{
	struct path	*p;

	for (p = paths; p; p = p->next)
		if (p->inf.iftype == addr.iftype &&
		p->inf.ifunit == addr.ifunit) {
			if (p->inf.iftype == IPD_PTP)
				return (p);
			else if (memcmp(&p->inf.sa, &addr.sa,
					sizeof (p->inf.sa)) == 0)
				return (p);
		}

	return (NULL);
}

struct path
*get_path_by_fd(int fd)
{
	struct path	*p;

	for (p = paths; p; p = p->next)
		if (p->s == fd)
			return (p);

	return (NULL);
}

struct path
*get_path_by_name(char *name)
{
	struct path	*p;

	for (p = paths; p; p = p->next)
		if (p->uucp.system_name &&
		    strcmp(p->uucp.system_name, name) == 0)
			return (p);

	return (NULL);
}

void
add_path(struct path *p)
{
	if (paths == NULL)
		paths = p;
	else {
		p->next = paths;
		paths = p;
	}
}

void
free_path(struct path *p)
{
	if (p) {
		if (p->auth.chap.name)
		    free(p->auth.chap.name);
		if (p->auth.chap.secret)
		    free(p->auth.chap.secret);
		if (p->auth.chap_peer.name)
		    free(p->auth.chap_peer.name);
		if (p->auth.chap_peer.secret)
		    free(p->auth.chap_peer.secret);
		if (p->auth.pap.id)
		    free(p->auth.pap.id);
		if (p->auth.pap.password)
		    free(p->auth.pap.password);
		if (p->auth.pap_peer.id)
		    free(p->auth.pap_peer.id);
		if (p->auth.pap_peer.password)
		    free(p->auth.pap_peer.password);
		if (p->uucp.system_name)
			free(p->uucp.system_name);
		free(p);
	}
}
