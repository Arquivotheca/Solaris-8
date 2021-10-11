/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_RSASignature.java	1.4 99/08/04 SMI"
 *
 */

package com.sun.ami.sign;

import java.io.*;
import java.util.*;
import java.security.*;
import java.security.interfaces.*;
import java.security.spec.*;

import com.sun.ami.amiserv.*;
import com.sun.ami.keymgnt.*;
import com.sun.ami.keygen.*;
import com.sun.ami.AMI_Exception;
import com.sun.ami.utils.*;
import com.sun.ami.common.*;

// Smartcard classes
import com.sun.smartcard.*;

/**
 * This class provides the base RSA Signature implementation
 * for MD5/RSA, MD2/RSA and SHA1/RSA
 *
 * It uses the AMI_Server for signature operation if no key is provided.
 */

public class AMI_RSASignature extends SignatureSpi {

    /* algorithm parameters */
    private String  signatureAlgorithm = null;
    private byte[]  privateKey = null;
    private byte[]  publicKey = null;
    
    private String  privateAlgo;
    private String  publicAlgo;

    private String  keyAlias = null;
    private long    userId;
    private String  hostIP = null;

    /* Parameter for the Data to be signed/verified */
    private byte[]  data = null;

    public AMI_RSASignature() throws NoSuchAlgorithmException {
	// Can never be instatiated this way
    }

    protected AMI_RSASignature(String signatureAlgo)
	throws NoSuchAlgorithmException, AMI_Exception {
	    // Call super class
	    super();
	    signatureAlgorithm = new String(signatureAlgo);

	    // Get the user ID
	    AMI_C_Utils utils = new AMI_C_Utils();
	    userId = utils.ami_get_user_id();

	    // Get the host IP address
	    hostIP = AMI_VirtualHost.getHostIP();
    }

    /**
     * Initialize the RSA object with a RSA private key.
     * 
     * @param privateKey the RSA private key
     * 
     * @exception InvalidKeyException if the key is not a valid RSA private
     * key.
     */
    protected void engineInitSign(PrivateKey pKey)
    throws InvalidKeyException {
        try {
              AMI_Debug.debugln(2,
		 "AMI_RSASignature::Initialising for signature");
	} catch (Exception e) {
             throw new RuntimeException(e.toString());
	}
        
	// Reset the data to null
	data = null; 

        if (pKey == null) {
	    // This is valid, since the default key will used
	    keyAlias = null;
	    return;
	}

	// Check for dummy key object
	if (pKey instanceof AMI_PrivateKey) {
		keyAlias = ((AMI_PrivateKey) pKey).getAlias();
		return;
	}

	// Check if RSAPrivate key is provided
	if (!(pKey instanceof RSAPrivateKey)) {
	    throw new InvalidKeyException(
		"not a RSA private key: " + pKey);
	}

	privateKey = pKey.getEncoded();
	privateAlgo = pKey.getAlgorithm();
    }

    /**
     * Initialize the RSA object with a RSA public key.
     * 
     * @param publicKey the RSA public key.
     * 
     * @exception InvalidKeyException if the key is not a valid RSA public
     * key.
     */
    protected void engineInitVerify(PublicKey pKey)
    throws InvalidKeyException {

        try {
              AMI_Debug.debugln(2,
		 "AMI_RSASignature::Initialising for verification");
	} catch (Exception e) {
             throw new RuntimeException(e.toString());
	}

	// Reset the data to null        
	data = null;
	if (!(pKey instanceof RSAPublicKey)) {
	    throw new InvalidKeyException(
		"not a RSA public key: " + pKey);
	}

        publicKey = pKey.getEncoded();
        publicAlgo = pKey.getAlgorithm();
    }

