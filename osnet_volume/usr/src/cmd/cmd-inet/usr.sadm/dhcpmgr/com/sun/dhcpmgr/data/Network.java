/*
 * @(#)Network.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.data;

import java.io.Serializable;
import java.util.StringTokenizer;

/**
 * A representation of an IP network from DHCP's point of view; we're
 * primarily interested in the address and subnet mask.
 */
public class Network implements Serializable {
    private IPAddress address;
    private IPAddress netmask;
    
    /**
     * Construct an empty network object
     */
    public Network() {
	address = new IPAddress();
	netmask = new IPAddress();
    }
    
    /**
     * Construct a network with the supplied address.
     * @param addr The IP address of the network.
     */
    public Network(String addr) throws ValidationException {
	address = new IPAddress(addr);
	// Initialize a default netmask based on address class
	byte [] b = address.getAddress();
	int msb = (int)b[0] & 0xff;
	if (msb < 128) {
	    netmask = new IPAddress("255.0.0.0");
	} else if (msb < 192) {
	    netmask = new IPAddress("255.255.0.0");
	} else {
	    netmask = new IPAddress("255.255.255.0");
	}
    }
    
    /**
     * Construct a network with the supplied address and subnet mask
     * @param addr The IP address of the network as a <code>String</code>
     * @param mask The subnet mask as an <code>int</code>
     */
    public Network(String addr, int mask) throws ValidationException {
	address = new IPAddress(addr);
	netmask = new IPAddress(mask);
    }
    
    /**
     * Construct a network with the supplied address and subnet mask.
     * @param addr The IP address as an <code>IPAddress</code>
     * @param mask The subnet mask as an <code>IPAddress</code>
     */
    public Network(IPAddress addr, IPAddress mask) {
	address = addr;
	netmask = mask;
    }
    
    /**
     * Construct a network with the supplied address and subnet mask.
     * @param addr The IP address as a dotted decimal <code>String</code>
     * @param mask The subnet mask as a dotted decimal <code>String</code>
     */
    public Network(String addr, String mask) throws ValidationException {
	address = new IPAddress(addr);
	netmask = new IPAddress(mask);
    }
    
    /**
     * @return The IP address of the network
     */
    public IPAddress getAddress() {
	return address;
    }
    
    /**
     * Return the actual network number, which is the product of applying
     * the subnet mask to the address supplied.
     * @return The network number as an <code>IPAddress</code>
     */
    public IPAddress getNetworkNumber() {
	// If netmask is not set then ignore it and return address raw
	if (netmask.intValue() == 0) {
	    return address;
	} else {
	    return new IPAddress(address.intValue() & netmask.intValue());
	}
    }

    /**
     * @return The subnet mask of the network
     */
    public IPAddress getMask() {
	return netmask;
    }
    
    /**
     * Set the subnet mask.
     * @param mask The subnet mask.
     */
    public void setMask(IPAddress mask) {
	netmask = mask;
    }

    /**
     * Do the math to evaluate whether an address is part of this network.
     * @param addr The IP address to evaluate
     * @return <code>true</code> if the address is on this network,
     * <code>false</code> if not.
     */
    public boolean containsAddress(IPAddress addr) {
	return ((addr.intValue() & netmask.intValue())
	    == (address.intValue() & netmask.intValue()));
    }
    
    /**
     * Compare against another network object for equality.
     * @param obj The network to compare against.
     * @return <code>true</code> if the networks have the same network number
     */
    public boolean equals(Object obj) {
	// If object passed isn't of same type, always false.
	if (!(obj instanceof Network)) {
	    return false;
	}
	return getNetworkNumber().equals(((Network)obj).getNetworkNumber());
    }
    
    public String toString() {
	return getNetworkNumber().toString();
    }
}
