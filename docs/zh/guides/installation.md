# 安装 xcomm

Python 发行包名和导入名均为 `xspcomm`。预编译包覆盖 Linux x86-64、CPython 3.11 和 3.12，分别提供 GLIBC 2.17 与 2.28 基线。每个版本还提供一份源码包（sdist）。其他 Python 版本和平台暂不在此 wheel 矩阵内。

## Python wheel

完成首次 [PyPI 发布配置](../development/releases.md#pypi) 后，在实际运行仿真的 Python 环境中安装：

```sh
python3.12 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install xspcomm
python -m pip check
python -c 'import xspcomm as xs; print(xs.version(), xs.abi_version())'
```

相同的 wheel 和源码包也会附在 [GitHub Releases](https://github.com/XS-MLVP/xcomm/releases) 中；下载匹配的 wheel 后，可运行 `python -m pip install ./xspcomm-*.whl`。pip 根据解释器、架构和 GLIBC 选择 wheel。安装预编译包无需编译器或 SWIG。xcomm 本身不包含 RTL 仿真器。

官方 x86-64 wheel 使用 `-march=x86-64 -mtune=generic`。GLIBC 基线和 CPU 指令集是两个独立的兼容性条件。

## pip 源码编译安装

系统需要提供 C++20 编译器、CMake >=3.15、make 或 Ninja，以及与当前 Python 匹配的开发头文件。pip 在隔离构建环境中安装 SWIG 4.5.1 和 Python 构建后端。SWIG 是 xspcomm 的构建依赖，不是运行依赖。

```sh
python -m pip install --no-binary=xspcomm --no-cache-dir xspcomm
```

pip 自动编译并安装源码包。源码构建默认 `XSPCOMM_NATIVE=ON`，在支持的 CPU 上使用 `-march=native -mtune=native`，面向本机优化。需要将环境或 wheel 转移到其他机器时，可显式关闭：

```sh
python -m pip install --no-binary=xspcomm --no-cache-dir xspcomm \
  -Ccmake.define.XSPCOMM_NATIVE=OFF
```

`--no-cache-dir` 避免复用之前本地构建的 wheel。两种模式都保留 CMake Release 优化；`CMAKE_BUILD_TYPE=Release` 不会关闭 native CPU 优化。源码构建使用本机系统库，不会自动变成兼容 manylinux 的包。

## 源码与绑定

直接运行 CMake 时，还需在 PATH 中提供 SWIG >=4.2。pip 的构建依赖位于隔离环境中，源码安装完成后不保证保留 SWIG；Picker 等后续生成绑定的工具需要声明自己的 SWIG 依赖。

```sh
git clone https://github.com/XS-MLVP/xcomm.git
cd xcomm
BUILD_XSPCOMM_SWIG=python cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
PYTHONPATH=build/python python3 -c 'import xspcomm as xs; print(xs.abi_version())'
```

CMake 传入 `-DXSPCOMM_NATIVE=OFF` 可使用通用 CPU 基线，交叉编译也应关闭 native。`BUILD_XSPCOMM_SWIG=python,java` 可选择两种绑定；其他语言还需自己的工具链。参见[语言绑定](../reference/bindings.md)。

默认 `make` 会清理并重建；增量使用显式 CMake 或 `make build`。同时构建本地 wheel 和源码包：

```sh
python -m pip install build
python -m build
```

wheel 自动启用 Python 绑定，并关闭原生单元测试的编译。原始 CMake 默认版本字符串与发行包元数据不同，ABI 仍通过 `abi_version()` 核对。

## C++ SDK

wheel 内含供 Picker 等 C++ 消费者使用的 SDK。官方 wheel 使用 libstdc++ 的 C++11 ABI（`_GLIBCXX_USE_CXX11_ABI=1`），客户端需要兼容的 C++20 工具链。部分旧发行版工具链会强制使用 ABI 0，即使设置该宏也无法切换；此时应换用兼容编译器，或使用与客户端相同的工具链源码安装 xspcomm。GLIBC 兼容不等于 C++ ABI 兼容。

原始 CMake 构建使用独立的安装 prefix，按实际权限安装：

```sh
cmake --install build --prefix "$HOME/.local/xcomm"
```

公开头文件位于 `include/xspcomm`，生成的 xconfig.h 也需安装；原生库位于安装树的 lib 目录。链接时指定 include/lib 和运行库查找路径，先运行一个最小信号例子。`python -m xspcomm.info --help` 可查看资源查询入口，使用前确认返回的路径实际存在；不同安装布局不能凭包名推断。

下一步：[第一个信号和事件](../tutorials/first-simulation.md)。
