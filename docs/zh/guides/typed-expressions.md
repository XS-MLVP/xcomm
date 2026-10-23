# 宽位运算与表达式回收

用 typed Bits 节点构造大于 64 bit 的运算，并用 Collect 回收重复编译后不再使用的节点。以下示例演示 128-bit 加法、有符号除法和节点回收。

## 128-bit 加法与回收

<!-- runnable:python -->
```python
import xspcomm as xs

signal = xs.XData(128, xs.XData.InOut)
signal.value = (1 << 100) | 0xA5
expr = xs.ExprEngine()
a = expr.NewBitsSignal(signal, False)
b = expr.NewBitsConst(128, False, (7).to_bytes(16, "little"))
root = expr.NewBits(xs.ExprOp_ADD, a, b, 128, False)
assert expr.IsKnown(root)
assert int.from_bytes(expr.EvalBits(root), "little") == (1 << 100) | 0xAC
expr.NewConst(99)
remap = expr.Collect(xs.XIntVector([root]))
root = remap[root]
assert root >= 0 and expr.NodeCount() == 3
assert int.from_bytes(expr.EvalBits(root), "little") == (1 << 100) | 0xAC

```

NewBitsConst 使用恰好 ceil(width/8) 个 little-endian 字节；无效高位必须清零。加法在声明宽度内运算，超出部分截断。EvalBits 返回完整位模式；Eval 只返回低 64 bit。

Collect 返回旧 root 到新 root 的映射。用它更新应用保存的 roots，未保留项映射为 -1。signal 仍由调用方持有。

## 有符号除法

<!-- runnable:python -->
```python
import xspcomm as xs

expr = xs.ExprEngine()
width = 129
mask = (1 << width) - 1
a = expr.NewBitsConst(width, True, ((-7) & mask).to_bytes(17, "little"))
b = expr.NewBitsConst(width, True, ((-3) & mask).to_bytes(17, "little"))
quotient = expr.NewBits(xs.ExprOp_DIV, a, b, width, True)
remainder = expr.NewBits(xs.ExprOp_MOD, a, b, width, True)
assert int.from_bytes(expr.EvalBits(quotient), "little") == 2
bits = int.from_bytes(expr.EvalBits(remainder), "little")
value = bits - (1 << width) if bits & (1 << (width - 1)) else bits
assert value == -1

```

有符号除法向零截断，余数符号跟随被除数。除零和负移位量抛异常；超宽移位量按有效宽度饱和，带符号右移填充符号位。`CAST` 指定新的宽度/符号；扩宽时按照输入符号解释进行扩展。

普通二元运算要求操作数类型一致；混合宽度或符号先 CAST。比较/逻辑结果必须指定 width=1、is_signed=false。Bool 和 1-bit Bits 是不同类型，不能随意互换。

## 用于触发与 Coverage

在 XEngine 中使用 `ExprNewBits*` 构造同样的节点，不能传入独立 ExprEngine 的 root。宽数值条件应先构造 `NE(value, zero)` 等 Bool root，再 ArmExpr/作为 bin 条件；否则标量 Eval 的低 64 bit 不能代表完整宽值是否为零。

安全点调用 `CollectExpressions(retained_roots)`：它自动保留仍占用的注册、Coverage 程序及条件，重写原生引用并保持历史/计数。前端缓存中的其他 roots 要显式传入并依据返回表更新。注册清理使用 Disarm，节点回收使用 CollectExpressions，二者作用不同。

typed 路径没有实现完整的四态算术传播。求值前核对 IsKnown，避免把含 X/Z 的 aval 当作有效数值。更多限制见[表达式参考](../reference/expressions.md)。
