-- Load the xspcomm library from the current directory

local function get_current_directory()
    local info = debug.getinfo(1, "S")
    local path = info.source:match("^@(.*/)")
    return path
end

local current_directory = get_current_directory()
package.cpath = current_directory .. "?.so;" .. package.cpath

local libxspcomm = require("luaxspcomm")
return libxspcomm
