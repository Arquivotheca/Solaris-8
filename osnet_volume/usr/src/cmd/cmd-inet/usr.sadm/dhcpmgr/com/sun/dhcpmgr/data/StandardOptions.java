/*
 * @(#)StandardOptions.java	1.3	99/07/30 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.data;

import java.util.*;
import java.io.Serializable;

/**
 * This class defines the set of standard DHCP options we know about.
 */

/*
 * StdOption is just a convenience class for our use so we can ignore the
 * context parameter.
 */
class StdOption extends Option {
    StdOption(String name, short code, byte type, int gran, int max) {
	try {
	    setKey(name);
	    setContext(STANDARD);
	    setCode(code);
	    setType(type);
	    setGranularity(gran);
	    setMaximum(max);
	} catch (ValidationException e) {
	// This shouldn't happen!
	}
    }
}

public class StandardOptions implements Serializable {

    /*
     * Following list of codes must be kept in sync with the list in
     * usr/src/uts/common/netinet/dhcp.h in SunOS source tree.
     */
    public static final short CD_SUBNETMASK = 1;
    public static final short CD_TIMEOFFSET = 2;
    public static final short CD_ROUTER = 3;
    public static final short CD_TIMESERV = 4;
    public static final short CD_IEN116_NAME_SERV = 5;
    public static final short CD_DNSSERV = 6;
    public static final short CD_LOG_SERV = 7;
    public static final short CD_COOKIE_SERV = 8;
    public static final short CD_LPR_SERV = 9;
    public static final short CD_IMPRESS_SERV = 10;
    public static final short CD_RESOURCE_SERV = 11;
    public static final short CD_HOSTNAME = 12;
    public static final short CD_BOOT_SIZE = 13;
    public static final short CD_DUMP_FILE = 14;
    public static final short CD_DNSDOMAIN = 15;
    public static final short CD_SWAP_SERV = 16;
    public static final short CD_ROOT_PATH = 17;
    public static final short CD_EXTEND_PATH = 18;
    
    /* IP layer parameters */
    public static final short CD_IP_FORWARDING_ON = 19;
    public static final short CD_NON_LCL_ROUTE_ON = 20;
    public static final short CD_POLICY_FILTER = 21;
    public static final short CD_MAXIPSIZE = 22;
    public static final short CD_IPTTL = 23;
    public static final short CD_PATH_MTU_TIMEOUT = 24;
    public static final short CD_PATH_MTU_TABLE_SZ = 25;
    
    /* IP layer parameters per interface */
    public static final short CD_MTU = 26;
    public static final short CD_ALL_SUBNETS_LCL_ON = 27;
    public static final short CD_BROADCASTADDR = 28;
    public static final short CD_MASK_DISCVRY_ON = 29;
    public static final short CD_MASK_SUPPLIER_ON = 30;
    public static final short CD_ROUTER_DISCVRY_ON = 31;
    public static final short CD_ROUTER_SOLICIT_SERV = 32;
    public static final short CD_STATIC_ROUTE = 33;
    
    /* Link Layer Parameters per Interface */
    public static final short CD_TRAILER_ENCAPS_ON = 34;
    public static final short CD_ARP_TIMEOUT = 35;
    public static final short CD_ETHERNET_ENCAPS_ON = 36;
    
    /* TCP Parameters */
    public static final short CD_TCP_TTL = 37;
    public static final short CD_TCP_KALIVE_INTVL = 38;
    public static final short CD_TCP_KALIVE_GRBG_ON = 39;
    
    /* Application layer parameters */
    public static final short CD_NIS_DOMAIN = 40;
    public static final short CD_NIS_SERV = 41;
    public static final short CD_NTP_SERV = 42;
    public static final short CD_VENDOR_SPEC = 43;
    
    /* NetBIOS parameters */
    public static final short CD_NETBIOS_NAME_SERV = 44;
    public static final short CD_NETBIOS_DIST_SERV = 45;
    public static final short CD_NETBIOS_NODE_TYPE = 46;
    public static final short CD_NETBIOS_SCOPE = 47;
    
    /* X Window parameters */
    public static final short CD_XWIN_FONT_SERV = 48;
    public static final short CD_XWIN_DISP_SERV = 49;
    
    /* DHCP protocol extension options */
    public static final short CD_REQUESTED_IP_ADDR = 50;
    public static final short CD_LEASE_TIME = 51;
    public static final short CD_OPTION_OVERLOAD = 52;
    public static final short CD_DHCP_TYPE = 53;
    public static final short CD_SERVER_ID = 54;
    public static final short CD_REQUEST_LIST = 55;
    public static final short CD_MESSAGE = 56;
    public static final short CD_MAX_DHCP_SIZE = 57;
    public static final short CD_T1_TIME = 58;
    public static final short CD_T2_TIME = 59;
    public static final short CD_CLASS_ID = 60;
    public static final short CD_CLIENT_ID = 61;
    
    /* Netware options */
    public static final short CD_NW_IP_DOMAIN = 62;
    public static final short CD_NW_IP_OPTIONS = 63;
    
    /* Nisplus options */
    public static final short CD_NISPLUS_DMAIN = 64;
    public static final short CD_NISPLUS_SERVS = 65;
    
