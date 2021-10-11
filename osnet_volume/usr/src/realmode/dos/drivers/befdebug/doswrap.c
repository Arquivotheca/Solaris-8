/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */

/*
 * The #ident directive is commented out because it causes an error
 * in the MS-DOS linker.
 *
#ident "@(#)doswrap.c	1.5	97/04/11 SMI"
 */

/*
 *	Utility routines used by BEFDEBUG program.
 */

/*
 *	This file contains wrappers to some DOS routines that
 *	cannot be used directly in befdebug.c because of collision
 *	problems.  One set of collisions arises because rmsc.h
 *	includes the BEF version of stdio.h.  This file contains
 *	items that need the DOS version of stdio.h.  Also this
 *	file contains wrappers for C library routines which have
 *	different calling sequences between BEF and DOS (typically
 *	the differences are FAR versus NEAR pointers.)
 */

#include <stdio.h>

void *
dos_open(char *file_name, char *mode)
{
	FILE *answer;

	answer = fopen(file_name, mode);
	return ((void *)answer);
}

void
dos_close(void *handle)
{
	fclose((FILE *)handle);
}

int
dos_fprintf(void *handle, char *s, int a, int b, int c, int d)
{
	return (fprintf((FILE *)handle, s, a, b, c, d));
}

char *
dos_get_line(char *s, int n, void *handle)
{
	return (fgets(s, n, (FILE *)handle));
}

int
dos_put_line(char *s, void *handle)
{
	return (fwrite(s, strlen(s), 1, (FILE *)handle) == 1);
}

int
dos_strcmp(char *a, char *b)
{
	return (strcmp(a, b));
}

void
dos_strcpy(char *a, char *b)
{
	strcpy(a, b);
}

/*
 * dos_alloc: returns 0 for success otherwise an error code.
 * *answer is set to the new segment for success or the
 * biggest available segment for failure.
 */
unsigned short
dos_alloc(unsigned short amount, unsigned short *answer)
{
	unsigned short ret_val;
	unsigned short error_code;

	_asm {
		push	bx
		mov	ax, 4800h
		mov	bx, amount
		int	21h
		jc	bad_48
		mov	bx, ax
		xor	ax, ax
bad_48:
		mov	error_code, ax
		mov	ret_val, bx
		pop	bx
	}
	*answer = ret_val;
	return (error_code);
}

unsigned short
dos_free(unsigned short loc)
{
	unsigned short error_code;

	_asm {
		push	es
		mov	ax, 4900h
		mov	es, loc
		int	21h
		jc	bad_49
		xor	ax, ax
bad_49:
		mov	error_code, ax
		pop	es
	}
	return (error_code);
}

unsigned short
dos_mem_adjust(unsigned short block, unsigned short new, unsigned short *max)
{
	unsigned short ret_val;
	unsigned short error_code;

	_asm {
		push	bx
		push	es
		mov	ax, 4a00h
		mov	es, block
		mov	bx, new
		int	21h
		jc	bad_4a
		xor	ax, ax
bad_4a:
		mov	error_code, ax
		mov	ret_val, bx
		pop	es
		pop	bx
	}
	*max = ret_val;
	return (error_code);
}

unsigned short
dos_get_psp(void)
{
	unsigned short answer;
	
	_asm {
		push	bx
		mov	ah, 51h
		int	21h
		mov	answer, bx
		pop	bx
	}
	return (answer);
}

unsigned short
dos_get_size(unsigned short block)
{
	unsigned short answer;
	
	_asm {
		push	es
		mov	ax, block
		dec	ax
		mov	es, ax
		mov	ax, word ptr es:3
		mov	answer, ax
		pop	es
	}
	return (answer);
}

int 
dos_exec(char *file, char *arg)
{
	struct exec_args {
		unsigned short env;
		char *arg;
		char *fcb1;
		char *fcb2;
	} ea;
	static char fcb[50];
	struct exec_args *args_ptr;
	int answer;
	
	args_ptr = &ea;
	ea.env = 0;
	ea.arg = arg;
	ea.fcb1 = &fcb[10];
	ea.fcb2 = &fcb[10];
	
	_asm {
		push	ds
		push	es
		push	bx
		les	bx, args_ptr
		lds	dx, file
		mov	ax, 4B00h
		int	21h
		jc	exec_bad
		xor	ax, ax
	exec_bad:
		pop	bx
		pop	es
		pop	ds
		mov	answer, ax
	}
	return (answer);
}

int
dos_exit(unsigned short code)
{
	_asm {
		mov	al, byte ptr code
		mov	ah, 4Ch
		int	21h
	}
}

int
dos_kb_char(void)
{
	int answer;

	_asm {
;		Read 1 char from console
		mov	ax, 800h
		int	21h
		xor	ah, ah
		mov	answer, ax
	}
	return (answer);
}

int
dos_kb_char_nowait(void)
{
	int answer = -1;

	_asm {
;		Read 1 char from console if ready
		mov	ax, 600h
		mov	dl, 0FFh;	 FF means input
		int	21h
		jz	no_char
		xor	ah, ah
		mov	answer, ax
	no_char:
	}
	return (answer);
}

int
dos_write(int fd, char *buf, int count)
{
	int answer = 0;

#ifdef USE_DOS_WRITE
	_asm {
;		Put out the character using DOS write to fd 0.
;		Ignore any error.
		push	bx
		push	ds
		mov	ax, 4000h
		mov	bx, fd
		mov	cx, count
		lds	dx, buf
		int	21h
		jnc	write_ok
		mov	ax, 0FFFFh
	write_ok:
		pop	ds
		pop	bx
		mov	answer, ax
	}
#else
	char *s;
	char c;
	
	for (s = buf; count > 0; count--, s++) {
		c = *s;
		_asm {
			mov	dl, c
			mov	ah, 2
			int	21h
			jc	write_bad
			inc	answer
		write_bad:
		}
	}
#endif
	return (answer);
}

void
dos_usecwait(unsigned long usecs)
{
	_asm {
		/*
		 *  Issue BIOS call to wait for specified number of microseconds
		 */

		mov   ax, 8600h
		mov   cx, word ptr [usecs+2]
		mov   dx, word ptr [usecs]
		int   15h
	}
}

void
dos_msecwait(unsigned long msecs)
{
	unsigned long usecs = msecs * 1000;

	dos_usecwait(usecs);
}

void
dos_memset(void far *p, int c, unsigned len)
{
	(void) memset(p, c, len);
}

int
dos_memcmp(void far *p, void far *q, unsigned n)
{
	return (memcmp(p, q, n));
}

void
dos_memcpy(void far *dest, void far *src, unsigned n)
{
	(void) memcpy(dest, src, n);
}
