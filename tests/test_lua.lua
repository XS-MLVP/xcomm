
local x = require("xspcomm")

print("Test Lua with xpsomm:", x.version())

a = x.XData(32, x.XData.In)
b = x.XData(32, x.XData.In)

a:Set(0x12345678)
b:Set(0x87654321)

print("a = ", a:AsBinaryString())
print("b = ", b:String())

-- test get/set bytes
local vec = x.UCharVector()
vec:push_back(1)
vec:push_back(2)
vec:push_back(3)
a:Set(vec)
print("a = ", a:AsBinaryString())
vec = a:GetBytes()
print("a = ", vec[0], vec[1], vec[2])

clock = x.XClock(function(c) end)
clock:StepRis(function(c) print("callback: R cycle =>", c) end)
clock:StepFal(function(c) print("callback: F cycle =>", c) end)
clock:Step(10)
