# 公共工具与 C++ 协程

这里汇总基础设施 API。普通信号、事件等待和覆盖率使用者通常先看 [XData](xdata.md)、[XEngine](engine.md)，无需接触原生地址辅助函数。

## 比较与内存访问

头文件：[compare_op.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/common/compare_op.h)、[compare.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/common/compare.h)、[memory_access.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/common/memory_access.h)。

| API | 用途与约束 |
| --- | --- |
| `CompareOp`；`compare::Compare<Op>(a,b)`；`compare::Compare(op,a,b)` | EQ/NE/GT/GE/LT/LE 的共同分派，具体数据语义取决于操作数。 |
| `compare::Order(a,b)` | 返回 -1/0/1 的顺序结果。 |
| `compare::CompareWords(words, read_a, read_b)` | reader 提供低 word 在前的数据，顺序比较从高 word 扫描。 |
| `compare::EqualWords(a_count,b_count,read_a,read_b)` | 允许无符号零扩展的相等判断。 |
| `compare::CompareSignedBytes(a,b,bytes)` | 同长度 little-endian 二进制补码比较。 |
| `compare::WithinRange(...)` | 标量或 word reader 形式的无符号范围判断。 |
| `memory::Load<T>/Store<T>` | 对可平凡复制类型使用固定大小 memcpy，不要求地址满足 T 对齐；调用方仍要保证范围有效。 |
| `memory::UIntBytes<N>::Load(data)` | N=1..8 的精确宽度无符号读取，用于内部适配。 |
| `memory::ByteWords`；`Count()`；下标 | 非拥有 little-endian buffer 的 32-bit word 视图，末 word 可不满 4 bytes。 |

这些模板提供复用算法，不会自动让任意 XData 四态比较、指针有符号比较和 RangeCheck 具有相同语义。应用层通常使用对应对象的方法。

## XByteBuffer 与字符串

头文件：[memory.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/common/memory.h)。

| API | 作用与约束 |
| --- | --- |
| `XByteBuffer(bytes)` | 申请自有 buffer，bytes 必须为正。 |
| `XByteBuffer(address,bytes)` | 引用已有存储，不拥有或延长其生命期。 |
| `Copy()` | 申请独立副本，由 C++ 调用方释放。 |
| `SyncFrom(address,size)/SyncTo(address,size)` | 显式复制；size 和两端内存范围由调用方保证。 |
| `SetZero/BaseAddr/Size/AsBytes/FromBytes` | 清零、地址、长度、导出 bytes、导入最多容纳的 bytes 并返回写入长度。 |
| `operator==` | 比较长度和内容。 |
| `CString()` 或 `CString(text)`；`Get/Set` | 持有字符串。 |
| `CharAddress()` | 返回 c_str 地址，修改字符串或销毁对象后不能继续依赖该地址。 |
| `AssignFrom(pointer/address)` | 从 C 字符串读取。 |
| `AssignTo(pointer/address)` | 现有实现只改变函数内的局部指针，不复制到目标 buffer；不能当作字符串写回方法。 |

GetFromU64Array/GetFromU32Array/GetFromU8Array 和 SetU64Array/SetU32Array/SetU8Array 操作原生数组；U64PtrAsU64/U32PtrAsU64/U8PtrAsU64 与 U64AsU64Ptr/U64AsU32Ptr/U64AsU8Ptr 在地址和指针间转换。它们不检查对象寿命或 buffer 边界。

## xfunction 回调

头文件：[xcallback.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xcallback.h)。xfunction<R,Args...> 是 C++ callable 与 SWIG director 之间的适配器，C++ 可接收 lambda/std::function，提供 call、调用运算符、空值查询及赋值。

set_force_callable 用于 director 强制转发。原生函数地址桥接要求调用签名一致；传入 uint64 地址不会验证地址有效，也不会管理目标对象。

## C++ coroutine

头文件：[xcoroutine.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xcoroutine.h)，需要 ENABLE_XCOROUTINE。现有名称 XCorutine/xcorutine 和 schedule_awit 保留源码拼写。

| API | 作用 |
| --- | --- |
| `XCorutine<T>`；`xcorutine<T>` | 可 co_await 的返回类型，与 XPromise 配合。 |
| `XStep(clock,n)`；`clock.AStep(n)` | 等待时钟达到目标周期，依靠时钟的调度过程继续执行。 |
| `XCondition(checker)`；`clock.ACondition(checker)` | 等待 C++ callable 成立。 |
| `XNext(n)`；`clock.ANext(n)` | 按调度轮次等待，不把 n 当作墙钟时间。 |
| `schedule_awit(handler_map)` | 扫描并恢复 ready 的协程，常由时钟内部调用。 |
| `XPromise<T>`；`_XAWait/_PromiseBase` | 协程协议和实现扩展基础，不作为一般信号 API 使用。 |

这套本地协程由时钟调度。集成方负责协程取消、异常处理和关联对象的生命周期。Python asyncio 与 XReactor 使用各自的调度入口。

## 日志、版本和辅助函数

头文件：[xutil.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xutil.h)。

| API | 作用 |
| --- | --- |
| `version()`；`abi_version()`；`checkVersion()` | 包版本、原生 ABI、运行库与头文件版本检查。 |
| `get_log_level/set_log_level`；`LogLevel` | debug/info/warning/error/fatal。 |
| `Info/Warn/Error/Debug/Fatal`；`Assert`；`DebugC`；`Traceback` | 日志及诊断；Fatal/Assert 失败会终止进程。 |
| `uTime()`；`fmtTime(microseconds,format)`；`fmtNow(format)` | 主机时间辅助；fmtTime 接受微秒，当前 fmtNow 传入秒值存在单位不一致，需准确时间时使用 fmtTime(uTime())。 |
| `sFmt/sArrayHex/sWith/sLower/FmtSize` | 格式化、十六进制显示、前缀判断、小写、容量展示。 |
| `xRandom(a,b)`；`XSeed(seed)` | 基于 rand 的随机数辅助函数。 |
| `contians(vector,value)`；`removeSuffix(text,suffix)`；`fileExists(path)` | 容器搜索、后缀移除及文件检查；contians 为现有拼写。 |
| `big_shift/big_binstr/big_mask/sync_data_to` | 原生 word buffer 的移位、显示、mask 和同步工具。 |

common/callback_storage.h、common/lifetime.h 提供公开模板所需的实现辅助类型。它们需要随 SDK 安装，但不要求普通使用者直接依赖这些内部辅助类型。
