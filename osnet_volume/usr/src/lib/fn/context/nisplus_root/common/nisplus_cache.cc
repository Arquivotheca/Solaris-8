/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nisplus_cache.cc	1.3	96/05/23 SMI"

#include <rpcsvc/nis.h>  /* NIS+ object definitions */
#include <netdir.h>
#include <string.h>
#include <malloc.h>

extern "C" bool_t xdr_directory_obj(XDR *, directory_obj*);


#define	__MAX_ENDPOINTS 6


// Construct a nis_server structure that contains for host 'server_name'.
//
// The server's address is looked up using netdir_getbyname(),
// for transports specified in the local /etc/netconfig file.
//
// If server_ip_addr is given, address for the UDP/TCP connections
// are also included (in addition to any other UDP/TCP addresses
// looked up from the naming service).
//
static
nis_server *
get_server(const char *server_name, const char *server_ip_addr)
{
	endpoint  		*server_addr;
	nis_server 		*server;
	int			num_ep = 0, i;
	struct netconfig	*nc;
	void			*nch;
	struct nd_hostserv	hs;
	struct nd_addrlist	*addrs;

	server_addr = (endpoint *)malloc(sizeof (endpoint)*__MAX_ENDPOINTS);

	if (server_ip_addr) {
		static char uaddr[32];

		// construct uaddr for rpcbind's port on given server addr

		sprintf(uaddr, "%s.0.111", server_ip_addr);

		// UDP
		server_addr[num_ep].uaddr = strdup(uaddr);
		server_addr[num_ep].family = strdup("INET");
		server_addr[num_ep].proto = strdup("UDP");
		++num_ep;

		// TCP
		server_addr[num_ep].uaddr = strdup(uaddr);
		server_addr[num_ep].family = strdup("INET");
		server_addr[num_ep].proto = strdup("TCP");
		++num_ep;
	}

	// gather addresses for different transports in /etc/netconfig

	hs.h_host = (char *)server_name;
	hs.h_serv = "rpcbind";
	nch = setnetconfig();
	while (nc = getnetconfig(nch)) {
		if (! netdir_getbyname(nc, &hs, &addrs)) {
			for (i = 0;
			    i < addrs->n_cnt && num_ep < __MAX_ENDPOINTS;
			    i++, num_ep++) {
				server_addr[num_ep].uaddr =
				taddr2uaddr(nc, &(addrs->n_addrs[i]));
				server_addr[num_ep].family =
					    strdup(nc->nc_protofmly);
				server_addr[num_ep].proto =
					    strdup(nc->nc_proto);
			}
			netdir_free((char *)addrs, ND_ADDRLIST);
		}
	}
	endnetconfig(nch);

	if (num_ep == 0)
		return (0);

	server = new nis_server;

	server->name = strdup(server_name);
	server->ep.ep_len = num_ep;
	server->ep.ep_val = &server_addr[0];
	server->key_type = NIS_PK_NONE;
	server->pkey.n_bytes = NULL;
	server->pkey.n_len = 0;
	return (server);
}

/*
 * This function initializes a directory object for the specified directory,
 * using the given server address information.
 * It returns 0 if it couldn't get a nis_server structure.
 */

static
directory_obj *
new_nisplus_directory_object(const char *directory_name,
    const char *server_name, const char *server_addr)
{
	directory_obj *dd = new directory_obj;

	if (dd == 0)
		return (0);

	if (dd->do_servers.do_servers_val =
	    get_server(server_name, server_addr)) {
		dd->do_type = NIS;
		dd->do_name = strdup(directory_name);
		dd->do_servers.do_servers_len = 1;
		dd->do_ttl = 12*60*60;
		dd->do_armask.do_armask_len = 0;
		dd->do_armask.do_armask_val = NULL;
	} else {
		// could not gather server address information
		free(dd);
		dd = 0;
	}

	return (dd);
}

// Add given directory to NIS+ cache
// returns 0 if failed; 1 for success

static int
add_to_cache(const directory_obj *dobj)
{
	fd_result res;
	int len;
	bool_t answer;
	char *buf;
	XDR xdrs;

	memset((char *)&res, 0, sizeof (res));

	// If cache request ends up with nis_cachemgr, then
	// nis_cachemgr expects 'res' to contain serialized form
	// of directory object.

	// get size of directory object
	len = (int) xdr_sizeof((xdrproc_t)xdr_directory_obj, (void *)dobj);
	buf = (char *)malloc(len * sizeof (char));
	if (!buf)
		return (0);

	// XFN-encode directory into buffer
	xdrmem_create(&xdrs, buf, len, XDR_ENCODE);
	if (!xdr_directory_obj(&xdrs, (directory_obj *)dobj)) {
		free(buf);
		xdr_destroy(&xdrs);
		return (0);
	}

	res.dir_data.dir_data_val = buf;
	res.dir_data.dir_data_len = len;

	// Make call to add directory object to NIS+ cache
	answer = __nis_CacheAddEntry(&res, (directory_obj *)dobj);

	free(buf);
	xdr_destroy(&xdrs);
	return (answer);
}


// Given directory and server
int
update_nisplus_cache(const char *directory_name,
    const char *server_name, const char *server_addr)
{
	directory_obj cached_dobj;
	int in_cache_p;

	in_cache_p = (__nis_CacheSearch((char *)directory_name, &cached_dobj)
	    == NIS_SUCCESS);

	if (in_cache_p &&
	    nis_dir_cmp((nis_name)directory_name, cached_dobj.do_name)
	    == SAME_NAME) {
		// Directory already in cache, do nothing
		xdr_free((xdrproc_t)xdr_directory_obj, (char *)&cached_dobj);
	} else {
		// Directory not in cache yet, add it
		directory_obj *new_dobj;

		if (in_cache_p)
			xdr_free((xdrproc_t)xdr_directory_obj,
			    (char *)&cached_dobj);

		new_dobj = new_nisplus_directory_object(directory_name,
		    server_name, server_addr);

		if (new_dobj) {
			add_to_cache(new_dobj);
			/* free contents */
			xdr_free((xdrproc_t)xdr_directory_obj,
			    (char *)new_dobj);
			delete new_dobj; /* free holder */
		} else
			return (0);  // Failed
	}


	return (1); // returns 1 for OK; 0 for error
}
