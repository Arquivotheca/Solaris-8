/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_CryptoInfo.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.amiserv;

import java.io.*;

/**
 * 
 * A class to hold all information required by the AMI Server to perform
 * a private key operation. The users of the AMI server, create this object
 * initialising it with all the required information, to do a private key
 * operation.
 * 
 * @author Sangeeta Varma
 * @version 1.0
 */

public class AMI_CryptoInfo implements Serializable {

    public AMI_CryptoInfo() 
    {
    } 

    public AMI_CryptoInfo(String hostName, String hostIP,
	long userId, String userName, 
	byte[] input, String algorithm, String keyAlias) {

        this(new AMI_EntryInfo(hostName, hostIP, userId,
	    userName), input, algorithm, keyAlias);
    }     


    public AMI_CryptoInfo(AMI_EntryInfo entry,
	byte[] input, String algorithm, String keyAlias) 
    {
        _entry = entry;
        _input  = input;
        _algorithm  = algorithm;
        _keyAlias = keyAlias;
    }     


    public byte[] getInput() {
         return _input;
    }

    public AMI_EntryInfo getEntry() {
         return _entry;
    }

    public String getAlgorithm() {
         return _algorithm;
    }

    public String getKeyAlias() {
         return _keyAlias;
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


    private AMI_EntryInfo _entry;
    private byte[] _input;
    private String _algorithm;
    private String _keyAlias;

}

