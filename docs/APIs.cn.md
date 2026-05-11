# xspcomm API 中文文档

本文档基于当前项目源码整理，主要对应以下文件：

- 核心 C++ API：`include/xspcomm/xdata.h`、`xport.h`、`xclock.h`、`xsignal_cfg.h`
- 组合检查与表达式/FSM：`include/xspcomm/xcomuse_base.h`、`xexpr.h`、`xfsm.h`
- 多语言封装：`swig/python/xcomm.py`、`swig/java/java.i`、`swig/scala/xsp.scala`、`swig/golang/golang.i`、`swig/lua/lua.i`、`swig/javascript/xspcomm.js`

xspcomm 的核心抽象是：

- `XData`：DUT 引脚或内部信号的数据对象，支持 4 态逻辑 `0/1/Z/X`、DPI/VPI/native memory 绑定、位切片、读写模式和变化回调。
- `XPort`：一组 `XData` 的命名集合，用于批量连接、批量 IO 类型/写入模式设置，以及被 `XClock` 统一刷新。
- `XClock`：仿真时钟驱动器，负责拉高/拉低时钟引脚、调用 DUT step 函数、在上升沿/下降沿刷新端口并执行回调。
- `XSignalCFG`：从 YAML 配置或 YAML 字符串创建绑定到 native memory 的 `XData`。
- `ComUse*`、`ExprEngine`、`ComUseFsmTrigger`：用于通用仿真辅助，例如条件触发、表达式触发、FSM 触发、数据数组和字符串辅助。

## 通用约定

### 命名空间与头文件

C++ API 位于 `xspcomm` 命名空间。常用头文件：

```c++
#include "xspcomm/xcomm.h"       // 常用核心 API 汇总
#include "xspcomm/xcomuse.h"     // ComUse、Expr、FSM 汇总
```

多语言 SWIG 包通常复用 C++ 名称，部分语言会增加更符合本语言习惯的包装方法，见“多语言差异”。

### IOType

| C++ 枚举 | `XData` 静态别名 | 语义 |
| --- | --- | --- |
| `IOType::Input` | `XData::In` | 输入/可写对象。可由测试侧写入，通常可读可写。 |
| `IOType::Output` | `XData::Out` | 输出/只读对象。`SetWriteMode` 和赋值会被拒绝或警告。 |
| `IOType::InOut` | `XData::InOut` | 双向对象。可读写，连接时根据对端类型决定驱动方向。 |

### WriteMode

| C++ 枚举 | `XData` 静态别名 | 语义 |
| --- | --- | --- |
| `WriteMode::Imme` | `XData::Imme` | 立即写入。赋值后立刻调用绑定的写函数。 |
| `WriteMode::Rise` | `XData::Rise` | 上升沿写入。由 `XClock` 在上升沿调用 `WriteOnRise()`。 |
| `WriteMode::Fall` | `XData::Fall` | 下降沿写入。由 `XClock` 在下降沿调用 `WriteOnFall()`。 |

`XData` 默认写模式是 `Rise`。`XClock::Add(clock_pin)` 会把时钟引脚设置成 `Imme`，因为时钟电平需要在半周期内直接写入。

### 位宽约定

- `mWidth == 0` 表示 SystemVerilog scalar logic，值只能是 `0/1/Z/X`。
- `mWidth > 0` 表示 vector，内部按 32-bit `xsvLogicVecVal` 分片保存 `aval/bval`。
- `W()` 返回位宽。`W() == 0` 时通常表示 scalar logic，不是 0 bit vector。
- `U()` 和 `S()` 只直接返回低 64 bit；宽信号应使用 `GetBytes()` 或各语言的 BigInt 包装。

### 字符串赋值格式

`XData::Set(std::string)` / C++ `operator=(std::string)` 支持：

| 格式 | 示例 | 说明 |
| --- | --- | --- |
| `0b...` | `"0b1010_xz"` | 二进制，从字符串左到右写入低位到高位；`_` 会被忽略，支持 `0/1/x/z`。 |
| `0x...` | `"0x12ff_xz"` | 十六进制，按通常十六进制字面量方向解析；`x/z` 会写入未知/高阻 nibble。 |
| `::...` | `"::ABCD"` | 按字符 ASCII 写入，低地址/低位优先；`_` 会被忽略。 |
| `"x"` / `"z"` | `"x"` | 对 vector 写满 X/Z；对 scalar 写成 X/Z。 |

`String()` 返回十六进制字符串，不带 `0x` 前缀；包含 X/Z 的 byte 会显示为 `??`。`AsBinaryString()` 返回二进制字符串，包含 `x/z`，高位在前。

### 字节序

`GetBytes()` / `SetBytes()` / `GetVU8()` / `SetVU8()` 使用 little-endian 字节序：`bytes[0]` 对应最低 8 bit。返回长度按内部 32-bit 分片对齐，可能大于 `(W()+7)/8`。

### 回调与指针

项目使用 `xfunction<Ret, Args...>` 作为可被 SWIG 导出的 callback 包装。C++ 中可像 `std::function` 一样传 lambda；跨语言时通常由各语言 wrapper 创建 director callback。

`CSelf()` 会返回对象自身地址的 `uint64_t`，主要用于跨语言或 C 回调中传递 `this` 指针。

## PinBind

`PinBind` 表示 `XData` 的某一位，通常通过 `XData::At(index)` 或 C++ `xdata[index]` 获取。

| API | 说明 |
| --- | --- |
| `PinBind &Set(int v)` | 设置一位，`v` 必须是 `0/1/2/3`，分别表示 `0/1/Z/X`。 |
| `PinBind &Set(std::string &v)` | 设置 `"z"` 或 `"x"`。 |
| `int Get()` / `int AsInt32()` | 读取该位，返回 `0/1/2/3`。 |
| `std::string AsString()` | 返回 `"0"`、`"1"`、`"z"`、`"x"`。 |

C++ 中还支持：

```c++
pin[15] = 1;
pin[8] = "x";
int bit = pin[15];
```

Python 中 `xdata[index]` 返回 `int`，赋值使用 `xdata[index] = value`。

## XData

### 构造与成员

| API | 说明 |
| --- | --- |
| `XData()` | 创建 scalar `InOut` 数据，等价于 `XData(0, IOType::InOut)`。 |
| `XData(uint32_t width, IOType itype, std::string name = "")` | 创建指定宽度、IO 类型和名称的对象。 |
| `XData(XData &t)` | 拷贝构造。若复制的是切片引用，会重新绑定切片读写逻辑。 |
| `void ReInit(uint32_t width, IOType itype, std::string name = "")` | 重新初始化对象，释放旧 buffer，重建 bit binding。 |
| `std::string mName` | 信号名。 |
| `IOType mIOType` | IO 类型。 |
| `uint32_t mWidth` | 位宽。 |
| `XData &value` | C++ 自引用成员；Python wrapper 用 `value` 属性实现读写。 |

