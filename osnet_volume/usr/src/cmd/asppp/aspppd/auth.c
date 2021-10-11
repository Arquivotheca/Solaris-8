#ident	"@(#)auth.c	1.2	94/12/30 SMI"

/* Copyright (c) 1994 by Sun Microsystems, Inc. */

#include <stropts.h>

#include "path.h"
#include "ppp_ioctl.h"

static void	configure_pap(struct path *);
static void	configure_pap_peer(struct path *);
static void	configure_chap(struct path *);
static void	configure_chap_peer(struct path *);

void
set_authentication(struct path *p)
{
	pppAuthControlEntry_t	auth;
	struct strioctl		cmio;

	auth.pppAuthControlIndex = 0;

	switch (p->auth.will_do) {
	case none:
		auth.pppAuthTypeLocal = NO_AUTH;
		break;
	case pap:
		auth.pppAuthTypeLocal = DO_PAP;
		configure_pap(p);
		break;
	case chap:
		auth.pppAuthTypeLocal = DO_CHAP;
		configure_chap(p);
		break;
	case all:
		auth.pppAuthTypeLocal = DO_PAP | DO_CHAP;
		configure_pap(p);
		configure_chap(p);
		break;
	}

	switch (p->auth.required) {
	case none:
		auth.pppAuthTypeRemote = NO_AUTH;
		break;
	case pap:
		auth.pppAuthTypeRemote = DO_PAP;
		configure_pap_peer(p);
		break;
	case chap:
		auth.pppAuthTypeRemote = DO_CHAP;
		configure_chap_peer(p);
		break;
	case all:
		auth.pppAuthTypeRemote = DO_PAP | DO_CHAP;
		configure_pap_peer(p);
		configure_chap_peer(p);
		break;
	}

	cmio.ic_cmd = PPP_SET_AUTH;
	cmio.ic_timout = 1;
	cmio.ic_len = sizeof (auth);
	cmio.ic_dp = (char *)&auth;

	if (ioctl(p->s, I_STR, &cmio) < 0)
	    fail("set_authentication: ioctl failed\n");
}

static void
configure_pap(struct path *p)
{
	struct strioctl		cmio;
	papPasswdEntry_t	passwd;

	memset((void *)&passwd, 0, sizeof (passwd));

	passwd.protocol = pppAuthPAP;

	if (p->auth.pap.password) {
		passwd.papPasswdLen = strlen(p->auth.pap.password);
		if (passwd.papPasswdLen > PAP_MAX_PASSWD)
		    fail("configure_pap: PAP password > 255 octets\n");
		memcpy((void *)passwd.papPasswd, (void *)p->auth.pap.password,
			passwd.papPasswdLen);
	}

	if (p->auth.pap.id) {
		passwd.papPeerIdLen = strlen(p->auth.pap.id);
		if (passwd.papPeerIdLen > PAP_MAX_PASSWD)
		    fail("configure_pap: PAP id > 255 octets\n");
		memcpy((void *)passwd.papPeerId, (void *)p->auth.pap.id,
			passwd.papPeerIdLen);
	}

	cmio.ic_cmd = PPP_SET_LOCAL_PASSWD;
	cmio.ic_timout = 1;
	cmio.ic_len = sizeof (passwd);
	cmio.ic_dp = (char *)&passwd;

	if (ioctl(p->s, I_STR, &cmio) < 0) {
		fail("configure_pap: ioctl failed\n");
	}
}

