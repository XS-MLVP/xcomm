#!/usr/bin/env python3
"""Exercise an installed wheel and compile a client using only its public SDK."""

from importlib.metadata import version
import argparse
from pathlib import Path
import subprocess
import tempfile

import xspcomm as xs
from xspcomm import info

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--cxx11-abi", choices=("0", "1"))
parser.add_argument("--static-libstdcxx", action="store_true")
args = parser.parse_args()
compiler_flags = [f"-D_GLIBCXX_USE_CXX11_ABI={args.cxx11_abi}"] if args.cxx11_abi else []

if args.static_libstdcxx:
    compiler_flags.append("-static-libstdc++")
if args.cxx11_abi:
    compiler_flags.append(f"-DEXPECTED_CXX11_ABI={args.cxx11_abi}")

assert xs.version() == version("uc-xcomm")
signal = xs.XData(8, xs.XData.InOut)
signal.Set(0xA5)
assert signal.U() == 0xA5
clock = xs.XClock(lambda _: 0)
clock.Step(2)
assert clock.clk == 2
assert xs.abi_version() > 0
assert xs.XTriggerEngine.CoverageVersion() > 0

package = Path(xs.__file__).parent
soname = f"libxspcomm.so.{xs.abi_version()}"
assert (package / soname).is_file()
dynamic = subprocess.check_output(["readelf", "-d", str(package / "_pyxspcomm.so")], text=True)
assert soname in dynamic
assert "$ORIGIN" in dynamic
assert "/home/" not in dynamic and "/usr/local/lib" not in dynamic

include, library = Path(info.path_include()), Path(info.path_lib())
assert (include / "xspcomm/xconfig.h").is_file(), include
assert (library / soname).is_file(), library
with tempfile.TemporaryDirectory(prefix="xspcomm-sdk-") as directory:
    source, executable = Path(directory) / "client.cpp", Path(directory) / "client"
    source.write_text('''#include <xspcomm/xdata.h>
#include <xspcomm/xport.h>
#include <cassert>
#include <stdexcept>
#ifdef EXPECTED_CXX11_ABI
static_assert(_GLIBCXX_USE_CXX11_ABI == EXPECTED_CXX11_ABI);
#endif
int main() {
    xspcomm::XData signal(8, xspcomm::IOType::InOut, "probe");
    signal.Set(uint64_t(165));
    assert(signal.U() == 165);
    xspcomm::XPort port("dut.");
    assert(port.Add("signal", signal));
    assert(port.Has("signal"));
    assert(port.GetKeys().size() == 1);
    bool called = false;
    signal.OnChange([&](bool, xspcomm::XData *, uint64_t value, void *) {
        called = true;
        assert(value == 90);
        throw std::runtime_error("callback exception");
    }, nullptr, "client callback");
    bool caught = false;
    try {
        signal.Set(uint64_t(90));
    } catch (const std::runtime_error &error) {
        caught = std::string(error.what()) == "callback exception";
    }
    assert(called && caught);
    signal.ClearOnChangeCbs();
    return 0;
}
''')
    subprocess.run(["c++", "-std=c++20", "-fcoroutines", *compiler_flags, str(source), "-I", str(include),
                    "-L", str(library), f"-Wl,-rpath,{library}", "-lxspcomm", "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print(f"Installed uc-xcomm {version('uc-xcomm')}: Python binding and public C++ SDK passed")
