/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ns_connect.c	1.4	99/12/06 SMI"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <synch.h>
#include <time.h>
#include <libintl.h>
#include <thread.h>
#include "ns_sldap.h"
#include "ns_internal.h"

extern time_t conv_time(char *s);
extern int ldap_sasl_cram_md5_bind_s(LDAP *, char *, struct berval *,
		LDAPControl **, LDAPControl **);

static LDAP *MakeSession(const char *, const Auth_t *, ns_ldap_error_t **);

#define	LDAP_RESULT_TIMEOUT	60

static mutex_t	connPoolLock = DEFAULTMUTEX;

typedef struct connection {
	ConnectionID		connectionId;
	boolean_t		usedBit;
	char			*serverAddr;
	Auth_t			*auth;
	time_t			lastTimeUsed;	/* last time used */
	LDAP			*ld;
	thread_t		threadID;	/* thread ID using it */
	ns_ldap_cookie_t	*cookieInfo;
} Connection;

static Connection **connectionList = NULL;

#define	CACHE_INC	100
static int ConnCacheSize = CACHE_INC;

/*
 * Free the Connection structure
 */
static void
freeConnection(Connection* con)
{
	if (con == NULL)
		return;
	if (con->serverAddr)
		free(con->serverAddr);
	if (con->auth)
		__ns_ldap_freeAuth(&(con->auth));
	free(con);
}

#ifdef DEBUG

/*
 * printAuth(): prints the authentication structure
 */
static void
printAuth(int fd, Auth_t *auth)
{
	char buf[BUFSIZ];

	if (auth) {
		snprintf(buf, BUFSIZ, "AuthType=%d\n", auth->type);
		write(fd, buf, strlen(buf));
		snprintf(buf, BUFSIZ, "AuthType=%d\n", auth->type);
		write(fd, buf, strlen(buf));
		snprintf(buf, BUFSIZ, "SecType=%d\n", auth->security);
		write(fd, buf, strlen(buf));
		switch (auth->type) {
		case NS_LDAP_AUTH_SIMPLE:
		case NS_LDAP_AUTH_SASL_CRAM_MD5:
			snprintf(buf, BUFSIZ, "userID=%s\n",
				auth->cred.unix_cred.userID);
			write(fd, buf, strlen(buf));
			snprintf(buf, BUFSIZ, "passwd=%s\n",
				auth->cred.unix_cred.passwd);
			write(fd, buf, strlen(buf));
			break;
		case NS_LDAP_AUTH_TLS:
			snprintf(buf, BUFSIZ, "certPath=%s\n",
				auth->cred.cert_cred.path);
			write(fd, buf, strlen(buf));
			snprintf(buf, BUFSIZ, "passwd=%s\n",
				auth->cred.cert_cred.passwd);
			write(fd, buf, strlen(buf));
			break;
		case NS_LDAP_AUTH_SASL_GSSAPI:
			break;
		default:
			break;
		}
	}
}

/*
 * printConnection(): prints the connection structure
 */
static void
printConnection(int fd, Connection *con)
{
	char buf[BUFSIZ];

	if (con == NULL)
		return;
	snprintf(buf, BUFSIZ, "connectionID=%d\n", con->connectionId);
	write(fd, buf, strlen(buf));
	snprintf(buf, BUFSIZ, "usedBit=%d\n", con->usedBit);
	write(fd, buf, strlen(buf));
	snprintf(buf, BUFSIZ, "lastTimeUsed=%d\n", con->lastTimeUsed);
	write(fd, buf, strlen(buf));
	snprintf(buf, BUFSIZ, "threadID=%d\n", con->threadID);
	write(fd, buf, strlen(buf));
	if (con->serverAddr) {
		snprintf(buf, BUFSIZ, "serverAddr=%s\n", con->serverAddr);
		write(fd, buf, strlen(buf));
	}
	printAuth(fd, con->auth);
	strcpy(buf, "-----------------------------------------------\n");
	write(fd, buf, strlen(buf));
}

#endif /* DEBUG */


/*
 * addConnetion(): adds a connection to the list.  It will also sets
 * use bit as it adds.
 * Returns: -1 = failure, new Connection ID = success
 */
