/*	Copyright (c) 1996 Sun Microsystems, Inc	*/
/*	All Rights Reserved				*/

#pragma ident	"@(#)weaks.c	1.10	97/08/23 SMI"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/socketvar.h>

#pragma weak bind = _bind
#pragma weak listen = _listen
#pragma weak accept = _accept
#pragma weak connect = _connect
#pragma weak shutdown = _shutdown
#pragma weak recv = _recv
#pragma weak recvfrom = _recvfrom
#pragma weak recvmsg = _recvmsg
#pragma weak send = _send
#pragma weak sendmsg = _sendmsg
#pragma weak sendto = _sendto
#pragma weak getpeername = _getpeername
#pragma weak getsockname = _getsockname
#pragma weak getsockopt = _getsockopt
#pragma weak setsockopt = _setsockopt

extern int _so_bind();
extern int _so_listen();
extern int _so_accept();
extern int _so_connect();
extern int _so_shutdown();
extern int _so_recv();
extern int _so_recvfrom();
extern int _so_recvmsg();
extern int _so_send();
extern int _so_sendmsg();
extern int _so_sendto();
extern int _so_getpeername();
extern int _so_getsockopt();
extern int _so_setsockopt();
extern int _so_setsockname();
extern int _so_getsockname();


/*
 * Note that regular sockets use SOV_SOCKBSD here to not allow a rebind of an
 * already bound socket.
 */
int
_bind(int sock, struct sockaddr *addr, int addrlen)
{
	return (_so_bind(sock, addr, addrlen, SOV_SOCKBSD));
}

int
_listen(int sock, int backlog)
{
	return (_so_listen(sock, backlog, SOV_DEFAULT));
}

int
_accept(int sock, struct sockaddr *addr, int *addrlen)
{
	return (_so_accept(sock, addr, addrlen, SOV_DEFAULT));
}

int
_connect(int sock, struct sockaddr *addr, int addrlen)
{
	return (_so_connect(sock, addr, addrlen, SOV_DEFAULT));
}

int
_shutdown(int sock, int how)
{
	return (_so_shutdown(sock, how, SOV_DEFAULT));
}

int
_recv(int sock, char *buf, int len, int flags)
{
	return (_so_recv(sock, buf, len, flags & ~MSG_XPG4_2));
}

int
_recvfrom(int sock, char *buf, int len, int flags,
	struct sockaddr *addr, int *addrlen)
{
	return (_so_recvfrom(sock, buf, len, flags & ~MSG_XPG4_2,
		addr, addrlen));
}

int
_recvmsg(int sock, struct msghdr *msg, int flags)
{
	return (_so_recvmsg(sock, msg, flags & ~MSG_XPG4_2));
}

int
_send(int sock, char *buf, int len, int flags)
{
	return (_so_send(sock, buf, len, flags & ~MSG_XPG4_2));
}

int
_sendmsg(int sock, struct msghdr *msg, int flags)
{
	return (_so_sendmsg(sock, msg, flags & ~MSG_XPG4_2));
}

int
_sendto(int sock, char *buf, int len, int flags,
	struct sockaddr *addr, int *addrlen)
{
	return (_so_sendto(sock, buf, len, flags & ~MSG_XPG4_2,
		addr, addrlen));
}

int
_getpeername(int sock, struct sockaddr *name, int *namelen)
{
	return (_so_getpeername(sock, name, namelen, SOV_DEFAULT));
}

int
_getsockname(int sock, struct sockaddr *name, int *namelen)
{
	return (_so_getsockname(sock, name, namelen, SOV_DEFAULT));
}

int
_getsockopt(int sock, int level, int optname, char *optval, int *optlen)
{
	return (_so_getsockopt(sock, level, optname, optval, optlen,
		SOV_DEFAULT));
}

int
_setsockopt(int sock, int level, int optname, char *optval, int optlen)
{
	return (_so_setsockopt(sock, level, optname, optval, optlen,
		SOV_DEFAULT));
}

int
__xnet_bind(int sock, const struct sockaddr *addr, int addrlen)
{
	return (_so_bind(sock, addr, addrlen, SOV_XPG4_2));
}


int
__xnet_listen(int sock, int backlog)
{
	return (_so_listen(sock, backlog, SOV_XPG4_2));
}

int
__xnet_connect(int sock, const struct sockaddr *addr, int addrlen)
{
	return (_so_connect(sock, addr, addrlen, SOV_XPG4_2));
}

int
__xnet_recvmsg(int sock, struct msghdr *msg, int flags)
{
	return (_so_recvmsg(sock, msg, flags | MSG_XPG4_2));
}

int
__xnet_sendmsg(int sock, const struct msghdr *msg, int flags)
{
	return (_so_sendmsg(sock, msg, flags | MSG_XPG4_2));
}

int
__xnet_sendto(int sock, const char *buf, int len, int flags,
	const struct sockaddr *addr, int *addrlen)
{
	return (_so_sendto(sock, buf, len, flags | MSG_XPG4_2,
		addr, addrlen));
}

int
__xnet_getsockopt(int sock, int level, int option_name,
	void *option_value, int *option_lenp)
{
	return (_so_getsockopt(sock, level, option_name, option_value,
	    option_lenp, SOV_XPG4_2));
}
