# Language bindings and calling conventions

The shared interface is [xcomm.i](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/xcomm.i); Python conveniences live in [xcomm.py](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/python/xcomm.py). Distinguish the C++ API from wrappers: matching method names do not guarantee identical argument and return types.

## Names and containers

- C++ uses XEngine; bindings currently expose XTriggerEngine. The C++ XTriggerEngine name is an alias.
- Python enums are generally flattened, such as XPhase_RisingStable, ExprOp_EQ and XCoveragePointKind_Pattern. XData.In/Out/InOut are direction conveniences.
- Descriptor arrays use XCoveragePointVector, XCoverageBinVector, XSequenceStepVector and XFsmTransitionVector. Index containers include XIntVector/XUInt32Vector/XUInt64Vector; hit lists use XBackendHitVector.
- Python vector<unsigned char> arguments use bytes, not arbitrary lists. Byte results also use bytes.
- Native pointers stored in another object do not automatically keep other language proxies alive. Retain clocks, signals, ports and callbacks explicitly.

## Python XData

XData(width, direction=XData.In) creates an object; set mName afterward if needed. `.value`, `Set()` and `ImmSet()` accept Python int and use byte conversion for wide vectors. U/value return the full unsigned Python integer.

C++ S reads a signed low 64-bit value. The Python wide S wrapper currently uses the byte count to locate the sign bit, which can misinterpret signed values wider than 64 bits. To read a known signed value of an explicit width, convert U using W:

<!-- runnable:python -->
```python
import xspcomm as xs

signal = xs.XData(129, xs.XData.InOut)
signal.value = -3
unsigned = signal.U()
assert signal.DataValid()
width = signal.W()
signed = unsigned - (1 << width) if unsigned & (1 << (width - 1)) else unsigned
assert signed == -3
```

Index reads return 0/1/2/3; writes call PinBind.Set. Python XData == accepts another XData; compare `.value` with integers. BindDPIName is a Python convenience for generated DUTs.

## Python XClock and asyncio

| API | Python behavior |
| --- | --- |
| `XClock(callable)` or `XClock(step_addr, dut_addr)` | Wrap a step callable or use native addresses |
| `Step(cycles)` | Synchronously advance full cycles, check callback exceptions per cycle and notify the internal asyncio Event |
| `RawStep(cycles)` | Native Step; still enforces native ownership, without Python event notifications |
| `StepRis/StepFal(callable, args=(), kwargs={})` | Wrap Python edge callbacks receiving the cycle number and supplied arguments |
| `getEvent()` | Return the internal step event |
| `await AStep(n)` | Wait for n Python Step notifications; does not drive the clock |
| `await ACondition(callable)` | Check at notification points until true |
| `await ANext()` | Wait for the next notification |
| `await RunStep(n)` | Cooperatively drive n cycles in asyncio; differs from synchronous C++ RunStep |

Manual drive and legacy asyncio wait methods reject use while an engine owns the clock. Native engine execution does not emit the Python Step notifications. Schedule waits from engine hits instead of waiting on the old `_step_event` for equivalent behavior.

Python Event provides wait/set/clear; Queue provides put/get/put_nowait/get_nowait. sleep(delay) calls asyncio sleep using wall-clock time. These wrappers support the legacy RunStep cooperative scheduler.

Only callback APIs with Python convenience wrappers accept ordinary callables directly. Low-level APIs such as XData.OnChange require their SWIG callback/director type; XClock's lambda syntax does not apply to every callback.

## Other languages

| Binding | Wrapper focus | Source |
| --- | --- | --- |
| Java | Native library loading, byte arrays and BigInteger values | [java.i](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/java/java.i) |
| Scala | Java/SWIG base and Scala extensions | [xsp.scala](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/scala/xsp.scala) |
| Go | Pointer/integer-address wrappers and some raw slice APIs | [golang.i](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/golang/golang.i) |
| Lua | SWIG objects and numeric/callback adaptation | [lua.i](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/lua/lua.i) |
| JavaScript | SWIG/JS build and value wrappers | [xspcomm.js](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/javascript/xspcomm.js) |

See the [API index](api-index.md) and [binding tests](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/tests/bindings). Shared SWIG declarations do not imply a high-level Coverage DSL in every language. Validate each wrapper separately from Python/C++ examples.

## Exceptions and Python tools

SWIG maps std::invalid_argument to ValueError and other standard exceptions to RuntimeError. Some legacy APIs use Assert/Fatal to terminate the process or report an error and return a failure value; try/except cannot catch every failure.

`python -m xspcomm.info --help` describes queries for the installation root, include/lib paths, version and exported environment. See [info.py](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/python/info.py) for options.

Python thirdcall's init_third_call/free_third_call/get_function_id/call_third_function form a specialized native bridge with explicit word arrays and return capacities. See [thirdcall.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/python/thirdcall.h).
