/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * An application should not include this header directly.  Instead it
 * should be included only through the inclusion of other Sun headers.
 *
 * The contents of this header is limited to identifiers specified in the
 * C Standard.  Any new identifiers specified in future amendments to the
 * C Standard must be placed in this header.  If these new identifiers
 * are required to also be in the C++ Standard "std" namespace, then for
 * anything other than macro definitions, corresponding "using" directives
 * must also be added to <stdio.h>.
 */

/*
 * User-visible pieces of the ANSI C standard I/O package.
 */

#ifndef _ISO_STDIO_ISO_H
#define	_ISO_STDIO_ISO_H

#pragma ident	"@(#)stdio_iso.h	1.2	99/10/25 SMI"
/* SVr4.0 2.34.1.2 */

#include <sys/feature_tests.h>
#include <sys/va_list.h>
#include <stdio_tag.h>
#include <stdio_impl.h>

/*
 * If feature test macros are set that enable interfaces that use types
 * defined in <sys/types.h>, get those types by doing the include.
 *
 * Note that in asking for the interfaces associated with this feature test
 * macro one also asks for definitions of the POSIX types.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(_LP64) && (_FILE_OFFSET_BITS == 64 || defined(_LARGEFILE64_SOURCE))
/*
 * The following typedefs are adopted from ones in <sys/types.h> (with leading
 * underscores added to avoid polluting the ANSI C name space).  See the
 * commentary there for further explanation.
 */
#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
typedef	long long	__longlong_t;
#else
/* used to reserve space and generate alignment */
typedef union {
	double	_d;
	int	_l[2];
} __longlong_t;
#endif
#endif  /* !_LP64 && _FILE_OFFSET_BITS == 64 || defined(_LARGEFILE64_SOURCE) */

#if __cplusplus >= 199711L
namespace std {
#endif

#if !defined(_FILEDEFED) || __cplusplus >= 199711L
#define	_FILEDEFED
typedef	__FILE FILE;
#endif

#if !defined(_SIZE_T) || __cplusplus >= 199711L
#define	_SIZE_T
#if defined(_LP64) || defined(_I32LPx)
typedef unsigned long	size_t;		/* size of something in bytes */
#else
typedef unsigned int	size_t;		/* (historical version) */
#endif
#endif	/* !_SIZE_T */

#if defined(_LP64) || _FILE_OFFSET_BITS == 32
typedef long		fpos_t;
#else
typedef	__longlong_t	fpos_t;
#endif

#if __cplusplus >= 199711L
}
#endif /* end of namespace std */

#ifndef	NULL
#if defined(_LP64) && !defined(__cplusplus)
#define	NULL	0L
#else
#define	NULL	0
#endif
#endif

#define	BUFSIZ	1024

/*
 * The value of _NFILE is defined in the Processor Specific ABI.  The value
 * is chosen for historical reasons rather than for truly processor related
 * attribute.  Note that the SPARC Processor Specific ABI uses the common
 * UNIX historical value of 20 so it is allowed to fall through.
 */
#if defined(__i386)
#define	_NFILE	60	/* initial number of streams: Intel x86 ABI */
#else
#define	_NFILE	20	/* initial number of streams: SPARC ABI and default */
#endif

#define	_SBFSIZ	8	/* compatibility with shared libs */

#define	_IOFBF		0000	/* full buffered */
#define	_IOLBF		0100	/* line buffered */
#define	_IONBF		0004	/* not buffered */
#define	_IOEOF		0020	/* EOF reached on read */
#define	_IOERR		0040	/* I/O error from system */

#define	_IOREAD		0001	/* currently reading */
#define	_IOWRT		0002	/* currently writing */
#define	_IORW		0200	/* opened for reading and writing */
#define	_IOMYBUF	0010	/* stdio malloc()'d buffer */

#ifndef EOF
#define	EOF	(-1)
#endif

#define	FOPEN_MAX	_NFILE
#define	FILENAME_MAX    1024	/* max # of characters in a path name */

#define	SEEK_SET	0
#define	SEEK_CUR	1
#define	SEEK_END	2
#define	TMP_MAX		17576	/* 26 * 26 * 26 */

#define	L_tmpnam	25	/* (sizeof(P_tmpdir) + 15) */

#if defined(__STDC__)
extern __FILE	__iob[_NFILE];
#define	stdin	(&__iob[0])
#define	stdout	(&__iob[1])
#define	stderr	(&__iob[2])
#else
extern __FILE	_iob[_NFILE];
#define	stdin	(&_iob[0])
#define	stdout	(&_iob[1])
#define	stderr	(&_iob[2])
#endif	/* __STDC__ */