    /* Optional sname/bootfile options */
    public static final short CD_TFTP_SERV_NAME = 66;
    public static final short CD_OPT_BOOTFILE_NAME = 67;
    
    /* Additional server options */
    public static final short CD_MOBILE_IP_AGENT = 68;
    public static final short CD_SMTP_SERVS = 69;
    public static final short CD_POP3_SERVS = 70;
    public static final short CD_NNTP_SERVS = 71;
    public static final short CD_WWW_SERVS = 72;
    public static final short CD_FINGER_SERVS = 73;
    public static final short CD_IRC_SERVS = 74;
    
    /* Streettalk options */
    public static final short CD_STREETTALK_SERVS = 75;
    public static final short CD_STREETTALK_DA_SERVS = 76;
    
    /* Specials */
    public static final short CD_SIADDR = 257;
    public static final short CD_SNAME = 258;
    public static final short CD_BOOTFILE = 260;
    public static final short CD_BOOTPATH = 261;
    public static final short CD_BOOL_HOSTNAME = 1024;
    public static final short CD_BOOL_LEASENEG = 1025;
    public static final short CD_BOOL_ECHOVC = 1026;

    /*
     * Following list of options must be kept in sync with the list in
     * usr/src/cmd/cmd-inet/usr.lib/in.dhcpd/dhcptab.c in SunOS source tree.
     */
    private static Option [] options = {
	new StdOption("Subnet", CD_SUBNETMASK, Option.IP, 1, 1),
	new StdOption("UTCoffst", CD_TIMEOFFSET, Option.NUMBER, 4, 1),
	new StdOption("Router", CD_ROUTER, Option.IP, 1, 0),
	new StdOption("Timeserv", CD_TIMESERV, Option.IP, 1, 0),
	new StdOption("IEN116ns", CD_IEN116_NAME_SERV, Option.IP, 1, 0),
	new StdOption("DNSserv", CD_DNSSERV, Option.IP, 1, 0),
	new StdOption("Logserv", CD_LOG_SERV, Option.IP, 1, 0),
	new StdOption("Cookie", CD_COOKIE_SERV, Option.IP, 1, 0),
	new StdOption("Lprserv", CD_LPR_SERV, Option.IP, 1, 0),
	new StdOption("Impress", CD_IMPRESS_SERV, Option.IP, 1, 0),
	new StdOption("Resource", CD_RESOURCE_SERV, Option.IP, 1, 0),
	new StdOption("Hostname", CD_BOOL_HOSTNAME, Option.BOOLEAN, 0, 0),
	new StdOption("Bootsize", CD_BOOT_SIZE, Option.NUMBER, 2, 1),
	new StdOption("Dumpfile", CD_DUMP_FILE, Option.ASCII, 0, 0),
	new StdOption("DNSdmain", CD_DNSDOMAIN, Option.ASCII, 0, 0),
	new StdOption("Swapserv", CD_SWAP_SERV, Option.IP, 1, 1),
	new StdOption("Rootpath", CD_ROOT_PATH, Option.ASCII, 0, 0),
	new StdOption("ExtendP", CD_EXTEND_PATH, Option.ASCII, 0, 0),
	new StdOption("IpFwdF", CD_IP_FORWARDING_ON, Option.NUMBER, 1, 1),
	new StdOption("NLrouteF", CD_NON_LCL_ROUTE_ON, Option.NUMBER, 1, 1),
	new StdOption("PFilter", CD_POLICY_FILTER, Option.IP, 2, 0),
	new StdOption("MaxIpSiz", CD_MAXIPSIZE, Option.NUMBER, 2, 1),
	new StdOption("IpTTL", CD_IPTTL, Option.NUMBER, 1, 1),
	new StdOption("PathTO", CD_PATH_MTU_TIMEOUT, Option.NUMBER, 4, 1),
	new StdOption("PathTbl", CD_PATH_MTU_TABLE_SZ, Option.NUMBER, 2, 0),
	new StdOption("MTU", CD_MTU, Option.NUMBER, 2, 1),
	new StdOption("SameMtuF", CD_ALL_SUBNETS_LCL_ON, Option.NUMBER, 1, 1),
	new StdOption("Broadcst", CD_BROADCASTADDR, Option.IP, 1, 1),
	new StdOption("MaskDscF", CD_MASK_DISCVRY_ON, Option.NUMBER, 1, 1),
	new StdOption("MaskSupF", CD_MASK_SUPPLIER_ON, Option.NUMBER, 1, 1),
	new StdOption("RDiscvyF", CD_ROUTER_DISCVRY_ON, Option.NUMBER,	1, 1),
	new StdOption("RSolictS", CD_ROUTER_SOLICIT_SERV, Option.IP, 1, 1),
	new StdOption("StaticRt", CD_STATIC_ROUTE, Option.IP, 2, 0),
	new StdOption("TrailerF", CD_TRAILER_ENCAPS_ON, Option.NUMBER,	1, 1),
	new StdOption("ArpTimeO", CD_ARP_TIMEOUT, Option.NUMBER, 4, 1),
	new StdOption("EthEncap", CD_ETHERNET_ENCAPS_ON, Option.NUMBER, 1, 1),
	new StdOption("TcpTTL",	CD_TCP_TTL, Option.NUMBER, 1, 1),
	new StdOption("TcpKaInt", CD_TCP_KALIVE_INTVL, Option.NUMBER, 4, 1),
	new StdOption("TcpKaGbF", CD_TCP_KALIVE_GRBG_ON, Option.NUMBER, 1, 1),
	new StdOption("NISdmain", CD_NIS_DOMAIN, Option.ASCII, 0, 0),
	new StdOption("NISservs", CD_NIS_SERV, Option.IP, 1, 0),
	new StdOption("NTPservs", CD_NTP_SERV, Option.IP, 1, 0),
	new StdOption("NetBNms", CD_NETBIOS_NAME_SERV, Option.IP, 1, 0),
	new StdOption("NetBDsts", CD_NETBIOS_DIST_SERV, Option.IP, 1, 0),
	new StdOption("NetBNdT", CD_NETBIOS_NODE_TYPE, Option.NUMBER, 1, 1),
	new StdOption("NetBScop", CD_NETBIOS_SCOPE, Option.ASCII, 0, 0),
	new StdOption("XFontSrv", CD_XWIN_FONT_SERV, Option.IP, 1, 0),
	new StdOption("XDispMgr", CD_XWIN_DISP_SERV, Option.IP, 1, 0),
	new StdOption("LeaseTim", CD_LEASE_TIME, Option.NUMBER, 4, 1),
	new StdOption("Message", CD_MESSAGE, Option.ASCII, 1, 0),
	new StdOption("T1Time",	CD_T1_TIME, Option.NUMBER, 4, 1),
	new StdOption("T2Time",	CD_T2_TIME, Option.NUMBER, 4, 1),
	new StdOption("NW_dmain", CD_NW_IP_DOMAIN, Option.ASCII, 0, 0),
	new StdOption("NWIPOpts", CD_NW_IP_OPTIONS, Option.OCTET, 1, 128),
	new StdOption("NIS+dom", CD_NISPLUS_DMAIN, Option.ASCII, 0, 0),
	new StdOption("NIS+serv", CD_NISPLUS_SERVS, Option.IP,	1, 0),
	new StdOption("TFTPsrvN", CD_TFTP_SERV_NAME, Option.ASCII, 0, 64),
	new StdOption("OptBootF", CD_OPT_BOOTFILE_NAME, Option.ASCII, 0, 128),
	new StdOption("MblIPAgt", CD_MOBILE_IP_AGENT, Option.IP, 1, 0),
	new StdOption("SMTPserv", CD_SMTP_SERVS, Option.IP, 1, 0),
	new StdOption("POP3serv", CD_POP3_SERVS, Option.IP, 1, 0),
	new StdOption("NNTPserv", CD_NNTP_SERVS, Option.IP, 1, 0),
	new StdOption("WWWservs", CD_WWW_SERVS, Option.IP, 1, 0),
	new StdOption("Fingersv", CD_FINGER_SERVS, Option.IP, 1, 0),
	new StdOption("IRCservs", CD_IRC_SERVS, Option.IP, 1, 0),
	new StdOption("STservs", CD_STREETTALK_SERVS, Option.IP, 1, 0),
	new StdOption("STDAserv", CD_STREETTALK_DA_SERVS, Option.IP, 1, 0),
	new StdOption("BootFile", CD_BOOTFILE, Option.ASCII, 0, 128),
	new StdOption("BootSrvA", CD_SIADDR, Option.IP, 1, 1),
	new StdOption("BootSrvN", CD_SNAME, Option.ASCII, 0, 64),
	new StdOption("LeaseNeg", CD_BOOL_LEASENEG, Option.BOOLEAN, 0, 0),
	new StdOption("EchoVC", CD_BOOL_ECHOVC, Option.BOOLEAN, 0, 0),
	new StdOption("BootPath", CD_BOOTPATH, Option.ASCII, 0, 128)
    };
    
    /**
     * Return the size of this list
     * @return the number of options known
     */
    public int size() {
	return options.length;
    }
    
    /**
     * Enumerate the options defined here.
     * @return an Enumeration of the standard options.
     */
    public Enumeration enumOptions() {
	return new Enumeration() {
	    int cursor = 0;
	    
	    public boolean hasMoreElements() {
		return (cursor < options.length);
	    }
	    
	    public Object nextElement() throws NoSuchElementException {
		if (cursor >= options.length) {
		    throw new NoSuchElementException();
		}
		return (options[cursor++]);
	    }
	};
    }
    
    /**
     * Return all options as an array
     * @return the array of options defined here
     */
    public static Option [] getAllOptions() {
	return options;
    }
    
    /**
     * Find the option name for a given code.  This could be
     * much faster but not clear that it needs to be yet.
     * @return the name of the option, or null if that code is unknown.
     */ 
    public static String nameForCode(int code) {
	for (int i = 0; i < options.length; ++i) {
	    if (options[i].getCode() == code) {
		return options[i].getKey();
	    }
	}
	return null;
    }
}
