/*
 * @(#)DhcpClientRecord.java	1.5	99/05/07 SMI
 *
 * Copyright (c) 1998 Sun Microsystems, Inc.
 * All rights reserved.
 */

package com.sun.dhcpmgr.data;

import java.util.Date;
import java.io.Serializable;

/**
 * This class represents a record in a DHCP network table.  It can also be used
 * to manage an associated hosts record by setting the client name; that effect
 * is not part of this class, but rather is provided by the DhcpNetMgr.
 */
public class DhcpClientRecord implements Serializable, Comparable, Cloneable {
    /**
     * Flag for BOOTP addresses
     */
    public static final byte BOOTP = 8;
    /**
     * Flag for unusable addresses.
     */
    public static final byte UNUSABLE = 4;
    /**
     * Flag for manually assigned addresses.
     */
    public static final byte MANUAL = 2;
    /**
     * Flag for permanently assigned addresses.
     */
    public static final byte PERMANENT = 1;
    
    private String clientId;
    private byte flags;
    private IPAddress clientIP;
    private IPAddress serverIP;
    private Date expiration;
    private String macro;
    private String comment;
    private String clientName;
    private String serverName;
    public static final String DEFAULT_CLIENT_ID = "00";
    
    /**
     * Constructs a basic, empty client record.
     */
    public DhcpClientRecord() {
	clientId = DEFAULT_CLIENT_ID;
	macro = comment = null;
	flags = 0;
	clientIP = serverIP = null;
	expiration = null;
    }
    
    /**
     * Constructs a fully specified client record
     * @param clientId Client's unique identifier
     * @param flags Status flags for the record
     * @param clientIP Client's IP address
     * @param serverIP IP address of owning server
     * @param expiration Lease expiration time in seconds since Unix epoch
     * @param macro Configuration macro associated with this record
     * @param comment User notes on this record
     */
    public DhcpClientRecord(String clientId, String flags, String clientIP,
			    String serverIP, String expiration, String macro,
			    String comment) throws ValidationException {
	setClientId(clientId);
	this.flags = Byte.parseByte(flags);
	setClientIP(new IPAddress(clientIP));
	setServerIP(new IPAddress(serverIP));
	// Convert Unix time in seconds to a Date, need milliseconds
	this.expiration = new Date((long)(Long.parseLong(expiration)*1000));
	this.macro = macro;
	this.comment = comment;
    }
    
    /**
     * Make a copy of this record
     */
    public Object clone() {
	DhcpClientRecord newrec = new DhcpClientRecord();
	newrec.clientId = clientId;
	newrec.flags = flags;
	if (clientIP != null) {
		newrec.clientIP = (IPAddress)clientIP.clone();
	}
	if (serverIP != null) {
		newrec.serverIP = (IPAddress)serverIP.clone();
	}
	if (expiration != null) {
		newrec.expiration = (Date)expiration.clone();
	}
	newrec.macro = macro;
	newrec.comment = comment;
	newrec.clientName = clientName;
	newrec.serverName = serverName;
	return newrec;
    }
    
    /**
     * Retrieve the client ID
     * @return Client ID as a String
     */
    public String getClientId() {
	return clientId;
    }
    
    /**
     * Set the client ID.  See dhcp_network(4) for the rules about client
     * ID syntax which are implemented here.
     * @param clientId Client's unique identifier
     */
    public void setClientId(String clientId) throws ValidationException {
	if (clientId.length() > 64 || clientId.length() % 2 != 0) {
	    // Must be even number of characters, no more than 64 characters
	    throw new ValidationException();
	}
	char [] c = clientId.toCharArray();
	for (int i = 0; i < c.length; ++i) {
	    if ((c[i] < '0' || c[i] > '9') && (c[i] < 'A' || c[i] > 'F')) {
		throw new ValidationException();
	    }
	}
	this.clientId = clientId;
	if (this.clientId.length() == 0) {
	    this.clientId = DEFAULT_CLIENT_ID;
	}
    }
    
    /**
     * Get the flags byte
     * @return A <code>byte</code> containing the record's status flags
     */
    public byte getFlags() {
	return flags;
    }
    
    /**
     * Get the flags as a string
     * @return The flag byte converted to a String
     */
    public String getFlagString() {
    	StringBuffer b = new StringBuffer();
	b.append(flags);
	// Make sure we always have a 2-character representation.
	if (flags < 10) {
	    b.insert(0, 0);
	}
	return b.toString();
    }
    
    /**
     * Test for setting of unusable flag
     * @return <code>true</code> if the unusable flag is set, 
     * <code>false</code> if not.
     */
    public boolean isUnusable() {
	return ((flags & UNUSABLE) == UNUSABLE);
    }
    
    /**
     * Set/reset the unusable flag.
     * @param state <code>true</code> if address is to be unusable
     */
    public void setUnusable(boolean state) {
	if (state) {
	    flags |= UNUSABLE;
	} else {
	    flags &= ~UNUSABLE;
	}
    }
    
    /**
     * Test for setting of bootp flag
     * @return <code>true</code> if the bootp flag is set,
     * <code>false</code> if not.
     */
    public boolean isBootp() {
	return ((flags & BOOTP) == BOOTP);
    }
    
    /**
     * Set/reset the bootp flag
     * @param state <code>true</code> if address is reserved for BOOTP clients
     */
    public void setBootp(boolean state) {
	if (state) {
	    flags |= BOOTP;
	} else {
	    flags &= ~BOOTP;
	}
    }
    
    /**
     * Test for setting of manual assignment flag
     * @return <code>true</code> if address is manually assigned,
     * <code>false</code> if not.
     */
    public boolean isManual() {
	return ((flags & MANUAL) == MANUAL);
    }
    
