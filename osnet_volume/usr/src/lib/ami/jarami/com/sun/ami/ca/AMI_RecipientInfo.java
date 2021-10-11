/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_RecipientInfo.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.ca;

import java.io.OutputStream;
import java.io.IOException;
import java.security.cert.X509Certificate;
import java.security.*;
import java.util.ArrayList;

import sun.security.util.*;
import sun.security.pkcs.ParsingException;
import sun.security.x509.X500Name;
import sun.security.x509.KeyUsageExtension;
import sun.security.x509.PKIXExtensions;
import sun.misc.HexDumpEncoder;

//import com.sun.ami.utils.AlgorithmId;
import com.sun.ami.cmd.AlgorithmId;

/**
 * A RecipientInfo, as defined in PKCS#7's enveloped data  type.
 *
 * @author Sangeeta Varma
 * @version 1.0
 */
public class AMI_RecipientInfo implements DerEncoder {

    BigInt version;
    X500Name issuerName;
    BigInt certificateSerialNumber;
    AlgorithmId keyEncryptionAlgorithmId;
    byte[] encryptedKey;

    public AMI_RecipientInfo(X500Name 	issuerName,
		      BigInt serial,
		      AlgorithmId keyEncryptionAlgorithmId,
		      byte[] encryptedKey) {
	this.version = new BigInt(0);
	this.issuerName	= issuerName;
	this.certificateSerialNumber = serial;
	this.keyEncryptionAlgorithmId = keyEncryptionAlgorithmId;
	this.encryptedKey = encryptedKey;
    }

    /**
     * Parses a PKCS#7 recipient info.
     */
    public AMI_RecipientInfo(DerInputStream derin)
	throws IOException, ParsingException
    {
	// version
	version = derin.getInteger();

	// issuerAndSerialNumber
	DerValue[] issuerAndSerialNumber = derin.getSequence(2);
	byte[] issuerBytes = issuerAndSerialNumber[0].toByteArray();
	issuerName = new X500Name(new DerValue(DerValue.tag_Sequence,
					       issuerBytes));
	certificateSerialNumber = issuerAndSerialNumber[1].getInteger();

	// keyAlgorithmId
	DerValue tmp = derin.getDerValue();

	keyEncryptionAlgorithmId = AlgorithmId.parse(tmp);

	// encryptedKey
	encryptedKey = derin.getOctetString();

	// all done
	if (derin.available() != 0) {
	    throw new ParsingException("extra data at the end");
	}
    }

    public void encode(DerOutputStream out) throws IOException {

	derEncode(out);
    }

    /**
     * DER encode this object onto an output stream.
     * Implements the <code>DerEncoder</code> interface.
     *
     * @param out
     * the output stream on which to write the DER encoding.
     *
     * @exception IOException on encoding error.
     */
    public void derEncode(OutputStream out) throws IOException {
	DerOutputStream seq = new DerOutputStream();
	seq.putInteger(version);
	DerOutputStream issuerAndSerialNumber = new DerOutputStream();
	issuerName.encode(issuerAndSerialNumber);
	issuerAndSerialNumber.putInteger(certificateSerialNumber);
	seq.write(DerValue.tag_Sequence, issuerAndSerialNumber);

	keyEncryptionAlgorithmId.encode(seq);

	seq.putOctetString(encryptedKey);

	DerOutputStream tmp = new DerOutputStream();
	tmp.write(DerValue.tag_Sequence, seq);

	out.write(tmp.toByteArray());
    }


    public BigInt getVersion() {
	    return version;
    }

    public X500Name getIssuerName() {
	return issuerName;
    }

    public BigInt getCertificateSerialNumber() {
	return certificateSerialNumber;
    }

    public AlgorithmId getKeyAlgorithmId() {
	return keyEncryptionAlgorithmId;
    }


    public byte[] getEncryptedKey() {
	return encryptedKey;
    }

    public String toString() {
	HexDumpEncoder hexDump = new HexDumpEncoder();

	String out = "";

	out += "Recipient Info for (issuer): " + issuerName + "\n";
	out += "\tversion: " + version + "\n";
	out += "\tcertificateSerialNumber: " + certificateSerialNumber +
	    "\n";
	out += "\tkeyAlgorithmId: " + keyEncryptionAlgorithmId + "\n";
	out += "\tencryptedKey: " + "\n" +
	    hexDump.encodeBuffer(encryptedKey) + "\n";
	return out;
    }

}




