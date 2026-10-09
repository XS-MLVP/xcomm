# xcomm source layout

Public interfaces are installed below `include/xspcomm`. Common class entry
headers (`xdata.h`, `xclock.h`, `xexpr.h`, `xtrigger.h`) describe their own modules;
`xcomm.h` and `xcomuse.h` are the aggregate entry points.

- `common/`: comparison algorithms and memory/string utilities. Comparison
  algorithms depend only on standard headers, so XData and checkers share them.
- `xcomuse/`: clock callbacks, condition/range checkers, expression checkers and
  the text-program FSM adapter. `ExprEngine` itself belongs to `xexpr.h`.
- `xpattern.h`: shared sequence/FSM descriptors. `detail/pattern.h` holds the
  matching state required by registrations; `src/pattern.cpp` implements the
  shared matching algorithms.
- `xcoverage.h`: coverage descriptors and snapshots. The independent coverage
  component in `src/coverage` owns sampling history, bins, crosses, counters and
  diagnostics, and has no dependency on trigger types.
- `src/trigger`: trigger registration/execution and a small coverage adapter.
  Both consumers use the same expression and pattern implementations. Coverage
  runs in the native sampling phase, with no event queue or language callback.
- `src/core`: signal storage/value operations, backend bindings, ports, clocks,
  coroutines and configuration. YAML parsing stays in the configuration source;
  third-party YAML headers are not part of the public header dependency graph.
- `swig/`: common binding definitions and language-specific wrappers. Native and
  JavaScript builds use the source list in `cmake/Sources.cmake`.
- `tests/unit`, `tests/bindings`, `tests/integration`, `tests/fixtures`: native
  unit tests, language checks, simulator integration tests and configuration data.
  Run native tests with `ctest --test-dir build --output-on-failure`.
- `third_party/`: vendored fkYAML and Catch2 headers, with their existing licenses.

## Header migration

Update includes directly; removed paths do not have forwarding headers:

| Previous include | Current include |
| --- | --- |
| `xspcomm/xcomuse/compare.h` | `xspcomm/common/compare.h` |
| `xspcomm/xcomuse/utils.h` | `xspcomm/common/memory.h` |
| `xspcomm/xfsm.h` | `xspcomm/xcomuse/fsm.h` |
| `xspcomm/xexpr.h` for `ComUseExprCheck` | `xspcomm/xcomuse/expr.h` |
| `xspcomm/tlm_msg.h` | `xspcomm/tlm/message.h` |
| `xspcomm/tlm_pbsb.h` | `xspcomm/tlm/pubsub.h` |

`xinstance.h` and its `test_xdata()` helper are removed from the production SDK;
that test implementation is compiled into `test_xdata` instead. Python-only
third-call declarations live beside the Python wrapper. Existing runtime class
layouts and Python public methods are retained; source clients must update the
includes above and rebuild bindings as needed.

## TLM source SDK

The standalone TLM integration lives in `integrations/tlm`. Installation exports
`pubsub.cpp`, `uvmc_bridge.h` and `python.i` under `share/xspcomm/tlm`, alongside the
public `xspcomm/tlm/` headers. Simulator makefiles can set `XSP_COMM_TLM`; its
default is `$(XSP_COMM_INCLUDE)/../share/xspcomm/tlm`. The integration tests use the
source SDK and staged build headers directly.

The optional legacy `BUILD_XSPCOMM_VCS_UVMPS` combined build retains its previous
configuration. It still references the pre-existing absent `src/uvm_pbsb.cpp`
and `swig/python/python_uvmps.i`; use the standalone TLM SDK instead of that path.
Actual VCS/SystemC execution requires the corresponding external tools.
