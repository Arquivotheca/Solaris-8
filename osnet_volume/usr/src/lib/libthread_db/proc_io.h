/* @(#)proc_io.h 1.4 91/11/14 SMI */
/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#ifndef proc_io_h
#define proc_io_h

/*
 * Some modules might only want to do io (threads for instance) and
 * don't need all the proc.h baggage.
 */

#ifdef __cplusplus
extern "C" {
#endif

extern int read_err_flag;

void		iread(char *buff, Address addr, int nbytes);
void		ireadt(char *buff, Address addr, int nbytes);
void		iwrite(char *buff, Address addr, int nbytes);
void		dread(char *buff, Address addr, int nbytes);
void		dwrite(char *buff, Address addr, int nbytes);

void		dwritel(Address, unsigned v);
unsigned	dreadl(Address);

void		dwrites(Address, unsigned short);
unsigned short	dreads(Address);

void		dwriteb(Address, unsigned char);
unsigned char	dreadb(Address);

void		iwritel(Address, unsigned v);
unsigned	ireadl(Address);

#ifdef __cplusplus
}
#endif

#endif	// proc_io_h
