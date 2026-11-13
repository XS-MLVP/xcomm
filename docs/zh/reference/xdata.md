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
| `SubDataRef(start, width, name)` | 引用原对象的 `[start, start+width)`，width 为零时选一个标量 bit；写入影响父信号，父对象须存活，改宽后旧切片失效。 |
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
提供与聚合数组相同的递归 `XArray` 接口，每层接受真实 RTL 下标，支持降序和负下标。
`Left/Right/Size` 表示当前维度的边界和长度；`LeftBounds/RightBounds/Shape` 描述连续
数组维度。`Array(index)` 选择子数组，`Data(index)` 选择已有的 packed 叶子，Python
也支持 `array[i][j]`。

通过工厂创建同质 unpacked 数组及其共享索引后端：

- `XArray.FromDPI(width, left, right, getter, setter)` 使用 `void get(int index, void *value)`，以及标量的
  `void set(int index, unsigned char value)` 或位向量的
  `void set(int index, const void *value)`。setter 为零表示只读，总数须适合正 C++ `int`。
- `XArray.FromNative(width, left, right, base, byte_strides)` 的基地址是各维最小 RTL 下标对应的元素。字节步长
  按从外到内排列；省略时使用上述原生元素表示的连续布局，允许不重叠的行主序 padding。
  会检查维数、下标、位宽、重叠以及尺寸和地址运算。

数组选择按需创建、缓存递归节点和共享叶子。共享绑定、扁平下标换算与元素地址属于内部
实现，不再构成另一套公开数组类型。`IsArrayBound()` 判断叶子是否走数组后端。叶子保存共享
后端状态，以及 DPI 扁平下标或绑定时计算好的原生地址；不会安装逐元素读写
`std::function`。原有写入时序、切片、mask 和变化通知继续有效；原生位向量读取沿用原有
bval 策略，原生内存并不是 SV 四态缓冲区。

数组叶子可直接用于 `ExprEngine` 和 `XTriggerEngine`，无需经过 Python 代理或另行登记
`XPort` 才能采样。宽位向量的 Value 判断会先刷新后端再检查四态有效性；即使叶子没有
变化回调，从 X/Z 恢复后也能在下一次指定采样阶段匹配。

保留叶子可以延长共享元数据的生命周期，但不会延长模型内存、DPI 动态库的生命周期。
需要切换存储时创建新数组，不能重新绑定已有数组。叶子 `ReInit` 或绑定独立后端会解除
数组绑定；使用原数组视图期间不要重新初始化或重新绑定其缓存叶子。复制 XData 仍保持
原有快照语义，不复制数组绑定。

`XSignalCFG.NewXArray(name)` 消费 `array_dimensions` 和 `array_strides`，缓存
原生数组视图。数组 YAML 必须同时提供这两个字段：维度乘积须等于 `array_size`，步长
rank 须与维度 rank 一致。不完整的布局直接拒绝，不做推断。内部视图使用从零开始的存储
坐标；带下标的 `NewXData` 也使用同一共享后端，但其旧有下标仍表示扁平存储位置。

数组布局使用原生 ABI 4。DPI/native 路径面向 Verilator 5.026 及更新版本，回归验证
版本为 5.026、5.048、5.052。

原公开类型 `XDataArray` 及 `NewXDataArray`/`NewXDataArrayView` 工厂已移除，不保留
别名。请改用 `XArray.FromDPI`/`FromNative` 和 `XSignalCFG.NewXArray`。
`Size()` 现在表示当前维度长度而非元素总数；使用逐层选择，不再传完整坐标或扁平下标。
例如 Python：

<!-- runnable:python -->
```python
import ctypes
import xspcomm as xs

memory = (ctypes.c_ubyte * 6)()
array = xs.XArray.FromNative(8, [-1, 3], [1, 2], ctypes.addressof(memory), [2, 1])
array[0][2].AsImmWrite()
array[0][2].value = 42
assert memory[2] == 42
```

## 递归 struct 与 packed 数组视图

`XStruct` 是按字段名组织的独立递归容器，不继承 `XData`。`XArray` 保留一个声明的
数组维度，元素可以是普通 `XData`、另一层数组或 struct。共享实现位于 `xview.h`。
Picker 的 RTL 聚合端口集成要求 Verilator 5.048 及更新版本；xcomm 容器本身不依赖
特定仿真器版本。

| 存储类型 | 表示 |
| --- | --- |
| packed struct | 一个父 `XData`，字段叶子映射位切片 |
| packed array | 一个父 `XData`，数组下标映射位切片 |
| unpacked struct | 独立字段 `XData` 与递归容器 |
| unpacked array | XArray 组织独立元素值与递归容器；同类叶子共享内部索引后端 |