### 读写与转换

| API | 返回 | 说明 |
| --- | --- | --- |
| `XData &Set(XData &data)` | `XData&` | 拷贝同宽 `XData` 的 raw 4 态数据。 |
| `XData &Set(const char *data)` / `Set(std::string &data)` | `XData&` | 按字符串格式写入。 |
| `XData &Set(int/unsigned int/int64_t/uint64_t data)` | `XData&` | 写入整数，超出位宽的高位会被截断。负数按二进制补码写入。 |
| `XData &Set(std::vector<unsigned char> &buffer)` | `XData&` | 按 little-endian 字节写入 vector。 |
| `template <typename T> XData &ImmSet(T &&data)` | `XData&` | 临时切到 `Imme` 写入，再恢复原写模式，适合需要绕过边沿写模式的一次性写入。 |
| `uint32_t W()` | `uint32_t` | 返回位宽。 |
| `uint64_t U()` | `uint64_t` | 读取低 64 bit 无符号值。 |
| `int64_t S()` | `int64_t` | 读取低 64 bit 有符号值；`1..64` bit 会按最高有效位符号扩展。 |
| `bool B()` | `bool` | 读取低位并转 bool。 |
| `int AsInt32()` / `int64_t AsInt64()` | 整数 | `AsInt32()` 基于 `S()` 截断到 `int`；`AsInt64()` 等价 `S()`。 |
| `std::string String()` | 字符串 | 十六进制字符串，不带 `0x`；含 X/Z 的 byte 显示 `??`。 |
| `std::string AsBinaryString()` | 字符串 | 二进制字符串，支持 `x/z`。 |
| `std::vector<unsigned char> GetBytes()` | bytes | 等价 `GetVU8()`，little-endian，按 32-bit 分片对齐。 |
| `void SetBytes(std::vector<unsigned char> &buffer)` | `void` | 等价 `SetVU8()`。 |
| `bool DataValid()` | `bool` | 当前数据不含 X/Z 时为 true。 |
| `bool Equal(XData &xdata)` | `bool` | 等价 C++ `operator==`。 |

C++ 运算符：

```c++
XData a(32, XData::InOut, "a");
a = 1000;
a = "0xff";
a = "0b1010";
uint64_t u = a;
std::string name = a;  // 转换结果是 mName，不是 String()
bool eq = (a == "000000ff");
```

### IO 类型与写模式

| API | 说明 |
| --- | --- |
| `WriteMode GetWriteMode()` | 返回当前写模式。 |
| `bool SetWriteMode(WriteMode mode)` | 设置写模式；`Output` 类型会返回 false 并打印警告。 |
| `bool IsInIO()` / `IsOutIO()` / `IsBiIO()` | 查询 IO 类型。 |
| `XData &AsInIO()` / `AsOutIO()` / `AsBiIO()` | 修改 IO 类型并返回自身。 |
| `XData &FlipIOType()` | `Input <-> Output` 互换；`InOut` 保持不变。 |
| `bool IsImmWrite()` / `IsRiseWrite()` / `IsFallWrite()` | 查询写模式；`Output` 类型总是返回 false。 |
| `XData &AsImmWrite()` / `AsRiseWrite()` / `AsFallWrite()` | 设置写模式并返回自身。 |
| `void WriteOnRise()` / `WriteOnFall()` | 如果当前写模式匹配，则调用绑定写函数。通常由 `XClock` 调用。 |
| `void WriteDirect()` | 直接调用绑定写函数，不检查写模式。 |
| `void ReadFresh(WriteMode m)` | 根据 `m` 和当前写模式刷新读数据；`Output` 始终可读。 |

### 位操作与切片

| API | 说明 |
| --- | --- |
| `PinBind &At(int index)` / `operator[](uint32_t index)` | 获取一位。`index` 从低位开始，必须小于 `mWidth`。 |
| `std::shared_ptr<XData> SubDataRef(uint32_t start, uint32_t width, std::string name = "")` | 创建引用原对象 `[start, start+width)` 的切片。写切片会同步回原对象。 |
| `XData *SubDataRefRaw(...)` | 与 `SubDataRef` 相同，但返回裸指针。主要用于 Go 包装。 |
| `void SetBits(uint8_t *buffer, int count, uint8_t *mask = nullptr, int start = 0)` | 以 byte 为单位写入，`start` 是 byte 偏移。 |
| `void SetBits(uint32_t *buffer, uint32_t count, uint32_t *mask = nullptr, uint32_t start = 0)` | 以 32-bit word 为单位写入，`start` 是 word 偏移。 |
| `bool GetBits(uint8_t *buffer, uint32_t count)` | 读出 byte 数据；如果读取范围含 X/Z 返回 false。 |
| `bool GetBits(uint32_t *buffer, uint32_t count)` | 读出 32-bit word 数据；如果读取范围含 X/Z 返回 false。 |

示例：

```c++
XData full(128, XData::InOut, "full");
full = "0xffffffffffffffffffffffffffffffff";

auto sub = full.SubDataRef(30, 64, "sub");
*sub = "0xababa12ba98babab";   // 同步修改 full 的 [30, 94) 位
```

### 绑定 DPI、VPI 与 native memory

| API | 说明 |
| --- | --- |
| `void BindDPIPtr(uint64_t read_ptr, uint64_t write_ptr)` | 按地址绑定 DPI read/write 函数。vector 和 scalar 会选择不同函数签名。 |
| `void BindDPIRW(...)` | 直接绑定读写 callback。`mWidth > 0` 使用 vector 回调，`mWidth == 0` 使用 scalar 回调。 |
| `void BindNativeData(uint64_t pdata)` | 绑定本地内存地址。读写 `XData` 会同步到该地址。 |
| `bool BindVPI(vpiHandle obj, func_vpi_get get, func_vpi_get_value get_value, func_vpi_put_value put_value, std::string name = "")` | 绑定 VPI object，并按 object 类型/宽度重建 `XData`。 |
| `static XData *FromVPI(...)` | 创建并绑定一个新的 `XData`；失败返回 `nullptr`。 |
| `bool IsVPIBinded()` | 是否已经绑定 VPI。 |
| `XData &AsVPIWriteNoDelay()` / `AsVPIWriteForce()` / `AsVPIWriteRelease()` | 设置 VPI put value flag。 |
| `XData &SetVPIWriteFlag(int flag)` | 设置自定义 VPI write flag。 |
| `XData &AsVPIAuto()` / `AsVPIScale()` / `AsVPIInt()` / `AsVPIVector()` | 设置 VPI 读写格式选择。`AsVPIScale` 为源码中现有拼写，语义是 scalar。 |

### 连接、比较、回调

