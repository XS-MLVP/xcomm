import json
import gc
import unittest

import xspcomm as xsp


def bits(width=0):
    return dict(kind="bits", packed=True, width=width)


def array(left, right, element, *, packed):
    width = (abs(left - right) + 1) * (element["width"] or 1) if packed else 0
    return dict(kind="array", packed=packed, width=width, left=left, right=right, element=element)


class StructTests(unittest.TestCase):
    def test_checked_xview_conversions_and_explicit_packed_access(self):
        self.assertFalse(hasattr(xsp, "XDataView"))
        layout = dict(kind="struct", packed=True, width=8, fields=[
            dict(name="data", offset=0, type=bits(8)),
        ])
        signal = xsp.XData(8, xsp.XData.In)
        signal.mName = "packet"
        view = xsp.XView.FromData(signal, json.dumps(layout))
        self.assertTrue(view.HasValue())
        self.assertEqual(view.PackedData().CSelf(), signal.CSelf())
        self.assertEqual(view.Struct().Data("data").CSelf(), signal.CSelf())
        with self.assertRaisesRegex(ValueError, "actual=struct, requested=array"):
            view.Array()
        with self.assertRaisesRegex(ValueError, "actual=struct, requested=bits"):
            view.Data()
        with self.assertRaisesRegex(ValueError, "empty XView"):
            xsp.XView().Struct()
        self.assertFalse(xsp.XView().HasValue())
        del signal
        gc.collect()
        with self.assertRaisesRegex(RuntimeError, "destroyed"):
            view.Struct()

    def test_packed_parent_and_nested_arrays(self):
        matrix = array(1, 0, array(-2, 1, bits(), packed=True), packed=True)
        signal = xsp.XData(8, xsp.XData.In)
        signal.value = 0xA5
        view = signal.ArrayView(json.dumps(matrix))
        self.assertEqual(view.shape, (2, 4))
        self.assertEqual(tuple(view.indices), (1, 0))
        self.assertEqual(view[1][-2].value, 1)
        self.assertEqual(view[1][-1].value, 0)
        view[1][-1].value = 1
        self.assertEqual(signal.value, 0xE5)
        self.assertIs(view[1], view[1])
        self.assertIs(view[1][-1], view[1][-1])
        with self.assertRaises(IndexError):
            view[-1]

    def test_independent_unpacked_fields_and_reserved_names(self):
        layout = dict(kind="struct", packed=False, width=0, fields=[
            dict(name="Has", type=bits(8)),
            dict(name="rows", type=array(3, 2, bits(8), packed=False)),
        ])
        port = xsp.XPort()
        fields = [xsp.XData(8, xsp.XData.In) for _ in range(3)]
        for name, field in zip(("p.Has", "p.rows[3]", "p.rows[2]"), fields):
            port.Add(name, field)
        view = xsp.XView.FromPort(port, "p", json.dumps(layout)).Struct()
        self.assertEqual(view.fields, ("Has", "rows"))
        self.assertEqual(view["Has"].CSelf(), fields[0].CSelf())
        view["Has"].value = 0x31
        view.rows[2].value = 0x52
        self.assertEqual(fields[0].value, 0x31)
        self.assertEqual(fields[2].value, 0x52)
        self.assertIs(view.rows, view.rows)
        self.assertEqual(view.rows.shape, (2,))
        with self.assertRaises(AttributeError):
            view.rows = fields[0]
        with self.assertRaises(ValueError):
            view.PackedData()


if __name__ == "__main__":
    unittest.main()
