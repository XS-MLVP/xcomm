# Versions, compatibility and migration

This page describes xcomm's API, native ABI, and descriptor protocols. The Python
distribution is `uc-xcomm`, imported as `xspcomm`. See
[installation](../guides/installation.md) for wheels and source builds.

## Version layers

| Layer | Value / query | Purpose |
| --- | --- | --- |
| Python package/native version string | `version()` and package metadata | Identifies the package build |
| Native C++ ABI | 4 | Object layout and dynamic linking; `abi_version()` and libxspcomm.so.4 |
| Coverage descriptor protocol | 5 | Frontend point/bin/program structure; `XTriggerEngine.CoverageVersion()` |

Picker and xcomm use a matched toolchain. Picker records its native source
dependency in `.build-config.yml`; regenerate DUTs and layout YAML after
toolchain upgrades.

Release wheels target a CPython minor version and platform. The wheel matrix
covers Linux x86-64, CPython 3.11/3.12, and GLIBC 2.17/2.28. See the installation
guide for source builds and the C++ SDK.

Fixed multidimensional unpacked arrays use shared DPI or native-memory bindings.
The DPI/native array paths target **Verilator 5.026 and newer**, with regression
coverage on **5.026, 5.048, and 5.052**. See
[fixed unpacked arrays](xdata.md#fixed-unpacked-arrays).

## Header and name migration

Removed paths have no forwarding headers. Update includes directly:

| Previous path | Current path (below xspcomm/) |
| --- | --- |
| xcomuse.h | xmonitor.h |
| xcomuse/{callback,condition,range,expr,fsm}.h | monitor/{callback,condition,range,expr,fsm}.h |
| xcomuse/compare.h | common/compare.h |
| xcomuse/utils.h | common/memory.h |
| xfsm.h | monitor/fsm.h |
| xexpr.h for ComUseExprCheck | monitor/expr.h |
| common/operation.h | common/compare_op.h |
| detail/function.h, detail/lifetime.h | common/callback_storage.h, common/lifetime.h |
| common/memory/access.h | common/memory_access.h |
| trigger/types.h | runtime/types.h |
| tlm_msg.h, tlm_pbsb.h | tlm/message.h, tlm/pubsub.h |

| Previous name | Current name |
| --- | --- |
| ComUseStepCb, ComUseCondCheck | XStepCallback, XConditionCheck |
| ComUseExprCheck, ComUseRangeCheck | XExprCheck, XRangeCheck |
| ComUseFsmTrigger, ComUseEcho | XFsmMonitor, XEcho |
| ComUseDataArray, ComUseCondCmp | XByteBuffer, CompareOp |
| XCoverageItem, XCoverageItemKind, XCoverageItemVector | XCoveragePoint, XCoveragePointKind, XCoveragePointVector |
| bin.item_id | bin.point_id |
| Python XPin | Use XData directly |

These removed class names have no compatibility aliases. C++ `XTriggerEngine` remains an alias for `XEngine`; SWIG still exposes XTriggerEngine. Former detail/{trigger,coverage,data} implementation headers and xinstance.h are no longer installed. Applications use xpattern.h, xcoverage.h and xengine.h instead of private src headers.

## Coverage and scheduling migration

Protocol 5 uses explicit `XCoveragePoint.kind` (Value/Pattern/Cross), `XCoverageBin.kind` and `bin.program`. Protocol 4 frontends must move flat program fields into XTriggerProgram and replace point.pattern with kind. Pattern points need no artificial source; Cross currently references preceding Value points only. See [descriptors and counter layout](coverage.md).

After creating an engine, AcquireClock before registering and running. Clear all registrations and ReleaseClock when finished. Fired registrations still occupy slots until Disarm. Manual DUT.Step/clock.Step and legacy Python asyncio driving are rejected while owned; two entry points cannot drive the same clock concurrently.

Typed NewBits, EvalBits and explicit collection are new in v1.0. CompileExpr retains uint64 semantics. After Collect returns a root mapping, the frontend must replace every retained integer root. See the [typed expression guide](../guides/typed-expressions.md).