static int
addConnection(Connection* con)
{
	int i;

	if (!con)
		return (-1);
#ifdef DEBUG
	fprintf(stderr, "Adding connection thrid=%d\n", con->threadID);
#endif /* DEBUG */
	mutex_lock(&connPoolLock);
	if (connectionList == NULL) {
		connectionList = calloc(CACHE_INC,
				sizeof (struct connection **));
		if (!connectionList) {
			mutex_unlock(&connPoolLock);
			return (-1);
		}
#ifdef DEBUG
		fprintf(stderr, "Increased ConnCacheSize=%d\n", ConnCacheSize);
#endif /* DEBUG */
	}
	for (i = 0; (i < ConnCacheSize) && (connectionList[i] != NULL); ++i)
		;
	if (i == ConnCacheSize) {
		/* run out of array, need to increase connectionList */
		Connection **cl;
		cl = (Connection **) realloc(connectionList,
			(ConnCacheSize + CACHE_INC) *
			sizeof (Connection **));
		if (!cl) {
			mutex_unlock(&connPoolLock);
			return (-1);
		}
		memset(cl + ConnCacheSize, 0, CACHE_INC);
		connectionList = cl;
		ConnCacheSize += CACHE_INC;
#ifdef DEBUG
		fprintf(stderr, "Increased ConnCacheSize=%d\n", ConnCacheSize);
#endif /* DEBUG */
	}
	connectionList[i] = con;
	con->usedBit = B_TRUE;
	mutex_unlock(&connPoolLock);
	con->connectionId = i;
	con->lastTimeUsed = time(0);
#ifdef DEBUG
	fprintf(stderr, "Connection added [%d]\n", i);
	printConnection(2, con);
#endif /* DEBUG */
	return (i);
}

/*
 * deleteConnection(): removes a connection to the list
 * Returns: -1 = failure, the deleted Connection ID = success
 */
static int
deleteConnection(ConnectionID cID)
{
	Connection *cp;

	if ((cID < 0) || (cID >= ConnCacheSize))
		return (-1);
#ifdef DEBUG
	fprintf(stderr, "Deleting connection cID=%d\n", cID);
#endif /* DEBUG */
	mutex_lock(&connPoolLock);

	cp = connectionList[cID];
	/* sanity check before removing */
	if (!cp || !cp->usedBit || cp->threadID != thr_self()) {
		mutex_unlock(&connPoolLock);
		return (-1);
	}
	connectionList[cID] = NULL;
	mutex_unlock(&connPoolLock);
	(void) ldap_unbind(cp->ld);
	freeConnection(cp);
	return (cID);
}

/*
 * findConnection(): finds a connection from the list that matches the
 * criteria specified in Connection structure.
 * Returns: -1 = failure, the Connection ID found = success
 */
static int
findConnection(const char *serverAddr, Auth_t *auth, Connection **conp)
{
	Connection *cp;
	int i;

	if (!serverAddr || !*serverAddr || !auth)
		return (-1);
#ifdef DEBUG
	fprintf(stderr, "Find connection\n");
	fprintf(stderr, "Looking for ....\n");
	fprintf(stderr, "serverAddr=%s\n", serverAddr);
	printAuth(2, auth);
#endif /* DEBUG */
	if (connectionList == NULL)
		return (-1);
	mutex_lock(&connPoolLock);
	for (i = 0; i < ConnCacheSize; ++i) {
		if (connectionList[i] == NULL)
			continue;
		cp = connectionList[i];
#ifdef DEBUG
		fprintf(stderr, "checking connection [%d] ....\n", i);
		printConnection(2, cp);
#endif /* DEBUG */
		if ((cp->usedBit) ||
		    (strcmp(serverAddr, cp->serverAddr) != 0) ||
		    (cp->auth->type != auth->type) ||
		    (cp->auth->security != auth->security))
			continue;
		if (((cp->auth->type == NS_LDAP_AUTH_SASL_CRAM_MD5) ||
		    (cp->auth->type == NS_LDAP_AUTH_SIMPLE)) &&
		    (strcasecmp(cp->auth->cred.unix_cred.userID,
			    auth->cred.unix_cred.userID) != 0))
				continue;
		cp->usedBit = B_TRUE;
		mutex_unlock(&connPoolLock);
		cp->threadID = thr_self();
		*conp = cp;
#ifdef DEBUG
		fprintf(stderr, "Connection found cID=%d\n", i);
#endif /* DEBUG */
		return (i);
	}
	mutex_unlock(&connPoolLock);
	return (-1);
}

