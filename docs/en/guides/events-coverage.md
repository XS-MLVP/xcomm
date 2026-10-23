# Wait for events while collecting coverage

Collect coverage in the same engine while waiting for an event. This example registers a three-cycle event and counts samples where a signal equals 7.

## Choose a component

- To wait for three rising edges, a value change or a completed sequence, register an ordinary Trigger and inspect RunUntil hits.
- To count values or patterns on each eligible sample, AttachCoverage to a sampling-source registration.
- To run a C++ edge checker and Disable a clock, attach a Monitor.

## Combine Trigger and Coverage

<!-- runnable:python -->
```python
import xspcomm as xs

value = xs.XData(8, xs.XData.InOut)
value.value = 7
clock = xs.XClock(lambda dump: 0)
engine = xs.XTriggerEngine(clock)
engine.AcquireClock()
try:
    point = xs.XCoveragePoint()
    point.signal = value
    bin_spec = xs.XCoverageBin()
    bin_spec.point_id = 0
    bin_spec.program.root = engine.ExprNewCompare(
        xs.ExprOp_EQ, engine.ExprNewSignal(value), engine.ExprNewConst(7))
    sample = engine.ArmSample(xs.XPhase_RisingStable)
    assert sample.IsValid()
    engine.AttachCoverage(sample, xs.XCoveragePointVector([point]),
                          xs.XCoverageBinVector([bin_spec]))
    stop = engine.ArmClockCycles(3)
    assert stop.IsValid()
    result = engine.RunUntil(20)
    assert result.advanced_ticks == 6
    assert len(result.hits) == 1 and result.hits[0].slot == stop.slot
    snapshot = engine.CoverageSnapshot(sample)
    assert snapshot.counters[0] == 3
    assert snapshot.counters[-1] == 3
finally:
    engine.Clear()
    engine.ReleaseClock()

```

engine.RunUntil advances the simulation while Coverage counts at three sampling points. At the end of the third cycle, the stopping event appears in hits and the call returns. The snapshot then reports three samples and three bin hits.

## From point to bin

A point defines what is observed: Value reads a signal, Pattern observes a temporal program, and Cross combines earlier Value points. A bin classifies values or completions within that point; point_id indexes the points array. One point may contain multiple independently counted bins.

Value uses expressions for simple bins. Pattern can use Sequence Wait/Next/Within/Hold steps or FSMs. Identical programs in one Pattern point can share execution, while different points keep separate histories. Ordinary Trigger and Coverage reuse the same matching kernel.

## Interpret statistics correctly

The sampling source determines the count: a rising-edge source samples at each rising edge, while a sequence-completion source samples at each completed sequence. Group/point gates control sampling; abort clears matching history. ResetCoverage can preserve counters.

Snapshots read existing statistics. See [Coverage reference](../reference/coverage.md) for Normal/Ignore/Illegal/Default classification and counter layout. Frontends use coverage goals and these counters to calculate percentages and generate reports.

## Connect a language DSL

A frontend lowers definitions into signal references, expression roots, XTriggerProgram and point/bin arrays, then selects a sampling phase. Native coverage registrations coexist with other triggers in one engine. The frontend compiles supported conditions into bin.program and handles remaining Python predicates, await composition, and pytrigger scheduling in the language runtime.
