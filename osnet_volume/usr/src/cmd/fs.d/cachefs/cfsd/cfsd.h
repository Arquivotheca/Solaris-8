/*
 *
 *			cfsd.h
 *
 * Include file for the cfsd
 */

#ident   "@(#)cfsd.h 1.1     96/02/23 SMI"
/* Copyright (c) 1994 by Sun Microsystems, Inc. */

#define	CFSDStrMax  256
void *cfsd_calloc(int size);
void cfsd_free(void *free_ptr);
void cfsd_sleep(int sec);
