"""Exercise array ownership, bounds, strides, and indexed DPI through SWIG."""
import ctypes
import gc
import unittest

import xspcomm as xs


class XArrayTests(unittest.TestCase):
    def test_native_multidimensional_view(self):
        memory = (ctypes.c_ubyte * 18)()
        self.assertFalse(hasattr(xs, "XDataArray"))
        array = xs.XArray.FromNative(8, [-1, 3, 4], [1, 2, 6], ctypes.addressof(memory), [6, 3, 1])
        self.assertEqual(tuple(array.Shape()), (3, 2, 3))
        self.assertEqual(array.Size(), 3)
        self.assertEqual(array.Left(), -1)
        leaf = array[0][2][5]
        self.assertEqual(leaf.CSelf(), array.Array(0).Array(2).Data(5).CSelf())
        self.assertTrue(leaf.IsArrayBound())
        leaf.AsImmWrite()
        leaf.value = 0xa5
        self.assertEqual(memory[7], 0xa5)
        with self.assertRaises(IndexError):
            array[0][1][5]
        with self.assertRaises(ValueError):
            array.Data(0)
        with self.assertRaises(IndexError):
            array.At(2)
        del array
        gc.collect()
        leaf.value = 0x5a
        self.assertEqual(memory[7], 0x5a)

    def test_indexed_scalar_dpi(self):
        values = [0, 1, 0, 1]

        @ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.POINTER(ctypes.c_ubyte))
        def getter(index, result):
            result[0] = values[index]

        @ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_ubyte)
        def setter(index, value):
            values[index] = value

        array = xs.XArray.FromDPI(0, [2, -1], [3, 0],
            ctypes.cast(getter, ctypes.c_void_p).value, ctypes.cast(setter, ctypes.c_void_p).value)
        first, last = array[2][-1], array[3][0]
        first.AsImmWrite()
        last.AsImmWrite()
        first.value, last.value = 1, 0
        self.assertEqual(values, [1, 1, 0, 0])
        self.assertEqual(first.GetBackendKind(), xs.XDataBackendKind_DPI)

    def test_signal_cfg_explicit_layout(self):
        memory = (ctypes.c_ubyte * 140)()
        config = """variables:
  - name: mem
    offset: 0
    rtl_width: 65
    mem_bytes: 12
    array_size: 6
"""
        cfg = xs.XSignalCFG(config + "    array_dimensions: [2, 3]\n"
                               "    array_strides: [80, 24]\n", ctypes.addressof(memory))
        view = cfg.NewXArray("mem")
        leaf = cfg.NewXData("mem", 5)
        leaf.AsImmWrite()
        leaf.value = 77
        self.assertEqual(memory[128], 77)
        self.assertEqual(view[1][2].value, 77)
        self.assertEqual(cfg.NewXArray("mem")[1][2].value, 77)
        for index in (-2, 6):
            with self.assertRaises(IndexError):
                cfg.NewXData("mem", index)
        self.assertIsNone(cfg.NewXData("missing", 0))
        for layout in ("", "    array_dimensions: [6]\n", "    array_strides: [12]\n"):
            with self.subTest(layout=layout):
                incomplete = xs.XSignalCFG(config + layout, ctypes.addressof(memory))
                self.assertIsNone(incomplete.NewXArray("mem"))


if __name__ == "__main__":
    unittest.main()
