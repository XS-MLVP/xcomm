# XSignalCFG: create signals from a layout

[XSignalCFG](https://github.com/XS-MLVP/xcomm/blob/master/include/xspcomm/xsignal_cfg.h) maps names, widths and native memory offsets to XData. It also creates read-only constants and expression signals. It consumes an existing layout; it does not allocate DUT memory or derive addresses from RTL.

## Native layout

Picker exports one `.xscfg` configuration database by default. It contains port
metadata, native records, shared types, field bindings and derived signals, with
no YAML/JSON/index sidecars. `XSignalCFG` recognizes the file by its magic bytes
(including when extracted from a JAR). The YAML input shown below remains useful
for hand-written configurations; it is not the generated runtime format.

variables accepts a list or nested mapping. See the [list fixture](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/tests/fixtures/signal_cfg.yaml) and [mapping fixture](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/tests/fixtures/signal_cfg_map.yaml).

```yaml
variables:
  - name: Top.data
    type: SData
    offset: 0
    mem_bytes: 2
    rtl_width: 16
```

| Field | Meaning |
| --- | --- |
| name | Name in list form; mapping keys combine into a hierarchical name |
| offset | Byte offset from base_address |
| mem_bytes | Storage size per element recorded by the layout; array addressing uses array_strides |
| rtl_width | RTL width; 1 creates a width=0 scalar XData |
| array_size | Number of array elements; nonzero values require array_dimensions and array_strides |
| array_dimensions | Element counts per dimension; their product must equal array_size |
| array_strides | Byte strides, outermost first; rank must match array_dimensions |
| type | Layout type description; does not override BindNativeData's storage convention |
| layout_id | Optional RTL type reference into the top-level `types` table, not a C++ type name |
| bindings | Homogeneous unpacked-struct field groups with native offset/mem_bytes and enclosing array dimensions/strides |

When `array_size` is nonzero, both `array_dimensions` and `array_strides` are
mandatory. Malformed layouts fail during parsing.

```yaml
variables:
  - name: Top.mem
    type: VlWide<3>
    offset: 0
    mem_bytes: 12
    rtl_width: 65
    array_size: 6
    array_dimensions: [2, 3]
    array_strides: [80, 24]
```

## API

| API | Behavior and constraints |
| --- | --- |
| `XSignalCFG(path_or_yaml, base_address=0, model_id="")` | Accept an XSCFG file, a hand-written YAML file or YAML text. Load on the first actual query. Optional model_id rejects another build's database; generated DUTs always supply it. |
| `Metadata(name)` | Explicitly decode one metadata record as JSON, e.g. `Metadata("ports")`; missing metadata returns `"null"`. |
| `NewXData(name, xname="")` | Create a signal; for an array, create the element at flat index 0. May return nullptr on failure. C++ callers own the result. |
| `NewXData(name, index, xname="")` | Create an array element by flat, zero-based storage index. The index must be less than the array size and is checked; out-of-range values throw. Returns nullptr for a non-array name. |
| `NewXArray(name)` | Return the cached recursive XArray with zero-based storage bounds, or nullptr for a missing or non-array name. Size is the current dimension length; Array/Data select one index per dimension. |
| `NewXView(name)` | Return a cached RTL-indexed view, including field/array paths. Missing names return an empty view; missing or inconsistent types/bindings throw. Dynamic leaves default to immediate writes. |
| `GetSignalNames(pattern="")` | List names using pattern as a substring filter. |
| `At(name)`; `operator[]` | Query native layout; missing entries have is_empty=true and become None in Python. |
| `Address(name)` | Return base+offset. Check existence with At first; an address alone does not prove existence. |
| `String()` | Describe layout and metadata. |
| `cfg_data/cfg_base_address` | Configuration and base address. Changes after parsing do not reload the layout; create a new object. |

<!-- runnable:python -->
```python
import ctypes
import xspcomm as xs

memory = ctypes.c_uint16(0x1234)
config = xs.XSignalCFG("""
variables:
  - name: Top.data
    type: SData
    offset: 0
    mem_bytes: 2
    rtl_width: 16
""", ctypes.addressof(memory))
signal = config.NewXData("Top.data")
assert signal is not None
assert signal.U() == 0x1234
signal.ImmSet(0x55)
assert memory.value == 0x55
```

Keep memory alive longer than every signal bound to it. Offsets, mem_bytes, widths, dimensions and strides must match the simulator's storage, including the exact size of narrow C objects.

## Single-file binary database

The versioned little-endian format serializes no process pointers or native C++
struct layouts. A 128-byte header identifies the build and locates the index and
block directory. The 8-byte index entries contain a 32-bit name fingerprint and a
packed block/slot locator. Fingerprints are only a search accelerator: every hit
compares the full category/name, including collisions. Prefix-coded names and
tagged values share independently checksummed deflate blocks, up to 256 records
per ordinary block. A block stays uncompressed when compression would grow it.

On POSIX, opening maps the file instead of parsing every record or building maps
for every signal. The first query validates and decodes only its blocks. The
decompressed block cache is bounded to 2 MiB; unusually large individual records
require temporary memory outside that cache. Signal objects, type records and
bindings are materialized on demand. Enumeration scans blocks without decoding
all values; `String()` explicitly loads all signals and is not a startup operation.
Header/interval checks, block checksums and value/depth bounds reject malformed
input. Header validation is eager, block validation is lazy, not a whole-file
integrity scan. Replacement uses an adjacent temporary file and atomic rename;
readers of the old inode remain valid. There is no journal or persistent cache.

Format version 1 and native ABI 5 are independent. This backend requires the zlib
development package when building (`zlib1g-dev` on Debian/Ubuntu). The read-only
database owns metadata, not DUT storage; keep the model alive while using signals.

## Recursive native types

The optional top-level `types` table interns type nodes; `element` and each field's
`type` reference another type ID. `NewXView()` expands types only on demand,
checking depth, cycles, widths, shapes and storage sizes. Packed structs/arrays
share one complete `XData`; unpacked structs bind independent homogeneous field
groups. Array elements are created lazily, without repeated element type trees or
per-element callbacks. Ordinary single-dimension bit vectors are value leaves.

`NewXArray()` retains zero-based storage coordinates, while `NewXView()` uses RTL
bounds. Existing array configurations without RTL metadata still support
`NewXArray()`, but element counts cannot recover RTL indices. Unpacked structs
have no whole bit value: use their view, not `NewXData()` on their first byte.
`XView.FromNativeBindings(base, layout, bindings)` consumes the same verified
description; the caller owns the storage lifetime. Offsets, sizes and strides must
come from the native compiler that built the model, not guessed padding from bit
widths. `XView.FromNative()` handles one packed value or fixed unpacked arrays of it.

## Read-only derived signals

The optional signals list uses name/kind and fields such as rtl_width, offset/mem_bytes, expr or value:

- kind=direct binds native storage and still needs offset/mem_bytes/rtl_width.
- kind=const parses a value string and creates a read-only signal using BindConst, with a 64-bit numeric evaluation range.
- kind=unavailable has neither proven storage nor an evaluable expression; `NewXData()` returns nullptr.
- Other derived entries create an ExprEngine from expr and read through BindExpr. source/deps are metadata; source alone does not automatically create an alias expression.

Derived signals inherit ExprEngine's numeric and syntax limits; they do not interpret arbitrary C++. Cycles or expression parse failures make NewXData report an error and return nullptr. s_xsignal_cfg and s_xsignal_meta describe native layout and derived metadata respectively; At primarily queries the former.
