# xcomm source layout

Public interfaces are installed below `include/xspcomm`. Common class entry
headers (`xdata.h`, `xclock.h`, `xexpr.h`, `xtrigger.h`) describe their own modules;
`xcomm.h` and `xmonitor.h` are the aggregate entry points.

- `common/`: comparison operations and algorithms, plus memory/string utilities.
  `compare_op.h` defines the shared comparison enum; `memory_access.h`
  provides non-owning reads and writes with exact access
  widths. It depends only on standard headers and is shared by comparison and
  native signal backends. `XByteBuffer` retains storage ownership/copy duties.
  `callback_storage.h` and `lifetime.h` supply callback storage and ownership
  helpers used by the public callback templates.
- `monitor/`: clock callbacks, condition/range checkers, expression construction
  and the text-program FSM adapter. `XExprCheck` uses `XConditionCheck`'s
  registration namespace, hit processing and clock-stop behavior. Native pointer,
  XData and expression evaluation keep their own comparison semantics.
- `xpattern.h`: trigger program descriptions, event modes and sequence/FSM
  descriptors. `runtime/types.h` contains phases, handles, hits and run results;
  it depends only on standard headers and does not pull in `XClock`.
- `src/trigger/`: program validation, single-execution state advancement
  (`executor.h/.cpp`), overlapping matchers (`matcher.h/.cpp`) and trigger
  registrations (`registration.h`). The matching kernel has no coverage dependency.
- `xcoverage.h`: explicitly typed points/bins and snapshots. Coverage execution
  history and statistical policy live in `src/coverage/state.h/.cpp`.
  Bins contain a shared
  `XTriggerProgram` description and select completion results. Coverage owns
  crosses, counters, classification and terminal selection.
- `xengine.h` and `src/runtime`: the shared clock/phase schedule and registration
  lifecycle. The engine sends matches to either trigger events or coverage
  statistics. `xtrigger.h` retains the `XTriggerEngine` entry name as an alias of
  `XEngine`; language bindings retain their existing class name. Coverage-specific
  registration operations live under `src/coverage`, not the trigger kernel.
- `src/runtime/registration.h`: the private `XEngine::Watcher` definition,
  combining trigger state with optional coverage state. `xengine.h` only declares
  this type. The existing vector storage and inline execution path are retained;
  hiding the definition adds no indirection or per-registration allocation.
- `src/data/`: signal storage (`data.cpp`), values and comparisons (`value.cpp`),
  backend bindings (`backends.cpp`), VPI access (`vpi.cpp`) and individual
  bit references (`pin.cpp`).
  `native_memory.h` is the templated native-to-vector storage adapter.
  Fixed word counts stay expanded at compile time; widths are selected at binding.
- `src/core/`: clocks, port collections, coroutines and configuration.
  YAML parsing stays in the configuration source;
  third-party YAML headers are not part of the public header dependency graph.
- `swig/`: common binding definitions and language-specific wrappers. Native and
  JavaScript builds use the source list in `cmake/Sources.cmake`.
- `tests/unit`, `tests/bindings`, `tests/integration`, `tests/fixtures`: native
  unit tests, language checks, simulator integration tests and configuration data.
  Run native tests with `ctest --test-dir build --output-on-failure`.
- `third_party/`: vendored fkYAML and Catch2 headers, with their existing licenses.

`include/xspcomm/` is the installed SDK. Pure implementation headers live beside
their sources under `src/` and are not installed. Public templates keep their
required helpers in `include/xspcomm/common/`; the `xspcomm::detail` namespace
marks those helper types. Native and JavaScript targets use `src/` as a private
include directory. Only tests of implementation types use it directly.

## Header migration

Update includes directly; removed paths do not have forwarding headers:

| Previous include | Current include |
| --- | --- |
| `xspcomm/xcomuse.h` | `xspcomm/xmonitor.h` |
| `xspcomm/xcomuse/{callback,condition,range,expr,fsm}.h` | `xspcomm/monitor/{callback,condition,range,expr,fsm}.h` |
| `xspcomm/xcomuse/compare.h` | `xspcomm/common/compare.h` |
| `xspcomm/xcomuse/utils.h` | `xspcomm/common/memory.h` |
| `xspcomm/xfsm.h` | `xspcomm/monitor/fsm.h` |
| `xspcomm/xexpr.h` for `ComUseExprCheck` | `xspcomm/monitor/expr.h` |
| `xspcomm/common/operation.h` | `xspcomm/common/compare_op.h` |
| `xspcomm/detail/function.h` | `xspcomm/common/callback_storage.h` |
| `xspcomm/detail/lifetime.h` | `xspcomm/common/lifetime.h` |
| `xspcomm/common/memory/access.h` | `xspcomm/common/memory_access.h` |
| `xspcomm/trigger/types.h` | `xspcomm/runtime/types.h` |
| `xspcomm/tlm_msg.h` | `xspcomm/tlm/message.h` |
| `xspcomm/tlm_pbsb.h` | `xspcomm/tlm/pubsub.h` |

