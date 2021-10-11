/*
 *	db_entry_c.x
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

%#pragma ident	"@(#)db_entry_c.x	1.6	97/04/22 SMI"

/*
 * Some manifest constants, chosen to maximize flexibility without
 * plugging the wire full of data.
 */

#if RPC_HDR
%#ifndef _DB_NIS_H
%#define _DB_NIS_H

%#include <rpcsvc/nis.h>
#endif RPC_HDR

#ifdef USINGC
enum db_status {DB_SUCCESS, DB_NOTFOUND, DB_NOTUNIQUE,
		DB_BADTABLE, DB_BADQUERY, DB_BADOBJECT,
		DB_MEMORY_LIMIT, DB_STORAGE_LIMIT, DB_INTERNAL_ERROR};

enum db_action {DB_LOOKUP, DB_REMOVE, DB_ADD, DB_FIRST, DB_NEXT,
			DB_ALL, DB_RESET_NEXT, DB_ADD_NOLOG,
			DB_ADD_NOSYNC, DB_REMOVE_NOSYNC };
#endif USINGC

/* Make alias to NIS definition */

typedef entry_obj entry_object;
typedef entry_object * entry_object_p;

typedef nis_name db_stringname;
typedef nis_attr db_attrname;          /* What the database knows it as */


/*  nis_dba.x ----------------------------- */

/* 
 * Structure definitions for the parameters and results of the actual
 * NIS DBA calls
 *
 * This is the standard result (in the protocol) of most of the nis 
 * requests.
 */

/*typedef long db_next_desc;*/

typedef opaque db_next_desc<>;            /* opaque string */

struct db_result {
	db_status 	status;		/* The status itself 	 */
	db_next_desc    nextinfo;       /* for first/next sequence */
	entry_object_p	objects<>;	/* And the objects found */
	long		ticks;		/* for statistics	 */
};

struct db_request {
  db_stringname table_name;
  db_attrname  attrs<NIS_MAXCOLUMNS>;
  entry_object * obj;      /* only used for addition */
};

#ifndef USINGC
%#ifdef __cplusplus
%extern "C"  entry_object * new_entry( entry_object*);
%extern "C"  bool_t copy_entry ( entry_object*, entry_object*);
%extern "C"  void free_entry (entry_object*);
%#elif __STDC__
%extern entry_object * new_entry( entry_object*);
%extern bool_t copy_entry ( entry_object*, entry_object*);
%extern void free_entry (entry_object*);
%#endif
#else
#if RPC_HDR
%extern void print_entry();
%extern char copy_entry();
%extern void free_entry();
%extern void new_entry();
#endif RPC_HDR
#endif USINGC

#if RPC_HDR
%#endif _DB_NIS_H
#endif RPC_HDR
