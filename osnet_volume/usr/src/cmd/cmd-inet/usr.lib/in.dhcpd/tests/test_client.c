/*
 * Copyright (c) 1996-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)test_client.c	1.10	99/02/23 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include <stropts.h>
#include <netinet/in_systm.h>
#include <netinet/dhcp.h>
#include <thread.h>

struct client {
	thread_t		id;
	PKT_LIST		*pktlistp;
	cond_t			cv;
	mutex_t			mtx;
	int			proto;
	u_char			chaddr[20];
	int			hlen;
};

#define	FALSE	0
#define	TRUE	1

static int clients, s;
static PKT request;
static char	ifname[IFNAMSIZ];
static mutex_t	go_mtx;
static cond_t	go_cv;
static int time_to_go;

static void
dhcpmsgtype(u_char pkt, char *buf)
{
	char	*p;

	switch (pkt) {
	case DISCOVER:
		p = "DISCOVER";
		break;
	case OFFER:
		p = "OFFER";
		break;
	case REQUEST:
		p = "REQUEST";
		break;
	case DECLINE:
		p = "DECLINE";
		break;
	case ACK:
		p = "ACK";
		break;
	case NAK:
		p = "NAK";
		break;
	case RELEASE:
		p = "RELEASE";
		break;
	case INFORM:
		p = "INFORM";
		break;
	default:
		p = "UNKNOWN";
		break;
	}

	(void) strcpy(buf, p);
}

static void *
client(void *args)
{
	PKT			crequest, *irequestp;
	PKT_LIST		*bp, *wbp, *twbp;
	struct client		*mep = (struct client *)args;
	time_t			retry_time = 2, lease, sleep_time;
	u_char			*endp;
	int			state, nstate, done, ms = -1;
	DHCP_OPT		*optp, *unused_optp;
	timespec_t		ts, tr;
	int			timeout, config;
	int			error = 0;
	thread_t		myself = thr_self();
	struct sockaddr_in	to, myip, maskip;
	struct in_addr		serverip;
	time_t			start_time, expired = 0;
	int			cidlen;
	u_char			cid[BUFSIZ];
	char			cifname[IFNAMSIZ];
	char			p[30], np[30];
	struct ifreq		ifr;

	(void) sleep((0x7 & myself) + 3); /* desynchronize clients */
	(void) fprintf(stdout, "Client %04d - started.\n", myself);
	start_time = time(NULL);
	(void) sprintf(cifname, "%s:%d", ifname, myself);

	to.sin_addr.s_addr = INADDR_BROADCAST;
	to.sin_port = htons(IPPORT_BOOTPS);
	to.sin_family = AF_INET;

	(void) memcpy(&crequest, &request, sizeof (request));
	(void) memcpy(crequest.chaddr, mep->chaddr, mep->hlen);

	if (mep->proto) {
		state = DISCOVER;
		optp = (DHCP_OPT *)&crequest.options[3]; /* skip TYPE */
		optp->code = CD_CLIENT_ID;
		optp->len = mep->hlen + 1;
		optp->value[0] = 0x01;
		(void) memcpy(&optp->value[1], mep->chaddr, mep->hlen);
		cidlen = sizeof (cid);
		(void) octet_to_ascii(optp->value, mep->hlen + 1, (char *)cid,
		    &cidlen);
		unused_optp = (DHCP_OPT *)&optp->value[mep->hlen + 1];
	} else {
		state = 0;
		cidlen = sizeof (cid);
		(void) octet_to_ascii(mep->chaddr, mep->hlen, (char *)cid,
		    &cidlen);
	}

	/* Use global descriptor at first */
	ms = s;

	myip.sin_addr.s_addr = htonl(INADDR_ANY);
	done = FALSE;
	config = FALSE;
	do {
		timeout = FALSE;

		if (time_to_go) {
			if (state == ACK) {
				state = RELEASE;
				(void) fprintf(stderr,
				    "Client %04d - RELEASEing %s\n",
				    myself, inet_ntoa(myip.sin_addr));
				optp = (DHCP_OPT *)crequest.options;
				(void) memset((char *)unused_optp, 0,
				    (int)((char *) &crequest.options[
				    sizeof (crequest.options)] -
				    (char *)unused_optp));
				optp->value[0] = RELEASE;
			} else {
				done = TRUE;
				(void) fprintf(stderr,
				    "Client %04d - terminated.\n", myself);
				break;
			}
		}

		if (state == REQUEST && expired < time(NULL)) {
			/* drop back to INIT state. */
			(void) fprintf(stderr,
			    "Client %04d - Dropping back to INIT.\n", myself);
			optp = (DHCP_OPT *)crequest.options;
			optp->value[0] = DISCOVER;
			(void) memset((char *)unused_optp, 0, (int)((char *)
			    &crequest.options[sizeof (crequest.options)] -
			    (char *)unused_optp));
			retry_time = 2;
			state = DISCOVER;
		}

		/* Send request... */
		crequest.secs = htons((u_short)(time(NULL) - start_time));
		crequest.xid = htonl((myself << 2) + time(NULL));
		if (sendto(ms, (char *)&crequest, sizeof (PKT), 0,
		    (struct sockaddr *)&to, sizeof (struct sockaddr)) < 0) {
			perror("Sendto");
			error = 4;
			thr_exit(&error);
		}

		if (state == RELEASE) {
			done = TRUE;
			(void) strcpy(ifr.ifr_name, cifname);
			if (ioctl(ms, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
				(void) fprintf(stderr,
"Client %04d - can't get interface flags on %s\n", myself, cifname);
				error = 7;
			}
			ifr.ifr_flags &= ~IFF_UP;
			if (ioctl(ms, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
				(void) fprintf(stderr,
"Client %04d - can't set interface flags on %s\n", myself, cifname);
				error = 7;
			}
			myip.sin_addr.s_addr = htonl(INADDR_ANY);
			ifr.ifr_addr = *(struct sockaddr *)&myip;
			if (ioctl(ms, SIOCSIFADDR, (caddr_t)&ifr)) {
				(void) fprintf(stderr,
"Client %04d - Can't unset address on %s\n", myself, cifname);
				error = 8;
			}
			(void) close(ms);
			break;
		}

		/* await reply */
		(void) mutex_lock(&mep->mtx);
		ts.tv_sec = time(NULL) + retry_time;
		ts.tv_nsec = 0;

		while (mep->pktlistp == NULL)
			if (cond_timedwait(&mep->cv, &mep->mtx, &ts) == ETIME) {
				timeout = TRUE;
				if (retry_time > 64)
					retry_time = 64;
				else
					retry_time *= 2;
				break;
			} else {
				if (time_to_go)
					break;
			}
		(void) mutex_unlock(&mep->mtx);

		if (time_to_go || timeout)
			continue;

		(void) mutex_lock(&mep->mtx);
		bp = NULL;
		wbp = mep->pktlistp;
		while (wbp != NULL) {
			irequestp = wbp->pkt;
			if (bp == NULL && memcmp(&crequest.xid, &irequestp->xid,
			    sizeof (crequest.xid)) == 0) {
				bp = wbp;
				wbp = wbp->next;
				continue;
			}
			(void) free(wbp->pkt);
			twbp = wbp;
			wbp = wbp->next;
			(void) free(twbp);
			(void) fprintf(stderr, "Client %04d - Moldy xid\n",
			    myself);
		}

		mep->pktlistp = NULL;
		(void) mutex_unlock(&mep->mtx);

		if (bp == NULL)
			continue;

		irequestp = bp->pkt;

		if (mep->proto) {
			/*
			 * Scan for CD_DHCP_TYPE, CD_SERVER_ID, and
			 * CD_LEASE_TIME if proto.
			 */
			nstate = 0;
			maskip.sin_addr.s_addr = serverip.s_addr = INADDR_ANY;
			maskip.sin_family = AF_INET;
			lease = (time_t)0;
			optp = (DHCP_OPT *)irequestp->options;
			endp = &irequestp->options[bp->len];
			while ((u_char *)optp < (u_char *)endp) {
				switch (optp->code) {
				case CD_DHCP_TYPE:
					nstate = optp->value[0];
					break;
				case CD_SUBNETMASK:
					(void) memcpy(&maskip.sin_addr,
					    optp->value,
					    sizeof (struct in_addr));
					break;
				case CD_SERVER_ID:
					(void) memcpy(&serverip, optp->value,
					    sizeof (struct in_addr));
					break;
				case CD_LEASE_TIME:
					(void) memcpy(&lease, optp->value,
					    sizeof (time_t));
					lease = htonl(lease);
					break;
				}
				optp = (DHCP_OPT *)&optp->value[optp->len];
			}
			if (state == DISCOVER && nstate == OFFER) {
				state = REQUEST;
				expired = time(NULL) + 60;
				/*
				 * Add in the requested IP address
				 * option and server ID.
				 */
				optp = (DHCP_OPT *)crequest.options;
				optp->value[0] = REQUEST;
				optp = unused_optp; /* step over CD_DHCP_TYPE */
				optp->code = CD_REQUESTED_IP_ADDR;
				optp->len = sizeof (struct in_addr);
				(void) memcpy(optp->value, &irequestp->yiaddr,
				    sizeof (struct in_addr));
				optp = (DHCP_OPT *)&optp->value[
				    sizeof (struct in_addr)];
				optp->code = CD_SERVER_ID;
				optp->len = sizeof (struct in_addr);
				(void) memcpy(optp->value, &serverip,
				    sizeof (struct in_addr));
				(void) free(bp->pkt);
				(void) free(bp);
				continue;
			} else if ((state == REQUEST || state == ACK) &&
			    nstate == ACK) {
				/*
				 * we're bound. defend the lease. Add the
				 * address to our interface. Due to the
				 * service architecture of this program,
				 * we can't unset the broadcast bit..
				 */
				state = ACK;
				nstate = 0;
				retry_time = 2;
				myip.sin_family = AF_INET;
				myip.sin_addr.s_addr = irequestp->yiaddr.s_addr;
				crequest.ciaddr.s_addr = myip.sin_addr.s_addr;
				optp = unused_optp;
				optp->code = CD_LEASE_TIME;
				optp->len = sizeof (time_t);
				(void) memcpy(optp->value, &lease,
				    sizeof (time_t));
				optp = (DHCP_OPT *)
				    &optp->value[sizeof (time_t)];
				(void) memset((char *)optp, 0, (int)((char *)
				    &crequest.options[
				    sizeof (crequest.options)] - (char *)optp));
				to.sin_addr.s_addr = serverip.s_addr;

				if (lease == -1) {
					done = TRUE; /* permanent lease */
					sleep_time = 0;
				} else {
					sleep_time = lease / 2;
					lease = time(NULL) + lease;
				}

				(void) fprintf(stdout,
"Client %04d(%s) - DHCP: %s == %s",
				    myself, cid, inet_ntoa(myip.sin_addr),
				    (lease == -1) ? "Forever\n" :
				    ctime(&lease));
				if (!config) {
					/* Add mask and address */
					if ((ms = socket(AF_INET, SOCK_DGRAM,
					    0)) < 0) {
						(void) fprintf(stderr,
"Client %04d - can't open DGRAM socket.\n", myself);
						error = 7;
						break;
					}
					(void) strcpy(ifr.ifr_name, cifname);
					ifr.ifr_addr =
					    *(struct sockaddr *)&maskip;
					if (ioctl(ms, SIOCSIFNETMASK,
					    (caddr_t)&ifr)) {
						(void) fprintf(stderr,
"Client %04d - Can't set netmask: %s on %s\n",
						    myself,
						    inet_ntoa(maskip.sin_addr),
						    cifname);
						error = 7;
						(void) close(ms);
						break;
					}
					if (ioctl(ms, SIOCGIFFLAGS,
					    (caddr_t)&ifr) < 0) {
						(void) fprintf(stderr,
"Client %04d - can't get interface flags on %s\n", myself, cifname);
						error = 7;
						(void) close(ms);
						break;
					}
					ifr.ifr_flags |= IFF_UP;
					if (ioctl(ms, SIOCSIFFLAGS,
					    (caddr_t)&ifr) < 0) {
						(void) fprintf(stderr,
"Client %04d - can't set interface flags on %s\n", myself, cifname);
						error = 7;
						(void) close(ms);
						break;
					}
					ifr.ifr_addr =
					    *(struct sockaddr *)&myip;
					if (ioctl(ms, SIOCSIFADDR,
					    (caddr_t)&ifr)) {
						(void) fprintf(stderr,
"Client %04d - Can't set address on %s\n", myself, cifname);
						error = 8;
						(void) close(ms);
						break;
					}
					config = TRUE;
				}

				if (sleep_time != 0) {
					/* Go to sleep for 50% of lease time. */
					tr.tv_sec = time(NULL) + sleep_time;
					(void) fprintf(stderr,
"Client %04d - sleeping until %s",
					    myself, ctime(&tr.tv_sec));
					tr.tv_nsec = 0;
					(void) mutex_lock(&go_mtx);
					while (time_to_go == FALSE) {
						if (cond_timedwait(&go_cv,
						    &go_mtx, &tr) == ETIME)
							break;
					}
					(void) mutex_unlock(&go_mtx);
					(void) fprintf(stderr,
					    "Client %04d - awake\n", myself);

				}
			} else if (state == ACK && nstate == NAK) {
				(void) fprintf(stdout,
				    "Client %04d - DHCP: we got NAKed.\n",
				    myself);
				/* drop back to INIT state. */
				(void) fprintf(stderr,
				    "Client %04d - Dropping back to INIT.\n",
				    myself);
				(void) memcpy(&crequest, &request,
				    sizeof (request));
				(void) memcpy(crequest.chaddr, mep->chaddr,
				    mep->hlen);
				retry_time = 2;
				state = DISCOVER;
			} else {
				dhcpmsgtype(nstate, np);
				dhcpmsgtype(state, p);
				(void) fprintf(stderr,
"Client %04d - unexpected mesg: %s, when I'm in state: %s.\n",
				    myself, np, p);
				error = 9;
				break;
			}
		} else {
			done = TRUE;	/* BOOTP is done */
			(void) fprintf(stdout,
			    "Client %04d(%s) - BOOTP: %s\n",
			    myself, cid, inet_ntoa(irequestp->yiaddr));
		}
	} while (done == FALSE);

	if (!done) {
		(void) fprintf(stderr,
		    "Client %04d - %s: configuration failed.\n",
		    myself, (mep->proto) ? "DHCP" : "BOOTP");
	}

	wbp = mep->pktlistp;
	while (wbp != NULL) {
		twbp = wbp->next;
		if (wbp->pkt != NULL)
			(void) free(wbp->pkt);
		(void) free(wbp);
		wbp = twbp;
	}

	thr_exit(&error);
	return (NULL);	/* NOTREACHED */
}

/*
 * Never returns. Just loads client lists.
 */
static void *
service(void *args)
{
	struct client	*clientp = (struct client *)args;
	PKT_LIST 	*bp, *wbp;
	PKT		*irequestp;
	int		error = 0;
	struct pollfd	pfd;
	u_long		*bufp;	/* u_long to force alignment */
	int		len, i;

	pfd.fd = s;
	pfd.events = POLLIN | POLLPRI;

	for (;;) {
		pfd.revents = 0;
		if (poll(&pfd, (nfds_t)1, INFTIM) < 0) {
			(void) fprintf(stderr, "Service - can't poll...\n");
			error = 5;
			break;
		}

		(void) mutex_lock(&go_mtx);
		if (time_to_go == TRUE) {
			(void) fprintf(stderr, "Service - exiting...\n");
			error = 0;
			break;
		}

		(void) mutex_unlock(&go_mtx);
		len = BUFSIZ * 2;
		bufp = (u_long *)malloc(len);
		len = recv(s, (char *)bufp, len, 0);
		if (len < 0) {
			(void) fprintf(stderr, "Service - can't receive...\n");
			error = 6;
			break;
		} else {
			irequestp = (PKT *)bufp;
			for (i = 0; i < clients; i++) {
				if (memcmp(clientp[i].chaddr, irequestp->chaddr,
				    clientp[i].hlen) == 0) {
					(void) mutex_lock(&clientp[i].mtx);
					bp = malloc(sizeof (PKT_LIST));
					bp->pkt = irequestp;
					bp->len = len;
					(void) fprintf(stderr,
"Service - received packet for thread %04d...\n", clientp[i].id);
					if (clientp[i].pktlistp == NULL) {
						clientp[i].pktlistp = bp;
						bp->prev = NULL;
					} else {
						for (wbp = clientp[i].pktlistp;
						    wbp->next != NULL;
						    wbp = wbp->next)
							/* null */;
						wbp->next = bp;
						bp->prev = wbp;
					}
					bp->next = NULL;
					(void) cond_signal(&clientp[i].cv);
					(void) mutex_unlock(&clientp[i].mtx);
				}
			}
		}
	}
	thr_exit(&error);
	return (NULL);	/* NOTREACHED */
}

/* ARGSUSED */
static void *
sig_handle(void *arg)
{
	int		leave = FALSE;
	int		sig;
	sigset_t	set;
	char		buf[SIG2STR_MAX];

	(void) sigfillset(&set);

	while (leave == FALSE) {
		switch (sig = sigwait(&set)) {
		case SIGHUP:
		case SIGINT:
			/* FALLTHRU */
		case SIGTERM:
			(void) sig2str(sig, buf);
			(void) fprintf(stderr,
			    "Signal: %s received...Exiting\n", buf);
			(void) mutex_lock(&go_mtx);
			time_to_go = TRUE;
			(void) cond_broadcast(&go_cv);
			(void) mutex_unlock(&go_mtx);
			leave = TRUE;
			break;
		default:
			(void) sig2str(sig, buf);
			(void) fprintf(stderr,
			    "Signal: %s received...Ignoring\n", buf);
			leave = FALSE;
			break;
		}
	}
	thr_exit((void *)NULL);
	return (NULL); /* NOTREACHED */
}

int
main(int argc, char *argv[])
{
	int i, j, proto, threrror = 0, *threrrorp;
	struct sockaddr_in	from;
	int sockoptbuf = 1;
	register char *endp, *octet;
	struct client	*clientsp;
	thread_t	service_id, sig_id;
	sigset_t	set;
	u_int buf;

	if (argc != 5) {
		(void) fprintf(stderr,
		    "%s <interface> <ether_addr> <protocol> <clients>\n",
		    argv[0]);
		return (1);
	}

	(void) strcpy(ifname, argv[1]);

	if (strcasecmp(argv[3], "dhcp") == 0)
		proto = TRUE;
	else
		proto = FALSE;

	clients = atoi(argv[4]);

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Socket");
		return (1);
	}

	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&sockoptbuf,
	    (int)sizeof (sockoptbuf)) < 0) {
		perror("Setsockopt");
		return (2);
	}

	from.sin_family = AF_INET;
	from.sin_addr.s_addr = INADDR_ANY;
	from.sin_port = htons(IPPORT_BOOTPC);
	if (bind(s, (struct sockaddr *)&from, sizeof (from)) < 0) {
		perror("Bind");
		return (3);
	}

	request.op = 1;		/* BOOTP request */
	request.htype = 1;	/* Ethernet */
	request.hlen = 6;	/* Ethernet addr len */

	endp = octet = argv[2];
	for (i = 0; i < (int)request.hlen && octet != NULL; i++) {
		if ((endp = (char *)strchr(endp, ':')) != NULL)
			*endp++ = '\0';
		(void) sscanf(octet, "%x", &buf);
		request.chaddr[i] = (u_char)buf;
		octet = endp;
	}

	/* broadcast bit */
	request.flags = htons(0x8000);

	/* magic cookie */
	request.cookie[0] = 99;
	request.cookie[1] = 130;
	request.cookie[2] = 83;
	request.cookie[3] = 99;

	if (proto) {
		/* Pretend to be a discover packet */
		request.options[0] = CD_DHCP_TYPE;
		request.options[1] = 1;
		request.options[2] = DISCOVER;
		request.options[3] = 0xff;

		(void) cond_init(&go_cv, USYNC_THREAD, NULL);
		(void) mutex_init(&go_mtx, USYNC_THREAD, NULL);
	}

	(void) sigfillset(&set);
	(void) thr_sigsetmask(SIG_SETMASK, &set, NULL);

	/*
	 * Create signal handling thread.
	 */
	if (thr_create(NULL, 0, sig_handle, NULL,
	    THR_NEW_LWP | THR_DAEMON | THR_DETACHED, &sig_id) != 0) {
		(void) fprintf(stderr, "Error starting signal handler.\n");
		return (1);
	} else
		(void) fprintf(stderr, "Started Signal handler: %04d...\n",
		    sig_id);

	(void) sigdelset(&set, SIGABRT);

	/*
	 * Create the client threads
	 */
	clientsp = (struct client *)malloc(sizeof (struct client) * clients);
	(void) memset((char *)clientsp, 0, sizeof (struct client) * clients);
	if (clientsp == NULL)
		return (1);
	for (i = 0; i < clients; i++) {
		(void) memcpy(clientsp[i].chaddr, request.chaddr, request.hlen);
		clientsp[i].hlen = request.hlen;
		if (i > 100)
			j = 3;
		else if (i > 50)
			j = 2;
		else if (i > 5)
			j = 1;
		else
			j = 0;
		clientsp[i].chaddr[j] = (unsigned char)i;
		clientsp[i].chaddr[3] += (unsigned char)j;
		clientsp[i].chaddr[4] = (unsigned char)(i * j);
		(void) cond_init(&clientsp[i].cv, USYNC_THREAD, 0);
		(void) mutex_init(&clientsp[i].mtx, USYNC_THREAD, 0);
		clientsp[i].proto = proto;
		if (thr_create(NULL, NULL, client, (void *)&clientsp[i],
		    THR_NEW_LWP | THR_SUSPENDED, &clientsp[i].id) != 0) {
			(void) fprintf(stderr, "Error starting Client %04d\n",
			    clientsp[i].id);
		}
	}

	/*
	 * Create/start the service thread.
	 */
	if (thr_create(NULL, NULL, service, (void *)clientsp, THR_NEW_LWP,
	    &service_id) != 0) {
		(void) fprintf(stderr, "Error starting Service %d\n",
		    service_id);
		exit(1);
	} else
		(void) fprintf(stderr, "Started Service %04d...\n",
		    service_id);

	/*
	 * Continue the client threads.
	 */
	for (i = 0; i < clients; i++)
		(void) thr_continue(clientsp[i].id);

	/*
	 * join them
	 */
	threrrorp = &threrror;
	for (i = 0; i < clients; i++) {
		if (thr_join(clientsp[i].id, NULL, (void **)&threrrorp) == 0) {
			if (threrror != 0) {
				(void) fprintf(stdout,
				    "Client %04d - exited with %d\n",
				    clientsp[i].id, threrror);
			}
			(void) cond_destroy(&clientsp[i].cv);
			(void) mutex_destroy(&clientsp[i].mtx);
		}
	}

	(void) close(s);	/* force service out of poll */

	if (thr_join(service_id, NULL, (void **)&threrrorp) == 0) {
		if (threrror != 0) {
			(void) fprintf(stdout, "Service - exited with %d\n",
			    threrror);
		}
	}

	(void) free((char *)clientsp);
	(void) fprintf(stdout, "Exiting...\n");

	return (0);
}
