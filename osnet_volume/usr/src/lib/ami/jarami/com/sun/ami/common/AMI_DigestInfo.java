/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_DigestInfo.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.common;

/**
 * This class encodes digested data as per the PKCS #1 Standard.
 *
 */

import java.io.*;
import sun.security.util.*;
import sun.security.pkcs.ParsingException;
import sun.security.x509.AlgorithmId;

import com.sun.ami.common.*;

public class AMI_DigestInfo {

    public AMI_DigestInfo(String algo, byte[] digest) {
	_algo = algo;
	_digest = digest;
    }


    /**
     * Parses a digest info.
     */
    public AMI_DigestInfo(InputStream derin)
	throws IOException, ParsingException
    {
	DerValue val;

        val = new DerValue(derin);
        if (val.tag != DerValue.tag_Sequence)
               throw new ParsingException("invalid data format");

         AlgorithmId algid = AlgorithmId.parse(val.data.getDerValue());
	 _algo = algid.getName();

         _digest = val.data.getOctetString();
         if (val.data.available() != 0)
                throw new ParsingException("excess key data");
    }


    public byte[] encode() throws IOException, Exception {

         DerOutputStream out = new DerOutputStream();
         encode(out);

         return (out.toByteArray());
    }

    public void encode(DerOutputStream out) throws IOException, Exception {
        AMI_Debug.debugln(2, "Encoding Digested data !");
	DerOutputStream seq =  new DerOutputStream();
	AlgorithmId algid = null;

	if (_algo.equalsIgnoreCase("MD5"))
		algid = new AlgorithmId(AlgorithmId.MD5_oid);
	else if (_algo.equalsIgnoreCase("MD2"))
		algid = new AlgorithmId(AlgorithmId.MD2_oid);
	else if (_algo.equalsIgnoreCase("SHA") ||
		_algo.equalsIgnoreCase("SHA1"))
		algid = new AlgorithmId(AlgorithmId.SHA_oid);
	else
		throw new Exception("Unsupported Algorithm : " + _algo);

        algid.encode(seq);

        seq.putOctetString(_digest);

	out.write(DerValue.tag_Sequence, seq);
    }

    public static byte[] encode(String algo, byte[] digest) throws Exception {
	DerOutputStream seq =  new DerOutputStream();
	AlgorithmId algid = null;
        DerOutputStream out = new DerOutputStream();

	if (algo.equalsIgnoreCase("MD5"))
		algid = new AlgorithmId(AlgorithmId.MD5_oid);
	else if (algo.equalsIgnoreCase("MD2"))
		algid = new AlgorithmId(AlgorithmId.MD2_oid);
	else if (algo.equalsIgnoreCase("SHA1"))
		algid = new AlgorithmId(AlgorithmId.SHA_oid);
	else
		throw new Exception("Unsupported Algorithm : " + algo);

        algid.encode(seq);

        seq.putOctetString(digest);

	out.write(DerValue.tag_Sequence, seq);
        return (out.toByteArray());
    }

    private String _algo;
    private byte[] _digest; 
}
