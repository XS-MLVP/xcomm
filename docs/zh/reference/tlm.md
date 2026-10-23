# TLM：事务消息与 SystemC/UVM 桥接

公开头文件：[tlm/message.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/tlm/message.h)、[tlm/pubsub.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/tlm/pubsub.h)。TLM 交换事务级消息，不通过 XData 的 bit/vector 读写路径处理每个事务。

这部分需要单独构建，并依赖配套的 SystemC/UVMC 和模拟器环境。Python 常见模块名为 tlm_pbsb，取决于桥接构建时的 MODULE_NAME。

## tlm_msg

| API/字段 | 作用 |
| --- | --- |
| `tlm_msg()`；拷贝构造 | 创建消息及复制内容。默认构造初始化 cmd/resp_status/option；建议从默认构造开始再设置数据。 |
| `from_bytes(data)`；`as_bytes()` | 设置或读取 payload 字节。 |
| `clone()` | 创建新消息副本，C++ 调用方负责释放。 |
| `cmd` | TLM_READ_COMMAND、TLM_WRITE_COMMAND、TLM_IGNORE_COMMAND |
| `resp_status` | OK/INCOMPLETE 及 GENERIC/ADDRESS/COMMAND/BURST/BYTE_ENABLE 错误 |
| `option` | TLM_MIN_PAYLOAD、TLM_FULL_PAYLOAD、TLM_FULL_PAYLOAD_ACCEPTED |
| `delay/time_unit`；`data` | 延迟数值、时间单位、字节载荷 |

指针范围和 vector 的构造重载也存在；它们不应被假定与默认构造有完全相同的字段初始化行为。设置 cmd/resp_status/option 时保持显式，避免把未定义的默认字段传入桥。

## 发布与订阅

| API | 作用 |
| --- | --- |
| `TLMPub(channel)`；`Connect()` | 创建并连接发布端 |
| `SendMsg(msg)`；`GetChannel()` | 发送事务、查询通道 |
| `TLMSub(channel)`；`SetHandler(callback)`；`Connect()` | 创建订阅端、设置接收回调并连接 |
| Python `TLMSub(channel, callback)` | 额外的便捷构造重载 |
| `MakeChannel/UMakeChannel` | 添加/移除发布 .pub、订阅 .sub 后缀的辅助接口 |
| `tlm_pbsb_run(time, unit="ns")` | 推进 SystemC 侧运行 |
| `step(time, scale=1, unit="ns")` | 桥接环境中的时间推进 |
| `tlm_vcs_init(argc,argv)`；`tlm_vcs_step(delay)` | USE_VCS 构建下的 VCS 初始化和推进；Python init 包装接受字符串参数 |

收发对象、handler 和桥接运行时须在使用期间保持存活。TLM 和 XClock 分别管理时间推进，集成方负责安排两者的驱动顺序。

## 构建与示例

集成源位于 [integrations/tlm](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/integrations/tlm)，安装到 SDK 的 share/xspcomm/tlm。新的源文件入口是 python.i、pubsub.cpp；公开类型留在 include/xspcomm/tlm，不再从 include 下编译旧实现文件。

完整环境示例见 [tests/integration/tlm/test.py](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/tests/integration/tlm/test.py) 和相邻 Makefile/VCS 文件。这组文档的本地校验不宣称运行了 VCS/SystemC 集成；核心 wheel 导入成功不能证明 TLM 桥已可运行。
