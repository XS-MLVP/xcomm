
local x = require("xspcomm")

print("Test Lua with xpsomm:", x.version())

a = x.XData(32, x.XData.In)
b = x.XData(32, x.XData.In)

a:Set(0x12345678)
b:Set(0x87654321)

print("a = ", a:AsBinaryString())
print("b = ", b:String())

clock = x.XClock(function(c) end)
clock:StepRis(function(c) print("callback: R cycle =>", c) end)
clock:StepFal(function(c) print("callback: F cycle =>", c) end)
clock:Step(10)