| API | 说明 |
| --- | --- |
| `bool Connect(XData &xdata)` | 连接两个不同方向的 `XData`。驱动端变化时自动写入被驱动端。 |
| `XData &Invert()` | 按位取反；vector 的 X/Z mask 会被清零。 |
| `bool Comp(XData &data, int opcode, int eq = 0)` | 比较。`opcode=0/1/2` 分别是等于/小于/大于；`eq` 允许相等。 |
| `operator < <= > >= ==` | C++ 比较运算符。宽信号比较要求位宽兼容。 |
| `void OnChange(xfunction<void, bool, XData *, uint64_t, void *> func, void *args = nullptr, std::string desc = "")` | 数据变化时回调。参数为 `valid`、自身指针、低 64 bit 值、用户参数。 |
| `void ClearOnChangeCbs()` | 清空变化回调。 |
| `void SetIgnoreSameDataWrite(bool w)` | 控制相同数据是否跳过写函数。默认跳过相同写入。 |
| `uint64_t CSelf()` | 返回自身地址。 |

`Connect()` 的方向规则：

- `Output -> Input/InOut`：允许。
- `InOut -> Input`：允许。
- `Output/Input` 同类型连接会失败。
- `InOut <-> InOut` 会以调用方为驱动端建立变化回调。

## XPort

`XPort` 是一组带前缀命名的 `XData*`。`Add("valid", data)` 在内部保存 raw key `prefix + "valid"`，同时保留用户 key `"valid"`。

| API | 说明 |
| --- | --- |
| `XPort(std::string prefix = "")` | 创建 port。 |
| `int PortCount()` | 返回 pin 数量。 |
| `bool Add(std::string pin, XData &pin_data)` | 添加 pin。重复 key 返回 false。 |
| `bool Del(std::string pin)` | 删除 pin。 |
| `bool Has(std::string key, bool raw_key = false)` | 判断 key 是否存在。`raw_key=false` 时会自动加前缀。 |
| `XData &Get(std::string key, bool raw_key = false)` | 获取 pin，不存在会 assert。 |
| `XData &operator[](std::string key)` | C++ 按 key 获取 pin，等价 `Get(key)`。 |
| `bool Connect(XPort &target)` | 按用户 key 逐一连接两个 port 中同名 pin。缺失 pin 会警告并继续。 |
| `XPort &NewSubPort(std::string subprefix)` | 创建 `prefix + subprefix` 开头的子 port。返回动态分配对象的引用，调用方需要自行管理生命周期。 |
| `XPort &SelectPins(std::vector<std::string> pins)` | 选择若干用户 key 生成新 port。返回动态分配对象的引用，调用方需要自行管理生命周期。 |
| `XPort &SelectPins(std::initializer_list<std::string> pins)` | C++ initializer list 版本；SWIG 中被忽略。 |
| `XPort &FlipIOType()` | 批量调用 `XData::FlipIOType()`。 |
| `XPort &AsBiIO()` | 批量设为 InOut。 |
| `XPort &AsImmWrite()` / `AsRiseWrite()` / `AsFallWrite()` | 批量设置非 Output pin 的写模式。 |
| `XPort &WriteOnRise()` / `WriteOnFall()` | 批量触发边沿写入。通常由 `XClock` 调用。 |
| `XPort &ReadFresh(WriteMode m)` | 批量刷新带 OnChange callback 的 pin。 |
| `XPort &SetZero()` | 把所有非 Output pin 写 0。 |
| `std::string String(std::string prefix = "")` | 返回形如 `key=0xvalue` 的调试字符串。 |
| `std::string GetPrefix()` | 返回 port 前缀。 |
| `std::vector<std::string> GetKeys(bool raw_key = false)` | 返回用户 key 或 raw key。 |
| `uint64_t CSelf()` | 返回自身地址。 |

示例：

```c++
XData valid(1, XData::InOut, "valid");
XData data(32, XData::InOut, "data");

XPort req("req_");
req.Add("valid", valid);
req.Add("data", data);

req["valid"] = 1;
auto keys = req.GetKeys();      // ["data", "valid"]，顺序由 map 决定
```

## XClock

`XClock` 将 DUT step 函数、时钟 pin、需要边沿读写的 `XPort` 组织到统一调度中。

### 构造与初始化

| API | 说明 |
| --- | --- |
| `XClock()` | 创建空 clock；未设置 step 函数时调用 step 会警告。 |
| `XClock(xfunction<int, bool> stepfunc, initializer_list<XData*> clock_pins = {}, initializer_list<XPort*> ports = {})` | 绑定 step callback、时钟引脚和 ports。 |
| `void ReInit(xfunction<int, bool> stepfunc, initializer_list<XData*> clock_pins = {}, initializer_list<XPort*> ports = {})` | 重新设置 step 函数并追加 pins/ports。 |
| `XClock(uint64_t stepfunc, uint64_t dut, ...)` | 绑定 C 函数地址，函数签名按 `int step(uint64_t dut, uint64_t cycle, bool dump)` 调用。 |
| `void ReInit(uint64_t stepfunc, uint64_t dut, ...)` | 地址版本重新初始化。 |

`stepfunc(bool dump)` 会被 `XClock` 在半周期中调用。`dump=false` 通常表示只推进组合逻辑，`dump=true` 通常表示推进时间并 dump 波形，具体语义取决于 DUT 后端。

### 调度与控制

| API | 说明 |
| --- | --- |
| `XClock &Add(XData *d)` / `Add(XData &d)` | 添加时钟 pin，并设置为 `Imme` 写入。 |
| `XClock &Add(XPort *p)` / `Add(XPort &p)` | 添加需要边沿刷新/写入的 port。 |
| `XClock &AddPin(...)` | `Add(XData...)` 的别名。 |
| `void Step(int s = 1)` | 推进 `s` 个完整周期。默认每周期先下降沿后上升沿，使周期结束停在上升沿。 |
| `void RunStep(int s = 1)` | C++ 中等价 `Step(s)`。Python/JS 中是 async 驱动函数。 |
| `void RefreshComb()` / `eval()` | 调用 step 函数，`dump=false`，不推进 `clk`。 |
| `void RefreshCombT()` / `eval_t()` | 调用 step 函数，`dump=true`，不推进 `clk`。 |
| `void Reset()` | 把 `clk` 计数清 0。 |
| `void default_stop_on_rise(bool rise)` | 设置完整周期结束时停在上升沿还是下降沿。 |
| `uint64_t clk` | 当前周期计数。 |
| `bool IsDisable()` / `Disable()` / `Enable()` | 查询/控制 clock 是否停止。`Step()` 在 disabled 时直接返回。 |
| `uint64_t CSelf()` | 返回自身地址。 |

`Step()` 内部流程简化如下：

1. `clk += 1`
2. 根据 `stop_on_rise` 执行两个半周期
3. 半周期中写 clock pins、调用 DUT step、写 ports、刷新 ports、执行对应边沿回调
4. 调度 C++ coroutine awaiter

禁止在 `StepRis`/`StepFal` 回调内部再次调用同一个 `XClock::Step()`；源码会打印警告并忽略。

