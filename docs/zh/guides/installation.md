# 安装 xcomm

选择 Python wheel 可直接运行教程；修改 C++ 或需要其他语言时从源码构建。xcomm 不包含 RTL 模拟器，入门示例使用一个空执行函数。

## Python wheel

v1.0.0 已在 [GitHub Release](https://github.com/XS-MLVP/xcomm/releases/tag/v1.0.0) 提供 CPython 3.12 / Linux x86-64 wheel。使用匹配的 Python：

```sh
python3.12 -m venv .venv
. .venv/bin/activate
python -m pip install 'https://github.com/XS-MLVP/xcomm/releases/download/v1.0.0/xspcomm-1.0.0-cp312-cp312-linux_x86_64.whl'
python -m pip check
python -c 'import xspcomm as xs; print(xs.version(), xs.abi_version())'
```

最后一行应输出 `1.0.0 3`。其他 Python 版本或平台可从源码构建。

## 源码与绑定

准备 C++20 编译器（常用 GCC 11+）、CMake >= 3.15、SWIG >= 4.2、Python 开发头文件和 make；其他语言还需要自己的工具链。

```sh
git clone --branch v1.0.0 https://github.com/XS-MLVP/xcomm.git
cd xcomm
BUILD_XSPCOMM_SWIG=python cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
PYTHONPATH=build/python python3 -c 'import xspcomm as xs; print(xs.abi_version())'
```

绑定选择示例：`BUILD_XSPCOMM_SWIG=python,java`。可选值包含 python/java/scala/golang/lua/javascript；存在构建入口不等于所有语言已有相同的高层 DSL。

默认 `make` 会清理并重建；增量使用显式 CMake 或 `make build`。构建 wheel：

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install build
python -m build --wheel
python -m pip install --no-index --find-links=dist 'xspcomm==1.0.0'
```

wheel 自动启用 Python 绑定。原始 CMake 默认版本字符串与发行包元数据不同，ABI 仍通过 `abi_version()` 核对。

## C++ SDK

使用独立的安装 prefix，按实际权限安装：

```sh
cmake --install build --prefix "$HOME/.local/xcomm"
```

公开头文件位于 `include/xspcomm`，生成的 xconfig.h 也需安装；原生库位于安装树的 lib 目录。链接时指定 include/lib 和运行库查找路径，先运行一个最小信号例子。`python -m xspcomm.info --help` 可查看资源查询入口，使用前确认返回的路径实际存在；不同安装布局不能凭包名推断。

下一步：[第一个信号和事件](../tutorials/first-simulation.md)。
