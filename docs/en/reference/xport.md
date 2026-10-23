# XPort: named signal collections

Header: [xport.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xport.h). XPort stores XData pointers for naming, bulk direction/write-mode changes and edge refresh. It owns neither signals nor clock advancement.

<!-- runnable:python -->
```python
import xspcomm as xs

valid = xs.XData(1, xs.XData.InOut)
data = xs.XData(32, xs.XData.InOut)
port = xs.XPort("bus.")
assert port.Add("valid", valid)
assert port.Add("data", data)
port["data"] = 0x55
assert port["data"].value == 0x55
assert port.Has("bus.data", True)
```

## API

| API | Purpose and contract |
| --- | --- |
| `XPort(prefix="")`; `prefix`; `GetPrefix()` | Prefix local names passed to Add. |
| `Add(name, data)`; `Del(name)` | Add/remove a mapping; duplicate Add returns false. Del does not destroy XData. |
| `PortCount()`; `GetKeys(raw_key=false)` | Count signals/list local names; raw_key includes prefixes. |
| `Has(name, raw_key=false)`; `Get(name, raw_key=false)`; C++ indexing | Query/access a signal. Missing Get triggers an assertion; use Has first. |
| `Connect(other)` | Match local names and apply XData direction rules. Missing peers are reported while processing continues; true does not establish complete connectivity. |
| `NewSubPort(subprefix)` | Create a prefixed view over the same signals. |
| `SelectPins(names)` | Create a subset view; missing names are reported. |
| `FlipIOType/AsBiIO` | Change directions in bulk. |
| `AsImmWrite/AsRiseWrite/AsFallWrite` | Change writable-signal commit timing. |
| `WriteOnRise/WriteOnFall/ReadFresh(mode)` | Commit/refresh at edges via XClock or explicitly. |
| `SetZero()` | Assign zero subject to writability and write mode. |
| `String(prefix="")` | Describe the collection. |
| C++ `operator=(other)` | Assign values from the other port's same-named signals. |
| `CSelf()`; `port_list/port_vec` | Native bridge/internal collections; prefer Add/Del/Get over direct edits. |

C++ NewSubPort/SelectPins allocate with new and return a reference. The caller owns the result, for example `std::unique_ptr<XPort>(&port.NewSubPort(...))`. Check generated ownership rules for language bindings; reference returns are not automatically managed everywhere.

After adding a port to XClock, keep the port and every signal alive for that clock's use. Python should retain explicit signal references too.
