# ExprEngine：构造、求值和回收

头文件：[xexpr.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xexpr.h)。ExprEngine 管理表达式 DAG，返回整数 root。它不推进时钟、不注册事件、不累计覆盖率。root 只属于创建它的那个引擎。

## 先选择数值模型

| 需求 | 使用路径 | 结果 |
| --- | --- | --- |
| 既有 uint64 运算/文本条件 | NewConst/NewSignal/NewBinary/CompileExpr | Eval 返回 uint64 |
| 宽信号直接比较 | NewCompareSigSig / *Bytes / MaskedCompare | 已知性检查后返回布尔数值 |
| 固定位宽、符号、cast 和宽算术 | NewBitsConst/NewBitsSignal/NewBits | EvalBits 返回完整 little-endian 位模式 |

宽比较不需要先构建完整算术图。typed 路径适合中间结果也必须携带位宽/符号的表达式。实际代码见[宽表达式指南](../guides/typed-expressions.md)。

## 标量与专用比较 API

| API | 作用与限制 |
| --- | --- |
| `NewConst(uint64)` | 创建普通常量 |
| `NewSignal(XData*)` | 普通信号叶；W>64 返回 -1 |
| `NewUnary(op, child)` / `NewBinary(op, lhs, rhs)` | uint64 一元/二元计算 |
| `NewCompare(op, lhs, rhs)` | 标量节点比较 |
| `NewSelect(cond, lhs, rhs)` | 普通条件选择 |
| `NewCompareSigSig(op, lhs, rhs)` | 完整 XData 比较；顺序比较要求同宽已知值 |
| `NewCompareSigConst` / `NewCompareConstSig` | uint64 常量零扩展到信号宽度 |
| `NewCompareSigConstBytes` / `NewCompareConstBytesSig` | 完整 little-endian 常量与信号比较 |
| `NewMaskedCompareSigConstBytes(signal, value, mask)` | mask 选择的位等值；字节长度为 ceil(max(1,W)/8)，无效高位为零，value 不能在 mask 外置位 |
| `NewWithin(child, window)` / `NewHold(child, window)` | 带历史的窗口/连续成立条件 |

普通算术/位运算走 uint64 路径；除零/模零返回零。typed 运算的除零行为见下节。

## typed API

| API | 作用与限制 |
| --- | --- |
| `NewBitsConst(width, is_signed, bytes)` | width>0，字节长度必须精确且无效高位清零 |
| `NewBool(value)` | 构造 Bool 类型节点 |
| `NewBitsSignal(signal, is_signed)` | 借用 XData；scalar 使用有效宽度 1 |
| `NewBits(op, lhs, rhs, width, is_signed)` | 操作数必须是 typed roots；一元操作使用 rhs=-1 |
| `EvalBits(root)` | 完整位模式副本；仅接受 typed root |

支持 ADD/SUB/MUL/DIV/MOD、BAND/BOR/BXOR/BNOT、SHL/SHR、LAND/LOR/LNOT、EQ/NE/GT/GE/LT/LE、CAST。SUB 的 rhs=-1 表示取负。typed NewBits 不提供 SELECT/WITHIN/HOLD 构造。

普通二元算术/比较要求操作数宽度、符号和 Bool/Bits 类型一致；移位量可有不同宽度。算术结果保持左操作数宽度/符号；比较/逻辑结果必须为无符号 width=1。混合类型先 CAST。Bool 只参与符合类型约束的逻辑/等值操作或显式 CAST。

结果按声明宽度截断。signed 除法向零截断，余数跟随被除数；除零和负移位量抛异常。signed 右移填充符号位；超宽移位量饱和。Eval(typed_root) 仍仅返回低 64 bit，宽条件应转换成 Bool 后注册。

## 求值、历史与文本

| API | 作用 |
| --- | --- |
| `ValidRoot(root)` | 检查编号是否在有效范围内 |
| `Eval(root)` / `IsKnown(root)` | 取 uint64 数值 / 检查已知性；整数载荷本身不表达 X/Z |
| `SetCycle(cycle)` | 为 within/hold 指定时间单位；XEngine 使用 half tick |
| `ResetState()` | 清除 within/hold 历史，保留图 |
| `OptimizeShortCircuitOrder(root)` | 调整无状态短路子树次序，同时保留时序求值要求 |
| `RegisterExternalSignal(name, signal)` | 注册外部命名信号，优先于配置解析 |
| `GetOrCreateSignal(cfg, name)` | 获取已有信号或从 XSignalCFG 创建 |
| `CompileExpr(text, cfg)` | 编译文本；失败抛异常 |

文本支持整数、信号、括号、算术/位运算、比较、`! / not`、`&& / and`、`|| / or`、`within(window, expr)` 和 `hold(window, expr)`。文本编译仍使用原有标量语义；不自动把表达式提升为 typed 宽算术。

within 在条件成立时及后续 window 时间内成立；hold 要求连续达到窗口。时间由 SetCycle 决定，不自动等于完整 DUT 周期。typed 运算没有完整四态算术传播；先检查 IsKnown。

## 生命周期与回收

`NodeCount()`、`ConstantCount()` 查询存储。`Collect(roots)` 在调用方安全点保留可达节点/常量，返回 old-to-new root 表；未保留项为 -1，调用方必须重写全部保存的 roots。外部信号不由图拥有。`Clear()` 清空图、内部信号与历史，使旧 roots 全部失效。

XEngine 对应方法是 ExprNew*、ExprEvalBits、CollectExpressions；它会保留自身注册和 Coverage roots。XExprCheck 只包装部分标量/比较接口。root 仅在创建它的对象中使用。C++ BindExpr 使用 shared_ptr 保持表达式引擎，但外部信号仍须存活；其 XData 结果载荷仍受 64-bit Eval 合同约束。
