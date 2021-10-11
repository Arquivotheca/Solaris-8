/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)DerOutputStream.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.utils;

import java.io.FilterOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.TimeZone;
import java.util.Vector;

import java.util.Comparator;
import java.util.Arrays;

import sun.security.util.BigInt;
import sun.security.util.BitArray;
import sun.security.util.ByteArrayLexOrder;
import sun.security.util.ByteArrayTagOrder;

/**
 * Output stream marshaling DER-encoded data.  This is eventually provided
 * in the form of a byte array; there is no advance limit on the size of
 * that byte array.
 *
 * <P>At this time, this class supports only a subset of the types of
 * DER data encodings which are defined.  That subset is sufficient for
 * generating most X.509 certificates.
 *
 * @version 1.34
 *
 * @author David Brownell
 * @author Amit Kapoor
 * @author Hemma Prafullchandra
 */
public class DerOutputStream 
extends ByteArrayOutputStream implements DerEncoder {
    /**
     * Construct an DER output stream.
     *
     * @param size how large a buffer to preallocate.
     */
    public DerOutputStream(int size) { super(size); }

    /**
     * Construct an DER output stream.
     */
    public DerOutputStream() { }

    /**
     * Writes tagged, pre-marshaled data.  This calcuates and encodes
     * the length, so that the output data is the standard triple of
     * { tag, length, data } used by all DER values.
     *
     * @param tag the DER value tag for the data, such as
     *		<em>DerValue.tag_Sequence</em>
     * @param buf buffered data, which must be DER-encoded 
     */
    public void write(byte tag, byte[] buf) throws IOException {
	write(tag);
	putLength(buf.length);
	write(buf, 0, buf.length);
    }

    /**
     * Writes tagged data using buffer-to-buffer copy.  As above,
     * this writes a standard DER record.  This is often used when
     * efficiently encapsulating values in sequences.
     *
     * @param tag the DER value tag for the data, such as
     *		<em>DerValue.tag_Sequence</em>
     * @param out buffered data
     */
    public void write(byte tag, DerOutputStream out) throws IOException {
	write(tag);
	putLength(out.count);
	write(out.buf, 0, out.count);
    }

    /**
     * Writes implicitly tagged data using buffer-to-buffer copy.  As above,
     * this writes a standard DER record.  This is often used when
     * efficiently encapsulating implicitly tagged values.
     *   
     * @param tag the DER value of the context-specific tag that replaces
     * original tag of the value in the output, such as in
     * <pre>
     *          <em> <field> [N] IMPLICIT <type></em>
     * </pre>
     * For example, <em>FooLength [1] IMPLICIT INTEGER</em>, with value=4;
     * would be encoded as "81 01 04"  whereas in explicit
     * tagging it would be encoded as "A1 03 02 01 04".
     * Notice that the tag is A1 and not 81, this is because with
     * explicit tagging the form is always constructed. 
     * @param value original value being implicitly tagged
     */  
    public void writeImplicit(byte tag, DerOutputStream value)
    throws IOException {
        write(tag);
        write(value.buf, 1, value.count-1);
    }

    /**
     * Marshals pre-encoded DER value onto the output stream.
     */
    public void putDerValue(DerValue val) throws IOException {
	val.encode(this);
    }

    /*
     * PRIMITIVES -- these are "universal" ASN.1 simple types.
     *
     * 	BOOLEAN, INTEGER, BIT STRING, OCTET STRING, NULL
     *	OBJECT IDENTIFIER, SEQUENCE(OF), SET(OF)
     *	PrintableString, T61String, IA5String, UTCTime
     */

    /**
     * Marshals a DER boolean on the output stream.
     */  
    public void putBoolean(boolean val) throws IOException {
        write(DerValue.tag_Boolean);
        putLength(1);
        if (val) {
            write(0xff);
        } else {
            write(0);
        }
    } 

    /**
     * Marshals a DER unsigned integer on the output stream.
     */
    public void putInteger(BigInt i) throws IOException {
	write(DerValue.tag_Integer);
        putBigInt(i);
    }

    /**
     * Marshals a DER enumerated on the output stream.
     */
    public void putEnumerated(BigInt i) throws IOException {
	write(DerValue.tag_Enumerated);
        putBigInt(i);
    }

    private void putBigInt(BigInt i) throws IOException {
	byte[]	buf = i.toByteArray();
	if ((buf[0] & 0x080) != 0) {
	    /*
	     * prepend zero so it's not read as a negative number
	     */
	    putLength(buf.length + 1);
	    write(0);
	} else
	    putLength(buf.length);
	write(buf, 0, buf.length);
    }

    /**
     * Marshals a DER bit string on the output stream. The bit 
     * string must be byte-aligned.
     *
     * @param bits the bit string, MSB first
     */
    public void putBitString(byte[] bits) throws IOException {
	write(DerValue.tag_BitString);
	putLength(bits.length + 1);
	write(0);		// all of last octet is used
	write(bits);
    }

    /**
     * Marshals a DER bit string on the output stream.
     * The bit strings need not be byte-aligned.
     *
     * @param bits the bit string, MSB first
     */
    public void putUnalignedBitString(BitArray ba) throws IOException {
	byte[] bits = ba.toByteArray();

	write(DerValue.tag_BitString);
	putLength(bits.length + 1);
	write(bits.length*8 - ba.length()); // excess bits in last octet
	write(bits);
    }

    /**
     * DER-encodes an ASN.1 OCTET STRING value on the output stream.
     *
     * @param octets the octet string
     */
    public void putOctetString(byte[] octets) throws IOException {
	write(DerValue.tag_OctetString, octets);
    }

    /**
     * Marshals a DER "null" value on the output stream.  These are
     * often used to indicate optional values which have been omitted.
     */
    public void putNull() throws IOException {
	write(DerValue.tag_Null);
	putLength(0);
    }

    /**
     * Marshals an object identifier (OID) on the output stream.
     * Corresponds to the ASN.1 "OBJECT IDENTIFIER" construct.
     */
    public void putOID(ObjectIdentifier oid) throws IOException {
	oid.encode(this);
    }

    /**
     * Marshals a sequence on the output stream.  This supports both
     * the ASN.1 "SEQUENCE" (zero to N values) and "SEQUENCE OF"
     * (one to N values) constructs.
     */
    public void putSequence(DerValue[] seq) throws IOException {
	DerOutputStream	bytes = new DerOutputStream();
	int i;

	for (i = 0; i < seq.length; i++)
	    seq[i].encode(bytes);

	write(DerValue.tag_Sequence, bytes);
    }

    /**   
     * Marshals the contents of a set on the output stream without
     * ordering the elements.  Ok for BER encoding, but not for DER
     * encoding. 
     *
     * For DER encoding, use orderedPutSet() or orderedPutSetOf(). 
     */
    public void putSet(DerValue[] set) throws IOException {
	DerOutputStream	bytes = new DerOutputStream();
	int i;

	for (i = 0; i < set.length; i++)
	    set[i].encode(bytes);

	write(DerValue.tag_Set, bytes);
    }

    /**   
     * Marshals the contents of a set on the output stream.  Sets
     * are semantically unordered, but DER requires that encodings of
     * set elements be sorted into ascending lexicographical order
     * before being output.  Hence sets with the same tags and
     * elements have the same DER encoding.
     *
     * This method supports the ASN.1 "SET OF" construct, but not
     * "SET", which uses a different order.  
     */
    public void putOrderedSetOf(byte tag, DerEncoder[] set) throws IOException {
	putOrderedSet(tag, set, lexOrder);
    }

    /**   
     * Marshals the contents of a set on the output stream.  Sets
     * are semantically unordered, but DER requires that encodings of
     * set elements be sorted into ascending tag order
     * before being output.  Hence sets with the same tags and
     * elements have the same DER encoding.
     *
     * This method supports the ASN.1 "SET" construct, but not
     * "SET OF", which uses a different order.  
     */
    public void putOrderedSet(byte tag, DerEncoder[] set) throws IOException {
	putOrderedSet(tag, set, tagOrder);
    }

    /**
     *  Lexicographical order comparison on byte arrays, for ordering
     *  elements of a SET OF objects in DER encoding.
     */
    private static ByteArrayLexOrder lexOrder = new ByteArrayLexOrder();

    /**
     *  Tag order comparison on byte arrays, for ordering elements of 
     *  SET objects in DER encoding.
     */
    private static ByteArrayTagOrder tagOrder = new ByteArrayTagOrder();

    /**   
     * Marshals a the contents of a set on the output stream with the 
     * encodings of its sorted in increasing order.
     *
     * @param order the order to use when sorting encodings of components.
     */
    private void putOrderedSet(byte tag, DerEncoder[] set, 
			       Comparator order) throws IOException {
	DerOutputStream[] streams = new DerOutputStream[set.length];

	for (int i = 0; i < set.length; i++) {
	    streams[i] = new DerOutputStream();
	    set[i].derEncode(streams[i]);
	}

	// order the element encodings
	byte[][] bufs = new byte[streams.length][];
	for (int i = 0; i < streams.length; i++) {
	    bufs[i] = streams[i].toByteArray();
	}
	Arrays.sort(bufs, order);

	DerOutputStream	bytes = new DerOutputStream();
	for (int i = 0; i < streams.length; i++) {
	    bytes.write(bufs[i]);
	}
	write(tag, bytes);

    }

    /**
     * XXX what character set is this?
     */
    public void putPrintableString(String s) throws IOException {
	write(DerValue.tag_PrintableString);
	putLength(s.length());
	for (int i = 0; i < s.length(); i++)
	    write((byte) s.charAt(i));
    }

//    /*
//     * T61 is an 8 bit extension to ASCII, escapes e.g. to Japanese
//     */
//    public void putT61String(String s) throws IOException {
//	// XXX IMPLEMENT ME
//
//	throw new IOException("DerOutputStream.putT61String() NYI");
//    }

    /**
     * Marshals a string which is consists of IA5(ASCII) characters
     */
    public void putIA5String(String s) throws IOException {
	write(DerValue.tag_IA5String);
	putLength(s.length());
	for (int i = 0; i < s.length(); i++)
	    write((byte) s.charAt(i));
    }

    /**
     * Marshals a DER UTC time/date value.
     *
     * <P>YYMMDDhhmmss{Z|+hhmm|-hhmm} ... emits only using Zulu time
     * and with seconds (even if seconds=0) as per IETF-PKIX partI.
     */
    public void putUTCTime(Date d) throws IOException {
	/*
	 * Format the date.
	 */
	TimeZone tz = TimeZone.getTimeZone("GMT");
        String pattern = "yyMMddHHmmss'Z'";
        SimpleDateFormat sdf = new SimpleDateFormat(pattern);
        sdf.setTimeZone(tz);
        byte[] utc = (sdf.format(d)).getBytes();
	/*
	 * Write the formatted date.
	 */
	write(DerValue.tag_UtcTime);
	putLength(utc.length);
	write(utc);
    }

    /**
     * Marshals a DER Generalized Time/date value.
     *   
     * <P>YYYYMMDDhhmmss{Z|+hhmm|-hhmm} ... emits only using Zulu time
     * and with seconds (even if seconds=0) as per IETF-PKIX partI.
     */  
    public void putGeneralizedTime(Date d) throws IOException {
        /*
         * Format the date.
         */
	TimeZone tz = TimeZone.getTimeZone("GMT");
        String pattern = "yyyyMMddHHmmss'Z'";
        SimpleDateFormat sdf = new SimpleDateFormat(pattern);
        sdf.setTimeZone(tz);
        byte[] gt = (sdf.format(d)).getBytes();
	/*
	 * Write the formatted date.
	 */
        write(DerValue.tag_GeneralizedTime);
        putLength(gt.length);
        write(gt);
    }

    /**
     * Put the encoding of the length in the stream.
     *   
     * @params len the length of the attribute.
     * @exception IOException on writing errors.
     */  
    public void putLength(int len) throws IOException {
	if (len < 128) {
	    write((byte)len);

	} else if (len < (1 << 8)) {
	    write((byte)0x081);
	    write((byte)len);

	} else if (len < (1 << 16)) {
	    write((byte)0x082);
	    write((byte)(len >> 8));
	    write((byte)len);

	} else if (len < (1 << 24)) {
	    write((byte)0x083);
	    write((byte)(len >> 16));
	    write((byte)(len >> 8));
	    write((byte)len);

	} else {
	    write((byte)0x084);
	    write((byte)(len >> 24));
	    write((byte)(len >> 16));
	    write((byte)(len >> 8));
	    write((byte)len);
	}
    }

    /**
     * Put the tag of the attribute in the stream.
     *   
     * @params class the tag class type, one of UNIVERSAL, CONTEXT,
     *                            APPLICATION or PRIVATE
     * @params form if true, the value is constructed, otherwise it is
     * primitive.
     * @params val the tag value
     */  
    public void putTag(byte tagClass, boolean form, byte val) {
        byte tag = (byte)(tagClass | val);
        if (form) {
            tag |= (byte)0x20;
        }
        write(tag);
    }

    /**
     *  Write the current contents of this <code>DerOutputStream</code>
     *  to an <code>OutputStream</code>.
     *
     *  @exception IOException on output error.
     */
    public void derEncode(OutputStream out) throws IOException {
	out.write(toByteArray());
    }
}
