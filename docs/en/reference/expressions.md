# ExprEngine: construction, evaluation and collection

Header: [xexpr.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xexpr.h). ExprEngine owns an expression DAG and returns integer roots. It does not advance time, register events or count coverage. A root belongs only to its creating engine.

## Choose a numeric model

| Requirement | Path | Result |
| --- | --- | --- |
| Existing uint64 arithmetic/text conditions | NewConst/NewSignal/NewBinary/CompileExpr | Eval returns uint64 |
| Direct wide-signal comparison | NewCompareSigSig / *Bytes / MaskedCompare | Boolean numeric result after knownness checks |
| Explicit width, signedness, casts and wide arithmetic | NewBitsConst/NewBitsSignal/NewBits | EvalBits returns full little-endian bits |

Wide comparison does not require a complete arithmetic graph. Use typed nodes when intermediate results also need width/signedness. See [typed expressions](../guides/typed-expressions.md) for runnable examples.

## Scalar and specialized comparison APIs

| API | Purpose and limits |
| --- | --- |
| `NewConst(uint64)` | Ordinary constant |
| `NewSignal(XData*)` | Scalar-model leaf; returns -1 for W>64 |
| `NewUnary(op, child)` / `NewBinary(op, lhs, rhs)` | uint64 unary/binary operations |
| `NewCompare(op, lhs, rhs)` | Compare scalar nodes |
| `NewSelect(cond, lhs, rhs)` | Ordinary conditional selection |
| `NewCompareSigSig(op, lhs, rhs)` | Full XData comparison; ordering requires equal-width known values |
| `NewCompareSigConst` / `NewCompareConstSig` | Zero-extend uint64 constants to signal width |
| `NewCompareSigConstBytes` / `NewCompareConstBytesSig` | Compare full little-endian constants and signals |
| `NewMaskedCompareSigConstBytes(signal, value, mask)` | Equality on selected bits; byte length ceil(max(1,W)/8), unused high bits zero, no value bits outside mask |
| `NewWithin(child, window)` / `NewHold(child, window)` | Stateful window/consecutive conditions |

Ordinary arithmetic/bit operations use uint64. Division/modulo by zero return zero. Typed operations have different behavior.

## Typed APIs

| API | Purpose and limits |
| --- | --- |
| `NewBitsConst(width, is_signed, bytes)` | Positive width, exact byte length, unused high bits zero |
| `NewBool(value)` | Construct a Bool node |
| `NewBitsSignal(signal, is_signed)` | Borrow XData; scalar has effective width 1 |
| `NewBits(op, lhs, rhs, width, is_signed)` | Operands must be typed roots; unary operations use rhs=-1 |
| `EvalBits(root)` | Copy the complete bit pattern; requires a typed root |

Supported operations: ADD/SUB/MUL/DIV/MOD, BAND/BOR/BXOR/BNOT, SHL/SHR, LAND/LOR/LNOT, EQ/NE/GT/GE/LT/LE and CAST. SUB with rhs=-1 negates. Typed NewBits does not construct SELECT/WITHIN/HOLD.

Ordinary binary arithmetic/comparison requires matching operand width, signedness and Bool/Bits kind. Shift counts may have different widths. Arithmetic results retain the left operand's width/signedness; comparisons/logical results require unsigned width=1. CAST mixed types explicitly. Bool participates only in permitted logical/equality operations or explicit CAST.

Results truncate to declared width. Signed division truncates toward zero and remainder follows the dividend; division by zero and negative shifts throw. Signed right shift fills with the sign bit; oversized shift counts saturate. Eval(typed_root) still returns only the low 64 bits: convert wide predicates to Bool before registration.

## Evaluation, history and text

| API | Purpose |
| --- | --- |
| `ValidRoot(root)` | Check whether an ID is within the valid range |
| `Eval(root)` / `IsKnown(root)` | Read uint64 / inspect knownness; integers do not carry X/Z |
| `SetCycle(cycle)` | Set within/hold time units; XEngine uses half ticks |
| `ResetState()` | Clear within/hold history while retaining the graph |
| `OptimizeShortCircuitOrder(root)` | Reorder stateless short-circuit subtrees while respecting temporal evaluation |
| `RegisterExternalSignal(name, signal)` | Register named external signals ahead of configuration lookup |
| `GetOrCreateSignal(cfg, name)` | Reuse or create a configured signal |
| `CompileExpr(text, cfg)` | Compile text; throw on failure |

Text supports integers, signals, parentheses, arithmetic/bit operations, comparison, `! / not`, `&& / and`, `|| / or`, `within(window, expr)` and `hold(window, expr)`. It retains scalar semantics and does not automatically promote expressions to typed wide arithmetic.

Within holds when the child is true and for the subsequent window. Hold requires consecutive truth reaching its window. SetCycle defines time units, which are not automatically whole DUT cycles. Typed evaluation does not implement full four-state arithmetic propagation; check IsKnown first.

## Lifetime and collection

NodeCount/ConstantCount inspect storage. `Collect(roots)` retains reachable nodes/constants at a caller-managed safe point and returns an old-to-new map. Discarded IDs map to -1; remap every client-held root. External signals remain borrowed. Clear removes graphs, internally created signals and history, invalidating all roots.

XEngine provides ExprNew*, ExprEvalBits and CollectExpressions, automatically retaining its registration/Coverage roots. XExprCheck wraps only part of the scalar/comparison API. Use each root within its creating owner. C++ BindExpr retains the expression engine through shared_ptr, but external signals must remain alive; its XData payload still follows the 64-bit Eval contract.
