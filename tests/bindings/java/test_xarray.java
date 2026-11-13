package com.xspcomm.tests;

import com.xspcomm.*;
import java.math.BigInteger;

public class test_xarray {
    public static void main(String[] args) {
        xspcomm.init();
        try {
            Class.forName("com.xspcomm.XDataArray");
            throw new AssertionError("Removed array type survived SDK staging");
        } catch (ClassNotFoundException expected) {}
        XByteBuffer memory = new XByteBuffer(18);
        XUInt64Vector strides = new XUInt64Vector(new BigInteger[]{
            BigInteger.valueOf(6), BigInteger.valueOf(3), BigInteger.ONE});
        XArray array = XArray.FromNative(8,
            new XIntVector(new int[]{-1, 3, 4}), new XIntVector(new int[]{1, 2, 6}), memory.BaseAddr(), strides);
        assert array.Size().intValue() == 3;
        assert array.Shape().get(2) == 3;
        XData leaf = array.Array(0).Array(2).Data(5);
        assert leaf.IsArrayBound();
        assert leaf.CSelf().equals(array.Array(0).Array(2).Data(5).CSelf());
        leaf.AsImmWrite();
        leaf.Set(0xa5);
        assert (memory.AsBytes()[7] & 255) == 0xa5;
        array.delete();
        leaf.Set(0x5a);
        assert (memory.AsBytes()[7] & 255) == 0x5a;
        leaf.delete();
        XSignalCFG cfg = new XSignalCFG("variables:\n  - name: mem\n    offset: 0\n"
            + "    rtl_width: 8\n    mem_bytes: 1\n    array_size: 6\n"
            + "    array_dimensions: [2, 3]\n    array_strides: [3, 1]\n", memory.BaseAddr());
        XArray internal = cfg.NewXArray("mem");
        assert internal.Shape().get(0) == 2;
        assert internal.Size().intValue() == 2;
        XData selected = internal.Array(1).Data(2);
        assert selected.CSelf().equals(cfg.NewXArray("mem").Array(1).Data(2).CSelf());
        selected.AsImmWrite(); selected.Set(99);
        assert (memory.AsBytes()[5] & 255) == 99;
        selected.delete(); internal.delete(); cfg.delete();
        memory.delete();
    }
}
