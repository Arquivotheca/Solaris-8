/*
 *	db_dictlog_c.x
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

%#pragma ident	"@(#)db_dictlog_c.x	1.4	92/07/14 SMI"

#if RPC_HDR
%#ifndef _DB_DICTLOG_H
%#define _DB_DICTLOG_H

%
%/* A log entry that describes an action to be performed and its parameters. */
%
#endif RPC_HDR

#if RPC_HDR || RPC_XDR
#ifdef USINGC
%#include "db_vers_c.h"
#else
%#include "db_vers.h"
%#include "db_pickle.h"
#endif USINGC
%#include <rpcsvc/nis.h>
#endif RPC_HDR

%#define DB_ADD_TABLE 1
%#define DB_REMOVE_TABLE 2

#if RPC_HDR || RPC_XDR
#ifdef USINGC
struct db_dictlog_entry {
  vers aversion;                    /* version of log entry */
  int       action;                 /* action to be invoked */
  string table_name<>;                /* table_name supplied with action */
  table_obj *table_object;          /* object involved in action (if any) */
  struct db_dictlog_entry *next;    /* Used in constructing list */
  vers bversion;                    /* sanity check;should be same as aversion*/
};
typedef  struct db_dictlog_entry* db_dictlog_entry_p;
#endif USINGC
#endif RPC_HDR

#ifdef USINGC
#if RPC_HDR
%bool_t xdr_table_obj();
#endif
#endif USINGC

#ifndef USINGC
#ifdef RPC_HDR
%class db_dictlog_entry {
%  vers aversion;                     /* version of log entry */
%  int action;                        /* action to be invoked */
%  char *table_name;                  /* table_name supplied with action (if any) */
%  table_obj *table_object;           /* object involved in action (if any) */
%  db_dictlog_entry *next;                /* Used in constructing list */
%  vers bversion;                     /* sanity check */
% public:
%
%/*Constructor:  Create an empty log entry, with no table_name and not object */
%  db_dictlog_entry() { table_name = NULL, table_object = NULL; next = NULL; }
%
%/*Constructor:  Create a log entry using the given parameters.  Note that
%  pointers to table_name and table_object are simply assigned, not copied. */
%  db_dictlog_entry(int, vers *, char*, table_obj*);
%
%  ~db_dictlog_entry();
%
%/* Print this log entry to stdout */
%  void print();
%
%/* Accessor: return version of log entry */
%  vers *get_version()  { return( &aversion ); }
%
%/* Accessor: return pointer to action of log entry */
%  int get_action()  { return( action ); }
%
%/* Accessor:  return pointer to table_name part of log entry */
%  char* get_table_name()  { return( table_name ); }
%
%/* Predicate:  return whether log entry is complete and not truncated */
%  bool_t sane() { return( aversion.equal( &bversion ) ); }
%
%/* Accessor:  return pointer to copy of object in log entry */
%  table_obj *get_table_object()  { return( table_object ); }
%
%/* Accessor:  return pointer to to next log entry */
%  db_dictlog_entry * getnextptr()  { return( next ); }
%
%/* Accessor:  return pointer to copy of object in log entry */
%  void setnextptr( db_dictlog_entry *p )  { next = p; }
%};
%#ifdef __cplusplus
%extern "C" bool_t xdr_db_dictlog_entry(XDR*, db_dictlog_entry*);
%#elif __STDC__
%extern bool_t xdr_db_dictlog_entry(XDR*, db_dictlog_entry*);
%#endif
%typedef class db_dictlog_entry * db_dictlog_entry_p;
#endif RPC_HDR
#endif USINGC

struct db_dictlog_list {
  db_dictlog_entry_p list<>;
};
  
#ifndef USINGC
#ifdef RPC_HDR
%class db_dictlog: public pickle_file {
% public:
%
%/* Constructor:  create log file; default is PICKLE_READ mode. */
%  db_dictlog( char* f, pickle_mode m = PICKLE_READ ): pickle_file(f,m)  {}
%
%/* Execute given function 'func' on log.
%  function takes as arguments: pointer to log entry, character pointer to 
%  another argument, and pointer to an integer, which is used as a counter.
%  'func' should increment this value for each successful application.
%  The log is traversed until either 'func' returns FALSE, or when the log
%  is exhausted.  The second argument to 'execute_on_log' is passed as the
%  second argument to 'func'. The third argument, 'clean' determines whether
%  the log entry is deleted after the function has been applied.
%  Returns the number of times that 'func' incremented its third argument. */
%  int execute_on_log( bool_t(* func) (db_dictlog_entry *, char *, int *), 
%		      char *, bool_t = TRUE );
%
%/* Print contents of log file to stdout */
%  int print();
%
%/*Append given log entry to log. */
%  int append( db_dictlog_entry * );
%
%/* Return the next element in current log; return NULL if end of log or error.
%   Log must have been opened for READ. */
%  db_dictlog_entry *get();
%
%};
#endif RPC_HDR
#endif USINGC

#if RPC_HDR
%#endif _DB_DICTLOG_H
#endif RPC_HDR
