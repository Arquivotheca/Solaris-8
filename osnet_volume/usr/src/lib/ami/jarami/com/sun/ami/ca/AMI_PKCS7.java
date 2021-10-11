/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_PKCS7.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.ca;

import java.io.*;
import java.util.*;
import java.math.BigInteger;
import java.security.cert.Certificate;
import java.security.cert.X509Certificate;
import java.security.cert.CertificateException;
import java.security.cert.X509CRL;
import java.security.cert.CRLException;
import java.security.cert.CertificateFactory;
import java.security.*;

import sun.security.pkcs.*;
import sun.security.util.*;
import sun.misc.BASE64Encoder;
import sun.security.x509.AlgorithmId;
import sun.security.x509.X509CertImpl;
import sun.security.x509.X509CRLImpl;
import sun.security.x509.X500Name;

/**
 * This class replaces the PKCS7 class provided by JavaSoft in the Sun packages.
 * It implements the EnvelopedData type PKCS7 also.
 *  
 * It adds a print method to write out the PKCS7 data in a printable Base64 
 * encoded format.
 *
 *  @author Sangeeta Varma
 */

public class AMI_PKCS7  {

    public AMI_PKCS7(InputStream in) throws ParsingException, IOException {
        DataInputStream dis = new DataInputStream(in);
        byte[] data = new byte[dis.available()];
        dis.readFully(data);

        parse(new DerInputStream(data));
    }

    public AMI_PKCS7(DerInputStream derin) throws ParsingException,
	IOException {
	parse(derin);
    }

    public AMI_PKCS7(byte[] bytes) throws ParsingException, IOException  {
        DerInputStream derin = new DerInputStream(bytes);
        parse(derin);
    }


    public AMI_PKCS7(AMI_EncryptedContentInfo contentInfo,
                 AMI_RecipientInfo[] recipientInfos)
    {
        _version = new BigInt(0);
        _encryptedContentInfo = contentInfo;
	_recipientInfos = recipientInfos;

    }

    public AMI_PKCS7(AlgorithmId[] digestAlgorithmIds,
                 AMI_ContentInfo contentInfo,
                 X509Certificate[] certificates,
                 SignerInfo[] signerInfos)
    {
        _version = new BigInt(1);
        _digestAlgorithmIds = digestAlgorithmIds;
        _contentInfo = contentInfo;
        _certificates = certificates;
        _signerInfos = signerInfos;
    }


    /**
     * Construct an initialized AMI_PKCS7 block.
     *
     * @param caCertificate  The CA's certificate containing the public key
     * corresponding to th private key used for signing.
     * @param caPrivateKey   The CA's private key, to be used for
     * signing the data
     * @param data  The data which need to be wrapped as PKCS7.
     */

    /*
    * WOULD LIKE OT ADD THIS FUNCTION,
    * BUT CANNOT ACCESS PRIVATE VARIABLES ...
    *
    * Add a method which creates a PKCS7 envelope given the data, 
    * CACertificate and private key.
    *

    public void doPKCS7(X509Certificate    caCertificate,
			 PrivateKey         caPrivateKey,
			 byte[]             data)
    throws NoSuchAlgorithmException, InvalidKeyException, SignatureException
    {

        String digestAlgorithm;
        String signatureAlgorithm;

        X500Name name = (X500Name)caCertificate.getSubjectDN();
        BigInteger tmpSerial = caCertificate.getSerialNumber();
        BigInt serial = new BigInt(tmpSerial);
        String keyAlgorithm = caPrivateKey.getAlgorithm();

        if (keyAlgorithm.equals("DSA")) {
            signatureAlgorithm = "SHA/DSA";
            digestAlgorithm = "SHA";
        } else if (keyAlgorithm.equals("RSA") || keyAlgorithm.equals("DH")) {
            signatureAlgorithm = "MD5/RSA";
            digestAlgorithm = "MD5";
        } else {
            System.out.println("private key is not a DSA/RSA/DH key");
            return;
        }

        AlgorithmId digestAlg = AlgorithmId.get(digestAlgorithm);
        AlgorithmId sigAlg = AlgorithmId.get(signatureAlgorithm);

        Signature sig = Signature.getInstance(signatureAlgorithm);
        sig.initSign(caPrivateKey);

        contentInfo = new ContentInfo(data);

        sig.update(data);
        byte[] signature = sig.sign();

        SignerInfo signerInfo = new SignerInfo(name, serial,
                                               digestAlg, sigAlg,
                                               signature);
        version = new BigInt(1);

        AlgorithmId[]  l_digestAlgorithmIds = {digestAlg};
        SignerInfo[] l_signerInfos = {signerInfo};
        X509Certificate[] l_certificates = {caCertificate};

        digestAlgorithmIds = l_digestAlgorithmIds;
        signerInfos = l_signerInfos;
        certificates = l_certificates;
    }
    */

