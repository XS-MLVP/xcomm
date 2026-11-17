# XSignalCFG：从布局创建信号

头文件：[xsignal_cfg.h](https://github.com/XS-MLVP/xcomm/blob/master/include/xspcomm/xsignal_cfg.h)。XSignalCFG 将名称、位宽、原生内存偏移映射为 XData，并可创建只读常量或表达式信号。它使用已有内存布局，不负责申请 DUT 内存或从 RTL 推导地址。

## 原生布局

Picker 默认只导出一份 `.xscfg` 配置库，统一保存端口元数据、原生记录、共享类型、字段
绑定及派生信号，不附带 YAML/JSON/索引副本。`XSignalCFG` 按文件魔数识别该格式，
从 JAR 提取的文件也适用。下面的 YAML 入口仍用于手写配置，不是生成模型的运行时格式。

variables 支持列表或嵌套映射，例子见 [列表配置](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/tests/fixtures/signal_cfg.yaml) 和 [映射配置](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/tests/fixtures/signal_cfg_map.yaml)。

```yaml
variables:
  - name: Top.data
    type: SData
    offset: 0
    mem_bytes: 2
    rtl_width: 16
```

| 字段 | 含义 |
| --- | --- |
| name | 列表模式的名称；映射模式由嵌套 key 拼成名称 |
| offset | 相对于 base_address 的字节偏移 |
| mem_bytes | 布局记录的单元素存储长度；数组寻址使用 array_strides |
| rtl_width | RTL 位宽；1 会创建 width=0 的 scalar XData |
| array_size | 数组元素数量；非零时必须同时提供 array_dimensions 和 array_strides |
| array_dimensions | 各维的元素数量；乘积须等于 array_size |
| array_strides | 从外到内的字节步长；rank 须与 array_dimensions 一致 |
| type | 布局类型说明；不能靠它覆盖 BindNativeData 的内存约定 |
| layout_id | 可选的 RTL 类型引用，指向顶层 `types` 表；不是 C++ 类型名 |
| bindings | unpacked struct 的同构字段组：字段路径、真实 offset/mem_bytes 及包围它的数组维度/步长 |

array_size 非零时，array_dimensions 和 array_strides 均为必填；布局不完整会在解析阶段失败。

```yaml
variables:
  - name: Top.mem
    type: VlWide<3>
    offset: 0
    mem_bytes: 12
    rtl_width: 65
    array_size: 6
    array_dimensions: [2, 3]
    array_strides: [80, 24]
```

## API

| API | 用途与约束 |
| --- | --- |
| `XSignalCFG(path_or_yaml, base_address=0, model_id="")` | 接受 XSCFG 文件、手写 YAML 文件或文本，首次实际查询时加载。可选 model_id 拒绝其他构建的配置库；生成 DUT 始终传入该标识。 |
| `Metadata(name)` | 显式解码一个元数据记录并返回 JSON，例如 `Metadata("ports")`；缺失返回 `"null"`。 |
| `NewXData(name, xname="")` | 创建信号；数组名称创建扁平下标 0 的元素。失败可返回 nullptr；C++ 调用方管理返回对象。 |
| `NewXData(name, index, xname="")` | 按扁平、从零开始的存储下标创建数组元素；下标必须小于数组大小并会检查，越界抛出异常。非数组名称返回 nullptr。 |
| `NewXArray(name)` | 返回缓存的递归 XArray，边界为从零开始的存储坐标；名称不存在或不是数组时返回 nullptr。Size 为当前维度长度，Array/Data 每层选择一个下标。 |
| `NewXView(name)` | 返回缓存的 RTL 坐标视图，支持字段/数组路径；名称不存在时为空视图，缺少或不匹配的类型/绑定抛出异常。动态叶子默认立即写入。 |
| `BindDataLayouts(group)` | 从 XPort group 查找已有 Data，关联配置中的 RTL 类型；不创建视图、不替换后端。DPI 工程的 base 为 0 时也可使用。 |
| `NewViewFromPort(group, name)` | 用配置中指定 RTL 对象的类型，从 group 查找已有叶子并创建视图；生成器用它创建 unpacked 容器，packed 值通常通过 Data 的显式视图入口访问。 |
| `GetSignalNames(pattern="")` | 列出名称；pattern 按名称子串筛选。 |
| `At(name)`；`operator[]` | 原生布局查询；不存在时 is_empty=true，Python 包装成 None。 |
| `Address(name)` | 返回 base+offset；先核对 At 的存在性，不能用这个结果判断信号是否存在。 |
| `String()` | 输出布局和元数据说明。 |
| `cfg_data/cfg_base_address` | 配置与基地址；解析后修改字段不会重新加载，换布局时创建新对象。 |

<!-- runnable:python -->
```python
import ctypes
import xspcomm as xs

memory = ctypes.c_uint16(0x1234)
config = xs.XSignalCFG("""
variables:
  - name: Top.data
    type: SData
    offset: 0
    mem_bytes: 2
    rtl_width: 16
""", ctypes.addressof(memory))
signal = config.NewXData("Top.data")
assert signal is not None
assert signal.U() == 0x1234
signal.ImmSet(0x55)
assert memory.value == 0x55
```

memory 必须比所有绑定它的信号活得更久。相对地址、mem_bytes、位宽、维度和步长应与实际模拟器布局匹配，尤其不能将窄 C 对象当成任意宽 word 数组。

## 单文件二进制配置库

格式显式使用小端整数，不保存进程指针或原生 C++ struct 布局。128 字节文件头记录模型
标识及索引/块目录位置。每条索引占 8 字节，含 32 位名称指纹和压缩块/槽位定位符。
指纹仅用于加速搜索，命中后始终核对完整类别和名称，哈希冲突不会绑定到另一信号。
名称按公共前缀共享，类型化数值记录按块 deflate 压缩，普通块最多 256 条；压缩无收益的
块直接保存原数据，每块单独校验。

POSIX 上映射文件，不在打开时解析全部记录或为每个信号建立 map。首次查询仅校验和
解码涉及的块。解压块缓存上限为 2 MiB；异常大的单条记录需要缓存之外的临时空间。
信号对象、类型和绑定按需创建。名称枚举遍历块但不解码所有值；`String()` 会显式加载
全部信号，不是启动路径。文件头/区间检查、块校验和、值复杂度及深度限制拒绝非法输入。
文件头立即验证、数据块按需验证，不进行启动时的全文件完整性扫描。写入使用相邻临时
文件及原子重命名，旧文件已映射的读者继续有效；不产生日志或长期缓存文件。

文件格式版本 1 与原生 ABI 6 相互独立。构建需要 zlib 开发包（Debian/Ubuntu 为
`zlib1g-dev`）。只读配置库拥有元数据，不拥有 DUT 存储，使用信号时模型必须存活。

## 递归原生类型

可选顶层 `types` 是去重的类型节点表，节点的 `element` 和字段的 `type` 引用其他类型 ID。
类型在实际创建 Data、关联端口或请求视图时按需解析并共享，检查深度、循环引用和位宽；
原生视图还核对维度和存储大小，不在构造 xcfg 时全量展开。
packed struct/array 共享完整 `XData`，unpacked struct 按独立字段组绑定；数组元素按需创建，
不按元素重复存类型或注册回调。普通单维 bit-vector 为位值叶子。

`NewXArray()` 仍使用旧的零基存储坐标；`NewXView()` 使用 RTL 左右边界，两者不混用。
没有 RTL 元数据的旧数组配置仍可调用 `NewXArray()`，但不能凭元素数量猜出 RTL 边界。
unpacked struct 没有整体位值，应使用视图，不能用 `NewXData()` 绑定其首字节。
`XView.FromNativeBindings(base, layout, bindings)` 消费同一组经过核验的描述；调用方负责
存储生命周期。绑定组偏移、大小和 stride 必须由生成该模型的原生编译器确定，不能用
字段位宽推算 padding。`XView.FromNative()` 则适用于一个 packed 值或它的固定 unpacked 数组。

二进制库的 `port_types` 元数据将精确的逻辑端口名映射到同一类型表的 ID；原有 `ports`
展示树及其 `Layout` 保持可读。`BindDataLayouts()` 在所有叶子与位宽校验通过后才建立
关联，不从宿主属性名或名称前缀猜类型。Data 保留只读类型引用，不保留指向已销毁 xcfg
的裸指针。`NewXData()` 和存储数组中的 packed 叶子也保留可用的类型，支持随后无参的
`StructView()` / `ArrayView()`；这不改变这些入口原有的绑定和提交模式。

`XPort` 只是命名 Data 的 group，本身没有 RTL 类型或字段/元素关系。`port_types` 指真正的
RTL 导出端口类型；结构来自配置，不来自 group。`NewViewFromPort()` 创建的是视图，不是
XPort 或新信号；不增加 XPort 成员或时钟调度登记。

## 只读派生信号

可选 signals 列表的条目使用 name/kind，并按类型提供 rtl_width、offset/mem_bytes、expr 或 value 等字段：

- kind=direct：可直接绑定的原生布局，仍需要 offset/mem_bytes/rtl_width。
- kind=const：解析 value 字符串并通过 BindConst 创建只读信号，数值求值范围为 64 bit。
- kind=unavailable：生成器没有证明可访问存储或可求值表达式，`NewXData()` 返回 nullptr。
- 其他派生条目：使用 expr 创建 ExprEngine，并通过 BindExpr 读出结果。source/deps 是元数据，不能仅填 source 就假定它自动产生一个 alias 表达式。

派生信号继承 ExprEngine 的数值范围和语法限制，不提供任意 C++ 表达式解释。出现依赖环或表达式解析失败时，NewXData 报错并返回 nullptr。s_xsignal_cfg/s_xsignal_meta 分别描述原生布局和派生元数据；At 主要面向前者。
