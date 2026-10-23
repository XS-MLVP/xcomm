# XEngine：原生运行时

头文件：[xengine.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xengine.h)，结果类型见 [runtime/types.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/runtime/types.h)。C++ 可以使用 `XEngine` 或兼容入口 `XTriggerEngine`；当前 SWIG 导出名称为 `XTriggerEngine`。

XEngine 管理一个时钟上的注册与采样顺序，并批量推进到事件或运行预算。它同时服务普通 Trigger 和 Coverage，自己不定义上层协程 API。

## 使用顺序

`构造 → AcquireClock → 创建表达式/注册 → RunUntil 或 SamplePhase → 处理结果 → Disarm 所有注册 → ReleaseClock`。最小可运行代码见 [快速开始](../tutorials/first-simulation.md)。

构造不会接管时钟。AcquireClock 对同一 engine 可重复调用；若时钟由另一 engine 控制、已销毁、正在推进或回调中，则抛异常。运行期间不能递归 RunUntil、Clear 或 ReleaseClock。

## 控制与资源 API

| API | 用途与约束 |
| --- | --- |
| `XEngine(clock, capacity=1024)` | 借用一个非空 XClock；capacity 必须大于零，是注册槽上限。 |
| `AcquireClock()` | 显式独占时钟；后续注册和运行必须具备所有权。 |
| `ReleaseClock()` | 释放独占；必须先移除所有 occupied 注册，包括已命中但未 Disarm 的注册。 |
| `ActiveCount()`；`Capacity()` | 查询 occupied 槽数量和容量；包括已命中且尚未移除的注册。 |
| `Disarm(handle)` | 移除注册并释放槽及 Coverage 状态；无效/过期 handle 返回 false。 |
| `Rearm(handle)` | 让已命中的占用注册再次等待；仍 armed 或已移除时返回 false。 |
| `RearmSample(handle)` | 仅用于已命中的 Sample 注册。 |
| `ClearExecutionState()` | 要求没有注册，清除表达式与执行缓存，保留时钟控制权。 |
| `Clear()` | 清除全部注册、表达式与运行缓存；调用方丢弃全部旧 handle/root，随后仍需 ReleaseClock。 |
| 析构 | 若时钟仍存活且由自己控制，则解除控制；调用方仍应显式管理正常退出流程。 |

Engine 不拥有 XClock，也不自动持有绑定信号。先销毁 clock 时，后续 acquire/run 会检测到并拒绝使用；这不替代正确的对象生命期安排。

## 运行 API

| API | 行为 |
| --- | --- |
| `RunUntil(max_half_ticks, max_wall_time_ns=0, budget_check_interval=64)` | 最多推进指定半周期；没有命中也可返回，时间预算为 0 表示不使用墙钟预算。 |
| `SamplePhase(XPhase::DriveStable)` | 只允许在 FallingStable 后调用；刷新组合逻辑并采样 DriveStable，不推进 half tick。 |

墙钟预算按 check interval 周期性检查，实际返回时间可能超过预算。RunUntil 在普通 hit、DriveStable 边界、Disable 或预算结束时返回；同一相位可带回多条 hit。

DriveStable 的使用顺序是：在下降稳定相位停下，前端写入驱动信号，然后 SamplePhase(DriveStable)，随后继续 RunUntil。它不会自动写入用户想要的激励。

## 返回结果

| 类型/字段 | 说明 |
| --- | --- |
| `XRunResult.advanced_ticks` | 本次 RunUntil 推进的半周期数，SamplePhase 为 0。 |
| `stopped_phase`；`stop_reason` | 停留采样相位和原因。 |
| `hits`；`IsPhaseBarrier()` | 本次调用的命中列表及相位屏障判断辅助。 |
| `XRegistrationHandle.slot/generation`；`IsValid()` | 注册槽和代次；IsValid 只判断非空形态，不能验证槽尚未过期。 |
| `XBackendHit.event_id/source_id/tick` | 事件编号、调用方来源标识、half tick；同相位的普通边沿命中可共享 event_id。 |
| `slot/generation/kind/phase/flags` | 对应注册身份、事件类型、相位及保留标记。 |
| `value/x_mask` | 64-bit 结果载荷；宽信号的完整值需在暂停点读取。 |

原生 RunUntil 当前返回 RunLimit、TriggerHit、BackendStop、QuantumExpired、EdgeBarrier；BackendStop 来自时钟不能继续推进，例如 Disable。枚举还包含 UserPause、SimulationClose、CallbackError、BackendError 供协议扩展使用，具体错误仍可能通过异常抛出。

匹配/回调/容量错误也可能直接抛异常；仿真可能已推进到发生错误的相位。上层应处理异常并在 finally/RAII 路径清理注册和所有权。

## 表达式、Trigger 与 Coverage

`ExprNewConst/Signal/Unary/Binary/Compare/CompareSigSig/CompareSigConstBytes/CompareConstBytesSig/MaskedCompareSigConstBytes` 在 engine 自己的 ExprEngine 中建立 root。枚举参数使用 ExprOp 的整数值；接口列表见 [API 索引](api-index.md)。

`Arm*` 方法见 [Trigger](triggers.md)，`AttachCoverage/CoverageSnapshot/ResetCoverage/CoverageExecutionCount/CoverageVersion` 见 [Coverage](coverage.md)。前端定义和数据指针在注册后应保持一致，不在运行回调里修改这套结构。

## 宽表达式与安全点回收

`ExprNewBitsConst/ExprNewBool/ExprNewBitsSignal/ExprNewBits/ExprEvalBits` 处理显式位宽与符号的表达式。`ExpressionNodeCount/ExpressionConstantCount` 查询存储规模。

`CollectExpressions(retained_roots)` 必须在没有运行调用时执行；自动保留全部 occupied 注册（包括已命中等待 rearm 的槽）及 Coverage 引用，同时保留调用方传入的 roots。返回 old-to-new 映射；调用方保存的每个 root 都要替换，未保留项为 -1。原生注册、匹配历史与统计保持有效。它不释放注册，也不推进时钟。完整示例见[宽表达式与回收](../guides/typed-expressions.md)。
