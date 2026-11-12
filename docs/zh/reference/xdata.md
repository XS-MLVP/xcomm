# XData：信号和值

头文件：[xdata.h](https://github.com/XS-MLVP/xcomm/blob/master/include/xspcomm/xdata.h)。XData 将 DUT 信号或数据源统一为可读写对象，支持四态缓存、后端绑定和位切片。Python 使用 `xspcomm.XData`。

## 构造、方向与位宽

| API | 用途与约束 |
| --- | --- |
| `XData()`；`XData(width, type, name)`；拷贝构造；`ReInit(...)` | C++ 默认对象是 scalar InOut；Python 构造必须提供 width，默认方向是 In。ReInit 会重建内部存储，已有位引用需重新获取。 |
| `mName`、`mWidth`、`mIOType`；`W()` | 名称、位宽和 IO 方向。调整位宽使用 ReInit。 |
| `IsInIO/IsOutIO/IsBiIO`；`AsInIO/AsOutIO/AsBiIO/FlipIOType` | 查询和设置方向；Flip 交换 Input/Output，InOut 保持不变。 |
| `GetBackendKind()`；`IsReadonly()` | 查询数据来源及后端只读约束；不能只靠 IO 方向推断后端能力。 |

`width == 0` 表示 SV scalar logic；`width == 1` 是单 bit vector。scalar 的 `0/1/2/3` 表示 `0/1/Z/X`。vector 按 32-bit word 保存 `aval/bval`，四态编码为 `(aval,bval) = (0,0)/(1,0)/(0,1)/(1,1)`。

## 读写与表示

| API | 用途与约束 |
| --- | --- |
| `Set(value)`；赋值运算符 | 整数、字符串、同宽 XData 或字节数据；超出有效位宽的部分截断。 |
| `ImmSet(value)`；`ImmSetBytes(bytes)` | 本次写入临时使用立即模式，然后恢复原写模式。 |
| `U()`；`S()`；`B()`；`AsInt32/AsInt64` | C++ 整数读取最多 64 bit；S 对 1–64 bit 信号作符号扩展。Python 的 U/value 另外提供宽整数包装。 |
| `String()`；`AsBinaryString()` | 十六进制显示和含 x/z 的二进制显示；String 不带 0x，含未知位的 byte 显示 ??。 |
| `GetBytes/GetVU8`；`SetBytes/SetVU8` | little-endian 的 aval 数据；vector 返回按 32-bit word 对齐的字节数组。scalar 不使用这些 vector API。 |
| `GetBvalBytes()`；`XMask()` | vector 的完整未知位 mask 字节数组；XMask 只返回低 64 bit。 |
| `DataValid()` | 查询最近一次刷新后的缓存是否含 X/Z；该方法本身不主动读取后端。 |
| `Invert()` | 值取反；vector 同时清除未知 mask。 |

字符串可用 `0x...`、`0b...`、`::...`，其中二进制和十六进制位可表示 X/Z。vector 的完整未知值使用显式位数，例如 128-bit 全 X 写作 `0x` 加 32 个 `x`。scalar 可使用单字符 `x/z`。当前 `0b` 输入按文本左端到低位的顺序写入，与 `AsBinaryString()` 的高位在前不同；一般数值赋值优先用整数或十六进制。

读取外部存储后再判断有效性，例如先 `U()`/`GetBytes()`，再 `DataValid()`。整数值单独不能表达 X/Z。

<!-- runnable:python -->
```python
import xspcomm as xs

value = xs.XData(128, xs.XData.InOut)
value.value = (1 << 100) | 7
assert value.U() == (1 << 100) | 7
assert len(value.GetBytes()) == 16
value.value = "0x" + "x" * 32
assert value.GetBvalBytes() == bytes([255]) * 16
assert not value.DataValid()
```

## 写入时机

| API | 用途 |
| --- | --- |
| `GetWriteMode/SetWriteMode`；`IsImmWrite/IsRiseWrite/IsFallWrite` | 查询或选择 Imme、Rise、Fall，默认 Rise；Output 不能设置写模式。 |
| `AsImmWrite/AsRiseWrite/AsFallWrite` | 同上，返回自身方便链式使用。 |
| `WriteOnRise/WriteOnFall` | 对应边沿提交后端写入，通常由 XClock 处理。 |
| `WriteDirect()` | 直接请求后端写入，跳过边沿模式选择。 |
| `ReadFresh(mode)` | 根据写模式及方向安排后端刷新，通常由端口/时钟处理。 |
| `SetIgnoreSameDataWrite(bool)` | 控制是否省略与上次相同的后端写入，默认省略。 |

`Set()` 首先改变 XData 数据，写到实际 DUT 的时机由 WriteMode 决定。信号必须加入时钟管理的 port，或由调用方负责相应的提交操作。

## 位引用与切片

| API | 用途与约束 |
| --- | --- |
| `At(index)`；C++ `operator[]` | 返回 PinBind；Python 下标读取返回 0/1/2/3，赋值修改该位。 |
| `PinBind.Set/Get/AsInt32/AsString` | 读写一个四态 bit。 |
| `SubDataRef(start, width, name)` | 引用原对象的 `[start, start+width)`，写入切片会影响父信号；父对象须存活。 |
| `SubDataRefRaw(...)` | 裸指针形式，供部分绑定使用；需处理对象所有权。 |
| `SetBits(buffer, count, mask, start)` | byte 重载的 start/count 以 byte 计，word 重载以 32-bit word 计；mask 选择哪些数据位更新，未选中的 aval 位保留；已写区域的 bval 清零。 |
| `GetBits(buffer, count)` | 读取对应粒度的数据，含未知位时返回 false。 |

## 后端绑定

| API | 用途与边界 |
| --- | --- |
| `BindDPIRW(read, write)` | scalar/vector 的 DPI 风格回调；不同宽度使用对应函数签名。 |
| `BindDPIPtr(read_addr, write_addr)` | 从函数地址建立上述绑定；地址必须符合预期 ABI。 |
| Python `BindDPIName(dut, name)` | 从 Picker DUT 的 GetDPIHandle 获取读写入口并设置名称。 |
| `BindNativeData(address)` | 绑定调用方提供的原生存储，绑定时读取初值；实际写回遵循 WriteMode。 |
| `BindMixFromUvs(address)` | UVS 专用读取适配入口，不当作一般 native 读写绑定替代品。 |
| `BindExpr(shared_engine, root)`；`BindConst(uint64)` | 只读表达式结果或常量；结果载荷范围为 64 bit，可零扩展到更宽 XData。 |
| `BindVPI(...)`；`FromVPI(...)`；`IsVPIBinded()` | 使用模拟器的 handle/get/get_value/put_value 接口；FromVPI 创建新对象。 |
| `AsVPIWriteNoDelay/AsVPIWriteForce/AsVPIWriteRelease`；`SetVPIWriteFlag` | 选择 VPI 写回策略。 |
| `AsVPIAuto/AsVPIScale/AsVPIInt/AsVPIVector` | 选择 VPI 数据格式；AsVPIScale 是现有 scalar 接口的拼写。 |

Native vector 存储约定是 1–8 bit 用 uint8、9–16 bit 用 uint16、17 bit 以上用连续 uint32 words；scalar 用一个逻辑值 byte。vector 原生内存只承载 aval。调用方必须提供足够大小并保持存储存活。

<a id="unpacked-arrays"></a>

## 定长 unpacked 数组

[xarray.h](https://github.com/XS-MLVP/xcomm/blob/master/include/xspcomm/xarray.h)
提供 `XDataArray(width, left_bounds, right_bounds)`，用于固定多维
unpacked 数组。一个数组只持有一份共享索引后端，各元素值仍由普通叶子 `XData` 保存。
`FlatIndex(indices)`、`Indices(index)` 按声明顺序换算下标，最后一维变化最快，支持降序和
负 RTL 下标。`Left/Right/Shape/Size` 分别提供边界、各维长度和元素总数。

在创建或绑定叶子前，配置一次后端：

- `BindDPIPtr(getter, setter)` 使用 `void get(int index, void *value)`，以及标量的
  `void set(int index, unsigned char value)` 或位向量的
  `void set(int index, const void *value)`。setter 为零表示只读，总数须适合正 C++ `int`。
- `BindNativeData(base, byte_strides)` 的基地址是各维最小 RTL 下标对应的元素。字节步长
  按从外到内排列；省略时使用上述原生元素表示的连续布局，允许不重叠的行主序 padding。
  会检查维数、下标、位宽、重叠以及尺寸和地址运算。

`At(indices)`/`AtFlat(index)` 按需创建、缓存共享叶子，
`BindElement(existing_xdata, index)` 绑定已有的同宽叶子。`Address(index)` 返回原生元素
地址，`Strides()` 返回字节步长，`IsArrayBound()` 判断叶子是否走数组后端。叶子保存共享
后端状态，以及 DPI 扁平下标或绑定时计算好的原生地址；不会安装逐元素读写
`std::function`。原有写入时序、切片、mask 和变化通知继续有效；原生位向量读取沿用原有
bval 策略，原生内存并不是 SV 四态缓冲区。

数组叶子可直接用于 `ExprEngine` 和 `XTriggerEngine`，无需经过 Python 代理或另行登记
`XPort` 才能采样。宽位向量的 Value 判断会先刷新后端再检查四态有效性；即使叶子没有
变化回调，从 X/Z 恢复后也能在下一次指定采样阶段匹配。

保留叶子可以延长共享元数据的生命周期，但不会延长模型内存、DPI 动态库的生命周期。
需要切换存储时创建新数组，不能重新绑定已有数组。叶子 `ReInit` 或绑定独立后端会解除
数组绑定；复制 XData 仍保持原有快照语义，不复制数组绑定。

`XSignalCFG.NewXDataArrayView(name)` 消费 `array_dimensions` 和 `array_strides`，缓存
原生数组视图。数组 YAML 必须同时提供这两个字段：维度乘积须等于 `array_size`，步长
rank 须与维度 rank 一致。不完整的布局直接拒绝，不做推断。内部视图使用从零开始的存储
坐标；`NewXDataArray` 和带下标的 `NewXData` 也使用同一共享后端。

数组布局使用原生 ABI 4。DPI/native 路径面向 Verilator 5.026 及更新版本，回归验证
版本为 5.026、5.048、5.052。

## 连接、比较、回调

| API | 用途与约束 |
| --- | --- |
| `Connect(other)` | 建立值变化驱动关系。Output 驱动 Input/InOut；InOut 可驱动 Input；两个 InOut 以调用方为源。 |
| `Equal(other)`；`operator==` | XData 间比较四态内容；整数等值还要求没有未知位。C++ 字符串等值比较显示字符串。 |
| `Comp(other, opcode, eq)`；`< <= > >=` | opcode=0/1/2 对应等于/小于/大于，eq 允许相等。完整宽度的无符号顺序比较；涉及 64 bit 或更宽时要求同宽。应用应先保证值已知。 |
| `OnChange(callback, args, desc)`；`ClearOnChangeCbs/HasOnChangeCbs` | 刷新或写入导致缓存变化时回调，参数为 valid、XData 指针、低 64-bit 值、用户参数；在触发变化的调用中同步执行。 |
| `CSelf()` | 获取地址供原生桥接使用，不延长对象寿命。 |

Python 的 `==` 只接受另一个 XData；与整数比较请用 `.value`。Python 宽有符号值推荐从 `U()` 按 W 显式转换，见 [绑定差异](bindings.md)。
