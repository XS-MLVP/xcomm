package com.xspcomm.tests;

import com.xspcomm.*;
import java.math.BigInteger;

public class test_xarray {
    public static void main(String[] args) {
        xspcomm.init();
        XByteBuffer memory = new XByteBuffer(18);
        XDataArray array = new XDataArray(8,
            new XIntVector(new int[]{-1, 3, 4}), new XIntVector(new int[]{1, 2, 6}));
        XUInt64Vector strides = new XUInt64Vector(new BigInteger[]{
            BigInteger.valueOf(6), BigInteger.valueOf(3), BigInteger.ONE});
        array.BindNativeData(memory.BaseAddr(), strides);
        assert array.Size().intValue() == 18;
        assert array.Shape().get(2) == 3;
        assert array.Strides().get(0).intValue() == 6;
        XIntVector index = new XIntVector(new int[]{0, 2, 5});
        assert array.FlatIndex(index).intValue() == 10;
        XData leaf = array.At(index);
        assert leaf.IsArrayBound();
        assert leaf.CSelf().equals(array.AtFlat(BigInteger.TEN).CSelf());
        leaf.AsImmWrite();
        leaf.Set(0xa5);
        assert (memory.AsBytes()[7] & 255) == 0xa5;
        array.delete();
        leaf.Set(0x5a);
        assert (memory.AsBytes()[7] & 255) == 0x5a;
        leaf.delete();
        memory.delete();
    }
}
