/*
 *	db_query_c.x
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

%#pragma ident	"@(#)db_query_c.x	1.4	92/07/14 SMI"
 
#if RPC_HDR
%#ifndef _DB_QUERY_H
%#define _DB_QUERY_H

%
%/* db_query is the structure that contains the components of a query.
%   It contains the values for searching the indices. */
%

#ifdef USINGC
%#include "db_item_c.h"
%#include "db_entry_c.h"
%#include "db_scheme_c.h"
#else
%#include "db_item.h"
%#include "db_entry.h"
%#include "db_scheme.h"
#endif USINGC
#endif RPC_HDR

%/* A component of a query */
struct db_qcomp {
  int which_index;             /* identifies which index is being used */
  item* index_value;           /* value to be used in search */
};

#if RPC_HDR || RPC_XDR
#ifdef USINGC
struct db_query {
  db_qcomp components<>;
};
#endif USINGC
#endif RPC_HDR

#ifndef USINGC
#ifdef RPC_HDR
%
%class db_query {
%protected:
%  int num_components;
%  db_qcomp* components;
% public:
%/* Accessor:  returns number of components */
%  int size() { return num_components; }
%
%/* Accessor:  returns location of start of query */
%   db_qcomp* queryloc() { return components; }
%
%
%/* Null constructor:  returns empty empty query. */
%  db_query() { num_components = 0; components = NULL; }
%
%/* Returns a db_query containing the index values as obtained from
%   'attrlist.' */
%  db_query( db_scheme*, int, nis_attr* );
%
%/* Returns a newly db_query containing the index values as
%   obtained from the given object.  The object itself, 
%   along with information on the scheme given, will determine 
%   which values are extracted from the object and placed into the query.
%   Returns an empty query if 'obj' is not a valid entry.
%   Note that space is allocated for the query and the index values 
%   (i.e. do not share pointers with strings in 'obj'.)
%*/
%  db_query( db_scheme*, entry_object_p );
%
%  /* destructor (frees all components) */
%  ~db_query();
%
%  /* clear component structure */
%  void clear_components( int );
%
%/* Print all components of this query to stdout. */
%  void print();
%};
%typedef class db_query * db_query_p;
#endif RPC_HDR
#endif USINGC

#if RPC_HDR
%#endif _DB_QUERY_H
#endif RPC_HDR
