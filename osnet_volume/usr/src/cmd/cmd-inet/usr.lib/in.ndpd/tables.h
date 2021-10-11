/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)tables.h	1.2	99/12/06 SMI"

enum adv_states { NO_ADV = 0, REG_ADV, INIT_ADV, SOLICIT_ADV, FINAL_ADV };
enum adv_events { ADV_OFF, START_INIT_ADV, START_FINAL_ADV, RECEIVED_SOLICIT,
			ADV_TIMER };

enum solicit_states { NO_SOLICIT = 0, INIT_SOLICIT, DONE_SOLICIT };
enum solicit_events { SOLICIT_OFF, START_INIT_SOLICIT, SOL_TIMER,
			SOLICIT_DONE };

/*
 * Data structures used to handle configuration variables set in ndpd.conf.
 * cf_notdefault is set for variables explicitly set in ndpd.conf.
 */
struct confvar {
	uint_t		cf_value;
	boolean_t	cf_notdefault;
};

/*
 * Interfaces configuration variable indicies
 */
#define	I_DupAddrDetectTransmits	0	/* From RFC 2462 */
#define	I_AdvSendAdvertisements		1
#define	I_MaxRtrAdvInterval		2	/* In seconds */
#define	I_MinRtrAdvInterval		3	/* In seconds */
#define	I_AdvManagedFlag		4
#define	I_AdvOtherConfigFlag		5
#define	I_AdvLinkMTU			6
#define	I_AdvReachableTime		7	/* In milliseconds */
#define	I_AdvRetransTimer		8	/* In milliseconds */
#define	I_AdvCurHopLimit		9
#define	I_AdvDefaultLifetime		10	/* In seconds */
#define	I_IFSIZE			11	/* # of variables */

/*
 * A doubly linked list of all physical interfaces that each contain a
 * doubly linked list of prefixes (i.e. logical interfaces) and default
 * routers.
 */
struct phyint {
	struct phyint	*pi_next;
	struct phyint	*pi_prev;
	struct prefix	*pi_prefix_list;	/* Doubly linked prefixes */
	struct router	*pi_router_list;	/* Doubly linked routers */

	uint_t		pi_index;		/* Identifier > 0 */
	char		pi_name[LIFNAMSIZ];	/* Used to identify it */
	int		pi_sock;		/* For sending and receiving */
	struct in6_addr	pi_ifaddr;		/* Local address */
	uint_t		pi_flags;		/* IFF_* flags */
	uint_t		pi_hdw_addr_len;
	uchar_t		pi_hdw_addr[ND_MAX_HDW_LEN];
	uint_t		pi_mtu;			/* From SIOCGLIFMTU */
	struct in6_addr pi_token;
	uint_t		pi_token_length;
	struct in6_addr	pi_dst_token;		/* For POINTOPOINT */

	uint_t		pi_state;		/* PI_* below */
	uint_t		pi_kernel_state;	/* PI_* below */
	boolean_t	pi_in_use;		/* To detect removed phyints */
	boolean_t	pi_onlink_default;	/* Has onlink default route */
	uint_t		pi_num_k_routers;	/* # routers in kernel */
	uint_t		pi_prefix_time_since_saved;	/* In milliseconds */
	uint_t		pi_reach_time_since_random;	/* In milliseconds */

	/* Applies if pi_AdvSendAdvertisements */
	uint_t		pi_adv_time_left;	/* In milliseconds */
	uint_t		pi_adv_time_since_sent;	/* In milliseconds */
	enum adv_states	pi_adv_state;
	uint_t		pi_adv_count;

	/* Applies if not pi_AdvSendAdvertisements */
	uint_t		pi_sol_time_left;	/* In milliseconds */
	enum solicit_states pi_sol_state;
	uint_t		pi_sol_count;