    /**
     * Parses a PKCS#7 block.
     *
     * @param derin the ASN.1 encoding of the PKCS#7 block.
     * @param oldStyle flag indicating whether or not the given PKCS#7 block
     * is encoded according to JDK1.1.x.
     */
    protected void parse(DerInputStream derin)
        throws IOException
    {
        _contentInfo = new AMI_ContentInfo(derin);
        _contentType = _contentInfo.getContentType();
        DerValue content = _contentInfo.getContent();

        if (_contentType.equals(AMI_ContentInfo.SIGNED_DATA_OID)) {
            parseSignedData(content);
        } else if (_contentType.equals(AMI_ContentInfo.ENVELOPED_DATA_OID)) {
            parseEnvelopedData(content);
        } else {
            throw new ParsingException("content type " + _contentType +
                                       " not supported.");
        }
    }

    protected void parseSignedData(DerValue val)
        throws ParsingException, IOException {

        DerInputStream dis = val.toDerInputStream();

        // Version
        _version = dis.getInteger();

        // digestAlgorithmIds
        DerValue[] digestAlgorithmIdVals = dis.getSet(1);
        int len = digestAlgorithmIdVals.length;
        _digestAlgorithmIds = new AlgorithmId[len];
        try {
            for (int i = 0; i < len; i++) {
                DerValue oid = digestAlgorithmIdVals[i];
                _digestAlgorithmIds[i] = AlgorithmId.parse(oid);
            }

        } catch (IOException e) {
            ParsingException pe =
                new ParsingException("Error parsing digest AlgorithmId IDs: " +
                                     e.getMessage());
            pe.fillInStackTrace();
            throw pe;
        }
        // contentInfo
        _contentInfo = new AMI_ContentInfo(dis);

        /*
         * check if certificates (implicit tag) are provided
         * (certificates are OPTIONAL)
         */
        if ((byte)(dis.peekByte()) == (byte)0xA0) {
            DerValue[] certVals = dis.getSet(2, true);

            len = certVals.length;
            _certificates = new X509Certificate[len];

            CertificateFactory certfac = null;
            try {
                certfac = CertificateFactory.getInstance("X.509");
            } catch (CertificateException ce) {
                // do nothing
            }

            for (int i = 0; i < len; i++) {
                ByteArrayInputStream bais = null;
                try {
                    if (certfac == null)
                        _certificates[i] = new X509CertImpl(certVals[i]);
                    else {
                        byte[] encoded = certVals[i].toByteArray();
                        bais = new ByteArrayInputStream(encoded);
                        _certificates[i] =
                            (X509Certificate)certfac.generateCertificate(bais);
                        bais.close();
                        bais = null;
                    }
                } catch (CertificateException ce) {
                    throw new ParsingException("CertificateException: " +
                                               ce.getMessage());
                } catch (IOException ioe) {
                    throw new ParsingException("CertificateException: " +
                                               ioe.getMessage());
                } finally {
                    if (bais != null)
                        bais.close();
                }
            }
        }

        // check if crls (implicit tag) are provided (crls are OPTIONAL)
        if ((byte)(dis.peekByte()) == (byte)0xA1) {
            DerValue[] crlVals = dis.getSet(1, true);

            len = crlVals.length;
            _crls = new X509CRL[len];

            for (int i = 0; i < len; i++) {
                try {
                    X509CRL crl = (X509CRL) new X509CRLImpl(crlVals[i]);
                    _crls[i] = crl;
                } catch (CRLException e) {
                    ParsingException pe =
                        new ParsingException("CRLException: " +
                                             e.getMessage());
                    pe.fillInStackTrace();
                    throw pe;
                }
            }
        }

        // signerInfos
        DerValue[] signerInfoVals = dis.getSet(1);

        len = signerInfoVals.length;
        _signerInfos = new SignerInfo[len];

        for (int i = 0; i < len; i++) {
            DerInputStream in = signerInfoVals[i].toDerInputStream();
            _signerInfos[i] = new SignerInfo(in);
        }
    }

