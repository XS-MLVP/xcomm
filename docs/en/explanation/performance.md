# Performance and memory: where costs occur

Simulation time is spent in DUT evaluation, language calls, signal copies, expression evaluation, and temporal matching. To compare performance, fix sampling phases, waveform settings, and coverage configuration, then measure these components.

## Current cost controls

| Path | Mechanism | Boundary |
| --- | --- | --- |
| XData change callbacks | Skip shadow comparison with no callbacks; establish a baseline at registration | With callbacks, aval/bval checks and shadow updates remain necessary |
| Repeated writes | memcmp checks the full vector; memcpy saves changed values | Comparison also costs work; controls must preserve required repeated-write semantics |
| XPort batch refresh | map for names, vector for hot iteration | Add/Del/subports must maintain both indexes |
| Native memory | Choose storage type/fixed word count at binding; expand short paths with templates, loop for long paths | Read exact uint8/uint16/uint32 storage; never overread a narrow object as uint64 |
| Engine scheduling | Native half-cycle batches and phase-local matching | Python edge callbacks still cross the language boundary |
| Coverage | Equivalent Pattern programs within a point share execution | Bins count independently; separate points cannot share history |
| Typed evaluation | Explicit stack, reusable per-node results/division scratch, shared DAG nodes evaluated once per call | Every evaluation refreshes inputs; no cache from a previous cycle |

For a fixed typed program whose relevant nodes have been visited, warm `Eval` reuses workspace. `EvalBits` returns an owning byte vector and therefore copies its result. New nodes, newly visited branches and evaluation after Collect may allocate again. Collect/Clear release unreachable nodes and workspace. Memory use depends on width, graph size, and matcher overlap capacity.

## Why exact-width memory access

Native storage differs from XData's aval/bval layout. memcpy-based `memory::Load/Store<T>` supports unaligned objects without accessing them through incompatible pointer types. Compilers can lower small fixed-size copies to direct loads/stores. Inspect generated instructions for the compiler, target, and optimization settings being measured.

Current build flags include `-fno-strict-aliasing`, limiting optimizations based on type-based non-aliasing assumptions. It does not repair out-of-bounds access, invalid unaligned access, dangling pointers or data races. Correct types, sizes and lifetimes remain necessary.

BindNativeData's two-state storage holds aval only. See the [API contract](../reference/xdata.md) for how existing four-state cache data affects reads/writes.

## Comparing performance

1. Fix commits, compiler/flags, CPU, simulator and input sequence. Measure build and runtime separately.
2. Match callbacks, waveforms, coverage, sampling phases and matcher capacities. Verify identical results/counts first.
3. Measure narrow and wide data, cold and warm calls, ordinary events and overlapping Patterns separately.
4. Report repeated-run distributions, peak memory, and allocation counts.

`tests/unit/test_expr_allocations.cpp` checks warm-evaluation allocation constraints; `test_xexpr` checks typed values and errors; `test_trigger_matcher` and `test_coverage_core` cover matching/statistics semantics. Keep performance claims with their benchmark and environment.
