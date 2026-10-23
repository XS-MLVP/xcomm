# xcomm

中文 | [English](README.en.md)

xcomm 是硬件仿真的 C++20 组件库，提供四态信号、端口集合、时钟、表达式、时序事件和功能覆盖率。Python 发行包名为 `uc-xcomm`，导入名为 `xspcomm`，也支持 Java、Scala、Go、Lua 和 JavaScript。

## 开始使用

完成[安装](docs/zh/guides/installation.md)后，可以直接创建和读写宽信号：

```python
import xspcomm as xs

signal = xs.XData(128, xs.XData.InOut)
signal.value = (1 << 96) + 3
print(hex(signal.value))
```

输出为 `0x1000000000000000000000003`。[入门教程](docs/zh/tutorials/first-simulation.md)继续演示时钟推进和事件等待。

## 文档

[完整中文文档](docs/zh/index.md) · [English documentation](docs/en/index.md)

- 使用：[事件与覆盖率](docs/zh/guides/events-coverage.md)、[宽位表达式](docs/zh/guides/typed-expressions.md)。
- 查询：[API 索引](docs/zh/reference/api-index.md)、[语言绑定](docs/zh/reference/bindings.md)、[接口迁移](docs/zh/reference/compatibility.md)、[故障排查](docs/zh/reference/troubleshooting.md)。
- 开发：[架构](docs/zh/explanation/architecture.md)、[性能](docs/zh/explanation/performance.md)、[测试](docs/zh/development/contributing.md)、[文档与 Wiki](docs/zh/development/documentation.md)。

问题和建议请提交到 [GitHub Issues](https://github.com/XS-MLVP/xcomm/issues)。许可见 [LICENSE](LICENSE)。
