/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_EntryInfo.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.amiserv;

import java.io.*;
import java.math.BigInteger;
import sun.security.util.DerOutputStream;
import sun.security.util.DerInputStream;
import sun.security.util.BigInt;


/**
 * A class to hold all information defining a user ( can be a host),
 * which is associated with a key store.
 *
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 */

public class AMI_EntryInfo implements Serializable {

    public AMI_EntryInfo() 
    {
    } 

    public AMI_EntryInfo(String hostName, String hostIP, long userId, 
			  String userName) 
    {
        _hostName  = hostName;
        _hostIP = hostIP;
        _userId = userId;
        _userName  = userName;

    }     

    public AMI_EntryInfo(DerInputStream in) throws IOException
    {
        read(in);
    } 


    public String getHostName() {
         return _hostName;
    }

    public String getHostIP() {
         return _hostIP;
    }

    public long getUserId() {
         return _userId;
    }

    public String getUserName() {
         return _userName;
    }

    public boolean equals(AMI_EntryInfo info) {

      // Match the User Id and Host IP

	if (_userId != info.getUserId())
	   return false;
	
	if (_hostIP == null) {
	    if (info.getHostIP() != null)
	        return false;
	} else if ((_hostIP.compareTo(info.getHostIP()) != 0) &&
	    (info.getHostIP().compareTo("0.0.0.0") != 0))
	    return false;

        return true;
    }

    public String toString() 
    {
         StringBuffer sb = new StringBuffer("\tEntry: ");
	 sb.append("\tHost IP = " + _hostIP);
	 sb.append("\tHostName = " + _hostName);
	 sb.append("\tUser Id = " + _userId);
	 sb.append("\tUser Name = " + _userName);

	 return sb.toString();
    }

    public void write(DerOutputStream derOut) throws IOException
    {	
	BigInt bi = null;
	
	derOut.putBitString(_hostIP.getBytes());

	if (_hostName == null) {
	    derOut.putBitString("NULL".getBytes());
	}
	else
	    derOut.putBitString(_hostName.getBytes());        

	if (_userId == 0)
	  bi = new BigInt(0);
        else 
	   bi = new BigInt(BigInteger.valueOf(_userId));

	derOut.putInteger(bi);
	if (_userName == null) {
	    derOut.putBitString("NULL".getBytes());
	    // derOut.putNull(); 
	}
	else
	  derOut.putBitString(_userName.getBytes());

    }


    public void read(DerInputStream derIn) throws IOException
    {	
        byte[] bitStr = null;

	// Read host IP
	bitStr = derIn.getBitString();
	_hostIP = new String(bitStr);
	
	// Read host name , otherwise read null
	bitStr = derIn.getBitString();
	if ("NULL".compareTo(new String(bitStr)) != 0)
	    _hostName = new String(bitStr);

	// Read user id
	_userId = (derIn.getInteger().toBigInteger()).longValue();

	// Read user name, otherwise read null

	  bitStr = derIn.getBitString();

	  if ("NULL".compareTo(new String(bitStr)) != 0)
	    _userName = new String(bitStr);
    }

    private void writeObject(ObjectOutputStream stream) throws IOException
    {
        stream.defaultWriteObject();
    }

    private void readObject(ObjectInputStream stream)
	throws IOException, ClassNotFoundException
    {
        stream.defaultReadObject();
    }

    private String _hostName = null;
    private String _hostIP;
    private long   _userId = 0;
    private String _userName = null;
}