    /**
     * Sign all the data thus far updated. The signature is formatted
     * according to the Canonical Encoding Rules, returned as a DER
     * sequence ofthe signed integer.
     *
     * @return a signature block formatted according to the Canonical
     * Encoding Rules.
     *
     * @exception SignatureException if the signature object was not
     * properly initialized, or if another exception occurs.
     * 
     * This mehtod will invoke the native signature method from the 
     * AMI_Signature class.
     *
     * @see AMI_Signature
     * @see com.sun.ami.keygen.AMI_RSASignature#engineUpdate
     * @see com.sun.ami.keygen.AMI_RSASignature#engineVerify
     */
    protected byte[] engineSign() throws SignatureException {
      
        AMI_Signature signature = null;
	int length = 0;
	byte[] retData;  // Will contain the signed data

	// If private key has been initialised, then invoke 
	// the signature operation
	if (privateKey != null) {
	    try {
                AMI_Debug.debugln(2, "AMI_RSASignature:: " +
		    "Attempting to sign using private key supplied by user"); 
	    } catch (Exception e) {
		throw new SignatureException(e.toString());
	    }

	    try {
		// Digest the data
		String digestAlgo = null;
		if (signatureAlgorithm.toUpperCase().startsWith("MD5"))
			digestAlgo = new String("MD5");
		else if (signatureAlgorithm.toUpperCase().startsWith("MD2"))
			digestAlgo = new String("MD2");
		else if (signatureAlgorithm.toUpperCase().startsWith("SHA"))
			digestAlgo = new String("SHA1");
		MessageDigest digest = MessageDigest.getInstance(digestAlgo);
		digest.update(data);
		byte[] toBeSigned = digest.digest();

		// Encode the data
		AMI_DigestInfo digInfo = new AMI_DigestInfo(digestAlgo,
		    toBeSigned);
		byte[] encodedData = digInfo.encode();

		// Sign the digested & encoded data
	        signature = new AMI_Signature();
		length = privateKey.length;

		signature.ami_rsa_sign(encodedData, privateKey,
		    signatureAlgorithm, privateAlgo, length);
	    } catch (Exception amie) {
	        try {
                     AMI_Debug.debugln(1, "AMI_RSASignature:: " +
		     "Unable to initialise ami for sign: " + amie.toString()); 
		} catch (Exception e) {
		    throw new SignatureException(e.toString());
		}
	        throw(new SignatureException(amie.getLocalizedMessage()));
	    }
	    retData =  signature.getSign();
	} else if ((keyAlias != null) &&
	    keyAlias.toLowerCase().startsWith("smartcard:")) {
		// Alias is specific to smartcard
		// Check for valid algorithm
		String alias = keyAlias.substring(11);
		if (signatureAlgorithm.equalsIgnoreCase("SHA1/RSA") ||
		    signatureAlgorithm.equalsIgnoreCase("SHA/RSA"))
			retData = smartcardSign(alias, "RSA", data);
		else
			throw(new SignatureException(signatureAlgorithm));
	} else {
	    // Private Key is not available,
	    // do signature operation in the AMIServer
	    try {
		AMI_Debug.debugln(2, "AMI_RSASignature::Null Private " +
		    "key was passed. Trying to sign using the AMI Server " +
		    "and key: " + keyAlias);
	    } catch (Exception e) {
		throw new SignatureException(e.toString());
	    }
	  	     
	    // Get a handle to the client, based on the protocol.
	    AMI_KeyServ client = null;
	    try {
		client = AMI_KeyServClient.getInstance(
		    AMI_KeyMgntClient.getProperty(
		    AMI_Constants.PROTOCOL_PROPERTY));
	    } catch (Exception exp) {
		try {
		    AMI_Debug.debugln(1, 
			"AMI_ServerMD5RSASignature::Unable to get client"); 
		} catch (Exception e) {
		    throw new SignatureException(e.toString());
		}
		throw new SignatureException(exp.getLocalizedMessage());
	    }

	    AMI_CryptoInfo info = new AMI_CryptoInfo(null, hostIP, 
		userId, null, data, signatureAlgorithm, keyAlias);

	    try {
		AMI_Debug.debugln(2, 
		    "AMI_RSASignature::Signing data: " + info);
	    } catch (Exception e) {
		throw new SignatureException(e.toString());
	    }

	    try {
		retData = client.signData(info);
	    } catch (Exception exp) {
		try {
		    AMI_Debug.debugln(1, 
			"AMI_RSASignature::Signature Failed : " +
			 exp.toString());
		} catch (Exception e) {
		    throw new SignatureException(e.toString());
		}
		throw new SignatureException(exp.getLocalizedMessage());
	    }
	}
	
	try {
	    AMI_Debug.debugln(1, 
		"AMI_RSASignature::RSA Signature successful");
	    Object[] messageArguments = { new String("MD5/RSA") };
	    AMI_Log.writeLog(1, "AMI_Sign.sign", messageArguments);
	} catch (Exception e) {
	    throw new SignatureException(e.toString());
	}
	return retData;
    }


