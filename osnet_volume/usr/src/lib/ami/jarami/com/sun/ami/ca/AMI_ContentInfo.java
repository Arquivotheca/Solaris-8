/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_ContentInfo.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.ca;

import java.io.*;

import sun.security.util.*;
import sun.security.pkcs.ParsingException;

/**
 * A ContentInfo type, as defined in PKCS#7.
 *
 * This is a copy from the sun.security.
 * packages. It adds the getContentType() method,
 * as there is no way to access the content type otherwise.
 *
 * @version 1.0
 * @author Sangeeta Varma
 */

public class AMI_ContentInfo {

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

    // this is for backwards-compatibility with JDK 1.1.x
    private static final int[] OLD_SDATA = {1, 2, 840, 1113549, 1, 7, 2};
    public static final ObjectIdentifier OLD_SIGNED_DATA_OID =
    new ObjectIdentifier(OLD_SDATA);
    private static final int[] OLD_DATA = {1, 2, 840, 1113549, 1, 7, 1};
    public static final ObjectIdentifier OLD_DATA_OID =
    new ObjectIdentifier(OLD_DATA);

    /**
     * The ASN.1 systax for the Netscape Certificate Sequence
     * data type is defined
     * <a href=http://www.netscape.com/eng/security/comm4-cert-download.html>
     * here.</a>
     */
    public static final ObjectIdentifier NETSCAPE_CERT_SEQUENCE_OID =
    new ObjectIdentifier(nsdata);

    protected ObjectIdentifier contentType;
    protected DerValue content; // OPTIONAL

    public AMI_ContentInfo(ObjectIdentifier contentType, DerValue content) {
	this.contentType = contentType;
	this.content = content;
    }

    /**
     * Make a contentInfo of type data.
     */
    public AMI_ContentInfo(byte[] bytes) {
	DerValue octetString = new DerValue(DerValue.tag_OctetString, bytes);
	this.contentType = DATA_OID;
	this.content = octetString;
    }

    /**
     * Parses a PKCS#7 content info.
     */
    public AMI_ContentInfo(DerInputStream derin)
	throws IOException, ParsingException
    {
	this(derin, false);
    }

    /**
     * Parses a PKCS#7 content info.
     *
     * <p>This constructor is used only for backwards compatibility with
     * PKCS#7 blocks that were generated using JDK1.1.x.
     *
     * @param derin the ASN.1 encoding of the content info.
     * @param oldStyle flag indicating whether or not the given content info
     * is encoded according to JDK1.1.x.
     */
    public AMI_ContentInfo(DerInputStream derin, boolean oldStyle)
	throws IOException, ParsingException
    {
        DerInputStream disType;
	DerInputStream disTaggedContent;
	DerValue type;
	DerValue taggedContent;
	DerValue[] typeAndContent;
	DerValue[] contents;

	typeAndContent = derin.getSequence(2);

	// Parse the content type
	type = typeAndContent[0];
	disType = new DerInputStream(type.toByteArray());
	contentType = disType.getOID();

	if (oldStyle) {
	    // JDK1.1.x-style encoding
	    content = typeAndContent[1];
	} else {
	    // This is the correct, standards-compliant encoding.
	    // Parse the content (OPTIONAL field).
	    // Skip the [0] EXPLICIT tag by pretending that the content is the
	    // one and only element in an implicitly tagged set
	    if (typeAndContent.length > 1) { // content is OPTIONAL
		taggedContent = typeAndContent[1];
		disTaggedContent
		    = new DerInputStream(taggedContent.toByteArray());
		contents = disTaggedContent.getSet(1, true);
		content = contents[0];
	    }
	}
    }

    public DerValue getContent() {
	return content;
    }

    public byte[] getData() throws IOException {
	if (contentType.equals(DATA_OID) || contentType.equals(OLD_DATA_OID)) {
	    if (content == null)
		return null;
	    else
		return content.getOctetString();
	}
	throw new IOException("content type is not DATA: " + contentType);
    }

    public void encode(DerOutputStream out) throws IOException {
	DerOutputStream contentDerCode;
	DerOutputStream seq;

	seq = new DerOutputStream();
	seq.putOID(contentType);

	// content is optional, it could be external
	if (content != null) {
	    DerValue taggedContent = null;
	    contentDerCode = new DerOutputStream();
	    content.encode(contentDerCode);

	    // Add the [0] EXPLICIT tag in front of the content encoding
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
	if (content == null)
	    return null;

	DerInputStream dis = new DerInputStream(content.toByteArray());
	return dis.getOctetString();
    }

    /**
     */
    public ObjectIdentifier getContentType() {
	return contentType;
    }

    public String toString() {
	String out = "";

	out += "Content Info Sequence\n\tContent type: " + contentType + "\n";
	out += "\tContent: " + content;
	return out;
    }
}