### 边沿回调

| API | 说明 |
| --- | --- |
| `void StepRis(xfunction<void, uint64_t, void *> func, void *args = nullptr, std::string desc = "")` | 添加上升沿回调。 |
| `void StepFal(xfunction<void, uint64_t, void *> func, void *args = nullptr, std::string desc = "")` | 添加下降沿回调。 |
| `void StepRis(uint64_t func, uint64_t args = 0, std::string desc = "")` | C 函数地址版本。 |
| `void StepFal(uint64_t func, uint64_t args = 0, std::string desc = "")` | C 函数地址版本。 |
| `int RemoveStepRisCbByDesc(std::string desc)` / `RemoveStepFalCbByDesc` | 按描述删除，返回删除数量。 |
| `int RemoveStepRisCbByFunc(...)` / `RemoveStepFalCbByFunc` | 按 callback 删除。 |
| `int RemoveStepRisCb(...)` / `RemoveStepFalCb(...)` | 同时按 callback 和 desc 删除。 |
| `std::vector<std::string> ListSteRisCbDesc()` / `ListSteFalCbDesc()` | 列出 callback 描述。源码中函数名为 `ListSte...`。 |
| `int StepRisQueueSize()` / `StepFalQueueSize()` | 回调队列长度。 |
| `void ClearRisCallBacks()` / `ClearFalCallBacks()` | 清空回调。 |

### 分频时钟

| API | 说明 |
| --- | --- |
| `void FreqDivWith(int div, XClock *clk, int shift = 0)` | 把目标 clock 绑定为当前 clock 的分频 clock。目标 clock 每 `div` 个源周期触发一个周期，`shift` 是半周期偏移。 |
| `void FreqDivWith(int div, XClock &clk, int shift = 0)` | 引用版本。 |
| `void FreqDivDelete(XClock *clk)` / `FreqDivDelete(XClock &clk)` | 删除分频绑定。 |

### FastMode

| 枚举 | 值 | 说明 |
| --- | --- | --- |
| `FastMode::Default` | `0` | 默认模式，上升沿和下降沿都 step/refresh。 |
| `FastMode::IGNORE_READ_REFRESH` | `1` | 跳过 port read refresh。 |
| `FastMode::IGNORE_STEP_FALSE` | `2` | 跳过 `step(false)`，只调用 `step(true)`。 |
| `FastMode::IGNORE_PORT_WRITE` | `3` | 跳过 port write，常配合 `XData` 立即写模式。 |
| `FastMode::ONLY_STEP_RIS` | `-1` | 只执行上升沿半周期。 |
| `FastMode::ONLY_STEP_FAL` | `-2` | 只执行下降沿半周期。 |

| API | 说明 |
| --- | --- |
| `void SetFastMode(int level)` / `SetFastMode(FastMode mode)` | 设置 fast mode。 |
| `int GetFastMode()` | 返回当前 fast mode 值。 |

### C++ coroutine API

需要编译时启用 C++20 coroutine。

| API | 说明 |
| --- | --- |
| `XStep AStep(int i = 1)` | `co_await` 等待 `i` 个 `Step()` 周期。 |
| `XCondition ACondition(std::function<bool(void)> checker)` | `co_await` 等到 `checker()` 为 true。 |
| `XNext ANext(int n = 1)` | 等待调度次数。 |

示例：

```c++
xcorutine<> wait_valid(XClock &clk, XData &valid) {
    co_await clk.ACondition([&]() { return valid == 1; });
    co_await clk.AStep(1);
}
```

## XSignalCFG

`XSignalCFG` 从 YAML 文件路径或 YAML 字符串加载信号布局，然后基于 `base_address + offset` 创建绑定 native memory 的 `XData`。

支持两种 YAML 结构。

列表模式：

```yaml
variables:
  - name: Top.clock
    type: CData
    mem_bytes: 1
    rtl_width: 1
    offset: 8
```

映射模式：

```yaml
variables:
  Top:
    clock:
      type: CData
      mem_bytes: 1
      rtl_width: 1
      offset: 8
```

配置项：

| 字段 | 说明 |
| --- | --- |
| `offset` | 相对 `base_address` 的字节偏移，必填。 |
| `mem_bytes` | 单个元素占用字节数，必填。 |
| `rtl_width` | RTL 位宽，必填。`rtl_width == 1` 会创建 `mWidth == 0` 的 scalar `XData`。 |
| `array_size` | 数组元素数量，数组 API 使用。 |
| `type` | 类型描述字符串，仅保存/展示，不参与绑定逻辑。 |

| API | 说明 |
| --- | --- |
| `XSignalCFG(std::string path_or_str_data, uint64_t base_address = 0)` | 创建配置对象。参数既可以是文件路径，也可以是 YAML 字符串。 |
| `XData *NewXData(std::string name, std::string xname = "")` | 创建并绑定一个信号。`xname` 为空时使用 `name`。 |
| `XData *NewXData(std::string name, int array_index, std::string xname = "")` | 创建并绑定数组元素。地址为 `base + offset + mem_bytes * array_index`。 |
| `std::vector<std::shared_ptr<XData>> NewXDataArray(std::string name, std::string xname = "")` | 根据 `array_size` 创建整组数组元素。 |
| `std::vector<std::string> GetSignalNames(std::string pattern = "")` | 列出信号名；`pattern` 非空时按 substring 过滤。 |
| `s_xsignal_cfg At(std::string name)` / `operator[]` | 返回信号配置。不存在时返回 `is_empty=true` 的默认配置并打印错误。 |
| `uint64_t Address(std::string name)` | 返回 `base_address + offset`。 |
| `std::string String()` | 输出配置详情。 |

示例：

```c++
uint64_t mem[16] = {};
XSignalCFG cfg("tests/test_signal_cfg.yaml", (uint64_t)mem);

auto clk = cfg.NewXData("Cache_top.clock");
auto rst = cfg.NewXData("Cache_top.reset");
clk->AsImmWrite();
*clk = 1;
```

## ExprEngine

`ExprEngine` 编译和求值表达式，常用于触发条件。表达式中的信号名会通过 `RegisterExternalSignal()` 或 `XSignalCFG` 解析到 `XData`。

### 表达式语法

支持：

- 数字：十进制、`0x` 十六进制、`0b` 二进制；允许 `_` 分隔。
- 信号名：字母、数字、`_`、`.`、`$`。
- 算术：`+`、`-`、`*`、`/`、`%`。
- 位运算：`~`、`&`、`^`、`|`、`<<`、`>>`。
- 逻辑：`!`/`not`、`&&`/`and`、`||`/`or`。
- 比较：`==`、`!=`、`>`、`>=`、`<`、`<=`。
- 时序辅助：`within(window, expr)`、`hold(window, expr)`。
- 括号：`(...)`。

限制：