    /**
     * Verify all the data thus far updated. 
     *
     * @param signature the alledged signature, encoded using the
     * Canonical Encoding Rules, as an integer.
     *
     * @exception SignatureException if the signature object was not
     * properly initialized, or if another exception occurs.
     *
     * @see com.sun.ami.keygen.AMI_RSASignature#engineUpdate
     * @see com.sun.ami.keygen.AMI_RSASignature#engineSign 
     */
    protected boolean engineVerify(byte[] signature) 
    throws SignatureException {
	AMI_Signature verification = null;
	boolean ret_val = false;

	try {
	    AMI_Debug.debugln(2, "AMI_ServerMD5RSASignature:: " +
		"Attempting to verify RSA signature"); 
	} catch (Exception e) {
             throw new SignatureException(e.toString());
	}

    	verification = new AMI_Signature();

	try {
	    // Currently the algorithms passed here are ignored, and 
	    // standard algos are used
	    ret_val = verification.ami_rsa_verify(signature, 
		data, publicKey, signatureAlgorithm, publicAlgo);
	} catch (Exception se) {
             try {
                   AMI_Debug.debugln(1, "AMI_ServerMD5RSASignature:: " +
			    "Unable to verify signature: " + se.toString());
		} catch (Exception e) {
			throw new SignatureException(e.toString());
		}
		throw new SignatureException(se.getLocalizedMessage());
	}
	
	try {
		AMI_Debug.debugln(1, 
		"AMI_ServerMD5RSASignature::RSA Signature verified");
		Object[] messageArguments = {
		   new String("MD5/RSA")
		};
		AMI_Log.writeLog(1, "AMI_Sign.verify", messageArguments);
	} catch (Exception e) {
		throw new SignatureException(e.toString());
	}
	return ret_val;
    }

    /**
     * Update a byte to be signed or verified.
     *
     * @param b the byte to updated.
     */
    protected void engineUpdate(byte b) {

         byte[] buffer = null;
	 int last_indx = 0;

	 if (data != null) {
		buffer = new byte[data.length + 1];
		for (int i = 0; i < data.length; i++) 
			buffer[i] = data[i];
		last_indx = data.length;
	 } else
		buffer = new byte[1];

	 buffer[last_indx] = b;
	 data = buffer;
    }
    

    /**
     * Update an array of bytes to be signed or verified.
     * 
     * @param data the bytes to be updated.
     */
    protected void engineUpdate(byte[] d, int off, int len) {
         byte[] buffer = null;
	 int last_indx = 0;

	 if (data != null) {
		buffer = new byte[len + data.length];
		for (int i = 0; i < data.length; i++)
 			buffer[i] = data[i];
		last_indx = data.length;
	 } else 
		buffer = new byte[len];

	for (int i = off, j = last_indx; i < len; i++, j++)
		buffer[j] = d[i];

	data = buffer;
    }

    /**
     *
     * This method has been deprecated.
     *
     * @deprecated 
     */
    protected void engineSetParameter(String name, Object value) 
        throws UnsupportedOperationException {
	    throw new UnsupportedOperationException(
		"engineSetParameter:: not supported ");
    }

    /**
     *
     * This method has been deprecated.
     *
     * @deprecated 
     */
    protected Object engineGetParameter(String param) 
        throws UnsupportedOperationException {
	    throw new UnsupportedOperationException(
		"engineGetParameter:: not supported ");
    }

    /**
     * Return a human readable rendition of the engine.
     */
    public String toString() {
	return ("Signature " + signatureAlgorithm);
    }

    protected byte[] smartcardSign(String alias, String algo, byte[] data)
	throws SignatureException {
	    try {
		Smartcard sc = new Smartcard();
		if (!sc.cardPresent(new CardSpec(), new ReaderSpec()))
			throw(new SignatureException(alias+algo));
		Card card = sc.waitForCardInserted(new CardSpec(),
		    new ReaderSpec(), new AIDSpec(), new TimeoutSpec());
		SignatureCardService sics = (SignatureCardService)
		    card.getCardService(Card.SIGNATURE_CARD_SERVICE);
		return (sics.sign(alias, algo, data));
	    } catch (Exception e) {
		throw(new SignatureException(e.getLocalizedMessage()));
	    }
	}
}