	/* Configurable variables on router */
	struct confvar	pi_config[I_IFSIZE];
#define	pi_DupAddrDetectTransmits pi_config[I_DupAddrDetectTransmits].cf_value
#define	pi_AdvSendAdvertisements pi_config[I_AdvSendAdvertisements].cf_value
#define	pi_MaxRtrAdvInterval	pi_config[I_MaxRtrAdvInterval].cf_value
#define	pi_MinRtrAdvInterval	pi_config[I_MinRtrAdvInterval].cf_value
#define	pi_AdvManagedFlag	pi_config[I_AdvManagedFlag].cf_value
#define	pi_AdvOtherConfigFlag	pi_config[I_AdvOtherConfigFlag].cf_value
#define	pi_AdvLinkMTU		pi_config[I_AdvLinkMTU].cf_value
#define	pi_AdvReachableTime	pi_config[I_AdvReachableTime].cf_value
#define	pi_AdvRetransTimer	pi_config[I_AdvRetransTimer].cf_value
#define	pi_AdvCurHopLimit	pi_config[I_AdvCurHopLimit].cf_value
#define	pi_AdvDefaultLifetime	pi_config[I_AdvDefaultLifetime].cf_value

	/* Recorded variables on node/host */
	uint_t		pi_LinkMTU;
	uint_t		pi_CurHopLimit;
	uint_t		pi_BaseReachableTime;		/* In milliseconds */
	uint_t		pi_ReachableTime;		/* In milliseconds */
	/*
	 * The above value should be a uniformly-distributed random
	 * value between ND_MIN_RANDOM_FACTOR and
	 * ND_MAX_RANDOM_FACTOR times BaseReachableTime
	 * milliseconds.  A new random value should be
	 * calculated when BaseReachableTime changes (due to
	 * Router Advertisements) or at least every few hours
	 * even if no Router Advertisements are received.
	 * Tracked using pi_each_time_since_random.
	 */
	uint_t		pi_RetransTimer;		/* In milliseconds */
};

/*
 * pi_state/pr_kernel_state values
 */
#define	PI_PRESENT		0x01
#define	PI_JOINED_ALLNODES	0x02	/* allnodes multicast joined */
#define	PI_JOINED_ALLROUTERS	0x04	/* allrouters multicast joined */


/*
 * Prefix configuration variable indicies
 */
#define	I_AdvValidLifetime	0	/* In seconds */
#define	I_AdvOnLinkFlag		1
#define	I_AdvPreferredLifetime	2	/* In seconds */
#define	I_AdvAutonomousFlag	3
#define	I_AdvValidExpiration	4	/* Seconds left */
#define	I_AdvPreferredExpiration 5	/* Seconds left */
#define	I_PREFIXSIZE		6	/* # of variables */

/*
 * A doubly linked list of prefixes for onlink and addrconf.
 */
struct prefix {
	struct prefix	*pr_next;	/* Next prefix for this physical */
	struct prefix	*pr_prev;	/* Prev prefix for this physical */
	struct phyint	*pr_physical;	/* Back pointer */

	struct in6_addr	pr_prefix;	/* Used to indentify prefix */
	uint_t		pr_prefix_len;	/* Num bits valid */

	char		pr_name[LIFNAMSIZ];
	struct in6_addr	pr_address;
	uint_t		pr_flags;	/* IFF_* flags */

	uint_t		pr_state;	/* PR_ONLINK | PR_AUTO etc */
	uint_t		pr_kernel_state; /* PR_ONLINK | PR_AUTO etc */
	boolean_t	pr_in_use;	/* To detect removed prefixes */

	/* Used when sending advertisements */
	struct confvar	pr_config[I_PREFIXSIZE];
#define	pr_AdvValidLifetime	pr_config[I_AdvValidLifetime].cf_value
#define	pr_AdvOnLinkFlag	pr_config[I_AdvOnLinkFlag].cf_value
#define	pr_AdvPreferredLifetime	pr_config[I_AdvPreferredLifetime].cf_value
#define	pr_AdvAutonomousFlag	pr_config[I_AdvAutonomousFlag].cf_value
#define	pr_AdvValidExpiration	pr_config[I_AdvValidExpiration].cf_value
#define	pr_AdvPreferredExpiration pr_config[I_AdvPreferredExpiration].cf_value
	/* The two below are set if the timers decrement in real time */
#define	pr_AdvValidRealTime	pr_config[I_AdvValidExpiration].cf_notdefault
#define	pr_AdvPreferredRealTime	\
			pr_config[I_AdvPreferredExpiration].cf_notdefault

