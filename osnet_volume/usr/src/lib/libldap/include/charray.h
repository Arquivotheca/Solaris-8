/*
 *
 * Copyright 02/06/98 Sun Microsystems, Inc. All Rights Reserved
 *
 */

#pragma ident   "@(#)charray.h 1.2     98/02/06 SMI"

extern void charray_add(char ***a, char *s);
extern void charray_add_uniq(char ***a, char *s);
extern void charray_add_case_uniq(char ***a, char *s);
extern void charray_merge(char ***a, char **s);
extern void charray_free( char **array );
extern int charray_inlist( char **a, char *s);
extern char ** charray_dup( char **a );
extern int charray_count( char **a);
extern char ** str2charray( char *str, char *brkstr );
extern char ** str2charray2( char *str, char *brkstr, int *NbItems );
extern char * ch_strdup( char *s1 );
extern void charray_sort(char **a, int (*comp_func)(const char *, const char *));
extern int  charray_pos(char **a, char *s);