#if __cplusplus >= 199711L
namespace std {
#endif

#if defined(__STDC__)

extern int	remove(const char *);
extern int	rename(const char *, const char *);
extern FILE	*tmpfile(void);
extern char	*tmpnam(char *);
extern int	fclose(FILE *);
extern int	fflush(FILE *);
extern FILE	*fopen(const char *, const char *);
extern FILE	*freopen(const char *, const char *, FILE *);
extern void	setbuf(FILE *, char *);
extern int	setvbuf(FILE *, char *, int, size_t);
/* PRINTFLIKE2 */
extern int	fprintf(FILE *, const char *, ...);
/* SCANFLIKE2 */
extern int	fscanf(FILE *, const char *, ...);
/* PRINTFLIKE1 */
extern int	printf(const char *, ...);
/* SCANFLIKE1 */
extern int	scanf(const char *, ...);
/* PRINTFLIKE2 */
extern int	sprintf(char *, const char *, ...);
/* SCANFLIKE2 */
extern int	sscanf(const char *, const char *, ...);
extern int	vfprintf(FILE *, const char *, __va_list);
extern int	vprintf(const char *, __va_list);
extern int	vsprintf(char *, const char *, __va_list);
extern int	fgetc(FILE *);
extern char	*fgets(char *, int, FILE *);
extern int	fputc(int, FILE *);
extern int	fputs(const char *, FILE *);
#if (__cplusplus >= 199711L && (defined(_LP64) || defined(_REENTRANT))) || \
	__cplusplus < 199711L
extern int	getc(FILE *);
extern int	putc(int, FILE *);
#endif
#if (__cplusplus >= 199711L && defined(_REENTRANT)) || \
	__cplusplus < 199711L
extern int	getchar(void);
extern int	putchar(int);
#endif
extern char	*gets(char *);
extern int	puts(const char *);
extern int	ungetc(int, FILE *);
extern size_t	fread(void *, size_t, size_t, FILE *);
extern size_t	fwrite(const void *, size_t, size_t, FILE *);
extern int	fgetpos(FILE *, fpos_t *);
extern int	fseek(FILE *, long, int);
extern int	fsetpos(FILE *, const fpos_t *);
extern long	ftell(FILE *);
extern void	rewind(FILE *);
#if (__cplusplus >= 199711L && (defined(_LP64) || defined(_REENTRANT))) || \
	__cplusplus < 199711L
extern void	clearerr(FILE *);
extern int	feof(FILE *);
extern int	ferror(FILE *);
#endif
extern void	perror(const char *);

#ifndef	_LP64
extern int	__filbuf(FILE *);
extern int	__flsbuf(int, FILE *);
#endif	/*	_LP64	*/

#else	/* !defined __STDC__ */

extern int	remove();
extern int	rename();
extern FILE	*tmpfile();
extern char	*tmpnam();
extern int	fclose();
extern int	fflush();
extern FILE	*fopen();
extern FILE	*freopen();
extern void	setbuf();
extern int	setvbuf();
extern int	fprintf();
extern int	fscanf();
extern int	printf();
extern int	scanf();
extern int	sprintf();
extern int	sscanf();
extern int	vfprintf();
extern int	vprintf();
extern int	vsprintf();
extern int	fgetc();
extern char	*fgets();
extern int	fputc();
extern int	fputs();
extern int	getc();
extern int	getchar();
extern char	*gets();
extern int	putc();
extern int	putchar();
extern int	puts();
extern int	ungetc();
extern size_t	fread();
extern size_t	fwrite();
extern int	fgetpos();
extern int	fseek();
extern int	fsetpos();
extern long	ftell();
extern void	rewind();
extern void	clearerr();
extern int	feof();
extern int	ferror();
extern void	perror();

#ifndef	_LP64
extern int	_filbuf();
extern int	_flsbuf();
#endif	/*	_LP64	*/

#endif	/* __STDC__ */

#if __cplusplus >= 199711L
}
#endif /* end of namespace std */

#if !defined(__lint)

#ifndef	_REENTRANT

#ifndef	_LP64
#ifdef	__STDC__
#if __cplusplus >= 199711L
namespace std {
inline int getc(FILE *_p) {
	return (--_p->_cnt < 0 ? __filbuf(_p) : (int)*_p->_ptr++); }
inline int putc(int _x, FILE *_p) {
	return (--_p->_cnt < 0 ? __flsbuf(_x, _p)
		: (int)(*_p->_ptr++ = (unsigned char) _x)); }
}
#else /* __cplusplus >= 199711L */
#define	getc(p)		(--(p)->_cnt < 0 ? __filbuf(p) : (int)*(p)->_ptr++)
#define	putc(x, p)	(--(p)->_cnt < 0 ? __flsbuf((x), (p)) \
				: (int)(*(p)->_ptr++ = (unsigned char) (x)))
#endif /* __cplusplus >= 199711L */
#else /* __STDC__ */
#define	getc(p)		(--(p)->_cnt < 0 ? _filbuf(p) : (int) *(p)->_ptr++)
#define	putc(x, p)	(--(p)->_cnt < 0 ? _flsbuf((x), (p)) : \
				(int) (*(p)->_ptr++ = (unsigned char) (x)))
#endif	/* __STDC__ */
#endif	/* _LP64 */

#if __cplusplus >= 199711L
namespace std {
inline int getchar() { return getc(stdin); }
inline int putchar(int _x) { return putc(_x, stdout); }
}
#else
#define	getchar()	getc(stdin)
#define	putchar(x)	putc((x), stdout)
#endif /* __cplusplus >= 199711L */

#ifndef	_LP64
#if __cplusplus >= 199711L
namespace std {
inline void clearerr(FILE *_p) { _p->_flag &= ~(_IOERR | _IOEOF); }
inline int feof(FILE *_p) { return _p->_flag & _IOEOF; }
inline int ferror(FILE *_p) { return _p->_flag & _IOERR; }
}
#else /* __cplusplus >= 199711L */
#define	clearerr(p)	((void)((p)->_flag &= ~(_IOERR | _IOEOF)))
#define	feof(p)		((p)->_flag & _IOEOF)
#define	ferror(p)	((p)->_flag & _IOERR)
#endif /* __cplusplus >= 199711L */
#endif	/* _LP64 */

#endif	/* _REENTRANT */

#endif	/* !defined(__lint) */

#ifdef	__cplusplus
}
#endif

#endif	/* _ISO_STDIO_ISO_H */
