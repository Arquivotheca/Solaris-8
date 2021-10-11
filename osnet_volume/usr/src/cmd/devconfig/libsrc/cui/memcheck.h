#pragma ident "@(#)memcheck.h   1.3     92/11/25 SMI"

/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved. Sun
 * considers its source code as an unpublished, proprietary trade secret, and
 * it is available only under strict license provisions.  This copyright
 * notice is placed here only to protect Sun in the event the source is
 * deemed a published work.  Dissassembly, decompilation, or other means of
 * reducing the object code to human readable form is prohibited by the
 * license agreement under which this code is provided to the user or company
 * in possession of this copy.
 * 
 * RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the Government
 * is subject to restrictions as set forth in subparagraph (c)(1)(ii) of the
 * Rights in Technical Data and Computer Software clause at DFARS 52.227-7013
 * and in similar clauses in the FAR and NASA FAR Supplement.
 */

/*
 *	$RCSfile: memcheck.h $ $Revision: 1.2 $ $Date: 1992/09/12 15:32:05 $
 *			  memory-checking stuff
 */

#ifndef _MEMCHECK_H
#define _MEMCHECK_H


#ifdef MEMDB

void *emalloc(size_t size, char *file, int line);
void  efree(void *ptr, char *file, int line);
char *CUI_newString(char *string);
void  recordMalloc(void *ptr, size_t size, char *file, int line);
void  recordFree(void *ptr, char *file, int line);
int   memSetRecording(int val);
void  memWriteStats(void);

#define CUI_malloc(x)		emalloc(x, __FILE__, __LINE__)
#define CUI_free(x) 		efree(x, __FILE__, __LINE__)
#define CHK_MALLOC(x, y)	recordMalloc(x, y, __FILE__, __LINE__)
#define CHK_FREE(x) 		recordFree(x, __FILE__, __LINE__)
#define MEMCHECK(x) 		memSetRecording(x)
#define MEMDUMP()			memWriteStats()
#define MEMHINT()		 { \
	MEMfile == NULL ? MEMfile = __FILE__, MEMline = __LINE__ : 0; }

extern char *MEMfile;	/* hint for memory-checker */
extern int	MEMline;	/* hint for memory-checker */

#else

void *emalloc(size_t size);
void  efree(void *ptr);
char *CUI_newString(char *string);

#define CUI_malloc(x)		emalloc(x)
#define CUI_free(x) 		efree(x)
#define CHK_MALLOC(x, y)
#define CHK_FREE(x)
#define MEMCHECK(x)
#define MEMDUMP()
#define MEMHINT()

#endif /* MEMDB */

#endif /* _MEMCHECK_H */

