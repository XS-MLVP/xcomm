# Install xcomm

The Python distribution is `uc-xcomm`; import it as `xspcomm`. The release wheels target Linux x86-64, CPython 3.11 and 3.12, with both GLIBC 2.17 and 2.28 baselines. Each version also includes a source distribution (sdist). Other Python versions and platforms are not part of this wheel matrix.

## Python wheel

After the first [PyPI release is configured](../development/releases.md#pypi-setup), install into the environment that will run the simulation:

```sh
python3.12 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install uc-xcomm
python -m pip check
python -c 'import xspcomm as xs; print(xs.version(), xs.abi_version())'
```

The same wheels and sdist are attached to [GitHub Releases](https://github.com/XS-MLVP/xcomm/releases). Download a matching wheel and install it with `python -m pip install ./uc_xcomm-*.whl` if using GitHub assets. Pip selects a wheel matching the interpreter, architecture and GLIBC. Installing a wheel needs neither a compiler nor SWIG. xcomm does not include an RTL simulator.

Official x86-64 wheels use `-march=x86-64 -mtune=generic`. The GLIBC baseline and CPU instruction set are independent compatibility requirements.

## Compile during pip installation

Prepare a C++20 compiler, CMake >=3.15, make or Ninja, and development headers matching the active Python interpreter. Pip installs SWIG 4.5.1 and the Python build backend in an isolated build environment. SWIG is a build dependency, not a runtime dependency of xspcomm.

```sh
python -m pip install --no-binary=uc-xcomm --no-cache-dir uc-xcomm
```

Pip builds and installs the sdist automatically. Source builds default to `XSPCOMM_NATIVE=ON`, using `-march=native -mtune=native` on supported CPUs. This targets the build machine; use the following option when producing an environment or wheel for other machines:

```sh
python -m pip install --no-binary=uc-xcomm --no-cache-dir uc-xcomm \
  -Ccmake.define.XSPCOMM_NATIVE=OFF
```

`--no-cache-dir` forces a fresh build instead of reusing an earlier locally built wheel. Both native and portable builds use CMake's Release optimization; `CMAKE_BUILD_TYPE=Release` does not disable native CPU tuning. A source build uses the local system libraries and does not automatically become manylinux-compatible.

## Source and bindings

For raw CMake builds, also provide SWIG >=4.2 on PATH. The pip build dependency is isolated and need not remain installed after a source installation. Tools such as Picker that generate new bindings must provide their own SWIG dependency.

```sh
git clone https://github.com/XS-MLVP/xcomm.git
cd xcomm
BUILD_XSPCOMM_SWIG=python cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
PYTHONPATH=build/python python3 -c 'import xspcomm as xs; print(xs.abi_version())'
```

Pass `-DXSPCOMM_NATIVE=OFF` to CMake for a portable CPU baseline, including cross-compilation. `BUILD_XSPCOMM_SWIG=python,java` selects two bindings; other languages require their own toolchains. See [language bindings](../reference/bindings.md).

Default `make` cleans and rebuilds. Use explicit CMake commands or `make build` for incremental work. To build both a local wheel and an sdist:

```sh
python -m pip install build
python -m build
```

Wheel builds automatically enable Python and disable native unit-test compilation. Raw CMake's default version string differs from release package metadata; inspect native ABI with `abi_version()`.

## C++ SDK

The wheel includes an SDK for Picker and other C++ consumers. Official wheels use libstdc++'s C++11 ABI (`_GLIBCXX_USE_CXX11_ABI=1`); clients need a compatible C++20 toolchain. Some older distribution toolchains force ABI 0 even when the macro is set. In that case, use a compatible compiler or install xspcomm from source with the same toolchain as the client. GLIBC compatibility alone does not guarantee C++ ABI compatibility.

For a raw CMake build, choose an installation prefix with appropriate permissions:

```sh
cmake --install build --prefix "$HOME/.local/xcomm"
```

Public headers live in `include/xspcomm`, including generated xconfig.h. Native libraries live under the installation's lib directory. Configure include/library/runtime search paths and run a minimal signal program first. `python -m xspcomm.info --help` documents resource queries; verify returned paths exist because installation layouts differ.

Next: [your first signal and event](../tutorials/first-simulation.md).
