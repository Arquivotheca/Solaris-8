%/*
% *	Copyright (c) 1997 Sun Microsystems, Inc.
% *	All Rights Reserved.
% */
%
%#pragma ident   "@(#)nsm_addr.x	1.3	98/04/16 SMI"
%
%/* from nsm_addr.x */
%
%/*
% * This is the definition for the REG procedure which is used
% * to register name/address pairs with statd.
% */
%
enum nsm_addr_res {
	nsm_addr_succ = 0,		/* simple success/failure result */
	nsm_addr_fail = 1
};

struct reg1args {
	unsigned int family;		/* address families from socket.h */
	string name<1024>;		/* name to register with this addr */
	netobj address;
};

struct reg1res {
	nsm_addr_res status;
};
%
%/*
% * This is the definition for the UNREG procedure which is used
% * to unregister an address (and its associated name, if that name
% * has no other addresses registered with it) with statd.
% */
struct unreg1args {
	unsigned int family;		/* address families from socket.h */
	string name<1024>;		/* name under this addr to unregister */
	netobj address;
};

struct unreg1res {
	nsm_addr_res status;
};

%
%/*
% * This is the definition for the NSM address registration network
% * protocol which is used to privately support address registration
% * with the status daemon statd (NSM).
% */
program NSM_ADDR_PROGRAM {
	version NSM_ADDR_V1 {
		void
		 NSMADDRPROC1_NULL(void) = 0;
		reg1res
		 NSMADDRPROC1_REG(reg1args) = 1;
		unreg1res
		 NSMADDRPROC1_UNREG(unreg1args) = 2;
	} = 1;
} = 100133;
