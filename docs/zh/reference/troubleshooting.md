# 故障排查

先区分安装/动态链接、信号数据、时钟推进和匹配描述。记录实际导入的包，避免在修复另一份源码：

```sh
python3 -c 'import xspcomm as x; print(x.__file__, x.version(), x.abi_version(), x.XTriggerEngine.CoverageVersion())'
python3 -m pip show uc-xcomm
```

v1.0.0 的预期版本是 1.0.0 / ABI 3 / Coverage 5。

| 现象 | 核对项 | 处理 |
| --- | --- | --- |
| import 失败、undefined symbol、找不到 libxspcomm.so.3 | Python 小版本、平台、库搜索路径、DUT 编译时 ABI | 安装匹配 wheel；在同一环境重建 DUT，不混用旧 SDK |
| 手动 Step 抛出 ownership 错误 | 是否有 engine AcquireClock | 在当前 engine 中推进；结束后 Clear/ReleaseClock 再手动推进 |
| ReleaseClock 失败 | 是否还有已触发但未 Disarm 的注册 | Disarm 每个 handle 或 Clear，检查 ActiveCount 已为零 |
| Coverage 不增长 | engine 是否推进/采样、phase、gate、点顺序、程序是否完成 | 从 [最小事件与覆盖率示例](../guides/events-coverage.md) 缩减复现；区分匹配与分类计数 |
| SamplePhase(DriveStable) 报错 | 是否处于 FallingStable | 在合法相位采样；SamplePhase 不会自动推进 |
| 宽表达式高位丢失 | 是否调用 U64、Eval 或旧 NewSignal/CompileExpr | Python U/value、字节接口、typed EvalBits；宽谓词显式比较零 |
| typed 注册或计算报错 | width/sign/Bool 类型、常量字节数、除零/负移位 | 显式 Cast 并核对 [类型规则](expressions.md) |
| Collect 后 root 指向错误节点 | 是否替换全部保留 root | 用返回映射更新保留的 root，在所属 ExprEngine 中使用 |
| 新绑定原生内存读到值但仍 unknown | 之前 XData 是否有 bval、存储是否二态 | 核对 [BindNativeData 语义](xdata.md)，新建已知信号并保持存储寿命 |
| 写入没有立刻到 DUT | 写模式、IO 方向、是否到了刷新边沿 | 按目标边沿推进；确需立即写入时用 ImmSet，先理解 DUT 时序 |
| 绑定内存后随机崩溃 | 对象是否存活、offset/stride/位宽、数组界限 | 使用匹配布局；保留内存和 proxy；先用单个信号复现 |
| XConditionCheck 不触发 | 只 BindXClock 而没有 Attach | BindXClock 选择停止目标，Attach 才安排采样 |
| Python await 永不醒来 | 混用了 engine 与旧 Step 事件 | 选择一个调度入口；engine waits 通过其 hit 协议实现 |
| TLM 模块缺失 | 是否构建独立 SystemC/UVMC bridge | 按 [TLM 环境](tlm.md) 构建；核心 wheel 不包含完整模拟器环境 |

## 报告问题需要什么

提供版本/ABI/协议、平台与 Python/编译器版本、最小代码、完整异常和预期结果。时序问题额外记录初始相位、Step/RunUntil 参数、gate/abort 和注册/释放顺序；宽值问题记录 width、signedness、aval/bval 字节。涉及 DUT 时提供可公开的最小 RTL 或独立 XData 复现。

标准异常可以捕获，但部分旧 Assert/Fatal 会终止进程。将可能崩溃的最小复现放在独立进程中运行，有助于保留调用日志。