/*
 * releaseConnection(): releases the connection back to the list.
 * Returns: -1 = failure, the connectionID released = success
 */
static int
releaseConnection(ConnectionID cID)
{
	Connection *cp;

	if ((cID < 0) || (cID >= ConnCacheSize))
		return (-1);
#ifdef DEBUG
	fprintf(stderr, "Release connection cID=%d\n", cID);
#endif /* DEBUG */
	mutex_lock(&connPoolLock);
	cp = connectionList[cID];
	/* sanity check before removing */
	if (!cp || !cp->usedBit || cp->threadID != thr_self()) {
		mutex_unlock(&connPoolLock);
		return (-1);
	}
	cp->usedBit = B_FALSE;
	cp->threadID = 0;	/* unmark the threadID */
	cp->lastTimeUsed = time(0);
	mutex_unlock(&connPoolLock);
	return (cID);
}


/*
 * Failure: returns NULL, error code and message should be in errorp
 * Success: returns the pointer to the LDAP structure
 */
/*ARGSUSED*/
LDAP *
MakeConnection(const char *serverAddr, Auth_t *auth,
	int flag, ConnectionID *cID, ns_ldap_error_t **errorp)
{
	Connection *con;
	ConnectionID id;
	char errstr[MAXERROR];

	*errorp = NULL;

	/* server address must be defined */
	if (!serverAddr && !*serverAddr) {
		sprintf(errstr, gettext("Server not defined"));
		MKERROR(*errorp, LDAP_CONNECT_ERROR, strdup(errstr), NULL);
		return (NULL);
	}

	/* auth must be defined */
	if (!auth) {
		sprintf(errstr, gettext("Missing Authentication Info"));
		MKERROR(*errorp, LDAP_CONNECT_ERROR, strdup(errstr), NULL);
		return (NULL);
	}

	if ((id = findConnection(serverAddr, auth, &con)) != -1) {
		/* connection found in cache */
#ifdef DEBUG
		fprintf(stderr, "connection found in cache %d\n", id);
#endif /* DEBUG */
		*cID = id;
		return (con->ld);
	}

	/* No cached connection, create cache structure */
	if ((con = calloc(1, sizeof (struct connection))) == NULL)
		return (NULL);

	con->serverAddr = strdup((char *)serverAddr);

	con->auth =  (Auth_t *)calloc(1, sizeof (Auth_t));
	con->auth->type = auth->type;
	con->auth->security = auth->security;
	switch (auth->type) {
	case NS_LDAP_AUTH_SIMPLE:
	case NS_LDAP_AUTH_SASL_CRAM_MD5:
		con->auth->cred.unix_cred.userID =
				strdup(auth->cred.unix_cred.userID);
		con->auth->cred.unix_cred.passwd =
				strdup(auth->cred.unix_cred.passwd);
		break;
	case NS_LDAP_AUTH_NONE:
		break;
	case NS_LDAP_AUTH_SASL_GSSAPI:
	default:
		sprintf(errstr, gettext("Invalid Authentication Info"));
		MKERROR(*errorp, LDAP_AUTH_METHOD_NOT_SUPPORTED, strdup(errstr),
				NULL);
		freeConnection(con);
		return (NULL);
	}

	/* get connection */
	if ((con->ld = MakeSession(serverAddr, auth, errorp)) == NULL) {
		freeConnection(con);
		return (NULL);
	}
	con->threadID = thr_self();
	if ((id = addConnection(con)) == -1) {
		sprintf(errstr, gettext("addConnection failed"));
		MKERROR(*errorp, LDAP_CONNECT_ERROR, strdup(errstr), NULL);
		freeConnection(con);
		return (NULL);
	}
#ifdef DEBUG
	fprintf(stderr, "connection added into cache %d\n", id);
#endif /* DEBUG */
	*cID = id;
	return (con->ld);
}


/*
 * Failure: returns -1
 * Success: returns the Connection ID
 */
int
DropConnection(ConnectionID cId, int flag)
{
	if (flag & NS_LDAP_KEEP_CONN)
		return (releaseConnection(cId));
	return (deleteConnection(cId));
}

