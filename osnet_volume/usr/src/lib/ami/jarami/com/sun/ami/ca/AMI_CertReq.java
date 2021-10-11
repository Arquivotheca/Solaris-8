/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_CertReq.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.ca;

import java.io.ByteArrayOutputStream;
import java.io.PrintStream;
import java.io.InputStream;
import java.io.IOException;
import java.math.BigInteger;

import java.security.cert.CertificateException;
import java.security.NoSuchAlgorithmException;
import java.security.InvalidKeyException;
import java.security.Signature;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SignatureException;

import sun.misc.BASE64Encoder;
import sun.misc.HexDumpEncoder;
import sun.security.util.*;	// DER
import sun.security.x509.X500Name;
import sun.security.x509.X500Signer;
import sun.security.pkcs.PKCS10Attributes;
import sun.security.x509.AlgorithmId;

/**
 * The AMI_CertReq class represents a PKCS10 Certificate Request. 
 * 
 * <P>It is used to generate a Certificate Request, which is then
 * sent to a CA for certification, or a self-signed certificate can be
 * generated from it.
 *
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 */

public class AMI_CertReq
{
    /**
     * Constructs an unsigned PKCS #10 certificate request.  Before this
     * request may be used, it must be encoded and signed.  Then it
     * must be retrieved in some conventional format (e.g. string).
     * 
     */
    public AMI_CertReq()
    {}

    public AMI_CertReq(AMI_CertReqInfo info) 
    {
        _info = info;
    }

    /**
    * Unmarshals a cert request  from its encoded form, parsing the
    * encoded bytes. 
    *
    * @param certReqData the encoded bytes, with no trailing padding.
    * @exception AMI_CertReqException on parsing and initialization errors.
    */
    public AMI_CertReq(byte[] certReqData) throws AMI_CertReqException
    {

    try {
            DerValue	in = new DerValue(certReqData);

            parse(in);
            _certificateRequest = certReqData;
        } catch (IOException e) {
             throw new AMI_CertReqException("Unable to initialize, " + e);
        }	
    }

    /**
    * Unmarshals a cert request  from the stream, which contains an
    * encoded certificate request.
    *
    * @param in the Input  stream.
    * @exception AMI_CertReqException on parsing and initialization errors.
    */
    public AMI_CertReq(InputStream in)
    throws AMI_CertReqException {
        try {
            DerValue	val = new DerValue(in);

            parse(val);
            _certificateRequest = val.toByteArray();
        } catch (IOException e) {
             throw new AMI_CertReqException("Unable to initialize, " + e);
        }
    }


    /**
    *  Get the Public key of the requestor
    *  Convenience Function .. User can retrieve info object and then 
    *  use the get() methods from there.
    */
    public PublicKey getPublicKey() {
          return _info.getPublicKey();
    }

    /**
    *  Get the DName of the requestor
    *  Convenience Function .. User can retrieve info object and then 
    *  use the get() methods from there.
    */
    public X500Name getSubject() {
          return _info.getSubject();
    }

    /**
    *  Get the info object from the certreq
    */
    public AMI_CertReqInfo getCertReqInfo() {
          return _info;
    }

    /**
    * Set the Private key of the signer: same as requestor
    */
    public void setPrivateKey(PrivateKey privateKey) {
          _privateKey =  privateKey;
    }

    /**
    *  Set the Signature Algo 
    */
    public void setSignAlg(String signAlg) throws NoSuchAlgorithmException  {
          _algId = AlgorithmId.get(signAlg);
    }


    /**
    * Create the signed certificate request.  This will later be
    * retrieved in either string or binary format.
    *
    * @param privateKey The signer's private key.
    * @param signAlg  The algorithm to be used for signing.
    *
    * @exception IOException on errors.
    * @exception AMI_CertReqException on certreq handling errors.
    * @exception SignatureException on signature handling errors.
    * @exception InvalidKeyException on key handling errors.
    * @exception NoSuchAlgorithmException on signature handling errors.
    */
    public void encodeAndSign(PrivateKey privateKey, String signAlg)
    throws AMI_CertReqException, IOException, SignatureException,
    InvalidKeyException, NoSuchAlgorithmException {
	setPrivateKey(privateKey);
	setSignAlg(signAlg);
       
	encodeAndSign();
    }

