# xcomm 文档

xcomm 提供硬件仿真中常用的 C++ 组件：信号读写、端口分组、时钟推进、表达式求值、时序事件匹配和功能覆盖率。Python 发行包名为 `uc-xcomm`，导入名为 `xspcomm`，也提供 Java、Scala、Go、Lua 和 JavaScript 绑定。

第一次使用，从[安装](guides/installation.md)和[第一个信号与事件](tutorials/first-simulation.md)开始。教程用一个小脚本演示宽信号、时钟和事件等待。

## 编写测试和验证框架

| 需要做什么 | 文档 |
| --- | --- |
| 等待条件、匹配序列并统计覆盖率 | [事件与覆盖率](guides/events-coverage.md) |
| 运算超过 64 bit 的值 | [宽位表达式](guides/typed-expressions.md) |
| 将语言侧定义交给 C++ 执行 | [架构与 IR](explanation/architecture.md) |
| 排查运行错误 | [故障排查](reference/troubleshooting.md) |
| 迁移已有代码 | [接口迁移](reference/compatibility.md) |

## 查询 API

| 组件 | 用途 |
| --- | --- |
| [XData](reference/xdata.md) | 表示四态信号，读写值、位切片和宽数据 |
| [XPort](reference/xport.md) | 按名称组织信号，批量刷新 |
| [XClock](reference/xclock.md) | 连接模拟器执行函数，管理时钟和边沿回调 |
| [ExprEngine](reference/expressions.md) | 构造和求值表达式 |
| [XEngine](reference/engine.md) | 推进仿真，执行已注册的事件和覆盖率采样 |
| [Trigger](reference/triggers.md) | 匹配条件、Sequence 和 FSM |
| [Coverage](reference/coverage.md) | 定义 point、bin 和 cross，记录命中次数 |
| [Monitor](reference/monitors.md) | 在时钟回调中运行检查器 |
| [XSignalCFG](reference/signal-config.md) | 根据布局信息绑定原生信号 |
| [语言绑定](reference/bindings.md)、[TLM](reference/tlm.md) | 多语言接口和事务通信 |

完整类型和头文件列表见 [API 索引](reference/api-index.md)。实现修改和优化可参考[开发与测试](development/contributing.md)、[性能说明](explanation/performance.md)。文档编辑、网站预览和 Wiki 导出见[文档维护](development/documentation.md)。