- 普通算术/位运算目前只支持 64 bit 范围。
- 宽信号（`W() > 64`）只支持与常量或同宽信号做比较，不支持参与算术/位运算。
- 除 0 和模 0 返回 0，不抛异常。
- `within`/`hold` 依赖 `SetCycle()` 提供当前周期。

`within(n, expr)`：`expr` 当前为 true 时返回 true；之后 `n` 个周期内仍返回 true。

`hold(n, expr)`：`expr` 连续 true 达到 `n` 个周期后返回 true。

### API

| API | 说明 |
| --- | --- |
| `int NewConst(uint64_t v)` | 创建常量节点。 |
| `int NewSignal(XData *sig)` | 创建信号节点。 |
| `int NewUnary(ExprOp op, int child)` | 创建一元节点。 |
| `int NewBinary(ExprOp op, int lhs, int rhs)` | 创建二元节点。 |
| `int NewCompare(ExprOp op, int lhs, int rhs)` | 创建普通比较节点。 |
| `int NewCompareSigSig(ExprOp op, XData *lhs, XData *rhs)` | 创建宽信号比较节点。 |
| `int NewCompareSigConst(ExprOp op, XData *lhs, uint64_t rhs)` | 创建信号与常量比较节点。 |
| `int NewCompareConstSig(ExprOp op, uint64_t lhs, XData *rhs)` | 创建常量与信号比较节点。 |
| `int NewWithin(int child, uint64_t window)` | 创建 `within` 节点。 |
| `int NewHold(int child, uint64_t window)` | 创建 `hold` 节点。 |
| `void RegisterExternalSignal(const std::string &name, XData *sig)` | 注册外部信号。优先级高于 `XSignalCFG` 自动创建。 |
| `XData *GetOrCreateSignal(XSignalCFG *cfg, const std::string &name)` | 获取或通过 cfg 创建信号。 |
| `int CompileExpr(std::string expr, XSignalCFG *cfg)` | 编译表达式，返回 root id；失败抛 `std::runtime_error`。 |
| `uint64_t Eval(int root)` | 求值。返回 0 表示 false，非 0 表示 true/数值。 |
| `void SetCycle(uint64_t cycle)` | 设置当前周期，供 `within/hold` 使用。 |
| `void ResetState()` | 重置 `within/hold` 的历史状态。 |
| `void OptimizeShortCircuitOrder(int root)` | 优化无状态逻辑短路顺序。`CompileExpr()` 会自动调用。 |
| `void Clear()` | 清空所有节点和内部信号。 |

示例：

```c++
XData valid(1, XData::InOut);
XData data(32, XData::InOut);

ExprEngine eng;
eng.RegisterExternalSignal("valid", &valid);
eng.RegisterExternalSignal("data", &data);

int root = eng.CompileExpr("valid && data == 0x1234", nullptr);
eng.SetCycle(10);
if (eng.Eval(root)) {
    // triggered
}
```

## ComUse 系列

### ComUseStepCb

`ComUseStepCb` 是可挂到 `XClock::StepRis/StepFal` 的基类。派生类覆写 `Call()` 实现逻辑。

| API | 说明 |
| --- | --- |
| `static uint64_t GetCb()` | 返回静态 trampoline 函数地址。可传给 `XClock::StepRis(GetCb(), CSelf())`。 |
| `static void Cb(uint64_t c, void *self)` | trampoline：设置 `cycle`，调用 `Call()`，并处理最大次数。 |
| `virtual void Call()` | 业务逻辑入口。基类实现只打印错误。 |
| `void Disable()` / `Enable()` / `bool IsDisable()` | 控制该 callback 是否生效。 |
| `void SetMaxCbs(int c)` | 设置最大触发次数；触发次数达到后自动 Disable。 |
| `int GetCbCount()` / `IncCbCount()` / `DecCbCount()` | 触发计数。派生类通常在实际触发时调用 `IncCbCount()`。 |
| `void Reset()` | 启用 callback 并清零计数。 |
| `uint64_t CSelf()` | 返回自身地址。 |
| `uint64_t cycle` | 当前回调周期，由 `Cb()` 设置。 |

### ComUseEcho

当 `valid != 0` 时打印 `data`。

| API | 说明 |
| --- | --- |
| `ComUseEcho(uint64_t valid, uint64_t data, bool stderr_echo = true, std::string fmt = "%c", int convert = 0)` | `valid` 和 `data` 是 `XData*` 地址，通常传 `xdata.CSelf()`。 |
| `void Call()` | valid 为真时按格式输出。 |

`convert` 取值：

| 值 | 输出类型 |
| --- | --- |
| `0` | `char` |
| `1` | `int64_t` |
| `2` | `float` |
| `3` | `double` |
| `4` | `data->String().c_str()` |

### ComUseCondCheck

`ComUseCondCheck` 检查一组条件；任一条件触发时会 `Disable()` 绑定的 clocks，并记录触发 key。

比较模式：

| 枚举 | 说明 |
| --- | --- |
| `ComUseCondCmp::EQ` | 等于 |
| `ComUseCondCmp::NE` | 不等于 |
| `ComUseCondCmp::GT` | 大于 |
| `ComUseCondCmp::GE` | 大于等于 |
| `ComUseCondCmp::LT` | 小于 |
| `ComUseCondCmp::LE` | 小于等于 |

| API | 说明 |
| --- | --- |
| `ComUseCondCheck(XClock *clk = nullptr)` | 可选绑定 clock。 |
| `void BindXClock(XClock *clk)` | 添加触发后需要 disable 的 clock。 |
| `void SetCondition(std::string unique_name, XData *pin, XData *val, ComUseCondCmp cmp, XData *valid = nullptr, XData *valid_value = nullptr, xfunction<bool, XData*, XData*, uint64_t> func = nullptr, uint64_t arg = 0)` | 添加/更新 XData 条件。valid 非空时先检查 valid 与 valid_value。当前源码对 XData 自定义 `func` 有 assert 限制，常规用法应传 `nullptr`。 |
| `void SetCondition(std::string unique_name, uint64_t pin_ptr, uint64_t val_ptr, ComUseCondCmp cmp, int bytes, uint64_t valid_ptr = 0, uint64_t valid_value_ptr = 0, int valid_bytes = 1, xfunction<bool, uint64_t, uint64_t, uint64_t> func = nullptr, uint64_t arg = 0)` | 添加/更新内存指针条件。指针比较按 `bytes` 做有符号比较。 |
| `void RemoveCondition(std::string unique_name)` | 删除条件。 |
| `std::map<std::string, bool> ListCondition()` | 返回所有条件及本轮是否触发。 |
| `std::vector<std::string> GetTriggeredConditionKeys()` | 返回本轮触发的 key。 |
| `ComUseCondCmp GetValidCmpMode(std::string unique_name)` | 获取 valid gating 的比较模式。 |
| `void SetValidCmpMode(std::string unique_name, ComUseCondCmp cmp)` | 设置 valid gating 比较模式，默认 `EQ`。 |
| `void ClearClock()` / `ClearCondition()` / `ClearAll()` | 清空绑定 clock、条件或全部。 |
| `xfunction<bool, XData*, XData*, uint64_t> AsXDataXFunc(uint64_t func)` | 把函数地址转成 XData 自定义比较 callback。 |
| `xfunction<bool, uint64_t, uint64_t, uint64_t> AsPtrXFunc(uint64_t func)` | 把函数地址转成 pointer 自定义比较 callback。 |
| `void Call()` | 执行检查。通常挂到 clock 边沿。 |