Picker 自动生成递归布局和绑定叶子的路径。`XView.FromPort(port, name, layout_json)`
组织已有叶子，不改变其绑定、写模式或 `XPort` 注册。
`XData.StructView(layout_json)`、`XData.ArrayView(layout_json)` 直接在一个值上建立
packed 视图。布局包含 `kind`、`packed`、`width`（标量叶子为零），可选的
`signed`/`four_state`，数组的 `left`/`right`/`element`，或 struct 的 `fields`；字段含
`name`、`type`，packed 字段还含 `offset`。会检查位宽、边界、重名、字段重叠、空隙和
递归深度，再访问数据。

`XView` 是动态节点的轻量句柄，不是另一种值或后端。`Kind()` 返回 `bits`、`struct` 或
`array`，`Data()`、`Struct()`、`Array()` 要求实际类型完全匹配；不匹配时报告节点路径、
实际类型和目标类型。空句柄、来源已失效的句柄在转换时会报错；`HasValue()` 只检查是否有
节点，不代表来源仍然存活。

C++ 的 `XView` 工厂、`Field`、`At` 和 `XVPI.Get` 按值返回 `XView`。赋给明确类型的目标时会隐式执行
同样的检查，不复制信号数据：

```cpp
XView node = record->Field("payload");
std::shared_ptr<XArray> array = node;   // payload 必须是 array
std::shared_ptr<XStruct> wrong = node; // 抛异常；赋给已有目标时，原目标保持不变
```

packed 容器不隐式转换为 `XData`，需要完整 packed 值时显式调用 `PackedData()`。
`XStruct.Data(name)`、`XArray.Data(index)` 同样是显式选择完整 packed 字段或元素。
Python 通常直接解包为具体类型；Python 类型注解不检查普通变量赋值。

`XStruct.Names()` 保留 RTL 字段声明顺序，`Field(name)` 返回类型节点，
`Data(name)`、`Struct(name)`、`Array(name)` 直接访问字段。`PackedData()` 返回完整
packed 值，对 unpacked 容器报错。`XArray.At(index)` 使用真实 RTL 下标而非位置；
`Data/Struct/Array(index)` 返回选中的对象。`Shape`、`LeftBounds`、`RightBounds`
描述连续且 packed/unpacked 类型相同的各层维度。

Python 支持 `view[name]`、struct 字段属性、数组 `[index]`、`bounds`、`shape` 与按
声明顺序遍历。packed struct 的位向量字段、从 unpacked 数组取出的 packed 值仍是
完整 `XData`；需要按 packed 下标访问时，显式调用 `Array(name)` 或 `Array(index)`。
与方法重名的字段使用 `view[name]`。容器结构只读，应给信号的 `.value` 赋值，不替换字段。

packed 选择按需创建并缓存；切片直接经过父信号分发，不安装逐切片后端回调，同时保留
aval/bval，字段写入不会覆盖其他位。后端写时机与边沿提交由父信号控制，不要额外把切片
注册成普通数据引脚。布局与子容器可以比外层视图活得更久，但来源 `XData`、DUT 内存与
DPI 动态库必须仍然存活；父信号销毁后的切片访问会报错，不解引用已销毁父对象。
Go/Lua 返回的缓存 `XData` 叶子是借用引用，保留叶子时也须保持来源视图存活；生成的
DUT 自身会保存这些视图。
Go 容器句柄由调用者拥有，不再使用时调用 `DeleteXView`、`DeleteXStruct` 或
`DeleteXArray` 释放，包括 `Field`、`At`、`Struct`、`Array` 返回的句柄；不要删除
借用的 `XData` 叶子。Lua 垃圾回收会释放拥有所有权的容器句柄。

## 连接、比较、回调

| API | 用途与约束 |
| --- | --- |
| `Connect(other)` | 建立值变化驱动关系。Output 驱动 Input/InOut；InOut 可驱动 Input；两个 InOut 以调用方为源。 |
| `Equal(other)`；`operator==` | XData 间比较四态内容；整数等值还要求没有未知位。C++ 字符串等值比较显示字符串。 |
| `Comp(other, opcode, eq)`；`< <= > >=` | opcode=0/1/2 对应等于/小于/大于，eq 允许相等。完整宽度的无符号顺序比较；涉及 64 bit 或更宽时要求同宽。应用应先保证值已知。 |
| `OnChange(callback, args, desc)`；`ClearOnChangeCbs/HasOnChangeCbs` | 刷新或写入导致缓存变化时回调，参数为 valid、XData 指针、低 64-bit 值、用户参数；在触发变化的调用中同步执行。 |
| `CSelf()` | 获取地址供原生桥接使用，不延长对象寿命。 |

Python 的 `==` 只接受另一个 XData；与整数比较请用 `.value`。Python 宽有符号值推荐从 `U()` 按 W 显式转换，见 [绑定差异](bindings.md)。