    /**
     * Create the signed certificate request.  This will later be
     * retrieved in either string or binary format.
     *
     * @exception IOException on errors.
     * @exception AMI_CertReqException on certreq handling errors.
     * @exception SignatureException on signature handling errors.
     * @exception InvalidKeyException on key handling errors.
     * @exception NoSuchAlgorithmException on signature handling errors.
     */
    public void encodeAndSign()
    throws AMI_CertReqException, IOException, SignatureException,
    InvalidKeyException, NoSuchAlgorithmException {
	DerOutputStream		out, scratch;
	byte 			certificateRequestInfo [];
	byte			sig [];

	if (_certificateRequest != null)
	    throw new SignatureException("request is already signed");
	
	// Create a Signature object for given signature algo
	Signature signature = Signature.getInstance(_algId.getName());
        signature.initSign(_privateKey);

         scratch = new DerOutputStream();
	// Encode cert request info

	_info.encode(scratch);
	 certificateRequestInfo = scratch.toByteArray();

	// Encode Algo Id
	_algId.encode(scratch);

	// Sign it
	signature.update(certificateRequestInfo, 0,
	    certificateRequestInfo.length);
	sig = signature.sign();

	scratch.putBitString(sig);			// sig

            
	// Wrap the signed data in a SEQUENCE { data, algorithm, sig }
	out = new DerOutputStream();
	out.write(DerValue.tag_Sequence, scratch);

	// Finally set the certificate request !
	_certificateRequest = out.toByteArray();
    }


    /*
    *  Returns the DER-encoded certificate request as a byte array.
    */
    public byte [] toByteArray()
    {
	return _certificateRequest;
    }


    /**
     * Prints an E-Mailable version of the certificate request on the print
     * stream passed.  The format is a common base64 encoded one, supported
     * by most Certificate Authorities because Netscape web servers have
     * used this for some time.  Some certificate authorities expect some
     * more information, in particular contact information for the web
     * server administrator.
     *
     * @param out the print stream where the certificate request
     *	will be printed.
     * @exception IOException when an output operation failed
     * @exception SignatureException when the certificate request was
     *	not yet signed.
     */
    public void print(PrintStream out)
    throws IOException, SignatureException
    {
	if (_certificateRequest == null)
	    throw new SignatureException("Cert request was not signed");
	
	BASE64Encoder encoder = new BASE64Encoder();

	out.println("-----BEGIN NEW CERTIFICATE REQUEST-----");
	encoder.encodeBuffer(_certificateRequest, out);
	out.println("-----END NEW CERTIFICATE REQUEST-----");
    }

    /**
     * Provides a short description of this request.
     */
    public String toString()
    {
        if (_info == null || _algId == null || _signature == null)
            return "";

        StringBuffer sb = new StringBuffer();

        sb.append("[PKCS10 Certificate Request \n");
        sb.append(_info.toString() + "\n");
        sb.append("  Algorithm: [" + _algId.toString() + "]\n");

        HexDumpEncoder encoder = new HexDumpEncoder();
        sb.append("  Signature:\n" + encoder.encodeBuffer(_signature));
        sb.append("\n]");

        return sb.toString();
    }

    /*
     * Cert Req is SIGNED ASN.1 encoded, a three elment sequence:
     *
     *	- The "raw" cert req data
     *	- Signature algorithm
     *	- The signature bits
     *
     * This routine unmarshals the cert req
     */
    private void parse(DerValue val) throws AMI_CertReqException, IOException {
 
        DerValue	seq[] = new DerValue[3];
	DerInputStream  in;
	Signature sig = null;

        seq[0] = val.data.getDerValue();
        seq[1] = val.data.getDerValue();
        seq[2] = val.data.getDerValue();
	
        if (val.data.available() != 0) {
            throw new AMI_CertReqException("signed overrun, bytes = "
                                     + val.data.available());
        }
        if (seq[0].tag != DerValue.tag_Sequence) {
            throw new AMI_CertReqException("signed fields invalid");
        }

        _algId = AlgorithmId.parse(seq[1]);
        _signature = seq[2].getBitString();

        if (seq[1].data.available() != 0) {
            throw new AMI_CertReqException("algid field overrun");
        }
        if (seq[2].data.available() != 0)
            throw new AMI_CertReqException("signed fields overrun");

        // The CertReq Info

	_info = new AMI_CertReqInfo(seq[0]);


        //
        // OK, we parsed it all ... validate the signature using the
        // key and signature algorithm we found.
        //
        try {
            sig = Signature.getInstance(_algId.getName());
            sig.initVerify(_info.getPublicKey());
            sig.update(seq[0].toByteArray());
            if (!sig.verify(_signature))
                throw new AMI_CertReqException("Invalid PKCS #10 signature");
        } catch (InvalidKeyException e) {
            throw new AMI_CertReqException("invalid key");
        } catch (NoSuchAlgorithmException e) {
            throw new AMI_CertReqException("Invalid algorithm: " +
		_algId.getName());
        } catch (SignatureException e) {
            throw new AMI_CertReqException("Unable to verify Signature ." +
		e.getMessage());
	}

    }

    // Cert Req data, and its envelope
    protected AMI_CertReqInfo	_info = null;
    protected AlgorithmId	_algId;
    protected byte[]		_signature;

    private PrivateKey          _privateKey;
    private byte		_certificateRequest [];	// signed
}
