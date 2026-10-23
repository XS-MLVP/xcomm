# Coverage: points, bins and native statistics

Header: [xcoverage.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xcoverage.h). Coverage reuses Trigger matching and adds classification/counting. XEngine executes it at registered sampling points.

## Three layers

- A **sampling-source registration** determines when to sample: an edge, a condition, or source sequence/FSM completion.
- **XCoveragePoint** chooses what to observe: a value, a temporal pattern or a cross of value points.
- **XCoverageBin** defines a category: parent point, matching program, bin kind, FSM terminals or cross tuple.

An ordinary trigger normally makes RunUntil return. Attaching Coverage makes that registration persistent; ordinary sampling hits become statistics. Register a separate ArmClockCycles or other ordinary trigger to stop execution.

## Runnable example

<!-- runnable:python -->
```python
import xspcomm as xs

signal = xs.XData(8, xs.XData.InOut)
signal.value = 7
clock = xs.XClock(lambda dump: 0)
engine = xs.XTriggerEngine(clock)
engine.AcquireClock()
handle = None
try:
    root = engine.ExprNewCompare(
        xs.ExprOp_EQ, engine.ExprNewSignal(signal), engine.ExprNewConst(7))
    point = xs.XCoveragePoint()
    point.signal = signal
    program = xs.XTriggerProgram()
    program.root = root
    program.overlap = False
    bin_desc = xs.XCoverageBin()
    bin_desc.point_id = 0
    bin_desc.program = program
    handle = engine.ArmEdge(xs.XPhase_RisingStable)
    assert handle.IsValid()
    engine.AttachCoverage(handle, xs.XCoveragePointVector([point]),
                          xs.XCoverageBinVector([bin_desc]))
    result = engine.RunUntil(10)
    snapshot = engine.CoverageSnapshot(handle)
    assert len(result.hits) == 0
    assert snapshot.counters[0] == 5
    assert snapshot.counters[-1] == 5
finally:
    if handle is not None and handle.IsValid():
        engine.Disarm(handle)
    engine.ReleaseClock()
```

## Point and bin descriptors

| Descriptor/field | Meaning and constraints |
| --- | --- |
| `XCoveragePoint.kind` | Value, Pattern or Cross; defaults to Value. |
| `.signal` | Required for Value; absent for Pattern/Cross. |
| `.gate` | Point-level root; -1 disables gating. |
| `.dimensions` | Cross references earlier Value point IDs. |
| `XCoverageBin.point_id` | Zero-based parent index in points. |
| `.kind` | Normal, Ignore, Illegal or Default; defaults to Normal. |
| `.program` | XTriggerProgram. Ordinary Value bins use Expr; full temporal matching uses Pattern points. |
| `.terminals` | Pattern FSM completion selection; empty selects all terminals. |
| `.dimensions` | Cross tuple of Normal bin IDs, indexed across the complete bins array. |

Within one Pattern point, identical roots/steps/transitions/mode/overlap/capacity can share matching execution. Terminal selection and bin kind do not prevent sharing. Sharing is determined by comparing descriptor fields. Different points and registrations retain independent histories.

Simple Value bins evaluate on every eligible sample instead of counting Pattern Expr Enter events. Adjacent-transition Value bins support Wait/Next only; full Wait/Within/Hold/FSM programs use Pattern. Pattern does not support Default bins.

## Management APIs

All methods belong to XEngine.

| API | Purpose and contract |
| --- | --- |
| `CoverageVersion()` | Descriptor protocol 5, distinct from ABI and package version. |
| `AttachCoverage(handle, points, bins, gate=-1, abort=-1, raise_illegal=true, diagnostic_capacity=1024, overlap=false, max_active=1, diagnostics=false)` | Attach once to a valid registration; nonempty point/bin arrays and positive capacities required. |
| `CoverageSnapshot(handle, progress=false)` | Copy counters/diagnostics without sampling or advancing. |
| `ResetCoverage(handle, counters=true)` | Clear source/bin matching history. With true, also clear counters/diagnostics and increment epoch. |
| `CoverageExecutionCount(handle)` | Number of matching execution instances currently held by deduplicated Pattern bins. |

AttachCoverage overlap/max_active govern the sampling source. bin.program fields govern the bin's own attempts; these are separate layers. Capacity exhaustion throws an exception.

Sequence/FSM sources can finish several attempts in one phase and produce several group samples. Bin progress follows group samples; source completion is not automatically one sample per clock edge.

## Classification, gating and errors

Value classification priority is Illegal > Ignore > Normal, then Default if none match. Multiple bins at the same priority may count. Pattern bins count independent completions; Ignore/Illegal do not suppress other Pattern bins in that point.

Group/point gates must be known true. Closing gates clears affected bin history. Known-true abort clears source/bin history and skips processing while retaining counts. X/Z Value signals are not valid known samples and increment unknown.

Illegal hits throw RuntimeError after recording by default; raise_illegal=false still records them. Diagnostic capacity exhaustion, counter overflow and active-attempt exhaustion also throw. A run that throws may have partially updated state. incomplete marks some capacity/diagnostic failures, not every possible exception.

Cross correlates Normal bins from earlier Value points in the same group sample. Pattern crosses and automatic correlation across temporal completion events are unsupported.

## Snapshot layout

For P points and B bins, counters has length `2 + 5*P + B`:

| Offset | Meaning |
| --- | --- |
| 0, 1 | Group samples, group gated |
| Five entries at `2 + 5*p` | Point samples, gated, ignored, unmatched, unknown |
| `2 + 5*P + b` | Count for bin b |

Unknown also increments point gated. A gated group increments only group gated, not each point. These metrics cannot be combined into a percentage without a defined coverage policy.

generation/epoch/tick identify registration generation, reset epoch and current half tick. illegal_bins/illegal_values/illegal_ticks capture bin IDs, full hexadecimal values and time at the illegal hit.

With diagnostics enabled, source and each bin have seven entries: started, completed, failed, expired, aborted, cleared, peak_active. progress=true adds five-column rows: pattern ID, step, age, held, FSM state. ID 0 is source; bin b uses b+1. Progress lists currently active matching history.

## Frontend integration

Language-level CoverPoint/CoverBin definitions can lower to these descriptors without Python callbacks on every hit. The API does not supply a complete DSL, report renderer or universal percentage formula. Arbitrary Python predicates and pytrigger scheduling belong to the frontend; native registrations can coexist in the same engine.
