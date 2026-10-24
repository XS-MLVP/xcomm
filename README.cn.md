# xcomm

[![PyPI](https://img.shields.io/pypi/v/uc-xcomm?style=flat-square&logo=pypi&logoColor=white)](https://pypi.org/project/uc-xcomm/)
[![Python](https://img.shields.io/pypi/pyversions/uc-xcomm?style=flat-square&logo=python&logoColor=white)](https://github.com/XS-MLVP/xcomm/wiki/zh-guides-installation)
[![CI](https://img.shields.io/github/actions/workflow/status/XS-MLVP/xcomm/python-wheel.yml?branch=master&style=flat-square&label=CI&logo=github)](https://github.com/XS-MLVP/xcomm/actions/workflows/python-wheel.yml?query=branch%3Amaster)
[![License](https://img.shields.io/badge/license-MulanPSL--2.0-blue?style=flat-square)](https://github.com/XS-MLVP/xcomm/blob/master/LICENSE)
[![Wiki](https://img.shields.io/badge/docs-Wiki-0A7EA4?style=flat-square)](https://github.com/XS-MLVP/xcomm/wiki/zh-Home)
[![GitHub Stars](https://img.shields.io/github/stars/XS-MLVP/xcomm?style=social)](https://github.com/XS-MLVP/xcomm)

中文 | [English](https://github.com/XS-MLVP/xcomm/blob/master/README.en.md)

xcomm 是硬件仿真的 C++20 组件库，提供四态信号、端口集合、时钟、表达式、时序事件和功能覆盖率。Python 发行包名为 `uc-xcomm`，导入名为 `xspcomm`，也支持 Java、Scala、Go、Lua 和 JavaScript。

## 开始使用

完成[安装](https://github.com/XS-MLVP/xcomm/wiki/zh-guides-installation)后，可以直接创建和读写宽信号：

```python
import xspcomm as xs

signal = xs.XData(128, xs.XData.InOut)
signal.value = (1 << 96) + 3
print(hex(signal.value))
```

输出为 `0x1000000000000000000000003`。[入门教程](https://github.com/XS-MLVP/xcomm/wiki/zh-tutorials-first-simulation)继续演示时钟推进和事件等待。

## 文档

[完整中文文档](https://github.com/XS-MLVP/xcomm/wiki/zh-Home) · [English documentation](https://github.com/XS-MLVP/xcomm/wiki/en-Home)

- 使用：[事件与覆盖率](https://github.com/XS-MLVP/xcomm/wiki/zh-guides-events-coverage)、[宽位表达式](https://github.com/XS-MLVP/xcomm/wiki/zh-guides-typed-expressions)。
- 查询：[API 索引](https://github.com/XS-MLVP/xcomm/wiki/zh-reference-api-index)、[语言绑定](https://github.com/XS-MLVP/xcomm/wiki/zh-reference-bindings)、[接口迁移](https://github.com/XS-MLVP/xcomm/wiki/zh-reference-compatibility)、[故障排查](https://github.com/XS-MLVP/xcomm/wiki/zh-reference-troubleshooting)。
- 开发：[架构](https://github.com/XS-MLVP/xcomm/wiki/zh-explanation-architecture)、[性能](https://github.com/XS-MLVP/xcomm/wiki/zh-explanation-performance)、[测试](https://github.com/XS-MLVP/xcomm/wiki/zh-development-contributing)、[文档与 Wiki](https://github.com/XS-MLVP/xcomm/wiki/zh-development-documentation)。

问题和建议请提交到 [GitHub Issues](https://github.com/XS-MLVP/xcomm/issues)。许可见 [LICENSE](https://github.com/XS-MLVP/xcomm/blob/master/LICENSE)。
