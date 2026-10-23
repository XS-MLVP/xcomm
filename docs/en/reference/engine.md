# XEngine: native execution runtime

Headers: [xengine.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xengine.h), [runtime/types.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/runtime/types.h). C++ accepts XEngine or its XTriggerEngine alias; SWIG currently exposes XTriggerEngine.

The engine manages registrations and sampling order on one clock, batching advancement until events or budgets. It serves Trigger and Coverage without defining a language-level coroutine API.

## Lifecycle

Construct → AcquireClock → build expressions/register → RunUntil or SamplePhase → process results → Disarm all registrations → ReleaseClock. See the [tutorial](../tutorials/first-simulation.md).

Construction does not acquire. Acquisition is repeatable for the same engine; a competing owner, destroyed clock, active stepping or callback context causes an exception. Recursive RunUntil, Clear and ReleaseClock are rejected during execution.

## Ownership and resources

| API | Purpose and contract |
| --- | --- |
| `XEngine(clock, capacity=1024)` | Borrow a non-null XClock; positive capacity limits occupied slots. |
| `AcquireClock()` | Acquire exclusive driving before registration/execution. |
| `ReleaseClock()` | Requires all occupied slots removed, including fired registrations. |
| `ActiveCount()`; `Capacity()` | Count occupied slots / capacity; includes fired registrations awaiting removal. |
| `Disarm(handle)` | Remove registration and Coverage state; return false for stale/invalid handles. |
| `Rearm(handle)` | Reuse a fired occupied registration; false if already armed or removed. |
| `RearmSample(handle)` | Rearm a fired Sample registration only. |
| `ClearExecutionState()` | Requires no registrations; clear expressions/execution caches while retaining clock ownership. |
| `Clear()` | Clear registrations, expressions and caches. Discard old handles/roots; still ReleaseClock afterwards. |
| Destructor | Release ownership if the clock remains alive; explicit normal cleanup is preferable. |

Keep the borrowed clock and signals alive while the engine uses them. Acquisition and execution checks reject a destroyed clock.

## Execution

| API | Behavior |
| --- | --- |
| `RunUntil(max_half_ticks, max_wall_time_ns=0, budget_check_interval=64)` | Advance up to a half-cycle budget; may return without hits. Zero wall-time budget disables that limit. |
| `SamplePhase(XPhase::DriveStable)` | Allowed after FallingStable; refresh combinational logic and sample without advancing half ticks. |

Wall-time checks are periodic, so actual return time may exceed the budget. RunUntil returns for ordinary hits, DriveStable barriers, disabled clocks or budget exhaustion. One phase may produce multiple hits. At a DriveStable boundary, drive inputs, call SamplePhase(DriveStable), then continue; the engine does not generate stimulus.

## Results

| Type/field | Meaning |
| --- | --- |
| `XRunResult.advanced_ticks` | Half ticks advanced by this call; zero for SamplePhase. |
| `stopped_phase`; `stop_reason` | Sampling phase and reason for return. |
| `hits`; `IsPhaseBarrier()` | Hits from this call and a phase-barrier helper. |
| `XRegistrationHandle.slot/generation`; `IsValid()` | Slot and generation; IsValid only checks nonempty shape, not liveness. |
| `XBackendHit.event_id/source_id/tick` | Event identity, caller routing ID and half tick; ordinary same-phase edge hits can share event_id. |
| `slot/generation/kind/phase/flags` | Registration identity, kind, phase and reserved flags. |
| `value/x_mask` | 64-bit payload; read the full wide signal at the pause point. |

RunUntil currently returns RunLimit, TriggerHit, BackendStop, QuantumExpired or EdgeBarrier. BackendStop includes inability to advance a disabled clock. Other enum values (UserPause, SimulationClose, CallbackError, BackendError) are reserved protocol extensions. Specific errors may still throw exceptions.

Matching, callback and capacity errors can throw after time has advanced. Use finally/RAII to clean registrations and ownership.

## Expressions, Trigger and Coverage

`ExprNewConst/Signal/Unary/Binary/Compare/CompareSigSig/CompareSigConstBytes/CompareConstBytesSig/MaskedCompareSigConstBytes` create roots in the engine's own ExprEngine. Operation arguments use integer ExprOp values; see the [API index](api-index.md).

Registration APIs are described in [Trigger](triggers.md). `AttachCoverage/CoverageSnapshot/ResetCoverage/CoverageExecutionCount/CoverageVersion` are described in [Coverage](coverage.md). Keep frontend definitions and borrowed signals consistent after registration; do not restructure them in execution callbacks.

## Typed expressions and collection

`ExprNewBitsConst/ExprNewBool/ExprNewBitsSignal/ExprNewBits/ExprEvalBits` express explicit width and signedness. `ExpressionNodeCount/ExpressionConstantCount` inspect storage.

`CollectExpressions(retained_roots)` runs at a safe point outside execution. It retains occupied registrations, including fired registrations waiting to rearm, Coverage references, and supplied client roots. It returns an old-to-new mapping: remap every client-held root; -1 means discarded. Native references, matching history and statistics survive. Collection neither frees registrations nor advances time. See [typed expressions and collection](../guides/typed-expressions.md).