static void
configure_pap_peer(struct path *p)
{
	struct strioctl		cmio;
	papPasswdEntry_t	passwd;

	memset((void *)&passwd, 0, sizeof (passwd));

	passwd.protocol = pppAuthPAP;

	if (p->auth.pap_peer.password) {
		passwd.papPasswdLen = strlen(p->auth.pap_peer.password);
		if (passwd.papPasswdLen > PAP_MAX_PASSWD)
		    fail("configure_pap_peer: PAP password > 255 octets\n");
		memcpy((void *)passwd.papPasswd,
			(void *)p->auth.pap_peer.password,
			passwd.papPasswdLen);
	}

	if (p->auth.pap_peer.id) {
		passwd.papPeerIdLen = strlen(p->auth.pap_peer.id);
		if (passwd.papPeerIdLen > PAP_MAX_PASSWD)
		    fail("configure_pap_peer: PAP id > 255 octets\n");
		memcpy((void *)passwd.papPeerId, (void *)p->auth.pap_peer.id,
			passwd.papPeerIdLen);
	}

	cmio.ic_cmd = PPP_SET_REMOTE_PASSWD;
	cmio.ic_timout = 1;
	cmio.ic_len = sizeof (passwd);
	cmio.ic_dp = (char *)&passwd;

	if (ioctl(p->s, I_STR, &cmio) < 0) {
		fail("configure_pap_peer: ioctl failed\n");
	}
}

static void
configure_chap(struct path *p)
{
	struct strioctl		cmio;
	chapPasswdEntry_t	passwd;

	passwd.protocol = pppCHAP;

	if (p->auth.chap.secret == NULL)
	    fail("configure_chap: Must specify a CHAP secret\n");
	passwd.chapPasswdLen = strlen(p->auth.chap.secret);
	if (passwd.chapPasswdLen > CHAP_MAX_PASSWD)
	    fail("configure_chap: CHAP secret > 255 octets\n");
	memcpy((void *)passwd.chapPasswd, (void *)p->auth.chap.secret,
		passwd.chapPasswdLen);

	if (p->auth.chap.name == NULL)
	    fail("configure_chap: Must specify a CHAP name\n");
	passwd.chapNameLen = strlen(p->auth.chap.name);
	if (passwd.chapNameLen > CHAP_MAX_NAME)
	    fail("configure_chap: CHAP name > 255 octets\n");
	memcpy((void *)passwd.chapName, (void *)p->auth.chap.name,
		passwd.chapNameLen);

	cmio.ic_cmd = PPP_SET_LOCAL_PASSWD;
	cmio.ic_timout = 1;
	cmio.ic_len = sizeof (passwd);
	cmio.ic_dp = (char *)&passwd;

	if (ioctl(p->s, I_STR, &cmio) < 0)
	    fail("configure_chap: ioctl failed\n");
}

static void
configure_chap_peer(struct path *p)
{
	struct strioctl		cmio;
	chapPasswdEntry_t	passwd;

	passwd.protocol = pppCHAP;

	if (p->auth.chap_peer.secret == NULL)
	    fail("configure_chap_peer: Must specify a CHAP secret\n");
	passwd.chapPasswdLen = strlen(p->auth.chap_peer.secret);
	if (passwd.chapPasswdLen > CHAP_MAX_PASSWD)
	    fail("configure_chap_peer: CHAP secret > 255 octets\n");
	memcpy((void *)passwd.chapPasswd, (void *)p->auth.chap_peer.secret,
		passwd.chapPasswdLen);

	if (p->auth.chap_peer.name == NULL)
	    fail("configure_chap_peer: Must specify a CHAP name\n");
	passwd.chapNameLen = strlen(p->auth.chap_peer.name);
	if (passwd.chapNameLen > CHAP_MAX_NAME)
	    fail("configure_chap_peer: CHAP name > 255 octets\n");
	memcpy((void *)passwd.chapName, (void *)p->auth.chap_peer.name,
		passwd.chapNameLen);

	cmio.ic_cmd = PPP_SET_REMOTE_PASSWD;
	cmio.ic_timout = 1;
	cmio.ic_len = sizeof (passwd);
	cmio.ic_dp = (char *)&passwd;

	if (ioctl(p->s, I_STR, &cmio) < 0)
	    fail("configure_chap_peer: ioctl failed\n");
}
