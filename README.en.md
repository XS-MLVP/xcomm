# xcomm

[![PyPI](https://img.shields.io/pypi/v/uc-xcomm)](https://pypi.org/project/uc-xcomm/)

English | [中文](https://github.com/XS-MLVP/xcomm/blob/master/README.cn.md)

xcomm is a C++20 library for hardware simulation, providing four-state signals, port collections, clocks, expressions, temporal events, and functional coverage. Install the Python distribution as `uc-xcomm` and import it as `xspcomm`; bindings are also available for Java, Scala, Go, Lua, and JavaScript.

## Get started

After [installation](https://github.com/XS-MLVP/xcomm/wiki/en-guides-installation), create and read a wide signal:

```python
import xspcomm as xs

signal = xs.XData(128, xs.XData.InOut)
signal.value = (1 << 96) + 3
print(hex(signal.value))
```

The output is `0x1000000000000000000000003`. Continue with the [tutorial](https://github.com/XS-MLVP/xcomm/wiki/en-tutorials-first-simulation) to advance a clock and wait for an event.

## Documentation

[English documentation](https://github.com/XS-MLVP/xcomm/wiki/en-Home) · [完整中文文档](https://github.com/XS-MLVP/xcomm/wiki/zh-Home)

- Use the library: [events and coverage](https://github.com/XS-MLVP/xcomm/wiki/en-guides-events-coverage), [typed expressions](https://github.com/XS-MLVP/xcomm/wiki/en-guides-typed-expressions).
- Look up an interface: [API index](https://github.com/XS-MLVP/xcomm/wiki/en-reference-api-index), [language bindings](https://github.com/XS-MLVP/xcomm/wiki/en-reference-bindings), [migration](https://github.com/XS-MLVP/xcomm/wiki/en-reference-compatibility), [troubleshooting](https://github.com/XS-MLVP/xcomm/wiki/en-reference-troubleshooting).
- Develop the library: [architecture](https://github.com/XS-MLVP/xcomm/wiki/en-explanation-architecture), [performance](https://github.com/XS-MLVP/xcomm/wiki/en-explanation-performance), [tests](https://github.com/XS-MLVP/xcomm/wiki/en-development-contributing), [documentation and Wiki](https://github.com/XS-MLVP/xcomm/wiki/en-development-documentation).

Report issues and suggestions through [GitHub Issues](https://github.com/XS-MLVP/xcomm/issues). See [LICENSE](https://github.com/XS-MLVP/xcomm/blob/master/LICENSE) for licensing.
