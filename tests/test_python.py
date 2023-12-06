
from xspcomm import *
import sys
from asyncio import run, create_task, sleep

def test_xdata():
    a: XData = XData(32, XData.In)
    b: XData = XData(32, XData.In)
    c: XData = XData(128, XData.In)

    l_value = 0xffffffffffffffffff123456
    print("size: ", l_value.bit_length())

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

    clk = XClock(lambda a: 1 if print("lambda stp: ", a) else 0)
    clk.StepRis(lambda c, x, y: print("lambda ris: ", c, x, y), (1, 2))
    clk.StepRis(lambda c, x, y: print("lambda fal: ", c, x, y), (3, 4))

    clk.Step(3)
    print(clk)


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

if __name__ == "__main__":
    test_xdata()
    run(test_async())
    print("version: %s" % version())
