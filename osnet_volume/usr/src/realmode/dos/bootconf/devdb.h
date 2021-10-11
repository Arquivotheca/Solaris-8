/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * devdb.h -- public definitions for devdb module
 */

#ifndef	_DEVDB_H
#define	_DEVDB_H

#ident	"@(#)devdb.h	1.88	99/05/26 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <befext.h>	/* Get EISA resource flag masks			    */
#include <string.h>	/* Get strlen() prototype			    */
#include <board_res.h>	/* Get Board, Resource, devtrans and devprop	    */

extern const char *DevTypes[];		/* Table of device type names	    */
extern const int DevTypeCount;		/* Number of entries in the table   */

extern char *ResTypes[];		/* Resource types (for msgs & such) */
extern unsigned char ResTuples[];	/* Callback tuple lengths for same  */

/*
 *  Board record sizing routines:
 *
 *  Board records may be dynamically sized.  User routine must use malloc
 *  to allocate the initial buffer and call "ResetBoard_devdb" to record the
 *  initial buffer length.  Thereafter, calls of the form:
 *
 *	  bp = ResizeBoard_devdb(bp, len)
 *
 *  may be used to ensure that the buffer is large enough to concatenate
 *  a "len"-byte data structure to it's variable-length portion.  If not,
 *  "ResizeBoard_devdb" will reallocate the buffer and return its new address;
 *  NULL if it runs out of memory.
 *
 *  The "ResetBoard_devdb" routine is used to flush the contents of
 *  the variable-length portion of the target Board record.
 */

Board *ResizeBoard_devdb(Board *bp, unsigned len);
void ResetBoard_devdb(Board *bp);

/*
 * These functions are for manipulating resources in a board record
 */
Board *AddResource_devdb(Board *bp, unsigned short type, long start,
	long len);
int DelResource_devdb(Board *bp, Resource *trp);

/*
 *  Device property functions:
 *
 *  These may be used to set/get specific device properties for a board
 *  record (actually, they can be used with any list of "devprop" structs).
 */
int SetDevProp_devdb(devprop **list, char far *name, void far *value, int len,
	int bin);
int GetDevProp_devdb(devprop **list, char *name, char **value, int *len);


/*
 *  Device/driver translations:
 *
 *  These routines provide acceess to the device/driver translation tables.
 *  These tables are composed of information taken from the "devicedb/master"
 *  file.
 *
 *  Given a pointer to a Board record, the "Get" functions return the
 *  device's (full ASCII) name, the corresponding realmode driver's name,
 *  or a printable ASCII version of the device ID respectively.
 *
 *  The "Translate" functions provide direct lookup of the device/driver
 *  translation tables by driver name or device ID/bus type, respectively.
 */

char *GetDeviceName_devdb(Board *bp, int verbose);
char *GetDeviceId_devdb(Board *bp, char *buf);
unsigned long get_isa_id_devdb();

devtrans *TranslateDriver_devdb(char *dp);
devtrans *TranslateDevice_devdb(unsigned long id, unsigned char bt);
void master_file_update_devdb(char *master_file_line);

/*
 *  Module initialization
 */
void init_devdb();

#ifdef	__cplusplus
}
#endif

#endif	/* _DEVDB_H */
