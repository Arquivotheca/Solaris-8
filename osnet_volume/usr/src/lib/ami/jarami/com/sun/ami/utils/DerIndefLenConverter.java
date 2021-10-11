/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)DerIndefLenConverter.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.utils;

import java.io.IOException;
import java.util.Vector;

/**
 * A package private utility class to convert indefinite length DER
 * encoded byte arrays to definite length DER encoded byte arrays.
 *
 * This assumes that the basic data structure is "tag, length, value"
 * triplet. In the case where the length is "indefinite", terminating
 * end-of-contents bytes are expected.
 *
 * @author Hemma Prafullchandra
 * @version 1.3 98/08/24
 */
class DerIndefLenConverter {

    private static final int TAG_MASK            = 0x1f; // bits 5-1
    private static final int FORM_MASK           = 0x20; // bits 6
    private static final int CLASS_MASK          = 0xC0; // bits 8 and 7

    private static final int LEN_LONG            = 0x80; // bit 8 set
    private static final int LEN_MASK            = 0x7f; // bits 7 - 1
    private static final int SKIP_EOC_BYTES      = 2;

    private byte[] data, newData;
    private int newDataPos, dataPos, dataSize, index;

    private Vector ndefsList = new Vector();
    private Vector eocList = new Vector();

    private boolean isEOC(int tag) {
        return (((tag & TAG_MASK) == 0x00) &&  // EOC
                ((tag & FORM_MASK) == 0x00) && // primitive
                ((tag & CLASS_MASK) == 0x00)); // universal
    }

    // if bit 8 is set then it implies either indefinite length or long form
    static boolean isLongForm(int lengthByte) {
        return ((lengthByte & LEN_LONG) == LEN_LONG);
    }

    /*
     * Default package private constructor
     */
    DerIndefLenConverter() { }

    /**
     * Checks whether the given length byte is of the form
     * <em>Indefinite</em>.
     *
     * @param lengthByte the length byte from a DER encoded
     *        object.
     * @return true if the byte is of Indefinite form otherwise
     *         returns false.
     */
    static boolean isIndefinite(int lengthByte) {
        return (isLongForm(lengthByte) && ((lengthByte & LEN_MASK) == 0));
    }

    /**
     * Parse the tag and if it is an end-of-contents tag then
     * add the current position to the <code>eocList</code> vector.
     */
    private void parseTag() {
        if (dataPos == dataSize)
            return;
        if (isEOC(data[dataPos]) && (data[dataPos + 1] == 0)) {
            eocList.add(index, new Integer(dataPos));
            ndefsList.add(index++, new Integer(0));
        }
        dataPos++;
    }

    /**
     * Write the tag and if it is an end-of-contents tag
     * then skip the tag and its 1 byte length of zero.
     */
    private void writeTag() {
        if (dataPos == dataSize)
            return;
        int tag = data[dataPos++];
        if (isEOC(tag) && (data[dataPos] == 0)) {
            dataPos++;	// skip length
            writeTag();
        } else
            newData[newDataPos++] = (byte)tag;
    }

    /**
     * Parse the length and if it is an indefinite length then add
     * the current position to the <code>ndefsList</code> vector.
     */
    private int parseLength() throws IOException {
        int curLen = 0;
        if (dataPos == dataSize)
            return curLen;
        int lenByte = data[dataPos++] & 0xff;
        if (isIndefinite(lenByte)) {
            ndefsList.add(index, new Integer(dataPos));
            eocList.add(index++, new Integer(0));
            return curLen;
        }
        if (isLongForm(lenByte)) {
            lenByte &= LEN_MASK;
            if (lenByte > 4)
                throw new IOException("Too much data");
            if ((dataSize - dataPos) < (lenByte + 1))
                throw new IOException("Too little data");
            for (int i = 0; i < lenByte; i++)
                curLen = (curLen << 8) + (data[dataPos++] & 0xff);
        } else
           curLen = (lenByte & LEN_MASK);
        return curLen;
    }

    /**
     * Write the length and if it is an indefinite length
     * then calculate the definite length from the positions
     * of the indefinite length and its matching EOC terminator.
     * Then, write the value.
     */
    private void writeLengthAndValue() throws IOException {
        if (dataPos == dataSize)
           return;
        int curLen = 0;
        int lenByte = data[dataPos++] & 0xff;
        if (isIndefinite(lenByte)) {
            int ndefPos = ((Integer)ndefsList.get(index)).intValue();
            if (ndefPos == dataPos) {
                curLen = ((Integer)eocList.get(index)).intValue() - ndefPos;
                index++;
                writeLength(curLen);
                return;
            } else
                throw new IOException("Internal corruption, cannot" +
                                      " convert to definite length" +
                                      " encoding");
        }
        if (isLongForm(lenByte)) {
            lenByte &= LEN_MASK;
            for (int i = 0; i < lenByte; i++)
                curLen = (curLen << 8) + (data[dataPos++] & 0xff);
        } else
            curLen = (lenByte & LEN_MASK);
        writeLength(curLen);
        writeValue(curLen);
    }

