/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)msgdefs.h	1.10	96/05/16 SMI"	/* SVr4.0  1.1.1.1	*/

#define	ERR_NOPKGS	"no packages were found in <%s>"

#define	ERR_CHDIR	"unable to change directory to <%s>"

#define	ERR_DSINIT	"could not process datastream from <%s>"

#define	ERR_STREAMDIR	"unable to make temporary directory to unpack " \
		"datastream"

#define	MSG_SUSPEND	"Installation of <%s> has been suspended."

#define	MSG_1MORE_PROC	"\nThere is 1 more package to be processed."

#define	MSG_1MORE_INST	"\nThere is 1 more package to be installed."

#define	MSG_MORE_PROC	"\nThere are %d more packages to be processed."

#define	MSG_MORE_INST	"\nThere are %d more packages to be installed."

#define	ASK_CONTINUE	"Do you want to continue with installation"

#define	ERR_NOTROOT	"You must be \"root\" for %s to execute properly."

#define	ERR_NODEVICE	"unable to determine device to install from"

#define	ERR_NORESP	"response file <%s> must not exist"

#define	ERR_ACCRESP	"unable to access response file <%s>"

#define	ERR_PKGVOL	"unable to obtain package volume"

#define	ERR_DSARCH	"unable to find archive for <%s> in datastream"

#define	ERR_USAGE0	"usage: %s -r response [-d device] [-R host_path] " \
		"[pkg [pkg ...]]\n"

#define	ERR_USAGE1	"usage:\n" \
		"\t%s [-n] [-d device] [[-M] -R host_path] [-V fs_file]" \
		"[-a admin_file] [-r response] [-v] " \
		"[pkg [pkg ...]]\n" \
		"\t%s -s dir [-d device] [pkg [pkg ...]]\n"
