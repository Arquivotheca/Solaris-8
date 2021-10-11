/*
 *	db_scheme_c.x
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

%#pragma ident	"@(#)db_scheme_c.x	1.4	92/07/14 SMI"

#if RPC_HDR
%#ifndef _DB_SCHEMA_H
%#define _DB_SCHEMA_H

#ifdef USINGC
%#include "db_item_c.h"
%#include "db_entry_c.h"
#else
%#include "db_item.h"
%#include "db_entry.h"
#endif USINGC

const DB_KEY_CASE = TA_CASE;

#endif RPC_HDR

%/* Positional information of where field starts within record 
%   and its maximum length in terms of bytes. */
struct db_posn_info {
  short int start_column;
  short int max_len;
};

%/* Description of a key */
struct db_key_desc {
  item *key_name;
  unsigned long key_flags;  /* corresponds to tc_flags in table_col defn */
  int column_number;        /* column within data structure */
  db_posn_info where;       /* where within record entry is 'key' located */
  short int store_type;     /* ISAM or SS ?  maybe useless */
};

%/* Description of the data field. */
struct db_data_desc {
  db_posn_info where;       /* where within record entry is 'data' located */
  short int store_type;     /* ISAM or SS ? maybe useless */
};

%/* A scheme is a description of the fields of a table. */

#if RPC_HDR || RPC_XDR
#ifdef USINGC

struct db_scheme {
  db_key_desc keys<>;
  short int max_columns;  /* applies to data only ? */
  db_data_desc data;
};

typedef struct db_scheme  * db_scheme_p;
#endif USINGC
#endif RPC_HDR

#ifndef USINGC
#ifdef RPC_HDR
%
%class db_scheme {
% protected:
%  struct {
%	int keys_len;
%	db_key_desc *keys_val;
%  } keys;
%  short int max_columns;  /* applies to data only ? */
%  db_data_desc data;
%
% public:
%/* Accessor: return number of keys in scheme. */
%  int numkeys() { return keys.keys_len; }
%
%/* Accessor:  return location of array of key_desc's. */
%  db_key_desc* keyloc () { return keys.keys_val; }
%  
%/* Constructor:  create empty scheme */
%  db_scheme() { keys.keys_len = 0; keys.keys_val = NULL; }
%
%/* Constructor:  create new scheme by making copy of 'orig'.
%   All items within old scheme are also copied (i.e. no shared pointers). */
%  db_scheme( db_scheme* orig );
%
%/* Constructor:  create new sheme by using information in 'zdesc'. */
%  db_scheme( table_obj * );
%
%/* Destructor:  delete all keys associated with scheme and scheme itself. */
%  ~db_scheme();
%
%/* Free space occupied by columns. */
%  void clear_columns( int );
%
%/* Predicate:  return whether given string is one of the index names
%   of this scheme.  If so, return in 'result' the index's number. */
%  bool_t find_index( char*, int* );
%
%/* Print out description of table. */
%  void print();
%};

%typedef class db_scheme * db_scheme_p;
#endif RPC_HDR
#endif USINGC

#if RPC_HDR
%#endif _DB_SCHEMA_H

#endif RPC_HDR
