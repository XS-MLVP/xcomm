# 版本、兼容性与迁移

本文对应 **xcomm v1.0.0**。包名为 xspcomm，仓库名为 xcomm。v1.0.0 已发布；发行包与源码构建的区别见[安装](../guides/installation.md)。

## 三种独立的版本

| 层次 | 当前值 | 用途 |
| --- | --- | --- |
| Python 包/原生版本字符串 | 1.0.0 | 安装与发行识别；`version()`、包元数据 |
| 原生 C++ ABI | 3 | 对象布局与动态链接；`abi_version()`、libxspcomm.so.3 |
| Coverage 描述协议 | 5 | 前端生成 point/bin/program 的结构；`XTriggerEngine.CoverageVersion()` |

Picker v2.1.0 源码依赖范围为 xspcomm>=1.0.0.dev0,<2，原生构建固定到 c39db6652cd08d46df05d08f0bb5a033b83e4d2f；该提交与 xcomm v1.0.0 的源码树一致。包版本满足范围不代表任意已编译 DUT 都兼容：升级 ABI 时需重新构建 DUT 和原生客户端，并检查实际加载的库。

发布 wheel 按 CPython 小版本及平台构建。1.0.0 的 GitHub 资产提供 CPython 3.12 / Linux x86-64；其他环境按源码构建。CMake 的无包元数据默认版本只是构建回退值，不能用它替代 Git 标签/发行包版本。

## 旧头文件与名称迁移

删除的路径没有转发头；直接更新 include：

| 旧路径 | 当前路径（均位于 xspcomm/） |
| --- | --- |
| xcomuse.h | xmonitor.h |
| xcomuse/{callback,condition,range,expr,fsm}.h | monitor/{callback,condition,range,expr,fsm}.h |
| xcomuse/compare.h | common/compare.h |
| xcomuse/utils.h | common/memory.h |
| xfsm.h | monitor/fsm.h |
| xexpr.h 中的 ComUseExprCheck | monitor/expr.h |
| common/operation.h | common/compare_op.h |
| detail/function.h、detail/lifetime.h | common/callback_storage.h、common/lifetime.h |
| common/memory/access.h | common/memory_access.h |
| trigger/types.h | runtime/types.h |
| tlm_msg.h、tlm_pbsb.h | tlm/message.h、tlm/pubsub.h |

| 旧名称 | 当前名称 |
| --- | --- |
| ComUseStepCb、ComUseCondCheck | XStepCallback、XConditionCheck |
| ComUseExprCheck、ComUseRangeCheck | XExprCheck、XRangeCheck |
| ComUseFsmTrigger、ComUseEcho | XFsmMonitor、XEcho |
| ComUseDataArray、ComUseCondCmp | XByteBuffer、CompareOp |
| XCoverageItem、XCoverageItemKind、XCoverageItemVector | XCoveragePoint、XCoveragePointKind、XCoveragePointVector |
| bin.item_id | bin.point_id |
| Python XPin | 直接使用 XData |

上述删除的类名不提供兼容别名。C++ `XTriggerEngine` 是保留的 `XEngine` 别名，SWIG 仍暴露 XTriggerEngine。原来的 detail/{trigger,coverage,data} 实现头以及 xinstance.h 不再安装；应用改用 xpattern.h、xcoverage.h、xengine.h。

## Coverage 与调度迁移

协议 5 使用显式 `XCoveragePoint.kind`（Value/Pattern/Cross）、`XCoverageBin.kind` 和 `bin.program`。协议 4 前端要把平铺程序字段移入 XTriggerProgram，并用 kind 替换 point.pattern。Pattern 点不需要伪造 source；Cross 当前只引用此前声明的 Value 点。参见[描述与计数布局](coverage.md)。

创建 engine 后，先 AcquireClock，再注册和运行；结束时清空所有注册并 ReleaseClock。已触发但未 Disarm 的注册仍占槽。接管期间手动 DUT.Step/clock.Step 和旧 Python asyncio 驱动会被拒绝，不能用两套入口推进同一时钟。

typed NewBits、EvalBits 和显式回收是 v1.0 的新增能力；旧 CompileExpr 保留 uint64 语义。Collect 返回 root 映射后，前端必须替换所有保留的整数 root。详见[宽位运算指南](../guides/typed-expressions.md)。
