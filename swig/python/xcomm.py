
from .pyxspcomm import *
import sys

# PinBind
PinBind__old__setattr__ = PinBind.__setattr__
def PinBind__setattr__(self: PinBind, name, value):
    if name != "value":
        return PinBind__old__setattr__(self, name, value)
    return self.Set(value)

PinBind.__setattr__ = PinBind__setattr__


# XData
XData_old__init__ = XData.__init__
def XData__init__(self: XData, width: int, direction = XData.In, raw_args = None):
    if raw_args is not None:
        return XData_old__init__(self, *(width, direction, *raw_args))
    return XData_old__init__(self, width, direction)

def XData__str__(self: XData):
    return f"XData(width={self.mWidth}, value=0x{self.String()})"

XData_old__setattr__ = XData.__setattr__
def XData__setattr__(self: XData, name, value):
    if name != "value":
        return XData_old__setattr__(self, name, value)
    if type(value) is bool:
        return self.Set(1 if value else 0)
    if type(value) is int:
        if value.bit_length() <= 64:
            return self.Set(value)
        self.SetVU8(value.to_bytes((value.bit_length() + 7) // 8, byteorder='little'))
    else:
        return self.Set(value)

XData_old__getattribute__ = XData.__getattribute__
def XData__getattribute__(self: XData, name):
    if name != "value":
        return XData_old__getattribute__(self, name)
    if self.W() <= 64:
        return self.U()
    return int.from_bytes(self.GetVU8(), 'little')

def XData__getitem__(self: XData, key):
    return self.At(key).AsInt32()

def XData__setitem__(self: XData, key, value):
    self.At(key).Set(value)

XData.__init__ = XData__init__
XData.__str__ = XData__str__
XData.__setattr__ = XData__setattr__
XData.__getattribute__ = XData__getattribute__
XData.__getitem__ = XData__getitem__
XData.__setitem__ = XData__setitem__

# XPort
XPort_old__init__ = XPort.__init__
def XPort__init__(self: XPort, prefix = ""):
    return XPort_old__init__(self, prefix)

def XPort__str__(self: XPort):
    return f"XPort(prefix={self.prefix}, size={self.PortCount()})"

def XPort__getitem__(self: XPort, key):
    return self.Get(key)

def XPort__setitem__(self: XPort, key, value):
    self.At(key).Set(value)


XPort.__init__ = XPort__init__
XPort.__str__ = XPort__str__
XPort.__getitem__ = XPort__getitem__
XPort.__setitem__ = XPort__setitem__

# XClock
class xclock_cb_step(cb_int_bool):
    """XClock step call back"""
    def __init__(self, func):
        cb_int_bool.__init__(self)
        self.func = func
    def call(self, dump: bool):
        return self.func(dump)

class xclock_cb_step_rf(cb_void_u64_voidp):
    """XClock step Ris/Fal call back"""
    def __init__(self, func, args, kwargs):
        cb_void_u64_voidp.__init__(self)
        self.func = func
        self.args = args
        self.kwargs = kwargs
    def call(self, cycle: int, args=None):
        return self.func(cycle, *self.args, **self.kwargs)

# Async
import asyncio

timestamp = 0
def tick_timestamp():
    global timestamp
    timestamp = (timestamp + 1) % 2**32

def has_unwait_task():
    for task in asyncio.all_tasks():
        if "XClock_RunStep" not in task.__repr__() and "wait_for" not in task.__repr__():
            return True
    return False

async def tick_clock_ready():
    global timestamp
    timestamp_old = timestamp
    await asyncio.sleep(0)
    while has_unwait_task() or timestamp_old != timestamp:
        await asyncio.sleep(0)
        timestamp_old = timestamp



XClock_old_init__ = XClock.__init__
def XClock__init__(self, step_func):
    self._step_event = asyncio.Event()
    self._step_event.clear()
    fc = xclock_cb_step(step_func)
    fc.set_force_callable()
    return XClock_old_init__(self, fc.__disown__())

XClock.__init__ = XClock__init__

def XClock__getEvent(self):
    return self._step_event

XClock.getEvent = XClock__getEvent

XClock_old_StepRis = XClock.StepRis
XClock_old_StepFal = XClock.StepFal
def XClock_StepRis(self, call_back, args=(), kwargs={}):
    fc = xclock_cb_step_rf(call_back, args, kwargs)
    fc.set_force_callable()
    return XClock_old_StepRis(self, fc.__disown__(), None, call_back.__name__)

def XClock_StepFal(self, call_back, args=(), kwargs={}):
    fc = xclock_cb_step_rf(call_back, args, kwargs)
    fc.set_force_callable()
    return XClock_old_StepFal(self, fc.__disown__(), None, call_back.__name__)

XClock.StepRis = XClock_StepRis
XClock.StepFal = XClock_StepFal

XClock__old_Step = XClock.Step
def XClock_Step(self, cycle: int):
    for i in range(cycle):
        XClock__old_Step(self, 1)
        self._step_event.set()
        self._step_event.clear()

async def XClock_AStep(self, cycle: int):
    for i in range(cycle):
        await self._step_event.wait()

async def XClock_ACondition(self, condition):
    assert callable(condition), "condition must be callable"
    while not condition():
        await self._step_event.wait()

async def XClock_ANext(self):
    return await self._step_event.wait()

async def XClock_RunStep(self, cycle: int):
    for i in range(cycle):
        await tick_clock_ready()
        XClock_Step(self, 1)
        await asyncio.sleep(0)

XClock.Step = XClock_Step
XClock.AStep = XClock_AStep
XClock.ACondition = XClock_ACondition
XClock.ANext =XClock_ANext
XClock.RunStep = XClock_RunStep


class Event:
    def __init__(self):
        self.event = asyncio.Event()

    async def wait(self):
        await self.event.wait()
        tick_timestamp()

    def set(self):
        self.event.set()

    def clear(self):
        self.event.clear()


class Queue:
    def __init__(self):
        self.queue = asyncio.Queue()

    async def put(self, value):
        await self.queue.put(value)
        tick_timestamp()

    async def get(self):
        ret = await self.queue.get()
        tick_timestamp()
        return ret

    def put_nowait(self, value):
        self.queue.put_nowait(value)

    def get_nowait(self):
        return self.queue.get_nowait()

async def sleep(delay: float):
    await asyncio.sleep(delay)
    tick_timestamp()


# XPin

class XPin:
    def __init__(self, xdata, event):
        self.xdata = xdata
        self.event = event

    def __str__(self):
        return f"XPin({self.xdata})"

    def __getattribute__(self, name):
        if name == "xdata" or name == "event":
            return object.__getattribute__(self, name)
        return self.xdata.__getattribute__(name)

    def __setattr__(self, name, value):
        if name == "xdata" or name == "event":
            return object.__setattr__(self, name, value)
        return self.xdata.__setattr__(name, value)

    def __getitem__(self, key):
        return self.xdata[key]

    def __setitem__(self, key, value):
        self.xdata[key] = value

xcomm_version = "0.0.1"
if xcomm_version != version():
    print("Warn: python wrapper version(%s) conflict with _pyxcomm.so version(%s)", xcomm_version, version())

__version__ = version()
