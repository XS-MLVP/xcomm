# Typed expressions and collection

Use typed Bits nodes for arithmetic wider than 64 bits and Collect to reclaim nodes left unused after recompilation. The examples below cover 128-bit addition, signed division, and node collection.

## 128-bit addition and collection

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

NewBitsConst requires exactly ceil(width/8) little-endian bytes with unused high bits cleared. Arithmetic wraps to the declared width. EvalBits returns the full bit pattern; Eval returns only the low 64 bits.

Collect returns an old-to-new root map. Use it to update every retained client root; discarded roots map to -1. The caller still owns signal.

## Signed division

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

Signed division truncates toward zero; remainder follows the dividend's sign. Division by zero and negative shift counts throw. Oversized shifts saturate at the effective width; signed right shift fills with the sign bit. CAST specifies a new width/signedness and extends according to the input's signed interpretation.

Ordinary binary operations require matching operand types. CAST mixed widths or signedness explicitly. Comparison/logical results must use width=1 and is_signed=false. Bool and one-bit Bits are distinct types.

## Use with Trigger and Coverage

Use ExprNewBits* to create nodes inside XEngine; roots from a separate ExprEngine cannot be transferred. Convert wide numeric conditions to Bool, such as `NE(value, zero)`, before ArmExpr/bin use. Otherwise scalar Eval's low 64 bits cannot represent full-width nonzero truth.

Call `CollectExpressions(retained_roots)` at a safe point. It automatically retains occupied registrations, Coverage programs and conditions, remapping native references while preserving histories/counters. Pass other frontend-cached roots explicitly and update them from the returned map. Disarm frees registrations; CollectExpressions reclaims expression nodes.

Typed evaluation does not implement full four-state arithmetic propagation. Check IsKnown before treating aval as a numeric value. See the [expression reference](../reference/expressions.md) for limits.
