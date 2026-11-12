# XSignalCFG: create signals from a layout

[XSignalCFG](https://github.com/XS-MLVP/xcomm/blob/master/include/xspcomm/xsignal_cfg.h) maps names, widths and native memory offsets to XData. It also creates read-only constants and expression signals. It consumes an existing layout; it does not allocate DUT memory or derive addresses from RTL.

## Native layout

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
| `XSignalCFG(path_or_yaml, base_address=0)` | Accept an existing file path or YAML text; parse on the first query/construction operation. |
| `NewXData(name, xname="")` | Create a signal; for an array, create the element at flat index 0. May return nullptr on failure. C++ callers own the result. |
| `NewXData(name, index, xname="")` | Create an array element by flat, zero-based storage index. The index must be less than the array size and is checked; out-of-range values throw. Returns nullptr for a non-array name. |
| `NewXDataArray(name, xname="")` | Create all elements in flat order from one shared view; C++ returns a shared_ptr vector. Returns an empty vector for a non-array name. |
| `NewXDataArrayView(name)` | Return the cached XDataArray view with zero-based bounds, or nullptr for a missing or non-array name. |
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

## Read-only derived signals

The optional signals list uses name/kind and fields such as rtl_width, offset/mem_bytes, expr or value:

- kind=direct binds native storage and still needs offset/mem_bytes/rtl_width.
- kind=const parses a value string and creates a read-only signal using BindConst, with a 64-bit numeric evaluation range.
- Other derived entries create an ExprEngine from expr and read through BindExpr. source/deps are metadata; source alone does not automatically create an alias expression.

Derived signals inherit ExprEngine's numeric and syntax limits; they do not interpret arbitrary C++. Cycles or expression parse failures make NewXData report an error and return nullptr. s_xsignal_cfg and s_xsignal_meta describe native layout and derived metadata respectively; At primarily queries the former.
