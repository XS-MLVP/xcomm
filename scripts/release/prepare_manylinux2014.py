#!/usr/bin/env python3
"""Enable the SDK's C++11 ABI in the pinned, disposable manylinux2014 image.

Devtoolset 10 ships both ABIs in libstdc++.a, but its configuration header
unconditionally disables the new ABI. Release linking uses -static-libstdc++
so that enabling these declarations does not require a newer system runtime.
This script must never run against a user's system compiler.
"""

import os
from pathlib import Path
import re
import subprocess


def main():
    if os.environ.get("AUDITWHEEL_PLAT") != "manylinux2014_x86_64":
        raise SystemExit("This preparation is only for the manylinux2014 x86-64 image")
    expanded = subprocess.check_output(
        ["g++", "-E", "-x", "c++", "-include", "bits/c++config.h", "-"],
        input="", text=True,
    )
    headers = re.findall(r'^# \d+ "([^"]*bits/c\+\+config.h)"', expanded, re.MULTILINE)
    header = Path(headers[0]).resolve()
    toolchain = Path("/opt/rh/devtoolset-10/root/usr")
    if not header.is_relative_to(toolchain):
        raise SystemExit(f"Unexpected compiler configuration: {header}")
    archive = Path(subprocess.check_output(
        ["g++", "-print-file-name=libstdc++.a"], text=True,
    ).strip())
    if not archive.is_file() or not archive.resolve().is_relative_to(toolchain):
        raise SystemExit(f"Missing bundled C++ runtime: {archive}")
    content = header.read_text()
    disabled, enabled = "# define _GLIBCXX_USE_DUAL_ABI 0", "# define _GLIBCXX_USE_DUAL_ABI 1"
    if disabled in content:
        header.write_text(content.replace(disabled, enabled))
    elif enabled not in content:
        raise SystemExit("Unexpected libstdc++ configuration; review the pinned image")
    macros = subprocess.check_output(
        ["g++", "-D_GLIBCXX_USE_CXX11_ABI=1", "-dM", "-E", "-x", "c++",
         "-include", "bits/c++config.h", "-"], input="", text=True,
    )
    assert "#define _GLIBCXX_USE_CXX11_ABI 1" in macros
    print(f"Enabled C++11 ABI declarations in {header}; use -static-libstdc++")


if __name__ == "__main__":
    main()
