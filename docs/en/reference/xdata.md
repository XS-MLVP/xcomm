# XData: signals and values

Header: [xdata.h](https://github.com/XS-MLVP/xcomm/blob/master/include/xspcomm/xdata.h). XData provides a common readable/writable signal interface with four-state caching, backend bindings and slices. Python exposes `xspcomm.XData`.

## Construction, direction and width

| API | Purpose and contract |
| --- | --- |
| `XData()`; `XData(width, type, name)`; copy constructor; `ReInit(...)` | C++ defaults to scalar InOut. Python requires width and defaults to In. ReInit rebuilds storage; obtain new bit references afterwards. |
| `mName`, `mWidth`, `mIOType`; `W()` | Name, width and direction. Use ReInit to change width. |
| `IsInIO/IsOutIO/IsBiIO`; `AsInIO/AsOutIO/AsBiIO/FlipIOType` | Inspect/set direction. Flip exchanges Input/Output and preserves InOut. |
| `GetBackendKind()`; `IsReadonly()` | Inspect source and backend read-only constraints; direction alone is insufficient. |

`width == 0` represents scalar SV logic; `width == 1` is a one-bit vector. Scalar values `0/1/2/3` mean `0/1/Z/X`. Vectors store 32-bit `aval/bval` words: `(0,0)/(1,0)/(0,1)/(1,1)` encode those four states.

## Values and representations

| API | Purpose and contract |
| --- | --- |
| `Set(value)`; assignment operators | Integers, strings, same-width XData or bytes; truncate excess bits. |
| `ImmSet(value)`; `ImmSetBytes(bytes)` | Temporarily use immediate writing, then restore the write mode. |
| `U()`; `S()`; `B()`; `AsInt32/AsInt64` | C++ reads at most 64 bits; S sign-extends widths 1–64. Python U/value adds full-width integer conversion. |
| `String()`; `AsBinaryString()` | Hexadecimal display and binary display including x/z. String omits 0x and displays unknown bytes as ??. It prints one byte per byte that *starts* below the width, so an 8-bit signal shows two hex digits while a 116-bit one still shows its partial top byte. |
| `GetBytes/GetVU8`; `SetBytes/SetVU8` | Little-endian aval bytes. Vector output is padded to 32-bit words; scalar signals do not use these vector APIs. |
| `GetBvalBytes()`; `XMask()` | Full vector unknown-mask bytes; XMask returns only the low 64 bits. |
| `DataValid()` | Inspect whether the last refreshed cache contains X/Z; does not refresh the backend itself. |
| `Invert()` | Invert values; vector inversion also clears the unknown mask. |

Strings accept `0x...`, `0b...` and `::...`; binary/hexadecimal digits can represent X/Z. To fill a 128-bit vector with X, use `0x` followed by 32 `x` digits. Scalar signals accept a single `x/z`. The current `0b` setter processes the text's left end into low bits, unlike the high-bit-first AsBinaryString display; prefer integers or hexadecimal for numeric assignment.

Refresh with U/GetBytes before checking DataValid. Integer payloads alone do not encode X/Z.

<!-- runnable:python -->
```python
import xspcomm as xs

value = xs.XData(128, xs.XData.InOut)
value.value = (1 << 100) | 7
assert value.U() == (1 << 100) | 7
assert len(value.GetBytes()) == 16
value.value = "0x" + "x" * 32
assert value.GetBvalBytes() == bytes([255]) * 16
assert not value.DataValid()
```

## Write timing

| API | Purpose |
| --- | --- |
| `GetWriteMode/SetWriteMode`; `IsImmWrite/IsRiseWrite/IsFallWrite` | Select Imme, Rise or Fall; default Rise. Output does not accept a write mode. |
| `AsImmWrite/AsRiseWrite/AsFallWrite` | Chainable mode setters. |
| `WriteOnRise/WriteOnFall` | Commit at the matching edge, normally managed by XClock. |
| `WriteDirect()` | Request a backend write without edge-mode selection. |
| `ReadFresh(mode)` | Arrange refresh based on direction and mode, normally via port/clock. |
| `SetIgnoreSameDataWrite(bool)` | Control suppression of identical backend writes; enabled by default. |

Set first changes XData storage. WriteMode determines when the DUT receives it. Add the signal to a clock-managed port or manage commits explicitly.

## Bits and slices

| API | Purpose and contract |
| --- | --- |
| `At(index)`; C++ `operator[]` | Return PinBind. Python indexing reads 0/1/2/3 and assignment updates that bit. |
| `PinBind.Set/Get/AsInt32/AsString` | Read or write one four-state bit. |
| `SubDataRef(start, width, name)` | Borrow `[start, start+width)`; width zero selects one scalar bit. Writes affect the parent, which must remain alive; resizing it invalidates old slices. |
| `SubDataRefRaw(...)` | Raw-pointer form for bindings; manage ownership explicitly. |
| `SetBits(buffer, count, mask, start)` | Byte overloads count bytes; word overloads count 32-bit words. Mask selects aval bits to update; other aval bits survive. Written regions clear bval. |
| `GetBits(buffer, count)` | Read the selected granularity; returns false for unknown data. |

## Bind a backend

| API | Purpose and limits |
| --- | --- |
| `BindDPIRW(read, write)` | DPI-style scalar/vector callbacks with corresponding signatures. |
| `BindDPIPtr(read_addr, write_addr)` | Bind function addresses with the expected ABI. |
| Python `BindDPIName(dut, name)` | Resolve Picker GetDPIHandle accessors and set the name. |
| `BindNativeData(address)` | Borrow native storage and read its initial value; writes follow WriteMode. |
| `BindMixFromUvs(address)` | UVS-specific read adapter. |
| `BindExpr(shared_engine, root)`; `BindConst(uint64)` | Read-only expression/constant; payload is at most 64 bits, zero-extended for wider XData. |
| `BindVPI(...)`; `FromVPI(...)`; `IsVPIBinded()` | Bind simulator handles/get/value/put callbacks; FromVPI creates an object. |
| `AsVPIWriteNoDelay/AsVPIWriteForce/AsVPIWriteRelease`; `SetVPIWriteFlag` | Select VPI write policy. |
| `AsVPIAuto/AsVPIScale/AsVPIInt/AsVPIVector` | Select VPI representation; AsVPIScale is the existing scalar method spelling. |

Native vectors use uint8 for 1–8 bits, uint16 for 9–16 bits, and contiguous uint32 words from 17 bits upwards. Scalar storage is one logic-value byte. Native vectors carry aval, not SV four-state structs. Supply sufficient live storage.

## Fixed unpacked arrays

[xarray.h](https://github.com/XS-MLVP/xcomm/blob/master/include/xspcomm/xarray.h)
provides the same recursive `XArray` interface used by aggregate arrays. Each
dimension accepts its actual RTL index, including descending and negative bounds.
`Left/Right/Size` describe the current dimension; `LeftBounds/RightBounds/Shape`
describe consecutive dimensions. `Array(index)` selects a child array and
`Data(index)` selects an existing packed leaf. Python supports `array[i][j]`.

Create a homogeneous unpacked array and its shared indexed backend with a factory:

- `XArray.FromDPI(width, left, right, getter, setter)` uses `void get(int index, void *value)` and either
  scalar `void set(int index, unsigned char value)` or vector
  `void set(int index, const void *value)`. A null setter is read-only. Total count
  must fit a positive C++ `int`.
- `XArray.FromNative(width, left, right, base, byte_strides)` uses the element at the minimum RTL index
  of every dimension as its base. Strides are outermost first; omitted strides
  mean contiguous storage with the native element representation described above.
  Non-overlapping row-major padding is accepted. Rank, bounds, width, overlap,
  and size/address arithmetic are checked.

Selections lazily create/cache recursive nodes and leaf objects. Shared binding,
flat-index conversion and element addresses are internal implementation details,
not a second public array type. `IsArrayBound()` identifies a leaf using this backend. The leaf stores shared
backend state plus its DPI flat index or a native address resolved at binding;
no per-element read/write `std::function` is installed. Existing write modes,
slices, masks, and change notifications continue to apply. Native vector reads
preserve the existing bval policy; native memory is not a four-state SV buffer.

Array leaves work directly with `ExprEngine` and `XTriggerEngine`, without a
Python proxy or an extra `XPort` registration for sampling. Wide Value conditions
refresh the backend before checking four-state validity, so a leaf recovering
from X/Z can match at the next selected phase even without a change callback.

Backend state outlives the array view when a leaf is retained, but model memory
and DPI libraries are borrowed and must outlive all access. Bind a new array to
change the backing store; rebinding an existing array is rejected. ReInit on an
individual leaf detaches it; standalone backend bindings also detach it. Do not
reinitialize or rebind a cached array leaf while using its original view. Existing
XData copies retain their snapshot-copy semantics, not the array binding.

`XSignalCFG.NewXArray(name)` consumes `array_dimensions` and
`array_strides`, and caches a native array view. Array YAML must provide both
fields: the product of the dimensions must equal `array_size`, and the stride
rank must match the dimension rank. Incomplete layouts are rejected rather than
inferred. The view uses zero-based storage coordinates. Indexed `NewXData` uses
the same shared backend, but its legacy index is a flat storage position.

Array layouts use native ABI 6. The DPI/native paths target Verilator 5.026 and
newer, with regression coverage on 5.026, 5.048, and 5.052.

The former public `XDataArray` type and `NewXDataArray`/`NewXDataArrayView`
factories are removed, without aliases. Use `XArray.FromDPI`/`FromNative` and
`XSignalCFG.NewXArray` instead. `Size()` now describes one dimension, not the
total element count; select each dimension recursively rather than passing a
coordinate vector or flat index. Python, for example:

<!-- runnable:python -->
```python
import ctypes
import xspcomm as xs

memory = (ctypes.c_ubyte * 6)()
array = xs.XArray.FromNative(8, [-1, 3], [1, 2], ctypes.addressof(memory), [2, 1])
array[0][2].AsImmWrite()
array[0][2].value = 42
assert memory[2] == 42
```

## Recursive struct and packed-array views

`XStruct` is an independent, recursively named container, not a subclass of
`XData`. `XArray` preserves one declared array dimension; its elements can
be ordinary `XData`, another array, or a struct. Their shared implementation is
in `xview.h`.
Picker's RTL aggregate port integration requires Verilator 5.048 or newer;
the xcomm containers themselves do not depend on a simulator version.

| Storage | Representation |
| --- | --- |
| Packed struct | One parent `XData`; field leaves are bit slices |
| Packed array | One parent `XData`; index selections are bit slices |
| Unpacked struct | Independent field `XData` values and recursive containers |
| Unpacked array | XArray with independent element values and recursive containers; homogeneous leaves share an internal indexed backend |

Picker emits recursive layouts and associates them with the actual port Data
through `xcfg.BindDataLayouts(group)` during initialization. It does not add packed
`*_view` members. Native internal `NewXData()` also associates available types.
Associating a type does not change DPI/native bindings, write modes or XPort registration.

| API | Behavior |
| --- | --- |
| `HasLayout()` | Whether a type is associated; conversion still checks width, kind, and source lifetime |
| `SetLayout(layout_json)` | Explicit type association for manual Data; validate before replacing the type and clearing its view cache |
| `StructView()` / `ArrayView()` | Explicitly obtain a view on demand using the associated layout; missing layouts and kind mismatches throw |
| `StructView(layout_json)` / `ArrayView(layout_json)` | Create a view using an explicit layout; still available for manual Data |

Data of the same configuration type share an immutable parsed description instead
of duplicating JSON per element. Views and their selected leaves are cached on the
Data on demand. Ordinary Data bit indexing remains LSB-zero even when RTL bounds
are available. Data selected by typed fields or elements inherit their child type;
arbitrary `SubDataRef()` slices do not inherit the entire parent layout. Copying
Data preserves its type without adding ownership of model storage. `ReInit()`
clears types and caches and invalidates associated views/slices even at the same
width. Type metadata outlives xcfg, but does not extend DUT storage lifetime.

Ordinary vectors remain bits/Data. If the type retains actual `left`/`right`
bounds such as `[7:4]` or `[0:3]`, explicit `ArrayView()` selects bits with those
RTL indices; `data[0]` still selects the least significant bit. Manual Data,
scalars, or incomplete VPI types without declared ranges do not guess them from
width; provide a layout explicitly instead.

The JSON below only demonstrates manually constructed Data. Picker-generated
signals already have their configuration types and use argument-free views
without users writing the layout again.

<!-- runnable:python -->
```python
import json
import xspcomm as xs

signal = xs.XData(8, xs.XData.In)
signal.SetLayout(json.dumps({
    "kind": "array", "packed": True, "width": 8, "left": 1, "right": 0,
    "element": {
        "kind": "array", "packed": True, "width": 4, "left": -2, "right": 1,
        "element": {"kind": "bits", "packed": True, "width": 0},
    },
}))
assert signal.HasLayout()
array = signal.ArrayView()
array[1][-2].value = 1
assert signal.value == 0x80
signal[0] = 1
assert signal.value == 0x81
assert array.bounds == ((1, 0), (-2, 1))
```

`XView.FromPort(port, name, layout_json)` remains available to group existing leaves.
Layouts include `kind`, `packed`, `width` (zero for scalar leaves), optional
`signed`/`four_state`, array `left`/`right`/`element`, or struct `fields` with
`name`, `type`, and packed `offset`. Widths, bounds, duplicate names, overlapping
fields, gaps and recursive depth are checked before accessing data.

`XView` is a lightweight dynamic node handle, not another value or backend.
`Kind()` identifies `bits`, `struct`, or `array`; `Data()`, `Struct()` and `Array()`
require that exact kind. A mismatch reports the node path, actual kind and requested
kind. Empty handles and expired sources are rejected when converting; `HasValue()`
only tests whether a node exists, not whether its source remains alive.

C++ `XView` factories, `Field`, `At` and `XVPI.Get` return `XView` by value. A typed
destination implicitly performs the same checks without copying signal data:

```cpp
XView node = record->Field("payload");
std::shared_ptr<XArray> array = node;   // payload must be an array
std::shared_ptr<XStruct> wrong = node; // throws; an existing target stays unchanged
```

Packed containers never implicitly convert to `XData`; explicitly use `PackedData()`
for their complete packed value. `XStruct.Data(name)` and `XArray.Data(index)` also
explicitly select a whole packed field/element. Default Python field/element
access unwraps packed values to Data; Python annotations do not enforce assignment checks.

`XStruct.Names()` retains
RTL field declaration order. `Field(name)` returns a typed node; `Data(name)`,
`Struct(name)` and `Array(name)` provide direct access. `PackedData()` returns a
complete packed value and rejects unpacked containers. `XArray.At(index)`
uses an actual RTL index, not a position; its `Data/Struct/Array(index)` helpers
return the selected object. `Shape`, `LeftBounds` and `RightBounds` describe the
consecutive dimensions of the same packed/unpacked kind.

Python containers support `view[name]`, struct field attributes, array `[index]`,
`bounds`, `shape`, and declaration-order iteration. Default field access and packed
struct/array values selected from unpacked arrays return complete `XData`; then
explicitly use `StructView()` / `ArrayView()`. `Struct(name)`, `Array(name)`,
`Struct(index)` and `Array(index)` are themselves explicit structural selections.
An explicitly obtained packed array view continues to index its declared dimensions
instead of treating an inner array as a Data bit offset.
Method-name collisions are accessible with `view[name]`. Container structure is
read-only; drive the referenced signal's `.value` instead of replacing a field.

Packed selections are created lazily and cached. Slices dispatch through their
parent without per-slice backend callbacks, preserve both aval/bval, and merge
writes without changing sibling bits. The parent controls backend write timing
and edge commits; do not register slices as additional ordinary data pins.
Layout and child containers can outlive their outer view, but the source
`XData`, DUT memory and DPI library must remain alive. Access through a slice
whose parent was destroyed fails rather than dereferencing the parent.
Go/Lua expose cached `XData` leaves as borrowed references; keep their source alive
while retaining a leaf. Data retain argument-free explicit views and their leaves;
callers still manage the lifetime of containers created by other factories or XView.
Go container handles are owned resources: release them with `DeleteXView`,
`DeleteXStruct` or `DeleteXArray` when no longer needed, including handles
returned by `Field`, `At`, `Struct` and `Array`. Do not delete borrowed `XData`
leaves. Lua garbage collection releases owned container handles.

## Connections, comparison and callbacks

| API | Purpose and contract |
| --- | --- |
| `Connect(other)` | Propagate changes: Output drives Input/InOut; InOut drives Input; with two InOut objects, the caller is the source. |
| `Equal(other)`; `operator==` | Compare four-state XData content. Integer equality also requires known data; C++ string equality compares display text. |
| `Comp(other, opcode, eq)`; `< <= > >=` | Opcodes 0/1/2 mean equal/less/greater; eq includes equality. Full-width unsigned order comparison requires equal widths when either is at least 64 bits. Check knownness first. |
| `OnChange(callback, args, desc)`; `ClearOnChangeCbs/HasOnChangeCbs` | Cache-change callbacks receive valid, XData pointer, low 64-bit value and user data. Signals wider than 64 bits report only their low 64 bits; read the XData argument inside the callback for the full value. No background monitoring thread is created. |
| `CSelf()` | Native address bridge; does not extend lifetime. |

Python `==` accepts another XData; compare `.value` with integers. For wide signed conversion, explicitly interpret U() using W(); see [bindings](bindings.md).