示例：

```c++
ComUseCondCheck check(&clk);
clk.StepRis(check.GetCb(), check.CSelf(), "cond-check");
check.SetCondition("data_eq", &data, &expect, ComUseCondCmp::EQ, &valid, &one);

clk.Step(1000);  // 触发后 clk 会被 Disable()
auto keys = check.GetTriggeredConditionKeys();
```

### ComUseExprCheck

`ComUseExprCheck` 是基于 `ExprEngine` 的表达式触发器。任一表达式求值为非 0 时会 disable 绑定 clocks，并记录触发 key。

| API | 说明 |
| --- | --- |
| `ComUseExprCheck(XClock *clk = nullptr)` | 可选绑定 clock。 |
| `void BindXClock(XClock *clk)` | 添加触发后需要 disable 的 clock。 |
| `int CompileExpr(std::string expr, XSignalCFG *cfg)` | 编译表达式。失败返回 `-1` 并打印错误。 |
| `void SetExpr(std::string name, int root)` | 添加/更新表达式 root。 |
| `void RemoveExpr(std::string name)` | 删除表达式。 |
| `std::map<std::string, bool> ListExpr()` | 返回所有表达式及本轮是否触发。 |
| `std::vector<std::string> GetTriggeredExprKeys()` | 返回本轮触发表达式 key。 |
| `void ClearExpr()` / `ClearAll()` | 清空表达式和内部 engine。 |
| `void Call()` | 执行表达式检查。 |
| `ExprNewConst/Signal/Unary/Binary/Compare/...` | 直接透传到内部 `ExprEngine` 的节点构造 API。 |

示例：

```c++
ComUseExprCheck checker(&clk);
clk.StepRis(checker.GetCb(), checker.CSelf(), "expr-check");

XData valid(1, XData::InOut);
XData data(32, XData::InOut);
int valid_node = checker.ExprNewSignal(&valid);
int data_eq = checker.ExprNewCompareSigConst((int)ExprOp::EQ, &data, 0x55);
int root = checker.ExprNewBinary((int)ExprOp::LAND, valid_node, data_eq);
checker.SetExpr("hit_data", root);
```

### ComUseFsmTrigger

`ComUseFsmTrigger` 加载一个简易 FSM 脚本。触发 `trigger` 后会 disable 绑定 clocks。

| API | 说明 |
| --- | --- |
| `ComUseFsmTrigger(XClock *clk = nullptr)` | 可选绑定 clock。 |
| `void BindXClock(XClock *clk)` | 添加触发后需要 disable 的 clock。 |
| `void LoadProgram(std::string program, XSignalCFG *cfg)` | 加载 FSM 脚本。解析失败会打印错误并清空 FSM，不抛异常给调用者。 |
| `void Reset()` | 回到 start state，清空 `$flag*` 和 `$counter*`。 |
| `void Clear()` | 清空 FSM。 |
| `bool IsTriggered()` | 是否已经触发。 |
| `std::string GetTriggeredState()` | 返回触发所在 state。 |
| `std::string GetCurrentState()` | 返回当前 state；无有效状态时返回空字符串。 |
| `std::vector<std::string> ListStates()` | 列出 state 名。 |
| `void Call()` | 执行当前状态动作和转移。 |

FSM 脚本语法：

```text
start S0

state S0:
  set $flag0
  if signal_a == 1 goto S1
  else goto S2

state S1:
  inc $counter0
  if $counter0 >= 2 trigger

state S2:
  clear $flag0
  trigger
```

支持语句：

| 语句 | 说明 |
| --- | --- |
| `start STATE` | 设置起始状态。省略时默认第一个 state。 |
| `state NAME:` | 定义状态。 |
| `set $flag*` / `clear $flag*` | 设置/清除 1-bit 内部 flag。 |
| `inc $counter*` / `reset $counter*` | 增加/清零 64-bit 内部 counter。 |
| `if EXPR goto STATE` | 条件转移。 |
| `if EXPR trigger` | 条件触发。 |
| `elif` / `elseif` | 与 `if` 语义相同，按书写顺序尝试。 |
| `else goto STATE` / `else trigger` | 无条件兜底转移/触发。 |
| `goto STATE` | 无条件转移。 |
| `trigger` | 无条件触发。 |
| `# comment` | 行注释。 |

表达式语法同 `ExprEngine`。`$flag*` 和 `$counter*` 是 FSM 内部变量，也可在表达式中使用。

### ComUseDataArray

`ComUseDataArray` 是 byte buffer 包装，可拥有内存，也可引用外部地址。

| API | 说明 |
| --- | --- |
| `ComUseDataArray(int byte_size)` | 创建自有 buffer。 |
| `ComUseDataArray(uint64_t base, int byte_size)` | 引用外部地址。 |
| `ComUseDataArray *Copy()` | 拷贝一份自有 buffer。 |
| `void SyncFrom(uint64_t addr, int size)` | 从地址拷贝到内部 buffer。 |
| `void SyncTo(uint64_t addr, int size)` | 从内部 buffer 拷贝到地址。 |
| `void SetZero()` | 清零。 |
| `uint64_t BaseAddr()` | 返回内部 buffer 地址。 |
| `int Size()` | 返回 byte size。 |
| `std::vector<unsigned char> AsBytes()` | 返回 bytes。 |
| `int FromBytes(std::vector<unsigned char> &input)` | 从 bytes 写入，返回实际写入长度。 |
| `operator==` | 比较 size 和内容。 |

### ComUseRangeCheck

`ComUseRangeCheck` 生成可传给 `ComUseCondCheck` 的范围比较函数。

| API | 说明 |
| --- | --- |
| `ComUseRangeCheck(int range, int bytes)` | `bytes <= 8`。 |
| `static bool cmp(uint64_t t, uint64_t c, int r)` | 当 `r >= 0` 时检查 `c-r <= t <= c`；当 `r < 0` 时检查 `c <= t <= c-r`。 |
| `uint64_t CSelf()` | 返回自身地址，用作 callback arg。 |
| `GetArrayCmp()` | 返回 pointer 比较 callback。 |
| `GetXDataCmp()` | 返回 XData 比较 callback。 |

### CString

跨语言传递/保存 C 字符串的辅助对象。

