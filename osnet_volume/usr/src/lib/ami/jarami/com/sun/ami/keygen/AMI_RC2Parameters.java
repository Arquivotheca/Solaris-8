/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_RC2Parameters.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keygen;

import java.io.IOException;
import java.math.BigInteger;
import java.security.AlgorithmParametersSpi;
import java.security.spec.InvalidParameterSpecException;
import javax.crypto.spec.RC2ParameterSpec;
import java.security.spec.AlgorithmParameterSpec;
import sun.security.util.DerInputStream;
import sun.security.util.DerOutputStream;
import sun.security.util.DerValue;
import sun.security.util.BigInt;

/**
 * This class specifies the set of parameters used with the Rc2 algorithm.
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 * @see AlgorithmParameterSpec, AMI_RC2ParametersNoJce
 *
 */

public class AMI_RC2Parameters extends AlgorithmParametersSpi
{

protected void engineInit(AlgorithmParameterSpec paramSpec)
	throws InvalidParameterSpecException {
                if (!(paramSpec instanceof RC2ParameterSpec)) {
                throw new InvalidParameterSpecException
                    ("Inappropriate parameter specification");
            }
            _iv = ((RC2ParameterSpec)paramSpec).getIV();
	    _effectiveKeySize = ((RC2ParameterSpec)paramSpec).
getEffectiveKeyBits();
    }

    protected void engineInit(byte[] params) throws IOException {
        DerValue encodedParams = new DerValue(params);

        if (encodedParams.tag != DerValue.tag_Sequence) {
            throw new IOException("RC2 params parsing error");
        }

        encodedParams.data.reset();

        _effectiveKeySize = encodedParams.data.getInteger().toInt();
        _iv = encodedParams.data.getBitString();

        if (encodedParams.data.available() != 0) {
            throw new IOException("encoded params have " +
                                  encodedParams.data.available() +
                                  " extra bytes");
        }
    }

    protected void engineInit(byte[] params, String decodingMethod)
        throws IOException {
            engineInit(params);
    }


    protected AlgorithmParameterSpec engineGetParameterSpec(Class paramSpec)
        throws InvalidParameterSpecException {
            try {
                Class rc2ParamSpec = Class.forName
                    ("javax.crypto.spec.RC2ParameterSpec");
                if (rc2ParamSpec.isAssignableFrom(paramSpec)) {
                    return new RC2ParameterSpec(_effectiveKeySize, _iv);
                } else {
                    throw new InvalidParameterSpecException
                        ("Inappropriate parameter Specification");
                }
            } catch (ClassNotFoundException e) {
                throw new InvalidParameterSpecException
                    ("Unsupported parameter specification: " + e.getMessage());
            }
    }

    protected byte[] engineGetEncoded() throws IOException {
        DerOutputStream out = new DerOutputStream();
        DerOutputStream bytes = new DerOutputStream();

	// Encode params 
	
        bytes.putInteger(new BigInt(_effectiveKeySize));
	bytes.putBitString(_iv);
        out.write(DerValue.tag_Sequence, bytes);
        return out.toByteArray();
    }

    protected byte[] engineGetEncoded(String encodingMethod)
        throws IOException {
            return engineGetEncoded();
    }

    /*
     * Returns a formatted string describing the parameters.
     */
    protected String engineToString() {
        String str = "AMI_RC2Parameters::";

	str += "\n\teffective Key size = " + _effectiveKeySize; 

	str += "\n\tIv = ";
	for (int ii = 0; ii < _iv.length; ii ++) 
		str += _iv[ii] + " ";

	str += "\n";

	return str;
    }

    private byte[] _iv;
    private int _effectiveKeySize;
}
