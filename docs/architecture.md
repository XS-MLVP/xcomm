# xcomm source layout

Public interfaces are installed below `include/xspcomm`. Common class entry
headers (`xdata.h`, `xclock.h`, `xexpr.h`, `xtrigger.h`) describe their own modules;
`xcomm.h` and `xcomuse.h` are the aggregate entry points.

- `common/`: comparison algorithms and memory/string utilities. Comparison
  algorithms depend only on standard headers, so XData and checkers share them.
- `xcomuse/`: clock callbacks, condition/range checkers, expression checkers and
  the text-program FSM adapter. `ExprEngine` itself belongs to `xexpr.h`.
- `xpattern.h`: shared event modes and sequence/FSM descriptors.
  `detail/pattern.h` holds borrowed program views and matching state;
  `src/pattern.cpp` implements sequence/FSM advancement. The common execution
  component in `src/trigger/matcher.*` manages expression events, overlapping
  attempts, completion counts and FSM terminal results for native consumers.
- `xcoverage.h`: coverage descriptors and snapshots. The component in
  `src/coverage` organizes points and bins, selects completion results, and owns
  crosses, counters and statistical policies. It uses the common trigger
  matcher for execution history and advancement.
- `src/trigger`: trigger registration/execution and a small coverage adapter.
  Ordinary triggers and coverage use the same expression events and pattern
  advancement. Coverage runs in the native sampling phase, with no event queue
  or language callback.
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
third-call declarations live beside the Python wrapper. Existing signal/clock
layouts and callable interfaces are retained. Coverage
descriptors have expanded, so native clients must rebuild against ABI 3;
source clients must also update the includes above.

## Trigger programs and coverage

Language frontends lower declarations to expression roots, sequence steps or
FSM transitions. A value point observes an XData signal; a pattern point sets
`XCoverageItem.pattern` and organizes complete trigger programs without a
synthetic signal. Pattern bins select Expr, Sequence or FSM with
`XCoverageBin.program_kind` (0, 1 or 2), and specify event mode, bounded overlap
and optional FSM terminals. `CoverageVersion() == 4` identifies this descriptor
protocol; the native library ABI is independently versioned as 3.

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
