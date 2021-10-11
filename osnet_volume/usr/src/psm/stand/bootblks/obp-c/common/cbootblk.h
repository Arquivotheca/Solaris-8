/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _CBOOTBLK_H
#define	_CBOOTBLK_H

#pragma ident	"@(#)cbootblk.h	1.9	96/12/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern int openfile(char *, char *);
extern int closefile(int);
extern int readfile(int, char *, int);
extern void seekfile(int, off_t);

extern void exit(void);
extern void puts(char *);
extern int utox(char *p, u_int n);

extern void fw_init(void *);

extern char *getbootdevice(char *);

extern int devbread(void *, void *, int, int);
extern void *devopen(char *);
extern int devclose(void *);
extern void get_rootfs_start(char *device);
extern u_int fdisk2rootfs(u_int offset);

extern void bcopy(const void *, void *, size_t);
extern void bzero(void *, size_t);
extern int strcmp(const char *, const char *);
extern int strncmp(const char *, const char *, size_t);
extern size_t strlen(const char *);
extern char *strcpy(char *, const char *);

extern void main(void *);
extern void exitto(void *, void *);

extern char ident[];
extern char fscompname[];
extern unsigned long read_elf_file(int, char *);
void sync_instruction_memory(caddr_t, u_int);

#ifdef	__cplusplus
}
#endif

#endif	/* _CBOOTBLK_H */
