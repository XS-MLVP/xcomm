# Versions, compatibility and migration

This documentation targets **xcomm v1.0.0**, published as the xspcomm package. See [installation](../guides/installation.md) for release assets and source builds.

## Three independent versions

| Layer | Current value | Purpose |
| --- | --- | --- |
| Python package/native version string | 1.0.0 | Installation and release identity; `version()` and package metadata |
| Native C++ ABI | 3 | Object layout and dynamic linking; `abi_version()` and libxspcomm.so.3 |
| Coverage descriptor protocol | 5 | Frontend point/bin/program structure; `XTriggerEngine.CoverageVersion()` |

Picker v2.1.0 sources require xspcomm>=1.0.0.dev0,<2 and pin native builds to c39db6652cd08d46df05d08f0bb5a033b83e4d2f, whose tree matches xcomm v1.0.0. Satisfying the package range does not guarantee compatibility with every existing compiled DUT. Rebuild DUTs and native clients when upgrading the ABI, and check which library is actually loaded.

Release wheels target a CPython minor version and platform. The 1.0.0 GitHub asset targets CPython 3.12 / Linux x86-64. Build from source for other environments. For source builds, use the Git tag or package metadata to identify the release; CMake supplies a fallback when package metadata is absent.

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
