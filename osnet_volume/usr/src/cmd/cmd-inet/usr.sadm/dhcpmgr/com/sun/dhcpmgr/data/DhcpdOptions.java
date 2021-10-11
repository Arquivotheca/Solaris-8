/*
 * @(#)DhcpdOptions.java	1.7 99/10/21 SMI
 *
 * Copyright (c) 1998-1999 Sun Microsystems, Inc.
 * All rights reserved.
 */

package com.sun.dhcpmgr.data;

import java.util.ArrayList;
import java.util.Arrays;
import java.io.Serializable;

/**
 * DhcpdOptions models the option settings for the in.dhcpd daemon.  We read
 * and write the option settings in the daemon's defaults file.
 * Getter and setter methods are provided for each individual option.
 */

public class DhcpdOptions implements DhcpDefaults, Serializable {
    /* The list of facility choices the user may select from for logging */
    private static final Integer [] loggingFacilities = {
        new Integer(0), new Integer(1), new Integer(2), new Integer(3),
	new Integer(4), new Integer(5), new Integer(6), new Integer(7)
    };

    // Store the option settings here
    private ArrayList options;
    
    public DhcpdOptions() {
    	options = new ArrayList();
    }

    public DhcpdOptions(DhcpResource [] opts) {
    	options = new ArrayList(Arrays.asList(opts));
    }
    
    public Object clone() {
	DhcpdOptions o = new DhcpdOptions();
	o.options = (ArrayList)options.clone();
	return o;
    }
    
    // Test whether a particular option is set
    private boolean isSet(String key) {
	DhcpResource res = new DhcpResource();
	res.setKey(key);
    	return options.contains(res);
    }

    // Set an option to a supplied value
    private void set(String key, String value) {
    	set(key, value, false);
    }
    
    /**
     * Set an option to a supplied value, or add a comment line to the
     * table.
     * @param key The name of the option to set
     * @param value The value of the option
     * @param comment true if this is a comment, in which case value is ignored
     * and the comment text is contained entirely in key.
     */
    public void set(String key, String value, boolean comment) {
	DhcpResource res = new DhcpResource(key, value, comment);
    	int i = options.indexOf(res);
	if (i != -1) {
	    options.set(i, res);
	} else {
	    options.add(res);
	}
    }

    // Clear an option setting
    private void clear(String key) {
    	DhcpResource res = new DhcpResource();
	res.setKey(key);
	int i = options.indexOf(res);
	if (i != -1) {
	    options.remove(i);
	}
    }

    // Return the value of an option setting; null if it's not set
    private String valueOf(String key) {
    	DhcpResource res = new DhcpResource();
	res.setKey(key);
	int i = options.indexOf(res);
	if (i != -1) {
	    return ((DhcpResource)options.get(i)).getValue();
	} else {
	    return null;
	}
    }

    /**
     * Return all of the option settings as an array
     * @return An array of Objects which will all be DhcpResources
     */
    public Object [] getAll() {
    	return options.toArray();
    }

    /**
     * Set the resource (aka data store) in which DHCP data is stored.
     * This automatically also sets the run mode to server.
     * @param s Unique name of resource
     */
    public void setResource(String s) {
	set(RUN_MODE, SERVER);
    	set(RESOURCE, s);
    }

    /**
     * Retrieve the name of the resource/data store used for DHCP.
     * @return The unique name of the resource
     */
    public String getResource() {
    	return valueOf(RESOURCE);
    }

    /**
     * Set the path within the resource in which to place the tables.
     * For files, this is a Unix pathname; for NIS+, the directory name.
     * @param s The path
     */
    public void setPath(String s) {
	set(RUN_MODE, SERVER);
    	set(PATH, s);
    }

    /**
     * Get the path used for data storage.
     * @return The path within the resource
     */
    public String getPath() {
    	return valueOf(PATH);
    }

    /**
     * Test whether BOOTP compatibility is enabled.
     * @return true if BOOTP compatibility is enabled.
     */
    public boolean isBootpCompatible() {
	return isSet(BOOTP_COMPAT);
    }
    
    /**
     * Enable or disable BOOTP compatibility.
     * @param state true if BOOTP compatibility is enabled, false if not.
     * @param isAutomatic true if automatic allocation is allowed.
     */
    public void setBootpCompatible(boolean state, boolean isAutomatic) {
	if (state) {
	    if (isAutomatic) {
		set(BOOTP_COMPAT, AUTOMATIC);
	    } else {
		set(BOOTP_COMPAT, MANUAL);
	    }
	} else {
	    clear(BOOTP_COMPAT);
	}
    }
    
    /**
     * Test whether BOOTP compatibility is automatic or manual
     * @return true if BOOTP compatibility is automatic.
     */
    public boolean isBootpAutomatic() {
	return AUTOMATIC.equals(valueOf(BOOTP_COMPAT));
    }
    
    /**
     * Test whether relay hop limit is set.
     * @return true if the limit is set, false if default value is used.
     */
    public boolean isRelayHops() {
	return isSet(RELAY_HOPS);
    }
    
    /**
     * Set the relay hop limit.
     * @param state true if hop limit should be set, false if not
     * @param hops Number of hops to limit forwarding to
     */
    public void setRelayHops(boolean state, Integer hops) {
	if (state) {
	    set(RELAY_HOPS, hops.toString());
	} else {
	    clear(RELAY_HOPS);
	}
    }
    
    /**
     * Get the relay hop limit.
     * @return The number of hops currently set, or null if this isn't set.
     */
    public Integer getRelayHops() {
	String hops = valueOf(RELAY_HOPS);
	if (hops != null) {
	    return new Integer(hops);
	} else {
	    return null;
	}
    }
    
