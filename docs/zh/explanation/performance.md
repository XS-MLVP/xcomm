# 性能与内存：成本发生在哪里

仿真开销主要来自 DUT 求值、跨语言调用、信号复制、表达式求值和时序匹配。比较性能时，固定采样相位、波形和覆盖率配置，再测量这些部分的耗时。

## 当前实现中的成本控制

| 路径 | 当前机制 | 边界 |
| --- | --- | --- |
| XData change callback | 无回调时跳过 shadow 比较；注册回调时建立基线 | 有回调时仍需检查 aval/bval 和更新 shadow |
| 重复写入 | memcmp 检查完整 vector，变化后 memcpy 保存写入快照 | 比较也有成本；控制项不能破坏所需的重复写语义 |
| XPort 批量刷新 | map 用于名称查找，vector 用于热路径遍历 | Add/Del/子端口需保持两种索引一致 |
| native 内存访问 | 绑定时选择存储类型/固定 word 数；模板展开短路径，长路径循环 | 精确读取 uint8/uint16/uint32 存储；不能从窄对象过读 uint64 |
| engine 调度 | 原生半周期批处理，相位内运行 matcher | Python 边沿回调仍有跨语言调用成本 |
| Coverage | 同一点中等价 Pattern programs 共享执行 | 各 bin 独立计数；不同点不能共享历史 |
| typed 求值 | 显式栈、每节点结果和除法 scratch 复用，单次求值共享 DAG 节点只算一次 | 每次求值都重新读输入；不缓存上一周期结果 |

typed 程序固定且已访问相关节点后，热 `Eval` 复用工作区。`EvalBits` 返回自有 byte vector，因此会复制结果；新增节点、首次访问的分支和 Collect 后重新求值可能再次分配。Collect/Clear 释放不可达节点及对应工作区。内存成本取决于位宽、图大小和匹配重叠容量。

## 为什么使用精确宽度访问

native 存储与 XData 的 aval/bval 布局不同。memcpy 风格的 `memory::Load/Store<T>` 支持未对齐对象且避免通过不兼容指针类型访问；编译器可将固定小尺寸复制降低为直接加载/存储。最终指令取决于编译器、目标与优化，不能仅凭 C++ 写法承诺“一条指令”。

项目当前编译参数含 `-fno-strict-aliasing`，用于限制编译器基于类型不重叠假设的优化。它不会修复越界、未对齐的非法访问、悬空指针或数据竞争。边界正确性仍靠匹配的类型/尺寸与对象寿命。

BindNativeData 的二态存储只保存 aval；已有四态缓存如何反映到读写由[当前 API 语义](../reference/xdata.md)说明。

## 如何比较性能

1. 固定提交、编译器/参数、CPU、仿真器与输入序列；分别记录构建时间和运行时间。
2. 保持回调、波形、coverage、采样相位和匹配容量相同；先验证结果/计数相同。
3. 分开测量小宽度与宽向量、冷启动与热调用、普通事件与多重叠 Pattern。
4. 报告多次运行的分布、峰值内存及分配次数，不把一次计时当作稳定提升。

`tests/unit/test_expr_allocations.cpp` 验证热求值的分配约束；`test_xexpr` 验证 typed 值与错误；`test_trigger_matcher`、`test_coverage_core` 检查匹配和统计语义。性能数据应随具体 benchmark 和环境保存，本文不提供跨平台的提升百分比。
