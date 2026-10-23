# Development, testing and contribution

Use the [architecture](../explanation/architecture.md) to locate a change in signals, expressions, matching, statistics or bindings. Application APIs belong under include/xspcomm; pure implementation belongs under src. Test private types in native unit tests.

## Incremental build and native tests

From the xcomm repository root, with a C++20 compiler and CMake>=3.15; bindings also require their language toolchain:

```sh
BUILD_XSPCOMM_SWIG=python cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
PYTHONPATH=build/python python3 tests/bindings/python/test_python.py
```

Default `make` runs rbuild and cleans build/dist first. Use the CMake commands above to retain incremental artifacts. Use build/python with the native library from the same build. Python bindings require SWIG>=4.2 and Python development headers.

Native CTest currently registers 11 targets:

| Test | Main coverage |
| --- | --- |
| test_xdata | Signal values, widths, writes and memory boundaries |
| test_xclock | Stepping, callbacks and ownership |
| test_xtrigger | Registration, execution and event protocol |
| test_xcoverage | Coverage integrated with the engine |
| test_xmonitor | Checkers and attachment lifetime |
| test_xexpr | Expressions, typed arithmetic and collection |
| test_xvpi | VPI adaptation |
| test_xfsm | Text FSM monitor |
| test_trigger_matcher | Private matching kernel |
| test_expr_allocations | Warm expression allocation constraints |
| test_coverage_core | Independent statistical policy and execution state |

## Choosing validation

Signal/memory changes need narrow, unaligned, full-width and rebinding coverage. Clock changes need manual/owned execution, exceptional exit and destruction order. Coverage changes need classification, gate/abort, same-sample Cross, counter layout and shared matching. Wide arithmetic needs signs, partial words, division/shift errors and root remapping after collection.

Run the corresponding tests/bindings for wrapper changes. Compile a minimal client against the installed SDK after public-header changes, so source include paths do not hide missing installed headers. TLM tests in tests/integration/tlm require actual SystemC/UVMC/simulator tools. State which environments were not tested.

## Commits and interface changes

PRs explain the triggering problem, final behavior, evidence for performance impact and actual validation scope. New APIs need [both language references](documentation.md), complete examples, lifetimes and error semantics. State whether each error returns false, throws, or terminates the process.

Evaluate ABI changes using the [release guide](releases.md); evaluate Coverage protocol changes separately. A group of unpublished branch changes shares one ABI transition. PRs targeting master need exactly one release:patch/minor/major label.
