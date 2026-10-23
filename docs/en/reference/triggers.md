# Trigger: native events and temporal matching

Descriptors: [xpattern.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xpattern.h). Register through [XEngine](engine.md). Matching handles conditions, sequences and FSM completion. Ordinary waits return control on a hit; [Coverage](coverage.md) continuously counts matching results.

## Registration APIs

AcquireClock before registering. Check the returned handle's IsValid: invalid signals/programs, zero cycles or exhausted capacity can produce an invalid handle. These methods do not create Python await objects.

| XEngine API | Wait condition |
| --- | --- |
| `ArmEdge(phase, source_id=0)` | Next selected phase; default edge source ID uses the clock address. |
| `ArmClockCycles(cycles, phase=RisingStable, source_id=0)` | Positive count of selected phases, equivalent to cycles with the default phase. |
| `ArmValueEq(signal, uint64, phase, mode, source_id)` | Known W<=64 signal equal to a constant. |
| `ArmValueEqBytes(signal, bytes, phase, mode, source_id)` | W>64 signal equal to little-endian bytes; supply the remaining arguments explicitly. |
| `ArmValueChange(signal, phase, source_id)` | Change from registration/Rearm value, including four-state mask changes and wide signals. |
| `ArmSample(phase, source_id)` | One sampling hit at the next selected phase for frontend processing. |
| `ArmExpr(root, phase, mode, source_id)` | Condition event from the engine's own root. |
| `ArmSequence(steps, phase, source_id)` | Completion of XSequenceStep descriptors. |
| `ArmFsm(state_count, start_state, transitions, phase, source_id)` | Completion of a trigger transition in an FSM. |

source_id is an integer routing identifier; some signal APIs use the signal address for zero. It does not confer ownership or reference counting.

## Condition modes

| XConditionMode | Meaning |
| --- | --- |
| Enter | Known false-to-true entry; the first true sample can fire. |
| EachSample | Each known true sample can fire. |
| Change | Change of a known condition's Boolean result, including true-to-false. |

An ordinary hit disarms execution but leaves its slot occupied. Rearm reuses it. Expr/ValueEq condition history is not universally cleared by rearming: Enter may need an intervening false sample. Use ArmValueChange for data changes.

Unknown conditions do not hit and preserve the previous known condition history. ValueEqBytes and wide-change hits do not contain complete byte snapshots in hit.value. Read the signal at the pause point or retain data in the frontend.

## Sequence steps

XSequenceStep fields are kind, root, minimum, maximum and cycles. Each selected phase advances one step; completion does not consume the next step in the same sample.

| Kind | Fields and behavior |
| --- | --- |
| Wait | root; wait for known true. |
| Next | root; the step's next sample must be true or restart the attempt. |
| Within | root, minimum, maximum; age starts at 1, completes inside the closed interval and expires past maximum. |
| Hold | root, cycles; require consecutive known true samples. Interruption clears this step's consecutive count and continues waiting. |

Bounds count samples at the registration phase. RunUntil budgets use half cycles.

<!-- runnable:python -->
```python
import xspcomm as xs

clock = xs.XClock(lambda dump: 0)
engine = xs.XTriggerEngine(clock)
engine.AcquireClock()
handle = None
try:
    root = engine.ExprNewConst(1)
    first = xs.XSequenceStep()
    first.kind = xs.XSequenceStepKind_Wait
    first.root = root
    second = xs.XSequenceStep()
    second.kind = xs.XSequenceStepKind_Next
    second.root = root
    handle = engine.ArmSequence(xs.XSequenceStepVector([first, second]))
    assert handle.IsValid()
    result = engine.RunUntil(10)
    assert result.advanced_ticks == 4
    assert len(result.hits) == 1
finally:
    if handle is not None and handle.IsValid():
        engine.Disarm(handle)
    engine.ReleaseClock()
```

## FSMs and programs

XFsmTransition contains from_state, root, next_state, terminal_id and trigger. root=-1 is unconditional. Inspect transitions in declaration order and take the first match. A trigger transition completes with terminal_id; other transitions enter next_state. States must be within state_count.

XTriggerProgram describes Expr, Sequence or Fsm:

| Field | Purpose |
| --- | --- |
| kind | XTriggerProgramKind_Expr/Sequence/Fsm |
| root / steps / transitions | Select one payload matching kind |
| state_count/start_state | FSM range and initial state |
| mode | Condition-event mode; temporal programs use Enter |
| overlap/max_active | Concurrent-attempt configuration for Coverage matching |

Ordinary ArmSequence/ArmFsm waits use a single independent attempt. Program overlap is primarily used by Coverage sources/bins. Overlapping sequences start with Wait and require positive max_active. Non-overlap requires max_active=1. Explicitly set overlap=false on Pattern Expr programs.

Descriptors are copied at registration; referenced signals remain borrowed. Roots are valid within their creating engine.