	/* Recorded variables on node/host */
	uint_t		pr_ValidLifetime;	/* In ms w/ 2 hour rule */
	boolean_t	pr_OnLinkFlag;
	uint_t		pr_PreferredLifetime;	/* In millseconds */
	boolean_t	pr_AutonomousFlag;
	uint_t		pr_OnLinkLifetime;	/* Ms valid w/o 2 hour rule */
};

/*
 * Flags used for pr_kernel_state and pr_state where the latter is
 * user-level state.
 */
#define	PR_ONLINK	0x01		/* On-link */
#define	PR_AUTO		0x02		/* Stateless addrconf */
#define	PR_DEPRECATED	0x04		/* Address is deprecated */
#define	PR_STATIC	0x08		/* Not created by ndpd */


/*
 * Doubly-linked list of default routers on a phyint.
 */
struct router {
	struct router	*dr_next;	/* Next router for this physical */
	struct router	*dr_prev;	/* Prev router for this physical */
	struct phyint	*dr_physical;	/* Back pointer */

	struct in6_addr	dr_address;	/* Used to identify the router */
	uint_t		dr_lifetime;	/* In milliseconds */
	boolean_t	dr_inkernel;	/* Route added to kernel */
	boolean_t	dr_onlink;	/* Is this the onlink default route? */
};

/*
 * Globals
 */
extern struct phyint *phyints;


/*
 * Functions
 */
extern struct phyint	*phyint_lookup(char *name);
extern struct phyint	*phyint_lookup_on_index(uint_t ifindex);
extern struct phyint	*phyint_create(char *name);
extern int		phyint_init_from_k(struct phyint *pi);
extern void		phyint_delete(struct phyint *pi);
extern uint_t		phyint_timer(struct phyint *pi, uint_t elapsed);
extern void		phyint_print_all(void);
extern void		phyint_write_state_file(struct phyint *pi);
extern void		phyint_read_state_file(struct phyint *pi);
extern void		phyint_reach_random(struct phyint *pi,
			    boolean_t set_needed);

extern struct prefix	*prefix_lookup(struct phyint *pi, struct in6_addr addr,
			    int addrlen);
extern struct prefix	*prefix_create(struct phyint *pi, struct in6_addr addr,
			    int addrlen);
extern struct prefix	*prefix_lookup_name(struct phyint *pi, char *name);
extern struct prefix	*prefix_lookup_addr_match(struct prefix *pr);
extern struct prefix	*prefix_create_name(struct phyint *pi, char *name);
extern int		prefix_init_from_k(struct prefix *pr);
extern void		prefix_delete(struct prefix *pr);
extern void		prefix_update_k(struct prefix *pr);
extern uint_t		prefix_timer(struct prefix *pr, uint_t elapsed);

extern struct router	*router_lookup(struct phyint *pi, struct in6_addr addr);
extern struct router	*router_create(struct phyint *pi, struct in6_addr addr,
			    uint_t lifetime);
extern struct router	*router_create_onlink(struct phyint *pi);
extern void		router_update_k(struct router *dr);
extern uint_t		router_timer(struct router *dr, uint_t elapsed);


extern void	logperror_pi(struct phyint *pi, char *str);
extern void	logperror_pr(struct prefix *pr, char *str);
extern void	check_to_advertise(struct phyint *pi, enum adv_events event);
extern void	check_to_solicit(struct phyint *pi,
		    enum solicit_events event);
extern uint_t	advertise_event(struct phyint *pi, enum adv_events event,
		    uint_t elapsed);
extern uint_t	solicit_event(struct phyint *pi, enum solicit_events event,
		    uint_t elapsed);

extern void	print_route_sol(char *str, struct phyint *pi,
		    struct nd_router_solicit *rs, int len,
		    struct sockaddr_in6 *addr);
extern void	print_route_adv(char *str, struct phyint *pi,
		    struct nd_router_advert *ra, int len,
		    struct sockaddr_in6 *addr);
extern void	print_iflist(struct confvar *confvar);
extern void	print_prefixlist(struct confvar *confvar);

extern void	in_data(struct phyint *pi);

extern void	incoming_ra(struct phyint *pi, struct nd_router_advert *ra,
		    int len, struct sockaddr_in6 *from, boolean_t loopback);
