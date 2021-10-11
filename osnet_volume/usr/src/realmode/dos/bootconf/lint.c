/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)lint.c   1.1   98/05/14 SMI"

#ifdef lint
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <sys/stat.h>
#include <setjmp.h>
#include <time.h>

/*ARGSUSED*/
int _chdir(const char *a)
{ return 0; }
/*ARGSUSED*/
int _close(int a)
{ return 0; }
unsigned char _ctype[1024];
/*ARGSUSED*/
unsigned _dos_findfirst(const char *a, unsigned b, struct _find_t *c)
{ return 0; }
/*ARGSUSED*/
unsigned _dos_findnext(struct _find_t *a)
{ return 0; }
/*ARGSUSED*/
void (*_dos_getvect(unsigned a))()
{ return 0; }
/*ARGSUSED*/
void _dos_setvect(unsigned a, void (*b)())
{ return; }
/*ARGSUSED*/
int _filbuf(FILE *a)
{ return 0; }
/*ARGSUSED*/
int _flsbuf(int a, FILE *b)
{ return 0; }
/*ARGSUSED*/
int _fstat(int a, struct _stat *b)
{ return 0; }
/*ARGSUSED*/
int _getch(void)
{ return 0; }
/*ARGSUSED*/
int _inp(unsigned a)
{ return 0; }
/*ARGSUSED*/
unsigned _inpw(unsigned a)
{ return 0; }
/*ARGSUSED*/
int _int86(int a, union _REGS *b , union _REGS *c)
{ return 0; }
/*ARGSUSED*/
int _int86x(int a, union _REGS *b, union _REGS *c, struct _SREGS *d)
{ return 0; }
FILE _iob[1024];
/*ARGSUSED*/
int _kbhit(void)
{ return 0; }
/*ARGSUSED*/
long _lseek(int a, long b, int c)
{ return 0; }
/*ARGSUSED*/
int _open(const char *a, int b, ...)
{ return 0; }
unsigned char _osmajor;
unsigned char _osminor;
/*ARGSUSED*/
int _outp(unsigned a, int b)
{ return 0; }
/*ARGSUSED*/
unsigned _outpw(unsigned a, unsigned b)
{ return 0; }
/*ARGSUSED*/
int _read(int a, void *b, unsigned int c)
{ return 0; }
/*ARGSUSED*/
void _segread(struct _SREGS *a)
{ return; }
/*ARGSUSED*/
char *_strlwr(char *a)
{ return 0; }
/*ARGSUSED*/
int _strnicmp(const char *a, const char *b, unsigned int c)
{ return 0; }
/*ARGSUSED*/
int _write(int a, const void * b, unsigned int c)
{ return 0; }
/*ARGSUSED*/
int abs(int a)
{ return 0; }
/*ARGSUSED*/
double atof(const char *a)
{ return (double)0; }
/*ARGSUSED*/
int atoi(const char *a)
{ return 0; }
/*ARGSUSED*/
long atol(const char *a)
{ return 0; }
/*ARGSUSED*/
void *calloc(size_t a, size_t b)
{ return 0; }
/*ARGSUSED*/
int close(int a)
{ return 0; }
volatile int errno;
/*ARGSUSED*/
void exit(int a)
{ return; }
/*ARGSUSED*/
int fclose(FILE *a)
{ return 0; }
/*ARGSUSED*/
int fflush(FILE *a)
{ return 0; }
/*ARGSUSED*/
char *fgets(char *a, int b, FILE *c)
{ return 0; }
/*ARGSUSED*/
FILE *fopen(const char *a, const char *b)
{ return 0; }
/*ARGSUSED*/
int fputc(int a, FILE *b)
{ return 0; }
/*ARGSUSED*/
int fputs(const char *a, FILE *b)
{ return 0; }
/*ARGSUSED*/
size_t fread(void *a, size_t b, size_t c, FILE * d)
{ return 0; }
/*ARGSUSED*/
void free(void *a)
{ return; }
/*ARGSUSED*/
size_t fwrite(const void *a, size_t b, size_t c, FILE *d)
{ return 0; }
/*ARGSUSED*/
void longjmp(unsigned int *a, int b)
{ return; }
/*ARGSUSED*/
void *malloc(size_t a)
{ return 0; }
/*ARGSUSED*/
int memcmp(const void *a, const void *b, size_t c)
{ return 0; }
/*ARGSUSED*/
void * memcpy(void *a, const void *b, size_t c)
{ return 0; }
/*ARGSUSED*/
void * memset(void *a, int b, size_t c)
{ return 0; }
/*ARGSUSED*/
void qsort(void *a, size_t b, size_t c, int (*d) (const void *aa, const void *bb))
{ return ; }
/*ARGSUSED*/
void *realloc(void *a, size_t b)
{ return 0; }
/*ARGSUSED*/
void rewind(FILE *a)
{ return; }
/*ARGSUSED*/
void setbuf(FILE *a, char * b)
{ return; }
/*ARGSUSED*/
int setjmp(jmp_buf a)
{ return 0; }
/*ARGSUSED*/
int setvbuf(FILE *a, char *b, int c, size_t d)
{ return 0; }
/*ARGSUSED*/
int sscanf(const char *a, const char *b, ...)
{ return 0; }
/*ARGSUSED*/
char * strcat(char * a, const char *b)
{ return 0; }
/*ARGSUSED*/
char * strchr(const char *a, int b)
{ return 0; }
/*ARGSUSED*/
int strcmp(const char *a, const char *b)
{ return 0; }
/*ARGSUSED*/
char * strcpy(char *a, const char *b)
{ return 0; }
/*ARGSUSED*/
char * strerror(int a)
{ return 0; }
/*ARGSUSED*/
int stricmp(const char *a, const char *b)
{ return 0; }
/*ARGSUSED*/
unsigned int strlen(const char *a)
{ return 0; }
/*ARGSUSED*/
int strncmp(const char *a, const char * b, size_t c)
{ return 0; }
/*ARGSUSED*/
char * strncpy(char *a, const char *b, size_t c)
{ return 0; }
/*ARGSUSED*/
char * strpbrk(const char *a, const char *b)
{ return 0; }
/*ARGSUSED*/
char * strrchr(const char *a, int b)
{ return 0; }
/*ARGSUSED*/
char * strstr(const char *a, const char *b)
{ return 0; }
/*ARGSUSED*/
char * strtok(char *a, const char *b)
{ return 0; }
/*ARGSUSED*/
long strtol(const char *a, char **b, int c)
{ return 0; }
/*ARGSUSED*/
unsigned long strtoul(const char *a, char **b, int c)
{ return 0; }
/*ARGSUSED*/
time_t time(time_t *a)
{ return 0; }
/*ARGSUSED*/
int tolower(int a)
{ return 0; }
/*ARGSUSED*/
int toupper(int a)
{ return 0; }
/*ARGSUSED*/
int unlink(const char *a)
{ return 0; }
#endif