    protected void parseEnvelopedData(DerValue val)
        throws ParsingException, IOException {

        DerInputStream dis = val.toDerInputStream();

        // Version
        _version = dis.getInteger();

	// recipient infos
	DerValue[] recipientInfoVals = dis.getSet(1);
        int len = recipientInfoVals.length;
        _recipientInfos = new AMI_RecipientInfo[len];

        for (int i = 0; i < len; i++) {
            DerInputStream in = recipientInfoVals[i].toDerInputStream();
            _recipientInfos[i] = new AMI_RecipientInfo(in);
        }


        // contentInfo
        _encryptedContentInfo = new AMI_EncryptedContentInfo(dis);
    }


    /**
     * Encodes the encrypted data to an output stream.
     *
     * @param out the output stream to write the encoded data to.
     * @exception IOException on encoding errors.
     */
    public void encodeEnvelopedData(OutputStream out) throws IOException {
        DerOutputStream derout = new DerOutputStream();
        encodeEnvelopedData(derout);
        out.write(derout.toByteArray());
    }


    /**
     * Encodes the signed data to a DerOutputStream.
     *
     * @param out the DerOutputStream to write the encoded data to.
     * @exception IOException on encoding errors.
     */
    public void encodeEnvelopedData(DerOutputStream out)
        throws IOException
    {
        DerOutputStream encryptedData = new DerOutputStream();

        // version
        encryptedData.putInteger(_version);

        // recipientInfo
        encryptedData.putOrderedSetOf(DerValue.tag_Set, _recipientInfos);
 
       // encryptedContentInfo
        _encryptedContentInfo.encode(encryptedData);

        // making it a encrypted data block
        DerValue encryptedDataSeq = new DerValue(DerValue.tag_Sequence,
                                              encryptedData.toByteArray());
        // making it a content info sequence
        AMI_ContentInfo block = new AMI_ContentInfo(
	    AMI_ContentInfo.ENVELOPED_DATA_OID, encryptedDataSeq);

        // writing out the contentInfo sequence
        block.encode(out);
    }

    /**
     * Encodes the signed data to an output stream.
     *
     * @param out the output stream to write the encoded data to.
     * @exception IOException on encoding errors.
     */
    public void encodeSignedData(OutputStream out) throws IOException {
        DerOutputStream derout = new DerOutputStream();
        encodeSignedData(derout);
        out.write(derout.toByteArray());
    }

