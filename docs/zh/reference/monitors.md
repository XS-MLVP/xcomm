# Monitor：边沿回调检查器

聚合头文件：[xmonitor.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xmonitor.h)。Monitor 提供可挂接到 XClock 的检查与打印组件。它适合“在边沿做检查，成立就 Disable 时钟”，不同于 XEngine 的注册、命中列表和原生时序运行协议。

## XStepCallback

定义在 [monitor/callback.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/monitor/callback.h)。派生类实现 Call；XConditionCheck、XExprCheck、XFsmMonitor、XEcho 使用此基础能力。

| API | 作用 |
| --- | --- |
| `Attach(clock, rising=true)`；`Detach()` | 在指定边沿挂接，或解除全部挂接；重复挂同一边沿不重复注册，析构自动 Detach。 |
| `Enable/Disable/IsDisable` | 控制检查器自身的回调启用状态，不等于 XClock.Enable/Disable。 |
| `SetMaxCbs(n)` | 根据回调计数限制检查器，计数由具体检查器更新。 |
| `GetCbCount/IncCbCount/DecCbCount` | 查询或显式调整计数；派生实现决定何时增加。 |
| `Reset()`；`cycle` | 重置自身启用状态及计数；cycle 是最近一次传入的时钟周期号。 |
| `Call()` | 派生检查器的工作入口。 |
| `GetCb()`；`Cb(cycle,self)`；`CSelf()` | 原始函数地址桥接；一般 C++ 使用 Attach 即可。 |

Attach 处理挂接记录及 clock/callback 寿命观察；内部借用的 signals、比较值和用户参数仍由调用方保持存活。清空时钟的整个回调队列会同时移除其他用户回调，优先用 Detach。

## XConditionCheck

定义在 [monitor/condition.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/monitor/condition.h)。构造参数 clock 和 BindXClock 指定**命中后停止哪些时钟**，不自动注册边沿回调；还需要 Attach(clock)。

<!-- runnable:cpp -->
```cpp
#include "xspcomm/xclock.h"
#include "xspcomm/xmonitor.h"
#include <cassert>
using namespace xspcomm;

int main() {
    XClock clock([](bool) { return 0; });
    XData signal(8, XData::InOut), expected(8, XData::InOut);
    signal = 7;
    expected = 7;
    XConditionCheck check(&clock);
    check.SetCondition("match", &signal, &expected, CompareOp::EQ);
    check.Attach(&clock);
    clock.Step(10);
    assert(clock.IsDisable());
    assert(check.GetTriggeredConditionKeys().size() == 1);
    check.Detach();
}
```

| API | 作用与约束 |
| --- | --- |
| `XConditionCheck(clock=nullptr)`；`BindXClock(clock)` | 命中后 Disable 的时钟集合。 |
| `SetCondition(name, XData*, XData*, CompareOp, valid, valid_value, func, arg)` | 信号比较，可选 valid 门控和自定义比较函数。 |
| `SetCondition(name, address, address, CompareOp, bytes, valid_addr, valid_value_addr, valid_bytes, func, arg)` | 等长原生内存按 little-endian **有符号**整数比较；可超过 64 bit。 |
| `GetValidCmpMode/SetValidCmpMode` | valid 门控的比较方式，默认为 EQ。 |
| `RemoveCondition/ListCondition/GetTriggeredConditionKeys` | 按名字管理条件，查询最近一次检查结果。 |
| `ClearClock/ClearCondition/ClearAll` | 清空停止目标、条件或两者；不等于 Detach。 |
| `AsXDataXFunc/AsPtrXFunc` | 从原生函数地址构建自定义比较回调。 |
| `Call()` | 检查全部条件；本轮任何命中则 Disable 目标时钟并增加一次回调计数。 |

CompareOp 为 EQ、NE、GT、GE、LT、LE。XData 比较使用其自身语义；原生 pointer 比较使用有符号解释；XRangeCheck 则使用无符号范围语义。

## XExprCheck

定义在 [monitor/expr.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/monitor/expr.h)，继承 XConditionCheck，共享命名条件集合、结果处理及停止目标，表达式求值使用自己的 ExprEngine。

| API | 作用 |
| --- | --- |
| `ExprNewConst/Signal/Unary/Binary/Compare` | 基本表达式节点。 |
| `ExprNewCompareSigSig/SigConst/ConstSig/SigConstBytes/ConstBytesSig` | 专用信号比较节点。 |
| `CompileExpr(text, cfg)` | 编译文本，解析失败报告错误并返回 -1。 |
| `SetExpr/RemoveExpr/ListExpr/GetTriggeredExprKeys` | 命名表达式及最近结果；与 SetCondition 共用名称空间。 |
| `ClearExpr()`；`ClearAll()` | 清空表达式及内部节点；当前 ClearAll 使用 ClearExpr 行为，不清空 BindXClock 停止目标，需额外 ClearClock/Detach。 |
| `Call()` | 在共同的检查入口中执行条件和表达式。 |

XExprCheck 当前只包装普通表达式和专用宽比较；typed NewBits 与回收接口使用独立 ExprEngine 或 XEngine，见 [ExprEngine](expressions.md)。一个名字只对应一项条件或表达式，设置同名条目会替换已有定义。

## XFsmMonitor

定义在 [monitor/fsm.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/monitor/fsm.h)。这是文本 FSM 检查器，和 Trigger 的 XFsmTransition 描述接口分开。

| API | 作用 |
| --- | --- |
| `XFsmMonitor(clock=nullptr)`；`BindXClock` | 指定命中后 Disable 的时钟，仍需 Attach 安排调用。 |
| `LoadProgram(text, cfg)` | 加载文本 FSM；错误报告并清空程序。 |
| `Reset()` | 回到起点，清除内部 flags/counters 和触发状态。 |
| `Clear()` | 清除 FSM 程序。 |
| `IsTriggered/GetTriggeredState/GetCurrentState/ListStates` | 查询状态与命中。 |
| `Call()` | 执行动作及按顺序选择转移。 |

语法包括 `start NAME`、`state NAME:`、`if EXPR goto NAME`、`if EXPR trigger`、无条件 goto/trigger，以及 `$flag*` 的 set/clear、`$counter*` 的 inc/reset。多条转移按文本顺序检查；不把这套脚本等同于 SystemVerilog assertion 语言。

## XRangeCheck 与 XEcho

XRangeCheck 定义在 [monitor/range.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/monitor/range.h)，是比较函数适配器，不继承 XStepCallback、不自行挂时钟。

| API | 作用 |
| --- | --- |
| `XRangeCheck(range, bytes)` | bytes 为原生 buffer 的比较长度，至少 1；range 是 int 容差。 |
| `cmp(target, center, range)` | range>=0 检查 center-range <= target <= center；range<0 检查 center <= target <= center-range，不作整数回绕。 |
| `ArrayCmp`；`GetArrayCmp()` | 指针比较入口/回调，支持未对齐和宽 buffer。 |
| `XDataCmp`；`GetXDataCmp()` | 完整宽度、无符号 XData 范围比较，未知值返回 false，bytes 不截断 XData。 |
| `CSelf()` | 配合上述回调作为 arg，适配器必须存活。 |

XEcho(valid_addr, data_addr, stderr_echo=true, fmt="%c", convert=0) 在 valid 非零时打印 data，继承 Attach/Detach。convert 选择 char/int/float/double/string；fmt 必须匹配转换类型。适合在仿真过程中打印调试信息。
