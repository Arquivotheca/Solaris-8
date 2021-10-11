/*
 *	db_item_c.x
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

%#pragma ident	"@(#)db_item_c.x	1.5	92/09/21 SMI"

%
% /* A 'counted' string. */
%

#if RPC_HDR
%#ifndef _DB_ITEM_H
%#define _DB_ITEM_H
#endif RPC_HDR

#if RPC_HDR || RPC_XDR
#ifdef USINGC
struct item{
  char itemvalue<>;
};
#endif USINGC
#endif RPC_HDR

#ifndef USINGC
#ifdef RPC_HDR
%class item {
%  int len;
%  char *value;
% public:
%/* Constructor: creates item using given character sequence and length */
%  item( char* str, int len);
%
%/* Constructor: creates item by copying given item */
%  item( item* );
%
%/* Constructor: creates empty item (zero length and null value). */
%  item() {len = 0; value = NULL;}
%
%/* Destructor: recover space occupied by characters and delete item. */
%  ~item() {delete value;}
%
%/* Equality test.  'casein' TRUE means case insensitive test. */
%  bool_t equal( item *, bool_t casein = FALSE );
%
%/* Equality test.  'casein' TRUE means case insensitive test. */
%  bool_t equal( char *, int, bool_t casein = FALSE );
%
%/* Assignment:  update item by setting pointers.  No space is allocated. */
%  void update( char* str, int n) {len = n; value = str;}
%
%/* Return contents of item. */
%  void get_value( char** s, int * n ) { *s = value; *n=len;}
%
%/* Prints contents of item to stdout */
%  void print();
%
%/* Return hash value.  'casein' TRUE means case insensitive test. */
%  unsigned int get_hashval( bool_t casein = FALSE );
%};
#endif RPC_HDR
#endif USINGC

#if RPC_HDR
%#endif _DB_ITEM_H
#endif RPC_HDR
