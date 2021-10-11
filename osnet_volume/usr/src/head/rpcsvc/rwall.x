%/*
% * Copyright (c) 1991 by Sun Microsystems, Inc.
% */

%/* from rwall.x */
%
%/*
% * Remote write-all ONC service
% */

#ifdef RPC_HDR
%
#elif RPC_SVC
%
%/*
% *  Server side stub routines for the rpc.rwalld daemon
% */
%
#elif RPC_CLNT
%
%/*
% *  Client side stub routines for the rwall program
% */
%
#endif

typedef string wrapstring<>;	/* Define for RPC library's xdr_wrapstring */

program WALLPROG {
	version WALLVERS {
		/*
		 * There is no procedure 1
		 */
		void
		WALLPROC_WALL (wrapstring) = 2;
	} = 1;
} = 100008;

#ifdef RPC_HDR
%
%
%#if defined(__STDC__) || defined(__cplusplus)
%enum clnt_stat rwall(char *, char *);
%#else
%enum clnt_stat rwall();
%#endif
%
#endif
