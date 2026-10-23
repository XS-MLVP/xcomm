# Public API index

This index groups entry points by responsibility. Module pages explain arguments, return values, lifetimes and limits. Overloads are grouped by method family; constructors, destructors and operators belong to their object pages.

| Module | Public types/entry points | Details |
| --- | --- | --- |
| Signals | XData, PinBind, IOType, WriteMode, XDataBackendKind, xsvLogic, xsvLogicVecVal | [XData](xdata.md) |
| Ports | XPort | [XPort](xport.md) |
| Clocks | XClock, FastMode, XStep | [XClock](xclock.md) |
| Expressions | ExprEngine, ExprOp, ExprNode, NewBits, EvalBits, Collect | [ExprEngine](expressions.md) |
| Native runtime | XEngine / XTriggerEngine | [XEngine](engine.md) |
| Runtime results | XPhase, XHitKind, XStopReason, XRegistrationHandle, XBackendHit, XRunResult | [XEngine](engine.md) |
| Matching descriptors | XConditionMode, XSequenceStepKind, XSequenceStep, XFsmTransition, XTriggerProgramKind, XTriggerProgram | [Triggers](triggers.md) |
| Coverage | XCoveragePointKind, XCoverageBinKind, XCoveragePoint, XCoverageBin, XCoverageSnapshot | [Coverage](coverage.md) |
| Monitors | XStepCallback, XEcho, XConditionCheck, XExprCheck, XFsmMonitor, XRangeCheck | [Monitors](monitors.md) |
| Layouts | XSignalCFG, s_xsignal_cfg, s_xsignal_meta | [Signal configuration](signal-config.md) |
| Memory/strings | XByteBuffer, CString, array/pointer helpers | [Utilities](utilities.md) |
| Comparison | CompareOp, compare::Compare/Order/CompareWords/EqualWords/CompareSignedBytes/WithinRange | [Utilities](utilities.md) |
| Exact memory access | memory::Load/Store, UIntBytes, ByteWords | [Utilities](utilities.md) |
| Callbacks/coroutines | xfunction, XCorutine/xcorutine, XPromise, XCondition, XNext, schedule_awit | [Utilities](utilities.md) |
| TLM | tlm_msg, TLMSub, TLMPub, tlm_pbsb_run, step, tlm_vcs_init/tlm_vcs_step | [TLM](tlm.md) |
| VPI ABI | vpiHandle, s_vpi_value/s_vpi_vecval/s_vpi_time, func_vpi_get/get_value/put_value/get_str | [XData](xdata.md); definitions in [xvpi.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xvpi.h) |
| Versions/logging | version, abi_version, checkVersion, logging and formatting helpers | [Utilities](utilities.md) |
| Python extensions | value, BindDPIName, RawStep, asyncio helpers, info, thirdcall | [Bindings](bindings.md) |

## XEngine method lookup

| Methods | Purpose |
| --- | --- |
| AcquireClock, ReleaseClock | Clock ownership |
| ArmEdge, ArmClockCycles, ArmValueEq, ArmValueEqBytes, ArmValueChange, ArmSample, ArmExpr, ArmSequence, ArmFsm | [Event registration](triggers.md) |
| ExprNewConst, ExprNewSignal, ExprNewUnary, ExprNewBinary, ExprNewCompare, ExprNewCompareSigSig, ExprNewCompareSigConstBytes, ExprNewCompareConstBytesSig, ExprNewMaskedCompareSigConstBytes | [Engine expression graph](engine.md) |
| RunUntil, SamplePhase | Execution and phase sampling |
| Disarm, Rearm, RearmSample, ActiveCount, Capacity, ClearExecutionState, Clear | Registration and state management |
| CoverageVersion, AttachCoverage, CoverageSnapshot, ResetCoverage, CoverageExecutionCount | [Native coverage](coverage.md) |

## Choosing headers

Use xcomm.h for core APIs and xmonitor.h for monitors. Include individual headers to narrow dependencies: xdata.h, xport.h, xclock.h, xexpr.h, xengine.h, xpattern.h, xcoverage.h, xsignal_cfg.h, monitor/*, tlm/* and common/*.

runtime/types.h defines runtime protocol types without pulling in clocks or the runtime implementation. xtrigger.h provides the historical engine alias.

ExprNode's lhs/rhs/imm/sig fields represent graph internals. Construct nodes with New* methods instead of editing history fields. Underscore-prefixed XData/XClock helpers, TEST_* functions, private src headers and SWIG director storage types are not application entry points.

## v1.0 typed expressions and collection

ExprEngine provides `NewBitsConst/NewBool/NewBitsSignal/NewBits/EvalBits` and `NodeCount/ConstantCount/Collect`. XEngine exposes corresponding `ExprNew*` wrappers plus `ExpressionNodeCount/ExpressionConstantCount/CollectExpressions`. Python root mappings use XIntVector. See the [expression reference](expressions.md) and [task guide](../guides/typed-expressions.md).
