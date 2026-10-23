# 第一个信号与事件

这个练习会创建一个 128-bit 信号，推进时钟，然后注册一个在三个周期后触发的事件。先完成[安装](../guides/installation.md)，使用已安装 `xspcomm` 的 Python 环境。

## 1. 保存一个宽信号

将下面的代码保存为 `first_event.py`：

<!-- runnable:python -->
```python
import xspcomm as xs

signal = xs.XData(128, xs.XData.InOut)
signal.value = (1 << 100) | 0x5A
print(hex(signal.value))
assert signal.value == (1 << 100) | 0x5A
```

运行 `python3 first_event.py`，应得到：

```text
0x1000000000000000000000005a
```

`XData(128, ...)` 指定位宽，`InOut` 允许读写。`.value` 使用 Python 整数，因此可以保存这里的第 100 位和低位 `0x5A`。

## 2. 推进时钟

将脚本替换为：

<!-- runnable:python -->
```python
import xspcomm as xs

clock = xs.XClock(lambda dump: 0)
clock.Step(2)
print(f"cycles={clock.clk}, half_ticks={clock.GetHalfTick()}")
```

再次运行，得到：

```text
cycles=2, half_ticks=4
```

`XClock` 接受一个执行函数。练习中的空函数让我们可以单独观察时间计数；接入电路时，这里调用模拟器。`Step(2)` 执行两个完整周期，每个周期有两个半周期。

## 3. 等待三个周期

接着用执行引擎等待一个事件。将脚本替换为以下完整代码：

<!-- runnable:python -->
```python
import xspcomm as xs

clock = xs.XClock(lambda dump: 0)
clock.Step(2)
engine = xs.XTriggerEngine(clock)
engine.AcquireClock()
try:
    handle = engine.ArmClockCycles(3)
    result = engine.RunUntil(20)
    assert result.advanced_ticks == 6
    assert len(result.hits) == 1
    assert result.hits[0].slot == handle.slot
    print(f"advanced={result.advanced_ticks}, hits={len(result.hits)}")
    engine.Disarm(handle)
finally:
    engine.Clear()
    engine.ReleaseClock()
clock.Step(1)
print(f"cycles={clock.clk}")
assert clock.clk == 6
```

运行后得到：

```text
advanced=6, hits=1
cycles=6
```

`AcquireClock()` 将时钟推进交给引擎。`ArmClockCycles(3)` 注册等待三个周期的事件，`RunUntil(20)` 最多推进二十个半周期。事件在六个半周期后命中，引擎随即返回，所以 `advanced_ticks` 为 6。

`Disarm()` 移除已经处理的注册。清理并 `ReleaseClock()` 后，脚本又手动推进一个周期，总计为 `2 + 3 + 1 = 6`。

下一步可以把周期事件换成[信号条件和覆盖率采样](../guides/events-coverage.md)。接口签名和采样相位见 [XEngine](../reference/engine.md) 与 [Trigger](../reference/triggers.md)。