| API | 说明 |
| --- | --- |
| `CString(std::string val = "")` | 构造字符串。 |
| `uint64_t CharAddress()` | 返回 `str.c_str()` 地址。 |
| `void AssignTo(char *addr)` / `AssignTo(uint64_t addr)` | 当前实现只是把局部指针改为 `c_str()`，不会拷贝内容到目标地址。使用时需注意。 |
| `void AssignFrom(const char *val)` / `AssignFrom(uint64_t val)` | 从 C 字符串地址读取。 |
| `std::string Get()` / `void Set(std::string val)` | 读写字符串。 |

### 数组与指针辅助函数

| API | 说明 |
| --- | --- |
| `GetFromU64Array/GetFromU32Array/GetFromU8Array(address, index)` | 从地址或指针读取数组元素。 |
| `SetU64Array/SetU32Array/SetU8Array(address, index, data)` | 写数组元素。 |
| `U64PtrAsU64/U32PtrAsU64/U8PtrAsU64(ptr)` | 指针转 `uint64_t`。 |
| `U64AsU64Ptr/U64AsU32Ptr/U64AsU8Ptr(uint64_t)` | `uint64_t` 转指针。 |

## 工具函数

`xspcomm/xutil.h` 中常用公开工具：

| API | 说明 |
| --- | --- |
| `LogLevel get_log_level()` / `void set_log_level(LogLevel val)` | 获取/设置日志等级。 |
| `std::string version()` | 返回库版本。 |
| `bool checkVersion()` | 检查动态库版本与头文件版本是否一致。 |
| `long uTime()` | 当前微秒时间戳。 |
| `std::string fmtTime(long utime, std::string fmt = "%Y-%m-%d %H:%M:%S")` | 格式化微秒时间戳。 |
| `std::string fmtNow(std::string fmt = "%Y-%m-%d %H:%M:%S")` | 格式化当前时间。 |
| `std::string sFmt(const std::string &format, Args... args)` | `snprintf` 风格字符串格式化。 |
| `std::string sArrayHex(unsigned char *buff, int size)` | bytes 转十六进制显示。 |
| `bool sWith(const std::string &str, const std::string &prefix)` | starts-with 判断。 |
| `std::string sLower(std::string input)` | 转小写。 |
| `std::string FmtSize(uint64_t s)` | 格式化大小。 |
| `uint64_t xRandom(uint64_t a, uint64_t b)` | 返回 `[a, b)` 范围随机数。 |
| `void XSeed(unsigned int seed)` | 设置随机种子。 |
| `bool fileExists(const std::string &fileName)` | 文件是否存在。 |

日志宏：

| 宏 | 说明 |
| --- | --- |
| `Info/Warn/Error/Debug/Fatal(fmt, ...)` | 日志输出；`Fatal` 会退出。 |
| `Assert(c, fmt, ...)` | 条件为 false 时输出 fatal 日志、打印栈并退出。 |
| `DebugC(c, fmt, ...)` | 条件为 true 时输出 debug。 |

## Python API 差异

Python 包装模块名为 `xspcomm`。

### XData

Python 用 `value` 属性模拟赋值/读取：

```python
from xspcomm import *

pin = XData(129, XData.InOut)
pin.value = 1000
pin.value = -1
pin.value = "0x1234"
pin.value = b"\x34\x12"

u = pin.value   # 无符号 Python int，宽信号也支持大整数
s = pin.S()     # 有符号 Python int
pin[15] = 1
bit = pin[15]
```

注意：

- `pin.value` 默认返回无符号值。
- `pin.S()` 对宽信号返回有符号大整数。
- `XData.__eq__` 被包装为只允许和 `XData` 比较；如果要和整数比较请用 `pin.value`。
- `Set(bytes)` 会调用 `SetBytes()`。
- `ImmSet(...)` 也已在 Python wrapper 中暴露，语义是临时切到立即写模式写入后恢复原模式。
- `BindDPIName(dut, name)` 是 Python wrapper 增加的方法，会通过 `dut.GetDPIHandle(name, rw)` 绑定 DPI。

### XPort

```python
port = XPort("req_")
port.Add("valid", valid)
port["valid"] = 1
data = port["valid"].value
```

`__getitem__` 会检查 key 存在；`__setitem__` 会写 `XData.value`。

### XClock

Python `XClock` 构造支持 Python callable：

```python
clk = XClock(lambda dump: 0)
clk.StepRis(lambda cycle: print("rise", cycle))
clk.StepFal(lambda cycle: print("fall", cycle))
clk.Step(10)
```

也支持 C 函数地址：

```python
clk = XClock(TEST_get_u64_step_func(), 0x123)
clk.StepRis(TEST_get_u64_ris_fal_cblback_func(), 0x456)
```

异步 API：

```python
import asyncio
from xspcomm import *

async def main():
    clk = XClock(lambda dump: 0)
    task = asyncio.create_task(clk.RunStep(30))
    await clk.AStep(3)
    await clk.ACondition(lambda: clk.clk == 20)
    await task

asyncio.run(main())
```

Python wrapper 中：

- `RawStep` 是原始 C++ `Step`。
- `Step` 会逐周期检查 callback 异常，并驱动 asyncio event。
- `RunStep` 是 async 函数，用于和其他 coroutine 协同推进时钟。
- `Event` 和 `Queue` 是 Python wrapper 辅助类型，用于在同一仿真 tick 内同步 coroutine。

## Java / Scala API 差异

Java/Scala 通过 SWIG jar 暴露 API。初始化通常需要：

```java
import com.xspcomm.*;

xspcomm.init();
```

### Java XData BigInteger 包装

Java 为 `XData` 增加：

| API | 说明 |
| --- | --- |
| `void Set(BigInteger v)` | 写入任意宽整数。负数按补码扩展到 `W()`。 |
| `BigInteger Get()` | 获取无符号大整数。 |
| `BigInteger U()` | 等价 `Get()`。 |
| `BigInteger S()` | 获取有符号大整数。 |
| `BigInteger U64()` | 原 C++ `U()` 重命名，返回低 64 bit 无符号值。 |
| `long S64()` | 原 C++ `S()` 重命名，返回低 64 bit 有符号值。 |
| `void Set(byte[] v)` | 写 bytes。 |
| `void Set(int/long/boolean v)` | 便捷重载。 |

Java `XClock` 增加 lambda 友好构造和回调：

```java
XClock clock = new XClock((dump) -> {
  System.out.println("dump: " + dump);
});

clock.StepRis((cycle) -> {
  System.out.println("Rising edge: " + cycle);
});
```

### Scala 扩展

Scala 复用 Java 封装，并在 `com.xspcomm` package object 中增加：

| 扩展 | 说明 |
| --- | --- |
| `port("pin")` | 等价 `port.Get("pin")`。 |
| `xdata := value` | 支持 `Int`、`Long`、`String`、`Array[Byte]`、`BigInteger`。 |
| `(Long) => Unit` 自动转 `cb_void_u64_voidp` | 可直接传给 `StepRis/StepFal`。 |
| `(Boolean) => Unit` 自动转 `cb_int_bool` | 可直接传给 `XClock` 构造或 `ReInit`。 |