    private void writeLength(int curLen) {
        if (curLen < 128) {
            newData[newDataPos++] = (byte)curLen;

        } else if (curLen < (1 << 8)) {
            newData[newDataPos++] = (byte)0x81;
            newData[newDataPos++] = (byte)curLen;

        } else if (curLen < (1 << 16)) {
            newData[newDataPos++] = (byte)0x82;
            newData[newDataPos++] = (byte)(curLen >> 8);
            newData[newDataPos++] = (byte)curLen;

        } else if (curLen < (1 << 24)) {
            newData[newDataPos++] = (byte)0x83;
            newData[newDataPos++] = (byte)(curLen >> 16);
            newData[newDataPos++] = (byte)(curLen >> 8);
            newData[newDataPos++] = (byte)curLen;

        } else {
            newData[newDataPos++] = (byte)0x84;
            newData[newDataPos++] = (byte)(curLen >> 24);
            newData[newDataPos++] = (byte)(curLen >> 16);
            newData[newDataPos++] = (byte)(curLen >> 8);
            newData[newDataPos++] = (byte)curLen;
        }
    }

    /**
     * Parse the value;
     */
    private void parseValue(int curLen) {
        dataPos += curLen;
    }

    /**
     * Write the value;
     */
    private void writeValue(int curLen) {
        for (int i=0; i < curLen; i++)
            newData[newDataPos++] = data[dataPos++];
    }

    /*
     * Match the EOC with the Indefinite-length positions
     * taking into account the EOC bytes.
     */
    private void calcLens(int index) {
        int half = index/2;
        boolean notDone = true;
        int ndefPos, eocPos;
        Integer eocVal;

        while (notDone) {
            for (int i=0; i < index; i++) {
                ndefPos = ((Integer)ndefsList.get(i)).intValue();
                if (ndefPos == 0) {
                    eocList.set((i-1), eocList.get(i));
                    eocList.remove(i);
                    ndefsList.remove(i);
                    if (i == half) {
                        notDone = false;
                        break;
                    }
                }
            }
        }
        notDone = true;
        while (notDone) {
            for (int i=0; i < half; i++) {
                ndefPos = ((Integer)ndefsList.get(i)).intValue();
                eocPos = ((Integer)eocList.get(i)).intValue();
                if (ndefPos != 0 && eocPos != 0) {
                    eocVal = new Integer(
                             ((Integer)eocList.get(half)).intValue()
                             - SKIP_EOC_BYTES);
                    eocList.remove(half);
                    ndefsList.remove(half);
                    int where = i - 1;
                    eocList.set(where, eocVal);
                    if (where == 0)
                        notDone = false;
                    break;
                }
            }
        }
    }

    /**
     * Converts a indefinite length DER encoded byte array to
     * a definte length DER encoding.
     *
     * @param indefData the byte array holding the indefinite
     *        length encoding.
     * @return the byte array containing the definite length
     *         DER encoding.
     * @exception IOException on parsing or re-writing errors.
     */
    byte[] convert(byte[] indefData) throws IOException {
        data = indefData;
        dataPos=0; index=0;
        dataSize = data.length;
        int len=0;

        // parse and set up the vectors of all the indefinite-lengths
        // and the EOCs
        while (dataPos < dataSize) {
            parseTag();
            len = parseLength();
            parseValue(len);
        }
        // align the two vectors
        calcLens(index);

        /*
         * The length of the new byte array is calculated as:
         * Original buffer has "dataSize" bytes of which the following is:
         *   variable = index/2 * tagIndefiniteByte +
         *              index/2 * tagEOCByte + index/2 * eocValueByte
         *
         * (EOC is 2 bytes, 1 byte for tag and 1 byte for value/length).
         * ("index" is number of indefinite-length tagged bytes
         *  found + number of EOC tagged bytes found).
         *
         * In the new buffer the EOC bytes are removed. The indefinite length
         * byte is replaced with 1 + n number of bytes. Where n is determined
         * by the size of the data value. In DerValue we only support n <= 4;
         * so n can be at most 4.
         * There are at most index/2 * (1 + 4) bytes needed to write out
         * the new lengths. With the removal of EOC bytes and accounting
         * for the 1 byte used for indefinite length, we have,
         * additional-bytes-needed = index/2 * 2 ==> index.
         */
        newData = new byte[dataSize + index];
        dataPos=0; newDataPos=0; index=0;

        // write out the new byte array replacing all the indefinite-lengths
        // and EOCs
        while (dataPos < dataSize) {
           writeTag();
           writeLengthAndValue();
        }
        int lenLen=1, valueLen=0;
        int lenByte = newData[lenLen] & 0xff;
        if (isLongForm(lenByte)) {
            lenByte &= LEN_MASK;
            for (; lenByte > 0; lenByte--)
                valueLen = (valueLen << 8) + (newData[++lenLen] & 0xff);
        } else
           valueLen = (lenByte & LEN_MASK);

        byte[] trimmedData = new byte[1 + lenLen + valueLen];
        System.arraycopy(newData, 0, trimmedData, 0, trimmedData.length);
        return trimmedData;
    }
}