    /**
     * Encodes the signed data to a DerOutputStream.
     *
     * @param out the DerOutputStream to write the encoded data to.
     * @exception IOException on encoding errors.
     */
    public void encodeSignedData(DerOutputStream out)
        throws IOException
    {
        DerOutputStream signedData = new DerOutputStream();

        // version
        signedData.putInteger(_version);

        // digestAlgorithmIds
        signedData.putOrderedSetOf(DerValue.tag_Set, _digestAlgorithmIds);

        // contentInfo
        _contentInfo.encode(signedData);

        // certificates (optional)
        if (_certificates != null && _certificates.length != 0) {
            // cast to X509CertImpl[] since X509CertImpl implements DerEncoder
            X509CertImpl implCerts[] = new X509CertImpl[_certificates.length];
            for (int i = 0; i < _certificates.length; i++) {
                if (_certificates[i] instanceof X509CertImpl)
                    implCerts[i] = (X509CertImpl) _certificates[i];
                else {
                    try {
                        byte[] encoded = _certificates[i].getEncoded();
                        implCerts[i] = new X509CertImpl(encoded);
                    } catch (CertificateException ce) {
                        throw new IOException(ce.getMessage());
                    }
                }
            }

            // Add the certificate set (tagged with [0] IMPLICIT)
            // to the signed data
            signedData.putOrderedSetOf((byte)0xA0, implCerts);
        }

        // no crls (OPTIONAL field)

        // signerInfos
        signedData.putOrderedSetOf(DerValue.tag_Set, _signerInfos);

        // making it a signed data block
        DerValue signedDataSeq = new DerValue(DerValue.tag_Sequence,
                                              signedData.toByteArray());

        // making it a content info sequence
        AMI_ContentInfo block = new AMI_ContentInfo(
	    AMI_ContentInfo.SIGNED_DATA_OID,
            signedDataSeq);

        // writing out the contentInfo sequence
        block.encode(out);
    }


    public void printEnvelopeAndData(PrintStream out, boolean prtBoundry)
    throws IOException
    {
        BASE64Encoder encoder = new BASE64Encoder();

        DerOutputStream outs = new DerOutputStream();

        encodeEnvelopedData(outs);

        byte[] pkcs7Data = outs.toByteArray();

	if (prtBoundry)
	    out.println("-----BEGIN ENCRYPTION INFO AND ENCRYPTED DATA-----");

        encoder.encodeBuffer(pkcs7Data, out);

	if (prtBoundry)
	    out.println("-----END ENCRYPTION INFO AND ENCRYPTED DATA-----");
    }

    public void printEnvelopeOnly(PrintStream out, boolean prtBoundry)
    throws IOException
    {
        BASE64Encoder encoder = new BASE64Encoder();

        DerOutputStream outs = new DerOutputStream();

        encodeEnvelopedData(outs);

        byte[] pkcs7Data  = outs.toByteArray();

	if (prtBoundry)
	    out.println("-----BEGIN ENCRYPTION INFO-----");

        encoder.encodeBuffer(pkcs7Data, out);

	if (prtBoundry)
	    out.println("-----END ENCRYPTION INFO-----");
    }

    public void printSignatureAndData(PrintStream out, boolean prtBoundry)
    throws IOException
    {
        BASE64Encoder encoder = new BASE64Encoder();

        DerOutputStream outs = new DerOutputStream();

        encodeSignedData(outs);

        byte[] pkcs7Data  = outs.toByteArray();

        if (prtBoundry)
		out.println(
		    "-----BEGIN DIGITAL SIGNATURE AND DATA SIGNED-----");

        encoder.encodeBuffer(pkcs7Data, out);

        if (prtBoundry)
		out.println(
		    "-----END DIGITAL SIGNATURE AND DATA SIGNED-----");
    }

    public void printSignatureOnly(PrintStream out, boolean prtBoundry)
    throws IOException
    {
	BASE64Encoder encoder = new BASE64Encoder();

	DerOutputStream outs = new DerOutputStream();

	encodeSignedData(outs);

	byte[] pkcs7Data  = outs.toByteArray();

	if (prtBoundry)
	    out.println("-----BEGIN DIGITAL SIGNATURE-----");

	encoder.encodeBuffer(pkcs7Data, out);

	if (prtBoundry)
	    out.println("-----END DIGITAL SIGNATURE-----");
    }

    public  BigInt getVersion() {
        return _version;
    }

    /**
     * Returns the message digest algorithms specified in this PKCS7 block.
     * @return the array of Digest Algorithms or null if none are specified
     *         for the content type.
     */
    public AlgorithmId[] getDigestAlgorithmIds() {
        return  _digestAlgorithmIds;
    }

