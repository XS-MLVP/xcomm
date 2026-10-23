# XData: signals and values

Header: [xdata.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xdata.h). XData provides a common readable/writable signal interface with four-state caching, backend bindings and slices. Python exposes `xspcomm.XData`.

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
| `String()`; `AsBinaryString()` | Hexadecimal display and binary display including x/z. String omits 0x and displays unknown bytes as ??. |
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
| `SubDataRef(start, width, name)` | Borrow `[start, start+width)`; writes affect the parent, which must remain alive. |
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

## Connections, comparison and callbacks

| API | Purpose and contract |
| --- | --- |
| `Connect(other)` | Propagate changes: Output drives Input/InOut; InOut drives Input; with two InOut objects, the caller is the source. |
| `Equal(other)`; `operator==` | Compare four-state XData content. Integer equality also requires known data; C++ string equality compares display text. |
| `Comp(other, opcode, eq)`; `< <= > >=` | Opcodes 0/1/2 mean equal/less/greater; eq includes equality. Full-width unsigned order comparison requires equal widths when either is at least 64 bits. Check knownness first. |
| `OnChange(callback, args, desc)`; `ClearOnChangeCbs/HasOnChangeCbs` | Cache-change callbacks receive valid, XData pointer, low 64-bit value and user data. No background monitoring thread is created. |
| `CSelf()` | Native address bridge; does not extend lifetime. |

Python `==` accepts another XData; compare `.value` with integers. For wide signed conversion, explicitly interpret U() using W(); see [bindings](bindings.md).