示例：

```scala
import com.xspcomm._

xspcomm.init()

val x = new XData(32, IOType.Input)
x := "0x1234"

val p = new XPort("req_")
p.Add("data", x)
println(p("data").Get())

val clk = new XClock((dump: Boolean) => println(dump))
clk.StepRis((cycle: Long) => println(cycle))
clk.Step(5)
```

## Go API 差异

Go wrapper 对 `XData` 做了接口包装：

```go
package main

import (
    "fmt"
    "math/big"
    "xspcomm"
)

func main() {
    x := xspcomm.NewXData(129, xspcomm.IOType_Input)
    x.Set(123)
    x.Set(big.NewInt(-1236))

    fmt.Println(x.Get()) // unsigned *big.Int
    fmt.Println(x.S())   // signed *big.Int
    fmt.Println(x.U64()) // uint64, low 64 bits
    fmt.Println(x.S64()) // int64, low 64 bits
}
```

Go 中常用包装：

| API | 说明 |
| --- | --- |
| `NewXData(...) XData` | 构造包装后的 `XData` 接口。 |
| `XDataFromVPI(...) XData` | VPI 构造包装。 |
| `Set(a ...interface{}) XData` | 支持普通 SWIG 类型和 `*big.Int`。 |
| `Get() / U() *big.Int` | 无符号大整数。 |
| `S() *big.Int` | 有符号大整数。 |
| `AsImmWrite/AsRiseWrite/AsFallWrite/...` | 返回 `XData`，便于链式调用。 |
| `SubDataRef(...) XData` | 返回包装后的切片。 |

`XClock` 支持 Go callback：

```go
func step(dump bool) int { return 0 }
func onRise(cycle uint64) {}

clk := xspcomm.NewXClock(step)
clk.StepRis(onRise)
clk.Step(3)
```

也支持 C 函数地址：

```go
clk := xspcomm.NewXClock(xspcomm.TEST_get_u64_step_func(), uint64(0x123))
clk.StepRis(xspcomm.TEST_get_u64_ris_fal_cblback_func(), uint64(0x456))
```

## Lua API 差异

Lua wrapper 基本保持 SWIG 原始方法名，调用使用 `:`：

```lua
local x = require("xspcomm")

local a = x.XData(32, x.XData.In)
a:Set(0x12345678)
print(a:AsBinaryString())

local clk = x.XClock(function(dump) return 0 end)
clk:StepRis(function(c) print("rise", c) end)
clk:StepFal(function(c) print("fall", c) end)
clk:Step(10)
```

`std::vector<unsigned char>` 导出为 `UCharVector`：

```lua
local vec = x.UCharVector()
vec:push_back(1)
vec:push_back(2)
a:Set(vec)
```

## JavaScript API 差异

JavaScript wrapper 使用 `xsp_run(fc)` 加载 wasm/napi 模块后传入 `xsp` 对象。

```javascript
import("./xspcomm.js").then((m) => {
  m.xsp_run(async (xsp) => {
    const x = new xsp.XData(128, xsp.XData.In)
    x.value = BigInt("0x123456789abcdef0")
    console.log(x.String())

    const clk = new xsp.XClock((dump) => {})
    clk.StepRis((cycle) => console.log(cycle))

    await clk.RunStep(10)
  })
})
```

JS wrapper 增加：

| API | 说明 |
| --- | --- |
| `xdata.value` | `W() <= 64` 时返回 `AsInt64()`；宽信号返回 `GetVU8()` 当前实现结果。设置 `bigint` 时走 bytes typemap。 |
| `XClock.clk` | getter，读取 C++ `GetClk()`。 |
| `XClock.AStep(cycle)` / `ACondition(cond)` / `ANext()` | Promise/async 版本。 |
| `XClock.RunStep(cycle)` | async 推进时钟并 resolve 等待任务。 |
| `XClock.StepRis(func)` / `StepFal(func)` | JS 函数回调版本。 |

## Python thirdcall API

`include/xspcomm/thirdcall.h` 和 `swig/python/thirdcall.cpp` 提供从 C/C++ 回调 Python 函数的辅助能力，主要用于特定集成场景。

| API | 说明 |
| --- | --- |
| `bool init_third_call()` | 导入 Python 模块并缓存其中 callable。模块名由环境变量 `PYTHON_THIRD_MODUEL` 指定，未设置时使用 `thirdcall`。源码变量名拼写为 `MODUEL`。 |
| `bool free_third_call()` | 释放缓存。 |
| `int get_function_id(const char *function_name)` | 获取函数 id，未找到返回 `-1`。 |
| `bool call_third_function(int id, unsigned int *args, int argc, unsigned int *ret, int *retc)` | 调用 Python 函数。参数以 list 传入；返回值应为 int list。 |
| `bool test_third_call()` | 查找并调用 `test_third_call` 进行自测。 |

## 常见组合示例

### C++：端口、时钟和条件触发

```c++
#include "xspcomm/xcomm.h"
#include "xspcomm/xcomuse.h"

using namespace xspcomm;

int main() {
    XData clk_pin(1, XData::InOut, "clock");
    XData valid(1, XData::InOut, "valid");
    XData data(32, XData::InOut, "data");
    XData expect(32, XData::InOut, "expect");
    XData one(1, XData::InOut, "one");

    expect = 0x55;
    one = 1;

    XPort port("dut_");
    port.Add("valid", valid);
    port.Add("data", data);

    XClock clk([](bool dump) { return 0; }, {&clk_pin}, {&port});

    ComUseCondCheck check(&clk);
    clk.StepRis(check.GetCb(), check.CSelf(), "cond");
    check.SetCondition("data_hit", &data, &expect, ComUseCondCmp::EQ,
                       &valid, &one);

    clk.Step(1000);
    for (auto &key : check.GetTriggeredConditionKeys()) {
        printf("triggered: %s\n", key.c_str());
    }
}
```

### C++：表达式触发

```c++
ComUseExprCheck check(&clk);
clk.StepRis(check.GetCb(), check.CSelf(), "expr");

XData valid(1, XData::InOut);
XData data(32, XData::InOut);
int valid_node = check.ExprNewSignal(&valid);
int data_eq = check.ExprNewCompareSigConst((int)ExprOp::EQ, &data, 0x55);
int root = check.ExprNewBinary((int)ExprOp::LAND, valid_node, data_eq);
check.SetExpr("hit_data", root);

clk.Step(1000);
```

若要使用带名字的信号表达式，`ExprEngine` 更适合配合 `XSignalCFG` 或手工 `RegisterExternalSignal()`；`ComUseExprCheck` 这层更偏向“把已经构建好的表达式 root 挂到 clock 上”。
