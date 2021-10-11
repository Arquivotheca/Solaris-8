/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_CertificateX509Key.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.ca;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Enumeration;
import java.security.*;

import sun.security.util.*;
import sun.security.x509.CertAttrSet;
import sun.security.x509.AttributeNameEnumeration;

import com.sun.ami.keygen.AMI_RSAPublicKey;
import com.sun.ami.keygen.AMI_KeyUtils;

/**
 * This class defines the PublicKey attribute for the Certificate.
 *
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 * @see CertAttrSet
 * @see Serializable
 * @see AMI_X509CertImpl
 */

public class AMI_CertificateX509Key implements CertAttrSet {
    /**
     * Identifier for this attribute, to be used with the
     * get, set, delete methods of Certificate, x509 type.
     */  
    public static final String IDENT = "x509.info.key";

    /**
     * Sub attributes name for this CertAttrSet.
     */
    public static final String NAME = "key";
    public static final String KEY = "value";

    /**
     * Default constructor for the certificate attribute.
     *
     * @param key the PublicKey
     */
    public AMI_CertificateX509Key(PublicKey key) {
        _key = key;
    }

    /**
     * Create the object, decoding the values from the passed DER stream.
     *
     * @param in the DerInputStream to read the PublicKey from.
     * @exception IOException on decoding errors.
     */
    public AMI_CertificateX509Key(DerInputStream in) throws IOException {
        DerValue val = in.getDerValue();
	System.out.println("In AMI_CertificateX509Key.. parsing");
        _key = AMI_KeyUtils.parse(val);
    }

    /**
     * Create the object, decoding the values from the passed stream.
     *
     * @param in the InputStream to read the PublicKey from.
     * @exception IOException on decoding errors.
     */
    public AMI_CertificateX509Key(InputStream in) throws IOException {
        DerValue val = new DerValue(in);
	System.out.println("In AMI_CertificateX509Key.. parsing");
        _key = AMI_KeyUtils.parse(val);
    }

    /**
     * Return the key as printable string.
     */
    public String toString() {
        if (_key == null)
		return ("");
        return (_key.toString());
    }

    /**
     * Decode the key in DER form from the stream.
     *
     * @param in the InputStream to unmarshal the contents from
     * @exception IOException on decoding or validity errors.
     */
    public void decode(InputStream in) throws IOException {
        DerValue val = new DerValue(in);
	System.out.println("In AMI_CertificateX509Key.. decoding");
        _key = AMI_KeyUtils.parse(val);
    }

    /**
     * Encode the key in DER form to the stream.
     *
     * @param out the OutputStream to marshal the contents to.
     * @exception IOException on errors.
     */
    public void encode(OutputStream out) throws IOException {
        out.write(_key.getEncoded());
    }

    /**
     * Set the attribute value.
     */
    public void set(String name, Object obj) throws IOException {
        if (!(obj instanceof PublicKey)) {
            throw new IOException("Attribute must be of type PublicKey.");
        }
        if (name.equalsIgnoreCase(KEY)) {
            _key = (PublicKey)obj;
        } else {
            throw new IOException("Attribute name not recognized by " +
                                  "CertAttrSet: AMI_CertificateX509Key.");
        }
    }

    /**
     * Get the attribute value.
     */
    public Object get(String name) throws IOException {
        if (name.equalsIgnoreCase(KEY)) {
            return (_key);
        } else {
            throw new IOException("Attribute name not recognized by " +
                                  "CertAttrSet: AMI_CertificateX509Key.");
        }
    }

    /**
     * Delete the attribute value.
     */
    public void delete(String name) throws IOException {
	if (name.equalsIgnoreCase(KEY)) {
		_key = null;
	} else {
		throw new IOException("Attribute name not recognized by " +
                    "CertAttrSet: AMI_CertificateX509Key.");
	}
    }

    /**
     * Return an enumeration of names of attributes existing within this
     * attribute.
     */
    public Enumeration getElements() {
        AttributeNameEnumeration elements = new AttributeNameEnumeration();
        elements.addElement(KEY);

        return (elements.elements());
    }

    /**
     * Return the name of this attribute.
     */
    public String getName() {
        return (NAME);
    }

    // Private data member
    private PublicKey	_key;

}