/*
 * XXX The following ldap_ssl_open() is only temporary.  This should be
 * removed once libldap supports TLS.
 */
/*ARGSUSED*/
LDAP *
ldap_ssl_open(char *host, int port, char *keyname)
{
	return (NULL);
}


static LDAP *
MakeSession(const char *serverAddr, const Auth_t *auth,
	ns_ldap_error_t **errorp)
{
	LDAP *ld = NULL;
	char *binddn, *passwd;
	int ldapVersion = LDAP_VERSION3;
	int derefOption = LDAP_DEREF_ALWAYS;
	int referralOption = (int)LDAP_OPT_ON;
	int rc;
	int timelimit = 0;
	int sizelimit = 0;
	char errstr[MAXERROR];
	int errnum = 0;
	LDAPMessage	*resultMsg;
	int msgId;
	void	**paramVal = NULL;
	struct timeval tv;

	*errorp = NULL;

	if (!serverAddr || !*serverAddr) {
		sprintf(errstr, gettext("Server not defined"));
		MKERROR(*errorp, LDAP_CONNECT_ERROR, strdup(errstr), NULL);
		return (NULL);
	}

	if (!auth) {
		sprintf(errstr, gettext("Authentication not defined"));
		MKERROR(*errorp, LDAP_CONNECT_ERROR, strdup(errstr), NULL);
		return (NULL);
	}

	/* set up transport */
	switch (auth->security) {
	case NS_LDAP_SEC_NONE:
#ifdef DEBUG
		fprintf(stderr, "+++Unsecure transport\n");
#endif /* DEBUG */
		if ((ld = ldap_init((char *)serverAddr, LDAP_PORT)) == NULL) {
			char *p = strerror(errno);
			MKERROR(*errorp, LDAP_CONNECT_ERROR, strdup(p), NULL);
			return (NULL);
		}
		break;
	/* to be coded later once the library supports these XXX */
	case NS_LDAP_SEC_SASL_INTEGRITY:
		break;
	/* to be coded later once the library supports these XXX */
	case NS_LDAP_SEC_SASL_PRIVACY:
		break;
	case NS_LDAP_SEC_TLS:
#ifdef DEBUG
		fprintf(stderr, "+++TLS transport\n");
#endif /* DEBUG */

		if ((ld = ldap_ssl_open((char *)serverAddr, 0, NULL)) == NULL) {
			sprintf(errstr, gettext("TLS security failed"));
			MKERROR(*errorp, LDAP_CONNECT_ERROR, strdup(errstr),
				NULL);
			return (NULL);
		}
		break;
	default:
		MKERROR(*errorp, LDAP_AUTH_METHOD_NOT_SUPPORTED, NULL, NULL);
		return (NULL);
	}

	ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &ldapVersion);
	ldap_set_option(ld, LDAP_OPT_DEREF, &derefOption);
	ldap_set_option(ld, LDAP_OPT_REFERRALS, &referralOption);
	ldap_set_option(ld, LDAP_OPT_TIMELIMIT, &timelimit);
	ldap_set_option(ld, LDAP_OPT_SIZELIMIT, &sizelimit);

	switch (auth->type) {
	case NS_LDAP_AUTH_NONE:
#ifdef DEBUG
		fprintf(stderr, "+++Anonymous bind\n");
#endif /* DEBUG */
		break;

	case NS_LDAP_AUTH_SIMPLE:
	case NS_LDAP_AUTH_SASL_CRAM_MD5:
		passwd = auth->cred.unix_cred.passwd;
		if (!passwd || !auth->cred.unix_cred.userID) {
			sprintf(errstr, gettext("Missing credentials"));
			MKERROR(*errorp, LDAP_INVALID_CREDENTIALS,
				strdup(errstr), NULL);
			ldap_unbind(ld);
			return (NULL);
		}
		binddn = strdup(auth->cred.unix_cred.userID);

		if (auth->type == NS_LDAP_AUTH_SIMPLE) {
			/* NS_LDAP_AUTH_SIMPLE */
#ifdef DEBUG
			fprintf(stderr, "+++Simple bind\n");
#endif /* DEBUG */
			msgId = ldap_simple_bind(ld, binddn, passwd);

			if (msgId == -1) {
				ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER,
					(void *)&errnum);
				sprintf(errstr, gettext("Binding failed - %s"),
						ldap_err2string(errnum));
				MKERROR(*errorp, errnum, strdup(errstr), NULL);
				ldap_unbind(ld);
				free(binddn);
				return (NULL);
			}

			(void) __ns_ldap_getParam(NULL, NS_LDAP_SEARCH_TIME_P,
				&paramVal, errorp);

			/* If getParam() fails use the default timeout */

			if (paramVal) {
				char	*tstr = (char *)*paramVal;
				tv.tv_sec = conv_time(tstr);
			} else
				tv.tv_sec = LDAP_RESULT_TIMEOUT;

			(void) __ns_ldap_freeParam(&paramVal);

			tv.tv_usec = 0;
			rc = ldap_result(ld, msgId, 0, &tv, &resultMsg);

			if ((rc == -1) || (rc == 0)) {
				ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER,
					(void *)&errnum);
				sprintf(errstr, gettext("Binding failed - %s"),
						ldap_err2string(errnum));
				ldap_unbind(ld);
				MKERROR(*errorp, rc, strdup(errstr), NULL);
				free(binddn);
				return (NULL);
			}

			errnum = ldap_result2error(ld, resultMsg, 1);
			if (errnum != 0) {
				sprintf(errstr, gettext("Binding failed - %s"),
						ldap_err2string(errnum));
				ldap_unbind(ld);
				MKERROR(*errorp, errnum, strdup(errstr), NULL);
				free(binddn);
				return (NULL);
			}

		} else {

			/* NS_LDAP_AUTH_SASL_CRAM_MD5 */
			struct berval cred;

#ifdef DEBUG
			fprintf(stderr, "+++SASL_CRAM-MD5 bind\n");
#endif /* DEBUG */
			cred.bv_val = passwd;
			cred.bv_len = strlen(passwd);

/*
 *			The ldap_sasl_cram_md5_bind_s is a non-standard
 *			interface.  This function call must be changed
 *			once we are using a standard ldap library.
 */

			if ((rc = ldap_sasl_cram_md5_bind_s(ld, binddn, &cred,
					NULL, NULL)) != LDAP_SUCCESS) {
				ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER,
					(void *)&errnum);
				if (errnum != 0)
					sprintf(errstr,
					gettext("Binding failed - %s"),
					ldap_err2string(errnum));
				MKERROR(*errorp, rc, strdup(errstr), NULL);
				free(binddn);
				ldap_unbind(ld);
				return (NULL);
			}
		}
		free(binddn);
		break;
	case NS_LDAP_AUTH_SASL_GSSAPI:
	default:
		MKERROR(*errorp, LDAP_AUTH_METHOD_NOT_SUPPORTED, NULL, NULL);
		ldap_unbind(ld);
		return (NULL);
	}

	return (ld);
}



