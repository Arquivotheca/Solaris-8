/*
 *	db_headers.h
 *
 *	Copyright (c) 1988-1999 by Sun Microsystems, Inc.
 *	All Rights Reserved.
 */

#pragma ident	"@(#)db_headers.h	1.10	99/06/03 SMI"

#ifndef _DB_HEADERS_H
#define	_DB_HEADERS_H

#include <rpc/rpc.h>
#include <syslog.h>
#include <stdlib.h>
#include <setjmp.h>

extern int verbose;
extern jmp_buf dbenv;

#define	FATAL(msg, fcode) { syslog(LOG_ERR, "ERROR: %s", (msg)); \
		    longjmp(dbenv, (int) (fcode)); }
#define	WARNING(x) { syslog(LOG_ERR, "WARNING: %s", (x)); }

#define	WARNING_M(x) { syslog(LOG_ERR, "WARNING: %s: %m", (x)); }


enum db_status {DB_SUCCESS, DB_NOTFOUND, DB_NOTUNIQUE,
		    DB_BADTABLE, DB_BADQUERY, DB_BADOBJECT,
		DB_MEMORY_LIMIT, DB_STORAGE_LIMIT, DB_INTERNAL_ERROR,
		DB_BADDICTIONARY, DB_SYNC_FAILED};
typedef enum db_status db_status;

enum db_action {DB_LOOKUP, DB_REMOVE, DB_ADD, DB_FIRST, DB_NEXT, DB_ALL,
			DB_RESET_NEXT, DB_ADD_NOLOG,
			DB_ADD_NOSYNC, DB_REMOVE_NOSYNC };
typedef enum db_action db_action;

#endif _DB_HEADERS_H
