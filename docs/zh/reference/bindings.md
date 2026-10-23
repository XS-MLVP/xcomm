# 语言绑定与调用差异

公共 SWIG 定义见 [xcomm.i](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/xcomm.i)，Python 的便捷行为见 [xcomm.py](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/python/xcomm.py)。C++ 接口与语言包装应分别理解；同名方法不一定保持同样的参数/返回形式。

## 命名与容器

- C++ 使用 XEngine，绑定当前使用 XTriggerEngine；C++ 的 XTriggerEngine 是别名。
- Python 枚举通常展平，例如 XPhase_RisingStable、ExprOp_EQ、XCoveragePointKind_Pattern；XData.In/Out/InOut 是方向便捷常量。
- 描述数组使用 XCoveragePointVector、XCoverageBinVector、XSequenceStepVector、XFsmTransitionVector；索引容器为 XIntVector/XUInt32Vector/XUInt64Vector，命中列表为 XBackendHitVector。
- Python 的 vector<unsigned char> 参数使用 bytes，不把任意 list 当作通用替代；输出字节也为 bytes。
- 跨语言对象里保存的原生指针不自动延长其他 proxy 的生命周期；应用显式保留 clock、signals、ports 和 callbacks。

## Python XData

XData(width, direction=XData.In) 创建对象；名称可随后设置 mName。`.value`、`Set()`、`ImmSet()` 接受 Python int，并对宽 vector 使用字节路径；U/value 返回完整无符号 Python int。

C++ 的 S 是低 64-bit 有符号读取。Python 另有宽 S 包装，当前实现误将字节数用于符号位定位，可能错误解释大于 64 bit 的有符号值。需要明确宽度的有符号值时，读取已知 U 后按 W 转换：

<!-- runnable:python -->
```python
import xspcomm as xs

signal = xs.XData(129, xs.XData.InOut)
signal.value = -3
unsigned = signal.U()
assert signal.DataValid()
width = signal.W()
signed = unsigned - (1 << width) if unsigned & (1 << (width - 1)) else unsigned
assert signed == -3
```

下标读取返回 0/1/2/3，写下标调用 PinBind.Set。XData 的 Python == 只支持另一个 XData；与数字比较用 `.value`。BindDPIName 是 Python 增加的 DUT 便捷接口。

## Python XClock 与 asyncio

| API | Python 行为 |
| --- | --- |
| `XClock(callable)` 或 `XClock(step_addr, dut_addr)` | 自动包装 step callable，或使用原生地址入口 |
| `Step(cycles)` | 同步完整周期推进，逐周期检查回调异常并通知内部 asyncio Event |
| `RawStep(cycles)` | 原生 Step；仍受原生控制权限制，不执行 Python 包装层事件通知 |
| `StepRis/StepFal(callable, args=(), kwargs={})` | 包装普通 Python 边沿回调，传入周期号及用户参数 |
| `getEvent()` | 返回内部 step 事件 |
| `await AStep(n)` | 等待 n 次 Python Step 通知，不自行推进 |
| `await ACondition(callable)` | 在通知点检查直到条件成立 |
| `await ANext()` | 等待下一次通知 |
| `await RunStep(n)` | asyncio 协作式驱动 n 个周期；不同于同步 C++ RunStep |

engine 控制时钟期间，上述手动推进及旧 asyncio 等待入口会拒绝使用。engine 原生推进不经过 Python Step 的事件通知；上层应通过 engine hits 安排自己的 await，不能同时等待旧 `_step_event` 获得同样的调度结果。

Python 的 Event 提供 wait/set/clear；Queue 提供 put/get/put_nowait/get_nowait；sleep(delay) 按墙钟时间调用 asyncio sleep。这些包装配合旧 RunStep 的协作调度。

只有提供了 Python 便捷包装的回调入口才能直接接受普通 callable。XData.OnChange 等低层入口需要对应 SWIG callback/director，不能将 XClock 的 lambda 用法推广到所有回调。

## 其他语言

| 绑定 | 当前包装重点 | 查阅位置 |
| --- | --- | --- |
| Java | native library 初始化、字节数组及 BigInteger 值包装 | [java.i](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/java/java.i) |
| Scala | Java/SWIG 基础及 Scala 扩展 | [xsp.scala](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/scala/xsp.scala) |
| Go | 指针/整数地址包装，部分 raw slice API | [golang.i](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/golang/golang.i) |
| Lua | SWIG 对象及数值/回调适配 | [lua.i](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/lua/lua.i) |
| JavaScript | SWIG/JS 构建与数值包装 | [xspcomm.js](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/javascript/xspcomm.js) |

具体调用例子见 [API 索引](api-index.md) 和 [绑定测试目录](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/tests/bindings)。公共 SWIG 描述不等于每种语言都有高层 Coverage DSL；Python/C++ 示例与其他语言包装分别验证。

## 异常与 Python 工具入口

公共 SWIG 包装将 std::invalid_argument 转为 ValueError，其他标准异常转为 RuntimeError。部分旧 API 使用 Assert/Fatal 终止进程或报告错误后返回失败值；不能假定所有错误都可用 try/except 捕获。

`python -m xspcomm.info --help` 可查询安装根目录、include/lib 路径、版本和环境变量导出，实际选项见 [info.py](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/python/info.py)。

Python thirdcall 的 init_third_call/free_third_call/get_function_id/call_third_function 是专用原生桥接，参数为明确的 word 数组和返回容量；见 [thirdcall.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/swig/python/thirdcall.h)。
