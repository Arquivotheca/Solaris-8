/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)util.h 1.4 94/03/11 SMI"

#ifndef UTIL_DOT_H
#define UTIL_DOT_H

char*	get_cat_dev_name(int idx);

void	error_exit(const char* fmt, ...);
void	no_def_notice(char* v_name);
int	scan_dir(char* dirname, char* matchstring, void(*action)(char*, char*));
void	vrb(const char*, ...);

#define ONCE()	{ static int done; if ( done ) return; ++done; }

#ifdef __STDC__
#define	TAG(m)		(DVC_##m)
#else
#define	TAG(m)		(DVC_/**/m)
#endif

#define	MSG(i)	dgettext(DVC_MSGS_TEXTDOMAIN, dvc_msgs[TAG(i)-DVC_MSGS_BASE])
#define	SMSG(i)	dgettext(DVC_MSGS_TEXTDOMAIN, dvc_msgs[i-DVC_MSGS_BASE])

#endif /* UTIL_DOT_H */
