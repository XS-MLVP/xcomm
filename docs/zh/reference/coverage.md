# Coverage：point、bin 与原生统计

头文件：[xcoverage.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xcoverage.h)。Coverage 使用 Trigger 的匹配能力，并负责分类和计数。由 XEngine 在注册的采样点执行。

## 三层关系

- **采样源注册**决定什么时候采样：某边沿、条件成立或源序列/FSM 完成。
- **XCoveragePoint** 决定观察什么：信号值、事件模式或一组值点的 cross。
- **XCoverageBin** 决定如何归类：属于哪个 point、匹配程序、bin 类别、FSM terminal 或 cross tuple。

普通 Trigger 命中通常让 RunUntil 返回；附加 Coverage 后，该注册持续工作，普通采样命中转为统计更新。需要按周期停止时另注册 ArmClockCycles 等普通 trigger。

## 最小可运行例子

<!-- runnable:python -->
```python
import xspcomm as xs

signal = xs.XData(8, xs.XData.InOut)
signal.value = 7
clock = xs.XClock(lambda dump: 0)
engine = xs.XTriggerEngine(clock)
engine.AcquireClock()
handle = None
try:
    root = engine.ExprNewCompare(
        xs.ExprOp_EQ, engine.ExprNewSignal(signal), engine.ExprNewConst(7))
    point = xs.XCoveragePoint()
    point.signal = signal
    program = xs.XTriggerProgram()
    program.root = root
    program.overlap = False
    bin_desc = xs.XCoverageBin()
    bin_desc.point_id = 0
    bin_desc.program = program
    handle = engine.ArmEdge(xs.XPhase_RisingStable)
    assert handle.IsValid()
    engine.AttachCoverage(handle, xs.XCoveragePointVector([point]),
                          xs.XCoverageBinVector([bin_desc]))
    result = engine.RunUntil(10)
    snapshot = engine.CoverageSnapshot(handle)
    assert len(result.hits) == 0
    assert snapshot.counters[0] == 5
    assert snapshot.counters[-1] == 5
finally:
    if handle is not None and handle.IsValid():
        engine.Disarm(handle)
    engine.ReleaseClock()
```

## Point 与 bin 描述

| 描述/字段 | 作用与约束 |
| --- | --- |
| `XCoveragePoint.kind` | Value、Pattern、Cross；默认为 Value。 |
| `.signal` | Value 必须提供 XData；Pattern/Cross 不带 signal。 |
| `.gate` | point 级表达式 root，-1 表示不设置门控。 |
| `.dimensions` | Cross 引用此前声明的 Value point ID。 |
| `XCoverageBin.point_id` | 所属 point 在 points 数组中的零基索引。 |
| `.kind` | Normal、Ignore、Illegal、Default，默认 Normal。 |
| `.program` | XTriggerProgram。Value 的普通 bin 使用 Expr；完整时序模式使用 Pattern point。 |
| `.terminals` | Pattern FSM 的完成终结选择，空数组表示全部 terminal。 |
| `.dimensions` | Cross 对应的 normal bin ID 元组，ID 在整个 bins 数组中编号。 |

同一 Pattern point 下，root/steps/transitions/mode/overlap/capacity 等完全一致的程序描述可共享匹配执行，terminal 选择和 bin kind 不影响这份执行共享。共享判断按程序描述的各字段比较。不同 point 或注册的历史不共享。

Value point 的简单 bin 在每个有效采样上判定条件，不套用 Pattern Expr 的 Enter 事件计数。Value 的相邻 transition bin 只允许 Wait/Next；完整 Wait/Within/Hold/FSM 使用 Pattern point。Pattern 不支持 Default bin。

## 管理 API

这些方法都属于 XEngine。

| API | 作用与约束 |
| --- | --- |
| `CoverageVersion()` | 当前描述协议为 5；不等于原生 ABI 或包版本。 |
| `AttachCoverage(handle, points, bins, gate=-1, abort=-1, raise_illegal=true, diagnostic_capacity=1024, overlap=false, max_active=1, diagnostics=false)` | 为一个有效注册附加统计状态；不能重复附加。points/bins 非空，容量为正。 |
| `CoverageSnapshot(handle, progress=false)` | 返回计数与诊断的副本；不重新采样或推进。 |
| `ResetCoverage(handle, counters=true)` | 清除采样源和 bins 的匹配历史；true 时同时清零计数/诊断、增加 epoch，false 保留统计。 |
| `CoverageExecutionCount(handle)` | 当前 Pattern bins 去重后持有的匹配执行实例数。 |

AttachCoverage 的 overlap/max_active 控制**采样源**的重叠；bin.program 中的同名字段控制**bin 自己**的匹配。两层分别配置。超过活跃尝试容量会抛出异常。

采样源为 Sequence/FSM 时，一次相位可能完成多个尝试，并产生多次 group sample。bin 匹配随 group sample 推进，不能将“源完成后的一次采样”自动解释为每个时钟边沿。

## 分类、门控与错误

Value point 的同次匹配优先级为 Illegal > Ignore > Normal；没有这些命中时才考虑 Default。同优先级多个 bin 可以同时计数。Pattern bins 各自根据完成结果计数，Ignore/Illegal 不压制同 point 中其他 Pattern bin 的结果。

group/point gate 为已知真时才启用；门控关闭会清除受影响的 bin 历史。abort 为已知真时清除源和 bin 历史并跳过本次处理，保留计数。Value 信号含 X/Z 时不作为有效已知值采样，并记录 unknown。

默认 Illegal 命中在记录后抛 RuntimeError；raise_illegal=false 仍记录。诊断容量耗尽、计数溢出、重叠容量耗尽也会抛异常；不能把异常后的统计当作一次完整成功运行。incomplete 标记部分容量/诊断溢出情况，不能替代异常处理。

Cross 只关联此前的 Value points 的 Normal bins，组合按同一次 group sample 计算；不支持 Pattern point cross 或跨时序完成事件的自动相关。

## Snapshot 布局

P 个 points、B 个 bins 时，`counters` 长度为 `2 + 5*P + B`：

| 位置 | 含义 |
| --- | --- |
| 0、1 | group samples、group gated |
| `2 + 5*p` 开始的五项 | point samples、gated、ignored、unmatched、unknown |
| `2 + 5*P + b` | bin b 的计数 |

unknown 会同时增加对应 point 的 gated 计数；group gated 时只增加 group gated，不把它展开为每个 point 的 gated。各指标应按其定义解释，不假定它们能直接拼成覆盖率百分比。

`generation/epoch/tick` 标识注册代次、统计重置周期和当前 half tick。`illegal_bins/illegal_values/illegal_ticks` 保存非法 bin ID、命中时的完整十六进制值和时间。

启用 diagnostics 后，source 和每个 bin 各有七项：started、completed、failed、expired、aborted、cleared、peak_active。progress=true 另外返回五元组行：pattern ID、step、age、held、FSM state；pattern ID 0 为 source，bin b 使用 b+1。progress 列出当前仍活跃的匹配历史。

## 上层前端

Python 或其他语言的 CoverPoint/CoverBin 定义可以降低为以上描述，后端无需每次 hit 都回调 Python。描述 API 不自带完整语言 DSL、覆盖率报告渲染或统一的覆盖率百分比算法。任意 Python predicate 与 pytrigger 的组合调度属于前端；原生注册可在同一个 engine 中共存，具体 await 接口由该前端提供。
