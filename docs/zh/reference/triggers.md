# Trigger：原生事件与时序匹配

描述头文件：[xpattern.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xpattern.h)；注册入口：[XEngine](engine.md)。Trigger 内核处理条件成立、序列推进和 FSM 终结。普通等待命中后将控制权交回调用方；持续统计使用 [Coverage](coverage.md)。

## 注册 API

以下方法属于 XEngine，注册前先 AcquireClock。返回 handle，应检查 IsValid；无效信号/程序、零周期或容量不足等情况可能返回无效 handle。它们不会自动创建 Python await 对象。

| API | 等待什么 |
| --- | --- |
| `ArmEdge(phase, source_id=0)` | 下一次对应采样相位，默认边沿来源标识为 clock 地址。 |
| `ArmClockCycles(cycles, phase=RisingStable, source_id=0)` | cycles 次指定相位；默认相当于完整周期数，cycles 必须大于零。 |
| `ArmValueEq(signal, uint64, phase, mode, source_id)` | W <= 64 信号的已知值等于常量。 |
| `ArmValueEqBytes(signal, bytes, phase, mode, source_id)` | W > 64 信号与 little-endian 字节常量等值；其余参数需显式传入。 |
| `ArmValueChange(signal, phase, source_id)` | 相对注册/Rearm 时的值变化，包括四态 mask 变化；支持宽信号。 |
| `ArmSample(phase, source_id)` | 在下一个指定相位返回一次采样 hit，适合前端自己处理采样。 |
| `ArmExpr(root, phase, mode, source_id)` | engine 内部表达式 root 的条件事件。 |
| `ArmSequence(steps, phase, source_id)` | 一组 XSequenceStep 完成。 |
| `ArmFsm(state_count, start_state, transitions, phase, source_id)` | FSM 的 trigger 转移完成。 |

source_id 是调用方路由事件的整数标识；部分信号入口在 0 时使用信号地址。它不提供引用计数或对象所有权。

## 条件事件模式

| XConditionMode | 语义 |
| --- | --- |
| Enter | 已知条件从假进入真；首次采样为真也可命中。 |
| EachSample | 每个已知且为真的采样点可命中。 |
| Change | 已知条件的布尔结果发生变化，包含真变假。 |

普通注册命中后会变为未 armed，但槽仍占用。调用 Rearm 可以复用；Expr/ValueEq 的条件历史不会因此一概清零，所以 Enter 模式再次命中可能需要先经过假再变真。需要任意数据变化时使用 ArmValueChange。

未知条件不会命中，并保留最近的已知条件历史。普通 ValueEqBytes 的 hit.value 不包含完整宽值，宽变化 hit 也不携带完整字节快照；在暂停点读取信号，或由上层保存需要的数据。

## 序列描述

`XSequenceStep` 的字段为 kind、root、minimum、maximum、cycles。每次指定相位到来推进一个步骤；完成本步不会在同一次采样中继续吃掉下一步。

| XSequenceStepKind | 使用字段与语义 |
| --- | --- |
| Wait | root；一直等到条件已知且为真。 |
| Next | root；下次该步骤的采样必须成立，否则整条尝试从头开始。 |
| Within | root、minimum、maximum；该步骤年龄从 1 计，在闭区间内成立即完成，超过 maximum 超时重置。 |
| Hold | root、cycles；连续已知为真达到次数；中断清零本步连续计数，再继续等待。 |

minimum/maximum/cycles 以注册相位的采样次数为单位。RunUntil 的预算以半周期为单位。

<!-- runnable:python -->
```python
import xspcomm as xs

clock = xs.XClock(lambda dump: 0)
engine = xs.XTriggerEngine(clock)
engine.AcquireClock()
handle = None
try:
    root = engine.ExprNewConst(1)
    first = xs.XSequenceStep()
    first.kind = xs.XSequenceStepKind_Wait
    first.root = root
    second = xs.XSequenceStep()
    second.kind = xs.XSequenceStepKind_Next
    second.root = root
    handle = engine.ArmSequence(xs.XSequenceStepVector([first, second]))
    assert handle.IsValid()
    result = engine.RunUntil(10)
    assert result.advanced_ticks == 4
    assert len(result.hits) == 1
finally:
    if handle is not None and handle.IsValid():
        engine.Disarm(handle)
    engine.ReleaseClock()
```

## FSM 与程序描述

`XFsmTransition` 字段：from_state、root、next_state、terminal_id、trigger。root == -1 表示无条件；当前状态的转移按声明顺序检查，采用首条匹配转移；trigger == true 表示完成并输出 terminal_id，否则进入 next_state。状态编号必须落在 state_count 范围内。

`XTriggerProgram` 统一描述 Expr、Sequence、Fsm：

| 字段 | 用途 |
| --- | --- |
| kind | XTriggerProgramKind_Expr/Sequence/Fsm |
| root / steps / transitions | 按 kind 选择一套有效载荷，不混合使用 |
| state_count/start_state | FSM 的状态范围和起点 |
| mode | 表达式条件事件模式；时序程序使用 Enter |
| overlap/max_active | Coverage 匹配执行的重叠尝试配置 |

ArmSequence/ArmFsm 的普通等待本身走独立单条尝试；完整程序的 overlap 主要用于 Coverage source/bin。重叠 Sequence 必须以 Wait 开始，max_active 要为正；非重叠要求 max_active == 1。Pattern Expr 程序须显式设 overlap=false。

注册后程序被复制，信号引用仍借用原对象。root 仅在创建它的 XEngine 中有效。
