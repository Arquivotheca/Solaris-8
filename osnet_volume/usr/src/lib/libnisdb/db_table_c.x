/*
 *	db_table_c.x
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

%#pragma ident	"@(#)db_table_c.x	1.7	96/01/05 SMI"

#if RPC_HDR
%#ifndef _DB_TABLE_H
%#define _DB_TABLE_H

#ifdef USINGC
%#include "db_entry_c.h"
#else
%#include "db_entry.h"
#endif USINGC
#endif RPC_HDR

typedef long entryp;      /* specifies location of an entry within table */

struct db_free_entry {
  entryp where;
  struct db_free_entry *next;
};

typedef struct db_free_entry * db_free_entry_p;

#if RPC_HDR || RPC_XDR
#ifdef USINGC
struct db_free_list {
  db_free_entry_p head;
  long count;
};
typedef struct db_free_list * db_free_list_p;
#endif USINGC
#endif RPC_HDR

#ifndef USINGC
#ifdef RPC_HDR
%class db_free_list {
%  db_free_entry_p head;
%  long count;
% public:
%  db_free_list()  {head = NULL; count = 0; }   /* free list constructor */
% 
%  ~db_free_list();
%
%  void reset();   /* empty contents of free list */
%
%  void init() {head = NULL; count=0;}          /* Empty free list */
%
%/* Returns the location of a free entry, or NULL, if there aren't any. */
%  entryp pop();
%
%/* Adds given location to the free list.  
%   Returns TRUE if successful, FALSE otherwise (when out of memory). */
%  bool_t push( entryp );
%
%/* Returns in a vector the information in the free list.
%   Vector returned is of form: <n free cells><n1><n2><loc1>,..<locn>.
%   Leave the first 'n' cells free.
%   n1 is the number of entries that should be in the freelist.
%   n2 is the number of entries actually found in the freelist.
%   <loc1...locn> are the entries.   n2 <= n1 because we never count beyond n1.
%   It is up to the caller to free the returned vector when he is through. */
% long* stats( int n );
%};
#endif RPC_HDR
#endif USINGC

#if RPC_HDR || RPC_XDR
#ifdef USINGC
struct db_table
{
  entry_object_p tab <>;
  long last_used;        /* last entry used; maintained for quick insertion */
  long count;            /* measures fullness of table */
  db_free_list freelist;
};
typedef struct db_table * db_table_p;

#endif USINGC
#endif RPC_HDR

#ifndef USINGC
#ifdef RPC_HDR
%class db_table
%{
%  long table_size;
%  entry_object_p *tab;   /* pointer to array of pointers to entry objects */
%  long last_used;        /* last entry used; maintained for quick insertion */
%  long count;            /* measures fullness of table */
%  db_free_list freelist;
%  void grow();           /* Expand the table.  
%			    Fatal error if insufficient error. */
%
% public:
%  db_table();            /* constructor for brand new, empty table. */
%  db_table( char * );    /* constructor for creating a table by loading
%			    in an existing one. */
%
%/* Return how many entries there are in table. */
%  long fullness() { return count; }
%
%/* Deletes table, entries, and free list */
%  ~db_table();
%
%/* empties table by deleting all entries and other associated data structures */
%   void reset();
%
%  int dump( char *);
%
%/* Returns whether location is valid. */
%  bool_t entry_exists_p( entryp i )  
%    { if( tab != NULL && i < table_size ) return( tab[i] != NULL ); 
%      else return (0); }
%
%/* Returns table size. */
%  long getsize()  { return table_size; }
%
%/* Returns the first entry in table, also return its position in
%   'where'.  Return NULL in both if no next entry is found. */
%  entry_object_p first_entry( entryp * where );
%
%/* Returns the next entry in table from 'prev', also return its position in
%   'newentry'.  Return NULL in both if no next entry is found. */
%  entry_object_p next_entry( entryp, entryp* );
%
%/* Returns entry at location 'where', NULL if location is invalid. */
%  entry_object_p get_entry( entryp );
%
%/* Adds given entry to table in first available slot (either look in freelist
%   or add to end of table) and return the the position of where the record
%   is placed. 'count' is incremented if entry is added. Table may grow
%   as a side-effect of the addition. Copy is made of the input. */
%  entryp add_entry( entry_object_p );
%
% /* Replaces object at specified location by given entry.  
%   Returns TRUE if replacement successful; FALSE otherwise.
%   There must something already at the specified location, otherwise,
%   replacement fails. Copy is not made of the input. 
%   The pre-existing entry is freed.*/
%  bool_t replace_entry( entryp, entry_object_p );
%
%/* Deletes entry at specified location.  Returns TRUE if location is valid;
%   FALSE if location is invalid, or the freed location cannot be added to 
%   the freelist.  'count' is decremented if the deletion occurs.  The object
%   at that location is freed. */
%  bool_t delete_entry( entryp );
%
%/* Returns statistics of table.
%   <table_size><last_used><count>[freelist].
%   It is up to the caller to free the returned vector when his is through
%   The free list is included if 'fl' is TRUE. */
%long * stats( bool_t fl );
%};
%#ifdef __cplusplus
%extern "C" bool_t xdr_db_table( XDR*, db_table*);
%#elif __STDC__
%extern bool_t xdr_db_table(XDR*, db_table*);
%#endif
%typedef class db_table * db_table_p;
#endif RPC_HDR
#endif USINGC

#if RPC_HDR
%#endif _DB_TABLE_H
#endif RPC_HDR
