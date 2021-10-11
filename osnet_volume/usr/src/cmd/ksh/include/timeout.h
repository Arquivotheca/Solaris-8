#ident	"@(#)timeout.h	1.7	96/05/09 SMI"	/* From AT&T Toolchest */

/*
 *	UNIX shell
 *
 *	S. R. Bourne
 *	AT&T Bell Laboratories
 *
 */

#define TGRACE		60	/* grace period before termination */
				/* The time_warn message contains this number */
extern longlong_t	sh_timeout;
extern const char	e_timeout[];
extern const char	e_timewarn[];
