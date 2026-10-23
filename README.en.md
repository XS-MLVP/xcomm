# xcomm

English | [中文](README.cn.md)

xcomm is a C++20 library for hardware simulation, providing four-state signals, port collections, clocks, expressions, temporal events, and functional coverage. Install the Python distribution as `uc-xcomm` and import it as `xspcomm`; bindings are also available for Java, Scala, Go, Lua, and JavaScript.

## Get started

After [installation](docs/en/guides/installation.md), create and read a wide signal:

```python
import xspcomm as xs

signal = xs.XData(128, xs.XData.InOut)
signal.value = (1 << 96) + 3
print(hex(signal.value))
```

The output is `0x1000000000000000000000003`. Continue with the [tutorial](docs/en/tutorials/first-simulation.md) to advance a clock and wait for an event.

## Documentation

[English documentation](docs/en/index.md) · [完整中文文档](docs/zh/index.md)

- Use the library: [events and coverage](docs/en/guides/events-coverage.md), [typed expressions](docs/en/guides/typed-expressions.md).
- Look up an interface: [API index](docs/en/reference/api-index.md), [language bindings](docs/en/reference/bindings.md), [migration](docs/en/reference/compatibility.md), [troubleshooting](docs/en/reference/troubleshooting.md).
- Develop the library: [architecture](docs/en/explanation/architecture.md), [performance](docs/en/explanation/performance.md), [tests](docs/en/development/contributing.md), [documentation and Wiki](docs/en/development/documentation.md).

Report issues and suggestions through [GitHub Issues](https://github.com/XS-MLVP/xcomm/issues). See [LICENSE](LICENSE) for licensing.
