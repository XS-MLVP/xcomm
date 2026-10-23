# Install xcomm

Use a Python wheel to run the tutorials directly. Build from source for C++ development or other languages. xcomm does not contain an RTL simulator; the introductory example uses an empty execution callback.

## Python wheel

The [v1.0.0 GitHub Release](https://github.com/XS-MLVP/xcomm/releases/tag/v1.0.0) provides a CPython 3.12 / Linux x86-64 wheel. Use matching Python:

```sh
python3.12 -m venv .venv
. .venv/bin/activate
python -m pip install 'https://github.com/XS-MLVP/xcomm/releases/download/v1.0.0/xspcomm-1.0.0-cp312-cp312-linux_x86_64.whl'
python -m pip check
python -c 'import xspcomm as xs; print(xs.version(), xs.abi_version())'
```

The last line should print `1.0.0 3`. Build from source for other Python versions or platforms.

## Source and bindings

Prepare a C++20 compiler (GCC 11+ is common), CMake >= 3.15, SWIG >= 4.2, Python development headers and make. Other languages require their own toolchains.

```sh
git clone --branch v1.0.0 https://github.com/XS-MLVP/xcomm.git
cd xcomm
BUILD_XSPCOMM_SWIG=python cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
PYTHONPATH=build/python python3 -c 'import xspcomm as xs; print(xs.abi_version())'
```

For example, `BUILD_XSPCOMM_SWIG=python,java` selects two bindings. Available selections include python/java/scala/golang/lua/javascript. See [language bindings](../reference/bindings.md) for language-specific interfaces.

Default `make` cleans and rebuilds. Use explicit CMake commands or `make build` for incremental work. To build a wheel:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install build
python -m build --wheel
python -m pip install --no-index --find-links=dist 'xspcomm==1.0.0'
```

Wheel builds automatically enable Python. Raw CMake's default version string differs from release package metadata; inspect native ABI with `abi_version()`.

## C++ SDK

Choose an installation prefix with appropriate permissions:

```sh
cmake --install build --prefix "$HOME/.local/xcomm"
```

Public headers live in `include/xspcomm`, including generated xconfig.h. Native libraries live under the installation's lib directory. Configure include/library/runtime search paths and run a minimal signal program first. `python -m xspcomm.info --help` documents resource queries; verify returned paths exist because installation layouts differ.

Next: [your first signal and event](../tutorials/first-simulation.md).
