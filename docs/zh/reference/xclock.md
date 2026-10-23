# XClock：仿真推进与边沿回调

头文件：[xclock.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xclock.h)。XClock 连接模拟器执行函数、时钟引脚和需要刷新的一组端口。它不创建模拟器，也不自动开始仿真。

## 时间与相位

默认 `stop_on_rise == true`，一个完整 `Step(1)` 依次经过 FallingStable 和 RisingStable。`StepHalf()` 只推进一个半周期。`clk` 在新周期的第一个半周期开始时增加；`GetHalfTick()` 每完成一个半周期增加。因此在半周期暂停时不能总用 `clk * 2` 代替 half tick。

`XPhase` 的 FallingStable/RisingStable 表示相应边沿执行及端口刷新后的采样点。DriveStable 是运行时在下降沿之后，允许前端驱动并刷新组合逻辑的额外采样点；它不增加 half tick，见 [XEngine](engine.md)。

执行函数参数 `bool dump` 区分执行/记录方式；时钟引脚由 XClock 设置。当前 `_step()` 不使用回调返回的 int 作为停止信号。

## API

| API | 用途与约束 |
| --- | --- |
| `XClock()`；`XClock(stepfunc, pins, ports)`；`ReInit(...)` | C++ 构造/绑定执行函数；Python 要求传入 callable 或函数地址。ReInit 更新入口并添加 pins/ports，不等于完整 Reset。 |
| `XClock(step_addr, dut_addr, ...)`；对应 ReInit | 地址入口 ABI 为 `int(uint64_t dut, uint64_t cycle, bool dump)`。 |
| `Add(XData)`；`AddPin(XData)` | 加入需要拉高/拉低的时钟引脚，并设为立即写入。 |
| `Add(XPort)`；`HasClockPin(data)` | 管理需要边沿提交/刷新的端口；检查是否已加入某时钟引脚。 |
| `Step(cycles)`；C++ `RunStep(cycles)` | 完整周期推进；在半周期中间不能调用 Step，先完成半周期。 |
| `StepHalf()` | 完成一个半周期，返回是否推进；Disable 时返回 false。 |
| `GetHalfTick/GetPhase/IsAtCycleBoundary` | 查询半周期时间、当前边沿相位和完整周期边界。 |
| `IsControlled()` | 当前是否由一个 XEngine 独占。 |
| `Reset()`；`default_stop_on_rise(bool)` | Reset 清零 xcomm 时间状态，不替 DUT 拉复位；后者选择完整周期结束的边沿。 |
| `RefreshComb/RefreshCombT`；`eval/eval_t` | 调用绑定执行函数，保持当前时间。 |
| `Disable/Enable/IsDisable` | 暂停/恢复推进；Disable 不释放 engine 的控制权。 |
| `SetFastMode/GetFastMode` | 选择哪些刷新/执行步骤可省略，见下表。 |
| `CSelf()` | 地址桥接。 |

接管期间 `Step/StepHalf/RunStep/Reset/ReInit/default_stop_on_rise` 被拒绝，受控时钟的复制和赋值也被拒绝。`RawStep()` 仍经过原生所有权检查。避免直接改公共 `clk/stop_on_rise` 或调用以下划线开头的辅助方法来推进。

## 回调

| API | 用途 |
| --- | --- |
| `StepRis/StepFal(callback, args, desc)` | 注册上升/下降沿回调，参数为周期号和用户参数；也有函数地址重载。 |
| `RemoveStepRisCbByDesc/RemoveStepFalCbByDesc` | 按描述删除。 |
| `RemoveStepRisCbByFunc/RemoveStepFalCbByFunc` | 按函数删除。 |
| `RemoveStepRisCb/RemoveStepFalCb` | 按函数与描述删除。 |
| `ListSteRisCbDesc/ListSteFalCbDesc` | 列出描述；Ste 是现有 API 拼写。 |
| `StepRisQueueSize/StepFalQueueSize` | 回调队列大小。 |
| `ClearRisCallBacks/ClearFalCallBacks` | 清空对应队列。 |

需要可管理生命周期的 C++ 检查器时，优先用 [XStepCallback.Attach/Detach](monitors.md)。时钟和 engine 在回调返回后继续推进。

## 分频与快速模式

`FreqDivWith(div, child_clock, shift=0)` 将子时钟的边沿活动安排到父时钟上；`FreqDivDelete(child)` 移除。调用方负责维持子时钟及关联信号的生命期，并避免对同一组引脚建立第二套独立驱动。

| FastMode | 省略的工作 |
| --- | --- |
| Default (0) | 标准双边沿流程 |
| IGNORE_READ_REFRESH (1) | 端口读刷新 |
| IGNORE_STEP_FALSE (2) | 上一项加上 step(false) |
| IGNORE_PORT_WRITE (3) | 再省略端口边沿写入，需使用立即写入 |
| ONLY_STEP_RIS (-1) | 只执行上升沿路径 |
| ONLY_STEP_FAL (-2) | 只执行下降沿路径 |

这些选项会改变可观察行为。先使用 Default 建立正确性，再针对模拟器和读写方式选择模式。

## 协程入口

C++ 在启用 C++20 coroutine 支持时提供 `AStep(n)`、`ACondition(checker)`、`ANext(n)`；它们产生等待对象，需要时钟/调度器继续运行才能唤醒。

Python 的 `AStep/ACondition/ANext/RunStep` 是单独的 asyncio 包装，语义和 C++ 同名方法不完全相同。它们在 engine 控制时钟时拒绝使用；应由上层 Execution 调度原生 trigger。见 [语言绑定](bindings.md)。
