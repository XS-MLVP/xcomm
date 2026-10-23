# XClock: simulation advancement and callbacks

Header: [xclock.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xclock.h). XClock connects a simulator execution function, clock pins and refreshable ports. It does not create a simulator or start running automatically.

## Time and phases

With default `stop_on_rise == true`, Step(1) passes FallingStable then RisingStable. StepHalf advances one half cycle. `clk` increments at the first half of a new cycle; GetHalfTick increments after each completed half. Thus `clk * 2` is not always the half tick when paused mid-cycle.

FallingStable/RisingStable follow edge execution and port refresh. DriveStable is an additional runtime sampling point after the falling edge, allowing the frontend to drive and refresh combinational logic without incrementing the half tick; see [XEngine](engine.md).

The execution callback's `bool dump` argument selects execution/recording behavior. XClock drives clock pins separately. The current `_step()` does not use the callback's int result as a stop signal.

## API

| API | Purpose and contract |
| --- | --- |
| `XClock()`; `XClock(stepfunc, pins, ports)`; `ReInit(...)` | C++ execution binding. Python requires a callable or function address. ReInit changes the entry point and adds pins/ports while retaining time state. |
| `XClock(step_addr, dut_addr, ...)`; corresponding ReInit | Address ABI: `int(uint64_t dut, uint64_t cycle, bool dump)`. |
| `Add(XData)`; `AddPin(XData)` | Add a clock pin and make it immediately writable. |
| `Add(XPort)`; `HasClockPin(data)` | Manage edge-updated ports / query a clock pin. |
| `Step(cycles)`; C++ `RunStep(cycles)` | Advance whole cycles. Finish a pending half cycle before calling Step. |
| `StepHalf()` | Complete a half cycle; returns whether it advanced, false when disabled. |
| `GetHalfTick/GetPhase/IsAtCycleBoundary` | Query time, edge phase and cycle boundary. |
| `IsControlled()` | Whether an XEngine owns the clock. |
| `Reset()`; `default_stop_on_rise(bool)` | Reset clock time; choose the ending edge of whole cycles. Apply RTL reset through its signal. |
| `RefreshComb/RefreshCombT`; `eval/eval_t` | Call the execution function while retaining the current time. |
| `Disable/Enable/IsDisable` | Pause/resume; disabling does not release ownership. |
| `SetFastMode/GetFastMode` | Select omitted work as below. |
| `CSelf()` | Native address bridge. |

Owned clocks reject Step/StepHalf/RunStep/Reset/ReInit/default_stop_on_rise and copying/assignment. RawStep still checks native ownership. Do not bypass it by editing clk/stop_on_rise or calling underscore-prefixed helpers.

## Callbacks

| API | Purpose |
| --- | --- |
| `StepRis/StepFal(callback, args, desc)` | Register edge callbacks receiving a cycle and user data; address overloads also exist. |
| `RemoveStepRisCbByDesc/RemoveStepFalCbByDesc` | Remove by description. |
| `RemoveStepRisCbByFunc/RemoveStepFalCbByFunc` | Remove by function. |
| `RemoveStepRisCb/RemoveStepFalCb` | Remove by function and description. |
| `ListSteRisCbDesc/ListSteFalCbDesc` | List descriptions; Ste is the existing spelling. |
| `StepRisQueueSize/StepFalQueueSize` | Query queue size. |
| `ClearRisCallBacks/ClearFalCallBacks` | Clear an edge's entire queue. |

Use [XStepCallback.Attach/Detach](monitors.md) for managed C++ monitor registration. Clock and engine advancement resumes after callbacks return.

## Divided clocks and fast modes

`FreqDivWith(div, child_clock, shift=0)` arranges child edges on a parent; `FreqDivDelete(child)` removes them. Keep the child and signals alive and avoid a second independent driver for the same pins.

| FastMode | Omitted work |
| --- | --- |
| Default (0) | Standard two-edge sequence |
| IGNORE_READ_REFRESH (1) | Port read refresh |
| IGNORE_STEP_FALSE (2) | Also step(false) |
| IGNORE_PORT_WRITE (3) | Also port edge writes; requires immediate writes |
| ONLY_STEP_RIS (-1) | Execute only the rising-edge path |
| ONLY_STEP_FAL (-2) | Execute only the falling-edge path |

These modes change observable behavior. Establish correctness with Default, then select a mode for the simulator/access path.

## Coroutine entry points

C++20 coroutine builds provide AStep(n), ACondition(checker) and ANext(n). These wait objects need the clock/scheduler to continue running.

Python AStep/ACondition/ANext/RunStep use separate asyncio wrappers with different semantics. They reject use while an engine owns the clock; an upper Execution layer should schedule native triggers. See [bindings](bindings.md).
