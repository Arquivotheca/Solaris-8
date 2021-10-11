/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_NCADOORHDR_H
#define	_INET_NCADOORHDR_H

#pragma ident	"@(#)ncadoorhdr.h	1.4	99/12/01 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _KERNEL
#include <stddef.h>
#endif _KERNEL

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define	ONE_KB				(1024)
#define	NCA_IO_MAX_SIZE			(256 * ONE_KB)
#define	NCA_IO_OFFSET			(sizeof (nca_io_t))

/*
 * Defines the data structures used by NCA and Webservers.
 */

typedef enum {
	/*
	 * NCA-to-HTTP-server protocol operation values:
	 */
	http_op		= 100,	/* NCA<>HTTP normal request/response */
	error_op	= 101,	/* NCA<-HTTP server error */
	error_retry_op	= 102,	/* NCA<-HTTP server transient error */
	resource_op	= 120,	/* NCA->HTTP server release resources */
	timeout_op	= 150,	/* NCA<-HTTP server timed out */
	/*
	 * NCA-to-Logging-server protocol operation values:
	 */
	log_op		= 10000,	/* NCA->Logger normal request */
	log_ok_op	= 10001,	/* NCA<-Logger request ok */
	log_error_op	= 10002,	/* NCA<-Logger request error */
	log_op_fiov	= 10003		/* NCA<>Logger file i/o vector */
} nca_op_t;

typedef enum {
	NCA_HTTP_VERSION1 = 1001,	/* NCA-to-HTTP-server protocol */
	NCA_LOG_VERSION1 = 5001,	/* NCA-to-Logging-server protocol */
	NCA_LOG_VERSION2 = 5002		/* with in-kernel logging support */
	/*
	 * Note: Other version values are reserved for other client-to-server
	 * Solaris door base protocols and as these protocols may or may not
	 * be for use with NCA a new datatype (door_version_t ?) will be
	 * defined.
	 */
} nca_version_t;

#define	HTTP_ERR	(-1)
#define	HTTP_0_0	0x00000
#define	HTTP_0_9	0x00009
#define	HTTP_1_0	0x10000
#define	HTTP_1_1	0x10001

typedef	uint32_t	fd_t;		/* File descriptor type */
typedef uint32_t	nca_tag_t;	/* Request id */
typedef uint32_t	nca_offset_t;	/* Offset */

typedef struct nca_io_t {

	nca_version_t	version;	/* version number */
	nca_op_t	op;		/* type of operation */
	nca_tag_t	tag;		/* analogous to req id */

	uint32_t	nca_tid;	/* NCA kernel thread id */

	uint8_t		more;		/* more chunks to follow */
	uint8_t		first;		/* first chunk for tag */

	uint8_t		advisory;	/* ask before using cache */
	uint8_t		nocache;	/* don't cache */

	uint32_t	filename_len;	/* length of file name */
	nca_offset_t	filename;	/* pointer into data */

	uint32_t	peer_len;	/* sockaddr of HTTP client */
	nca_offset_t	peer;		/* pointer into data */

	uint32_t	local_len;	/* sockaddr of NCA server */
	nca_offset_t	local;		/* pointer into data */

	uint32_t	http_len;	/* length of HTTP data */
	nca_offset_t	http_data;	/* pointer into data */

	uint32_t	trailer_len;	/* length of HTTP trailer */
	nca_offset_t	trailer;	/* pointer into data */

	/*
	 * Following this structure is optional meta data (i.e. filename,
	 * peer and local sockaddr), HTTP request/response data, and HTTP
	 * trailer data. All nca_offset_t's above are byte offsets from the
	 * end of this structure.
	 *
	 * Note: sockaddr meta data are IPv4 addresses, future revisions
	 * of the NCA-to-HTTP-server protocol will support IPv6.  So, the
	 * length of the sockaddr meta data must be honored as it will be
	 * increased for future IPv6 support.
	 */

} nca_io_t;

typedef enum {
	NCA_UNKNOWN,
	NCA_OPTIONS,
	NCA_GET,
	NCA_HEAD,
	NCA_POST,
	NCA_PUT,
	NCA_DELETE,
	NCA_TRACE
} nca_http_method_t;

typedef enum {
	HS_OK = 200,
	HS_CREATED = 201,
	HS_ACCEPTED = 202,
	HS_PARTIAL_CONTENT = 206,
	HS_MOVED_PERMANENT = 301,
	HS_MOVED = 302,
	HS_NOT_MODIFIED = 304,
	HS_BAD_REQUEST = 400,
	HS_AUTH_REQUIRED = 401,
	HS_FORBIDDEN = 403,
	HS_NOT_FOUND = 404,
	HS_PRECONDITION_FAILED = 412,
	HS_SERVER_ERROR = 500,
	HS_NOT_IMPLEMENTED = 501,
	HS_SERVICE_UNAVAILABLE = 503,
	HS_CONNECTION_CLOSED = 1000
} nca_http_status_code;

/* httpd (miss user space daemon) is attached to this door */
#define	MISS_DOOR_FILE	"/var/run/nca_httpd_1.door"

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_NCADOORHDR_H */
