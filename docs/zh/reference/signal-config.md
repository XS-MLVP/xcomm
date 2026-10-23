# XSignalCFG：从布局创建信号

头文件：[xsignal_cfg.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xsignal_cfg.h)。XSignalCFG 将名称、位宽、原生内存偏移映射为 XData，并可创建只读常量或表达式信号。它使用已有内存布局，不负责申请 DUT 内存或从 RTL 推导地址。

## 原生布局

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
| mem_bytes | 一个元素的存储长度，数组下标使用它作为 stride |
| rtl_width | RTL 位宽；1 会创建 width=0 的 scalar XData |
| array_size | 数组元素数量 |
| type | 布局类型说明；不能靠它覆盖 BindNativeData 的内存约定 |

## API

| API | 用途与约束 |
| --- | --- |
| `XSignalCFG(path_or_yaml, base_address=0)` | 接受存在的文件路径或 YAML 字符串，第一次查询/构造时解析。 |
| `NewXData(name, xname="")` | 创建并绑定信号，失败可返回 nullptr；C++ 调用方管理返回对象。 |
| `NewXData(name, index, xname="")` | 数组元素地址为 base + offset + mem_bytes*index，调用方核对索引和实际存储范围。 |
| `NewXDataArray(name, xname="")` | 创建 array_size 个信号，C++ 返回 shared_ptr vector。 |
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

memory 必须比所有绑定它的信号活得更久。相对地址、mem_bytes 与位宽应与实际模拟器布局匹配，尤其不能将窄 C 对象当成任意宽 word 数组。

## 只读派生信号

可选 signals 列表的条目使用 name/kind，并按类型提供 rtl_width、offset/mem_bytes、expr 或 value 等字段：

- kind=direct：可直接绑定的原生布局，仍需要 offset/mem_bytes/rtl_width。
- kind=const：解析 value 字符串并通过 BindConst 创建只读信号，数值求值范围为 64 bit。
- 其他派生条目：使用 expr 创建 ExprEngine，并通过 BindExpr 读出结果。source/deps 是元数据，不能仅填 source 就假定它自动产生一个 alias 表达式。

派生信号继承 ExprEngine 的数值范围和语法限制，不提供任意 C++ 表达式解释。出现依赖环或表达式解析失败时，NewXData 报错并返回 nullptr。s_xsignal_cfg/s_xsignal_meta 分别描述原生布局和派生元数据；At 主要面向前者。
