# XPort：命名信号集合

头文件：[xport.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xport.h)。XPort 保存 XData 指针，负责命名、批量 IO/写模式操作和边沿刷新；不拥有信号，也不推进时钟。

<!-- runnable:python -->
```python
import xspcomm as xs

valid = xs.XData(1, xs.XData.InOut)
data = xs.XData(32, xs.XData.InOut)
port = xs.XPort("bus.")
assert port.Add("valid", valid)
assert port.Add("data", data)
port["data"] = 0x55
assert port["data"].value == 0x55
assert port.Has("bus.data", True)
```

## API

| API | 用途与约束 |
| --- | --- |
| `XPort(prefix="")`；`prefix`；`GetPrefix()` | 对 Add 的局部名称附加 prefix。 |
| `Add(name, data)`；`Del(name)` | 添加/移除信号映射；重复 Add 返回 false；Del 不销毁 XData。 |
| `PortCount()`；`GetKeys(raw_key=false)` | 返回信号数量及局部名称，raw_key 时返回带 prefix 的名称。 |
| `Has(name, raw_key=false)`；`Get(name, raw_key=false)`；C++ 下标 | 查询或获取信号；Get 缺失会触发断言，先 Has。 |
| `Connect(other)` | 按局部名称对接信号，并使用 XData.Connect 的方向规则；缺少对端会报告并继续，返回 true 不代表全部成功连接。 |
| `NewSubPort(subprefix)` | 建立包含匹配前缀信号的新视图，信号仍是同一批对象。 |
| `SelectPins(names)` | 按名称创建子集视图；未找到的条目被报告。 |
| `FlipIOType/AsBiIO` | 批量调整信号方向。 |
| `AsImmWrite/AsRiseWrite/AsFallWrite` | 批量调整可写信号的提交时机。 |
| `WriteOnRise/WriteOnFall/ReadFresh(mode)` | 边沿提交与刷新，由 XClock 调用或显式管理。 |
| `SetZero()` | 为信号赋零，仍受可写性和写模式约束。 |
| `String(prefix="")` | 描述集合。 |
| C++ `operator=(other)` | 将对方同名信号的值赋给当前信号。 |
| `CSelf()`；`port_list/port_vec` | 地址桥接及底层集合；应用通常使用 Add/Del/Get 而非直接改集合。 |

C++ 的 NewSubPort/SelectPins 当前用 new 创建对象并返回引用，调用方负责释放；可以立即交给 `std::unique_ptr<XPort>(&port.NewSubPort(...))`。跨语言返回对象的所有权需核对生成绑定，不假定引用返回值自动托管。

把 port 加入 XClock 后，port 和其中所有 XData 都必须比这段时钟使用期更长。Python 也应显式保存信号引用，不依赖 SWIG 指针关系延长对象寿命。