    /**
     * Set/reset the manual assignment flag
     * @param state <code>true</code> if the address is manually assigned
     */
    public void setManual(boolean state) {
	if (state) {
	    flags |= MANUAL;
	} else {
	    flags &= ~MANUAL;
	}
    }
    
    /**
     * Test for setting of permanent assignment flag
     * @return <code>true</code> if lease is permanent,
     * <code>false</code> if dynamic
     */
    public boolean isPermanent() {
	return ((flags & PERMANENT) == PERMANENT);
    }
    
    /**
     * Set/reset the permanent assignment flag
     * @param state <code>true</code> if the address is permanently leased
     */
    public void setPermanent(boolean state) {
	if (state) {
	    flags |= PERMANENT;
	} else {
	    flags &= ~PERMANENT;
	}
    }
    
    /**
     * Set the flags as a unit
     * @param flags a <code>byte</code> setting for the flags
     */
    public void setFlags(byte flags) {
	this.flags = flags;
    }
    
    /**
     * Retrieve the client's IP address
     * @return the client's IP address
     */
    public IPAddress getClientIP() {
	return clientIP;
    }
    
    /**
     * Retrieve a string version of the client's IP address
     * @return A <code>String</code> containing the dotted decimal IP address.
     */
    public String getClientIPAddress() {
	if (clientIP == null) {
	    return "";
	} else {
	    return clientIP.getHostAddress();
	}
    }
    
    /**
     * Set the client's IP address
     * @param clientIP An <code>IPAddress</code> to assign from this record.
     */
    public void setClientIP(IPAddress clientIP) throws ValidationException {
	if (clientIP == null) {
	    throw new ValidationException();
	}
	this.clientIP = clientIP;
	clientName = clientIP.getHostName();
    }
    
    /**
     * Retrieve the IP address of the owning server.
     * @return An <code>IPAddress</code> for the server controlling this record.
     */
    public IPAddress getServerIP() {
	return serverIP;
    }
    
    /**
     * Retrieve a string version of the owning server's IP address
     * @return The server's dotted decimal IP address as a <code>String</code>
     */
    public String getServerIPAddress() {
	if (serverIP == null) {
	    return "";
	} else {
	    return serverIP.getHostAddress();
	}
    }
    
    /**
     * Assign this address to a server denoted by its IP address
     * @param serverIP The <code>IPAddress</code> of the owning server.
     */
    public void setServerIP(IPAddress serverIP) throws ValidationException {
	if (serverIP == null) {
	    throw new ValidationException();
	}
	this.serverIP = serverIP;
	serverName = serverIP.getHostName();
    }
    
    /**
     * @return The expiration time of this record's lease as a <code>Date</code>
     */
    public Date getExpiration() {
	return expiration;
    }
    
    /**
     * @return The expiration time of this record's lease in seconds
     * since the epoch, as a <code>String</code>
     */
    public String getExpirationTime() {
	return String.valueOf((expiration.getTime()/(long)1000));
    }
    
    /**
     * Set the lease expiration date.
     * @param expiration The <code>Date</code> when the lease expires.
     */
    public void setExpiration(Date expiration) {
	this.expiration = expiration;
    }
    
    /**
     * @return The name of the macro used to explicitly configure this address
     */
    public String getMacro() {
	return macro;
    }
    
    /**
     * Set the name of the macro used to explicitly configure this address
     */
    public void setMacro(String macro) {
	this.macro = macro;
    }
    
    /**
     * @return The descriptive comment for this record
     */
    public String getComment() {
	return comment;
    }
    
    /**
     * Set a descriptive comment for this record
     * @param comment The comment
     */
    public void setComment(String comment) {
	this.comment = comment;
    }
    
    /**
     * Perform comparisons to another DhcpClientRecord instance.  This is used
     * for sorting a network table by client address.
     * @param o A <code>DhcpClientRecord</code> to compare against.
     * @return 0 if the objects have the same address,
     * a negative number if this record has a lower IP address than the
     * supplied record, a positive number if this record has a higher IP 
     * address than the supplied record.
     */
    public int compareTo(Object o) {
	DhcpClientRecord r = (DhcpClientRecord)o;
	return (int)(getBinaryAddress() - r.getBinaryAddress());
    }
    
    /**
     * Retrieve the IP address as a number suitable for arithmetic operations.
     * We use a <code>long</code> rather than an <code>int</code> in order to 
     * be able to treat it as an unsigned value, since all Java types are
     * signed.
     * @return The IP address as a <code>long</code>.
     */
    public long getBinaryAddress() {
	byte [] bytes = clientIP.getAddress();
	long result = 0;
	for (int i = 0; i < bytes.length; ++i) {
	    // Defeat sign extension with cast & mask
	    result |= ((long)bytes[i] & 0xff) << (24-(i*8));
	}
	return result;
    }
    
    /**
     * @return The client's hostname
     */
    public String getClientName() {
	return clientName;
    }

    /**
     * @param name The hostname for the client.
     */
    public void setClientName(String name) {
	clientName = name;
    }
    
    /**
     * @return The server's hostname
     */
    public String getServerName() {
	return serverName;
    }
    
    /**
     * @param name The server's hostname
     */
    public void setServerName(String name) {
	serverName = name;
    }
    
    public String toString() {
	String s = clientId + " " + String.valueOf(flags) + " "
		+ clientIP.getHostAddress() + " " + serverIP.getHostAddress() 
		+ " " + expiration.toString() + " " + macro + " " + comment;
	return s;
    }
}
