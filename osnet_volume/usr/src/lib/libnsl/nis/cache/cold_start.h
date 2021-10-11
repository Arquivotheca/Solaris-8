/*
 *	Copyright (c) 1996, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	__COLD_START_H
#define	__COLD_START_H

#pragma ident	"@(#)cold_start.h	1.2	96/05/22 SMI"

extern bool_t readColdStartFile(char *fileName, directory_obj *dobj);
extern bool_t __nis_writeColdStartFile(char *fileName, directory_obj *dobj);

extern "C" bool_t writeColdStartFile_unsafe(directory_obj *dobj);
extern "C" bool_t writeColdStartFile(directory_obj *dobj);

#endif	/* __COLD_START_H */