    /**
     * Returns the content information specified in this PKCS7 block.
     */
    public AMI_ContentInfo getContentInfo() {
        return _contentInfo;
    }

    /**
     * Returns the X.509 certificates listed in this PKCS7 block.
     * @return the array of X.509 certificates or null if none are specified
     *         for the content type.
     */
    public X509Certificate[] getCertificates() {
        return (X509Certificate[])_certificates.clone();
    }

    /**
     * Returns the X.509 crls listed in this PKCS7 block.
     * @return the array of X.509 crls or null if none are specified
     *         for the content type.
     */
    public X509CRL[] getCRLs() {
        return _crls;
    }

    /**
     * Returns the signer's information specified in this PKCS7 block.
     * @return the array of Signer Infos or null if none are specified
     *         for the content type.
     */
    public SignerInfo[] getSignerInfos() {
        return _signerInfos;
    }

    /**
     * Returns the recipient's information specified in this PKCS7 block.
     * @return the array of Recipient Infos or null if none are specified
     *         for the content type.
     */
    public AMI_RecipientInfo[] getRecipientInfos() {
        return _recipientInfos;
    }


    /**
     * Returns the encrypted content information specified in this PKCS7 block.
     */
    public AMI_EncryptedContentInfo getEncryptedContentInfo() {
        return _encryptedContentInfo;
    }


    public X509Certificate getCertificate(BigInt serial, X500Name name) {
        if (_certificates != null)
            for (int i = 0; i < _certificates.length; i++) {
                X509Certificate cert = _certificates[i];
                X500Name thisName = (X500Name)cert.getIssuerDN();
                BigInteger tmpSerial = (BigInteger)cert.getSerialNumber();
                BigInt thisSerial = new BigInt(tmpSerial);
                if (serial.equals(thisSerial) && name.equals(thisName)) {
                    return cert;
                }
            }
        return null;
    }

    /**
     * Returns the PKCS7 block in a printable string form.
     */
    public String toString() {
        String out = "";

	if (_contentInfo != null)
	  out += _contentInfo + "\n";
	else if (_encryptedContentInfo != null)
	    out += _encryptedContentInfo + "\n";
        if (_version != null)
            out += "PKCS7 :: version: " + _version + "\n";
        if (_digestAlgorithmIds != null) {
            out += "PKCS7 :: digest AlgorithmIds: \n";
            for (int i = 0; i < _digestAlgorithmIds.length; i++)
                out += "\t" + _digestAlgorithmIds[i] + "\n";
        }
        if (_certificates != null) {
            out += "PKCS7 :: certificates: \n";
            for (int i = 0; i < _certificates.length; i++)
                out += "\t" + i + ".   " + _certificates[i] + "\n";
        }
        if (_crls != null) {
            out += "PKCS7 :: crls: \n";
            for (int i = 0; i < _crls.length; i++)
                out += "\t" + i + ".   " + _crls[i] + "\n";
        }
        if (_signerInfos != null) {
            out += "PKCS7 :: signer infos: \n";
            for (int i = 0; i < _signerInfos.length; i++)
                out += ("\t" + i + ".  " + _signerInfos[i] + "\n");
        }

        if (_recipientInfos != null) {
            out += "PKCS7 :: recipient infos: \n";
            for (int i = 0; i < _recipientInfos.length; i++)
                out += ("\t" + i + ".  " + _recipientInfos[i] + "\n");
        }

        return out;
    }

    private ObjectIdentifier _contentType;

    private BigInt _version = null;
    private AMI_EncryptedContentInfo _encryptedContentInfo = null;
    private AMI_RecipientInfo[] _recipientInfos = null;
    private AMI_ContentInfo _contentInfo = null;
    private AlgorithmId[] _digestAlgorithmIds = null;
    private X509Certificate[] _certificates = null;
    private X509CRL[] _crls = null;
    private SignerInfo[] _signerInfos = null;

}
