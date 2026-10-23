# Your first signal and event

In this exercise, you will create a 128-bit signal, advance a clock, and register an event that fires after three cycles. Complete [installation](../guides/installation.md) first and use a Python environment with `xspcomm` installed.

## 1. Store a wide signal

Save this code as `first_event.py`:

<!-- runnable:python -->
```python
import xspcomm as xs

signal = xs.XData(128, xs.XData.InOut)
signal.value = (1 << 100) | 0x5A
print(hex(signal.value))
assert signal.value == (1 << 100) | 0x5A
```

Run `python3 first_event.py`. The output should be:

```text
0x1000000000000000000000005a
```

`XData(128, ...)` sets the width, and `InOut` allows reading and writing. `.value` uses Python integers, so it can hold both bit 100 and the low bits `0x5A`.

## 2. Advance a clock

Replace the script with:

<!-- runnable:python -->
```python
import xspcomm as xs

clock = xs.XClock(lambda dump: 0)
clock.Step(2)
print(f"cycles={clock.clk}, half_ticks={clock.GetHalfTick()}")
```

Run it again:

```text
cycles=2, half_ticks=4
```

`XClock` accepts an execution function. The empty function in this exercise lets us observe time counters on their own; with a circuit, this function calls the simulator. `Step(2)` runs two complete cycles, each containing two half cycles.

## 3. Wait for three cycles

Now use the execution engine to wait for an event. Replace the script with this complete version:

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

The output is:

```text
advanced=6, hits=1
cycles=6
```

`AcquireClock()` gives the engine control of clock advancement. `ArmClockCycles(3)` registers an event after three cycles. `RunUntil(20)` allows up to twenty half cycles. The event fires after six half cycles and the engine returns, so `advanced_ticks` is 6.

`Disarm()` removes the processed registration. After cleanup and `ReleaseClock()`, the script advances one more cycle manually, giving a total of `2 + 3 + 1 = 6`.

Next, replace the cycle event with [signal conditions and coverage sampling](../guides/events-coverage.md). See [XEngine](../reference/engine.md) and [Trigger](../reference/triggers.md) for signatures and sampling phases.
