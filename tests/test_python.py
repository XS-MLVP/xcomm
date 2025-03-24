
from xspcomm import *
import sys
from asyncio import run, create_task, sleep

def test_xdata():
    a: XData = XData(32, XData.In)
    b: XData = XData(32, XData.In)
    c: XData = XData(129, XData.In)
    e: XData = XData(0, XData.In)
    f: XData = XData(4, XData.In)
    x = c.SubDataRef(1, 32)
    y = c.SubDataRef(5, 32)
    y.value = -1
    print("x:", x.value, "y:", y.value)
    print("a == b", a == b)
    a.value = 1
    print("a == b", a == b)
    try:
        print("a == 2:", 2 == a)
        raise Exception("AssertionError not raised")
    except AssertionError as _:
        print("test == assertion pass")

    data = XData(64, XData.InOut)
    data.value = -1
    data[0] = 0
    print("org: %64s" % data.AsBinaryString())
    data.value = 0
    ref2 = data.SubDataRef(30, 6)
    ref2.value = -1
    print("af2: %64s" % data.AsBinaryString())
    data.value = -1
    print("org: %64s" % data.AsBinaryString())
    ref2.value = 0
    print("af2: %64s" % data.AsBinaryString())

    TEST_DPI_LR(DPI_TEST_VR)
    TEST_DPI_LW(0)
    TEST_DPI_VR(DPI_TEST_VR)
    TEST_DPI_VW(DPI_TEST_VR)
    print("------------")
    a.SetWriteMode(a.Imme)
    e.SetWriteMode(e.Imme)
    a.BindDPIRW(DPI_TEST_VR, DPI_TEST_VW)
    a.value = 1
    e.value = 0

    l_value = 0xffffffffffffffffff123456
    print("size: ", l_value.bit_length(), "a:", a.value, "e: ", e.value)

    c.value = l_value

    b.value = a
    a.value = "0x123123123123123123123"
    b.value = 0xffffffff
    b[0] = 0
    print(a, b, b[0])
    print("long c: ", hex(c.value))

    port = XPort("x_")
    port.Add("a", a)
    port.Add("b", a)
    print(port)
    print(port["a"])
    port.SetZero()
    print(port["a"])
    c.value = -2
    f.value = -1
    assert c.S() == -2
    assert f.S() == -1

    port["a"] = 1
    print(f"expected 1, actual {port['a'].value}")
    port["a"].value = 2
    print(f"expected 2, actual {port['a'].value}")
    print("prefix:", port.GetPrefix())
    print(" raw pins:", port.GetKeys(True))
    print("     pins:", port.GetKeys())
    
    clk = XClock(lambda a: 1 if print("lambda stp: ", a) else 0)
    clk.StepRis(lambda c, x, y: print("lambda ris: ", c, x, y), (1, 2))
    clk.StepRis(lambda c, x, y: print("lambda fal: ", c, x, y), (3, 4))

    p1 = XPin(a, clk.getEvent())
    p2 = XPin(b, clk.getEvent())
    p3 = XPin(a, clk.getEvent())
    print("p1 == p3", p1 == p3)
    print("p1 == p2", p1 == p2)
    try:
        print("p1 == 2:", 2 == p1)
        raise Exception("AssertionError not raised")
    except AssertionError as _:
        print("test == assertion pass")

    echo = ComUseEcho(p1.CSelf(), p2.CSelf())
    print("echo:", p1.value, p2.value)
    p1.value = 1
    p2.value = b'A'
    clk.StepRis(echo.GetCb(), echo.CSelf())
    clk.Step(1)
    p1.value = 0
    p2.value = b'B'
    clk.Step(1)
    p1.value = 1
    p2.value = b'C'
    clk.Step(1)
    print(clk)
    clk.RawStep(1)
    print(clk)

    # test clock
    port1 = XPort("x_")
    port1.Add("a", a)
    port1.Add("b", b)
    port2 = port1.SelectPins(["a", "b"])
    print(port2.String())
    
    # test xclock u64 step/cbs
    clk = XClock(TEST_get_u64_step_func(), 0x123)
    clk.StepRis(TEST_get_u64_ris_fal_cblback_func(), 0x456)
    clk.StepFal(TEST_get_u64_ris_fal_cblback_func(), 0x987)
    clk.Step(3)

    # test xsignal_cfg.h
    cfg = XSignalCFG("tests/test_signal_cfg.yaml")
    cfg2 = XSignalCFG("tests/test_signal_cfg_map.yaml")
    assert cfg.String() == cfg2.String(), "%s != %s" % (cfg.String().strip(), cfg2.String().strip())


async def test_async():
    clk = XClock(lambda a: 0)
    clk.StepRis(lambda c : print("lambda ris: ", c))
    task = create_task(clk.RunStep(30))
    print("test      AStep:", clk.clk)
    await clk.AStep(3)
    print("test ACondition:", clk.clk)
    await clk.ACondition(lambda: clk.clk == 20)
    print("test        cpm:", clk.clk)
    await task

async def test_async_event():
    print("test_async_event")
    clk = XClock(lambda a: 0)
    clk.StepRis(lambda c : print("lambda ris: ", c))
    task = create_task(clk.RunStep(30))

    async def awaited_func(event, event2):
        await event.wait()
        print("event has been waited")
        event2.set()
        print("event2 has been set")

    async def set_event(event, event2):
        await clk.AStep(2)
        event.set()
        print("event has been set")
        await event2.wait()
        print("event2 has been waited")


    # Wrong usage: use asyncio.Event
    # set and wait will not occur in the same cycle
    import asyncio
    events = [asyncio.Event() for _ in range(2)]
    create_task(awaited_func(events[0], events[1]))
    create_task(set_event(events[0], events[1]))

    await clk.AStep(5)


    # Right usage: use Event in xspcomm
    # All set and wait will occur in the same cycle
    events = [Event() for _ in range(2)]
    create_task(awaited_func(events[0], events[1]))
    create_task(set_event(events[0], events[1]))


    await task


if __name__ == "__main__":
    test_xdata()
    run(test_async())
    run(test_async_event())
    print("version: %s" % version())
