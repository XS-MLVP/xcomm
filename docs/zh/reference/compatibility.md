# 版本、兼容性与迁移

本文说明 xcomm 的 API、原生 ABI 和描述协议。Python 发行包名为 `uc-xcomm`，
导入名为 `xspcomm`。wheel 与源码构建方式见[安装](../guides/installation.md)。

## 版本层次

| 层次 | 取值/查询方式 | 用途 |
| --- | --- | --- |
| Python 包/原生版本字符串 | `version()` 与包元数据 | 标识包的构建版本 |
| 原生 C++ ABI | 4 | 对象布局与动态链接；`abi_version()`、libxspcomm.so.4 |
| Coverage 描述协议 | 5 | 前端生成 point/bin/program 的结构；`XTriggerEngine.CoverageVersion()` |

Picker 与 xcomm 使用匹配的工具链。Picker 在 `.build-config.yml` 中记录原生源码依赖；
升级工具链后重新生成 DUT 和布局 YAML。

发布 wheel 按 CPython 小版本及平台构建，矩阵覆盖 Linux x86-64、CPython 3.11/3.12 和
GLIBC 2.17/2.28。源码构建及 C++ SDK 用法见安装指南。

定长多维 unpacked 数组使用共享的 DPI 或原生内存绑定。DPI/native 数组路径面向
**Verilator 5.026 及更新版本**，回归验证版本为 **5.026、5.048、5.052**。详见
[定长 unpacked 数组](xdata.md#unpacked-arrays)。

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
