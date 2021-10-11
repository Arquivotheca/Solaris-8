/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)lpsched.h	1.18	98/08/05 SMI"	/* SVr4.0 1.9.1.11	*/

#include "stdio.h"
#include "sys/types.h"
#include "memory.h"
#include "string.h"
#include "pwd.h"
#include "fcntl.h"
#include "errno.h"
#include "signal.h"
#include "unistd.h"
#include "stdlib.h"

#include "lp.h"
#include "access.h"
#include "form.h"
#include "requests.h"
#include "filters.h"
#include "printers.h"
#include "class.h"
#include "users.h"
#include "systems.h"
#include "secure.h"
#include "msgs.h"

#include "nodes.h"

/**
 ** Defines:
 **/

/*
 * These are the fields in the PSTATUS and CSTATUS files,
 * found in the SYSTEM directory.
 */

#define PST_MAX	8
# define PST_BRK	0
# define PST_NAME	1
# define PST_STATUS	2
# define PST_DATE	3
# define PST_DISREAS	4
# define PST_REJREAS	5
# define PST_PWHEEL	6
# define PST_FORM	7

#define CST_MAX	5
# define CST_BRK	0
# define CST_NAME	1
# define CST_STATUS	2
# define CST_DATE	3
# define CST_REJREAS	4

/*
 * Exit codes from child processes:
 *
 *    0 <= exit <= 0177 (127) are reserved for ``normal'' exits.
 * 0200 <= exit <= 0377 (255) are reserved for special failures.
 *
 * If bit 0200 is set, then we have three sets of special error
 * codes available, with 32 values in each set (except the first):
 *
 *	0201 - 0237	Printer faults
 *	0240 - 0277	Dial problems
 *	0300 - 0337	Port problems
 *	0340 - 0377	Exec problems
 *
 *	0200		Interface received SIGTERM
 */
#define EXEC_EXIT_OKAY	0	/* success */
#define EXEC_EXIT_USER	0177	/* user exit codes, 7 bits */
#define EXEC_EXIT_NMASK	0340	/* mask to uncover reason bits */
#define EXEC_EXIT_FAULT	0201	/* printer fault */
#define EXEC_EXIT_HUP	0202	/* got hangup early in exec */
#define EXEC_EXIT_INTR	0203	/* got interrupt early in exec */
#define EXEC_EXIT_PIPE	0204	/* got close of FIFO early in exec */
#define EXEC_EXIT_EXIT	0237	/* interface used reserved exit code */
#define EXEC_EXIT_NDIAL	0240	/* can't dial, low 5 bits abs(dial()) */
#define EXEC_EXIT_NPORT	0300	/* can't open port */
#define EXEC_EXIT_TMOUT	0301	/* can't open port in N seconds */
#define EXEC_EXIT_NOPEN	0340	/* can't open input/output file */
#define EXEC_EXIT_NEXEC	0341	/* can't exec */
#define EXEC_EXIT_NOMEM	0342	/* malloc failed */
#define EXEC_EXIT_NFORK	0343	/* fork failed, must try again */
#define EXEC_EXIT_NPUSH 0344	/* could not push streams module(s) */

/*
 * If killed, return signal, else 0.
 */
#define	KILLED(x) (!(x & 0xFF00)? (x & 0x7F) : 0)

/*
 * If exited, return exit code, else -1.
 */
#define	EXITED(x) (!(x & 0xFF)? ((x >> 8) & 0xFF) : -1)

/*
 * Events that can be scheduled:
 */
#define EV_SLOWF	1
#define	EV_INTERF	2
#define EV_NOTIFY	3
#define EV_LATER	4
#define EV_ALARM	5
#define	EV_MESSAGE	6
#define	EV_CHECKCHILD	7
#define	EV_SYSTEM	8
#define EV_ENABLE	9
#define	EV_POLLBSDSYSTEMS	10
#define	EV_FORM_MESSAGE	11
#define	EV_STATUS	12

/*
 * How long to wait before retrying an event:
 * (For best results, make CLOCK_TICK a factor of 60.)
 */
#define CLOCK_TICK	10		/* no. seconds between alarms	*/
#define MINUTE		(60/CLOCK_TICK)	/* number of ticks per minute	*/
#define WHEN_FORK	(MINUTE)	/* retry forking child process	*/
#define WHEN_PRINTER	(1*MINUTE)	/* retry faulted printer	*/

/*
 * Alert types:
 */
#define	A_PRINTER	1
#define	A_PWHEEL	2
#define	A_FORM		3

/*
 * How to handle active requests when disabling a printer:
 */
#define DISABLE_STOP    0
#define DISABLE_FINISH  1
#define DISABLE_CANCEL  2