The former `xspcomm/detail/{trigger,coverage,data}/*.h` implementation headers
are no longer installed. Use `xpattern.h`, `xcoverage.h` and `xengine.h` for
program descriptions, coverage descriptors and registration. Matching and
native-memory implementation headers belong to `src/trigger/`, `src/coverage/`
and `src/data/`; they are not replacement SDK include paths.

Monitor and utility names migrate in C++, SWIG bindings and Python together:

| Previous name | Current name |
| --- | --- |
| `ComUseStepCb` | `XStepCallback` |
| `ComUseCondCheck` | `XConditionCheck` |
| `ComUseExprCheck` | `XExprCheck` |
| `ComUseRangeCheck` | `XRangeCheck` |
| `ComUseFsmTrigger` | `XFsmMonitor` |
| `ComUseEcho` | `XEcho` |
| `ComUseDataArray` | `XByteBuffer` |
| `ComUseCondCmp` | `CompareOp` |

Removed names have no compatibility aliases. `CompareOp` is the existing shared
comparison enum, now exposed with the same name in language bindings.

`xinstance.h` and its `test_xdata()` helper are removed from the production SDK;
that test implementation is compiled into `test_xdata` instead. Python-only
third-call declarations live beside the Python wrapper. Existing signal
access and clock stepping interfaces are retained. Coverage descriptors and
clock/checker/runtime layouts have changed, so native clients must rebuild against ABI 5;
source clients must also update the includes above.

## Trigger programs and coverage

Language frontends lower declarations to `XTriggerProgram`: an explicitly typed
expression, sequence or FSM, with event mode and execution capacity. The trigger
and coverage paths use the same validator and advancement kernel. Matching state
belongs to each execution and is independent of the immutable program description.

`XCoverageItem.kind` selects Value, Pattern or Cross. Value points require an
XData source; pattern points have no synthetic source; crosses name preceding
value points. `XCoverageBin.kind` selects Normal, Ignore, Illegal or Default,
while `XCoverageBin.program` supplies matching and `terminals` selects FSM
completion results. Impossible field combinations are rejected at registration.
`CoverageVersion() == 5` identifies this descriptor protocol; the native library
ABI is independently versioned as 5. Protocol 4 frontends must migrate flat bin
program fields to `bin.program` and replace `item.pattern` with an explicit kind.

Equivalent programs within one point share an execution when their roots,
steps/transitions, event mode and overlap capacity agree. Terminal selection,
bin kind and coverage thresholds do not change execution. Each bin retains its
own count, and different points or registrations retain independent history.
`CoverageExecutionCount(handle)` counts these point-local pattern executions.

Each group is registered on one clock/phase. Pattern completion counts are
preserved, including multiple terminals completing in one sample. Unselected
FSM terminals still end their attempts. Closing a gate clears the affected bin
history; abort clears source and bin history. Reset can preserve counters while
clearing execution history. Pattern ignore/illegal bins affect their own
completion results; value points retain their classification priority rules.
Crosses containing pattern points are rejected until completion correlation
semantics are defined. Program reclamation during a long execution and complete
expression width/signed/cast/four-state semantics remain separate work.

## Monitor callback ownership

`XStepCallback.Attach(clock, rising)` registers a managed clock callback; `Detach`
and destruction remove its registrations. Destroying the clock first is safe.
Removal during another callback defers vector erasure until iteration finishes.
The callback base has a virtual destructor and cannot be copied with registrations.

`XConditionCheck.BindXClock` specifies clocks to stop on a hit, separately from
sampling registration. `ClearClock` clears that stop list. Existing `GetCb` /
`CSelf` raw callback registration remains available and requires the caller to
unregister before destroying the checker; use `Attach` for managed ownership.

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
