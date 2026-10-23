# Monitors: checks on clock edges

Include [xmonitor.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xmonitor.h) for checks and diagnostic output attached to XClock. Monitors suit an edge callback that disables a clock when a condition matches. XEngine instead provides registrations, hit lists and native temporal execution.

## XStepCallback

Defined in [monitor/callback.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/monitor/callback.h). Subclasses implement Call; XConditionCheck, XExprCheck, XFsmMonitor and XEcho use this base.

| API | Behavior |
| --- | --- |
| `Attach(clock, rising=true)`; `Detach()` | Attach to an edge or remove all attachments. Reattaching to the same edge does not duplicate registration; destruction detaches automatically. |
| `Enable/Disable/IsDisable` | Enable or disable the checker itself, independently of XClock.Enable/Disable. |
| `SetMaxCbs(n)` | Limit the callback count, which the concrete checker updates. |
| `GetCbCount/IncCbCount/DecCbCount` | Read or explicitly change the count. Subclasses decide when to increment it. |
| `Reset()`; `cycle` | Reset enabled state and count; cycle is the most recently supplied clock cycle number. |
| `Call()` | Subclass work entry point. |
| `GetCb()`; `Cb(cycle,self)`; `CSelf()` | Raw function-address bridge. Prefer Attach in ordinary C++ code. |

Attach tracks clock and callback lifetimes. Keep borrowed signals, comparison values and user arguments alive yourself. Prefer Detach to clearing the clock's entire callback queue, which also removes other users' callbacks.

## XConditionCheck

Defined in [monitor/condition.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/monitor/condition.h). The constructor's clock and BindXClock select **clocks to stop on a hit**. They do not register edge callbacks; call Attach separately.

<!-- runnable:cpp -->
```cpp
#include "xspcomm/xclock.h"
#include "xspcomm/xmonitor.h"
#include <cassert>
using namespace xspcomm;

int main() {
    XClock clock([](bool) { return 0; });
    XData signal(8, XData::InOut), expected(8, XData::InOut);
    signal = 7;
    expected = 7;
    XConditionCheck check(&clock);
    check.SetCondition("match", &signal, &expected, CompareOp::EQ);
    check.Attach(&clock);
    clock.Step(10);
    assert(clock.IsDisable());
    assert(check.GetTriggeredConditionKeys().size() == 1);
    check.Detach();
}
```

| API | Behavior and constraints |
| --- | --- |
| `XConditionCheck(clock=nullptr)`; `BindXClock(clock)` | Select clocks to Disable on a hit. |
| `SetCondition(name, XData*, XData*, CompareOp, valid, valid_value, func, arg)` | Compare signals, with optional validity gate and custom comparison function. |
| `SetCondition(name, address, address, CompareOp, bytes, valid_addr, valid_value_addr, valid_bytes, func, arg)` | Compare equal-length native buffers as little-endian **signed** integers, including widths above 64 bits. |
| `GetValidCmpMode/SetValidCmpMode` | Gate comparison mode; default EQ. |
| `RemoveCondition/ListCondition/GetTriggeredConditionKeys` | Manage named conditions and query the latest check results. |
| `ClearClock/ClearCondition/ClearAll` | Clear stop targets, conditions, or both. These do not Detach. |
| `AsXDataXFunc/AsPtrXFunc` | Adapt native function addresses for custom comparisons. |
| `Call()` | Check all conditions. Any match disables target clocks and increments the callback count once. |

CompareOp contains EQ, NE, GT, GE, LT and LE. XData comparisons follow XData semantics; native pointer comparisons are signed; XRangeCheck uses unsigned ranges.

## XExprCheck

Defined in [monitor/expr.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/monitor/expr.h). It extends XConditionCheck, sharing named conditions, hit handling and stop targets while evaluating expressions with its own ExprEngine.

| API | Behavior |
| --- | --- |
| `ExprNewConst/Signal/Unary/Binary/Compare` | Construct basic expression nodes. |
| `ExprNewCompareSigSig/SigConst/ConstSig/SigConstBytes/ConstBytesSig` | Construct specialized signal comparison nodes. |
| `CompileExpr(text, cfg)` | Compile text; report errors and return -1 on parsing failure. |
| `SetExpr/RemoveExpr/ListExpr/GetTriggeredExprKeys` | Manage named expressions and latest results. Names share the SetCondition namespace. |
| `ClearExpr()`; `ClearAll()` | Clear expressions and internal nodes. Currently ClearAll behaves like ClearExpr and retains BindXClock targets; use ClearClock/Detach separately. |
| `Call()` | Evaluate conditions and expressions through the shared check entry. |

XExprCheck wraps scalar expressions and specialized wide comparisons. For typed NewBits and collection, use a separate [ExprEngine](expressions.md) or XEngine. Reusing a name replaces the existing condition or expression.

## XFsmMonitor

Defined in [monitor/fsm.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/monitor/fsm.h). This text FSM monitor is separate from Trigger's XFsmTransition descriptor API.

| API | Behavior |
| --- | --- |
| `XFsmMonitor(clock=nullptr)`; `BindXClock` | Choose clocks to disable; Attach is still required for sampling. |
| `LoadProgram(text, cfg)` | Load a text FSM; report errors and clear the program on failure. |
| `Reset()` | Return to the start state and clear flags, counters and trigger state. |
| `Clear()` | Remove the FSM program. |
| `IsTriggered/GetTriggeredState/GetCurrentState/ListStates` | Query state and matches. |
| `Call()` | Execute actions and choose transitions in order. |

Syntax includes `start NAME`, `state NAME:`, `if EXPR goto NAME`, `if EXPR trigger`, unconditional goto/trigger, set/clear on `$flag*`, and inc/reset on `$counter*`. Transitions are checked in source order.

## XRangeCheck and XEcho

[XRangeCheck](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/monitor/range.h) adapts a comparison function. It neither inherits XStepCallback nor attaches itself to a clock.

| API | Behavior |
| --- | --- |
| `XRangeCheck(range, bytes)` | bytes is the native comparison length, at least 1; range is an int tolerance. |
| `cmp(target, center, range)` | For range>=0, test center-range <= target <= center; for range<0, test center <= target <= center-range, without integer wraparound. |
| `ArrayCmp`; `GetArrayCmp()` | Pointer comparison entry/callback supporting unaligned and wide buffers. |
| `XDataCmp`; `GetXDataCmp()` | Full-width unsigned XData range check. Unknown values return false; bytes does not truncate XData. |
| `CSelf()` | Pass the adapter as callback arg and keep it alive. |

XEcho(valid_addr, data_addr, stderr_echo=true, fmt="%c", convert=0) prints data when valid is nonzero and inherits Attach/Detach. convert selects char/int/float/double/string; fmt must match that type. Use XEcho to print diagnostics during simulation.