/*
 * validate_request() - VERIFY REQUEST CAN BE PRINTED
 * evaluate_request() - TRY REQUEST ON A PARTICULAR PRINTER
 * reevaluate_request() - TRY TO MOVE REQUEST TO ANOTHER PRINTER
 */

#define validate_request(PRS,PREFIXP,MOVING) \
	_validate((PRS), (PSTATUS *)0, (PSTATUS *)0, (PREFIXP), (MOVING))

#define evaluate_request(PRS,PPS,MOVING) \
	_validate((PRS), (PPS), (PSTATUS *)0, (char **)0, (MOVING))

#define reevaluate_request(PRS,PPS) \
	_validate((PRS), (PSTATUS *)0, (PPS), (char **)0, 0)

/*
 * Request is ready to be slow-filtered:
 */
#define	NEEDS_FILTERING(PRS) \
	((PRS)->slow && !((PRS)->request->outcome & RS_FILTERED))

/*
 * Where requests are handled:
 */

#define PRINTING_AT(PRS,PSS) \
			((PRS)->printer->system == (PSS))

#define ORIGINATING_AT(PRS,PSS) \
			STREQU((PRS)->secure->system, (PSS)->system->name)

/*
 * Misc:
 */

#define	isadmin(ID)		(!(ID) || (ID) == Lp_Uid)

#define makereqerr(PRS) \
	makepath( \
		Lp_Tmp, \
		(PRS)->secure->system, \
		getreqno((PRS)->secure->req_id), \
		(char *)0 \
	)

#define	EVER			;;

#define	DEFAULT_SHELL		"/bin/sh"

#define	BINMAIL			"/bin/mail"
#define	BINWRITE		"/bin/write"

#define RMCMD			"/usr/bin/rm -f"


#if	defined(MLISTENDEL_WORKS)
#define DROP_MD(MD)	if (MD) { \
			        mlistendel (MD); \
			        mdisconnect (MD); \
			} else /*EMPTY*/
#else
#define DROP_MD(MD)	if (MD) { \
				Close ((MD)->readfd); \
				if ((MD)->writefd == (MD)->readfd) \
					(MD)->writefd = -1; \
				(MD)->readfd = -1; \
			} else /*EMPTY*/
#endif

/**
 ** External routines:
 **/

typedef int (*qchk_fnc_type)( RSTATUS * );

CLASS *		Getclass ( char * );

CSTATUS *	search_ctable ( char * );
CSTATUS *	walk_ctable ( int );


FSTATUS *	search_fptable(register char *);
FSTATUS *	search_ftable ( char * );
FSTATUS *	walk_ftable ( int );

extern void GetRequestFiles(REQUEST *req, char *buffer, int length);


PRINTER *	Getprinter ( char * );

PSTATUS *	search_ptable ( char * );
PSTATUS *	walk_ptable ( int );

PWHEEL *	Getpwheel ( char * );

PWSTATUS *	search_pwtable ( char * );
PWSTATUS *	walk_pwtable ( int );

REQUEST *	Getrequest ( char * );

RSTATUS *	allocr ( void );
RSTATUS *	request_by_id ( char * );
RSTATUS *	request_by_id_num ( long );
RSTATUS *	request_by_jobid ( char * , char * );
RSTATUS *	walk_req_by_dest ( RSTATUS ** , char * );
RSTATUS *	walk_req_by_form ( RSTATUS ** , FSTATUS * );
RSTATUS *	walk_req_by_printer ( RSTATUS ** , PSTATUS * );
RSTATUS *	walk_req_by_pwheel ( RSTATUS ** , char * );

SECURE *	Getsecure ( char * );

USER *		Getuser ( char * );

_FORM *		Getform ( char * );

char *		_alloc_files ( int , char * , uid_t , gid_t, char * );
char *		dispatchName(int);
char *		statusName(int);
char *		getreqno ( char * );

int		Loadfilters ( char * );
int		Putsecure(char *, SECURE *);
int		cancel ( RSTATUS * , int );
int		disable ( PSTATUS * , char * , int );
int		enable ( PSTATUS * );
int		exec ( int , ... );
int		one_printer_with_charsets ( RSTATUS * );
int		open_dialup ( char * , PRINTER * );
int		open_direct ( char * , PRINTER * );
int		qchk_filter ( RSTATUS * );
int		qchk_form ( RSTATUS * );
int		qchk_pwheel ( RSTATUS * );
int		qchk_waiting ( RSTATUS * );
int		queue_repel ( PSTATUS * , int , int (*)( RSTATUS * ) );
int		rsort ( RSTATUS ** , RSTATUS ** );

long		getkey ( void );
long		_alloc_req_id ( void );

off_t		chfiles ( char ** , uid_t , gid_t );

short		_validate ( RSTATUS * , PSTATUS * , PSTATUS * , char ** , int );

