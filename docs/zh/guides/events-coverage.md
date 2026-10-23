# 等待事件并统计覆盖率

在等待事件期间，可以让同一个引擎持续采样并统计覆盖率。下面注册一个三周期事件，同时统计信号值为 7 的次数。

## 什么时候用哪个组件

- 需要“等三次上升沿/值变化/序列完成，再执行后续代码”：注册普通 Trigger，读取 RunUntil 的 hits。
- 需要“每次有效采样都累计某个值或模式的出现次数”：在采样源注册上 AttachCoverage。
- 需要“边沿执行一个 C++ 检查器并 Disable 时钟”：使用 Monitor Attach。

## 一起使用 Trigger 和 Coverage

<!-- runnable:python -->
```python
import xspcomm as xs

value = xs.XData(8, xs.XData.InOut)
value.value = 7
clock = xs.XClock(lambda dump: 0)
engine = xs.XTriggerEngine(clock)
engine.AcquireClock()
try:
    point = xs.XCoveragePoint()
    point.signal = value
    bin_spec = xs.XCoverageBin()
    bin_spec.point_id = 0
    bin_spec.program.root = engine.ExprNewCompare(
        xs.ExprOp_EQ, engine.ExprNewSignal(value), engine.ExprNewConst(7))
    sample = engine.ArmSample(xs.XPhase_RisingStable)
    assert sample.IsValid()
    engine.AttachCoverage(sample, xs.XCoveragePointVector([point]),
                          xs.XCoverageBinVector([bin_spec]))
    stop = engine.ArmClockCycles(3)
    assert stop.IsValid()
    result = engine.RunUntil(20)
    assert result.advanced_ticks == 6
    assert len(result.hits) == 1 and result.hits[0].slot == stop.slot
    snapshot = engine.CoverageSnapshot(sample)
    assert snapshot.counters[0] == 3
    assert snapshot.counters[-1] == 3
finally:
    engine.Clear()
    engine.ReleaseClock()

```

本例由 engine.RunUntil 推进仿真，Coverage 在三个采样点分别计数。第三个周期结束时，停止事件出现在 hits 中，调用返回。随后查询 snapshot，得到三次采样和三次 bin 命中。

## 从 point 到 bin

point 定义观察对象：Value 读一个信号，Pattern 观察时序程序，Cross 组合此前 Value points。bin 定义该 point 下的分类或完成条件；point_id 是 points 数组索引。一个 point 可以有多个 bin，每个 bin 保留自己的计数。

Value 用表达式定义简单 bin；Pattern 可用 Sequence 的 Wait/Next/Within/Hold 或 FSM。相同 Pattern point 下相同程序可共享执行，不同 point 各自保留历史。普通 Trigger 与 Coverage 使用相同的匹配内核。

## 正确解释统计

采样次数由采样源决定。采样源为上升沿时，每个上升沿采样一次；采样源为序列完成事件时，每次序列完成采样一次。group/point gate 控制采样，abort 清除匹配历史；ResetCoverage 可选择保留计数。

snapshot 读取已有统计。bin 的 Normal/Ignore/Illegal/Default 分类与计数布局见 [Coverage 参考](../reference/coverage.md)。上层框架根据覆盖率目标和这些计数计算百分比、生成报告。

## 接到 Python 或其他语言 DSL

前端将定义转换成 signal references、expression roots、XTriggerProgram、points/bins，再选择采样相位注册。原生描述可与同一个 engine 的其他 Trigger 共存。前端将支持的条件编译为 bin.program，并在语言侧处理其余 Python predicate、await 组合与 pytrigger 调度。
