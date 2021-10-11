/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)lpstat.h	1.6	93/10/18 SMI"	/* SVr4.0 1.7	*/

#include <sys/types.h>
#include <time.h>

#define	DESTMAX	14	/* max length of destination name */
#define	SEQLEN	8	/* max length of sequence number */
#define	IDSIZE	DESTMAX+SEQLEN+1	/* maximum length of request id */
#define	LOGMAX	15	/* maximum length of logname */
#define	OSIZE	7
#define SZ_DATE_BUFF 100	/* size of conversion buff for dates */

#define INQ_UNKNOWN	-1
#define INQ_ACCEPT	0
#define INQ_PRINTER	1
#define INQ_STORE	2
#define INQ_USER	3

#define V_LONG		0x0001
#define V_BITS		0x0002
#define V_RANK		0x0004
#define V_MODULES	0x0008

#define BITPRINT(S,B) \
	if ((S)&(B)) { (void)printf("%s%s",sep,#B); sep = "|"; }

typedef struct mounted {
	char			*name,
				**printers;
	struct mounted		*forward;
}			MOUNTED;

void		add_mounted ( char * , char * , char * );
void		def ( void );
void		do_accept ( char ** );
void		do_charset ( char ** );
void		do_class ( char ** );
void		do_device ( char ** );
void		do_form ( char ** );
void		do_paper ( char ** );
void		do_printer ( char ** );
void		do_request ( char ** );
void		do_user ( char ** );
void		done ( int );
void		parse ( int , char ** );
void		putoline(char *, char *, long, time_t, int, char *, char *,
			char *, int);
void		putpline(char *, int, char *, time_t, char *, char *, char *);
void		putqline(char *, int, time_t, char *);
void		putppline ( char * ,  char *);
void		running ( void );
void		send_message ( int , ... );
void		startup ( void );

int		output ( int );

#if	defined(_LP_PRINTERS_H)
char **		get_charsets ( PRINTER * , int );
#endif

extern int		exit_rc;
extern int		inquire_type;
extern int		D;
extern int		remote_cmd;
extern int		scheduler_active;

extern char		*alllist[];

extern unsigned int	verbosity;

extern MOUNTED		*mounted_forms;
extern MOUNTED		*mounted_pwheels;
