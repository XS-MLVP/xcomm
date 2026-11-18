"""Pin value/Set contracts while optimizing Python attribute and scalar dispatch."""
import ctypes
import enum
import unittest

import xspcomm as xs


def legacy_set(data, value):
    """Pre-optimization Python Set, using the unchanged overloaded binding."""
    if isinstance(value, bytes):
        data.SetBytes(value)
        return data
    if data.W() > 64 and type(value) is int:
        data.SetBytes(value.to_bytes((data.W() + 15) // 8, "little", signed=True))
        return data
    return xs.XData_old_Set(data, value)


def legacy_value_set(data, value):
    if type(value) is bool:
        return legacy_set(data, 1 if value else 0)
    width = data.W()
    if type(value) is int:
        if width <= 64:
            return legacy_set(data, value)
        data.SetBytes(value.to_bytes((data.W() + 15) // 8, "little", signed=True))
    else:
        return legacy_set(data, value)


def snapshot(data):
    return (data.value, data.String(), data.XMask(), data.DataValid())


class IntSubclass(int):
    pass


class IntEnum(enum.IntEnum):
    ONE = 1


class IntConvertible:
    def __int__(self):
        return 7

    def __index__(self):
        return 7


class TracedData(xs.XData):
    def __init__(self, width):
        super().__init__(width, xs.XData.InOut)
        self.calls = []

    def W(self):
        self.calls.append("W")
        return super().W()

    def Set(self, value):
        self.calls.append("Set")
        return super().Set(value)

    def SetBytes(self, value):
        self.calls.append("SetBytes")
        return super().SetBytes(value)


class XDataValueTests(unittest.TestCase):
    def test_value_uses_a_property_without_global_attribute_interception(self):
        self.assertIsInstance(xs.XData.value, property)
        self.assertIs(xs.XData.__getattribute__, xs.XData_old__getattribute__)
        self.assertIs(xs.XData.__setattr__, xs.XData_old__setattr__)
        data = xs.XData(8, xs.XData.InOut)
        data.description = "ordinary Python attribute"
        data.mName = "native member"
        self.assertEqual(data.description, "ordinary Python attribute")
        self.assertEqual(data.mName, "native member")

    def test_instance_dictionary_cannot_shadow_the_value_property(self):
        data = xs.XData(8, xs.XData.InOut)
        data.__dict__["value"] = "not a signal value"
        data.value = 7
        self.assertEqual(data.value, 7)
        self.assertEqual(data.__dict__["value"], "not a signal value")

    def test_subclasses_can_delegate_attribute_hooks_to_the_property(self):
        class HookedData(xs.XData):
            def __getattribute__(self, name):
                return super().__getattribute__(name)

            def __setattr__(self, name, value):
                return super().__setattr__(name, value)

        data = HookedData(64, xs.XData.InOut)
        data.value = 0x123456789abcdef0
        self.assertEqual(data.value, 0x123456789abcdef0)
        self.assertEqual(data.Set(-1).CSelf(), data.CSelf())
        self.assertEqual(data.value, (1 << 64) - 1)

    def test_redefining_value_in_a_subclass_uses_normal_descriptor_lookup(self):
        # Unlike the old unconditional __getattribute__ interception, a subclass
        # can now override value. Pin this intentional Python-level difference.
        class CustomValue(xs.XData):
            @property
            def value(self):
                return ("custom", xs.XData.value.fget(self))

            @value.setter
            def value(self, value):
                xs.XData.value.fset(self, value)

        data = CustomValue(8, xs.XData.InOut)
        data.value = 7
        self.assertEqual(data.value, ("custom", 7))

    def test_scalar_and_integer_boundaries_match_the_old_dispatch(self):
        values = [
            0, 1, -1, -(1 << 31), (1 << 31) - 1, (1 << 32) - 1,
            1 << 32, (1 << 63) - 1, -(1 << 63), (1 << 64) - 1,
            -(1 << 63) - 1, 1 << 64, (1 << 100) | 7,
            -(1 << 300), 1 << 300, True, False,
        ]
        for width in [0, 1, 7, 8, 16, 31, 32, 33, 63, 64, 65, 96, 128, 257]:
            # Scalars have four legal logic states, not an integer bit vector.
            current = [0, 1, 2, 3, True, False] if width == 0 else values
            for value in current:
                for operation in ["Set", "value"]:
                    with self.subTest(width=width, value=value, operation=operation):
                        self.compare_legacy(width, value, operation)

    def test_non_int_arguments_keep_the_old_overload_path(self):
        values = [
            "0x1234", "0b10xz", b"\x12\x34\x56\x78",
            bytearray(b"\x12\x34"), IntSubclass(7), IntEnum.ONE,
            ctypes.c_int64(7), IntConvertible(), 3.0, 3.5, None,
        ]
        for width in [8, 64, 65, 128]:
            for value in values:
                for operation in ["Set", "value"]:
                    with self.subTest(width=width, type=type(value), operation=operation):
                        self.compare_legacy(width, value, operation)

    def test_numpy_scalars_keep_the_old_conversion_rules_when_available(self):
        try:
            import numpy as np
        except ImportError:
            self.skipTest("NumPy is optional")
        values = [np.int8(-1), np.uint8(255), np.int32(-7), np.uint32(1 << 31),
                  np.int64(-(1 << 63)), np.uint64((1 << 64) - 1),
                  np.float32(3.0), np.float64(3.5)]
        for width in [8, 64, 128]:
            for value in values:
                for operation in ["Set", "value"]:
                    with self.subTest(width=width, type=type(value), operation=operation):
                        self.compare_legacy(width, value, operation)

    def compare_legacy(self, width, value, operation):
        old, new = (xs.XData(width, xs.XData.InOut) for _ in range(2))
        legacy = legacy_set if operation == "Set" else legacy_value_set
        try:
            expected = legacy(old, value)
        except (TypeError, ValueError, OverflowError) as error:
            with self.assertRaises(type(error)):
                if operation == "Set":
                    new.Set(value)
                else:
                    new.value = value
            self.assertEqual(snapshot(new), snapshot(old))
            return
        if operation == "Set":
            result = new.Set(value)
            self.assertIsInstance(expected, xs.XData)
            self.assertIsInstance(result, xs.XData)
            self.assertEqual(result.CSelf(), new.CSelf())
        else:
            new.value = value
        self.assertEqual(snapshot(new), snapshot(old))

    def test_copying_an_xdata_preserves_four_state_values(self):
        for width in [8, 64, 128]:
            # XData-to-XData assignment requires equal widths in the native API.
            source = xs.XData(width, xs.XData.InOut)
            source.value = "0x123456789abcdef0123456789abcxz01"
            for operation in ["Set", "value"]:
                with self.subTest(width=width, operation=operation):
                    self.compare_legacy(width, source, operation)

    def test_set_returns_the_native_object_for_chaining(self):
        data = xs.XData(64, xs.XData.InOut)
        for value in [0, (1 << 64) - 1, -1, -(1 << 63)]:
            result = data.Set(value)
            self.assertIsInstance(result, xs.XData)
            self.assertEqual(result.CSelf(), data.CSelf())
            self.assertEqual(result.Set(7).CSelf(), data.CSelf())
            self.assertEqual(data.value, 7)

    def test_alias_reinit_changes_width_for_every_proxy(self):
        data = xs.XData(8, xs.XData.InOut)
        port = xs.XPort()
        port.Add("data", data)
        self.assertEqual(data.value, 0)
        alias = port.Get("data")
        self.assertEqual(alias.CSelf(), data.CSelf())
        alias.ReInit(128, xs.XData.InOut)
        alias.value = (1 << 100) | 7
        self.assertEqual(data.W(), 128)
        self.assertEqual(data.value, (1 << 100) | 7)
        data.ReInit(8, xs.XData.InOut)
        alias.value = 0xa5
        self.assertEqual(alias.W(), 8)
        self.assertEqual(data.value, 0xa5)

    def test_value_keeps_subclass_set_and_setbytes_extension_points(self):
        for width in [8, 128]:
            for value in [3, True, "0b1", b"\x01"]:
                with self.subTest(width=width, value=value):
                    data = TracedData(width)
                    data.value = value
                    if type(value) is bool:
                        expected = (["Set", "W"] if width <= 64 else
                                    ["Set", "W", "W", "SetBytes"])
                    elif type(value) is int and width > 64:
                        expected = ["W", "W", "SetBytes"]
                    elif isinstance(value, bytes):
                        expected = ["W", "Set", "SetBytes"]
                    else:
                        expected = ["W", "Set", "W"]
                    self.assertEqual(data.calls, expected)
                    self.assertIsInstance(data.value, int)

    def test_native_reads_do_not_cache_values(self):
        memory = ctypes.c_uint64(0)
        data = xs.XData(64, xs.XData.Out)
        data.BindNativeData(ctypes.addressof(memory))
        for value in [0, 1, 1 << 63, (1 << 64) - 1]:
            memory.value = value
            self.assertEqual(data.value, value)
            self.assertEqual(data.U(), value)

    def test_native_write_modes_and_immset_are_unchanged(self):
        for mode in [xs.XData.Rise, xs.XData.Fall, xs.XData.Imme]:
            with self.subTest(mode=mode):
                memory = ctypes.c_uint64(0)
                data = xs.XData(64, xs.XData.In)
                data.BindNativeData(ctypes.addressof(memory))
                data.SetWriteMode(mode)
                data.value = 0x123456789abcdef0
                if mode != xs.XData.Imme:
                    self.assertEqual(memory.value, 0)
                    wrong = data.WriteOnFall if mode == xs.XData.Rise else data.WriteOnRise
                    right = data.WriteOnRise if mode == xs.XData.Rise else data.WriteOnFall
                    wrong()
                    self.assertEqual(memory.value, 0)
                    right()
                self.assertEqual(memory.value, 0x123456789abcdef0)
                data.ImmSet(-1)
                self.assertEqual(memory.value, (1 << 64) - 1)
                self.assertEqual(data.GetWriteMode(), mode)

    def test_dpi_read_and_write_still_use_the_bound_functions(self):
        class Word(ctypes.Structure):
            _fields_ = [("aval", ctypes.c_uint32), ("bval", ctypes.c_uint32)]

        values = [0, 0]
        writes = []

        @ctypes.CFUNCTYPE(None, ctypes.c_void_p)
        def read(pointer):
            words = ctypes.cast(pointer, ctypes.POINTER(Word))
            for index in range(2):
                words[index].aval = values[index]
                words[index].bval = 0

        @ctypes.CFUNCTYPE(None, ctypes.c_void_p)
        def write(pointer):
            words = ctypes.cast(pointer, ctypes.POINTER(Word))
            values[:] = [words[0].aval, words[1].aval]
            writes.append(tuple(values))

        data = xs.XData(64, xs.XData.InOut)
        data.BindDPIPtr(ctypes.cast(read, ctypes.c_void_p).value,
                        ctypes.cast(write, ctypes.c_void_p).value)
        data.AsImmWrite()
        data.value = 0x123456789abcdef0
        self.assertEqual(writes[-1], (0x9abcdef0, 0x12345678))
        values[:] = [7, 8]
        self.assertEqual(data.value, (8 << 32) | 7)

    def test_change_callbacks_match_the_old_dispatch(self):
        class Recorder(xs.cb_void_bool_XDatap_u43_voidp):
            def __init__(self):
                super().__init__()
                self.set_force_callable()
                self.events = []

            def call(self, valid, signal, value, args):
                self.events.append((valid, signal.value, signal.XMask(), value))

        for width in [8, 64, 128]:
            old, new = (xs.XData(width, xs.XData.InOut) for _ in range(2))
            old_recorder, new_recorder = Recorder(), Recorder()
            old.OnChange(old_recorder)
            new.OnChange(new_recorder)
            for value in [1, 1, -1, "0b10xz", b"\x12\x34", 0]:
                legacy_value_set(old, value)
                new.value = value
            with self.subTest(width=width):
                self.assertEqual(new_recorder.events, old_recorder.events)

    def test_slice_writes_preserve_the_parent_and_lifetime_checks(self):
        parent = xs.XData(128, xs.XData.InOut)
        parent.value = 1
        child = parent.SubDataRefRaw(60, 16)
        child.value = 0xabcd
        self.assertEqual(parent.value, (0xabcd << 60) | 1)
        parent.ReInit(128, xs.XData.InOut)
        # ReInit without a layout and with the same width keeps this slice valid.
        parent.ReInit(64, xs.XData.InOut)
        with self.assertRaises(RuntimeError):
            _ = child.value


if __name__ == "__main__":
    unittest.main()