void		add_flt_act ( MESG * , ... );
void		alert ( int , ... );
void		cancel_alert ( int , ... );
void		check_children ( void );
void		check_form_alert ( FSTATUS * , _FORM * );
void		check_pwheel_alert ( PWSTATUS * , PWHEEL * );
void		check_request ( RSTATUS * );
void		del_flt_act ( MESG * , ... );
void		dial_problem ( PSTATUS * , RSTATUS * , int );
void		dispatch ( int , char * , MESG * );
void		dowait ( void );
void		dump_cstatus ( void );
void		dump_fault_status(PSTATUS *);
void		dump_pstatus ( void );
void		dump_status ( void );
void		execlog ( char * , ... );
void		fail ( char * , ... );
void		free_form ( _FORM * );
void		freerstatus ( register RSTATUS * );
void		init_memory ( void );
void		init_messages ( void );
void		insertr ( RSTATUS * );
void		load_sdn ( char ** , SCALED );
void		load_status ( void );
void		load_str ( char ** , char * );
void		lp_endpwent ( void );
void		lp_setpwent ( void );
void		lpfsck ( void );
void		lpshut ( int );
void		mallocfail ( void );
void		maybe_schedule ( RSTATUS * );
void		note ( char * ,	... );
void		notify ( RSTATUS * , char * , int , int , int );
void		printer_fault ( PSTATUS * , RSTATUS * , char * , int );
void		clear_printer_fault ( PSTATUS * ,  char * );
void		putjobfiles ( RSTATUS * );
void		queue_attract ( PSTATUS * , int (*)( RSTATUS * ) , int );
void		queue_check ( int (*)( RSTATUS * ) );
void		queue_form ( RSTATUS * , FSTATUS * );
void		queue_pwheel ( RSTATUS * , char * );
void		remount_form(register PSTATUS *, FSTATUS *, short);
void		remover ( RSTATUS * );
void		rmfiles ( RSTATUS * , int );
void		rmreq ( RSTATUS * );
void		schedule ( int , ... );
void		take_message ( void );
void		terminate ( EXEC * );
void		unload_list ( char *** );
void		unload_str ( char ** );
void		unqueue_form ( RSTATUS * );
void		unqueue_pwheel ( RSTATUS * );
void		update_req ( char * , long );
int		isFormMountedOnPrinter ( PSTATUS *, FSTATUS * );
int		isFormUsableOnPrinter ( PSTATUS *, FSTATUS * );
char		*allTraysWithForm ( PSTATUS *, FSTATUS * );

/*
 * Things that can't be passed as parameters:
 */

extern FSTATUS		*form_in_question;

extern char		*pwheel_in_question;

/**
 ** External tables, lists:
 **/

extern CSTATUS		*CStatus;	/* Status of classes       */

extern EXEC		*Exec_Table;	/* Running processes       */

extern FSTATUS		*FStatus;	/* Status of forms	   */

extern PSTATUS		*PStatus;	/* Status of printers      */

extern PWSTATUS		*PWStatus;	/* Status of print wheels  */

extern RSTATUS		*Request_List;	/* Queue of print requests */
extern RSTATUS		*Status_List;	/* Queue of fake requests */
extern RSTATUS		*NewRequest;	/* Not in Request_List yet */

extern EXEC		*Exec_Slow,	/* First slow filter exec  */
			*Exec_Notify;	/* First notification exec */

extern int		CT_Size,	/* Size of class table		*/
			ET_Size,	/* Size of exec table		*/
			ET_SlowSize,	/* Number of filter execs  	*/
			ET_NotifySize,	/* Number of notify execs  	*/
			FT_Size,	/* Size of form table		*/
			PT_Size,	/* Size of printer table	*/
			PWT_Size,	/* Size of print wheel table	*/
			ST_Size,	/* Size of system status table	*/
			ST_Count;	/* No. active entries in above	*/

#if	defined(DEBUG)
#define DB_EXEC		0x00000001
#define DB_DONE		0x00000002
#define DB_INIT		0x00000004
#define DB_ABORT	0x00000008
#define DB_SCHEDLOG	0x00000010
#define DB_SDB		0x00000020
#define DB_MESSAGES	0x00000040
#define DB_MALLOC	0x00000080
#define DB_ALL		0xFFFFFFFF

extern unsigned long	debug;
#endif

extern char		*Local_System,	/* Node name of local system	*/
			*SHELL;		/* value of $SHELL, or default	*/

extern int		lock_fd;

extern uid_t		Lp_Uid;

extern gid_t		Lp_Gid;

extern int		Starting,
			OpenMax,
			Sig_Alrm,
			DoneChildren,
			am_in_background,
			Shutdown;

extern unsigned long	chkprinter_result;

#if defined(MDL)
#include	"mdl.h"
#endif
# define	CLOSE_ON_EXEC(fd)	(void) Fcntl(fd, F_SETFD, 1)