    /**
     * Test whether a network interface list is set; failure to set an interface
     * list implies that all interfaces will be monitored.
     * @return true if an interface list is set
     */
    public boolean isInterfaces() {
	return isSet(INTERFACES);
    }
    
    /**
     * Set the network interface list.
     * @param state true if interface list is to be set, false if it should be
     * cleared
     * @param list A comma-separated list of interface names
     */
    public void setInterfaces(boolean state, String list) {
	if (state) {
	    set(INTERFACES, list);
	} else {
	    clear(INTERFACES);
	}
    }
    
    /**
     * Get the list of network interfaces.
     * @return The comma-separated list of interfaces
     */
    public String getInterfaces() {
	return valueOf(INTERFACES);
    }
    
    /**
     * Test whether ICMP address verification is enabled
     * @return true if ICMP verification is performed
     */
    public boolean isICMPVerify() {
	/*
	 * Use this double-inverse comparison so that the default behavior of
	 * ICMP enabled is handled correctly.
	 */
	return !DEFAULT_FALSE.equals(valueOf(ICMP_VERIFY));
    }
    
    /**
     * Set ICMP verification 
     * @param state true if verification should be done, false otherwise
     */
    public void setICMPVerify(boolean state) {
	set(ICMP_VERIFY, state ? DEFAULT_TRUE : DEFAULT_FALSE);
    }
    
    /**
     * Test whether offer cache timeout is set
     * @return true if it is set
     */
    public boolean isOfferTtl() {
	return isSet(OFFER_CACHE_TIMEOUT);
    }
    
    /**
     * Set offer cache timeout value
     * @param state true if offer cache timeout value is set, false if server's
     * default will be used instead
     * @param time Number of seconds to hold offers in the cache
     */
    public void setOfferTtl(boolean state, Integer time) {
	if (state) {
	    set(OFFER_CACHE_TIMEOUT, time.toString());
	} else {
	    clear(OFFER_CACHE_TIMEOUT);
	}
    }
    
    /**
     * Get the offer cache timeout value
     * @return timeout value set, or null if server default is used
     */
    public Integer getOfferTtl() {
	String s = valueOf(OFFER_CACHE_TIMEOUT);
	if (s != null) {
	    return new Integer(s);
	} else {
	    return null;
	}
    }
    
    /**
     * Test whether server is running in relay mode
     * @return true if running as relay
     */
    public boolean isRelay() {
	return RELAY.equals(valueOf(RUN_MODE));
    }
    
    /**
     * Set relay mode and server list
     * @param state true if relay mode is desired, false for normal server
     * @param servers list of servers to which requests should be forwarded
     */
    public void setRelay(boolean state, String servers) {
	if (state) {
	    set(RUN_MODE, RELAY);
	    set(RELAY_DESTINATIONS, servers);
	} else {
	    set(RUN_MODE, SERVER);
	    clear(RELAY_DESTINATIONS);
	}
    }
    
    /**
     * Get list of server targets for relay
     * @return list of relay targets
     */
    public String getRelay() {
	return valueOf(RELAY_DESTINATIONS);
    }
    
    /**
     * Test for server automatic reload of dhcptab
     * @return true if server is rescanning dhcptab
     */
    public boolean isRescan() {
	return isSet(RESCAN_INTERVAL);
    }
    
    /**
     * Set the rescan interval
     * @param state true if rescanning is enabled, false if not
     * @param interval number of minutes between rescans
     */
    public void setRescan(boolean state, Integer interval) {
	if (state) {
	    set(RESCAN_INTERVAL, interval.toString());
	} else {
	    clear(RESCAN_INTERVAL);
	}
    }
    
    /**
     * Get the rescan interval
     * @return the rescan interval in minutes, or null if rescan is not enabled
     */
    public Integer getRescan() {
	String s = valueOf(RESCAN_INTERVAL);
	if (s != null) {
	    return new Integer(s);
	} else {
	    return null;
	}
    }
    
    /**
     * Test for verbose logging mode
     * @return true if verbose logging, false for normal
     */
    public boolean isVerbose() {
	return DEFAULT_TRUE.equals(valueOf(VERBOSE));
    }
    
    /**
     * Set verbose logging mode
     * @param state true for verbose, false for normal
     */
    public void setVerbose(boolean state) {
	set(VERBOSE, state ? DEFAULT_TRUE : DEFAULT_FALSE);
    }


    /**
     * Test for transaction logging mode.
     * @return true if transaction logging is enabled
     */
    public boolean isLogging() {
    	return isSet(LOGGING_FACILITY);
    }

    /**
     * Get the syslog facility number used for transaction logging
     * @return facility number, which will be between 0 and 7
     */
    public Integer getLogging() {
	String s = valueOf(LOGGING_FACILITY);
    	if (s != null) {
	    return new Integer(s);
	} else {
	    return null;
	}
    }

    /**
     * Set transaction logging
     * @param state true to enable transaction logging, false to disable
     * @param value syslog facility number 0-7 used for logging
     */
    public void setLogging(boolean state, Integer value) {
        if (state) {
	    set(LOGGING_FACILITY, value.toString());
	} else {
	    clear(LOGGING_FACILITY);
	}
    }

    /**
     * Get the list of logging facility choices
     * @return an array of facility numbers
     */
    public static Integer [] getLoggingFacilities() {
    	return loggingFacilities;
    }

    /**
     * Convert this object to a String representation
     */
    public String toString() {
	StringBuffer b = new StringBuffer();
	for (int i = 0; i < options.size(); ++i) {
	    DhcpResource res = (DhcpResource)options.get(i);
	    b.append(res.getKey());
	    String s = res.getValue();
	    if (s != null) {
	    	b.append('=');
	    	b.append(s);
	    }
	    b.append('\n');
	}
	return b.toString();
    }
}