int
__s_api_getCookieInfo(
	ConnectionID cID,
	ns_ldap_cookie_t **cookieInfo,
	LDAP **ld)
{

	Connection *cp;

	if ((cID < 0) || (cID >= ConnCacheSize))
		return (-1);
#ifdef DEBUG
	fprintf(stderr, "Getting Cookie Information cID=%d\n", cID);
#endif /* DEBUG */

	mutex_lock(&connPoolLock);

	cp = connectionList[cID];

	/* sanity check */

	if (!cp || !cp->usedBit) {
		mutex_unlock(&connPoolLock);
		return (-1);
	}

	*cookieInfo = cp->cookieInfo;
	*ld = cp->ld;

	mutex_unlock(&connPoolLock);
	return (cID);
}

int
__s_api_setCookieInfo(
	ConnectionID cID,
	ns_ldap_cookie_t *cookieInfo)
{

	Connection *cp;

	if ((cID < 0) || (cID >= ConnCacheSize))
		return (-1);
#ifdef DEBUG
	fprintf(stderr, "Setting Cookie Information cID=%d\n", cID);
#endif /* DEBUG */

	mutex_lock(&connPoolLock);

	cp = connectionList[cID];

	/* sanity check */

	if (!cp || !cp->usedBit) {
		mutex_unlock(&connPoolLock);
		return (-1);
	}

	cp->cookieInfo = cookieInfo;
	mutex_unlock(&connPoolLock);
	return (cID);
}
