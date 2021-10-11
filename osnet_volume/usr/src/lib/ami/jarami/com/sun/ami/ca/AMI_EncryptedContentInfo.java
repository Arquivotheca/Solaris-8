/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_EncryptedContentInfo.java	1.2 99/07/11 SMI"
 *
 */

package com.sun.ami.ca;

import java.io.*;

import java.security.NoSuchAlgorithmException;
import sun.security.pkcs.ParsingException;
import sun.security.util.*;

//import com.sun.ami.utils.AlgorithmId;
import com.sun.ami.cmd.AlgorithmId;
/**
 * An EncryptedContentInfo type, as defined in PKCS#7.
 *
 * @version 1.0
 * @author Sangeeta Varma
 */

public class AMI_EncryptedContentInfo {

    // pkcs7 pre-defined content types
    private static int[]  pkcs7 = {1, 2, 840, 113549, 1, 7};
    private static int[]   data = {1, 2, 840, 113549, 1, 7, 1};
    private static int[]  sdata = {1, 2, 840, 113549, 1, 7, 2};
    private static int[]  edata = {1, 2, 840, 113549, 1, 7, 3};
    private static int[] sedata = {1, 2, 840, 113549, 1, 7, 4};
    private static int[]  ddata = {1, 2, 840, 113549, 1, 7, 5};
    private static int[] crdata = {1, 2, 840, 113549, 1, 7, 6};
    private static int[] nsdata = {2, 16, 840, 1, 113730, 2, 5};

    public static final ObjectIdentifier PKCS7_OID =
    new ObjectIdentifier(pkcs7);

    public static final ObjectIdentifier DATA_OID =
    new ObjectIdentifier(data);

    public static final ObjectIdentifier SIGNED_DATA_OID =
    new ObjectIdentifier(sdata);

    public static final ObjectIdentifier ENVELOPED_DATA_OID =
    new ObjectIdentifier(edata);

    public static final ObjectIdentifier SIGNED_AND_ENVELOPED_DATA_OID =
    new ObjectIdentifier(sedata);

    public static final ObjectIdentifier DIGESTED_DATA_OID =
    new ObjectIdentifier(ddata);

    public static final ObjectIdentifier ENCRYPTED_DATA_OID =
    new ObjectIdentifier(crdata);

    ObjectIdentifier contentType;
    AlgorithmId contentEncryptionAlgorithm;
    DerValue encryptedContent = null; // OPTIONAL

    public AMI_EncryptedContentInfo(ObjectIdentifier contentType, 
				    AlgorithmId contentEncryptionAlgorithm, 
				    DerValue encryptedContent) {
	this.contentType = contentType;
	this.contentEncryptionAlgorithm = contentEncryptionAlgorithm;
	this.encryptedContent = encryptedContent;
    }

    public AMI_EncryptedContentInfo(AlgorithmId contentEncryptionAlgorithm, 
				    byte[] bytes) {

        if (bytes != null) {
	    DerValue octetString =
		new DerValue(DerValue.tag_OctetString, bytes);
	    this.encryptedContent = octetString;
	}

	this.contentType = ENVELOPED_DATA_OID;
	this.contentEncryptionAlgorithm = contentEncryptionAlgorithm;
    }

    /**
     * Make a EncryptedContentInfo of type data.
     */
    public AMI_EncryptedContentInfo(String algorithm, byte[] bytes) 
	throws NoSuchAlgorithmException {
        if (bytes != null) {
	    DerValue octetString =
		new DerValue(DerValue.tag_OctetString, bytes);
	    this.encryptedContent = octetString;
	}
	    this.contentType = ENVELOPED_DATA_OID;
	    this.contentEncryptionAlgorithm = AlgorithmId.get(algorithm);

    }

    /**
     * Parses a PKCS#7 content info.
     *     
     */
    public AMI_EncryptedContentInfo(DerInputStream derin)
	throws IOException, ParsingException
    {
        DerInputStream disType;
	DerInputStream disTaggedContent;
	DerValue type;
	DerValue taggedContent;
	DerValue[] typeAndContent;
	DerValue[] contents;

	typeAndContent = derin.getSequence(3);

	// Parse the content type
	type = typeAndContent[0];
	disType = new DerInputStream(type.toByteArray());
	contentType = disType.getOID();

	contentEncryptionAlgorithm = AlgorithmId.parse(typeAndContent[1]);
	if (typeAndContent.length > 2) { // content is OPTIONAL
		taggedContent = typeAndContent[2];

		if (taggedContent != null) {
			System.out.println("not null , in decode");
			disTaggedContent
			    = new DerInputStream(taggedContent.toByteArray());
			contents = disTaggedContent.getSet(1, true);
			encryptedContent = contents[0];
		}
	}
    }

    public DerValue getContent() {
	return encryptedContent;
    }

    public AlgorithmId getContentEncryptionAlgorithm() {
	return contentEncryptionAlgorithm;
    }


    public byte[] getData() throws IOException {
	if (contentType.equals(ENVELOPED_DATA_OID)) {
	    if (encryptedContent == null)
		return null;
	    else
		return encryptedContent.getOctetString();
	}
	throw new IOException("content type is not ENVELOPED_DATA: " +
	    contentType);
    }

    public void encode(DerOutputStream out) throws IOException {
	DerOutputStream contentDerCode;
	DerOutputStream seq;

	seq = new DerOutputStream();
	seq.putOID(contentType);

	contentEncryptionAlgorithm.encode(seq);
	// content is optional, it could be external
	    
	if (encryptedContent != null) {
	    
	    DerValue taggedContent = null;
	    contentDerCode = new DerOutputStream();
	    encryptedContent.encode(contentDerCode);

	    // Add the [0] IMPLICIT tag in front of the content encoding
	    taggedContent = new DerValue((byte)0xA0,
					 contentDerCode.toByteArray());
	    seq.putDerValue(taggedContent);
	}
	out.write(DerValue.tag_Sequence, seq);
    }

    /**
     * Returns a byte array representation of the data held in
     * the content field.
     */
    public byte[] getContentBytes() throws IOException {
	if (encryptedContent == null)
	    return null;

	DerInputStream dis = new DerInputStream(encryptedContent.toByteArray());
	return dis.getOctetString();
    }

    public String toString() {
	String out = "";

	out += "EncryptedContent Info Sequence\nEncryptedContent type: ";
	out += contentType + "\n";
	out += "EncryptedContent: " + encryptedContent+"\n";
	out += "EncryptionAlgorithm: " + contentEncryptionAlgorithm+"\n";
	return out;
    }
}
